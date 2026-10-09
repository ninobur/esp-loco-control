# 0123 — NAVI Pass 2 uses geographic physical-speed control with a recorded PWM ticker-tape fallback

Status: Accepted (2026-10-08). Documentation only; implementation, merge,
flashing, and field activation are not authorized.
Decided by: David, through the ten-question Pass 2 design reconciliation.

## Decision

Pass 2 executes the current geographic operating requirement with one
physical-speed controller. For the initial Four-Station Local in either
direction, it uses the settled Station 0 + 1.5-MM terminal, −10 to −5-MM
45→20 glide, −5 to 0-MM 20-pKPH region, 0 to +1.5-MM final glide, and
five-second dwell. The boundaries remain configurable geography.

Steady-speed homeostasis uses the median of five fresh IR speed observations,
±5% target tolerance, one-PWM-count corrections, and the established
five-beat correction moratorium. Glide paths use constant physical
deceleration. Their ±5% trajectory tolerance requires three consecutive
outside observations before a bounded ramp change; PWM never increases during
deceleration. Glides start at actual PWM. Manual's 150-ms/count acceleration
ramp runs until 95% of target before homeostasis takes over.

A station stop is complete at actual PWM zero plus 50 ms without a new IR
movement pulse. Decision 0120 remains fully binding for unexpected
PWM-zero movement.

NAVI records one rolling lap of actually applied PWM by geography, direction,
and applicable maneuver. This geographic PWM **ticker tape** is an actuator
fallback, not a geographic or navigation authority. A steady-speed IR outage
of up to 150 ms holds PWM without correction. During deceleration, one missed
expected IR pulse switches to the matching recorded tape; Hall anchors and
elapsed time locate approximate playback progress. Valid IR restores live
control and tape recording.

Every transition among Live IR, PWM Hold, and Ticker Tape records its reason,
location/progress, prior-mode duration, actual PWM, target speed, and measured
speed when available.

## Context

The October 8 reconciliation settled the numerical and behavioral questions
needed to make Document C's physical-speed architecture concrete, while
preserving NAVI as the sole navigation authority and current geography as the
source of operating requirements. The ticker tape supplies previously
successful actuator behavior when feedback is unavailable; it cannot replace
the stop, speed, direction, or overlay instruction that NAVI is executing.

## Alternatives considered

- Fixed geographic PWM as the normal controller was rejected: physical speed,
  not a presumed PWM, is the operating objective.
- Proportional PWM chasing and rapid repeated correction were rejected in favor
  of sparse, evidence-based homeostasis and bounded ramp adjustment.
- A 150-ms deceleration delay was rejected: a missed wheel pulse is too much
  distance near a fixed stopping point.
- A second fallback navigation controller was rejected: the tape replaces
  unavailable speed feedback only.

## Consequences

Implementation must preserve Decisions 0102, 0116, 0120, and 0121: NAVI owns
navigation; valid IR distance remains required for its normal navigation
relationship; PWM-zero movement requires operator intervention; and historical
station procedure cannot retain authority. The exact first-lap/no-record
response, no-IR tape timing/alignment and indexing, missed-pulse calculation,
and any return-to-live blend are deliberately unresolved. They require review
before implementation and must not be invented as defaults.

Telemetry must make changes between control modes explainable without emitting
repetitive status traffic. Field testing remains the evidence for physical
stopping and fallback behavior.

## References

- [Document C, §35](../NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
- [October 8 architectural decision record](../NAVI_DOCUMENT_C_ARCHITECTURAL_DECISIONS_20261008.md)
- [0120 — PWM-zero movement](0120-pwm-zero-movement-requires-operator-position-verification-and-declaration.md)
- [0121 — current geography and overlay authority](0121-current-position-direction-and-overlay-determine-navi-operating-authority.md)
