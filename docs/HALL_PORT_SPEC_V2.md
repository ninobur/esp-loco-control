# Hall Detector Port — `SOLONAV_1_0` → `SOLONAV_1_1`

> **Target file:** `SOLONAV_1_0.ino` (current development branch).
> `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST` is its parent/reference sketch only — do not edit it.

**Task:** port the validated Hall detector architecture from the `LL_Auto` branch
into the current development sketch. Single-file refactor, one seam, no changes
to station logic, the PWM ramp service, or DNA navigation.

The Hall detector in `SOLONAV_1_0` is unchanged from its r12 parent, so
everything below applies as written — only the file and line numbers differ.

---

## Background — why this port

Two branches diverged and never merged:

| | `SOLONAV_1_0` | `LL_Auto_r26a` |
|---|---|---|
| CTO / governor code | dormant (uncalled) | absent |
| Hall sampling in dedicated task | **no** | yes |
| Event queue with detection-time timestamps | **no** | yes |
| Zero-crossing polarity split | **no** | yes |
| `loopstat` observability | **no** | yes |
| 40 ms minimum event gate | yes (downstream) | yes (in detector) |

`SOLONAV_1_0` is the sketch we are keeping. Its detector is missing every
Hall fix from the last month. This port moves those fixes across.

**Note:** this sketch has already removed Hall-`dt` speed authority — PWM comes
from prescribed station profiles, not from measured segment times. The
feedback loop that confounded earlier measurements is therefore already broken,
and logs from this sketch are clean without needing a special manual-mode run.

**Evidence the fixes matter.** Field logs from 2026-07-17 showed the main loop
stalling 2–4 seconds during MQTT/WiFi activity while the (separately tasked)
Hall sampler continued with 3–7 ms gaps. Here, Hall sampling runs *inside*
that stalling loop, so the locomotive is blind while it passes magnets.

**Not in scope for this pass:** the baseline tracker. This branch uses a gated IIR
tracker that differs from the `LL_Auto` slew tracker. Leave it exactly as it
is. We change one variable at a time, and this pass is the task move. Baseline
behaviour is instrumented here so it can be measured *after*.

---

## The seam

`serviceHall()` (line 2056) is almost perfectly self-contained. Its entire
interface to the rest of the sketch is one call, at line 2110:

```cpp
onMagnetEvent(h, hallPeakNorthDelta, hallPeakSouthDelta, dur);
```

Everything above that line is detection. Everything reached through it is
navigation. Split the function at that seam and nothing downstream needs to
know.

---

## Changes

### 1. Split `serviceHall()` into detector and consumer

**Rename** the existing `serviceHall()` to `hallSampleOnce()`. Keep its logic
byte-for-byte except as specified in steps 3–5 below.

**Replace** the `onMagnetEvent(...)` call with a queue send:

```cpp
struct HallEventMsg {
  HallState     polarity;
  int           peakN, peakS;
  unsigned long durationMs;
  unsigned long detectedAtMs;   // captured at detection, not at processing
};

static QueueHandle_t hallEventQueue = nullptr;
static volatile unsigned long hallQueueDrops = 0;
```

```cpp
HallEventMsg m = { h, hallPeakNorthDelta, hallPeakSouthDelta, dur, now };
if (hallEventQueue && xQueueSend(hallEventQueue, &m, 0) != pdTRUE) hallQueueDrops++;
```

**Write a new** `serviceHall()` that drains the queue and calls
`onMagnetEvent()` for each event, preserving the existing signature and
argument order. `loop()` keeps calling `serviceHall()` — no change to `loop()`
required for this step.

### 2. Add the Hall task

```cpp
static volatile unsigned long hallTaskMaxGapMs = 0;
static volatile unsigned long hallTaskLastRunMs = 0;

static void hallTask(void*) {
  unsigned long prev = millis();
  for (;;) {
    unsigned long now = millis();
    unsigned long gap = now - prev;
    if (gap > hallTaskMaxGapMs) hallTaskMaxGapMs = gap;
    prev = now;
    hallTaskLastRunMs = now;
    hallSampleOnce();
    vTaskDelay(1);          // ~1 ms tick
  }
}
```

