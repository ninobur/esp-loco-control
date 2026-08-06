# IR_TEST — two structural fixes: speed filter and adaptive envelope

> **SUPERSEDED, 2026-08-05 (same day).** The schema and behaviour described
> below were replaced by the state/reacquisition pass before ever flying:
> the revolution-time median became an **interval** median, the `STOPPED`
> event no longer exists (replaced by `TIMEOUT`/`LATCH_TIMEOUT` with
> `state` and nullable speeds), `telem/status` gained the envelope
> diagnostics, and Change B's pulse-gated decay — whose deadlock is
> analysed in IR_SENSOR_NOTES — became activity-gated. **Do not check a
> parser against this document.** Current schema:
> `IR_TEST_STATE_AND_REACQUISITION.md` §4, plus the unverified-envelope
> NVS guard added 2026-08-06 (commit `dcf3936`). This file is retained as
> the record of the first pass and its field test.

**Date:** 2026-08-05
**Sketch:** `firmware/test-programs/IR_TEST/IR_TEST.ino` (formerly `Spoke_IR_RSSI_survey_v2`)
**Node:** `IR_SPEED_SENSOR` — unchanged
**Commits:** `24a55e3` (Change A), `dcbada0` (Change B), `73e93d5` (rename)
**Status:** superseded same day — see banner above.

Two independent faults, deliberately committed separately so they can be
flashed and field-tested one at a time. Either can ship without the other.

---

## Change A — `24a55e3` — the interval filter could wedge permanently

### The fault

`acceptInterval()` gated admission to the interval buffer on a multiple of the
buffer's **own** median — refuse anything beyond 3×. That is a self-referential
admission rule, and it wedges:

