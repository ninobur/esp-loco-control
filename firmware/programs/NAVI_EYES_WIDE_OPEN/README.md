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
reference. No later spatial replacement is included.

After the reference exists, two consecutive native Hall observations departing
from the active reference by at least 70 counts create a candidate opening.
Those two samples are excluded from confirmation. Three subsequent qualifying
samples that are strictly rising above the reference or strictly falling below
it confirm the magnet and establish polarity. Three subsequent sub-70 samples
abandon the candidate as a transient. This evidence does not grant an MM or
navigation authority.

After confirmation, NAVI uses that observation's cumulative IR distance as the
origin for the next spatial Hall reference. It ignores the 0–100 mm clearance,
selects one native Hall representative per distinct measured millimetre in the
100–200 mm interval, and replaces the active reference with the median at 200
mm. Repeated observations at one measured location therefore do not gain extra
weight from stationary time.

Run the host gates with:

```sh
sh NAVI_EYES_WIDE_OPEN/run_tests.sh
```
