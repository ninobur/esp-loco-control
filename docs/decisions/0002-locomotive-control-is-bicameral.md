# 0002 — All NGR locomotive controllers are bicameral

Status: Accepted — constitutional  (operator ruling 2026-08-02; recorded
2026-08-06; revised 2026-08-30 — operator audit separated his policy from
the implementation mechanism this record had conflated with it)

## Decision
The operator's policy (his words, 2026-08-30): "How manual operations
progress and how automatic operations progress follow different rules."

Concretely: two chambers. MANUAL: the operator is sovereign; navigation
observes, records, publishes and warns, but never writes to the motor.
AUTO: navigation acts with full authority. E-STOP belongs to the operator
and works in every chamber and every state.

## Mechanism (implementation choice, not operator policy)
"Lexically gated on `autoRunning`" — wrapping each individual
navigation-originated motor-write call site in `if(autoRunning)` at the
point of the call — is the specific technique an agent chose during
QUORUM 1.3 to enforce the policy above in code. It is an engineering
choice, not something the operator was asked about or decided. Other
techniques could satisfy the same policy: a single choke-point function all
motor writes pass through, a runtime assertion, or fully separate
manual/automatic firmwares (this last rejected below for unrelated
reasons).

**Known limitation, stated per the operator's 2026-08-30 ruling that a
principle's risk must be recorded alongside its benefit:** this mechanism
is enforced by manual code review, not by an automated test — no test in
`firmware/programs/QUORUM/tests/` checks chamber gating. It has already produced one
documented near-miss caught only by a human reviewer reading the code, not
a fixture (`QUORUM.ino:3454`, the CODEX 1.16 resume-interlock finding: "the
harness never calls serviceStations(), which is why no fixture could catch
this — the reviewer read the loop instead"), and one historical violation,
now fixed (`QUORUM.ino:4058` — a dispatcher STOP once zeroed the throttle
of a locomotive that had never enlisted in AUTO). The guarantee holds only
as long as every future motor-write call site is manually re-checked
against it.

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
Every navigation-originated motor write is intended to be lexically gated
on `autoRunning`. Operator paths are never gated. Exactly three chamber
crossings exist: E-STOP (always), enlistment (`cmd/auto`, the locomotive's
own act), release (dispatcher side). Every future navigator inherits the
policy above as spec §0.2, and every motor-write audit asks "which
chamber?" first — this is a standing manual-review obligation, not a
mechanically enforced one (see Mechanism, above).

## References
Spec §0.2 and R21; `QUORUM_1_3_IMPLEMENTATION_REPORT.md` (17-site audit);
amended in `1_4`. Revision 2026-08-30: operator decision-log audit;
`firmware/programs/QUORUM/QUORUM.ino:1846,3454,4058`.
