# IR CW/CCW full-lap comparison with Toby — pulse-distance correlates with position but disagrees on total distance by 17%

**Date:** 2026-08-26
**Firmware:** `IR_SCOPE_ESPNOW_TX_1_0` / `IR_SCOPE_ESPNOW_RX_1_0` (not yet in this
repo). Wheel: 7 spokes, 87.34 mm circumference (12.477 mm/pulse, one pulse per
rise, per spoke).
**Run:** Toby (`9950012`) towing the IR test car, QUORUM Auto Solo with station
stops, two full circuits of the Lowline, both starting/stopping at the
Hall-sensor-measured 040-041 marker:

- Lap 1: CCW, IR sensor mounted on the **right** side of the car
- Lap 2: CW, IR sensor mounted on the **left** side of the car (flipped
  between laps, same wheel)

**Data:** `field-records/logs/20260826_ir_lap0{1,2}_*.log` (raw IR),
`field-records/logs/20260826_toby_9950012_lap0{1,2}_*_alert.tsv` (paired Toby
`ngr/loco/9950012/alert` telemetry, extracted from the Pi's always-on
`~/NGR/telemetry/all_20260826.log`).

## What Toby's `dead_reckoned_mm`/`lc_mm` actually is

Despite the field name, this is **not** literal millimeters of physical
distance. Evidence: the total value accumulated over one full lap is
**171 units (lap 1) and 170 units (lap 2)** — essentially identical regardless
of direction, which only makes sense for a fixed position index tied to
landmarks around the loop (values 0–170ish, wrapping), not an integrated
distance counter. Per-segment deltas between the same two landmarks matched
almost exactly in both directions (e.g. Arches↔Northpoint: 9 units either
way; Bamboo↔Eastpoint: 17 units either way). This is a **position-index
scale**, not a distance in mm — worth fixing the field name or documenting
this explicitly, since it will mislead anyone who reads it as physical
distance.

## Segment-level comparison

For every landmark-to-landmark segment in both laps, IR-derived distance
(cumulative pulse delta × 12.477 mm) vs. Toby's position-index delta for the
same segment (direction-aware, handling the wrap at the 152↔000 boundary):

| Lap | Segment | Δt | IR pulses | IR mm | Toby units |
|---|---|---:|---:|---:|---:|
| 1 CCW | START(41)→Patio(15) | 95.0s | 532 | 6638 | 26 |
| 1 CCW | Patio(15)→Southpoint(0) | 52.0s | 374 | 4667 | 15 |
| 1 CCW | Southpoint(0)→Bamboo(157) | 26.0s | 242 | 3020 | 13 |
| 1 CCW | Bamboo(157)→Eastpoint(140) | 53.0s | 371 | 4629 | 17 |
| 1 CCW | Eastpoint(140)→Arches(107) | 48.0s | 721 | 8996 | 33 |
| 1 CCW | Arches(107)→Northpoint(98) | 45.0s | 260 | 3244 | 9 |
| 1 CCW | Northpoint(98)→Westpoint(72) | 30.0s | 422 | 5265 | 26 |
| 1 CCW | Westpoint(72)→END(40) | 219.0s | 617 | 7698 | 32 |
| 2 CW | START(40)→Grillers(63) | 80.0s | 703 | 8771 | 23 |
| 2 CW | Grillers(63)→Westpoint(72) | 45.0s | 132 | 1647 | 9 |
| 2 CW | Westpoint(72)→Northpoint(98) | 30.0s | 486 | 6064 | 26 |
| 2 CW | Northpoint(98)→Arches(107) | 15.0s | 221 | 2757 | 9 |
| 2 CW | Arches(107)→Eastpoint(140) | 71.0s | 845 | 10543 | 33 |
| 2 CW | Eastpoint(140)→Bamboo(157) | 26.0s | 375 | 4679 | 17 |
| 2 CW | Bamboo(157)→Southpoint(0) | 51.0s | 414 | 5166 | 13 |
| 2 CW | Southpoint(0)→Patio(15) | 27.0s | 340 | 4242 | 15 |
| 2 CW | Patio(15)→END(40) | 134.0s | 628 | 7836 | 25 |

