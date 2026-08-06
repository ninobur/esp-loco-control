# 0003 — There is one control sketch; MANUAL is retired to reference

Status: Accepted  (operator ruling 2026-08-05; recorded 2026-08-06)

## Decision
QUORUM is the only control firmware. The separate MANUAL sketch moved to
`firmware/test-programs/`, restated as reference-only, never to be flashed.

## Context
During development it made sense to test automatic functions from a manual
sketch, but a separate manual build meant a USB cable to change modes, and
the two sketches had already begun drifting as fixes landed in one and not
the other. Operator: "There is only one control sketch. It is bicameral."

## Alternatives considered
- Keeping MANUAL as a fallback firmware — rejected: mode changes must not
  require reflashing, and a second sketch is a second place for bugs.
- Deleting MANUAL entirely — rejected: with the automatic chamber deleted
  rather than gated, it has zero navigation-originated motor writes by
  construction — the property QUORUM must achieve with gates. It is the
  audit reference for the bicameral boundary.

## Consequences
Dispatcher STOP requires enlistment; broadcast E-STOP is subscribed in the
one firmware; the operator direction rule (refuse above PWM 20, snap-zero
then restore at or below) lives in QUORUM. MANUAL receives no fixes.

## References
Commit `b79b879` (QUORUM 1.6 + move); `docs/STATUS.md` §2.
