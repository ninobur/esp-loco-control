# `tools/interrupted_replay` — decision 0070's harness

**The change has landed in `firmware/programs/NAVI_ONE/variants/NAVI_ONE/`.** It is an
**experimental field-test build**, not field-accepted NAVI_ONE 1.0, and it
boots saying so. This directory no longer holds a copy of the firmware — the
duplicate `proposed/` tree was deleted the moment it became a second copy of
files that also live in `firmware/`, which is the exact failure this repository
has already had twice with `LocoConfig.h`. It is recoverable at `1cd027e` if it
is ever wanted.

```
0070.diff            the exact diff against 1b8b828 (NAVI_ONE 0.9), regenerate
                     with make_diff.sh
build_and_run.sh     RUN THIS
make_diff.sh         regenerate 0070.diff
make_fixtures.py     regenerate fixtures_captures.h from ~/ngr-telemetry/waveforms
```

The gate itself and its fixtures now live with the code they test, in
`firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/`.

## Running it

```sh
sh tools/interrupted_replay/build_and_run.sh
```

It extracts the historical `firmware/programs/NAVI_ONE` path **at commit
`1b8b828`** — the last
accepted firmware, the build Toby last ran — into a temporary tree, copies the
working tree into another, runs the eleven pre-existing gates against **both**
with the base commit's own unmodified runner, diffs the two outputs byte for
byte, and then runs gate 12.

The byte-for-byte diff is the equivalence proof: the eleven include the
2026-08-28 survey replay (187 real passages with their residuals), the
2026-08-29 lap replay, the polarity survey and both baseline gates. If the
change moved any verdict, residual or ruling anywhere those gates reach, that
diff would show it. It reads IDENTICAL.

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

**Gate 12 ends by naming a risk it does not close**, and says in its own summary
that a green run is not a clearance. See the decision record.

## What is real and what is modelled

**Real:** `HallCapture`, `MagnetRecognizer`, `Navigator`, `StationMachine` and
`RouteMap` are the firmware classes. `MagnetRecognizer` and `WaveformWindow` are
**unaltered** — this design has no second recognizer to test and moved no
threshold. `Rig` in gate 12 reproduces `NAVI_ONE.ino`'s `hallTask()`,
`stationService()`, `loop()`, `refusedStitched()` and `serviceRamp()`; they are
meant to be read side by side. The four records in `fixtures_captures.h` are the
samples the firmware itself published on `diag/waveform`.

**Modelled:** the ADC. A decimated field record is expanded by interpolating
across each stored reading's interval — the readings in between were never
transmitted, and they are *not* median-filtered, because the filter would smooth
the five stored samples that **are** finding 13's departure crossing. Sections D
onwards generate the field from a Gaussian model of a 30 mm magnet and drive it
with Toby's measured speed fit, `3.990 × (PWM − 25.1)` mm/s, and the station
machine's own ramp rates. Decision 0070 lists what these therefore do not prove.

## The sketch compiles

`arduino-cli compile --fqbn esp32:esp32:esp32`, core 3.3.11, against stub headers
for `PubSubClient` and `Adafruit_INA219` (neither is installed on this machine).

| | flash | RAM |
|---|---|---|
| `1b8b828` NAVI_ONE 0.9 | 957,287 | 58,996 |
| this field-test build | 960,239 | 60,164 |
| **cost** | **+2,952** | **+1,168** |

267,516 bytes of RAM remain free.
