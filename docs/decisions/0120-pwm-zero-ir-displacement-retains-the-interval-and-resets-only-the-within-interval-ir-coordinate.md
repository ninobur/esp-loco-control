# 0120 — PWM-zero IR displacement retains the interval; only the within-interval IR coordinate is lost

Status: Accepted for NAVI_EWO by David, 2026-10-02. Recorded for David and
Sam's review. Refines 0112 and partially supersedes 0116 (see below).
Documentation only: no firmware change, flashing or deployment is authorized
by this record. The current integrated candidate does **not** conform (see
Implementation status).
Decided by: David, in his 2026-10-02 instruction following Otto's run
`9950011_20261002_124930.log`. Quoted phrases below are his.

## Decision

When NAVI observes IR displacement while motive PWM = 0, **the observation is
factual**, but that displacement has no known relationship to signed route
travel. PWM-zero movement is an observed physical event that makes one
component of localization unknown. It is not a navigation error.

> "NAVI loses IR knowledge of where the locomotive is within the
> already-established MM interval. It does not lose the identity of that
> interval."

Two relationships are distinguished, and must be named separately:

1. **Interval identity / map context: retained.** PWM-zero IR displacement
   does not by itself change or invalidate the established MM interval; the
   last established MM; the expected adjacent target MM; the
   operator-established route direction/context; or the Hall reference, unless
   it is independently invalidated for another reason. The sensor observation
   has no authority to infer that the locomotive has left the established
   interval.
2. **IR coordinate within that interval / distance-to-target relationship:
   invalidated and must be re-established.** NAVI no longer knows the
   locomotive's precise IR-derived coordinate within the interval. IR
   displacement accumulated during PWM = 0 must not be interpreted as signed
   route distance or used to advance position.

**When powered movement resumes,** NAVI is in the startup problem: *interval
known; exact within-interval IR coordinate unknown.* It observes subsequent
powered movement and Hall evidence and re-establishes the precise IR/MM
landmark relationship when the appropriate bounding/target magnet is
encountered. The startup mechanism is the 0118 first-target rule. This is
analogous to startup **only** with respect to within-interval IR localization.
It does not return NAVI to an undeclared state.

**Operator authority.** Redeclaration remains an operator decision. If the
operator knows the locomotive was moved to another interval (for example, David
picks Otto up, turns it around and places it elsewhere), the operator may
redeclare. NAVI must not infer an interval change from PWM-zero IR
displacement.

> "PWM-zero IR displacement may require re-establishing within-interval IR
> localization. It does not itself require redeclaration of the interval."

**Terminology.** Do not write "PWM-zero movement destroys the IR/map
relationship" or "map/IR relationship unreliable". Those phrases blur the two
relationships above. Say which one is meant.

## Context

On 2026-10-02 Otto (build `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2`) produced 28
`PWM_ZERO_IR_DISPLACEMENT` events (13:30:40–13:31:05) while David picked up and
turned the train between a CCW and a CW session. The measuring wheel spun in
hand, so the counts are real wheel movement but no route distance. David then
redeclared 045–046, because he knew he had placed Otto in a different interval,
**not because the PWM-zero event required it.** Field record:
`../../field-records/verdicts/20261002_otto_ewo-r2-ft2_ccw-cw-stations.md`.

0112 and 0116 had treated the same event as loss of the whole mapped
relationship. 0116 wrote that IR movement during PWM = 0 "destroys the mapped
distance relationship" and that redeclaration is "the authorized restoration".
In effect that grouped a handled-but-unmoved locomotive with a genuine IR frame
break.

## Alternatives considered

- **Keep 0116's rule (PWM-zero displacement = frame loss, redeclare).** Not
  chosen. It discards interval identity that no evidence contradicts, and it
  makes the operator restore what NAVI never lost.
- **Use the PWM-zero counts as route distance.** Not chosen. No signed route
  displacement can be established at PWM = 0 (0106, 0112).
- **Infer an interval change from the amount of PWM-zero displacement.** Not
  chosen. The sensor has no authority to do so, and alternative-position search
  is excluded (0109).

## Consequences and risk

- **Accepted risk:** if the locomotive really was moved to another interval
  and the operator does not redeclare, NAVI will accept the next magnet that
  satisfies the retained target as that target. Position is then silently one
  or more markers off. This is the same class of limit as 0118's undetected
  first magnet. Operator redeclaration is the safeguard.
- **Interpretation for review (agent inference, not David's words):**
  - Applying 0118 "as at startup" implies there is no distance window and no
    Missed Magnet ruling for the retained target until it is re-established.
  - A reversal while the coordinate is unknown is not addressed by this
    record. 0116's reversal arithmetic needs a known coordinate.
  - David and Sam should confirm both points before implementation.
- **Unchanged:**
  - Genuine IR frame breaks (source, boot, calibration, scale or order) remain
    governed by 0116: hold, then operator redeclaration.
  - A motionless PWM-zero stop keeps the full relationship (0112).
  - Hall at PWM = 0 stays non-actionable (0112).

## Relationship to earlier decisions

- **0112: refined.**
  - *Survives:* "IR displacement at PWM = 0 is factual", not an instrument
    failure, and the rule against guessing the new position.
  - *Refined:* "mark the relationship between accumulated IR distance and
    mapped position unreliable" now means only the within-interval
    coordinate/distance-to-target relationship. Interval identity is retained.
  - 0112's degraded-Hall re-anchor stays withdrawn (0116). Re-establishment
    uses the 0118 Hall+IR startup rule instead.
- **0116: partially superseded.**
  - *Superseded:* "IR movement during PWM=0 destroys the mapped distance
    relationship" and the implication that "operator redeclaration is the
    authorized restoration" for that case.
  - *Survives:* everything else, including the genuine-frame-break rule, the
    absence of Hall-only confirmation, and reversal handling for coherent
    frames.
- **0118:** its first-target rule is the named re-establishment mechanism.
  0118 itself is unchanged.
- **0106, 0109:** applied. Redeclaration and the operator's authority are
  unchanged.

## Implementation status (audit of `958ef9a`, 2026-10-02)

Nonconforming. The core retains `mm_`, the target, the direction,
`positionReliable_` and the active Hall reference. However, it records PWM-zero
displacement as a true frame loss, cannot re-establish the coordinate without a
declaration, and publishes redeclaration wording. Locations are listed in the
integrated README, section "PWM-zero IR displacement (0120)".

## References

`0106`, `0109`, `0112`, `0116`, `0118`, `../NAVI_EWO_GOVERNING_DOCUMENTS.md`,
`../../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md`,
`../../field-records/logs/20261002_otto_ewo-r2-ft2_ccw-cw-stations.log`.
