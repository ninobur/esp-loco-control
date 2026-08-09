# IR_SCOPE built — a virtual oscilloscope for the merged-pulse question

**Date:** 2026-08-09
**Files:** `firmware/test-programs/IR_SCOPE/` — `IR_SCOPE.ino`,
`IR_SCOPE_Plotter.py`, `IR_SCOPE_Replay.py`, `README.md`
**Status:** built, replay-verified on synthetic data, **not flashed** —
CODEX reviews first.

## The question

Event telemetry shows a repeating mixture of normal and ~doubled pulse
widths/intervals on the seven-spoke finescale wheel. Leading hypothesis: the
signal enters a spoke normally but the inter-spoke trough does not cross
`thrLow`, so the detector never rearms and swallows the next spoke. Event
lines cannot show this — a missing fall edge produces no event. The
instrument that can is a time-aligned view of the raw waveform beside the
exact thresholds and state transitions that interpreted it.

## What was built

**`IR_SCOPE.ino`** (modeled on HallProbe 1.0's operating concepts, IR_DIAG's
architecture): 1 kHz deterministic sampling (`vTaskDelayUntil` — the one
deliberate timing change from IR_DIAG's drifting `vTaskDelay`), ONE
`analogRead()` per tick feeding both the stream and the detector (HallProbe's
separate raw/avg acquisitions deliberately not copied). The detection path is
IR_DIAG's verbatim: rolling 5/95-percentile envelope (2048-sample window,
256-bucket histogram, 250 ms recompute, 512-sample prime), `thrHigh = runMin
+ 2·span/3`, `thrLow = runMin + span/3`, `DEBOUNCE_US` 15000 rise-to-rise,
`LATCH_TIMEOUT_MS` 2500 discard, contrast-loss discard of the open pulse and
interval anchor. Only the PulseEvent bookkeeping that fed IR_DIAG's text
lines (peak, headrooms, gap trough) is omitted — nothing that affects an
`inPulse` transition.

Every sample is recorded as 16 bits (12-bit raw + inPulse/rise/fall/
contrast-valid flags) into 200-sample batches through a bounded 25-batch
queue; the network task drains peek-publish-remove, capped. A latch or
contrast-loss discard clears `inPulse` **without** a fall flag — the honest
record — with cumulative counters saying why. Batches carry session id,
sequence number, first-sample number, envelope values with mid-batch
recompute offsets, worst per-batch lateness, and cumulative
late/drop/latch/closs/sat counters. Sample N is exactly N ms after capture
start; MQTT timing never becomes sample timing. Wheel metadata (7 spokes,
27.8 mm) rides the status beat only; **no speed is computed anywhere** —
the hand-turn seven-pulse confirmation gates that, per the standing rule.

**`IR_SCOPE_Plotter.py`** (HallProbe Plotter 1.1 interaction model): live
raw + thrHigh + thrLow + shaded inPulse band + rise/fall marks + text
markers; automatic CSV logging flushed per batch; firmware-time x-axis;
session/sequence reassembly; gaps logged as GAP rows, drawn as red spans,
and never bridged; new session ids clear the trace and are never joined;
space freezes the display while acquisition and logging continue. Status
line: batch/sample sequence, arrival rate, firmware lateness, firmware
drops, plotter-detected missing batches, latch/closs/sat, detector and
contrast state.

**`IR_SCOPE_Replay.py`**: offline replay of the real detector semantics
(contrast gate, debounce, latch timeout — not a crossing counter) over a
recorded CSV at falling fractions 1/3 (exact integer arithmetic), 0.40,
0.50, 0.60 plus `--frac` extras, rising rule held at production. Reports
pulses, width/interval percentiles, merged count (interval ≥ 1.7× the p25
base — a swallowed spoke can only inflate an interval), implausibly short
pulses, latches, and pulses per apparent revolution. The 1/3 replay is
validated against the firmware's own recorded edge flags before any
candidate is read. `--plot` overlays a candidate on the waveform showing
exactly which troughs would emit a fall edge.

## Verification

1. **Builds** (`esp32:esp32:esp32`, `--warnings all`): IR_SCOPE
   910,516 B flash (69%) / 52,040 B RAM (15%), **zero warnings**. IR_DIAG
   and IR_TEST rebuilt unchanged: 909,188 (69%) / 52,864 and 912,516 (69%) /
   51,480 — matching their pre-existing sizes.
2. **Python**: `py_compile` + `ast.parse` clean on both tools (Python 3.9).
3. **End-to-end replay test** on a synthetic 8-revolution capture with every
   third inter-spoke trough shallow (1900 counts, above the 1/3 bar 1666 and
   the 0.40 bar 1800, below the 0.50 bar 2000):
   - semantics check: replay at 1/3 reproduced the synthetic recorded
     detector exactly — 38/38 rises, 38/38 falls matched;
   - 1/3 and 0.40: 38 pulses, 18 merged, 4.71 pulses/rev;
   - 0.50 and 0.60: 56 pulses, 0 merged, 0 short/false, 7.00 pulses/rev —
     the tool distinguishes recovery from chatter, which is the decision the
     field data will have to make;
   - overlay PNG renders the condition unmistakably (recorded band merging
     two spokes over a trough that never reaches the recorded thrLow, the
     candidate splitting them).

## Known limits / risks

- `markerText` is written from the network task (MQTT callback) and read
  from `loop()` without a lock — same as HallProbe; worst case is a torn
  marker string, never detector state.
- Batch counters (`latch`, `closs`, `late_n`…) are read at publish time, so
  they are cumulative-as-of-publish, not as-of-capture; the per-sample flags
  are capture-exact.
- A stalled tick is caught up by `vTaskDelayUntil` (samples late, not lost)
  and reported via `late_us`/`late_n`; sustained starvation would appear
  there first.
- Plotter redraws the full window each frame; keep `--window` ≤ ~20 s at
  1 kHz or the display (not the log — logging is unconditional) gets sluggish.
- The replay trusts the recorded envelope; it does not model how a different
  thrLow would have changed *pulse-driven* operator behavior (speed, stops)
  — it answers the optical/threshold question only.
- ~1.3 KB × 5 msg/s on the samples topic; if the link degrades, the queue
  absorbs ~5 s and then drops **newest** batches, counted in `bdrop` and
  visible as seq gaps.

## What the first capture decides

1. Are there seven distinguishable peaks and troughs per revolution at all?
2. Do merged pulses coincide with troughs that stay above `thrLow`?
3. Which falling fraction recovers seven pulses without false edges
   (replay table + overlay)?
4. How direction, speed, and illumination move the raw waveform (markers).

The production threshold does **not** change on this evidence alone —
decision 0010's headroom rule still governs that call, now with waveform
evidence instead of survivor-biased event statistics.
