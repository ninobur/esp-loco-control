# October 15 evening field run — consolidated final report

Publication scope: this repository copy contains the final report only. Supporting reports, raw recordings and lap exports remain in the local evidence archive; their paths below identify sources rather than published links.


**Otto / NAVI_EWO_0_6 with the separate IR_WAVE_1_TX optical car.**

The session produced **two completed CW timing laps and one completed CCW timing lap**, demonstrated operator-assisted IR reassessment, and established two station-control problems: insufficient approach drive at Grillers and repeated stopping at Patio. The experimental transmitter delivered measurements to Otto during movement, but this run does **not** establish independently verified optical counting accuracy or field acceptance.

This is the final consolidation of the available evening-session evidence, not a declaration that every investigation is finished. Lowline findings cover approximately 18:25–19:43 PDT on October 15, 2026. The operator's reported Highline start at 19:43 is a separate session boundary; later preserved status extends to 19:48:50. Highline performance has not been evaluated here. The earlier October 14/15 waveform research and October 15 afternoon run are separate investigations.

The report preserves physical observations independently of NAVI's labels. An optical distance is a recorded estimate, and a selected Hall marker is NAVI's geographic assignment unless independently checked by the operator. No data was adjusted to agree with mapped distances. No operating change is authorized by this report.

## Equipment and firmware actually used

The separate IR car was flashed with **IR_WAVE_1_TX** at approximately 18:17. Morphology-only recognition supplies its operational count; rhythm is observational. Calibration remains **9.652 mm per completed cycle**. The deployment report (`ir-wave-1-validation/deployment/REPORT.md`, local evidence) identifies the image, verified backup, stationary acquisition measurements and recording gaps.

Otto continued running its earlier flashed **NAVI_EWO_0_6** image. The subsequent **STARTUP_LAP_R1** revision was implemented and compiled separately; **it was not flashed during this session**. It therefore cannot explain any observed recovery or timing behavior. Its implementation report (`ewo-0-6-startup-lap-validation/REPORT.md`, local evidence) preserves the source, tests, image and rollback path.

## Chronological record

Times below are Pi receipt times unless described as operator estimates. Source clocks establish ordering where MQTT delivery order could be misleading.

| Time, PDT | Observation and supported finding |
| --- | --- |
| 18:25 | CW selected, initial interval 146–147 declared, AUTO enabled. |
| 18:30–18:37 | Otto stalled physically between MM062–063 at Grillers. The approach held PWM 36 before FINAL; no final braking or dwell began. The operator confirmed that drive was inadequate for this consist at that location/direction. Grillers report (`field-records/20261015_IR_WAVE_1_GRILLERS/REPORT.md`, local evidence). |
| 18:38:58 | New controller boot followed the operator's confirmed power cycle. |
| About 18:41 | Operator reported Otto on its side between MM071–072. Optical movement ceased while PWM remained 110 until a recorded E-stop. The physical cause is unresolved. Operator restored Otto and restarted it. Rollover report (`field-records/20261015_IR_WAVE_1_MM071_072/REPORT.md`, local evidence). |
| 18:45:26–18:50:08 | Operator-reported obstruction around MM130. PWM remained 90 and the optical total stayed fixed while fresh IR snapshots arrived. Movement resumed at about 18:50:09 after removal. Obstruction/Bamboo report (`field-records/20261015_IR_WAVE_1_MM130_BAMBOO/REPORT.md`, local evidence). |
| 18:51 | Operator observed a successful Bamboo stop. Telemetry also records a second braking/dwell cycle; its exact mechanism was not reconstructed in that incident check. |
| 18:53:33 | NAVI withdrew IR authority at its MM019→020 interval, then stopped with `NO_PREVIOUS_LAP_TIMING`. The operator's final resting observation was MM021–022. Stop and recovery report (`field-records/20261015_IR_WAVE_1_MM021_022/REPORT.md`, local evidence). |
| 18:55:11–26 | New 021–022 declaration armed reassessment. A fresh Hall anchor and one provisional interval restored IR authority. |
| 18:58:49 | Another discrepancy at NAVI MM121→122 triggered `IR_FAILED_AFTER_OPERATOR_REASSESSMENT`. The operator later reported rest between MM123–124. Recorded movement continued during the decision delay and braking ramp. MM123–124 report (`field-records/20261015_IR_WAVE_1_MM123_124/REPORT.md`, local evidence). |
| 19:02:19–38 | Declaration 123–124, restart, and successful reassessment restored IR authority again. |
| About 19:05, 19:12, 19:19 | Three operator-observed Patio double stops are corroborated. Each involved a completed stop, departure, and a second stop after station reselection. First analysis (`field-records/20261015_IR_WAVE_1_PATIO_1905/REPORT.md`, local evidence), repeat (`field-records/20261015_IR_WAVE_1_PATIO_1912/REPORT.md`, local evidence), third occurrence (`field-records/20261015_IR_WAVE_1_PATIO_1919/REPORT.md`, local evidence). |
| 19:09:36 / 19:16:33 | First and second complete CW timing laps registered. |
| 19:24:22–31 | CCW selected, interval 121–122 declared, AUTO enabled. |
| 19:29:53–19:30:17 | MM161 obstruction: 25 status samples held the same optical total at PWM 50. Movement resumed around 19:30:18; IR authority remained retained. The operator subsequently clarified CCW, resolving the earlier verbal CW/CCW discrepancy. Evidence (`field-records/20261015_IR_WAVE_1_MM161/correlation-1930.json`, local evidence), clarification (`field-records/20261015_IR_WAVE_1_MM161/direction-clarification.txt`, local evidence). |
| 19:31:43 | First complete CCW lap registered: 171 intervals. |
| 19:35:47 / 19:36:14 | Patio again reached zero PWM twice, now CCW. Both dwells lasted approximately five seconds. CCW Patio findings (`field-records/20261015_IR_WAVE_1_PATIO_CCW_1936/FINDINGS.txt`, local evidence). |
| 19:38:20 | Second CCW attempt ended after 150 recorded intervals with `IR_FAILED_AFTER_OPERATOR_REASSESSMENT`. Complete previous-lap timing was available at this stop. |
| 19:40:04 onward | Declaration 143–144 and another restart/reassessment appear in the recording. This does not complete the interrupted second CCW lap. |
| Reported 19:43 | Operator announced the Highline run. A windblown bag obstruction was also reported, without a definite location or timestamp. It is not automatically assigned as the cause of a particular software stop. Session boundary (`field-records/20261015_IR_WAVE_1_HIGHLINE_1943/session-boundary.json`, local evidence). |

