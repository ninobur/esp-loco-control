# Operator-reported ten turns: observed packets

User reported "10 done." Boot remained 0x4d1e4080e443f121 throughout.
Capture has two distinct activity periods; physical trial boundaries and
spoke count need confirmation before scoring.

- Pi receipt 19:04:46.364, seq 4966: completed 0.
- 19:04:48.380, seq 4986: completed 103, saturation counter 3.
- 19:04:54.812, seq 5051: completed 106.
- 19:05:01.062, seq 5113: completed 106.
- 19:05:31.964, seq 5422: completed 346.
- 19:06:00.284, seq 5705: still 346.

Thus the later approximately 31-second period added 240 completed pulses.
The earlier burst plus subsequent crossings added 106. It would be incorrect
to assign all 346 to the ten-turn trial without operator confirmation.
No additional sample gaps occurred between the listed endpoints (counter 1).
No inferred corrections were applied. Distance remained unvalidated.

If the later interval was exactly ten turns of a ten-spoke wheel, expected
count would be 100 and net excess 140; this is conditional, not yet a scored
result. Most snapshots in that interval reported optical TRACKING, which
does not prove correct counts. Nominal distance must not be used as truth.

Raw copy: `field-records/logs/20260919_ir_ten_turn_capture.log`.
