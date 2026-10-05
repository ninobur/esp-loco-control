# NAVI position/overlay documentation reconciliation — 2026-10-05

Documentation-only reconciliation authorized by David. Starting commit: `b79ce94231fda537e67f4c0aeb8b611848c8351a`.
No firmware, tests, operating behavior, adaptive braking, CTO/CE implementation, packets or protocols changed.

## Current authority and scope

[Decision 0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md) records the settled position/overlay decision. [Document C](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md) is the full architecture.
The current MM interval, direction and current overlay determine the operating requirement. Historical procedure state does not grant or retain authority; short-lived execution state serves a current instruction.

- [A — CTO/CE reconstruction](NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md): future close-train relationships, bubbles, consist geometry and service profiles.
- [B — vestigial-code cleanup](NAVI_VESTIGIAL_CODE_CLEANUP_ARCHITECTURE_20261005.md): current justification for production concepts and deliberate removal of obsolete ones.
- [C — position/overlay operating authority](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md): current geographic requirements and the rejection of historical procedure as authority.

All three remain separate and unchanged. This report classifies documentation; it grants no implementation authority.

## Supersession and implementation notices

No older document is wholly superseded by this reconciliation. Notices are scoped; removing each added notice reproduces the entire original file byte for byte. Original dates, statuses, reasoning, rejected alternatives, field evidence and implementation claims remain available.

| Record | Disposition | Exact scope |
|---|---|---|
| [docs/decisions/0101-current-position-determines-local-operating-requirements.md](decisions/0101-current-position-determines-local-operating-requirements.md) | Partial status notice | Current-position insight survives. Completed-visit suppression, preserved station procedure across pauses, and phase-watchdog authority are superseded. Genuine current dwell execution remains allowed. |
| [docs/decisions/0068-station-stops-return-with-per-direction-stopping-points.md](decisions/0068-station-stops-return-with-per-direction-stopping-points.md) | Partial status notice | Exact −10 station admission, persistent phase/Idle throttle authority, procedural +5 overshoot, 120-second phase timeout, and marker/PWM sequences as operating authority. Physical geography, original settings and actuator evidence remain history. |
| [docs/decisions/0090-sequence-overrules-declaration-15pct-window-auto-enabled.md](decisions/0090-sequence-overrules-declaration-15pct-window-auto-enabled.md) | Partial status notice | Exact −10 arming, armAfterCorrection, retained phase authority, and overshoot → MISSED → cruise. The actual record does not specify a station watchdog; that claim belongs to 0068 and R1. Existing navigation/IR supersessions remain intact. |
| [docs/NAVI_POSITION_STATIONS_R1_20260923.md](NAVI_POSITION_STATIONS_R1_20260923.md) | Historical design; partial supersession | Visit/phase authority, pause history, watchdog, procedural overshoot/late entry and marker/PWM operating sequences are not the target architecture. Tests, settings and compatibility telemetry remain evidence of R1. |
| [docs/NAVI_POSITION_STATIONS_R1_REVIEW_20260924.md](NAVI_POSITION_STATIONS_R1_REVIEW_20260924.md) | Historical review; partial supersession | Procedural endorsements and proposed repairs are no longer target requirements. Reproduced failures and the review of that exact build remain valid historical evidence. |
| [docs/NAVI_LEGACY_CTO_ASSUMPTIONS_AUDIT_20260923.md](NAVI_LEGACY_CTO_ASSUMPTIONS_AUDIT_20260923.md) | Partial conclusion notice | The §5 and Lessons Worth Keeping endorsement of visit history does not preserve completed-visit suppression. Phase/watchdog/pause descriptions cannot confer target authority. Dwell and smooth actuation are not rejected. |
| [docs/NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md](NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md) | Partial classification notice | The appendix class-A station phase watchdog endorsement is superseded; the data-flow/timing inventory remains historical. Other temporal execution/measurement mechanisms and navigation notices are unchanged. |
| [docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md](NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md) | Partial proposal notice | §7 cannot require inheritance of the old station phase/arming policy unchanged. Document A separately governs future CTO/CE. Packet layouts, protocols, field evidence and old proposal text are untouched. |
| [docs/NAVI_SIMPLIFIED_REVIEW_RESPONSE_20260918.md](NAVI_SIMPLIFIED_REVIEW_RESPONSE_20260918.md) | Partial disposition notice | Rows 1/13 cannot require obsolete missed-stop/phase-timeout handbacks or historical station-phase authority. Original accepted/open dispositions and unrelated actuator-correctness findings remain history. |
| [docs/NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md](NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md) | Partial continuation-record notice | Its station-phase/skip-stop work plan is not current NAVI/EWO governing architecture. Physical actuator-transition questions remain distinct and are not answered by this documentation task. |
| [firmware/README.md](../firmware/README.md) | Implementation/authority clarification | Catalogued visit/pause behavior and controlling-record links describe implementation and validation history, not the target. No build or field status is changed. |
| [firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/README.md](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/README.md) | Implementation/authority clarification | The R1 candidate still contains the described visit and pause/watchdog machinery; its existence does not govern the target architecture. |
| [firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md](../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md) | Implementation/authority clarification | Retained StationMachine visit/dwell/departure responsibilities are implementation facts, not future operating authority. Legitimate execution state and separate adaptive braking remain distinct. |

