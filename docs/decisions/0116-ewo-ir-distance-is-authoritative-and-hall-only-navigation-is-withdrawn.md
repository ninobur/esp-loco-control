# 0116 — EWO requires coherent IR distance for navigation advancement

Status: Accepted for NAVI_EWO, 2026-09-30. Implementation remains a candidate
until David and Sam review the compiled build and field evidence. This record
does not authorize flashing.
Decided by: David, after review with Sam and Otto's 2026-09-29/30 runs.

## Decision

IR health is observation and telemetry, not authority to erase coherent
cumulative distance. `INADEQUATE_CONTRAST`, saturation, sample gaps, pulse
aborts and reacquisition remain raw recorded diagnostics. No pulse progression
means no measured movement; it is provisionally STOPPED, even at nonzero PWM.
PWM >60 warrants a warning, not a manufactured movement conclusion.

EWO target confirmation requires the known target, NAVI-owned Hall support,
correct direction/context, and physically coherent IR distance (±15% of the
target interval). Missed Magnet remains dependent on IR passage of its mapped
distance. Without applicable IR distance, NAVI holds its last established MM
and target; Hall observations continue but cannot advance MM. The 650-ms
Hall-only fallback and degraded Hall re-anchor are withdrawn for EWO.

A normal reversal retains the existing IR/map frame. NAVI captures the
cumulative IR counter most recently received at reversal as the new directional
origin, selects the target in the new direction (including a second reversal
before a marker), and again requires Hall+IR coherence. The type-5
IR packet provides cumulative `completedPulses` and `nominalUm`, with boot,
source, calibration and ordering identity; no synthetic distance is needed.
A genuine reset/source/scale/order break, or IR movement during PWM=0, destroys
the mapped distance relationship. NAVI preserves established position and
context but cannot confirm or re-anchor by Hall alone. Operator redeclaration
is the authorized restoration of navigation context. A temporary missing
packet or stale link only suspends applicability; a later same-frame cumulative
counter can bridge the gap.

## Context and risk

Otto's normal stops produced `INADEQUATE_CONTRAST` and stationary diagnostic
counter changes. The previous health veto made IR unavailable; the 650-ms
fallback then confirmed MM13, MM12 and MM11 from one sustained Hall field
without IR pulse progression. The fallback, intended as protection, created
false position. IR dependence sacrifices automatic location progress during
real IR frame loss; the operator must diagnose/redeclare. A second IR sender,
counter corruption, or reversal without a coherent prior origin remains a
hold, not a pretext for Hall-only recovery. The latest-received packet can lag
the actual direction command by its reporting interval; this is the same
temporary association risk accepted in 0111 and needs field validation.

## Supersession

- 0111's 650-ms degraded EWO operation and moving-contrast health veto are
  withdrawn; its IR-as-normal-sensor and latest-received association survive.
- 0112's PWM-zero displacement observation survives, but its degraded-Hall
  re-anchor is withdrawn. A motionless station stop keeps the frame.
- 0093 remains historical NAVI_COHERENCE prior art; its 0111 extension to
  EWO is withdrawn.
- 0115's physical-origin and interval-local tolerance rules survive; its
  statement that degraded confirmation was not reopened is superseded here.

## References

`0111`, `0112`, `0115`, `0093`,
`../NAVI_EWO_GOVERNING_DOCUMENTS.md`, Otto 2026-09-29/30 field logs and
`../../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md`.
