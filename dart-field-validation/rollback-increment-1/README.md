# NAVI EWO DART Experiment 1

Experimental increment authorized October 14, 2026. **Not field ready; not flashed.**

Foundation: `NAVI_EWO_0_1_MM045_STOP` at `85a2cf2a16d262abdf7258c60f60508b823913ee`. This is a separate sketch. The original EWO program, COHERENCE sources, maps, calibration and shared dependencies are unchanged.

The navigator preserves median-of-five tick 70×2 openings, qualifies target polarity, retains the closest candidate, and commits once on the agreed symmetric boundary or better post-target observation. Exact opposite-side ties use an explicitly inferred midpoint. The historical distance gate and automatic distance-only missed-marker advances are absent. Opening coordinates use the newest applicable Type-5 snapshot received by the opening time; the evaluated coordinate is saved unchanged as the selected anchor. Spatial reference collection uses the same causal association without suppressing detection.

IR interval/cumulative residuals and a 16-record directed-interval timing ring are diagnostic only. No IR withdrawal/restoration, timing selection, morphology veto, PWM distance estimate or new missing-target stop exists. Missing candidates/coordinates leave the last selected identity unchanged; this does **not** necessarily stop the locomotive. Existing motor commands and overlay execution remain active. Do not operate this build until the unresolved stopping policy is approved.

`StopView` now exposes `identityAvailable`, `identifiedMm`, `identifiedDirection` and `tile` independently of `fine`. Existing `available`, fine lookup and `NaviStopOverlay::command()` behavior are unchanged. Identity-only throttle instructions are not activated.

## Validation

From the repository root:

```sh
sh firmware/programs/NAVI_EWO_0_1_DART_EXPERIMENT_1/run_tests.sh
python3 dart-validation/run_inherited.py
python3 dart-validation/check_boundaries.py
python3 dart-validation/test_transport.py
```

The active runner executes the new production-code scenarios/invariants and five unchanged applicable suites. Five original detector/window suites remain byte-for-byte in `tests/`; they fail against the experiment because their setup/assertions specify replaced behavior. Their baseline and experimental outcomes are preserved in `dart-validation/inherited-results.json`; they are not reported as passing. See `dart-validation/REPORT.md` for interpretation and replay limits.

## Recording

Existing telemetry and NSR layouts remain intact. New unretained MQTT suffixes are `diag/navi_decision` and `diag/navi_status`. Decision records use a separate eight-item compact transport queue, serviced after existing network work. Diagnostic loss increments counters and never feeds navigation state. Native acquisition loss is different: an incomplete comparison cannot justify a selection; no new automatic stop follows.

Decision reason numbers: 0 opening, 1 context-inapplicable, 2 wrong target polarity, 3 coordinate unavailable, 4 candidate, 5 replacement, 6 eliminated, 7 tie, 8 immediate selection, 9 boundary selection, 10 incomplete, 11 anchor mismatch. Records link opening serial/time, reference, local receiver and source capture times, IR boot/epoch/sequence/pulses, selected anchor and partner ID. `residual_valid=false` means the zero-valued slot is not a measured agreement. `interval_valid` means an uninterrupted observed context with unique endpoints; it does not certify physical identity or authorize training/navigation. Departure time is diagnostic and may be absent.

Finite evidence/transport buffers expose drops. Lossless field recording and actual Pi receipt have not been established. A successful local publish is not proof of recording.

## Build

ESP32 core **3.3.12**, board `esp32:esp32:esp32`. Verification uses the repository credential template in `/tmp/navi-dart-build-config/credentials.h`, not operating credentials. These binaries are not deployment images.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all \
  --build-property compiler.cpp.extra_flags=-I/tmp/navi-dart-build-config \
  --build-path /tmp/navi-dart-esp32-build \
  firmware/programs/NAVI_EWO_0_1_DART_EXPERIMENT_1
```

Retain preset PWM, grade settings, MM045 +150 mm geometry, final ramp, dwell/restart, manual controls and protections. Hall-opening coordinates are not visible disc centers. No stopping offset correction is implemented.
