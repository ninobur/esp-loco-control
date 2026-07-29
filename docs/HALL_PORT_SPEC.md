# Hall Detector Port — `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST` → r13

**Task:** port the validated Hall detector architecture from the `LL_Auto` branch
into the Continuity First sketch. Single-file refactor, one seam, no changes to
CTO, ESP-NOW, traffic control, station logic, governor, or DNA.

---

## Background — why this port

Two branches diverged and never merged:

| | `NGR_LL_DNA_CTO2_r12` | `LL_Auto_r26a` |
|---|---|---|
| CTO / ESP-NOW / traffic control | yes | no |
| Hall sampling in dedicated task | **no** | yes |
| Event queue with detection-time timestamps | **no** | yes |
| Zero-crossing polarity split | **no** | yes |
| `loopstat` observability | **no** | yes |
| 40 ms minimum event gate | yes (downstream) | yes (in detector) |

r12 is the sketch we are keeping. It is missing every Hall fix from the last
month. This port moves those fixes across.

**Evidence the fixes matter.** Field logs from 2026-07-17 showed the main loop
stalling 2–4 seconds during MQTT/WiFi activity while the (separately tasked)
Hall sampler continued with 3–7 ms gaps. In r12, Hall sampling runs *inside*
that stalling loop, so the locomotive is blind while it passes magnets.

**Not in scope for this pass:** the baseline tracker. r12 uses a gated IIR
tracker that differs from the `LL_Auto` slew tracker. Leave it exactly as it
is. We change one variable at a time, and this pass is the task move. Baseline
behaviour is instrumented here so it can be measured *after*.

---

## The seam

`serviceHall()` (≈ line 2112) is almost perfectly self-contained. Its entire
interface to the rest of the sketch is one call:

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

`HALL_MIN_EVENT_MS` is currently applied downstream (≈ line 1981). Apply it in
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

The split re-open is unconditional — do **not** apply r12's `SETTLING` entry
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

r12 has no loop observability at all. Add a topic
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
| Nothing broken | CTO pairing, station stops, and DNA locks behave as they did before |

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
- Do not touch CTO, ESP-NOW, traffic control, station, governor, or DNA code.
- Do not change `HALL_DEADBAND_COUNTS`, `HALL_ENTRY_MARGIN_COUNTS`, or
  `HALL_MIN_PEAK_DELTA`. Per-loco values come from the config header; the
  values in this file are `#ifndef` fallbacks and are **not** what gets flashed.
- Do not add a stuck-event timeout. It interacts with the baseline tracker in
  ways that need their own analysis.

## Commit

Bump `SKETCH_NAME` to `NGR_LL_DNA_CTO2_r13_HALL_TASK`. Commit the detector
change and the `loopstat` addition as two separate commits so they can be
bisected independently.

---

## Codex review notes

**Review date:** 2026-07-25  
**Reviewed against:** the current
`NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` in this folder  
**Result:** the port is feasible, but the specification is **not ready to
implement as written**. The timestamp seam, snapshot synchronization,
baseline cadence, and zero-crossing/downstream-reject interaction need explicit
correction first.

### 1. Seam accuracy

There is exactly one `onMagnetEvent()` call site in the current sketch:
`serviceHall()` calls it at **line 2166**. `loop()` calls `serviceHall()` once,
at **line 2346**. No second call site feeds Hall events into navigation.

The detector does, however, communicate through shared detector/baseline state
in addition to that call:

- `baselineCounts` and the four thresholds;
- `hallEventActive`, event start/return times, and peak deltas;
- baseline tracker samples, counters, state/reason strings, and telemetry time.

Those variables are currently owned entirely by the single-threaded Hall path,
apart from observability. Moving their mutation to a task does not directly
feed DNA/CTO/station/governor code, but it does create cross-thread reads for
the proposed telemetry and `loopstat`. Those reads must be synchronized.

More importantly, the proposed `detectedAtMs` field does not cross the seam.
`onMagnetEvent()` still executes `unsigned long now=millis()` at line 1970 and
derives `dt` from processing time at line 1972. Draining the queue through the
unchanged four-argument signature discards `HallEventMsg.detectedAtMs`.
Consequences after a loop stall include:

- a valid event gets the delayed queue-consumption time rather than detection
  time;
- two queued events drained together can appear less than
  `MIN_MAGNET_INTERVAL_MS` apart and the later one can be rejected;
