# Templates briefing and approval process

**Status:** Proposal only. Not approved. Authorizes no implementation.

## Purpose

Templates is a new navigator design. Before design or implementation continues,
the operator must receive a short, meaningful briefing and explicitly approve
each significant decision.

## Authority reset

No QUORUM decision, specification, approval, test, or implementation choice is
authoritative for Templates.

QUORUM material may be consulted only as historical evidence about observed
railway, sensor, or software behavior. Nothing carries forward automatically,
including quarantine, offsets, confidence, `HARD_BOUND`, timing gates, recovery
logic, or acceptance tests.

Every Templates behavior requires fresh, narrowly scoped operator approval.

## Required briefing

Before any significant change, present:

1. **Problem** — one plain-language paragraph.
2. **Current behavior** — what the locomotive does now.
3. **Proposed behavior** — what would change operationally.
4. **Evidence** — why the change is being considered.
5. **Risks** — what could go wrong.
6. **Alternatives** — including doing nothing.
7. **Approval requested** — one exact, bounded decision.
8. **Not approved** — related work explicitly outside the request.

Technical detail follows only when requested or when necessary to understand the
decision.

## Approval rules

- Discuss one decision at a time.
- Briefing and operator questions come before approval.
- No design assumption or implementation begins without explicit approval.
- Approval applies only to the stated scope and operating context.
- Approval may not be generalized to another feature, model, or condition.
- Operator corrections become part of the governing context for that decision.
- Any material deviation requires a new briefing and approval.
- Implementation is checked against the approved briefing, not a later AI
  interpretation.

## Approval record

Each approved decision must preserve:

- the complete briefing;
- the operator's questions and corrections;
- the exact approval statement;
- the approved scope;
- the explicitly excluded scope; and
- the evidence available when approval was given.

A summary, commit message, or piecemeal answer is not a substitute for this
record.

## Immediate boundary

Until this process is approved, no Templates design, code, tests, decision
records, firmware changes, flashing, or train operations are authorized.
