# 0106 - Motive PWM = 0 is a non-navigation interval for sensor changes

Status: Accepted as principle (2026-09-27, David with Sam). Resolves D3.
**Partially superseded by 0112 (2026-09-29)** for IR at PWM = 0 and the OPEN
station-stop question; Hall-at-PWM-0 rules stand. Body below unchanged.
Documentation only.
Decided by: David, after review with Sam. Wording corrected on David's
instruction, 2026-09-27 (see Revision at the end).

## Decision

**PWM = 0 means no movement. Period.**

For NAVI, motive PWM = 0 is a non-navigation interval: NAVI is not moving the
locomotive. Changes in Hall, IR or other movement-related sensor data during that
interval are not locomotive route movement for navigation purposes. They may come
from:
- operator handling;
- movement of part of the consist;
- movement or tilting of the IR car;
- changing Hall behavior while stopped in or near a field.

They may be retained, reported and available as context. They have no navigation
authority. They cannot:
- advance, retreat or correct position;
- accumulate route displacement;
- cause a compensating navigation response.

NAVI may transparently report anomalous activity. The established position remains
the incumbent (0104) unless other sufficient evidence later contradicts it.

**"Coasting" is simulated by PWM (David, 2026-09-27).** "Coasting is a simulation by
applying a slow ramp. The speed is absolutely controlled by pwm. That behavior is still
present in the current manual mode. To the operator, it feels like the train is
coasting, but the movement is 100% determined by pwm."
- In code, the manual brake setting only sets the down-ramp step rate: 400 ms per
  count at rest ("coast") down to 15 ms per count (hard brake) (`BRAKE_STEP_COAST_MS`,
  `brakeStepMs()` in the sketch).
- The locomotive follows the ramp. What the operator perceives as coasting is
  motion at PWM > 0. Once PWM reaches 0, the locomotive is not moving.
- Wherever older NGR text speaks of coasting after PWM reaches zero, it describes a
  behavior that does not exist.

**Operator declaration and redeclaration remain authoritative:** they are operator
authority intentionally changing NAVI's world view. By normal practice, small operator
adjustments are returned to substantially the same position; deliberate material
relocation requires redeclaration. When controlled movement resumes, later evidence
confirms or challenges the retained world view.

**IR at PWM 0.** Wheel movement recorded while PWM = 0 may be real movement of the
measuring wheel, including operator handling. It is not locomotive route movement and
cannot be used as signed route displacement. It must not contaminate the prior MM/IR
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

This is consistent with the decision. (The run 4 IR-car-only movement at
10:00:37–42 occurred at PWM 75 during a stall, not at PWM 0.)

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
- The principle is general. It follows "general model first; exceptions from evidence"
  (`../NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md` §13) and is not weakened in
  anticipation of hypothetical evidence.

## References

- `../NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md` (enduring principles)
- `../NAVI_DECISION_MODEL.md` §3
- `../../field-records/verdicts/20260926_toby_20q3_run{1,2,4}.md`
- `../NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md` (N2, N3, D2, D3, R4)

## Revision

2026-09-27, on David's instruction: the first wording of this record (`948b90b`)
stated the principle as a Toby-specific premise. Its Risk paragraph also added
exceptions that David and Sam did not decide: another locomotive, a grade or a
consist coasting at PWM 0, and motion below the tractive floor. The Decision now
states the principle as decided, and those exceptions are removed. The evidence
section is unchanged in substance.

2026-09-27, on David's instruction: added the clarification that "coasting" is a
PWM ramp simulation. Movement is entirely determined by PWM, and there is no
movement after PWM reaches 0.
