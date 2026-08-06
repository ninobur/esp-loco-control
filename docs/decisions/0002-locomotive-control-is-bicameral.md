# 0002 — All NGR locomotive controllers are bicameral

Status: Accepted — constitutional  (operator ruling 2026-08-02; recorded 2026-08-06)

## Decision
Two chambers. MANUAL: the operator is sovereign; navigation observes,
records, publishes and warns, but never writes to the motor. AUTO:
navigation acts with full authority. E-STOP belongs to the operator and
works in every chamber and every state.

## Context
QUORUM 1.0–1.2 regressed a property v2.22 had honoured: the NO_QUORUM
terminal stop called `requestPwm(0)` unconditionally — a navigation override
of a manual operator. The certified property "NAV_NO_QUORUM stops once" was
reviewed by everyone and chamber-gated by no one. A property can be correct
as specified and still wrong if nobody asks which chamber it belongs to.

## Alternatives considered
- Case-by-case gating of "safety" writes — rejected: produced exactly the
  stray this ruling exists to prevent; the class of exempt "motor-safety
  facts" was audited and found empty (CODEX reclassified its only member).
- Separate manual and automatic firmwares — rejected separately (0003).

## Consequences
Every navigation-originated motor write is lexically gated on `autoRunning`.
Operator paths are never gated. Exactly three chamber crossings exist:
E-STOP (always), enlistment (`cmd/auto`, the locomotive's own act), release
(dispatcher side). Every future navigator inherits this as spec §0.2, and
every motor-write audit asks "which chamber?" first.

## References
Spec §0.2 and R21; `QUORUM_1_3_IMPLEMENTATION_REPORT.md` (17-site audit);
amended in `1_4`.
