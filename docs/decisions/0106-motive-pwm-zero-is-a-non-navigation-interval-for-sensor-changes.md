# 0106 - Motive PWM = 0 is a non-navigation interval for sensor changes

Status: Accepted as principle (2026-09-27, David with Sam). Resolves D3.
Documentation only.
Decided by: David, after review with Sam. The physical premise is David's
statement of Toby's behavior; the quotations are from his codification
instruction of 2026-09-27.

## Decision

"At motive PWM=0, Toby is stopped under normal locomotive control. There is no
ordinary motive coasting after motive PWM reaches zero."

Therefore changes in Hall or IR data while motive PWM = 0 are not NAVI-controlled
route progression. They may come from:
- operator handling;
- movement of part of the consist;
- movement or tilting of the IR car;
- changing Hall behavior while stopped in or near a field.

NAVI retains and reports them. They have no authority to:
- advance, retreat or correct position;
- accumulate route displacement;
- cause a compensating navigation response.

NAVI may transparently report anomalous activity. The established position remains
the incumbent (0104) unless other sufficient evidence later contradicts it.

**Operator declaration and redeclaration are different:** they are operator authority
intentionally changing NAVI's world view. By normal practice, small operator
adjustments are returned to substantially the same position; deliberate material
relocation requires redeclaration. When controlled movement resumes, later evidence
confirms or challenges the retained world view.

**IR at PWM 0.** IR wheel movement at PWM 0 may validly measure wheel rotation, but it
cannot be interpreted as signed route displacement: it may have gone either way, or
come from handling the measuring car. It must not contaminate the prior MM/IR
relationship. If stopped IR activity has broken a trustworthy MM/IR relationship,
later IR may again measure movement, but it regains route-location authority only
through a trustworthy route reference: normally the next accepted MM, or an operator
declaration.

## Context and evidence (checked 2026-09-27 against the committed 20Q3 logs)

Across runs 1–5, IR pulses increased at applied PWM 0 only in these cases:
- **Standing optical flicker:** run 1 (2 pulses, 27 s after PWM > 0, while standing)
  and run 2 (1 pulse at the recorded IR-car tilt).
- **Recorded hand handling:** run 4a (positioning before declaring, 14 pulses) and
  run 4b (clearing the obstruction while E-stopped, 72 pulses).
- **Station stops:** run 4 Patio and Bamboo showed **zero** pulses at PWM 0; Toby had
  stopped while PWM was still ramping down (for example Patio: pulses stopped at
  09:55:35.4, DWELL_BEGIN at PWM 0 at 09:55:44.2).

This agrees with David's premise and with Toby's calibrated tractive floor
(`speed = 3.990 × (PWM − 25.1)`, `LL_LocoConfig_9950012.h`). The run 4 IR-car-only
movement at 10:00:37–42 happened at PWM 75 during a stall. This principle does not
cover it; it remains an example of IR moving without the locomotive.

## Alternatives considered

- **Accept IR-measured travel at PWM 0 as route progression** (plan D3 as drafted).
  Rejected: IR at PWM 0 is not signed and is dominated by handling.
- **Reproduce X22's PWM-zero suppression as a NAVI fallback** (plan D2 as drafted).
  Superseded: the data are retained and reported rather than suppressed; they simply
  carry no navigation authority.

## Consequences

- Supersedes the premise of X22's PWM-zero hold (N2) as an upstream suppression. The
  concept survives as a NAVI principle with the data visible.
- **OPEN:** whether a completely motionless, normal PWM-0 station stop must break an
  otherwise valid IR/MM reference. This is a separate physical question and is not
  decided here.
- **Risk.** The premise is Toby's. Another locomotive, a grade, or a consist pushing
  may coast at PWM 0; the principle then needs re-examination with measured evidence.
  Motion below the tractive floor (PWM 1–25) is outside this record.

## References

- `../NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md` (enduring principles)
- `../NAVI_DECISION_MODEL.md` §3
- `../../field-records/verdicts/20260926_toby_20q3_run{1,2,4}.md`
- `../NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md` (N2, N3, D2, D3, R4)
