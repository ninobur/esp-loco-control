# NAVI_HYPOTHESIS_SHADOW

This is a serial-driven, non-motor experiment for the mechanism proposed in
the SAM review:

```text
Hall observation -> hard physical bound -> bounded hypotheses -> location
```

It is deliberately not a field navigation build. It has no motor, station,
MQTT, baseline, or stopping authority. Its first job is replay and shadow
evaluation against known NAVI_ONE failures.

## What it implements

- Hall opening and later-window polarity are retained separately.
- A measured physical maximum speed can reject a proposed marker
  interpretation that arrived too soon.
- Stationary observations cannot advance a route hypothesis.
- One contradictory observation can fork bounded interpretations for:
  polarity error, false observation, or one missed marker.
- Later route polarity evidence removes hypotheses that would require a
  second fault.
- Location is certain only when every surviving hypothesis names the same
  marker.

## What it does not claim

- Elapsed time is not distance. There is no minimum-speed or "too late" rule.
- A physically plausible, matching-polarity false event is indistinguishable
  from a real marker using Hall alone.
- A stable Hall reference is not proved magnetically clean.
- The one-fault bound is a testable policy, not a discovered law.
- No hypothesis may yet command a station stop or locomotive movement.

These limits are intentional. Adding recent Hall-derived speed as hard truth
would make the observation under judgment validate itself.

## Serial protocol

At 115200 baud:

```text
DECLARE 110 1 1000
VMAX 1000
OBS 1400 S N 1 1
```

`OBS` fields are time, opening polarity, window polarity, whether the window
vote is valid, and whether independent policy permits motion. Every command
prints the full surviving hypothesis set as JSON.

## Host test

```sh
c++ -std=c++17 -Wall -Wextra -pedantic \
  firmware/test-programs/NAVI_HYPOTHESIS_SHADOW/tests/test_hypothesis.cpp \
  -o /tmp/test_hypothesis
/tmp/test_hypothesis
```

The test covers clean navigation, a too-soon false event, stationary evidence,
wrong polarity, one missed marker, and opening/window disagreement.
