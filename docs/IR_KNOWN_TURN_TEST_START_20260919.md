# Known-turn test starting observation

Follow-up: three consecutive goal turns awaited this same independent physical
trial. Latest checked snapshot is sequence 2416, sensor time 241983178 us,
same boot, zero completed pulses, INADEQUATE_CONTRAST. No independently counted
motion or spoke confirmation has arrived. Field validation is blocked pending
operator input; the full goal is not complete. Stationary recording alone
cannot establish motion sensitivity, error budgets, or marker discrimination.

Latest decoded Pi movement snapshot before requesting independent turns:
- Boot: 5556949912551420193 (0x4d1e4080e443f121).
- Sequence: 1865.
- Sensor time: 186869178 us.
- Completed pulses: 0; observed rises: 0.
- Optical reason: INADEQUATE_CONTRAST.
- Pi receipt epoch: 1789869576.193601.

Next required operator action: with USB still connected, rotate the measuring
wheel exactly ten full turns at about one turn per second, bracketed by ten
seconds stationary. Use a visible wheel feature to return to the same phase.
Report completion and whether the wheel still has the calibrated ten-spoke
pattern. Expected 100 pulses applies only if that pattern is confirmed.

This starting snapshot is evidence of count/time, not proof that subsequent
travel has occurred. No result or distance error is assigned until independent
turn count and ending snapshot are available. Retain initial departure losses
in the result rather than discarding them as warmup. Full operating-condition
validation remains pending, including the known slow-cycle limitation.
