# NAVI Design Fidelity Audit — recovered agreements vs repository documents

## Audit basis

| Artifact | Revision examined | Role |
|---|---|---|
| Original discussion | Supplied extract C:604–623 and baseline fragment B | Primary evidence available |
| Conference record | `9a6bbb7b` | Existing governing record |
| Clean-sheet proposal | `04ce4b6` | Proposal, not authority |
| Integrated proposal | `340b999` | Later proposal examined for baseline discrepancy |
| Independent review | `05f9682` | Not retrievable; no finding attributed to it |

## Executive assessment

`9a6bbb7b` accurately preserves the core Power of Normal: one expected target, continuous observation, completed-event qualification, shape diagnostic only, wait on nonqualification, boundary-only hold, and no search/recovery. `04ce4b6` generally carries those principles forward and correctly leaves several implementation choices open.

The material discrepancy is **baseline specificity**. R1 does not state a median-baseline method; R2 labels baseline/reference policy unresolved. The accessible primary fragment establishes spatial/inter-magnet intent and rejects a time substitute, but does not recover the full exact median algorithm. R3 adds a detailed spatial-reference proposal. It must not silently become governing unless its original conversation source is recovered or David explicitly ratifies it.

## Requirement traceability matrix

| Original agreement | Governing requirement | R1 treatment | R2 treatment | Discrepancy / required correction |
|---|---|---|---|---|
| One legitimate next MM; no alternatives | GEO-01–05 | Accurate | Accurate | None; retain explicit no-search rule. |
| Wrong polarity/nonqualification means continue waiting | HALL-06, TIME-04 | Accurate | Accurate | r5.1 contradiction-to-stop must not survive integration. |
| 70×2 opening; retrospective completion | HALL-02–04 | Accurate | Accurate, but sampling unspecified | Do not treat 20 ms means as native two-sample evidence without replay. |
| Shape cannot veto | HALL-05 | Accurate, expressly amends 6A | Accurate | Any later fit threshold would contradict agreement. |
| Minimum width necessary, not sufficient | HALL-04 | Accurate | Accurate | Value/convention truly open. |
| Timing predicts; Hall confirms | TIME-01–02 | Accurate | Accurate | Preserve onset-versus-RTB provenance. |
| Interval-specific pace-normalized boundary | TIME-03–06 | Accurate; rejects three-sigma | Accurate | Method, cap, and data sufficiency remain open. |
| Boundary is sole NAVI stop trigger | TIME-05 | Accurate | Accurate | Wrong polarity must not become a second stop path. |
| Continuous unconditional observation | HALL-01 | Accurate | Accurate | Existing host suppression is a conflict to remove only under authorization. |
| Host audit and replay precede integration | OPS-04, VAL-01 | Accurate | Recognizes audit findings | Do not combine audit, cleanup, and replay. |
| Spatial baseline; no time substitute | BASE-01–03 | Condensed/omitted | Calls baseline unresolved | Add explicit baseline decision record. |
| Exact median/promotion/zero-PWM behavior | BASE-04 | Not specified | Open, but may understate operator intent | Recover original summary before algorithm selection. |
| STOP/GO preserves encounter/reference | BASE-05 | Only Hall in hold stated | Arbitration open | Establish lifecycle from primary source before coding. |

## Median-baseline discrepancy

### Direct primary evidence

Baseline is tied to physical inter-magnet travel: Magnet Close begins measurement and 70ADC×2 opening ends it. A 50 mm notion may not silently become 50 ms. The record also names tail capture and PWM=0 as relevant. [B]

### R1: `9a6bbb7b`

It requires observed return to baseline but does not specify median sampling, spatial eligibility, promotion, or stationary treatment. It preserves completed-event logic but loses available baseline detail.

### R2: `04ce4b6`

It explicitly leaves baseline/reference policy, return definition, sampling representation, and stationary behavior unresolved. That is correct for the exact algorithm, but not permission to select an arbitrary time-based or approach-based baseline.

### R3: `340b999`

It adds “Magnet → intervening ordinary-track baseline → Magnet,” retained spatial reference, no rebaseline from stationary/quiet-time data, and persistence through STOP/GO. These are plausible and align with B, but R3 is a later proposal—not recovered primary agreement. Mark them candidate policy pending source recovery or ratification.

## Additions lacking direct primary support in this session

1. A particular median window/bin size or coverage rule.
2. Exact reference persistence/promote lifecycle through STOP/GO.
3. Queue-loss response or sample-record structure.
4. Exact hold release and motor arbitration.
5. ADC rate, return band, persistence duration, or baseline-update formula.

## Required correction sequence

1. Preserve the Decision Record and Governing Specification as the source-recovery packet; do not revise existing governing documents here.
2. Retrieve the original Ten Commandments and point-by-point baseline summary; append verbatim evidence and promote only what it supports.
3. Add a baseline decision section to the durable governing record: established spatial rules versus undecided parameters.
4. Label R2/R3 baseline detail proposal/candidate behavior until step 2.
5. Complete the r5.1 audit and historical replay before any implementation. Then separately authorize baseline and motor-arbitration decisions.

## Conclusion

The October 24 record is faithful on the core navigation model but incomplete for a median-baseline implementation. The limiting problem is missing original evidence, not an invitation to reconstruct an algorithm from later proposals.