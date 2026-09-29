# 0112 - At PWM = 0 Hall is non-actionable; IR displacement at PWM = 0 is factual and invalidates the map relationship, not the instrument

Status: Accepted as principle (recorded 2026-09-29; settled by David with Sam).
Partially supersedes 0106 (see below). Documentation only. No implementation,
flashing or deployment is authorized by this record.
Decided by: David, after review with Sam.

## Decision

**All observations still reach NAVI at PWM = 0.** Nothing is suppressed upstream.

**Hall at PWM = 0 is non-actionable navigation evidence.** Hall observations made
while motive PWM = 0 remain observable and recordable, but cannot themselves
alter:

- target confirmation;
- mapped position;
- target progression;
- Missed Magnet state;
- Hall-reference progression or replacement.

**IR remains a direct physical measurement.**

- During a normal PWM = 0 station stop, IR displacement is expected to be zero.
  Zero displacement is a valid result (0108, 0111) and **preserves the existing
  IR/MM relationship through the stop.**
- If IR reports **nonzero displacement at PWM = 0**:
  - accept the movement measurement as factual;
  - do not declare the IR instrument failed merely because movement was
    unexpected;
  - mark the relationship between accumulated IR distance and mapped position
    **unreliable**;
  - do not guess the new mapped position;
  - retain IR's ability to measure subsequent movement;
  - allow the next successfully confirmed expected MM, using the authorized
    degraded confirmation mechanism (0111) where appropriate, to re-anchor the
    IR/map relationship.

This condition should remain visible in telemetry and recording during
development.

## Context

0106 (2026-09-27) ruled that PWM = 0 is a non-navigation interval and that IR wheel
movement at PWM 0 is not route movement and "cannot be used as signed route
displacement". Its evidence (runs 1-5) showed IR pulses at PWM 0 only from
standing optical flicker and recorded hand handling; station stops showed zero
pulses. 0106 also left OPEN whether a motionless normal PWM-0 station stop breaks
a valid IR/MM reference. The 2026-09-27 capture adds long zero-pulse plateaus at
PWM 0 (32 s, 60 s and 22 s stops) in which the pulse count was unchanged.

## Alternatives considered

- **Discard IR displacement at PWM 0 as not real (0106's original wording).** Not
  chosen: the wheel did turn. The measurement is factual; what is unreliable is
  its relationship to the map.
- **Treat unexpected IR movement at PWM 0 as instrument failure.** Not chosen:
  it confuses an unexpected physical event with a failed instrument (0108).
- **Guess the new position from the displacement.** Not chosen: signed route
  displacement cannot be established at PWM 0, and guessing is excluded (0109).

## Consequences

- 0106's question "does a completely motionless normal PWM-0 station stop break an
  otherwise valid IR/MM reference?" is answered: **no.**
- IR keeps measuring after an unexpected-movement event; only its mapping to
  position is suspended until a confirmed expected MM.
- **Risk: while the relationship is unreliable, IR distance is not usable as the
  +/-15% target-interval evidence.** This record does not specify what NAVI does
  in that interval beyond "next confirmed expected MM re-anchors, using degraded
  confirmation where appropriate." By 0111 there is no no-IR Missed Magnet, so
  target progression by Missed Magnet is unavailable in that interval. **This
  consequence was confirmed by David and Sam, 2026-09-29:** while the relationship
  is unreliable, IR is not applicable as target-interval evidence, confirmation
  uses the degraded mechanism, and there is no Missed Magnet.
- **Risk: a re-anchoring MM confirmed by degraded (650 ms) evidence is weaker**
  than one confirmed with IR distance.

## Relationship to earlier decisions

- **0106: partially superseded.**
  - *Superseded:* the premise "PWM = 0 means no movement. Period." as applied to
    IR, and the rules that IR movement at PWM 0 "is not locomotive route movement",
    "cannot be used as signed route displacement" and that IR "must not
    contaminate" the MM/IR relationship. IR displacement at PWM 0 is accepted as a
    factual measurement, and the relationship is marked unreliable rather than
    protected.
  - *Resolved:* the OPEN item on motionless station stops.
  - *Retained:* Hall and other movement-related changes at PWM 0 have no
    navigation authority; the PWM-ramp account of "coasting"; operator
    declaration and redeclaration remain authoritative; re-anchoring through the
    next accepted MM or a declaration.
- **0108, 0111:** applied. Zero displacement is a valid no-change result and not
  an IR failure.
- **What stays true of 0106's headline:** motive PWM = 0 means NAVI is not driving
  the locomotive. What this record adds is that the measuring wheel may
  nonetheless turn (handling, tilt), and NAVI records that as a fact and does not
  deny it.

## References

- `0106`, `0108`, `0109`, `0111`
- `../NAVI_EWO_GOVERNING_DOCUMENTS.md`
- `../../field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference_report.md`
- `../../field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_challenge_report.md`
  (X19 Arches: stationary transitions accepted as MMs)
- `../../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md`
