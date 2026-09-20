# IR mixed-light lap, 2026-09-20

Operator reported one complete lap after the GPIO connection repair, wheel
insert installation, and bench verification of ten counts per revolution.
Battery power replaced USB on the test car. RX 1.2 captured ESP-NOW data via
the Pi recorder after its serial connection was reopened.

Source: /home/david/NGR/ir_espnow/ir_espnow_raw_20260920.log on the railway Pi.
Raw packet sender session observed: e4410597. No firmware changes during lap.

## Operator annotations

Times below are the latest RX timestamps read after each message, not exact
physical transition times. Messaging and tool latency apply. The RX clock
is milliseconds since receiver boot, not the transmitter sampling clock.

| Observation | Approximate RX seconds | Pi arrival epoch |
| --- | ---: | ---: |
| Run started | 184.292 | 1789931526.581727 |
| Leaving mostly shady for mostly sunny | 419.445 | 1789931761.695502 |
| Very sunny | 468.483 | 1789931810.755317 |
| Shady starts | 531.825 | 1789931874.108704 |
| Mostly sunny starts | 625.410 | 1789931967.622318 |
| Complete lap reported | 635.318 | 1789931977.599300 |

The annotation interval spans approximately 451 seconds (7 min 31 sec).
This is not a precision lap time. The operator did not report a stop at
lap completion, so subsequent records must not be labelled stationary.

## Evidence limits

## Route and final stop annotations

The operator subsequently specified the run started between MM040 and MM041,
clockwise. Mile-marker landmarks should be comparable between runs. The
operator planned to stop on returning to MM040-041, reported slower running
at the end, then confirmed DONE. The latest RX record checked after the first
completion message was RX 859.775 s, Pi epoch 1789932202.051559. This is an
approximate final-stop annotation, not a sample-exact stop time.

Distinguish the earlier complete-lap message (RX 635.318 s) from this final
run-stop message. Do not infer an exact lap count from the messages alone.
Account for deliberate slowing when comparing waveform periods; longer
periods near the end are not automatically missed pulses. The MM040-041
start/finish location is operator-reported, not independently verified from
locomotive telemetry here.

## Analysis status

Fresh IR packets were present at each checked annotation, including sunny
and shady sections. This does not establish uninterrupted delivery or
correct wheel counting. No complete waveform, CRC, loss, saturation, or
count-accuracy audit has yet been performed for this lap. Keep transmitter
sampling/detector evidence separate from missing receiver packets. Review
contrast and rail clipping in each annotated section and near transitions;
do not attribute an anomaly to sunlight solely by temporal association.
