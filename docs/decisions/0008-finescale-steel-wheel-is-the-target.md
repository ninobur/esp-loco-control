# 0008 — The 7-spoke finescale steel wheel is the production speed target

Status: Superseded by 0022 (2026-08-10)

## Decision
The survey car and the production locomotives use the 7-spoke, 27.8 mm
finescale steel wheel as the optical speed target. Nominal circumference
87.34 mm; effective rolling circumference to be calibrated under load, and
seven pulses per hand-turned revolution confirmed before speed is trusted.

## Context
The wheel is the first target to produce uniform intervals across every
phase position (sd 1.52 ms on a 24 ms median; interval-deviation
autocorrelation at wheel period −0.025). The LGB-style wheel kept a
phase-locked blind arc through a repaint; changing the wheel removed it.
History: 10 hand flags → 5 → 2 tape flags → bare 10-spoke LGB → this.

## Alternatives considered
- Tape flags on a solid face — rejected: excellent contrast but adhesive
  on a rotating wheel in a Southern California summer, and the era's data
  is superseded.
- Bare LGB spokes — rejected: phase-locked blind arc independent of paint.
- Encoder disc on the axle — not pursued for the survey car; remains the
  documented fallback if daylight defeats bare polished spokes.

## Consequences
`SPOKES_PER_WHEEL = 7`, `WHEEL_CIRCUMFERENCE_MM = 87.34` in both sketches.
All pre-2026-08-06 speed figures carry the old constants. The aperture
question (gaps aimed at sky and ballast) transfers to this wheel and is
what the daylight run instruments; black backing behind the spokes is the
cheapest untested lever.

## References
Commit `3a54b90` (CODEX); IR_SENSOR_NOTES 2026-08-05 amendments;
`IR_DIAG_DAYLIGHT_PREP.md`.