- speed/governor interval timing can be wrong.

The spec must choose an explicit timestamp seam. The clean option is to extend
the private `onMagnetEvent()` signature with `detectedAtMs` and use it as
`now`. That technically modifies `onMagnetEvent()`, so the “do not modify”
rule needs a narrowly stated exception. An alternative wrapper/global
timestamp would be more fragile. Merely placing a timestamp in the queue
without consuming it does not meet the stated architecture.

Queue delay also means `onMagnetEvent()` reads current loop-owned context
(`motionTimingState`, ramp/governor accumulation, and related state) at
consumption rather than detection. This is less serious than the lost event
timestamp, but the spec should state that only event time/polarity/peaks are
snapshotted; other context remains processing-time context.

### 2. MQTT and thread safety

The hazard is real and directly reachable:

`serviceHall()` lines **2118 and 2150** →
`serviceHallBaselineTelemetry()` line **1629** →
`publishHallBaselineTelemetry()` line **1628** →
`mqttPublish()`.

The detector also directly reaches `Serial.printf()` at lines **2139, 2165,
and 2167**. The proposed task path is MQTT-safe only if the implementation
removes/defer all of those calls. Section 3 is internally inconsistent:
“remove all ... `Serial.printf` reachable from `hallSampleOnce()`” conflicts
with “Serial output from the task is acceptable.” For this port, require no
MQTT calls and preferably no Serial calls from the task; loop-owned diagnostic
publishing can consume counters/snapshots.

`volatile HallBaselineSnapshot hallSnap` is **not a thread-safe handoff**.
`volatile` prevents some compiler optimizations but does not make a multiword
struct update atomic. Loop can observe a torn generation: for example, new
`raw` with old `baseline`/thresholds or a new tracker-state pointer with the
old reason and counters. Individual aligned words/pointers may be atomic on
ESP32, but the set is not mutually atomic.

No control/navigation code needs those snapshot fields to be mutually
consistent; the consumer is diagnostic JSON. Nevertheless, inconsistent
telemetry defeats the measurement purpose and can report impossible
threshold/delta combinations. Use one of:

- a FreeRTOS critical section protecting task-side struct update and
  loop-side copy into a non-volatile local struct;
- a seqlock/generation counter with retry around a local copy; or
- a length-one FreeRTOS queue with `xQueueOverwrite`, letting loop receive a
  complete snapshot.

Build the JSON only from the stable local copy. Do not copy some fields from
`hallSnap` and others directly from live baseline globals. Initialize the
snapshot strings to non-null literals before loop can format them.

The max-gap metric also has a benign data race if loop resets
`hallTaskMaxGapMs` while the task is updating it; a critical-section
read-and-reset avoids losing a maximum.

### 3. Current line locations

The cited locations are accurate in the current file:

- downstream 40 ms gate: **line 1981**;
- `serviceHall()` begins: **line 2112**;
- baseline telemetry calls inside it: **lines 2118 and 2150**;
- event close/classification: **lines 2154–2167**;
- sole `onMagnetEvent()` seam call: **line 2166**;
- `loop()`/`serviceHall()` call: **line 2346**;
- `calibrateBaseline()` in `setup()`: **line 2331**.

Claude Code should match code structure rather than rely only on these
numbers, because inserting declarations/helpers will immediately move them.

### 4. Zero-crossing split

The intended state transition is sound only if implemented in this order:

1. recognize the opposite-entry crossing while the first event is active;
2. capture the first event's peaks, opening pole, start time, and duration;
3. queue the first event only if it passes the detector-side duration/peak
   tests;
4. regardless of whether the first event was rejected as short, immediately
   re-open and seed the second event from the crossing sample;
5. return with the baseline tracker still frozen for an active event.

A shared “finish” helper must not `return` on a short first event before the
second event is opened. The pseudocode's
`if (dur < HALL_MIN_EVENT_MS) { ...; return; }` needs different control flow
when used by the split path.

There is a larger downstream conflict: `onMagnetEvent()` rejects accepted
events less than `MIN_MAGNET_INTERVAL_MS` (currently 700 ms) apart at lines
1993–1996. A direct zero-crossing pair is likely to produce two queued events
closer than that, so the second “magnet” can still be discarded downstream.
The spec must explain why validated split pairs are known to exceed 700 ms, or
define a narrowly scoped way for a detector-validated split successor to
bypass/change that reject. The latter reaches downstream and violates the
current “do not modify `onMagnetEvent()`” rule, so it is a design decision, not
an implementation detail.