The [governing index](NAVI_EWO_GOVERNING_DOCUMENTS.md) now qualifies its blanket 0101 endorsement, registers 0121 and records the limited supersessions. 0101’s current-position insight remains foundational. Only the conflicting procedural mechanisms lose governing authority.

## Search and classification

Searched current governing documentation, all Markdown under `docs/` (including decision records), the firmware catalog, relevant NAVI program READMEs and NAVI_COHERENCE reference READMEs. Terms covered ARMED/arming/admission, StationMachine/StPhase, persistent station phases, completed visits/completedIdx, one-stop-per-visit, visit identity, entryPwm/lastOff/pausedAtMs, pause/resume, MISSED, PHASE_TIMEOUT, phase watchdogs, procedural overshoot/late entry, STOP/GO, ZERO_RAMP and prerequisite-marker history. Contextual reads checked wrapped claims, known decision records and the old prospectus.

277 matched or contextually selected lines in 87 documents were classified. The inventory below refers to **line numbers at the starting commit**, before notices shifted the files; links open the retained documents. Categories classify the occurrence in context, not the entire file:

1. Current governing claim, including current rejection of an old concept.
2. Current implementation description, explicitly distinguished from target authority.
3. Historical record, prior-art proposal, source analysis or a different subsystem.
4. Superseded architectural claim; see the scoped notices above.
5. Mere telemetry/test reference, including historical evidence labels.

Counts by category: 1: 42, 2: 7, 3: 144, 4: 37, 5: 47.

