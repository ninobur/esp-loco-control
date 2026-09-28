# NAVI_EYES_WIDE_OPEN

Clean-slate NAVI candidate. X22/X22R is not included.

The sketch separates observation from judgment:

* Hall acquisition publishes every native Hall sample and retains the raw
  samples surrounding an observation.
* IR acquisition publishes cumulative pulses, measured displacement, and its
  health facts. No IR fact is converted into a veto or a magnet count.
* `NaviCore` receives each native Hall observation, unchanged and in acquisition
  order, owns the initial-reference measurement, and decides what observations
  mean. It is the only component allowed to accept, hold, reject, advance
  position, or stop.

The only use of time in the `.ino` is physical sampling/service scheduling and
telemetry timestamps. No navigation decision uses elapsed time or a time-based
interval.

This is a development candidate: host tests pass, but it is not field
accepted and must not be flashed without hardware review.

## Function 1 and Function 2 boundary

The hardware-acquisition path does only the technically required ADC read and
attaches factual metadata: sample serial, timestamp, IR facts, PWM, and
direction. It does not filter, average, median, qualify, gate, classify,
interpret, suppress, or form Hall events. Later baseline, RTB, event, and
navigation behavior is intentionally not implemented here. NAVI's initial
reference collection is separate: after the first IR-reported movement, NAVI
collects native Hall observations over 10 mm of measured IR travel and sets the
median as the held initial reference. No later spatial replacement is included.

Run the host gates with:

```sh
sh NAVI_EYES_WIDE_OPEN/run_tests.sh
```
