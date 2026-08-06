# 0009 — The diagnostic uses production detection thresholds, with a written revert criterion

Status: Accepted  (2026-08-06)

## Decision
IR_DIAG detects edges with IR_TEST's exact expressions — rise at
`runMin + 2·span/3`, fall at `runMin + span/3`, symmetric about the
midpoint — replacing an undocumented asymmetric pair (0.83/0.67 of range)
that nobody could establish was deliberate.

## Context
A diagnostic whose job is to validate production behaviour must not use
different detection rules from production; any defect it finds or misses
is otherwise unattributable. Secondary: on the captured window the minimum
available edge margin improves from 194 to ~356 counts.

## Alternatives considered
- Keeping the asymmetric pair — rejected: undocumented, unattributable,
  and divergent from production.
- Tuning new fractions from the darkness data — rejected: darkness data
  cannot answer a daylight question; change one thing and instrument it.

## Consequences
The trade is explicit: lowering the falling threshold helps a weak rise
and makes a shallow trough harder to catch (darkness replay: fall margin
estimate fell from ~+774 to ~+232 while rise margin rose). The revert
criterion is written down: if rising headroom sits far above falling
headroom in daylight, the asymmetric pair was accidentally right and this
reverts. Decision 0010's numbers judge it.

## References
Commit `47ddc6e` (qualification in message); `IR_DIAG_DAYLIGHT_PREP.md`.