| Document | Category: baseline lines | Disposition |
|---|---|---|
| [docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md](AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md) | 3: 94, 471 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/AUTONOMOUS_ACQUISITION_IMPLEMENTATION_MAP.md](AUTONOMOUS_ACQUISITION_IMPLEMENTATION_MAP.md) | 3: 70 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md](AUTONOMOUS_POSITION_ACQUISITION_SPEC.md) | 3: 611, 712 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CODEX_QA.md](CODEX_QA.md) | 5: 165 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CTO3/BUBBLE_V1_SPEC.md](CTO3/BUBBLE_V1_SPEC.md) | 3: 144, 241, 250 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CTO3/resources/CTO_OPERATIONAL_PRINCIPLES_JULY_2026.md](CTO3/resources/CTO_OPERATIONAL_PRINCIPLES_JULY_2026.md) | 3: 91 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CTO3/station-stop-v1/FIELD_20260808_INAUGURAL_FINDINGS.md](CTO3/station-stop-v1/FIELD_20260808_INAUGURAL_FINDINGS.md) | 3: 26, 31 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CTO3/station-stop-v1/IMPLEMENTATION_REPORT.md](CTO3/station-stop-v1/IMPLEMENTATION_REPORT.md) | 3: 27, 33, 35, 91, 94, 103 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CTO3/station-stop-v1/README.md](CTO3/station-stop-v1/README.md) | 3: 16, 24, 25, 79, 83, 147, 156, 181, 206 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/CTO3_IMPLEMENTATION_AND_FIELD_EVIDENCE_REVIEW_20260924.md](CTO3_IMPLEMENTATION_AND_FIELD_EVIDENCE_REVIEW_20260924.md) | 3: 41, 43, 105, 113 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/IR_DEV_REC/2026-08-09_IR_SCOPE_BUILD.md](IR_DEV_REC/2026-08-09_IR_SCOPE_BUILD.md) | 3: 98, 135, 137, 149 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/IR_DEV_REC/2026-08-26_IR_ILLUMINATION_PREDICTION_RESULT.md](IR_DEV_REC/2026-08-26_IR_ILLUMINATION_PREDICTION_RESULT.md) | 3: 132 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_AUTO_CORRECTION_FAILURE_20260923.md](NAVI_AUTO_CORRECTION_FAILURE_20260923.md) | 3: 37, 45, 46; 5: 12, 13, 18, 23, 24, 25 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md) | 5: 179 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_BASELINE_TIMING_20260916_C3B93D0B.md](NAVI_BASELINE_TIMING_20260916_C3B93D0B.md) | 5: 336, 358 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_COHERENCE_0_5_FIRST_AUTO_RUN_20260922.md](NAVI_COHERENCE_0_5_FIRST_AUTO_RUN_20260922.md) | 5: 43, 113 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md](NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md) | 3: 58, 75, 487, 581, 664, 748, 826, 852, 869, 933; 4: 707, 709, 723 | Scoped notice; original file retained intact. |
| [docs/NAVI_CTO_DISCREPANCY_RESPONSE_20260910.md](NAVI_CTO_DISCREPANCY_RESPONSE_20260910.md) | 3: 22, 88, 98 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_EWO_GOVERNING_DOCUMENTS.md](NAVI_EWO_GOVERNING_DOCUMENTS.md) | 4: 65, 66 | Blanket 0101 endorsement qualified; 0121 registered. |
| [docs/NAVI_FINAL_RUN_ANALYSIS_20260923.md](NAVI_FINAL_RUN_ANALYSIS_20260923.md) | 3: 50, 62, 130 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md](NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md) | 3: 61 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_LEGACY_CTO_ASSUMPTIONS_AUDIT_20260923.md](NAVI_LEGACY_CTO_ASSUMPTIONS_AUDIT_20260923.md) | 3: 104, 123; 4: 127, 170 | Scoped notice; original file retained intact. |
| [docs/NAVI_MOTION_AWARE_REARM_REPLAY_20260915.md](NAVI_MOTION_AWARE_REARM_REPLAY_20260915.md) | 3: 74, 75, 159, 291, 296, 323 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_0_6_FIELD_FINDING_10_BASELINE_CAPTURED_AT_A_STATION.md](NAVI_ONE_0_6_FIELD_FINDING_10_BASELINE_CAPTURED_AT_A_STATION.md) | 3: 13, 14, 52, 54 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_0_7_FIELD_FINDING_11_A_DWELL_ON_A_MAGNET_IS_REJECTED_ON_SHAPE.md](NAVI_ONE_0_7_FIELD_FINDING_11_A_DWELL_ON_A_MAGNET_IS_REJECTED_ON_SHAPE.md) | 5: 35 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_0_7_FIELD_VERDICT_BASELINE_GATE_AND_GRILLERS.md](NAVI_ONE_0_7_FIELD_VERDICT_BASELINE_GATE_AND_GRILLERS.md) | 5: 17 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_0_9_FIELD_FINDING_13_THE_DEPARTURE_MAGNET_LANDS_INSIDE_THE_DWELL_PASSAGE.md](NAVI_ONE_0_9_FIELD_FINDING_13_THE_DEPARTURE_MAGNET_LANDS_INSIDE_THE_DWELL_PASSAGE.md) | 5: 14 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_1_0X11_EPIPHANY_FIRST_RAILWAY_RUN_ARCHES_CW_20260903.md](NAVI_ONE_1_0X11_EPIPHANY_FIRST_RAILWAY_RUN_ARCHES_CW_20260903.md) | 5: 34, 37 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_1_0X_FIELDTEST_CW_LANDING_PREDICTION.md](NAVI_ONE_1_0X_FIELDTEST_CW_LANDING_PREDICTION.md) | 3: 13, 31 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_FIELD_FINDING_09_LATCH_CAUGHT_IN_THE_FIELD.md](NAVI_ONE_FIELD_FINDING_09_LATCH_CAUGHT_IN_THE_FIELD.md) | 3: 27, 189, 203 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_FIX_TO_STRIKE_CROSS_VARIANT_ANALYSIS_20260919.md](NAVI_ONE_FIX_TO_STRIKE_CROSS_VARIANT_ANALYSIS_20260919.md) | 3: 38 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_OPERATOR_RULING_AUDIT_20260910.md](NAVI_ONE_OPERATOR_RULING_AUDIT_20260910.md) | 3: 41, 201 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_ORDINARY_LEVEL_BETWEEN_INTERVALS_20260916.md](NAVI_ONE_ORDINARY_LEVEL_BETWEEN_INTERVALS_20260916.md) | 3: 82, 121, 126, 170 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260903.md](NAVI_ONE_PUBLISH_EVERY_PASSAGE_PROPOSAL_20260903.md) | 5: 52 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_X13_FIELD_VERDICT_20260904.md](NAVI_ONE_X13_FIELD_VERDICT_20260904.md) | 5: 56 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_ONE_X18-X21_RAW_AUDIT_20260916.md](NAVI_ONE_X18-X21_RAW_AUDIT_20260916.md) | 3: 561 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md) | 1: 97, 99, 117, 129, 134, 138, 192, 194, 243, 257, 269, 271, 285, 293, 298, 300, 302, 310, 312, 326, 330, 332, 334, 344, 348, 431, 515, 517, 518, 519, 520, 521, 522, 536, 575, 576, 577, 578, 579 | Governing text retained unchanged. |
| [docs/NAVI_POSITION_STATIONS_R1_20260923.md](NAVI_POSITION_STATIONS_R1_20260923.md) | 3: 19; 4: 72, 77, 81, 137; 5: 93, 106, 108, 143 | Scoped notice; original file retained intact. |
| [docs/NAVI_POSITION_STATIONS_R1_REVIEW_20260924.md](NAVI_POSITION_STATIONS_R1_REVIEW_20260924.md) | 3: 38, 52, 63, 64, 67, 68; 4: 25, 33, 59 | Scoped notice; original file retained intact. |
| [docs/NAVI_PROXIMAL_R1_IMPLEMENTATION_20260923.md](NAVI_PROXIMAL_R1_IMPLEMENTATION_20260923.md) | 5: 99 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md](NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md) | 5: 6, 18, 23, 24, 25, 30, 71, 75, 111, 112, 129 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md](NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md) | 5: 36 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_SIMPLIFIED_DISTANCE_EVIDENCE_INVENTORY_20260918.md](NAVI_SIMPLIFIED_DISTANCE_EVIDENCE_INVENTORY_20260918.md) | 5: 235 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md](NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md) | 4: 97 | Scoped notice; original file retained intact. |
| [docs/NAVI_SIMPLIFIED_REVIEW_CLAUDE_20260918.md](NAVI_SIMPLIFIED_REVIEW_CLAUDE_20260918.md) | 3: 17, 43, 45 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_SIMPLIFIED_REVIEW_RESPONSE_20260918.md](NAVI_SIMPLIFIED_REVIEW_RESPONSE_20260918.md) | 4: 7, 19 | Scoped notice; original file retained intact. |
| [docs/NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md](NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md) | 3: 167; 4: 596 | Scoped notice; original file retained intact. |
| [docs/NAVI_X16_POST_STOP_PROPOSAL_20260912.md](NAVI_X16_POST_STOP_PROPOSAL_20260912.md) | 3: 148 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NAVI_X19_IMPLEMENTATION_20260915.md](NAVI_X19_IMPLEMENTATION_20260915.md) | 5: 131 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NGR NAVI_SIMPLIFIED.md](<NGR NAVI_SIMPLIFIED.md>) | 3: 395 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT3_REVIEW_CODEX_20260808.md](NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT3_REVIEW_CODEX_20260808.md) | 3: 50 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NGR_DASHBOARD_FINDINGS_20260807.md](NGR_DASHBOARD_FINDINGS_20260807.md) | 3: 74 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/NGR_NAVI_ARCHITECTURAL_LINEAGE_DRAFT_v0_1.md](NGR_NAVI_ARCHITECTURAL_LINEAGE_DRAFT_v0_1.md) | 3: 267, 340, 341, 366 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_1_0_IMPLEMENTATION_REPORT.md](QUORUM_1_0_IMPLEMENTATION_REPORT.md) | 3: 129 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_1_10_IMPLEMENTATION_REPORT.md](QUORUM_1_10_IMPLEMENTATION_REPORT.md) | 3: 20, 40 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_1_14_IMPLEMENTATION_REPORT.md](QUORUM_1_14_IMPLEMENTATION_REPORT.md) | 3: 25, 34 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_1_16R_REVIEW_RESPONSE.md](QUORUM_1_16R_REVIEW_RESPONSE.md) | 3: 32 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_1_3_IMPLEMENTATION_REPORT.md](QUORUM_1_3_IMPLEMENTATION_REPORT.md) | 3: 35, 36, 37, 44, 87 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_1_4_IMPLEMENTATION_REPORT.md](QUORUM_1_4_IMPLEMENTATION_REPORT.md) | 3: 14, 36, 38 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_LABEL_SLIP_ROOT_CAUSE.md](QUORUM_LABEL_SLIP_ROOT_CAUSE.md) | 3: 87 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md](QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md) | 3: 222, 390 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/QUORUM_v3_0_implementation_spec.md](QUORUM_v3_0_implementation_spec.md) | 3: 51, 627, 642, 644, 1320, 1329, 1330, 1505 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/X20_STATION_DWELL_IMPLEMENTATION_20260915.md](X20_STATION_DWELL_IMPLEMENTATION_20260915.md) | 3: 34, 41 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/X21_HALL_ONLY_NAV_IMPLEMENTATION_20260916.md](X21_HALL_ONLY_NAV_IMPLEMENTATION_20260916.md) | 3: 248 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0024-the-conservation-gate-measures-rather-than-predicts.md](decisions/0024-the-conservation-gate-measures-rather-than-predicts.md) | 3: 112 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0034-v1-14-cto-wire-and-membership-contracts.md](decisions/0034-v1-14-cto-wire-and-membership-contracts.md) | 3: 46 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0037-a-pairing-dissolves-when-nose-tail-order-inverts.md](decisions/0037-a-pairing-dissolves-when-nose-tail-order-inverts.md) | 3: 37 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0067-the-curve-into-patio-runs-at-105-ccw-and-hands-over-to-the-station.md](decisions/0067-the-curve-into-patio-runs-at-105-ccw-and-hands-over-to-the-station.md) | 3: 64, 67, 72 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0068-station-stops-return-with-per-direction-stopping-points.md](decisions/0068-station-stops-return-with-per-direction-stopping-points.md) | 3: 17, 49; 4: 8, 16, 98, 99, 105, 106 | Scoped notice; original file retained intact. |
| [docs/decisions/0070-a-passage-may-not-span-a-stop.md](decisions/0070-a-passage-may-not-span-a-stop.md) | 3: 131, 315, 328 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0072-every-judged-passage-is-kept.md](decisions/0072-every-judged-passage-is-kept.md) | 5: 121 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0074-shape-is-recorded-not-refused.md](decisions/0074-shape-is-recorded-not-refused.md) | 5: 82, 169 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0075-publishing-every-passage-is-gated-on-measurement.md](decisions/0075-publishing-every-passage-is-gated-on-measurement.md) | 5: 44, 63 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0078-navi-states-are-mapped-onto-the-frozen-cto-v3-wire-never-redefined.md](decisions/0078-navi-states-are-mapped-onto-the-frozen-cto-v3-wire-never-redefined.md) | 5: 29 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0080-field-test-rulings-expire-and-morphology-has-no-navigation-authority.md](decisions/0080-field-test-rulings-expire-and-morphology-has-no-navigation-authority.md) | 3: 11 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0087-hall-navigation-is-the-opening-and-a-645-ms-guard.md](decisions/0087-hall-navigation-is-the-opening-and-a-645-ms-guard.md) | 3: 81 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [docs/decisions/0090-sequence-overrules-declaration-15pct-window-auto-enabled.md](decisions/0090-sequence-overrules-declaration-15pct-window-auto-enabled.md) | 4: 74, 75, 76, 77, 110, 111, 112 | Scoped notice; original file retained intact. |
| [docs/decisions/0101-current-position-determines-local-operating-requirements.md](decisions/0101-current-position-determines-local-operating-requirements.md) | 1: 12, 19; 4: 17, 18, 25, 26, 27, 28 | Scoped notice; original file retained intact. |
| [docs/decisions/0109-navi-ewo-is-a-target-only-navigator-it-confirms-its-target-shrugs-at-everything-else-and-continues.md](decisions/0109-navi-ewo-is-a-target-only-navigator-it-confirms-its-target-shrugs-at-everything-else-and-continues.md) | 1: 37 | Unchanged: navigation Missed Magnet, not station MISSED. |
| [docs/reviews/NAVI_ONE_0_1_REVIEW_CLOSEOUT_20260830.md](reviews/NAVI_ONE_0_1_REVIEW_CLOSEOUT_20260830.md) | 3: 24 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [firmware/README.md](../firmware/README.md) | 2: 74, 118 | Scoped notice; original file retained intact. |
| [firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/README.md](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/README.md) | 3: 50 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/README.md](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/README.md) | 2: 49, 50 | Scoped notice; original file retained intact. |
| [firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md](../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md) | 2: 11, 38, 39 | Scoped notice; original file retained intact. |
| [firmware/programs/NAVI_IR/README.md](../firmware/programs/NAVI_IR/README.md) | 3: 123 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [firmware/programs/NAVI_ONE/variants/NAVI_ONE_SIMPLE/README.md](../firmware/programs/NAVI_ONE/variants/NAVI_ONE_SIMPLE/README.md) | 3: 51 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |
| [firmware/programs/NAVI_ONE/variants/NAVI_ONE_STATION_CURVES/README.md](../firmware/programs/NAVI_ONE/variants/NAVI_ONE_STATION_CURVES/README.md) | 5: 19, 23, 43 | Unchanged: historical/different-system context or telemetry/test evidence; not current NAVI station authority. |

