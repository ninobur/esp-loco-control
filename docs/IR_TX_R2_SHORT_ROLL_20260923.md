# IR TX R2: First Short Hand Roll

2026-09-23, following authorized R2 flash. Toby confirmed off. Operator
reported prior handling movement and readiness for only a short roll.
Opened USB serial without resetting the IR car, then instructed a 5-10 second
forward roll followed by a stationary hold. Bounded 65-second capture.

Boot remains `8dc31ef4e9f8b7c4`. Initial completed count 53 belongs to earlier
handling, not this test. At capture times about 110 and 115 seconds since boot,
count 53 was stable with reason 4 (SIGNAL_STALE, retained quiet evidence).

At about 120 seconds: completed 69, TRACKING, span 1103.
At about 125 seconds: completed 94, rises 95, TRACKING, span 1231.
From about 130 seconds onward: completed/rises 99, INADEQUATE_CONTRAST,
span 31-47. Count stayed still, but retained readiness did not survive this
particular stopping transition. Sampling gaps stayed at the startup value 1;
saturation and reported send errors stayed zero.

Observed completed-count delta: 46. No independently measured roll length or
rotation count, so this is NOT a missed-pulse or distance-accuracy verdict.
The final unavailable state is not a valid-zero success. It also does not yet
prove an algorithm fault: stopping phase, raw waveform and learned reference
are not exposed by these five-second MOVE summaries. A mid-edge stop or
quality loss during deceleration remains a possibility, not a finding.

Operator was asked to leave the wheel in that position. No firmware edits,
reflash, motor commands or NAV/AUTO operation. Next: identify the optical
resting phase and capture controlled low/high stops with adequate waveform
evidence. Do not infer that an unavailable reading means measured zero.

Raw capture: `field-records/logs/20260923_ir_tx_r2_short_roll.log`.
