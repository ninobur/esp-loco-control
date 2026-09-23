# NAVI_ONE_STATION_CURVES 0.1

Measurement-only NAVI_ONE build for answering one question: does the Gaussian
shape judgement become unreliable specifically during station approach, zero
ramp, dwell, and departure?

It starts from `NAVI_ONE_1_0X11_FIELDTEST`. Station control, Hall acquisition,
recognition thresholds, navigation verdicts, and stop behaviour are unchanged.
The diagnostic addition publishes every completed Hall passage rather than only
refusals. Do not treat this as an operational firmware release.

## Records

Each passage produces:

- `ngr/loco/<id>/diag/waveform`: the existing binary, chunked, verbatim
  waveform. `openedAtMs` and `closedAtMs` are the join keys.
- `ngr/loco/<id>/diag/wave_meta`: JSON context with a monotonically increasing
  sequence number, opening and closing station phase, a phase bitmask, station,
  marker offset, navigation marker, direction, commanded and actual PWM,
  stop-episode/paused/stitch information, decimation, and the recognizer result.

`phase_mask` uses `1 << StPhase`: IDLE=1, APPROACH=2, ZONE=4, ZERO_RAMP=8,
DWELL=16, DEPART=32. It preserves transitions that happen while a passage is
open instead of assigning the whole waveform to its final phase.

The normal Pi MQTT logger already records both topics. This build does not add
permanent storage on the locomotive. A broker outage can fill the existing
publish queue; `pub_drop` exposes any lost messages.

## Field run

1. Start with Toby fully charged and the Hall sensor clear during its two-second
   baseline calibration.
2. Declare position and direction exactly as for NAVI_ONE.
3. Run at least two laps in one direction, including every normal station stop;
   reverse and repeat if battery and time permit.
4. Do not alter station positions, magnets, recognition thresholds, or ramps
   during the run.
5. Before accepting the dataset, confirm `pub_drop` remained zero and that the
   `wave_meta.seq` series has no gaps.
6. Compare residual and outcome by `phase_mask`, PWM, station, and direction.
   Cruise/IDLE passages are the control population; station-phase passages are
   the experimental population.

The build is useful even if Toby stops: every refusal is published immediately,
and the inherited six-passage withdrawal dump remains available as a duplicate
diagnostic record.

## 0.2

One change from 0.1: the discard branch in `HallCapture.h` may fire only while
the reading itself is below `entryMargin`. 0.1's first run showed every Arches
CW departure record beginning mid-flank (decision 0075, gate 14). Compiled,
not yet run. The field proof is repeated Arches CW departures with continuous
published flanks and `discards` at zero, not merely a lap without a shutdown.

## 0.3

Decision 0074: the Gaussian residual and the two-sided archaeology are
diagnostics. They are still computed and published, and `wave_meta` now
carries `would_shape_refuse` and `shape_outcome`, but they cannot refuse a
passage. Admission is the time guard, the amplitude floor, and the
Navigator's polarity and sequence. The stop-episode widening of refusals is
removed. Compiled, not yet run.
