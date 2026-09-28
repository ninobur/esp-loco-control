# NAVI_EYES_WIDE_OPEN: Historical Impostor Challenge

Date: 2026-09-28
Scope: repository research/replay only. No firmware, thresholds, IR processing, or NAVI behavior were changed.

## Verdict

**INCONCLUSIVE, with a strong partial result.**

The available Hall evidence supports the usefulness of the simple gates: four of seven classified non-MM impostor rows fail an available gate, and the known genuine-magnet failures caused by shape, duration, or return-flux handling would be conditionally accepted by the simplified model. However, none of the historical ONE STRIKE material contains native-rate synchronized IR distance. Three credible non-MM candidates survive every gate that can be applied from the old evidence. They cannot be called full-model survivors without manufacturing the missing IR measurement.

This is not a failure of the model and it is not support for the model. It is the result the experiment requires: the historical record identifies the cases that a synchronized IR replay must settle.

## Replay definition

The replay used exactly the proposed characteristics:

1. Hall departure of at least 70 counts for two consecutive native samples.
2. Opening polarity only; later return flux cannot overwrite it.
3. Measured IR travel since the previous accepted MM divided by the mapped interval, with the existing 0.85..1.15 window retained for this study.
4. Expected sequence and local context.
5. PWM=0 is not locomotive route movement, per decision 0106.

No waveform shape, Gaussian fit, lobe classifier, closure rule, duration floor, PWM-derived distance, or elapsed-time-derived IR distance was added. A stationary interval with no IR displacement was treated as a valid no-movement measurement, per decision 0108. A missing synchronized IR measurement is recorded as `UNTESTABLE FOR FULL MODEL - IR DISTANCE UNAVAILABLE`, never as zero distance.

The complete row-level registry is [20260928_navi_eyes_wide_open_historical_impostor_registry.json](/Users/davidbrown/esp-loco-control/field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_registry.json). It preserves the actual measured Hall values, polarity, mapped interval where documented, PWM/movement context, and source document for each row.

## Evidence population and limitation

The registry contains 23 evidence rows:

- 21 core documented ONE STRIKE or associated Hall cases, including grouped mechanisms where the repository does not preserve one candidate per physical magnet;
- 2 supplemental QUORUM false-accept cases from the August 25 trace replay.

All 23 rows have `ir_distance_mm: null`. The ONE STRIKE captures predate the native-rate synchronized IR recorder. The later NSR1 recorder cannot be used to reconstruct historical IR distance. The two QUORUM cases provide useful non-IR physical-speed contradictions, but their speed arithmetic is not substituted for the requested IR wheel distance.

The replay summary is reproducible with:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/replay_historical_impostors.py \
  field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_registry.json