## Conflicting historical material deliberately left unchanged

- **Legacy QUORUM/CTO3 and autonomous-acquisition specifications, implementation reports and station-stop-v1 records:** their station arming/phase models describe those earlier systems. Document C does not redefine their protocols or implementations. The NAVI governing index already treats QUORUM and prior navigation mechanisms as prior art; the new 0121 rules prevent importing their procedural authority into NAVI.
- **Decision 0070:** already explicitly superseded by 0080; its station-event table and stop-spanning morphology evidence remain historical. No second rewrite is needed.
- **Decision 0067:** station handover and starting-PWM remarks describe historical curve/actuator requirements, not a separate justification for a procedural admission latch. Physical starting output can remain execution context under Document C §§10/17/18.
- **Decision 0078 and legacy wire documentation:** stationPhase ordinals are historical packet/telemetry contracts. A field name alone is not operating authority. No protocol change or wider CTO supersession is performed by this C-focused reconciliation.
- **Dated NAVI field findings, correction failures, run logs, waveform/ramp studies, and reviews:** they report what particular builds did. Their phase labels, measured failures and successful checks remain evidence; they do not require the new architecture to reproduce the old behavior.
- **NGR NAVI architectural-lineage draft:** already calls POSITION_STATIONS_R1 a first implementation, not the final architecture; its phase/watchdog inventory and historical chronology remain useful.
- **The earlier NAVI_SIMPLIFIED prospectus §15.1:** records an open question about station behavior with unresolved position. It is not an adopted requirement for an ARMED latch; Document C settles operating authority without deciding new navigation-uncertainty behavior here. The linked continuation/disposition records now point explicitly to C.
- **Sensor ARMED states, IR sampling MISSED slots, NAVI Missed Magnet, and observation pause/resume:** these are not station-procedure admission or station MISSED. They are not changed merely because a word matches.

## Completion checks

- Repository README and NAVI decision model lead to the governing index; its separate read-first A/B/C references are intact and 0121 is registered.
- Known governing conflicts carry direct scoped notices or an implementation-versus-target clarification; the blanket 0101 endorsement is removed without discarding its principle.
- Notice-removal checks preserve every annotated original file byte for byte; A/B/C remain byte-identical to the starting commit.
- All introduced relative Markdown links resolve; whitespace and changed-file checks pass.
- The change set contains Markdown documentation only. No firmware source, tests, packet/protocol code or implementation behavior is changed. Firmware cleanup remains a separate task.