Pearson correlation between IR mm and Toby's position-index delta across all
17 segments: **r = 0.90**. IR is tracking real relative position along the
track — segments Toby scores as "long" (Eastpoint↔Arches: 33 units, both
directions) are consistently IR's longest segments too (8996–10543 mm).

## The 17% total-distance disagreement

Summing IR mm across each full lap:

- Lap 1 (CCW, right side): **44,157 mm ≈ 44.2 m**
- Lap 2 (CW, left side): **51,705 mm ≈ 51.7 m**

Same physical loop, same Toby-confirmed ~170-171 unit total both ways, but IR
reports a **17% longer** distance for the CW/left-side lap. I raised a
curve-geometry hypothesis (outer vs. inner wheel arc length changing which
side is which per direction) in conversation; the operator judged that
wouldn't account for a difference this size, and that's a reasonable read —
this record does not resolve the mechanism, only documents the effect.

## Investigated and ruled out (partially)

**Missing landmark:** Lap 1 confirmed 7 landmarks (Patio, Southpoint, Bamboo,
Eastpoint, Arches, Northpoint, Westpoint); lap 2 confirmed those same 7 plus
an 8th, **Grillers**, in the START→Westpoint stretch. The operator noted
hitting Toby's main power switch (which also powers the ESP32) around the end
of lap 1, raising the possibility that data went unreceived.

Checked: `ngr/loco/9950012/alert` telemetry from 11:09:00 through 11:13:59
(spanning lap 1's end and lap 2's start) is **fully continuous** — ~1s
cadence throughout, `uptime_ms` climbing monotonically with no reset, voltage
steady at ~15.4V. No reboot or transport gap is visible in this window. So
the specific "some data may not have been received" explanation is not
supported by the telemetry evidence checked so far, though the exact moment
of the power-switch action wasn't independently timestamped, so a narrower
window couldn't be ruled out. The missing Grillers confirmation and the 17%
distance gap remain open — most likely connected to whatever is producing the
distance disagreement generally, rather than to this specific power-switch
event.

## What this does and doesn't establish

- IR pulse counting behaves consistently enough, within a lap, to track
  relative position with the rest of the loop (r=0.90) — it is not producing
  noise.
- IR's absolute distance figure is **not yet trustworthy across conditions**:
  the same physical loop measured 17% differently between the two runs, and
  the two runs differ in direction, sensor side, and (per the operator) a
  power-cycle event, all confounded together. This record cannot separate
  those factors.
- Do not use either lap's total IR distance as a wheel-circumference
  calibration figure. Both are plausible in isolation (44–52 m against a
  previously-nominal ~55–60 m Lowline lap, from `2026-08-09_SYNCHRONIZED_
  HALL_IR_LAP.md`, different wheel), but the 17% spread between them means at
  least one is wrong and there's no independent tiebreaker in this data.
- Toby's own landmark-confirmation system performed normally on both laps
  (agree/disagree ratios healthy: 166/3 lap 1, 337/3 lap 2; zero `lostm`/
  `losts` throughout) — nothing here questions Toby's own navigation.

## Transport health (for reference, not the main question)

| Lap | Packets | Loss | Latch Δ | Contrast-loss Δ | RSSI min/med/max |
|---|---:|---:|---:|---:|---|
| 1 CCW right | 4485 | 23.0% | 7 | 2 | -98/-86/-67 dBm |
| 2 CW left | 3608 | 26.0% | 0 | 2 | -96/-86/-71 dBm |

`missedTotal`/`queueDrops`/`sendErrors` were 0 on both laps (sampler-side);
loss is ESP-NOW broadcast air loss, consistent with prior full-circuit runs.

## Next

- Isolate the confounded variables: repeat with sensor side fixed and
  direction varied, then direction fixed and sensor side varied, to find
  which one (or both) drives the 17% gap.
- Get an independent ground-truth lap length (tape measure or a wheel with
  known-good calibration) to arbitrate between the two totals instead of
  guessing which lap is closer to correct.
- If reproducible, investigate whether it's mechanical (wheel tracking,
  slack, curve geometry) rather than optical/electronic — the segment-level
  correlation (r=0.90) argues the detector itself is behaving consistently
  within each lap, so a mechanical distance-per-revolution difference between
  configurations is a more likely source than a detection-quality problem.
