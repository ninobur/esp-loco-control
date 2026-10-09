# 0123 — NAVI Pass 2 uses simplified IR-loss PWM hold and monotonic-ramp behavior

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

The earlier geographic PWM ticker-tape proposal is superseded. NAVI records no
tape and keeps no historical PWM memory. It has no tape playback, missed-pulse
prediction subsystem, elapsed-time alignment, or added recovery controller.

When valid IR feedback is unavailable at steady speed, NAVI holds the existing
PWM and suspends feedback corrections. The current geographic instruction
remains in force. When valid IR returns, the existing homeostasis corrections
resume.

During an established deceleration, NAVI continues the existing monotonic PWM
reduction. It does not add a feedback correction while IR is unavailable. Hall
geographic checkpoints may be used only where compatible with Decision 0116;
they may not advance NAVI's position, re-anchor the IR/MM relationship,
manufacture geographic progress, or create Hall-only navigation or control
authority. When valid IR returns, the existing glide-path correction resumes
through its established ramp.

## Context

The October 8 reconciliation settled the numerical and behavioral questions
needed to make Document C's physical-speed architecture concrete, while
preserving NAVI as the sole navigation authority and current geography as the
source of operating requirements. Subsequent agreement removed the ticker-tape
fallback: continuity of the already-established PWM behavior is sufficient
without recording, playback, or prediction machinery. No fallback may replace
the stop, speed, direction, or overlay instruction that NAVI is executing.

## Alternatives considered

- Fixed geographic PWM as the normal controller was rejected: physical speed,
  not a presumed PWM, is the operating objective.
- Proportional PWM chasing and rapid repeated correction were rejected in favor
  of sparse, evidence-based homeostasis and bounded ramp adjustment.
- Recorded PWM playback, historical PWM memory, and missed-pulse prediction
  were rejected: they add actuator-history machinery without changing the
  applicable geographic instruction.
- Hall-only navigation or control recovery was rejected: Decision 0116
  continues to require coherent IR distance for navigation advancement.

## Consequences

Implementation must preserve Decisions 0102, 0116, 0120, and 0121: NAVI owns
navigation; valid IR distance remains required for its normal navigation
relationship; PWM-zero movement requires operator intervention; and historical
station procedure cannot retain authority. First-lap/no-record response,
no-IR tape timing/alignment and indexing, missed-pulse calculation, and
return-to-live blending are not implementation questions: the ticker-tape
architecture that required them has been removed.

**Unresolved Hall-checkpoint semantics:** the agreement permits Hall geographic
checkpoints during deceleration only where compatible with Decision 0116. It
does not decide which, if any, checkpoint can help continue the
already-established monotonic PWM reduction without becoming Hall-only
navigation advancement or an IR-coordinate substitute. That boundary must be
explicitly decided before implementation; no exception is implied here.

Telemetry may record IR loss/recovery and relevant current values without a
new mode-history subsystem or repetitive status traffic. Field testing remains
the evidence for physical stopping and this fallback behavior. The stationless
firmware baseline is unchanged.

## References

- [Document C, §35](../NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
- [October 8 architectural decision record](../NAVI_DOCUMENT_C_ARCHITECTURAL_DECISIONS_20261008.md)
- [0120 — PWM-zero movement](0120-pwm-zero-movement-requires-operator-position-verification-and-declaration.md)
- [0121 — current geography and overlay authority](0121-current-position-direction-and-overlay-determine-navi-operating-authority.md)
