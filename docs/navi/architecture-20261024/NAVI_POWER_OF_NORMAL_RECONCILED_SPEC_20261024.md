# NAVI Power of Normal — Reconciled Governing Specification

**Authority:** derived from `NAVI_TWENTY_ORIGINAL_AGREEMENTS_20261024.md`. Later explicit amendments control. This is an implementation authority; historical candidates are not requirements merely because they occur in the transcript.

## 1. Navigation model

| ID | Requirement | Status | Authority |
|---|---|---|---|
| REC-01 | Automatic navigation is constrained to an operator-declared position, direction, and mapped sequence. | Established | Principles 1–2, 6–7 |
| REC-02 | NAVI has exactly one expected next magnet. | Established | Prop 8 |
| REC-03 | Normal cycle: **Expect → Observe → Qualify → Confirm/Advance**. | Established | Props 8–9 |
| REC-04 | Only qualified evidence for that target advances geography, exactly one marker. | Established | Principles 5–7; Prop 8 |
| REC-05 | NAVI must not search alternative identities, skip ahead, reacquire geography, or infer passage from movement alone. | Established | Principle 7; Prop 8 |

## 2. Observation versus navigation authority

| ID | Requirement | Status | Authority |
|---|---|---|---|
| REC-06 | Hall observation is continuous and independent of TARGET/NOT_THE_TARGET judgment. | Established | Principles 3, 9 |
| REC-07 | The observer can characterize physical Hall evidence but cannot change mapped position. | Established | Principle 9 |
| REC-08 | The navigator alone makes target and geographic decisions from the observation stream plus map/movement evidence. | Established | Principles 5, 9 |
| REC-09 | A nonqualifying observation does not interrupt subsequent observation. | Established | Principle 9; Prop 8 |

## 3. Qualification of the one target

| ID | Requirement | Status | Authority |
|---|---|---|---|
| REC-10 | The established candidate opening is 70 counts on two consecutive samples; preserve its opening polarity. | Established | Prop 7 |
| REC-11 | Qualification is retrospective: wait for observed completion/return to ordinary background before confirmation. | Established | Principles 3–4; Prop 6A |
| REC-12 | Correct polarity, minimum physically plausible width, observed completion, and expected geography are necessary qualifications. | Established; values/conventions open | Principles 6; Props 6A–8 |
| REC-13 | General shape fitting (Gaussian, bell, residual, smoothness, lobe/template score) is diagnostic only. It cannot veto an otherwise qualified expected target. | **Amended / established** | Prop 7 supersedes the prior admission-gate reading of Principle 4 / Prop 6A |
| REC-14 | Wrong polarity is a nonqualification: retain last confirmation and continue looking for the same target. It is not an immediate stop or a request to search. | Established | Prop 8 |

## 4. Baseline and ordinary background

| ID | Requirement | Status | Authority |
|---|---|---|---|
| REC-15 | Baseline evidence is spatial/inter-magnet ordinary-track evidence, not a substituted elapsed-time interval. | Established | Principle 6; original baseline discussion |
| REC-16 | The longer intervening ordinary-track segment supplies the baseline median. | Established as the conference model | Principle 6 |
| REC-17 | The exact median collection boundary, eligibility, promotion/persistence, return band, and stopped/PWM=0 handling require evidence-backed implementation definition. | Genuinely unresolved | Principle 6’s numerical criteria; source discussion |

## 5. Movement and protective motor authority

| ID | Requirement | Status | Authority |
|---|---|---|---|
| REC-18 | Movement estimates (elapsed time, operating history, PWM) support plausibility; they never substitute for magnetic confirmation. | Established | Principle 8; Prop 4 |
| REC-19 | IR is excluded from this Hall-only navigation project’s navigation decisions; it may corroborate field testing. | Established | Principle 8 |
| REC-20 | Before the validated arrival boundary, unconfirmed/nonqualifying means continue observing the same target. | Established | Props 6B, 8 |
| REC-21 | Only expiry of an empirically validated, per-interval, pace-normalized arrival boundary requests stop-and-hold. Retain position and continue Hall observation. | Established; numerical method open | Prop 6B |
| REC-22 | Three-sigma is not an authorized arrival-boundary method. | Established exclusion | Prop 6B |
| REC-23 | Arbitration/release semantics among hold, station, throttle, manual, and E-stop authorities need explicit implementation review. | Unresolved | Prop 10 audit requirement |

## 6. Engineering discipline and validation

| ID | Requirement | Status | Authority |
|---|---|---|---|
| REC-24 | No failure-specific navigation mechanism without field evidence that normal cycle plus stop-and-hold is inadequate. | Established | Principle 10; Prop 9 |
| REC-25 | Every mechanism must trace to an approved requirement; ambiguities return to David, Claude, and Sam rather than entering code silently. | Established | Principle 10 |
| REC-26 | Before integration, perform read-only host audit and historical replay, including known morphology-rejection failures. | Established | Prop 10 |

## Explicit exclusions

No recovery engine, alternative-MM matcher, hypothesis list, morphology/shape veto, timing-only advance, fixed quiet-time baseline surrogate, or universal timeout is authorized by this specification.
