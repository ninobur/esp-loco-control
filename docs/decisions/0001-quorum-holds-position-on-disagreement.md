# 0001 — QUORUM replaces tally navigation: position is held on disagreement, never discarded

Status: Accepted  (2026-08-01; recorded 2026-08-06 at adoption of this log)

## Decision
The Layer 3 navigator holds its position estimate when marker evidence
disagrees, evaluates six candidate offsets `{-1,0,+1,+2,+3,+4}` against an
evidence ring, adopts a correction only on a unique two-point lead, and
declares `NAV_NO_QUORUM` (controlled stop, operator re-declares) rather than
ever rebuilding position from nothing.

## Context
The DNA tally navigator's `navConfidence` could express *how much* it
disagreed but not *which position it might be in*; when confidence emptied it
discarded position entirely. A locomotive on rails is not flying: "I knew
where I was a minute ago" is evidence that a counter cannot represent.

## Alternatives considered
- Retuning tally thresholds — rejected: the failure is representational, not
  parametric.
- Automatic recovery from NO_QUORUM — rejected: a navigator that talked
  itself out of a terminal state once will do it again; the operator is the
  only party with ground truth.
- Reducing speed while evaluating — rejected: evaluation must not change
  behaviour the operator can feel before a verdict exists.

## Consequences
One disagreement is free. Corrections are single, applied once, validated by
the next agreement. NO_QUORUM is terminal by design. Twenty navigator
properties are certified in the spec and survive translation
character-for-character.

## References
`docs/QUORUM_v3_0_implementation_spec.md` (R21); QUORUM 1.0–1.4
implementation reports; `docs/STATUS.md` §3.