## Findings

**The long obstruction intervals did not require IR recovery.** At the MM130 and MM161 holds, fresh optical snapshots continued while the count stayed fixed. DART retained distance comparison. When physical movement resumed, measured distance resumed and subsequent Hall observations were selected. Elapsed time was not the active comparison criterion. This differs from loss of IR authority or loss of the radio stream.

The time was still recorded. The selected MM129→130 interval lasted **280.852753 seconds**; the CCW MM161→160 interval lasted **28.988381 seconds**. Both were marked suitable/resolved by the installed timing recorder, without manual-stop subtraction. Firmware suitability does not mean an interval is representative of unobstructed travel. The CCW obstruction remains explicitly identified in the preserved lap; it was not removed or replaced.

**The Grillers stall is an approach-control limitation.** Position-based PWM fell to 36 before the FINAL boundary. Once the consist stopped, position stopped progressing, so the controller continued asking for the same insufficient drive. FINAL's timed braking never began. The adequate replacement PWM or a revised rule was not determined or implemented.

**Patio's double stop is a demonstrated station-reselection defect in both directions.** After the first dwell, optical position briefly passed the station center. NAVI selected the next station, resetting the completed stop's execution state. The next selected Hall opening, labeled MM015, anchored the estimated position back at Patio's center. Patio became the nearest station again and a new braking/dwell cycle began. IR authority remained retained; this was not an IR-loss response.

The optical travel between rests was **318.516, 308.864 and 328.168 mm** in the three inspected CW cases, and **299.212 mm** in the CCW case. These values support the operator's description of roughly one marker interval of extra movement, without asserting an independent physical measurement. No repair to station selection was made.

**The navigation-fault stops were credibility decisions, not proof of a dead optical link.**

| Decision time | NAVI-assigned interval | Measured minus expected distance | Recorded stop |
| --- | --- | ---: | --- |
| 18:53:33 | CW 019→020 | −49.744 mm, −5.153751 pulses | `NO_PREVIOUS_LAP_TIMING` after authority withdrawal |
| 18:58:49 | CW 121→122 | −58.700 mm, −6.081641 pulses | `IR_FAILED_AFTER_OPERATOR_REASSESSMENT` |
| 19:38:20 | CCW 143→142 | +172.948 mm, +17.918359 pulses | `IR_FAILED_AFTER_OPERATOR_REASSESSMENT` |

