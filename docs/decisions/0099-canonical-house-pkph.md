# 0099 - Canonical prototype/house pKPH

Status: Accepted by operator, 2026-09-23.

Use `pKPH = mm/s / 5.37325` everywhere pKPH represents the navigation house
speed unit. Approximate multiplier: 0.18611, but implementations retain the
full divisor. This is not physical km/h or a geometric model scale.

- 130 mm/s = 24.1939236 pKPH, displayed as 24.2.
- 212 mm/s = 39.4547062 pKPH, displayed as 39.5 with ordinary one-decimal
  rounding. The requested approximate example 39.4 is within 0.1, but is not
  the rounded result of the canonical formula. Do not tune the factor to it.

Keep `ir_mmps` as the raw speed measurement. Firmware `ir_pkph` is the same
conversion, redundant cross-check telemetry; the dashboard computes from
`ir_mmps` using its one canonical constant. Respect explicit validity and
freshness. A bare Hall timing estimate must not be labeled IR speed.

Historical dashboard factor 0.162 is superseded. Physical km/h conversions,
raw captures and historical measured results remain untouched. Current and
versioned Python dashboards and the live Hall/IR comparator are standardized.

Implementation and deployment status: NAVI_IR_SPEED_TELEMETRY_20260923.md.
