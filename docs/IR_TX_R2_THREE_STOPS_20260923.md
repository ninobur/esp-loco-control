# IR TX R2: Three Short Rolls and Pauses

2026-09-23 approximately 20:10-20:12 PDT. Operator reported done after the
requested roll/pause sequence. Toby off; shroud/hubcap not requested to move.
No firmware or hardware configuration changes. Existing Pi recorder confirmed
active before the test; subsequent tail of daily log captured for analysis.

Initial bounded USB capture (TX time 2060-2155 s) preceded the actual rolls:
completed remained 110. Do not mistake that waiting period for failed rolls.
Pi recording continued after the USB capture closed. The selected Pi window
covers TX times 2103.313178-2274.739177, same boot 8dc31ef4e9f8b7c4.
It includes earlier movement from count 110 to 418 before the final three
short rolls; no independent distance/rotation ground truth was supplied.

## Last Three Short Rolls

| Roll | TX first/last changed-count report (s) | Completed count | Delta | Subsequent pause |
|---|---|---|---|---|
| 1 | 2223.093-2226.898 | 418 -> 462 | 44 | INADEQUATE_CONTRAST |
| 2 | 2236.906-2242.510 | 462 -> 532 | 70 | INADEQUATE_CONTRAST |
| 3 | 2258.023-2264.027 | 532 -> 627 | 95 | INADEQUATE_CONTRAST |

During each roll, reports included TRACKING. Each subsequent pause had no
completed-count growth, but no retained READY/SIGNAL_STALE measurement.
OpenAborts increased through the stops: 37 -> 40 -> 42 -> 44. Sampling gaps
remained at startup value 1; saturation remained 0. These are detector quality
transitions arriving over a live radio link, not absent radio reports.

After excluding roughly the first second after the last count change,
pause samples were all reason 1: 84, 141 and 92 received snapshots respectively.
Raw medians / 5th-95th percentile bands during these holds:

- First: 1937 / 1933-1946 (8352 raw samples).
- Second: 1586 / 1581-1595 (13920 raw samples).
- Third: 1607 / 1600-1619 (9216 raw samples).

Different resting optical levels were observed, but their physical spoke/gap
alignment is unknown; shielding prevents inspection. Small percentile bands
do not rule out ADC outliers. Raw min/max were 1811/2045, 1459/1673 and
1483/1724 respectively. Receiver-time windows approximate those holds;
not precise ADC-edge timestamps.

## Integrity and Disposition

Parsed 1700 matching type-5 snapshots with transport and internal CRC checks,
and 1766 matching raw batches with the existing transport CRC check. There
are 18 missing raw-batch ranges in the full saved window. Do not claim exact
pulse accuracy, gap-free waveform replay, or physical distance calibration.

The intended retained-zero behavior failed at all three final stop positions.
This repeats the initial bench finding and warrants design review before
further release or NAV/AUTO use. It does not by itself identify the precise
internal invalidation branch at each stop. The earlier waveform/code trace
in `IR_TX_R2_STOP_WAVEFORM_20260923.md` provides the candidate explanation:
fading live contrast revokes the remembered reference before quiet retention
can take over. Preserve explicit continuity loss; do not force an artificial
valid zero to conceal the failure.

No further physical rolling requested. R2 remains installed as a diagnostic
bench build; it is not physically accepted. Next: review captured gradual
deceleration and plateau noise, add representative regression coverage, and
separately decide whether/how to revise optical-reference retention without
reviving the false-zero/hidden-continuity bugs. No new flash authorized here.

Evidence: `field-records/logs/20260923_ir_r2_three_stops_radio.log.gz` and
`field-records/logs/20260923_ir_r2_pre_roll_wait.log`.
