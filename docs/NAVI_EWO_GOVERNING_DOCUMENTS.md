# NAVI_EWO governing documents

Directory of the canonical governing set for NAVI_EYES_WIDE_OPEN (EWO), in
precedence order. Last revised 2026-10-02.

This page is a directory and a supersession ledger. It authorizes nothing
(`/AGENTS.md` section 2), and no decision record authorizes an agent to act
(`decisions/README.md`).

**Four kinds of document, and they are not interchangeable:**

| Kind | What it does | May it override a decision? |
|---|---|---|
| **Canonical authority** | States what NAVI_EWO is to be | It *is* the decision |
| **Empirical evidence** | Says what was observed | No. Evidence is preserved, and does not silently alter a decision (AGENTS.md section 5) |
| **Implementation documentation** | Says what a candidate does today | No. Where it differs from a decision, the decision governs and the difference is a finding |
| **Historical prior art** | Says what earlier systems did and why | No. Kept as evidence; conflicts resolve to the newer canonical decision |

## 1. Design principles (canonical, enduring)

1. [`NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md)
2. [`NAVI_DECISION_MODEL.md`](NAVI_DECISION_MODEL.md) (its index covers 0102-0107 and
   0108; the EWO decisions below extend it)

## 2. Canonical decisions (in numerical order; later governs where they conflict)

| No. | Decision | Scope |
|---|---|---|
| [0102](decisions/0102-navi-is-the-sole-navigation-decision-authority-x22-is-prior-art.md) | NAVI is the sole navigation authority; X22 is prior art | NAVI |
| [0103](decisions/0103-direct-physical-measurement-before-proxy.md) | Direct physical measurement before proxy | NAVI |
| [0104](decisions/0104-evidence-accumulation-before-revision.md) | Evidence Accumulation Before Revision | NAVI |
| [0105](decisions/0105-evidence-is-relational-and-contextual-not-interchangeable-votes.md) | Evidence is relational, not interchangeable votes | NAVI |
| [0106](decisions/0106-motive-pwm-zero-is-a-non-navigation-interval-for-sensor-changes.md) | Motive PWM = 0 is a non-navigation interval (**partially superseded by 0112**) | NAVI |
| [0107](decisions/0107-hall-baseline-is-spatial-rtb-is-recognized-from-the-phenomenon.md) | Hall baseline is spatial (**refined by 0113 for EWO**) | NAVI |
| [0108](decisions/0108-measurement-applicability-absence-of-measured-change-is-not-sensor-failure.md) | Absence of measured change is not sensor failure | NAVI |
| [0109](decisions/0109-navi-ewo-is-a-target-only-navigator-it-confirms-its-target-shrugs-at-everything-else-and-continues.md) | Target-only mission; shrug; Missed Magnet; declaration/CRM; confident locomotive; station boundary; supersession scope | NAVI_EWO |
| [0110](decisions/0110-native-observations-belong-to-navi-and-target-confirmation-is-relational.md) | Native observations; median-of-five Hall statistic; relational confirmation | NAVI_EWO |
| [0111](decisions/0111-ir-is-a-normal-navigation-sensor-navi-decides-applicability-degraded-operation-stays-inside-navi.md) | IR as a normal sensor; temporary Hall/IR association; degraded 650 ms **withdrawn by 0116** | NAVI_EWO |
| [0112](decisions/0112-pwm-zero-makes-hall-non-actionable-ir-displacement-at-pwm-zero-is-factual-and-invalidates-the-map-relationship-not-the-instrument.md) | PWM = 0 applicability; degraded re-anchor **withdrawn by 0116**; final operator-intervention rule **0120** | NAVI_EWO |
| [0113](decisions/0113-the-navi-ewo-hall-reference-is-spatial-and-is-replaced-from-the-leading-field-boundary.md) | Spatial Hall reference | NAVI_EWO |
| [0114](decisions/0114-ewo-boot-reference-uses-five-observed-pulse-positions.md) | Historical five-position startup proposal; superseded/withdrawn | Historical |
| [0115](decisions/0115-ewo-provisional-single-sample-boot-reference-and-missed-magnet-origin.md) | Provisional single-sample boot reference; physical origin retained across Missed Magnets | NAVI_EWO |
| [0116](decisions/0116-ewo-ir-distance-is-authoritative-and-hall-only-navigation-is-withdrawn.md) | Health observes; IR distance required; no Hall-only confirmation; reversals retain coherent frames; true frame loss requires redeclaration | NAVI_EWO |
| [0117](decisions/0117-first-target-after-declaration-accepts-any-onset-within-interval-plus-15-percent.md) | Historical first-target upper-bound rule; superseded by 0118 | Historical |
| [0118](decisions/0118-first-target-after-declaration-is-found-by-hall-onset-without-a-distance-window.md) | First target: applicable positive IR travel and Hall onset, no distance window or miss | NAVI_EWO |
| [0119](decisions/0119-pwm-zero-ir-movement-retains-interval-and-reacquires-coordinate.md) | Automatic coordinate reacquisition **superseded by 0120** | Historical |
| [0120](decisions/0120-pwm-zero-movement-requires-operator-position-verification-and-declaration.md) | Zero-movement dwell preserves localization; PWM-zero movement holds navigation/withdraws AUTO until operator verification and declaration; no automatic recovery | NAVI_EWO |

Related decisions that remain in force and are not changed here: 0101
(position determines local operating requirements) and 0100. 0093 remains
NAVI_COHERENCE prior art; its EWO extension by 0111 is withdrawn by 0116.

## 3. Reconciliation and architectural history

- [`NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md`](NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md)
  (inventory and evidence still valid; its proposals are superseded per its own
  header note and 0102)

## 4. Empirical basis

- [`../field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference_report.md`](../field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference_report.md)
  (supports the spatial-reference principle; does not choose a bin width)
- [`../field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_challenge_report.md`](../field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_challenge_report.md)
  (verdict INCONCLUSIVE; replayed the earlier two-consecutive-sample 70-count
  gate. Prior art for 0110's statistic; it does not reopen it)

## 5. Current implementation documentation (candidates, not field accepted)

- [`../firmware/programs/NAVI_EYES_WIDE_OPEN/README.md`](../firmware/programs/NAVI_EYES_WIDE_OPEN/README.md)
- [`../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md`](../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md)
  (the integrated candidate; it carries the 21-item disposition audit)
- [`NAVI_SYNC_NATIVE_HALL_IR_RECORDER_20260927.md`](NAVI_SYNC_NATIVE_HALL_IR_RECORDER_20260927.md)
  (recorder)

The FT2 build `ab0938b` was flashed to Otto with David's explicit approval on
2026-10-02. The 0120 correction replaces unflashed 0119 candidate `b146815`; see
[current change log](NAVI_EWO_PWM_ZERO_DECLARATION_20261002.md). This directory does not
authorize flashing or deployment (AGENTS.md section 2).

## 6. Historical prior art (kept, not deleted, not authoritative for EWO)

X22/X22R detector code and records; NAVI_ONE variants and field findings;
NAVI_COHERENCE navigation judgment; QUORUM; 0098 proximal recovery. Their
conflicting assumptions are superseded for NAVI_EWO as follows.

## Supersession ledger

| Older assumption | Now | By |
|---|---|---|
| X22/X22R as a detector that decides which Hall observations NAVI sees; upstream gating (400 ms window, PWM-zero and old-field holds, cadence, MOVING/STOPPED, readiness) | NAVI receives every native observation; there is no upstream gate | 0102, 0110, 0112 |
| X22/X22R baseline, lock, settle and quiet mechanisms as the Hall reference | Spatial reference owned by NAVI | 0113 |
| Upstream Hall median or evidence selection | NAVI-owned median-of-five | 0110 |
| Two-consecutive-sample persistence and closure/shape/duration/lobe authority | No independent authority | 0110 |
| NAVI_ONE strike/accept/shape-rejection architecture | Target-only confirmation | 0109, 0110 |
| NAVI_COHERENCE navigation judgment (Navigator judge/traverse, accepted-MM reference, `hallReady` readiness, movement comparison) | NAVI_EWO judgment | 0109-0111 |
| Proximal recovery (0098), sequence correction, alternative-position search, map-wide matching, QUORUM recovery, 10-MM AUTO recovery limit | Not authorized. Missed Magnet is the only recovery | 0109 |
| Shadow "what X22 would have done" and shadow accepted-MM reference mechanisms | Not used. NAVI itself is instrumented | 0102, 0110 |
| IR as an experimental supplement; stationary `INADEQUATE_CONTRAST` as degradation | IR is a normal sensor; diagnostics cannot veto coherent distance | 0111, 0116 |
| 650-ms degraded Hall-only EWO confirmation or re-anchor | Withdrawn; no IR distance means hold MM/target, not Hall-only progress | 0116 (supersedes EWO extension of 0093 and part of 0111/0112) |
| Reversal as an automatic IR/map frame break | Preserve coherent frame, capture cumulative reversal origin, and require Hall+IR confirmation | 0116 |
| Genuine IR frame loss recoverable at next timed Hall field | Hold position/context; operator redeclaration restores the mapping | 0116 |
| IR displacement at PWM = 0 is not real and must not touch the MM/IR relationship (0106) | Factual wheel movement, not signed route travel or an instrument fault; context held for diagnosis, operator verification and declaration required | 0112 refined by 0120 |
| Automatic within-interval coordinate reacquisition after PWM-zero movement (0119) | Withdrawn: hold navigation and withdraw AUTO until operator verifies/repositions and declares; no later matching-magnet substitution | 0120 |
| Declaration supplies a marker-distance origin / first-target upper bound (0117) | Hall onset with positive applicable IR travel; no first-target window or miss | 0118 |
| Stationary boot reference (0107 allowance), then first-10-mm startup (0113) | One provisional native Hall ADC at boot; 0113 spatial replacement remains | 0115 |
| Refusal-and-stop on a missing magnet (0056) | Missed Magnet continuity with applicable IR | 0109 (confirmed 2026-09-29) |

## Confirmed by David and Sam, 2026-09-29

- **0056:** Missed Magnet continuity narrows 0056's refusal-and-stop consequence
  for NAVI_EWO (0109).
- **0104 RESOLVE:** EWO has no mechanism to revise position toward a different
  hypothesis beyond Missed Magnet advance and operator redeclaration (0109).
- **Unreliable IR/MM relationship (historical 2026-09-29 ruling):** The degraded
  mechanism was used to confirm an expected MM and re-anchor. Otto's later
  evidence caused David and Sam to withdraw that rule in 0116.

## Resolved by later instruction

- **0107 D4/D8:** resolved for NAVI_EWO by 0113 to the extent the spatial geometry
  answers them.
- **Confident locomotive:** added as enduring principle 17 in
  `NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`.

## Open points (not settled; do not treat any as decided)

1. **CRM challenge:** no threshold or mechanism for telling the operator that the
   encountered MM are inconsistent with the declaration (0109).
2. **0107 D4/D8 remainders:** whether the 100 mm clearance is physically
   sufficient, and how long a reference may be held across successive Missed
   Magnets. (The rest of D4/D8 is resolved by 0113.)
3. **Multiple IR senders:** the integrated candidate has no source-selection
   policy; David and Sam must decide a NAVI-owned policy before field use.
4. **Spatial-reference details:** bin width, minimum coverage, and the per-location
   representative rule.
5. **Synchronized IR evidence** for the three surviving impostor candidates
   (X21 Grillers departure, QUORUM 243211, QUORUM 263774): track evidence needed.