Create in `setup()`, **after** `calibrateBaseline()`:

```cpp
hallEventQueue = xQueueCreate(32, sizeof(HallEventMsg));
if (!hallEventQueue) { Serial.println("[FATAL] hall queue alloc failed"); while (1) delay(1000); }

if (xTaskCreatePinnedToCore(hallTask, "hallTask", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
  Serial.println("[FATAL] hall task creation failed"); while (1) delay(1000);
}
```

Core 0, priority 2, 4096 stack — these are the values that field-validated at
3–7 ms task gaps under real load. Do not change them. Fail loudly on
allocation failure; a silent failure here restores exactly the blindness this
port exists to remove.

### 3. ⚠️ Move MQTT out of the detector — **this is the one that bites**

`hallSampleOnce()` currently calls `serviceHallBaselineTelemetry()`, which calls
`mqttPublish()`. Once the detector runs in a task, that becomes an MQTT publish
from one thread while `loop()` publishes from another. PubSubClient is not safe
for concurrent use. The result is intermittent buffer corruption that presents
as unrelated bugs — dropped messages, garbled payloads, occasional crashes.

**Remove all `mqttPublish` and `Serial.printf` reachable from
`hallSampleOnce()`.** Replace the telemetry with a snapshot struct the task
writes and `loop()` publishes:

```cpp
struct HallBaselineSnapshot {
  int  raw, baseline;
  int  northEnter, northExit, southEnter, southExit;
  uint8_t stableSamples, unstableSamples;
  int  lastStep;
  const char* trackerState;
  const char* frozenReason;
};
static volatile HallBaselineSnapshot hallSnap;
```

The task updates `hallSnap` each sample (cheap, no locking needed — these are
word-sized scalars and `const char*` literals on ESP32). `loop()` publishes it
on the existing `HALL_BASELINE_TELEM_INTERVAL_MS` cadence to
`TOPIC_STATE_HALL`, keeping the current JSON shape so existing subscribers
still work.

Serial output from the task is acceptable but keep it minimal — it blocks.

### 4. Move the 40 ms gate into the detector

`HALL_MIN_EVENT_MS` is currently applied downstream, inside `onMagnetEvent()`
(line 1925). Apply it in
`hallSampleOnce()` before queueing, so short transients never enter the queue
or the odometer at all:

```cpp
if (dur < HALL_MIN_EVENT_MS) { hallShortRejects++; return; }
```

Add `static volatile unsigned long hallShortRejects = 0;`. Leave the downstream
check in place — it becomes redundant, not wrong, and removing it is a separate
change.

### 5. Add the zero-crossing split

A swing from one entry band directly into the *opposite* entry band is two
magnets, never one. The current dominance classifier
(`classifyCompletedHallEvent`, `HALL_DOMINANCE_PERCENT 120`) arbitrates these
merged pairs and sometimes reports one magnet, the wrong polarity, or neither.

Add a new state variable:

```cpp
static HallState hallEventOpenPole = HALL_NONE;
```

Set it wherever an event opens (`hallEventActive = true`), to `HALL_NORTH` if
`raw >= northEnterThreshold`, else `HALL_SOUTH`.

In the active-event branch, **before** the exit-band check:

```cpp
if ((hallEventOpenPole == HALL_NORTH && raw <= southEnterThreshold) ||
    (hallEventOpenPole == HALL_SOUTH && raw >= northEnterThreshold)) {
  // finish the first pole, then immediately open a fresh event on the second
  hallSplits++;
  <finish-and-queue using hallEventOpenPole for classification>
  hallEventActive     = true;
  hallEventStartedAtMs = now;
  hallBaselineReturnAtMs = 0;
  hallPeakNorthDelta  = n;      // seed with the crossing sample only
  hallPeakSouthDelta  = s;      // exactly one of n/s is non-zero here
  hallEventOpenPole   = (raw >= northEnterThreshold) ? HALL_NORTH : HALL_SOUTH;
  return;
}
```

