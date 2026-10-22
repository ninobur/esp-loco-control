# NAVI_TRACER_0_1 — Python/C++ behavioral equivalence report

**Status:** field-test candidate for review. Not field accepted, not flashed.

## What this report covers, stated precisely

This report verifies that `NaviTracerCore.h` (the C++ port) produces the
**same event stream** as the validated Python reference
(`research/navi_target_acquisition_20261021/target_acquisition.py`, commit
`6efc952`) **when fed identical synthetic timestamped Hall observations.**

**It does not replay the real `otto_train` recording.** That raw recording
(`raw/otto_train.bin`) is not committed to this repository on any branch
(`research/NAVI_CPR_0_1/.gitignore` excludes it) and was not reachable from
the environment this port was implemented in — per the research's own
`README.md`, it exists only at two paths on David's own machine. Every
fixture used below is a synthetic reconstruction built from the shapes,
amplitudes, and durations the research's `REPORT.md` and its own synthetic
test suite describe, not a replay of recorded samples. Where a fixture
stands in for a real-recording scenario named in the field-test assignment
(the 284.58 s anomaly, Trial 6), that substitution is stated explicitly, not
implied.

**Getting the real recorded data onto Otto's actual Hall telemetry during
the field trial itself (via the NAVI sync UDP stream / Serial log) is what
will let a true recorded-data equivalence check happen after the first run**
— this report is the pre-field-trial check the assignment's §10.B asks for,
not a substitute for it.

## Method

1. `tests/navi_tracer_0_1/generate_fixtures.py` generates seven synthetic
   `(t, hall, pwm)` CSV fixtures, deterministically.
2. `tests/navi_tracer_0_1/run_python_reference.py` fetches
   `target_acquisition.py` and `route_map.py` from commit `6efc952` via
   `git show` (not vendored — no possibility of drifting from what was
   actually validated), runs the real `TargetAcquisition` class over each
   fixture, and writes its event stream as CSV.
3. `tests/navi_tracer_0_1/equivalence_runner.cpp`, built from the identical
   `NaviTracerCore.h` used in the ESP32 sketch, runs the C++ port over the
   same fixture and writes its event stream in the same CSV format.
4. `tests/navi_tracer_0_1/compare_events.py` diffs the two streams: same
   event kind, same order, same `target_mm`, and every numeric field
   (timestamp, duration, sign, peak departure, etc.) within `1e-6` —
   floating-point precision only, not a tolerance for behavioral difference.

Run with `tests/navi_tracer_0_1/run_equivalence.sh`.

## Fixtures

| Fixture | Exercises | Stands in for |
|---|---|---|
| `basic_correct_sequence` | correct-polarity curve → OPEN_CANDIDATE, ACQUIRED, CLOSE_AND_ADVANCE | — |
| `wrong_polarity` | opposite-polarity curve → CONTRADICTION, no advance | — |
| `short_noise_excursion` | 0.04 s active duration: DISPOSED under 110 ms, ACQUIRED under the duration floor | the research's own duration-sensitivity finding (§4/§7 of `REPORT.md`) |
| `extended_no_timeout` | 8 s open candidate, must still close normally, no LOST/TIMEOUT exists | — |
| `stop_restart_mid_candidate` | PWM drops to 0 mid-curve (stall), candidate evidence survives, resumes | real stop/restart |
| `284s_anomaly_synthetic` | ~284.6 s near-flat plateau (~80 ADC departed, slow creep) must not falsely RTB | **synthetic reconstruction** of the real Trial 0/MM129 anomaly's reported shape — see caveat above |
| `multi_magnet_sequence_synthetic` | 10 consecutive correct-polarity encounters, alternating polarity per the **real** route map (MM100–109) | **synthetic stand-in** for sustained multi-magnet operation (what Trial 6 exercises in the real recording) — NOT the real Trial 6 recording |

## Results

All 7 fixtures × 2 duration configurations (`0.02 s` duration-floor default,
`0.11 s` legacy) = **14 equivalence runs, all PASS**: identical event kind
sequence, identical `target_mm` progression, every numeric field within
`1e-6`.

