# 0020 — No compensation mechanism is added without demonstrated need

Status: Accepted (2026-08-10)

## Decision

No firmware compensation, rejection, substitution, filtering, timeout,
debounce, inferred-fault response, or similar corrective mechanism is added
unless test or operational data first demonstrates the specific error it is
intended to address, **or an overwhelming case establishes the need before
the error is allowed to occur**.

The evidence must distinguish the physical or operational error from defects
in instrumentation, transport, logging, configuration, or analysis. The
proposal must cite that evidence, state the observed failure signature, and
define a test showing that the compensation corrects it without suppressing
valid behavior.

The exception is deliberately demanding. An overwhelming case must follow
from physical limits, protocol guarantees, a safety invariant, or comparably
strong prior evidence; explain why waiting to observe the failure would be
unsafe or unreasonable; bound the intervention narrowly; and still provide
an acceptance test and revert criterion. A merely plausible or imaginable
failure is not an overwhelming case.

Existing mechanisms are not presumed justified merely because they are
already present. When they materially affect a current decision, they are
audited against the same rule. Removal or alteration still follows the normal
review and safety gates; this decision does not authorize casual deletion.

## Context

The 2026-08-09 synchronized Hall + IR lap showed that the IR detector counted
approximately 5,238 spoke events over a complete Hall-confirmed circuit, while
only 1,310 individual PULSE detail records reached the combined log. Silent
MQTT/publish-queue loss made reliable local sensing appear to be a roughly
fourfold physical undercount. Earlier attempts to compensate for apparent
sensor failures therefore risked treating defects in the measuring system as
defects in the phenomenon measured.

The same investigation found a historical 15 ms debounce guard deleting valid
spoke events at speed. The guard was itself a source of the failure it was
supposed to prevent. Returning IR_DIAG to production's 2.5 ms value restored
approximately one pulse per optical peak in the replay evidence.

## Alternatives considered

- **Add defensive mechanisms for plausible failures.** Rejected: plausibility
  alone does not show that the failure exists, and interacting protections can
  suppress valid data while obscuring the actual cause.
- **Retain every existing protection unless conclusively disproved.** Rejected
  as a presumption of correctness. Existing mechanisms require evidence when
  they become relevant, although changes still require controlled review.
- **Remove all compensation immediately.** Rejected: some mechanisms may have
  established safety or field evidence, and uncontrolled removal could create
  new hazards.

## Consequences

- Every new compensation proposal carries an evidence reference and a
  falsifiable acceptance/revert test, or documents the overwhelming case to
  the same standard before implementation.
- Raw observations and monotonic source counters are preserved separately
  from corrected, filtered, inferred, or transported representations.
- A received MQTT message count is never used as a physical-event count unless
  delivery completeness is independently established.
- Reviews challenge mechanisms whose protected-against failure has not been
  observed; they do not invent speculative fault cases to justify complexity.
- Tomorrow's sunlight work first measures whether glare, saturation, contrast
  collapse, false edges, latching, or missed spokes actually occur. It does
  not begin by adding remedies for them.

## References

- `docs/IR_DEV_REC/2026-08-09_SYNCHRONIZED_HALL_IR_LAP.md`
- `field-records/logs/20260809_dualcap_otto-IR_2303-2306_estop.log`
- `field-records/logs/20260809_dualcap_otto-IR_2303-2306_estop_localtime.log`
- IR_DIAG debounce correction `8adadc8`