The split re-open is unconditional — do **not** apply the `SETTLING` entry
qualifier (`absErr > TRACK_WINDOW || unstableSamples >= 2`) here. That
qualifier exists to distinguish a shifted quiescent baseline from a magnet, and
at a polarity crossing we already know we are in a magnet.

**Classify by opening pole**, not by dominance:

```cpp
HallState h = HALL_NONE;
if      (hallEventOpenPole == HALL_NORTH) h = (pN >= (int)HALL_MIN_PEAK_DELTA) ? HALL_NORTH : HALL_NONE;
else if (hallEventOpenPole == HALL_SOUTH) h = (pS >= (int)HALL_MIN_PEAK_DELTA) ? HALL_SOUTH : HALL_NONE;
else                                      h = classifyCompletedHallEvent(pN, pS);
```

Note this makes `classifyCompletedHallEvent` unreachable in practice, since
`hallEventOpenPole` is always set when an event closes. Leave the function and
the `HALL_DOMINANCE_PERCENT` constant in place for now — the boot record still
reports the constant, and removing it is a separate change. **Do note the
tradeoff in a comment:** the first threshold crossing now determines polarity
and cannot be overruled by a larger opposite peak later.

Add `static volatile unsigned long hallSplits = 0;`.

### 6. Add `loopstat`

This sketch has no loop observability at all. (`serviceHallSilenceObserver()`
reports Hall silence, which is useful and should be left alone, but it does not
show loop stalls or baseline movement.) Add a topic
`ngr/loco/<id>/state/loopstat`, published every 1000 ms (not 30 s — we need to
see the baseline move within a single pass):

```json
{"loop_max_gap_ms":N,"hall_task_max_gap_ms":N,"hall_task_age_ms":N,
 "baseline":N,"hall_raw":N,"tracker_state":"TRACKING|FROZEN","frozen_reason":"...",
 "queue_drops":N,"short_rejects":N,"splits":N,"pwm":N}
```

`pwm` is `rampCurrent`. Reset `loop_max_gap_ms` and `hall_task_max_gap_ms`
after each publish.

Track `loop_max_gap_ms` by timestamping the top of `loop()`.

---

## Acceptance

Flash to Toby. Run one automatic lap.

| Check | Pass condition |
|---|---|
| Task independence | `loop_max_gap_ms` shows stalls of hundreds of ms or more, while `hall_task_max_gap_ms` stays under ~10 ms |
| Queue | `queue_drops` = 0 for the whole run |
| Short transients | No `mag` event with `hallms` < 40 |
| MQTT stability | No garbled or truncated JSON on any topic — this is the thread-safety check |
| Nothing broken | Station stops and DNA locks behave as they did before |

`splits` may legitimately be 0 — the merged-pole condition was observed under
the older, looser thresholds and may not occur with r12's current config. Zero
splits is a result, not a failure.

Do **not** expect this port to fix false DNA direction locks or reduce the
polarity mismatch rate. Those are separate problems in the decoder, not the
detector. Judging this port by whether they improve will produce a wrong
conclusion.

---

## Do not

- Do not modify the baseline tracker (`hallBaselineTrackIfIdle`,
  `hallBaselineSignalStable`, or any `HALL_BASELINE_*` constant).
- Do not modify `onMagnetEvent()` or anything downstream of it.
- Do not touch station logic, the PWM ramp service, or DNA navigation.
- Do not delete the dormant CTO/governor code in this pass. Purging it is a
  separate, larger change and mixing the two makes a failed lap unattributable.
- Do not re-enable `serviceGovernor()`, `ctoService()`, or
  `serviceTrafficControl()`.