```
basic_correct_sequence        d=0.02  PASS (3 events)
wrong_polarity                d=0.02  PASS (2 events)
short_noise_excursion         d=0.02  PASS (3 events)   # ACQUIRED under the floor
extended_no_timeout           d=0.02  PASS (3 events)
stop_restart_mid_candidate    d=0.02  PASS (3 events)
284s_anomaly_synthetic        d=0.02  PASS (3 events)
multi_magnet_sequence_synthetic d=0.02 PASS (30 events) # 10x (OPEN, ACQUIRED, CLOSE)
basic_correct_sequence        d=0.11  PASS (3 events)
wrong_polarity                d=0.11  PASS (2 events)
short_noise_excursion         d=0.11  PASS (2 events)   # DISPOSED under 110ms
extended_no_timeout           d=0.11  PASS (3 events)
stop_restart_mid_candidate    d=0.11  PASS (3 events)
284s_anomaly_synthetic        d=0.11  PASS (3 events)
multi_magnet_sequence_synthetic d=0.11 PASS (30 events)
```

Note that `short_noise_excursion` produces a *different* event count under
the two duration configurations (3 vs. 2 — ACQUIRED+CLOSE_AND_ADVANCE vs.
DISPOSED) — this is the expected, validated duration-floor-vs-110ms
behavioral difference from `REPORT.md` §4/§7, and both engines agree with
each other identically under each configuration, which is the actual claim
being tested.

Separately, `tests/navi_tracer_0_1/unit.cpp` ports all 12 of the Python
reference's own synthetic unit tests
(`tests/test_target_acquisition.py`, commit `6efc952`) plus its corrective-
investigation regression test, directly against `NaviTracerCore.h` (13
checks, all passing) — this is an independent correctness check of the C++
logic on its own terms, not a comparison against the Python output.

## Deviations from the Python reference, and why none changes recognition behavior

These are also documented at the point they occur in `NaviTracerCore.h`'s
own header comment:

1. **ΔBIC omitted** (NAVI_TRACER_0_1 authorization, item 2). Diagnostic
   only; never gates a recognition decision in the Python reference either.
2. **Fixed-capacity baseline history** (128 entries, ~2.56 s at the 20 ms
   cadence this recognizer assumes) in place of Python's unbounded list.
   Architecturally unreachable to overflow in normal operation: baseline
   establishment completes within ~33 samples (0.65 s / 8 bins) of
   collection starting, far under the 128-entry capacity.
3. **`double` throughout** the recognition core (matching `numpy.float64`),
   not `float` — chosen specifically to minimize floating-point divergence
   from the Python reference; the 50 Hz recognition update rate leaves
   ample ESP32 budget for this.
4. **Candidate's baseline-reseed tail** is a fixed 6-slot ring
   (`RTB_QUIET_BINS`) instead of slicing Python's full per-candidate sample
   list — a consequence of (1): since the full sample list is only needed
   for the dropped ΔBIC fit, it is not kept at all.

None of these change what `observe()` decides; the equivalence results above
are the direct evidence for that claim on the fixtures tested.

## What remains unverified (honest limitation, not resolved by this report)

- **No equivalence check against the real recorded Hall waveform exists.**
  The raw recording was not available in this environment. The 284.58 s
  anomaly and Trial 6 checks above are synthetic reconstructions of their
  *reported shape*, not replays of the recorded samples themselves.
- **No ESP32 cross-compilation was performed** (see the build report in
  `docs/NAVI_TRACER_0_1_FIELD_TEST_AND_ROLLBACK.md`) — `arduino-cli` could
  not be installed in this environment (egress policy denies
  `downloads.arduino.cc`). The identical `NaviTracerCore.h` was compiled and
  run on the host (`c++ -std=c++17`) for both the unit tests and the
  equivalence harness, with zero warnings under `-Wall -Wextra -Werror`, but
  this is not a substitute for an actual Xtensa/ESP32 toolchain build.
- **Real-hardware timing** (actual sample-period jitter, Hall ADC noise
  floor, the PWM ramp-down behavior around the `pwm == 0` gate — see
  decision 0023) is unverified by construction: this is a host-side,
  synthetic-data report.