```

## Incident registry

`IR ratio` is `-` for every historical row because synchronized IR distance was unavailable. `Conditional accept` means the non-IR gates pass; it is not a full-model acceptance.

| Incident | Class | Historical physical interpretation | 70x2 | Opening / expected | Context | Mapped mm | Replay result |
|---|---|---|---|---|---|---:|---|
| F02 MM110 | A | Genuine MM rejected by `WRONG_SHAPE` | pass | N / N | pass | 300 | conditional accept; IR unavailable |
| F03 MM146 | A | Genuine MM rejected by `WRONG_SHAPE` | pass | N / N | pass | 300 | conditional accept; IR unavailable |
| F05 MM70 | A | Genuine South MM, wrong entry impulse | fail | N / S | fail | 300 | reject; false rejection of genuine MM |
| F06 MM119 | A | Genuine North MM, wrong entry impulse | fail | S / N | fail | 300 | reject; false rejection of genuine MM |
| F07 MM169 | A | Genuine MM with tail artifact; shape rejection | pass | N / N | pass | 330 | conditional accept; IR unavailable |
| F08/F09 latch | D | Baseline/acquisition passage swallowed markers | n/a | n/a | corrupted | n/a | not a single candidate |
| F10 Bamboo baseline | D | Baseline captured on stopped magnet | n/a | n/a | reference corrupted | n/a | not a single candidate |
| F11 Grillers dwell | A/C | Genuine magnet inside a dwell-spanning passage | pass | not preserved | mixed | 300 | indeterminate; IR unavailable |
| F12 MM113 | A | Genuine MM after derailment; shape distorted | pass | N / N | pass | 300 | conditional accept; IR unavailable |
| F13 MM106 | A/C | Genuine MM arrived in a stationary fringe passage | pass | S / S | contaminated | 300 | indeterminate; IR unavailable |
| F14 stitched apex | A | Genuine crossing refused after stall/lurch; no strike | pass | not preserved | no contradiction | n/a | not an impostor acceptance |
| X14 Bamboo successor | E | Close post-stop successor versus re-read is disputed | pass | S / S | apparently pass | 300 | unresolved identity; IR unavailable |
| X15 Arches re-read | B | Non-MM/re-read admitted after post-stop exception | pass | N / S | fail | 300 | rejected by polarity/sequence |
| X15 Grillers successor | E | Close successor versus re-read unresolved | pass | S / S | pass | 325 | survives screens; identity unresolved |
| X16 Northpoint latch | D | Reference/acquisition latch swallowed markers | n/a | n/a | corrupted | n/a | not a single candidate |
| MM140 duration floor | A | Genuine South MM rejected by duration floor | pass | S / S | pass | 300 | conditional accept; IR unavailable |
| X19 Arches | B | Three stationary field transitions accepted as MMs | pass | N / N | pass | 300 | rejected by PWM=0 / decision 0106 |
| X20 MM136 | C | Genuine South MM assigned North by later shelf | pass | S / S | pass at opening | 300 | conditional accept; fixes polarity overwrite |
| X21 Grillers departure | B | Moving departure field transition counted as MM60 | pass | S / S | pass | 315 | **survives available screens; IR needed** |
| X21 MM117 burst | B | Non-MM opening before genuine MM116 | pass | N / S | fail | 300 | rejected by polarity/sequence |
| X21 Bamboo transient | B | Short non-MM North transient before South MM154 | pass | N / S | fail | 300 | rejected by polarity/sequence |
| QUORUM 243211 | B | Non-MM or re-read; physical-speed contradiction | pass | pass / pass | pass | n/a | **survives available screens; IR needed** |
| QUORUM 263774 | B | Non-MM or re-read; local interval restored when removed | pass | pass / pass | pass | n/a | **survives available screens; IR needed** |

The full source notes behind each row are listed in the registry. The table deliberately does not use morphology to rescue or reject a candidate.

## Direct counterexamples and required missing observation

Three rows survive every characteristic that the historical evidence can test:

1. **X21 Grillers departure.** The departure transition reached the 70x2 threshold, opened with the expected South polarity, occurred while the applied PWM was positive during the ramp, and was coherent with the then-current sequence. It was nevertheless a field transition rather than the next physical MM. The missing observation is the accepted IR travel from the previous physical MM to this candidate, compared with the documented 315 mm interval.
2. **QUORUM 243211.** The opening survived the replay's amplitude, polarity, and sequence stages, but the contemporaneous trace made the physical event implausible. Native synchronized IR distance is absent.
3. **QUORUM 263774.** Same surviving non-IR gates; removing the event restores a normal local interval. Native synchronized IR distance is absent.

These are not proven full-model counterexamples. They are the exact historical cases that must be measured with synchronized IR before the model can be supported. No additional rule is proposed here.

The unresolved X14 Bamboo and X15 Grillers post-stop passages are not counted as counterexamples because the repository cannot independently establish whether each was a genuine close successor or a re-read. They remain useful targets for future synchronized capture.

## Genuine-magnet behavior

Six genuine cases pass the non-IR portion of the proposed model and would be conditionally accepted if IR distance were coherent: F02/MM110, F03/MM146, F07/MM169, F12/MM113, MM140, and X20/MM136. This directly addresses the historical false-rejection classes caused by shape, duration, or allowing return structure to overwrite opening polarity.

F05/MM70 and F06/MM119 are important counter-results. They are genuine magnets, but the recorded opening impulse fails the 70x2 and expected-polarity tests. The simple model would refuse them, preventing an unsafe advance but also failing to recognize a real MM. F11/Grillers dwell and F13/MM106 are mixed passages whose opening/context evidence is not preserved cleanly enough for a binary replay. F14 already refused a corrupted crossing and did not cause a false advance.

## Answers to the requested questions

1. **Incidents reviewed:** 23 evidence rows: 21 core rows and 2 supplemental QUORUM rows. This is not a count of distinct physical shutdowns because several records group one failure mechanism or repeated field observations.
2. **Began with failure to recognize a genuine MM:** 10 rows are classified A or A/C. Two are mixed passage cases; the clean genuine-MM availability failures are the shape, impulse, duration, derailment, and stitched-waveform cases listed above.
3. **Began with acceptance of a non-MM impostor:** 7 rows are classified B.
4. **Primarily context/direction/reference problems:** 3 rows are explicitly D (F08/F09, F10, X16). X20 is separately classified C because it is a genuine MM whose identity was corrupted by later return structure. Two E rows remain unresolved rather than being forced into this count.
5. **Genuine MMs conditionally accepted:** 6 clean rows pass the available non-IR gates. Full-model acceptance is untestable for all six because IR distance is absent.
6. **Impostors failing at least one available characteristic:** 4 of the 7 B rows: X15 Arches, X19 Arches, X21 MM117, and X21 Bamboo.
7. **Rejecting characteristic:** X15 Arches fails expected opening polarity and sequence; X19 Arches fails PWM=0; X21 MM117 and X21 Bamboo fail expected opening polarity and sequence. The three survivors have no available IR distance to test.
8. **Non-MM impostors surviving all applicable evidence:** 0 proven complete-model survivors; 3 survive every historical gate that can actually be applied. They are X21 Grillers plus the two QUORUM rows, all `UNTESTABLE_FOR_FULL_MODEL - IR DISTANCE UNAVAILABLE`.

## Conclusion

The historical replay gives a strong partial result for the deliberately simple architecture:

- expected opening polarity removes several known re-reads and transients;
- PWM=0 removes the documented stationary Arches false advances without treating stationary IR as a fault;
- removing morphology and return-flux authority would preserve several genuine magnets that old builds rejected;
- reference/acquisition failures remain a separate problem rather than being mislabeled as impostor recognition;
- at least three moving, non-morphology-only candidates remain to be adjudicated by actual IR distance.

Therefore the formal verdict is **INCONCLUSIVE**. The next evidence needed is not another Hall rule. It is native-rate synchronized Hall plus accepted IR cumulative travel for the X21 Grillers and comparable moving false-accept cases, so their distance ratios can be reported against the existing 0.85..1.15 interval window.

## Sources

- `docs/NAVI_ONE_FIX_TO_STRIKE_CROSS_VARIANT_ANALYSIS_20260919.md`
- `docs/SIMPLE_DETECTOR_REPLAY_QUORUM.md`
- `docs/AUTO_OPENING_SEPARATION_SEPTEMBER.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_02_SHAPE_REJECTION_LAG_STOP.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_03_SECOND_SHAPE_REJECTION_MM146.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_05_IMPULSE_FLIPPED_POLARITY_AT_MM70.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_06_SECOND_ENTRY_IMPULSE_MM119.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_07_ARTIFACT_IN_THE_TAIL_WRONG_SHAPE_MM169.md`
- `docs/NAVI_ONE_0_9_FIELD_FINDING_12_A_DERAILED_CAR_CORRUPTS_SHAPE_NOT_AMPLITUDE.md`
- `docs/NAVI_ONE_0_9_FIELD_FINDING_13_THE_DEPARTURE_MAGNET_LANDS_INSIDE_THE_DWELL_PASSAGE.md`
- `docs/NAVI_ONE_1_0X_FIELDTEST_FIELD_FINDING_14_THE_APEX_IS_LOST_WHEN_A_STALL_ENDS_IN_A_LURCH.md`
- `docs/NAVI_POST_STOP_RESOLVER_REVIEW_20260912.md`
- `field-records/20260913_NORTHPOINT_ACQUISITION_LATCH.md`
- `field-records/20260914_MM140_FLOOR_REJECTION.md`
- `field-records/20260915_OTTO_X19_ARCHES_DWELL.md`
- `field-records/20260915_OTTO_X20_MM136_POLARITY_INVERSION.md`
- `field-records/20260916_OTTO_X21_GRILLERS_REST_MIGRATES_ON_THE_RAMP.md`
- `field-records/20260916_OTTO_X21_MM117_BURST_OPENING_STRIKE.md`
- `field-records/20260916_OTTO_X21_BAMBOO_37MS_PHANTOM_CW.md`
- `docs/decisions/0106-motive-pwm-zero-is-a-non-navigation-interval-for-sensor-changes.md`
- `docs/decisions/0108-measurement-applicability-absence-of-measured-change-is-not-sensor-failure.md`
