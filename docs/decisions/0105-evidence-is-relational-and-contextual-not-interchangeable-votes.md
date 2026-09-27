# 0105 - Evidence is relational and contextual, not interchangeable votes

Status: Accepted as a named canonical principle (2026-09-27, David with Sam).
Documentation only.
Decided by: David, after review with Sam. The quotation is from David's
codification instruction of 2026-09-27.

## Decision

"NAVI must not reduce heterogeneous evidence to an arbitrary numerical voting or
confidence system merely so unlike evidence can be added together."

- Evidence obtains meaning from context and from its relationship to other evidence.
- A direct physical measurement may **constrain the interpretation** of another datum,
  rather than merely add weight to one side. For example, a measured distance
  determines which mapped MMs a Hall opening could physically be.
- Physical quantities are quantified where appropriate: distance, speed, Hall
  departure, variance, acceleration, mapped spacing, measurement tolerance.
- Judgment stays explicit and reconstructable:

```
QUESTION -> DATA -> CONTEXT -> ALTERNATIVES -> PHYSICAL COHERENCE -> DECISION/HOLD -> WHAT COULD RESOLVE UNCERTAINTY
```

A single scalar confidence score is not a substitute for this reasoning.

## Context

Earlier lineages combined unlike signals into votes or scores. The 20Q3 window
already works relationally: the IR distance from the last MM/IR synchronization
decides *which* observation is eligible for the expected MM, rather than adding a
vote. Sam's "Transparent Judgment" framing (0091, endorsed by David) asks for the
reasoning to be visible, which a scalar hides.

## Alternatives considered

- **A weighted confidence score across Hall, IR, PWM and time.** Rejected: it forces
  commensurability onto incommensurable evidence and hides which datum constrained
  which.

## Consequences

- Telemetry records the chain above for each judgment, including HOLD (0104).
- **To re-examine:** PROXIMAL_R1 (0098) scores polarity matches over ten positions.
  That score counts one kind of observation against the map; it is not a mix of
  heterogeneous evidence, and physical feasibility is applied first as a constraint.
  It should nonetheless be reviewed against this principle when recovery is next
  revised.
- **Risk.** Explicit relational reasoning is harder to specify and test than a score.
  Each judgment's question and alternatives must be stated, or the principle degrades
  into undocumented special cases.

## References

- `../NAVI_DECISION_MODEL.md` §2, §4
- `0091-ir-trust-is-earned-kept-through-stops-and-revoked-by-introspection.md`
  ("Transparent Judgment")
- `0092-navi-ir-decision-model-physically-possible-positions-first.md`
