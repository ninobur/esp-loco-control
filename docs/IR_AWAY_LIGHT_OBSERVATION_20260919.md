# Sensor facing away from light: capture awaiting phase clarification

Operator turned car around so the sensor faced away from the light, then
reported completion of the requested ten-turn repeat. Same boot
0x4d1e4080e443f121.

Observed activity (Pi receipt times PDT):
- 19:13:06.914-19:13:10.752: two small bursts, count 620 -> 628 (+8),
  plausibly repositioning but not independently assigned.
- 19:13:59.712-19:14:58.388: main activity, seq 10498 -> 11088,
  count 628 -> 2374 (+1746), about 58.7 seconds.
- Saturation increased by 20 during main activity; sample-gap counter unchanged.
- Latest seq 11110 remained at 2374, only about two seconds beyond last growth.

Main activity is not uniform: first approximately 25 seconds added 1437 counts
(through cumulative 2065), then roughly 34 seconds added another 309. This
requires operator phase clarification before assigning ten-turn error. Do not
automatically treat all 1746 as ten rotations or call orientation the cause.
298 snapshots were received across sequences spanning 591 potential reports;
radio gaps do not invalidate cumulative endpoint totals but obscure internal
phase timing. Reasons: 153 inadequate contrast, 137 tracking, 8 reacquiring.
Optical span across snapshots ranged 95-1535. Distance stayed unvalidated.

Ask whether the ten rotations occupied the entire approximately one-minute
activity period or only its later roughly 30-second portion, and whether the
wheel was stationary during the earlier rapid count growth.

Raw source preserved at `field-records/logs/20260919_ir_away_light_capture.log`.
