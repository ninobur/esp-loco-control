# NAVI decision model

> **Read first:** [`NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md), the enduring NGR decision-system design principles. This
> document applies them to NAVI.

**Canonical, living document.** Last revised 2026-09-27, after David and Sam's
first-pass review of D1–D10 from
[`NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md`](NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md).

This document states the principles by which NAVI reaches navigation
judgments. Each principle is recorded, with its reasoning and history, in a
numbered decision record (`decisions/0102`–`0107`). This page is the index and
the summary.

**NAVI_EWO extension (2026-09-29/30):** decisions 0109-0113 and 0115-0116 extend this model for
NAVI_EYES_WIDE_OPEN. The governing set and supersession ledger are in
[`NAVI_EWO_GOVERNING_DOCUMENTS.md`](NAVI_EWO_GOVERNING_DOCUMENTS.md). The
sections below were not rewritten; where they conflict with later EWO
decisions, those decisions govern (notably 0106's IR-at-PWM-0 rule, superseded
by 0112, and the EWO Hall-only fallback, withdrawn by 0116).

**It authorizes no implementation.** Nothing here changes firmware, tests,
thresholds, the dashboard, the Pi or field behavior. Per
[`decisions/README.md`](decisions/README.md), no record authorizes an agent to
act; implementation authority comes from David, separately.

Principles in this document were decided by David with Sam. Wording marked as
a quotation comes from David's codification instruction of 2026-09-27.

---

## 1. Authority

**NAVI is the sole navigation decision-making authority.** → [0102](decisions/0102-navi-is-the-sole-navigation-decision-authority-x22-is-prior-art.md)

- Processing may be distributed for computational efficiency, organization,
  testing and signal conditioning. **Navigation judgment is not distributed.**
- Hall, IR, PWM, direction, map, history and other sources provide data.
  Low-level processing may condition or characterize those data. NAVI
  determines their navigation significance, using the relevant context
  available when the decision is required.
- There is one ESP32. No hardware or computational necessity has been
  identified for placing a lower-context navigation decision-maker between the
  physical data and NAVI.
- **Decide where the information is greatest.** A subordinate module must not
  make an irreversible navigation judgment merely because it is closer to the
  sensor. It must not prevent NAVI from receiving relevant data because it has
  already decided what the data mean.
- **X22 is prior art, not the architectural starting point.** Its knowledge is
  preserved. Its authority structure, gates, state machine and division of
  responsibility carry no presumption of survival.

## 2. Evidence

**Direct physical measurement before proxy.** → [0103](decisions/0103-direct-physical-measurement-before-proxy.md)
When NAVI has a direct physical measurement relevant to the question, it uses
that measurement, not a proxy for it:
- measured IR distance, not elapsed time;
- actual Hall-field behavior, not a fixed timer;
- measured movement, not motor command, where measured movement applies;
- mapped geometry, not generic timing.

Time, PWM and other indirect indicators remain useful when the corresponding
measurement is unavailable. They do not displace applicable direct evidence.

**Measurement applicability.** Absence of measured displacement is itself a
valid result; it is not, without context, sensor failure. An IR
`INADEQUATE_CONTRAST` diagnostic describes the detector's current optical
window. NAVI must distinguish that internal condition from independent
evidence that significant physical movement occurred while IR failed to
measure it. → [0108](decisions/0108-measurement-applicability-absence-of-measured-change-is-not-sensor-failure.md)

**Evidence is not a conclusion.** A datum is evidence; its existence does not
compel a change in NAVI's world view. → [0104](decisions/0104-evidence-accumulation-before-revision.md)

**EVIDENCE ACCUMULATION BEFORE REVISION** (named principle). → [0104](decisions/0104-evidence-accumulation-before-revision.md)

> NAVI shall not revise a well-supported world view merely because a new
> observation is inconsistent with it.

When contradictory evidence cannot distinguish among plausible explanations,
NAVI:
- **OBSERVES** — preserves what actually occurred, including contradictions;
- **HOLDS** — retains the incumbent best-supported hypothesis rather than
  forcing a new conclusion;
- **RESOLVES** — uses subsequent independent information to confirm the
  incumbent or support a different one.

It neither discards inconvenient evidence nor lets one unexplained datum erase
stronger accumulated knowledge. Difficulty deciding is evidence that more
information may be required, not justification for manufacturing certainty.

**Loss of information does not invalidate established knowledge.** When IR
becomes unavailable, NAVI does not forget its established position, baseline
or other coherent knowledge, and does not manufacture replacements from weaker
proxies. It retains what remains coherent and revises when sufficient evidence
justifies revision. → [0104](decisions/0104-evidence-accumulation-before-revision.md)

**Preserve uncertainty rather than manufacture certainty.** "I do not yet have
enough evidence to revise what I know" is a valid, desirable decision state,
and it must be reconstructable in telemetry. → [0104](decisions/0104-evidence-accumulation-before-revision.md)

**EVIDENCE IS RELATIONAL AND CONTEXTUAL, NOT INTERCHANGEABLE VOTES** (named
principle). → [0105](decisions/0105-evidence-is-relational-and-contextual-not-interchangeable-votes.md)
- NAVI must not reduce heterogeneous evidence to a numerical voting or
  confidence system merely so that unlike evidence can be added together.
- A direct physical measurement may *constrain the interpretation* of another
  datum rather than add weight to one side.
- Physical quantities are quantified where appropriate: distance, speed, Hall
  departure, variance, acceleration, mapped spacing, measurement tolerance.
- Judgment stays explicit and reconstructable (§4). A single scalar confidence
  score is not a substitute.

## 3. Physical-state rules

**Motive PWM = 0 is a non-navigation interval for sensor changes.** → [0106](decisions/0106-motive-pwm-zero-is-a-non-navigation-interval-for-sensor-changes.md)
- **PWM = 0 means no movement. Period.** Motive PWM 0 is a non-navigation
  interval: NAVI is not moving the locomotive.
- Changes in Hall, IR or other movement-related sensor data during that
  interval are not locomotive route movement for navigation purposes. They may
  be retained, reported and available as context, but they have no navigation
  authority. They cannot advance, retreat or correct position, accumulate route
  displacement, or cause a compensating navigation response.
- Operator declaration and redeclaration remain authoritative.
- IR wheel movement recorded at PWM 0 may be real movement of the measuring
  wheel, including operator handling. It is not locomotive route movement,
  cannot be used as signed route displacement, and must not contaminate the
  MM/IR relationship. If that relationship is broken, IR regains route-location
  authority only through a trustworthy route reference: normally the next
  accepted MM, or an operator declaration.
- **Open:** whether a completely motionless, normal PWM-0 station stop breaks
  an otherwise valid IR/MM reference.

**The Hall baseline is spatial information.** → [0107](decisions/0107-hall-baseline-is-spatial-rtb-is-recognized-from-the-phenomenon.md)
- A new operational baseline requires sampling across physical movement. A
  quiet stationary value is one physical point.
- At boot, with the operator's known condition that Toby is clear of magnets,
  NAVI may establish an initial stationary reference sufficient to recognize
  the first Hall excursion.
- The required sampling distance is not yet specified.

**Return to Baseline (RTB)** is the Hall signal returning to ordinary-track
behavior. The term replaces "magnet closure" wherever that is the phenomenon
meant. RTB is to be recognized from Hall data by a general model, not by a
timer or a locomotive-specific distance. → [0107](decisions/0107-hall-baseline-is-spatial-rtb-is-recognized-from-the-phenomenon.md)

**General model before locomotive-specific calibration.** Build the Hall/RTB
model from general physical and signal principles. Test it across locomotives.
Add locomotive-specific accommodation only if field evidence shows a
meaningful difference. The model needs enough discrimination for reliable
railway navigation, not unnecessary precision. → [0107](decisions/0107-hall-baseline-is-spatial-rtb-is-recognized-from-the-phenomenon.md)

## 4. The shape of a judgment

Every navigation judgment is reconstructable as:

```
QUESTION -> DATA -> CONTEXT -> ALTERNATIVES -> PHYSICAL COHERENCE -> DECISION / HOLD -> WHAT COULD RESOLVE UNCERTAINTY
```

NAVI's own instrumentation is the record of this:

```
DATA -> CONTEXT -> ALTERNATIVES -> EVALUATION -> DECISION
```

It is not recorded by a parallel decision system. There is no X22 shadow
(D7, §6).

## 5. Inherited restrictions

Many older NGR mechanisms were rational open-loop workarounds, adopted because
physical-state feedback did not exist.

The historical example: a following train once could not leave a station until
the leading train reached the next station. That arrival was effectively the
only reliable information the follower had about the leader's location.
Waiting was rational under that scarcity, but it severely restricted freedom
of action. Acquiring better physical information while keeping the old
restrictions would add telemetry without delivering the intelligence and
freedom of action NAVI was created to achieve.

For every inherited restriction or mechanism, ask:
1. What physical problem was this solving?
2. What information was unavailable when it was created?
3. What uncertainty made the restriction necessary?
4. Does NAVI now possess physical data capable of resolving that uncertainty?
   - **Yes:** reason from the physical data.
   - **No:** the older proxy may remain useful as degraded-mode evidence.

Preserve useful knowledge, not the historical authority structure.

## 6. Decision register (reconciliation plan D1–D10, reviewed 2026-09-27)

| # | Question (as posed on 2026-09-26) | Status | Disposition |
|---|---|---|---|
| D1 | Adopt the measurement-stage / NAVI split of plan §5.1 | **SETTLED, reframed** | NAVI is the sole decision authority, but the new structure is created anew from these principles. It is not X22 refactored into NAVI (0102) |
| D2 | NAVI fallback rules reproducing X22's PWM-zero and stopped-in-field outcomes | **SETTLED: superseded** | Preserve the information and useful concepts, not old outcomes because X22 produced them. PWM-0 behavior follows 0106 |
| D3 | Landmarks at PWM 0 with valid IR travel; IR-car-only motion | **SETTLED** | No navigation authority at motive PWM 0; IR at PWM 0 is not signed route displacement (0106) |
| D4 | IR-region bounds or shadow before a Toby measurement | **RESOLVED for NAVI_EWO (0113), in part** | Pending a general RTB model (0107). No 80 ms, 71 mm, Toby- or Otto-specific answer is to be predetermined *(Earlier text is historical; 0113 states what is resolved and what remains.)* |
| D5 | Stationary lock collection | **SETTLED** | A new operational baseline requires sampling across movement; boot reference excepted (0107) |
| D6 | Keep 3 s cadence as a no-IR fallback | **SETTLED: no** | Loss of IR does not require a new baseline. Retain the established one while coherent; address demonstrated inadequacy then (0104) |
| D7 | X22 shadow comparison telemetry | **SETTLED: no** | Instrument NAVI itself. Historical X22 behavior stays available in old code and field records (0102) |
| D8 | Does a sans-MM advance open a baseline opportunity? | **RESOLVED for NAVI_EWO (0113), in part** | Prerequisite question: is the baseline acquired-and-held, or a continuously maintained model of ordinary-track behavior? Resolve the baseline model and RTB first *(Earlier text is historical; 0113 states what is resolved and what remains.)* |
| D9 | Distance-based lost-lock replacement | **DEFERRED** | `lostMs` stays disabled. No replacement designed without a demonstrated failure; if one appears, characterize and solve that phenomenon |
| D10 | Toby raw Hall + IR recording as a prerequisite | **SETTLED** | General model first. Fresh Toby data is useful validation when convenient, not a prerequisite (0107) |

**Open questions not to be resolved silently:**
- D4 and D8: resolved for NAVI_EWO by 0113 only to the extent that its spatial
  geometry answers them (region, origin, held-and-replaced reference). Remaining
  parts are listed in 0113;
- D9 (deferred);
- whether a motionless normal PWM-0 station stop breaks a valid IR/MM
  reference (0106).

## 7. Relationship to earlier principle documents

These are compatible with and extended by this model. They are cited, not
superseded.

- [`NAVI_DECISIONAL_INERTIA_CLARIFICATION_20260923.md`](NAVI_DECISIONAL_INERTIA_CLARIFICATION_20260923.md):
  "Unavailable evidence is not contradictory evidence"; "decisional inertia is
  not permanence". Evidence Accumulation Before Revision (0104) names and
  generalizes this.
- [0098](decisions/0098-proximal-physical-recovery-before-polarity-scoring.md):
  retain the incumbent, physically plausible candidates first. It is an
  instance of 0103 and 0104. **Tension:** its ten-position polarity *score*
  is a count of like observations of one kind, not a mixing of heterogeneous
  evidence, but it should be re-examined against 0105 when recovery is next
  reviewed. The run 3 reversal finding is still open for a controlled retest.
- [0092](decisions/0092-navi-ir-decision-model-physically-possible-positions-first.md),
  [0094](decisions/0094-ir-validity-is-continuous-health-not-earned-trust.md):
  physically possible positions first; IR validity as continuous health.
- [0093](decisions/0093-provisional-navi-coherence-hall-only-guard-650-ms.md):
  historical NAVI_COHERENCE Hall-only fallback. Its EWO extension was
  withdrawn by 0116 after Otto's sustained-field false confirmations.
- [`NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md`](NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md),
  [`NAVI_SHARED_TRAIN_STATE_PRINCIPLE_20260923.md`](NAVI_SHARED_TRAIN_STATE_PRINCIPLE_20260923.md),
  [`NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md`](NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md).
- [`NGR_NAVI_ARCHITECTURAL_LINEAGE_DRAFT_v0_1.md`](NGR_NAVI_ARCHITECTURAL_LINEAGE_DRAFT_v0_1.md)
  (Sam's draft, kept byte-identical and **not edited**):
  - its "DETECTION → HARD/PHYSICAL CONSTRAINTS → NAVI JUDGMENT" framing and
    "Sensors observe; NAVI knows" are consistent with this model;
  - where the draft places decisions inside detection, 0102 governs.