- Do not change `HALL_DEADBAND_COUNTS`, `HALL_ENTRY_MARGIN_COUNTS`, or
  `HALL_MIN_PEAK_DELTA`. Per-loco values come from the config header; the
  values in this file are `#ifndef` fallbacks and are **not** what gets flashed.
- Do not add a stuck-event timeout. It interacts with the baseline tracker in
  ways that need their own analysis.

## Commit

Bump `SKETCH_NAME` to `SOLONAV_1_1_HALL_TASK`. Commit the detector
change and the `loopstat` addition as two separate commits so they can be
bisected independently.

---

## Codex review notes

**Review date:** 2026-07-25  
**Reviewed against:** the current `SOLONAV_1_0.ino` in this folder  
**Assessment:** the port is feasible, and the cited line numbers match the
compile-fixed file, but the spec is **not ready to implement unchanged**. Four
issues need resolution first: the queued detection timestamp is discarded,
the snapshot is not atomic, tasking changes baseline-tracker timing, and the
existing 700 ms filter can reject a zero-crossing successor.

### Seam

The narrow call seam is real:

- `serviceHall()` begins at line **2056**.
- Its sole call into navigation is
  `onMagnetEvent(h,hallPeakNorthDelta,hallPeakSouthDelta,dur)` at line
  **2110**.
- There is only one `onMagnetEvent()` call site in the entire file.
- `loop()` calls `serviceHall()` once at line **2295**.

There is no second Hall-event path into station or DNA logic. Detector state is
also shared through the baseline/event globals (`baselineCounts`, thresholds,
tracker counters/state strings, event state and peak values), but those
currently belong to the single-threaded detector. The proposed telemetry and
`loopstat` make some of them cross-thread observability state; they need a
synchronized handoff.

The more serious seam defect is that the proposed
`HallEventMsg.detectedAtMs` is never used. The new consumer is instructed to
preserve the existing four-argument `onMagnetEvent()` signature. That function
calls `millis()` at line **1914** and computes `dt` from queue-consumption time
at line **1916**. Therefore, after a loop stall:

- an event receives its processing time rather than its detection/completion
  time;
- multiple queued events drained together can appear nearly simultaneous;
- the second can fail the 700 ms filter;
- diagnostic Hall `dt`, speed, and timing logs become wrong.

SOLONAV no longer gives Hall-derived speed motor authority, so this timestamp
error does not directly drive PWM. It still affects event acceptance, DNA
continuity evidence, station-marker delivery, and diagnostics. Put
`detectedAtMs` through the seam—for example, add it as a private fifth argument
and use it as `now` inside `onMagnetEvent()`. That requires a narrowly
documented exception to “do not modify `onMagnetEvent()`.” A timestamp stored
but discarded does not provide detection-time semantics.

Queued processing also reads loop-owned context at consumption time, including
`motionTimingState` and the accumulated PWM sample state used by the event
diagnostics. The spec should explicitly distinguish snapshotted event data from
processing-time context.

### Current line numbers

The V2 references are accurate; the compile fix did not shift these locations:

- `onMagnetEvent()` begins: **1913**
- downstream 40 ms gate: **1925**
- 700 ms interval rejection: **1937–1940**
- `serviceHall()` begins: **2056**
- baseline telemetry calls: **2062** and **2094**
- close/classify block: **2098–2111**
- sole navigation seam call: **2110**
- `setup()`/`calibrateBaseline()`: **2277**
- `loop()` begins: **2292**
- `loop()` calls `serviceHall()`: **2295**

Implementation should still match surrounding code rather than depend only on
numbers, since added declarations immediately cause drift.

### MQTT and task-path safety

The MQTT hazard is confirmed:

`serviceHall()` lines **2062/2094** →
`serviceHallBaselineTelemetry()` line **1573** →
`publishHallBaselineTelemetry()` line **1572** →
`mqttPublish()`.

