# 0036 — A stationary IR wheel reports zero, but zero never grants upward motor authority

Status: Accepted (operator, 2026-08-15)

## Decision

An IR wheel whose completed-pulse count has not advanced for the established
stop interval reports an explicit `STOPPED` state and numeric speed `0.00`,
including when the wheel stops with the optical signal held on a spoke.

This partially supersedes decision 0005. Null remains correct for a missing
radio link, malformed or stale transport, and moving measurements suppressed by
marginal/reacquiring quality. `STOPPED` is distinct from those conditions.

For future control, a reported zero never authorizes an upward PWM correction.
If the active motion intent is zero, `STOPPED` confirms the requested result. If
motion is requested, `STOPPED` is published but the governor uses the existing
PWM preset and reports `MOTION_UNCONFIRMED`; it does not increase PWM in response
to the zero.

## Context

The IR Test Car correctly suppresses untrustworthy moving measurements, but a
wheel that is physically standing still has an ordinary, useful speed value:
zero. Stopping on a spoke can leave the signal on a fixed optical plateau and
must not leave the operator display at null indefinitely.

The sensor alone still cannot prove why edges ceased. A blinded sensor while
the wheel turns can imitate a stopped wheel. Keeping zero from requesting more
power preserves decision 0014's U-Haul rule while giving stationary operation
honest telemetry.

## Alternatives considered

- Keep every pulse timeout null — rejected by the operator because a genuinely
  stationary wheel should simply read zero.
- Treat zero as ordinary speed feedback under a positive target — rejected: a
  blind sensor could then cause the governor to add power.
- Use commanded PWM as proof of movement or rest — rejected: PWM is requested
  effort, not physical truth.

## Consequences

- The IR wire validity enum gains `STOPPED`; packet size does not change.
- `STOPPED` canonically carries numeric zero. Other non-valid states carry the
  invalid sentinel/null.
- The sender derives stop from absence of cumulative pulse progress, outside
  the proven acquisition path.
- Link loss remains null, never zero.
- Test A remains observation-only, so this change has no motor effect.
- A future governor must audit every zero-speed branch for the no-upward-
  authority rule.

## References

- `docs/IR_TEST_CAR_ESPNOW_FIRMWARE_SPEC.md`
- `docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md`
- `docs/QUORUM_IR_INTEGRATION_SPEC.md`
- decisions 0005 and 0014
