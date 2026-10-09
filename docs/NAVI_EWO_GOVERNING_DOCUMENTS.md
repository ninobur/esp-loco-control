# NAVI_EWO governing documents

Directory of the canonical governing set for NAVI_EYES_WIDE_OPEN (EWO), in
precedence order. Last revised 2026-10-09.

## Mandatory primary guide for every NAVI firmware revision

**Read first:** [NAVI_EWO_0_1 — Architectural Inheritance and
Simplification](NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md). This is the mandatory
primary architectural authority for **all future NAVI firmware revisions**.
It supersedes conflicting earlier architectural documents, implementation
instructions, and decisions. Compatible earlier decisions remain binding,
particularly 0116 and 0120. Proposed departures require David's explicit approval.
Every implementation review must verify compliance with the guide and its
Necessity and Simplicity Test.

Architectural precedence:
`Primary guide → compatible existing NAVI decisions → implementation specifications → historical documentation`.
`AGENTS.md` continues to govern work authorization and deployment.

The reconciliation considers both `db329782d422559dcb886f93db8c1f3e3922a7ab`
(the guide) and `7a31d913ef2218687b488936a998f82655f5d3c6` (simplified
IR-loss fallback). It retains PWM hold at steady speed and continued established
monotonic PWM reduction in deceleration; continuous homeostasis, adaptive glide
control, and ticker-tape recording/playback requirements are superseded.
IR recovery restores applicable evidence, not the superseded speed controllers.
**Further October 9 governing update (primary guide):** ordinary base tiles
assign cruise PWM **60**; station stopping and all exceptional operating
requirements are **temporary overlays**. NAVI has **one active PWM-authority
overlay at a time**, selected by priority, with geographic collision prevention
highest. A universal arbitrary-destination STOP overlay is developed **before**
Four-Station Local. Its geographic PWM ramp is resolved at IR-pulse positions,
using creep PWM **50** and a final approach toward PWM **20** at the target;
stop accuracy is measured, not an execution gate. For timed service,
completion requires **elapsed dwell and concurrently applied PWM=0**. The old
Station +1-MM moving-clock trigger and base station-stop tiles are superseded.
After release, ramp up independently of actual stop location; follow the
next valid geographic tile instruction. See the mandatory primary guide
§§4–5, 8, 13–14. Lower priority, overlay-consumption representation and
early-stop actuator progression to PWM zero remain explicitly undecided.

No firmware, merge, flash, or two-train activation is authorized by this index.