The current detector also directly reaches `Serial.printf()` at lines **2083,
2109, and 2111**. The implementation is task-safe only if all of those
MQTT/Serial paths are removed or deferred to loop-owned code. Section 3
contradicts itself by first requiring removal of every reachable
`Serial.printf()` and then allowing minimal Serial from the task. Prefer a
clear rule: no MQTT and no routine Serial output from `hallSampleOnce()`;
publish diagnostics from loop using counters/snapshots.

`static volatile HallBaselineSnapshot hallSnap` is **not an atomic handoff**.
Aligned word fields and pointers may each be atomic on ESP32, but the whole
multiword generation is not. Loop can publish a torn mixture—for example, a
new `raw` with old baseline/thresholds, or a new tracker state with the old
reason and sample counters. `volatile` only constrains compiler optimization;
it does not provide mutual exclusion or a coherent snapshot.

No station/PWM/DNA decision should consume `hallSnap`; it is diagnostic only.
Even so, torn telemetry can report impossible values and undermine the purpose
of the instrumentation. Use one of:

- a FreeRTOS critical section around task-side update and loop-side copy;
- a sequence-counter/seqlock with retry into a local non-volatile copy; or
- a one-element FreeRTOS queue with `xQueueOverwrite()` and loop-side receive.

Build both baseline JSON and `loopstat` entirely from one stable local copy,
not partly from `hallSnap` and partly from live globals. Initialize the string
pointers to non-null literals before any `%s` formatting. The
`hallTaskMaxGapMs` read/reset also needs synchronization with the task update
if the maximum must not occasionally be lost.

### Zero-crossing split

The intended split can close and reopen cleanly, but the implementation order
must be explicit:

1. detect the opposite entry while the first event remains active;
2. copy the first event's opening pole, peaks, start time, and duration to
   locals;
3. conditionally queue the first event if it passes duration/amplitude checks;
4. **even if the first event is rejected as short**, reopen the second event;
5. reset its start/return time, seed its peaks only from the crossing sample,
   set its new opening pole, leave the tracker frozen, and return.

The generic detector-side short gate shown in §4 returns from
`hallSampleOnce()`. If reused naively while finishing the first half of a
split, that return prevents the second half from reopening. A finish helper
must return a result rather than returning from the sampler, or the split path
must apply the gate without early exit.

The pseudocode uses `pN`/`pS` without defining when the old peaks are copied.
They must be captured before global peak variables are reseeded for the second
event.

Opening-pole classification changes existing single-magnet behavior when:

- noise or ringing causes the first threshold crossing on the wrong side and a
  later, stronger peak indicates the other polarity;
- one physical magnet's waveform crosses both entry thresholds;
- the event barely opens on one side, later develops a dominant opposite peak,
  but does not provide a normal baseline-return boundary.

The current dominance classifier can overrule the first crossing; the proposed
logic cannot. Crossing the opposite entry also becomes two events by
definition. The assertion that this is “two magnets, never one” is therefore a
hardware assumption that should be verified with raw traces from both sensor
mountings. Acceptance should include isolated north and south magnet passes
and require exactly one correctly classified event per physical magnet.

### Existing 700 ms downstream filter

The filter exists:

```cpp
static const uint32_t MIN_MAGNET_INTERVAL_MS = 700UL; // line 1777
```

`onMagnetEvent()` rejects a new event when `dt < 700` at lines **1937–1940**.
This can defeat §5. A direct opposite-band crossing will often yield two event
completions less than 700 ms apart. If both queue entries are drained together
without passing `detectedAtMs`, the second is even more certain to appear
nearly simultaneous and be rejected.

Before implementation, either:

- demonstrate from validated raw timing that every legitimate split successor
  arrives at least 700 ms after the first event completion; or
- carry an explicit, detector-validated split-successor flag/timestamp through
  the queue and define a narrow downstream exception.