1. A spurious 41 ms interval seeds the buffer (a collapsed envelope after idle —
   Change B's fault, which is how the two compounded).
2. The median becomes 41 ms, so the gate becomes 123 ms.
3. Every genuine interval that follows (232–620 ms) exceeds the gate and is refused.
4. Refused intervals never enter, so the median cannot move.
5. Reported speed freezes — while still publishing `"quality":"OK"`.

Observed in the field: six consecutive genuine intervals discarded, speed stuck
at 1402.44 mm/s. It cleared only because the 4 s stop timeout called
`resetIntervalBuffer()`. **A locomotive running continuously never gets that**,
so on the loco the wedge is permanent.

Widening the multiplier relocates the wedge; it does not remove it. Any filter
whose admission rule reads its own contents has this failure mode.

### The fix

Admission is unconditional; the robustness moved into the output computation.

| | before | after |
|---|---|---|
| admission | gated on 3× own median | **unconditional** |
| what is buffered | intervals | intervals **and** revolution times |
| statistic | mean of 5 intervals (2.5 revolutions) | **median of 5 revolution times** |
| speed | `revsPerSec × circumference` | `circumference / medianRevSeconds` |

A **revolution time** is `SPOKES_PER_WHEEL` consecutive intervals, recomputed on
every pulse — sliding, not tumbling. Two consequences:

- A revolution spans every flag **exactly once**, so unequal flag spacing cancels
  exactly. The old 5-interval mean averaged 2.5 revolutions, which does not
  cancel: reported speed oscillated with flag placement error.
- A missed edge roughly doubles one revolution time; a spurious edge roughly
  halves one. A median outvotes either. A mean does not.

`intervalRing[]` is exactly `SPOKES_PER_WHEEL` deep, so when full it holds
precisely one revolution's worth and summing every slot *is* the sliding
revolution time — no windowing arithmetic to get off by one.

**Why it cannot wedge:** no state outlives the buffer. Every sample ages out
within 5 pulses regardless of what precedes or follows it.

### Verification

Layer 2 was transcribed verbatim into a host harness and replayed.

*No code path skips insertion.* Instrumented insertion count equalled the
measured-interval count in every trace. The only non-insertion is the zero
sentinel `sensorTask` emits when there is no previous rising edge — the absence
of a measurement, not a measurement being judged.

*True ring, fixed depth, no conditional writes.* `intervalRing[SPOKES_PER_WHEEL]`
and `revBuffer[REV_MEDIAN_N]`, both `% depth`, both written unconditionally.

*No uninitialised reads during warm-up.* `medianRevMs()` sorts and indexes only
`revsFilled` slots; the revolution sum runs only once `intervalsFilled ==
SPOKES_PER_WHEEL`.

**Warm-up output (steady 400 ms intervals, true speed 143.75 mm/s):**

| pulse | interval | `accepted` | speed |
|---|---|---|---|
| 1 | 0 (sentinel) | 0 | 0.00 |
| 2 | 400 | 1 | 0.00 |
| 3 | 400 | 1 | **143.75** |

Zero, then correct. No garbage. Same after any `resetIntervalBuffer()`.

**Behaviour under the field trace and injected glitches:**

| scenario | result |
|---|---|
| the 41 ms wedge seed, then genuine 232–620 ms | never sticks; converges by pulse 6 |
| missed edge (one interval doubles) | **zero error** — fully absorbed |
| spurious edge (one interval halves) | +33% for three pulses, fully recovered by pulse 14 |
| uneven flags 380/420 ms | **exactly** 143.75 from the first revolution |

The spurious edge is the worse of the two glitches: it perturbs three
consecutive revolution times against `REV_MEDIAN_N = 5`, so for three pulses
the bad values outvote the good. Bounded and self-clearing, but worth knowing.
`N = 5` was chosen over 3 because 3 would let a spurious edge dominate outright.

### Known behaviour, not fixed here

`sensorTask` does not clear `prevRiseMs` across a stop, so the first pulse after
a dwell carries an interval spanning the whole dwell. That now enters the buffer
(it is a genuinely measured edge-to-edge time), contaminating the first two
revolution times before the median outvotes it — cleared by pulse 4.

I did **not** suppress it, because doing so would reintroduce a conditional
admission rule, which is the thing this change exists to delete. If Sam wants it
gone, the clean place is `sensorTask` emitting the existing zero sentinel when
the gap exceeds `SPEED_TIMEOUT_MS` — a fixed constant, not buffer state. That
would cost the dwell length from `interval_ms` on that one pulse.

---

## Change B — `dcbada0` — the envelope went blind while stationary

### The fault

The adaptive envelope decayed `runMin` up and `runMax` down on a wall clock,
independent of whether the wheel was turning. A stationary wheel presents a
**constant** to the ADC: there is nothing to adapt to, and relaxing against a
constant walks the two bounds together until they meet.

Measured twice in the field at 7.9 counts/second:

```
11:45:18  span 1911      11:47:18  span 951      11:48:38  span 314
11:49:18  span   11   ->  quality UNAVAILABLE
```

Four minutes at a station and the sensor is blind. It recovers on motion — the
expansion correctly sits above the quality gate — but the first pulses after a
dwell come through a squeezed window, which is what manufactured the 41 ms seed
that wedged Change A's filter. Station dwell is normal operation, so the two
faults compounded.

This is a design error, not a badly chosen time constant. **No** value of
`DECAY_STEP` avoids it.

### B1 — decay gated on motion

Decay runs only if a pulse was seen within `ENVELOPE_DECAY_MOTION_WINDOW_MS`
= **3000**. That sits deliberately just under `SPEED_TIMEOUT_MS` (4000), so decay
stops *before* the car is declared stopped. What gets frozen — and what gets
written to NVS — is by construction the envelope that was working while the car
was still rolling, never one that has already begun collapsing in the ambiguous
window before the timeout.

The decay phase is held while stationary (`lastDecayMs = now`), so a ten-minute
dwell earns no burst of catch-up relaxation on departure.

Min/max **expansion stays unconditional and stays above the quality gate**, so a
lighting change is still tracked with no motion and a collapsed envelope can
always recover.

### B2 — envelope persisted to NVS

`runMin`/`runMax` persist via `Preferences`, namespace `irsense`, keys
`envmin`/`envmax`.

- **Written on the moving→stopped transition only.** The guard on that block
  goes false the moment `resetIntervalBuffer()` runs and cannot go true again
  until a new pulse arrives, so it fires exactly once per stop. Nothing per
  pulse, nothing on the capture path.
- **Restored in `setup()` before `sensorTask` is created**, so there is no writer
  to race with.
- **Widened by 10% before use, never narrowed.** A too-wide envelope costs a
  missed pulse or two while expansion re-converges; a too-narrow one manufactures
  edges out of noise — the failure that started all of this.
- An already-unusable envelope (`span < MIN_USABLE_SPAN`) is never written.

`loop()` reads the envelope through volatile mirrors `lastRunMin`/`lastRunMax`
set by `sensorTask` — the same single-writer convention already used by
`lastRaw`/`lastSpan` — which keeps the NVS write off the sampling task entirely.
Plain assignment, so no compound-operation-on-volatile deprecation.

### Verification

Simulated at 1 ms resolution over a 10-minute idle, starting from the measured
span of 1911.

| | t=0 | t=60 s | t=180 s | t=600 s |
|---|---|---|---|---|
| **gated (now)** | 1907 | 1863 | 1863 | **1863 — OK** |
| **ungated (shipped)** | 1907 | 947 | 3 | **3 — UNAVAILABLE** |

The ungated row reproduces the field failure. The gated row loses 44 counts in
the first 3 seconds — the legitimate decay steps inside the motion window after
the final pulse — and is then perfectly flat indefinitely.

*Expansion still works while stationary:* with no motion at all, raw climbing
1800 → 3500 moved `runMax` 2887 → 3500 on the same tick. Span 1863 → 2476.

*Restore guards:*

| stored | result |
|---|---|
| 1000 / 2911 (span 1911) | restored 809 / 3102, span 2293 |
| 0 / 150 (span 150) | restored 0 / 165 — low clamp held |
| 2000 / 2050 (span 50) | **rejected**, cold prime |
| 4000 / 4095 (span 95) | **rejected**, cold prime |
| absent / `max <= min` / out of range | **rejected**, cold prime |

*First-ever boot, empty namespace:* `envPrefs.begin(ns, true)` returns false in
read-only mode when the namespace does not exist. Logs
`[ENV] no stored envelope (first boot) — cold prime from first sample` and falls
through to the unchanged cold-prime path.

*NVS write count.* Two `putInt` calls per stop. A two-hour session stopping every
two minutes is ~60 stops = ~120 key writes. ESP-IDF NVS skips the write entirely
when the stored value is unchanged, and wear-levels across the partition; a 4 KB
page holds on the order of 100 entries before compaction, so ~120 writes is
roughly one page erase per session against a ~100,000-erase endurance. Not a
concern at this rate. It would be a concern if the write were ever moved to the
pulse path — which is why it is not there.

---

## Published payload schema after both changes

Topics are unchanged. `NODE_NAME` = `IR_SPEED_SENSOR`.

### `ngr/spoke/IR_SPEED_SENSOR/telem/pulse` — one per pulse

```json
{"seq":41,"t_ms":128340,"interval_ms":400,"width_ms":95,
 "speed_mmps":143.75,"pkph":26.75,"raw":2410,"span":1863,
 "quality":"OK","accepted":1,"rssi":-67,"ev_drops":0,"rej":0}
```

| field | type | meaning | changed? |
|---|---|---|---|
| `seq` | u32 | monotonic pulse number since boot | — |
| `t_ms` | u32 | `millis()` at the rising edge | — |
| `interval_ms` | u32 | rising-to-rising; `0` on the first pulse after boot | — |
| `width_ms` | u32 | rising-to-falling of this pulse | — |
| `speed_mmps` | float 2dp | **now** circumference ÷ median revolution time | **semantics** |
| `pkph` | float 2dp | `speed_mmps / 5.37325` | — |
| `raw` | int | ADC value that crossed the threshold | — |
| `span` | int | `runMax - runMin` at the edge | — |
| `quality` | string | `OK` \| `MARGINAL` \| `UNAVAILABLE` | — |
| `accepted` | 0/1 | **now only** `0` = no prior edge to measure against | **semantics** |
| `rssi` | int | dBm; `0` in the USB build | — |
| `ev_drops` | u32 | events lost to a full `eventQueue` | — |
| `rej` | u32 | **always `0`** — retained for schema compatibility | **now constant** |

**Two fields changed meaning, none changed name, type, or position.** Existing
parsers continue to work.

- `accepted` was `0` for a gate rejection **or** a first pulse. It is now `0`
  only for a first pulse after boot or reset. `accepted:0` in a log written
  before today may mean either.
- `rej` was a monotonic rejection counter. It is now a hard `0`. **A non-zero
  `rej` in any record is therefore pre-2026-08-05 firmware** — which makes it a
  usable firmware-version discriminator when reading archives.

### `ngr/spoke/IR_SPEED_SENSOR/telem/pulse` — stop event

```json
{"seq":41,"event":"STOPPED","speed_mmps":0.00,"pkph":0.00,"rssi":-67}
```

Unchanged. Note the `event` key is present **only** on this variant — parsers
must not assume every `telem/pulse` message carries `interval_ms`.

### `ngr/spoke/IR_SPEED_SENSOR/telem/status` — every 5 s

```json
{"pulses":41,"raw":2410,"span":1863,"quality":"OK",
 "ev_drops":0,"pub_drops":0,"rej":0,
 "task_max_gap_ms":2,"mqtt_attempts":1,"rssi":-67,"heap":210344}
```

`rej` is now a hard `0` here too. All other fields unchanged.
`task_max_gap_ms` is windowed — reset after each publish.

### `ngr/spoke/IR_SPEED_SENSOR/telem/rssi` — 2 Hz

Bare integer string, e.g. `-67`. WiFi build only. Unchanged.

### `ngr/spoke/IR_SPEED_SENSOR/status/online`

`"1"` retained on connect; `"0"` retained as the LWT. Unchanged.

### New serial output (not MQTT)

```
[ENV] restored min=809 max=3102 span=2293 (stored 1000/2911, widened 10%)
[ENV] no stored envelope (first boot) — cold prime from first sample
[ENV] stored span 50 below MIN_USABLE_SPAN 120 — cold prime
[ENV] saved on stop: min=1000 max=2911 span=1911
[ENV] not saved: span 95 below MIN_USABLE_SPAN 120
```

`[PULSE]` lines are unchanged except that `[interval rejected]` is now
`[no prior edge]`.

---

## Build

Both build configurations, `esp32:esp32:esp32`, `--warnings all`:

| build | flash | RAM |
|---|---|---|
| WiFi telemetry (`#define IR_TELEMETRY_WIFI`) | 909,544 B (69%) | 47,248 B (14%) |
| USB-only (switch commented out) | 290,400 B (22%) | 22,348 B (6%) |

Braces balanced 52/52. Preprocessor balanced 12 `#if*` / 12 `#endif`.

**Warnings — all three pre-date this work, confirmed by building HEAD~3:**

- `pulseCount++` and `eventDrops++` — `-Wvolatile`, C++20 deprecates `++` on a
  volatile-qualified type. In `sensorTask`, untouched. Cosmetic; the fix is
  `x = x + 1`, deliberately not bundled into a safety change.
- `lastRssiPublish defined but not used` — USB build only; declared outside the
  `#ifdef` but used only inside it. Harmless.

No new warnings from either change.

---

## Identifier corrections

Names in the task prompt that differ from the source, checked before editing:

| prompt | actual | note |
|---|---|---|
| `rej` counter | `rejectedIntervals` | `rej` is the **JSON field**; the variable was `rejectedIntervals`. Variable deleted, field kept as literal `0`. |
| `WHEEL_CIRCUMFERENCE_MM` | ✓ exact | untouched, as instructed — still the wrong 115.0 placeholder against ~87 mm measured |

Everything else in the prompt matched exactly: `acceptInterval()`,
`resetIntervalBuffer()`, `SPOKES_PER_WHEEL`, `WINDOW_SIZE`, `DECAY_STEP`,
`DECAY_INTERVAL_MS`, `MIN_USABLE_SPAN`, `runMin`, `runMax`, `sensorTask`,
`accepted`.

Removed: `WINDOW_SIZE`, `intervalBuffer`, `medianInterval()`, `rejectedIntervals`.
Added: `REV_MEDIAN_N`, `intervalRing`, `revBuffer`, `revIndex`, `revsFilled`,
`medianRevMs()`, `lastRunMin`, `lastRunMax`, `envPrefs`, `restoreEnvelope()`,
`saveEnvelope()`, `ENVELOPE_DECAY_MOTION_WINDOW_MS`,
`ENVELOPE_RESTORE_WIDEN_PCT`, `NVS_NAMESPACE`, `NVS_KEY_MIN`, `NVS_KEY_MAX`.

---

## Still outstanding

- **`WHEEL_CIRCUMFERENCE_MM` is wrong.** 115.0 placeholder against ~87 mm
  measured on Toby's mile markers. Every `speed_mmps` and `pkph` figure above is
  proportionally wrong until a physical measurement lands. Deliberately not
  touched — separate change.
- **Differential ambient sampling** — the real optical fix. Separate redesign,
  still gated on whether the emitter is hard-tied to VCC on the breakout.
- **Three sketches now share `NODE_NAME = "IR_SPEED_SENSOR"`** — `IR_TEST`,
  `IR_DIAG`, and the retired `Spoke_IR_RSSI_survey`. They publish to overlapping
  topic trees, so running two at once puts two writers on one topic. Not a fault
  in either change; worth a decision before the next survey run.
