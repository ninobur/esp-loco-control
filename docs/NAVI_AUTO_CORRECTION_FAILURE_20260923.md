# Toby AUTO failure: recorded correction and station withdrawal

Read-only Pi investigation, 2026-09-23, about 15:05 PDT. Source:
/home/david/NGR/telemetry/all_20260923.log, locally snapshotted to
/private/tmp/toby_20260923_investigation.log. Loco boot DB1F62954A510442.
David's independent endpoint: physically between MM29 and MM30, almost touching
MM30; dashboard MM163. No control commands, resets or serial readers used.

## Confirmed Timeline (Pi receipt times)

- 14:27:28.065: first SEQUENCE_CORRECTED, MM70 -> MM68, offset -2,
  matches_before=6, auto_running=0, station_phase=IDLE.
- 14:36:27.989: Grillers ZERO_RAMP, off=-1, requested PWM=0.
- 14:36:34.788: IR epoch 10 ends for INADEQUATE_CONTRAST, pulses=11835.
- 14:36:36.806: Hall event 430, ADVANCED_WITH_DISCREPANCY; navigator
  reports MM166, trust SEQUENCE_RECOVERED, corrections=2, seq_matches=10.
- 14:36:36.831: explicit SEQUENCE_CORRECTED MM65 -> MM166, offset -70,
  matches_before=8, auto_running=1, station_phase=ZERO_RAMP, armed_after=0.
  This is the confirmed global correction, not an inference from final state.
- 14:36:39.909: Grillers DWELL_BEGIN at offset -65.
- 14:36:44.908: Grillers DEPART still at offset -65, requested PWM110.
- 14:37:53.295: Grillers DEPARTED, offset 2.
- 14:38:23.923: Arches ARMED; later ramp, dwell and departure occur.
- 14:39:48.151: Bamboo ARMED, offset -10.
- 14:40:04.123: Bamboo ZERO_RAMP, offset 1.
- 14:40:13.160: IR epoch 13 ends for contrast, pulses=16158.
- 14:40:14.564: event 854 advances believed MM162 -> MM163, CW;
  corrections still 2, observed Hall opening PWM10.
- 14:40:14.587: Station action unresolved or approach failed: controlled stop.

## Mechanism

The global polarity-sequence relocation occurred while incumbent agreement was
8/10. The code allows a whole-route exact sequence match to override it without
the Twenty Questions' proximal physical-candidate filtering.

applySequenceCorrection does not reset an active station machine. Its optional
armAfterCorrection only acts when Idle. The recorded Grillers dwell/departure
at offset -65 demonstrates station state surviving the large frame change.
This was not an immediate station withdrawal: subsequent station actions used
the new believed route position.

The eventual withdrawal is explained by Bamboo overshoot: centre157, believed
MM163 CW means offset+6, exceeding OVERSHOOT_ABANDON=5 in Stations.h.
The Ramp-phase tick returns MISSED before timeout processing. RecoveryControl.h
maps MISSED/PHASE_TIMEOUT to agrees=false; stationService then withdraws AUTO.
Thus a station failure warning at MM163 is consistent with an already active
Bamboo approach, even though MM163 is beyond its normal stopping area.

## IR and Limits

At the large correction, IR was explicitly unavailable, not granting a physical
relocation. Counts remained11835 in sampled health records from14:36:34.788
through14:36:39.302 while Hall-based positions advanced. Unavailable IR and
unchanged counts cannot prove zero physical travel or prove those Hall signals
false; this is a separate issue for waveform/event review. The observation-only
health monitor did not cause the correction or station command.

The endpoint mismatch is physically confirmed by David. Intermediate MM labels
are NAVI's belief, not independent ground truth. Do not assert that the single
-70 correction alone quantitatively explains the final positional error: later
Hall advances/refusals and prior tracking errors must also be examined.

## Next Implementation Constraint

Use the settled Twenty Questions: incumbent inertia, physically possible
positions before scoring, absolute +/-10-MM outer boundary, valid travel and
direction constraints, physical rolling history with missing observations
UNKNOWN, unique better explanation, retained history after correction.
Unavailable IR must not authorize whole-route search. No firmware was changed
during this investigation. AUTO remains off in the inspected latest telemetry.
