# 0089 - NAVI_IR coordinates mapped Hall landmarks with independent wheel travel

Status: Proposed experimental mechanism; implemented for review (2026-09-20)
Decided by: David requested the authority split, recovery opportunity, Toby
target and NAVI_IR 0.1 name. Codex selected the provisional mechanisms below;
their numerical limits and AUTO gate are not attributed to an operator ruling.

## Decision

Build NAVI_IR 0.1 as a new, explicitly experimental Toby sketch. Hall and route
history supply identity; the unpowered IR wheel supplies nominal distance.
Retain alternatives during ambiguity, publish the evidence and every retained
interpretation, and commit position only with joint consistency. Initial test
car input is ESP-NOW type 5, behind a transport-independent movement adapter.

## Context and alternatives

The operator rejected simply carrying forward symptom compensations from the
unsuccessful NAV trials. IR now offers an independent way to question a Hall
event even when its polarity matches. Noon repeatability is promising, but
does not justify inventing a hard distance tolerance or all-speed guarantee.

The implemented mechanism is a bounded one-fault hypothesis model with separate
IR anchors, nominal zero/one/two-interval association, ten-observation recovery,
and budget renewal after ten clean joint observations. It does not stop on the
first inconsistency or silently pick a cheaper branch. Unvalidated nominal
distance may expose alternatives, not hard-exclude a physically possible one.

## Consequences

Keep manual motor/MQTT integration, E-stop, brake and battery priorities. Gate
AUTO off pending independent review and physical tests. The included station
controller requires common actions across alternatives and stops on failed
approaches, rather than restoring cruise. No CTO/multi-train protection is
implemented in this experiment; no production capability is changed.

Costs: conditional one-fault certainty, receiver clock error, low-speed IR
limitations, 400 ms Hall judgement delay, baseline-fringe risk, and lack of a
reviewed wall-clock/travel fence if Hall observations cease. These remain field
gates, not hidden promises. No promotion into QUORUM and no flashing occurred.

## References

- `docs/NAVI_IR_0_1_IMPLEMENTATION_REPORT.md`
- `firmware/test-programs/NAVI_IR/README.md`
- `docs/IR_NOON_RUN_ANALYSIS_20260920.md`
- Operator instructions in this task, 2026-09-20.
