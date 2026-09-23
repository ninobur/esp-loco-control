# TX 1.5 confirmed stationary failure in away-light orientation

Operator explicitly stated "still now". The assistant obtained a starting
snapshot afterward, waited, then obtained an ending snapshot. No wheel motion
was requested during the interval. Boot remained 0x4d1e4080e443f121.

| Field | Start | End |
| --- | ---: | ---: |
| Sequence | 14463 | 14941 |
| Sensor microseconds | 1447061178 | 1494878178 |
| Completed pulses | 12857 | 15636 |
| Observed rises | 13013 | 15799 |
| Saturated samples | 54 | 66 |
| Sample gaps | 1 | 1 |
| Unreliable samples | 1315568 | 1363385 |

2779 false completed pulses over 47.817 seconds, about 58.1/s. All 172 received
snapshots in the interval reported INADEQUATE_CONTRAST. Unreliable samples
increased by 47817, matching the full interval at 1 kHz. Distance stayed
unvalidated; no inferred corrections were applied. Radio snapshot loss was
substantial but does not explain cumulative counter growth on the transmitter.

This contradicts any broad claim that TX 1.5 solves stationary false counts.
The earlier zero-count stationary window remains true for its earlier setup;
it does not generalize to this orientation/condition. The diagnostic validity
gate did flag this current noise, but the raw completed count is not a reliable
physical movement measure. Neither scale adjustment nor dividing by a constant
can correct this demonstrated problem.

A preliminary integer-frequency coherent scan (45-125 Hz) over 11424 available
tail samples did NOT establish a dominant 60 Hz component. The approximate
58/s count rate alone is not proof of mains or lamp flicker. Light-source and
USB/electrical effects remain unisolated. Next useful controlled comparison:
same stationary wheel and orientation, change only the nearby electric light
if available; then compare USB and battery while preserving recording.

Raw archive: `field-records/logs/20260919_ir_confirmed_still_failure.log`.