The withdrawal threshold was five installed pulses, or 48.260 mm. The final event compared 472.948 mm of recorded optical distance with 300 mm expected between NAVI's selected endpoints. The subsequent metric record explicitly referenced timing lap 3. Thus the 19:38 stop was **not caused by an unpopulated timetable**: the post-reassessment failure rule requested the stop even though a timing prediction was available. Original decision excerpt (`field-records/20261015_IR_WAVE_1_EVENING_FINAL/ccw-second-attempt-stop.json`, local evidence).

These discrepancies do not independently identify missed/extra optical cycles, incorrect Hall selections, coordinate timing errors or geographic offsets. A full native Hall/optical reconstruction and independent waveform audit are still needed to separate those mechanisms. No count was corrected using the desired mapped distance.

## Timetables preserved

| Record | Completion | Coverage and status |
| --- | --- | --- |
| First CW lap | 19:09:36.729 | Completed live reference bank; retained in the incident recordings. |
| Second CW lap | 19:16:33.422 | Exported **171 intervals**, 416.836281 s. Selected as the compiled CW startup reference. Contains its actual station behavior, including Patio's repeat stop. Original export (`field-records/20261015_IR_WAVE_1_STARTUP_LAP_REVIEW/lap-2-source.json`, local evidence). |
| First CCW lap | 19:31:43.402 | Separately exported **171 intervals**, 421.563302 s. Includes the identified MM161 obstruction; no provisional intervals or manual-stop deductions. **Not installed as a firmware seed.** Original export (`field-records/20261015_IR_WAVE_1_CCW_TIMETABLE/ccw-lap-1-source.json`, local evidence). |
| Second CCW attempt | Interrupted at 19:38:20 | 150 distinct intervals before the subsequent redeclaration. Not a complete replacement lap. |

The controller's `completed_laps=3` means two CW completions plus one CCW completion, not three CCW laps. Its live history bank holds the latest qualifying lap for its recorded direction; these separate archival exports preserve both directions. No mixed lap was assembled from selected “good” intervals. The CW startup source and table remain unchanged by the later CCW and Highline activity.

## Recording, protection and unresolved work

Actual Otto receipt of the experimental transmitter was established during the run: the Grillers snapshot had **2,758 Type-6 events received and accepted**, zero invalid Type-6 events and zero receive-queue drops. Fresh Type-5 snapshots accompanied the standstill. That advances the earlier stationary deployment report's receipt status, but 25 sequence/pulse breaks were also recorded. Zero queue drops do not establish lossless delivery.

Stationary deployment measured approximately 1 kHz acquisition and 4.595 µs mean recognizer execution, but also retained a source-clock discontinuity and missing raw/timestamp pairs. Those are stationary measurements, not complete moving-run resource margins. The independent post-run ADC waveform audit has **not** been completed. These incident investigations inspected installed decisions and must not be represented as a blinded optical audit.

The native Hall, optical and MQTT files were observed growing during the Lowline checks. Original sources remain on the Pi:

- `/home/david/NGR/telemetry/all_20261015.log`
- `/home/david/NGR/ir_espnow/ir_espnow_raw_20261015.log`
- `/home/david/NGR/navi_sync/navi_sync_20261015_100403.nsr`

Local incident packages retain original excerpts or original-line extractions, source offsets and hashes. A growing native Hall file does not prove complete sample continuity; this report does not claim a full native Hall replay or loss audit.

Startup reported **INA219 not found**, so current/voltage evidence and battery protection were unavailable in the inspected session. The stationary-with-PWM field is only an indicator, conditioned on PWM greater than 60; it did not stop the motor during the rollover hold and remained zero at PWM 36 or 50. No new motor-protection behavior was introduced. The rollover's physical cause remains unresolved; darkness and the operator's lack of an observed obstruction do not establish a cause either way.

At 19:48:49, the preserved follow-up showed AUTO off, software PWM 115, and an IR snapshot about 378 seconds old. This later state is separated from the Lowline result and cannot establish Highline navigation, optical coverage or physical position. Exact Highline entry and its eventual outcome remain unverified.

Remaining work is to assess the three credibility failures against independent waveform and Hall evidence, validate moving-run acquisition/transport/resource continuity, resolve the rollover's physical cause where possible, and obtain separately approved changes for the demonstrated station-control problems. Any future CCW startup-table use must retain or explicitly address its obstruction interval. The present report changes no firmware, calibration, map, operating rule, service or device state.
