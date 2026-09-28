# NAVI_EYES_WIDE_OPEN

Clean-slate NAVI candidate. X22/X22R is not included.

The sketch separates observation from judgment:

* Hall acquisition publishes every native Hall sample and retains the raw
  samples surrounding an observation.
* IR acquisition publishes cumulative pulses, measured displacement, and its
  health facts. No IR fact is converted into a veto or a magnet count.
* `SpatialHallReference` is measurement-only. It represents Hall samples by
  measured IR distance bins and returns a robust candidate reference. It has
  no timers, refractory period, cadence gate, dwell gate, polarity rule, or
  navigation state.
* `NaviCore` receives observations and decides what they mean. It is the only
  component allowed to accept, hold, reject, advance position, or stop.

The only use of time in the `.ino` is physical sampling/service scheduling and
telemetry timestamps. No navigation decision uses elapsed time or a time-based
interval.

This is a development candidate: host tests pass, but it is not field
accepted and must not be flashed without hardware review.

## Reference measurement

For a completed physically measured interval, callers feed Hall samples with
their cumulative IR distance. The reference is calculated as:

```text
IR-measured distance -> one robust Hall representative per distance bin
                      -> robust median across occupied bins
```

Zero IR progress is retained as a valid measured result. It does not mean IR
failure and it does not cause the sketch to discard the Hall samples.

The initial boot reference is the explicit operator-known clear condition. An
operational baseline after movement must be spatially acquired; the required
distance and RTB model remain open research questions.

Run the host gates with:

```sh
sh NAVI_EYES_WIDE_OPEN/run_tests.sh
```
