# `tools/interrupted_replay` — decision 0070's harness

**Nothing here is firmware.** `firmware/test-programs/NAVI_ONE/` is untouched
and stays untouched until the operator ratifies decision 0070. This directory
holds the proposal, the exact diff it would apply, and the test bench that runs
the real firmware classes with that diff applied in a temporary build tree.

```
proposed/            the four changed firmware files, complete, with the change
                     applied. MagnetRecognizer.h and WaveformWindow.h are NOT
                     here: this design does not touch them.
  tests/
    gate_interrupted.cpp   gate 12 -- the new gate, run against the proposed tree
    fixtures_captures.h    GENERATED: the four field records, verbatim
    run_tests.sh           the runner with gate 12 added
0070.diff            the exact diff, regenerate with make_diff.sh
build_and_run.sh     RUN THIS
make_diff.sh         regenerate 0070.diff from proposed/
make_fixtures.py     regenerate fixtures_captures.h from ~/ngr-telemetry/waveforms
```

## Running it

```sh
sh tools/interrupted_replay/build_and_run.sh
```

It assembles two trees under a temporary directory — one a verbatim copy of the
firmware, one with `proposed/` overlaid — runs the eleven existing gates against
**both** with the same unmodified runner, diffs the two outputs byte for byte,
and then runs gate 12, which exists only in the proposed tree.

The byte-for-byte diff is the equivalence proof the review asked for: the eleven
include the 2026-08-28 survey replay (187 real passages with their residuals),
the 2026-08-29 lap replay, the polarity survey and both baseline gates. If the
proposed change moved any verdict, residual or ruling anywhere those gates
reach, that diff would show it.

## What gate 12 covers

| | |
|---|---|
| A | the constants, and that `resumeMove` is `exitMargin` rather than a new number |
| B | loss-of-progression against the **measured** stationary noise of four real plateaus |
| C | findings 09 A, 09 B, 11 and 13 replayed through the real stack, clean and with noise |
| D | stops around the arc at three approach and three departure speeds, each against a **control** that creeps through the same way without stopping |
| E | the four artifacts the withdrawn design would have accepted, all refused |
| F | the sentinels: falling PWM does not pause a coasting locomotive; rising PWM does not resume a stationary, stalled or spinning one |
| G | uninterrupted crossings — nothing pauses without an arming |
| H | the real `StationMachine` over modelled track, coast swept 60–140 % |

## What is real and what is modelled

**Real:** `HallCapture`, `MagnetRecognizer`, `Navigator`, `StationMachine` and
`RouteMap` are the firmware classes, compiled from the firmware headers with the
proposed diff applied. `MagnetRecognizer` and `WaveformWindow` are the
firmware's **unaltered** — this design has no second recognizer to test. `Rig` in gate 12 reproduces `NAVI_ONE.ino`'s `hallTask()`,
`stationService()`, `loop()` and `serviceRamp()`; they are meant to be read side
by side. The four records in `fixtures_captures.h` are the samples the firmware
itself published on `diag/waveform`.

**Modelled:** the ADC. A decimated field record is expanded by interpolating
across each stored reading's interval — the readings in between were never
transmitted. Sections D onwards generate the field from a Gaussian model of a
30 mm magnet and drive it with Toby's measured speed fit,
`3.990 × (PWM − 25.1)` mm/s, and the station machine's own ramp rates. Decision
0070 lists what these therefore do not prove.

## The sketch compiles

`arduino-cli compile --fqbn esp32:esp32:esp32`, core 3.3.11, against stub headers
for `PubSubClient` and `Adafruit_INA219` (neither is installed on this machine).
Cost of the change: **+2,116 bytes of flash, +1,168 bytes of RAM.**
