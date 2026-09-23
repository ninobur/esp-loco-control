# Stationary lights-off comparison

Operator switched lights off, noted remaining low ambient light and a brief
bump of the car. A fresh starting snapshot was obtained after that message;
the bump/transition is outside the scored interval.

Same boot 0x4d1e4080e443f121:
- Start sequence 17389, sensor time 1739761178 us.
- End sequence 17857, sensor time 1786578178 us.
- Duration 46.817 seconds.
- Completed count held 27045; observed-rise count held 27239.
- No new saturation or sample gaps.
- 50 received snapshots, all INADEQUATE_CONTRAST.
- Reported optical span min/median/max: 15/31/47 counts.

Prior operator-confirmed stationary lights-on interval produced 2779 false
pulses in 47.817 seconds (about 58.1/s). Lights-off observation produced zero
in a comparable interval. This strongly supports a lighting-related effect,
but the reported bump means orientation/coupling change is a possible confound.
A lights-on repeat without touching the car is needed for a reversible
comparison. Do not claim a measured flicker frequency or confirmed electrical
mechanism from these observations.

Low contrast while stationary does not prove sensor failure or a stop; it
means movement validity is unavailable. Cumulative endpoint counts are usable
for this independently confirmed still test despite substantial radio loss.
They do not establish a moving distance error bound.

Raw archive: `field-records/logs/20260919_ir_lights_off_capture.log`.
