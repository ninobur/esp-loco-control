# 0108 - Measurement applicability: absence of measured change is not sensor failure

Status: Accepted as principle (2026-09-27, David with Sam). Documentation only.
Decided by: David, after review with Sam.

## Decision

A measurement must be interpreted in the context of the physical phenomenon
being measured. For a movement sensor, no measured displacement is a valid
result: **no significant movement was measured**. That result does not, by
itself, establish that the sensor is unavailable, unhealthy or incapable of
measurement.

Keep three statements distinct:

- the measurement result: no significant movement measured;
- an internal diagnostic: insufficient optical variation in the detector's
  current window to establish contrast;
- measurement failure: independent evidence shows that significant physical
  movement occurred while the measurement system failed to measure it.

In particular, an IR `INADEQUATE_CONTRAST` diagnostic is a statement about the
detector's current optical window. It must not automatically become the
upstream conclusion that IR failed or that no useful movement result exists.
NAVI and other upstream consumers must use the physical context they possess
to determine what the observation means.

## Context

The September 27 NSR1 field capture provides a supporting example, not the
definition of this principle. During 699 movement-window snapshots labeled
`INADEQUATE_CONTRAST`, the preceding and following pulse counts showed no
measured displacement in those intervals. The capture was after dark; the
label therefore cannot be explained by sunlight, and it does not by itself
demonstrate an IR hardware failure. In the same capture, sustained running
with advancing IR pulses did not carry that label. The observation-only NSR1
recorder was downstream of IR processing and did not feed back into it.

## Alternatives considered

- Treat every `INADEQUATE_CONTRAST` report as equivalent to total IR failure.
  Rejected: it confuses an internal detector condition with a system-level
  conclusion and ignores the physical state being measured.
- Change the detector's diagnostic vocabulary as part of this decision.
  Rejected: the distinction is interpretive; no firmware or wire-format change
  is authorized by this record.
- Infer movement from PWM merely to clear an optical diagnostic. Rejected:
  commanded or applied propulsion is context, not a substitute for measured
  displacement when displacement is the question.

## Consequences

Measurement consumers must preserve the raw diagnostic and the measurement
result separately. A true measurement failure remains possible and must be
recognized when independent evidence establishes significant movement that the
instrument did not measure. Conversely, a no-change result must not be turned
into a guessed zero or an automatic sensor-failure conclusion without the
relevant physical context.

This record changes documentation and interpretation only. It does not change
thresholds, detector states, NAVI behavior, Hall logic, dashboard behavior or
control authority.

## References

- [`NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`](../NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md) §16
- [`NAVI_DECISION_MODEL.md`](../NAVI_DECISION_MODEL.md) §2
- [`0106-motive-pwm-zero-is-a-non-navigation-interval-for-sensor-changes.md`](0106-motive-pwm-zero-is-a-non-navigation-interval-for-sensor-changes.md)
- [`../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/README.md`](../../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/README.md)
- [`NAVI_SYNC_NATIVE_HALL_IR_RECORDER_20260927.md`](../NAVI_SYNC_NATIVE_HALL_IR_RECORDER_20260927.md)
