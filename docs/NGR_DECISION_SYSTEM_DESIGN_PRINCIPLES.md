# NGR decision-system design principles

**Canonical and enduring.** These principles guide the architecture of any NGR
system that makes decisions from sensor data: locomotive navigation, traffic
coordination, stations, dispatch, and whatever replaces them.

- They were established by David and Sam in September 2026.
- They are meant to outlive any particular implementation.
- Read this document before proposing an architecture or an implementation.

It is not a status report, a plan, or an authorization. Implementation
authority comes from the operator, as set out in [`/AGENTS.md`](../AGENTS.md).
Specific applications of these principles are recorded in
[`decisions/`](decisions/README.md). The current navigation application is
[`NAVI_DECISION_MODEL.md`](NAVI_DECISION_MODEL.md).

---

## 1. Put judgment where the information is

Processing may be distributed. A consequential system decision should be made at
the point that holds the relevant available context.

Do not delegate a system-significant judgment to a lower-context component merely
because it is closer to a sensor.

*Example.* A sensor module may decide that a signal crossed a threshold. Whether
that crossing is a landmark is for the component that also knows position,
distance travelled and the map.

## 2. Do not simplify the decision-maker by withholding information

Signal conditioning and measurement processing are legitimate. Withholding
relevant evidence because an upstream component has already interpreted it is
not.

The decision-maker is not to be made simpler by making irreversible decisions
earlier, where less information is available.

*Example.* A detector that silently discards a second signal because "it came too
soon" has made a navigation judgment on the decision-maker's behalf, with less
context than the decision-maker has.

## 3. Prefer direct physical measurement to proxies

When the physical quantity relevant to a decision can be measured, use the
measurement. Do not substitute elapsed time, commanded power or another
convenient correlate because it is easier to implement.

Proxies remain useful when direct information is unavailable, as degraded-mode
evidence.

*Example.* When distance travelled is measured, use it; do not infer distance
from a timer.

## 4. Understand why legacy restrictions existed

Many NGR rules were rational open-loop workarounds for information scarcity.
Before carrying one forward, ask:
- What uncertainty made this restriction necessary?
- Does the system now have information that resolves that uncertainty?

Better information should permit greater freedom of appropriate action.

*Example.* A following train once could not leave a station until the leader
reached the next one, because that arrival was the only reliable information the
follower had. With continuous knowledge of the leader's position, the same wait
is a restriction without a reason.

## 5. Evidence is not a conclusion

A datum becomes meaningful in context. Preserve observations without requiring
each observation to change the world view.

## 6. Evidence Accumulation Before Revision

Do not revise a well-supported world view merely because one new observation
conflicts with it:
- **Observe:** preserve the discrepancy.
- **Hold:** retain the incumbent view while it remains best supported.
- **Resolve:** use subsequent, independent information to confirm the incumbent
  or support a different view.

Neither discard inconvenient evidence, nor let one unexplained datum erase
stronger accumulated knowledge. Inertia is not permanence: when accumulated
evidence supports a coherent alternative, revise.

## 7. Loss of information does not invalidate established knowledge

When information becomes unavailable, retain previously established knowledge
while it remains coherent. Do not manufacture replacement knowledge from weaker
evidence simply because the preferred source disappeared.

*Example.* When a distance instrument drops out, the known position and a
well-established reference remain known. The system does not rebuild them from
timing because the better instrument went quiet.

## 8. Do not force decisions under inadequate information

Difficulty deciding often means more information is needed. "Insufficient
evidence to revise" is a legitimate system conclusion. Represent uncertainty;
do not convert it into artificial certainty.

## 9. Evaluate evidence relationally and contextually

Heterogeneous evidence is not a collection of interchangeable votes. Do not
reduce unlike evidence to arbitrary points or weighted averages merely to obtain
a decision.

Determine what each datum means in context, and how one observation constrains
the interpretation of another. Quantify genuine physical quantities (distance,
speed, signal departure, variance, spacing, tolerance). Keep the judgment itself
explicit and reconstructable.

*Example.* A measured distance does not "add points" to a candidate landmark; it
determines which landmarks were physically reachable at all.

## 10. Distinguish measurement from meaning

A sensor may accurately report what happened to the sensor without establishing
what happened to the system. Measurement provenance and operating context
determine what conclusions a measurement can support.

*Example.* A measuring wheel that turns while the locomotive is stopped has
measured a real rotation, perhaps from someone handling the car. It has not
measured locomotive travel.

## 11. Preserve knowledge, not inherited authority structures

Previous systems are prior art:
- Retain experimentally demonstrated concepts and useful algorithms.
- Create new authority structures from current principles and current
  information.
- Existing code has no presumption of architectural survival merely because it
  works or already exists.

## 12. Recognize the phenomenon rather than memorize its proxy

Where possible, identify the physical or signal phenomenon itself: movement,
return to baseline, location, stopping position. Do not encode the time, command
sequence or other circumstance that happened to correlate with it in an earlier
experiment.

*Example.* A fixed delay after a signal drops memorizes one locomotive's speed
on one day. Recognizing that the signal has returned to ordinary-track
behavior describes the phenomenon.

## 13. General model first; exceptions from evidence

Begin with a model based on general physical principles, and test it across
equipment and circumstances. Introduce equipment-specific accommodations only
when observations show that meaningful differences actually exist.

The model needs enough discrimination for the task, not unnecessary precision.

## 14. More information should enable better decisions, not merely more telemetry

Adding sensors while keeping the restrictions created by their former absence
defeats the purpose of acquiring the information.

## 15. Transparent judgment is part of correctness

Important decisions should be reconstructable as:

```
QUESTION -> DATA -> CONTEXT -> ALTERNATIVES -> EVALUATION -> DECISION/HOLD -> RESOLUTION EVIDENCE
```

The system should be able to explain:
- what it knew;
- what changed;
- what alternatives remained plausible;
- why it retained or revised its view;
- what further evidence could resolve the uncertainty.

A decision that cannot be reconstructed cannot be reviewed, and a decision that
cannot be reviewed cannot be trusted on the railway.

## 16. Measurement applicability: absence of measured change is not sensor failure

A measurement must be interpreted in the context of the physical phenomenon
being measured.

For a movement sensor, no measured displacement is itself a valid measurement
result:

> No significant movement was measured.

That result does not, by itself, mean that the sensor is unavailable,
unhealthy, or incapable of measurement.

Distinguish:

- **measurement result:** no significant movement measured;
- **internal diagnostic:** insufficient optical variation in the detector's
  current window to establish contrast;
- **measurement failure:** independent evidence establishes that significant
  physical movement occurred while the measurement system failed to measure it.

An internal detector diagnostic must not automatically be promoted into the
operational conclusion that the measurement system failed. Evaluate a
measurement system against the physical phenomenon it is intended to measure,
and interpret the result at the component that has the relevant physical
context.

This principle does not claim that a movement sensor can measure arbitrarily
small displacement. "No significant movement measured" respects the
measurement's actual resolution and operating envelope.

---

## Using these principles

- An architecture or reconciliation document should state, near its top, how it
  applies this document. It should name any principle it deliberately departs
  from, and why.
- A principle here does not authorize any change. Proposals still go to the
  operator first ([`/AGENTS.md`](../AGENTS.md) §2).
- When field evidence contradicts a principle's application, record the evidence
  and re-examine the application. Do not quietly work around it.