**October 9 operating-arrangement clarification — read with the primary guide:**
[CTO bubble and Circuit Express: David's settled decisions](NAVI_CTO_BUBBLE_AND_CIRCUIT_EXPRESS_DECISIONS_20261009.md).
Independent single-train operation is **not CTO**. Normal Operations with
Collision Control permits independent operation. CTO is Toby and Otto's
negotiated two-train bubble, including coordinated movement, roles, station
holding/waiting, release/departure, and synchronized travel. **CE is a temporary
coordinated sequence within CTO: CTO → CE → role reversal → CTO.** Collision
protection is distinct and available under operating arrangements; one active
PWM overlay is a **per-locomotive** execution rule. The follower's traffic hold
does not consume its owed platform stop. The whole Four-Station Local service
overlay and higher-priority skip exception remain an architectural proposal,
without a detailed priority ladder below collision protection. The note is
incorporated into primary guide §§8–9 and preserves explicit supersessions.

**Before beginning NAVI Close Train Operations (CTO) or Circuit Express
reconstruction, read that clarification before:**
[`CTO and Circuit Express — reconstruction architecture`](NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md).
Document A preserves compatible concepts and the relationship to legacy CTO2;
its new header identifies descriptions superseded by the primary guide and
October 9 clarification. Historical text remains evidence, not a competing
definition of CTO/CE. Neither record authorizes implementation.

**Before reviewing or cleaning the production NAVI/EWO lineage, read first:**
[`NAVI Vestigial Code Cleanup`](NAVI_VESTIGIAL_CODE_CLEANUP_ARCHITECTURE_20261005.md).
These cleanup principles are separate from station position/overlay architecture,
station-stop implementation, adaptive braking, and CTO/CE reconstruction.
Preserving this architecture does not authorize cleanup implementation.

**After the primary guide, consult compatible operating architecture in:**
[Document C](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md) and
[Decision 0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md).
Document C's October 9 reconciliation identifies superseded control,
dwell-clock, and departure provisions. A governs compatible CTO/CE reconstruction;
B governs compatible vestigial cleanup; C preserves geographic tile/overlay
authority beneath the primary guide. Historical physical-speed-control
requirements do not regain authority through those scope labels.

The stationless firmware baseline remains
`c7c21163b016e443524eb9fa483f8b706ce76e22`; the demonstrated PWM physical-control
reference remains `ab0938b0f01531a38ce4d8a1d9d932ffad4f0334`.
Neither is an implemented or field-accepted `NAVI_EWO_0_1` candidate.
This revision changes documentation only.

**First universal STOP demonstration:** the
[MM045 bidirectional test definition](NAVI_STOP_MM045_BIDIRECTIONAL_TEST_DEFINITION_20261009.md)
records David's chosen vicinity and CODEX's selected midpoint of MM045–MM046.
Both directions target the same geographic point; entry anywhere in the
approach remains required. The note verifies the stationless source reference
and distinguishes target selection from the remaining implementation decisions.

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

## 1. Compatible design principles (canonical, enduring)

1. [`NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md)
2. [`NAVI_DECISION_MODEL.md`](NAVI_DECISION_MODEL.md) (its index covers 0102-0107 and
   0108; the EWO decisions below extend it)

## 2. Compatible decisions (numerical order; subject to the primary guide)

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
| [0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md) | Current MM interval + direction + overlay determine operating requirements; historical station procedure has no operating authority | NAVI operating/station architecture |
| [0122](decisions/0122-type5-pitch-is-configuration-and-calibration-metadata-has-no-runtime-authority.md) | Installed Type-5 pitch is configuration; calibration metadata has no runtime authority; real validity and continuity remain | NAVI_EWO Document B implementation |
| [0123](decisions/0123-navi-pass2-uses-geographic-speed-control-and-pwm-ticker-tape.md) | Reconciled simplified IR-loss PWM hold/monotonic-ramp behavior; continuous homeostasis, adaptive glide control, and ticker tape superseded | Subordinate to primary guide; documentation only; Hall-checkpoint semantics unresolved |

Decision 0101's current-position principle remains foundational. Its retained
procedural station mechanisms, including completed-visit suppression, pause
preservation and phase-watchdog behavior, are superseded by the October 5
[Position/Overlay and Speed-Control Operating Architecture](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
and [0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md).
Decision 0100 remains in force. 0093 remains NAVI_COHERENCE prior art; its EWO
extension by 0111 is withdrawn by 0116.

## 3. Reconciliation and architectural history

- The [mandatory primary guide](NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md),
  §14, records the October 9 supersessions and remaining questions. Its prior
  proposal status at `db329782` is superseded by David's current instruction.
- [Document C reconciliation](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
  and [Decision 0123 reconciliation](decisions/0123-navi-pass2-uses-geographic-speed-control-and-pwm-ticker-tape.md)
  retain the `7a31d913` historical texts with explicit current dispositions.
  Original ticker-tape requirements remain historical in `b646cab`; the
  October 7 adaptive-recording proposal at `ec1d62c` is historical proposal
  evidence and cannot override the guide.
- [Position/overlay documentation reconciliation, 2026-10-05](NAVI_POSITION_OVERLAY_DOCUMENTATION_RECONCILIATION_20261005.md)
  (scoped supersession notices and search classifications; historical evidence
  remains intact; no firmware or test changes)
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

- [Document B implementation ledger and validation, 2026-10-05](NAVI_VESTIGIAL_CODE_CLEANUP_IMPLEMENTATION_20261005.md)
  (vestigial cleanup only; Document C/station implementation remains a separate pass)
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
| CTO means independent operation plus collision avoidance; CE is a peer mode or single-locomotive express | Superseded/rejected. CTO is the negotiated two-train bubble; CE is its temporary coordinated role-reversal sequence, returning to ordinary CTO. Normal Operations can use collision protection independently | Primary guide §§8–9; October 9 CTO/CE decision note |
| A local PWM overlay defines the shared operating relationship; a traffic stop discharges platform service | Superseded/rejected. One PWM overlay per locomotive executes its instructions; shared coordination is distinct. The follower still owes its platform stop after the traffic hold | Primary guide §§8–9; October 9 CTO/CE decision note |
| Earlier NAVI architectural documents can govern a conflicting future revision | Mandatory primary guide governs all future NAVI revisions; compatible earlier decisions survive | Primary guide, governing-authority provision; AGENTS.md |
| Continuous IR speed homeostasis, adaptive glide control, five-beat corrections, 95% handoff, and rejection of geographic PWM as initial controller | Superseded required control architecture; use demonstrated PWM execution and existing ramps | Primary guide §6; reconciled Document C and 0123 |
| PWM ticker-tape recording/playback, historical PWM memory, missed-pulse prediction, timing/alignment, and automatic return to homeostasis/glide correction | No ticker tape; retain simplified PWM hold and established monotonic reduction; IR recovery restores applicable evidence | `7a31d913`, reconciled under primary guide §6 |
| Station dwell begins at Station +1 MM before the locomotive stops; base includes station STOP | Superseded: base cruise PWM 60 has no station STOP. Applied PWM=0 is the no-motive-movement indication and starts dwell; stop completes after dwell with PWM still zero. IR does not provide an additional stop-admission test. Observed movement at PWM zero remains governed by 0120. Resume independently of stop coordinate | Primary guide §§4–5, 8, 14 |
| Geographic station-stop acceptance region gates completion | Superseded: Station +1.5 is an *aim point*; prior +1.0–+1.7 and 0–+3 are diagnostic only. Actual early/late stop does not veto completion or demand corrective movement | Primary guide §§4–5, 14 |
| Runtime pitch/calibration changes identify a new measurement frame or exempt a report from ordering checks | Validate installed pitch; calibration ID is raw evidence; only real continuity boundaries separate epochs | Document B; 0122 |
| Station ARMED admission, persistent phases, completed-visit suppression, and historical station procedure across STOP/GO (0068, 0090, 0101 and POSITION_STATIONS_R1) | Current MM interval + direction + overlay determine the requirement; 0101's current-position principle survives | 0121; Document C §§5, 25–26 |
| Station-procedure MISSED, PHASE_TIMEOUT, procedural overshoot/late-entry failure, and marker/PWM sequences as operating authority | No historical station-procedure authority; current geographic boundaries remain legitimate and PWM is an actuator output | 0121; Document C §§2, 5 |
| An existing execution state keeps the old instruction authoritative after an overlay change | Execution state serves only a currently applicable instruction; physical maneuver and dwell state remain allowed | 0121; Document C §§3–4, 22–25 |
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

The primary guide §14 additionally preserves unresolved Hall-checkpoint use
during IR loss, tile-local execution-state lifetime/reset, exceptional dwell
clock initialization, physical-stop recognition without usable IR evidence,
and arbitrary-destination stopping-distance mapping. Ticker-tape first-lap,
playback alignment, and blending questions are retired with that architecture;
they must not be supplied as implementation defaults.


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
