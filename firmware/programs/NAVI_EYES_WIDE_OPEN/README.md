# NAVI_EYES_WIDE_OPEN

Clean-slate NAVI candidate. X22/X22R is not included.

The sketch separates observation from judgment:

* Hall acquisition publishes every native Hall sample and retains the raw
  samples surrounding an observation.
* IR acquisition publishes cumulative pulses, measured displacement, and its
  health facts. No upstream IR movement judgment is sent to NAVI.
* `NaviCore` receives each native Hall observation, unchanged and in acquisition
  order, owns the initial-reference measurement, and decides what observations
  mean. It is the only component allowed to accept, hold, reject, advance
  position, or stop.
* NAVI receives cumulative IR distance and health facts; no upstream movement
  boolean determines whether Hall observations are delivered or collected.

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
reference collection is separate: NAVI compares successive cumulative IR
distance observations itself, then collects every native Hall observation over
the first 10 mm of measured travel and sets the median as the held initial
reference. Later replacement is NAVI-owned and begins only after target
confirmation as described below.

After the reference exists, NAVI retains the latest five native Hall readings
and calculates their median internally. For the known target, the median must
depart from the active reference by at least 70 counts in the mapped target
polarity direction. The earliest expected-direction raw threshold reading in
that qualifying five-sample window is retained as the physical opening
landmark. No slope, candidate-magnet, transient, or abstract observed-polarity
classification is performed. Hall support alone is insufficient: NAVI also
checks the target's mapped direction/context and measured IR distance within
±15%. An inconsistent observation does not identify an alternative magnet or
alter position; NAVI continues seeking the same target while its interval
remains open. Passing the interval records the target as missing and advances
the target context without erasing NAVI's position.

After target confirmation, NAVI uses the retained opening landmark's cumulative
IR distance—not the later median-confirmation sample—as the origin for the next
spatial Hall reference. It ignores the 0–100 mm clearance, selects one native
Hall representative per distinct measured millimetre in the 100–200 mm interval,
and replaces the active reference with the median at 200 mm. Repeated
observations at one measured location therefore do not gain extra weight from
stationary time. A failed target confirmation never starts this spatial-reference
cycle. The rolling window is cleared when the active reference, target, or
navigation context changes.

Run the host gates with:

```sh
sh NAVI_EYES_WIDE_OPEN/run_tests.sh
```
