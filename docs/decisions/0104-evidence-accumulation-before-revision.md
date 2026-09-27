# 0104 - Evidence Accumulation Before Revision

Status: Accepted as a named canonical principle (2026-09-27, David with Sam).
Documentation only.
Decided by: David, after review with Sam. The quotations are from David's
codification instruction of 2026-09-27.

## Decision

**Evidence is not a conclusion.** "A datum is evidence. Its existence does not compel
a change in NAVI's world view." NAVI evaluates new information in the context of the
established world view and the other available evidence.

**EVIDENCE ACCUMULATION BEFORE REVISION.** "NAVI shall not revise a well-supported
world view merely because a new observation is inconsistent with it." When
contradictory evidence is insufficient to distinguish among plausible explanations,
NAVI:

- **OBSERVES** — preserves what actually occurred, including contradictions;
- **HOLDS** — retains the incumbent best-supported hypothesis rather than forcing a new
  conclusion;
- **RESOLVES** — uses subsequent independent information to confirm the incumbent or
  support a different one.

NAVI must neither discard inconvenient evidence nor let one unexplained datum erase
stronger accumulated knowledge. "Difficulty deciding is evidence that more information
may be required, not justification for manufacturing certainty."

**Loss of information does not invalidate established knowledge.** When IR becomes
unavailable, NAVI does not automatically forget its established position, baseline or
other coherent knowledge, nor manufacture replacements from weaker proxies. It retains
established information while it remains coherent and revises when sufficient evidence
justifies it.

**Preserve uncertainty.** "I do not yet have enough evidence to revise what I know" is a
valid and desirable decision state, and must be reconstructable in telemetry
(Transparent Judgment; 0105).

## Context

This names and generalizes the governing rule in
`../NAVI_DECISIONAL_INERTIA_CLARIFICATION_20260923.md`: "Unavailable evidence is not
contradictory evidence", and "decisional inertia is not permanence".

It resolved two D-items:
- **D6 (settled): the 3 s cadence gate is not carried forward as a no-IR fallback.**
  Its purpose was to avoid manufacturing a baseline under poor motion evidence. Under
  this principle NAVI need not replace an established baseline merely because IR became
  unavailable. If evidence later shows the baseline inadequate, that actual problem is
  evaluated then.
- **D9 (deferred): no distance-based replacement for `lostMs`** (disabled today). Whether
  Hall that stays elevated without IR is a real operational failure is not established.
  If field evidence demonstrates one, that phenomenon is characterized and solved.

## Alternatives considered

- **Revise on any contradiction; strike or re-acquire immediately.** Rejected: a single
  datum (for example run 2 #30, contaminated by handling) would overturn a coherent
  world view.
- **Never revise.** Rejected: inertia is not permanence. Run 3 showed an incumbent that
  was wrong by two MMs for 32 events; RESOLVES must be able to act.

## Consequences

- HOLD is a first-class outcome in NAVI telemetry, alongside advance and reject.
- **Risk: holding a wrong incumbent too long.** Run 3's `TRAVEL_UNAVAILABLE_HOLD` held a
  two-MM error with 15 contradictions and a unique fully consistent alternative. What
  counts as *sufficient* independent evidence to resolve is not specified here. That
  run's recovery mechanism awaits a controlled retest and is not decided by this record.

## References

- `../NAVI_DECISION_MODEL.md` §2, §6
- `../NAVI_DECISIONAL_INERTIA_CLARIFICATION_20260923.md`
- `0098-proximal-physical-recovery-before-polarity-scoring.md`
- `../../field-records/verdicts/20260926_toby_20q3_run2.md`, `…run3.md`