Classifying by opening pole does change existing behavior for a single
non-merged physical magnet in these cases:

- noise/ringing makes the first threshold crossing the wrong polarity, followed
  by a larger correct-polarity peak;
- a single magnet's waveform crosses the opposite entry threshold;
- an event opens barely over one entry threshold and later develops a
  dominant opposite peak without a clean baseline return.

The current dominance classifier can overrule the first crossing in those
cases; the proposed classifier cannot. If the opposite entry is crossed, the
new code also reports/splits it as two magnets by definition. That tradeoff is
already noted generally, but “a swing ... is two magnets, never one” is a
hardware assumption that should be confirmed from raw Toby/Otto traces. At
minimum, acceptance should include single-magnet passes in both physical
polarities and verify that each still yields exactly one correctly classified
event.

The pseudocode also refers to `pN`/`pS` without defining when they are copied.
The implementation must snapshot the first event's peak values before
re-seeding the globals for the second event.

### 5. Scope containment

The proposed code does not need to alter CTO, ESP-NOW, traffic, station,
governor, DNA matching, packet structures, or MQTT payloads already in use.
The queue consumer can preserve the single downstream seam.

Two claimed scope boundaries do not currently hold:

1. Passing detection time correctly requires a narrow change at
   `onMagnetEvent()` (or an explicitly designed equivalent).
2. Making split successors survive the existing 700 ms filter may require a
   second narrow downstream change unless field timing proves every real split
   exceeds that interval.

There is also a behavioral baseline change even if
`hallBaselineTrackIfIdle()` is textually unchanged. Today it runs once per
main-loop iteration. The new task calls it approximately once per millisecond.
Because stability is counted in samples and the IIR gain is per call, baseline
qualification and convergence become much faster in wall-clock time. That is
not “leave it exactly as it is” in behavior and is not only a sampling-location
change. Preserve the old effective tracker cadence with an explicit timer, or
acknowledge and validate the cadence change as part of this port.

### 6. Other build/hardware issues to resolve

- FreeRTOS queue/task APIs should be declared explicitly with the appropriate
  headers (`freertos/FreeRTOS.h`, `freertos/task.h`, and `freertos/queue.h`)
  rather than relying on transitive Arduino includes.
- `volatile` aggregate assignment/copy is awkward in C++ and may not compile
  through an implicitly generated assignment operator. Use explicit protected
  field updates/copy or a FreeRTOS queue.
- `hallEventQueue` must be created before the task, and the task must start
  only after `calibrateBaseline()` has completed, as specified.
- GPIO 33 is ADC1, which is the correct ESP32 ADC bank to use alongside Wi-Fi.
  The port should still confirm the target Arduino-ESP32 core's `analogRead`
  behavior from a pinned task on core 0.
- `vTaskDelay(1)` means one RTOS tick, not universally one millisecond. Confirm
  `configTICK_RATE_HZ`; use `pdMS_TO_TICKS(1)` for intent, noting that it can
  still round to one tick.
- Draining all 32 queued events in one `serviceHall()` call is finite but can
  lengthen an already recovering loop because each downstream event can
  publish multiple MQTT messages and run DNA/traffic/station logic. Instrument
  queue depth and verify this does not create a repeated catch-up stall.
- Moving the 40 ms reject into the detector intentionally removes the existing
  `HALL_SHORT_EVENT_IGNORED` MQTT/Serial diagnostic for those events; only the
  counter remains. The spec should state that observability change explicitly.
- The proposed acceptance criterion “no `mag` event with `hallms < 40`” already
  holds in r12 because the downstream gate rejects such events. Acceptance
  should instead verify `short_rejects` increments for injected/observed short
  transients and that none enter the event queue/downstream consumer.
- `detectedAtMs` should represent the event completion time (the current
  pseudocode uses `now` at close), not its start time; document this so interval
  semantics stay unambiguous.
- Static review cannot establish that a 4096-byte task stack is sufficient in
  this exact r12 build. Record/check the task stack high-water mark during the
  field test.

**Recommendation:** revise these four design points before implementation:
(1) carry detection time through the downstream seam, (2) use an atomic
snapshot handoff, (3) preserve or explicitly validate baseline track cadence,
and (4) reconcile zero-crossing successors with the 700 ms interval reject.
