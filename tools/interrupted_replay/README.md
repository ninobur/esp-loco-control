# `tools/interrupted_replay` — decision 0070's harness

**Nothing here is firmware.** `firmware/test-programs/NAVI_ONE/` is untouched
and stays untouched until the operator ratifies decision 0070. This directory
holds the proposal, the exact diff it would apply, and the test bench that runs
the real firmware classes with that diff applied in a temporary build tree.

```
proposed/            the six changed firmware files, complete, with the change applied
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
| A | `med3At` is `medianOfThree` elementwise — 20,000 random vectors |
| B | the constants the recognizer and the capture each restate agree |
| C | settle detection against the **measured** stationary noise of four real plateaus |
| D | findings 09 A, 09 B, 11 and 13 replayed through the real stack, clean and with noise |
| E | stops at 15 / 25 / 40 % rising, the peak, 40 / 15 % falling |
| F | eight things that are not magnets, with the stop rule switched **on** |
| G | uninterrupted slow crossings, 108 ms to 1800 ms sigma, each at its own physical PWM |
| H | the real `StationMachine` over modelled track, coast swept 60–140 % |
| I | twenty interrupted acceptances do not move the gain median |
| J | an episode that establishes nothing withdraws AUTO at once |

## What is real and what is modelled

**Real:** `HallCapture`, `MagnetRecognizer`, `Navigator`, `StationMachine` and
`RouteMap` are the firmware classes, compiled from the firmware headers with the
proposed diff applied. `Rig` in gate 12 reproduces `NAVI_ONE.ino`'s `hallTask()`,
`stationService()`, `loop()` and `serviceRamp()`; they are meant to be read side
by side. The four records in `fixtures_captures.h` are the samples the firmware
itself published on `diag/waveform`.

**Modelled:** the ADC. A decimated field record is expanded by median-filtering
the stored samples and interpolating between them — the readings in between were
never transmitted. Section H generates the field from a Gaussian model of a 30 mm
magnet and Toby's measured speed fit. Decision 0070 lists what these therefore do
not prove.

## The sketch compiles

`arduino-cli compile --fqbn esp32:esp32:esp32`, core 3.3.11, against stub headers
for `PubSubClient` and `Adafruit_INA219` (neither is installed on this machine).
Cost of the change: **+3,364 bytes of flash, +232 bytes of RAM.**