The second option changes `onMagnetEvent()` and its acceptance semantics, so it
must be acknowledged as a scope exception and tested against the double-trigger
problem the 700 ms filter was designed to suppress. Without one of these
decisions, the split may increase `hallSplits` while still delivering only its
first pole to station/DNA logic.

### Scope containment

Textually, the port need not alter station logic, `serviceRamp()`, DNA
matching, dormant CTO/governor functions, packet formats, or existing MQTT
payloads. The current main loop confirms `ctoService()`, `serviceGovernor()`,
and `serviceTrafficControl()` are not called.

Three behavioral/scope exceptions remain:

1. Correct timestamp semantics require a narrow seam change inside
   `onMagnetEvent()`.
2. Preserving a legitimate split successor may require a narrow exception to
   its 700 ms filter.
3. Moving unchanged baseline code to a ~1 tick task changes baseline behavior.
   `hallBaselineSignalStable()` counts samples, and the gated IIR applies its
   gain per call. Today both run once per main-loop iteration; afterward they
   run approximately once per task tick. Stability qualification and baseline
   convergence therefore become much faster in wall-clock time. This violates
   “leave it exactly as it is” behavior even if those functions are
   byte-for-byte unchanged.

Preserve the previous effective tracker cadence with an explicit elapsed-time
gate, or explicitly accept and field-test the faster cadence. This is
particularly important because event entry uses
`hallBaselineUnstableSamples >= 2`; changing sample cadence changes how quickly
an entry qualifies.

Queue draining also calls downstream station and DNA logic, which can publish
MQTT messages. Draining up to 32 events in one loop pass can lengthen a
catch-up iteration and delay the prescribed PWM ramp service that follows at
line **2297**. No ramp code must be edited, but queue depth and drain time
should be instrumented and bounded/validated so the new consumer does not
create repeated catch-up stalls.

### Other build and hardware findings

- Add explicit FreeRTOS headers for the APIs used:
  `freertos/FreeRTOS.h`, `freertos/task.h`, and `freertos/queue.h`, rather than
  relying on transitive Arduino headers.
- Aggregate reads/assignments involving a `volatile` C++ struct can also be
  awkward or fail because generated assignment operators are not
  volatile-qualified. A protected explicit copy or FreeRTOS queue avoids both
  the build and coherence problems.
- Create the queue before starting the task, and start it only after
  `calibrateBaseline()` has completed.
- GPIO 33 is on ADC1, which is appropriate alongside ESP32 Wi-Fi. Confirm the
  selected Arduino-ESP32 core supports the intended `analogRead()` use from a
  pinned core-0 task.
- `vTaskDelay(1)` means one RTOS tick, not inherently 1 ms. Confirm
  `configTICK_RATE_HZ`; use `pdMS_TO_TICKS(1)` to express intent, while noting
  it can still resolve to one tick.
- Moving the 40 ms gate removes the existing
  `HALL_SHORT_EVENT_IGNORED` MQTT/Serial event for short transients; only
  `hallShortRejects` remains. Document this deliberate observability change.
- “No `mag` event with `hallms < 40`” is not a discriminating acceptance test:
  current SOLONAV already prevents such events from reaching `mm/mag`.
  Acceptance should demonstrate that `short_rejects` increments and no short
  event reaches the queue/consumer.
- `detectedAtMs` should explicitly mean event-completion time. The proposed
  initializer captures the close/split time, not event start.
- A 4096-byte stack was validated in another branch, but static review cannot
  prove it is sufficient in this exact build. Publish or inspect the Hall
  task's stack high-water mark during validation.
- Routine task-side Serial output can block and disturb the very gap metric
  being measured; defer it to loop.

**Recommendation:** revise V2 before handoff to (1) carry detection time
through the seam, (2) make snapshots coherent, (3) decide how baseline cadence
is preserved or intentionally changed, and (4) reconcile validated split
successors with the 700 ms filter.
