# NAVI_TRACER_0_1 — build report, field-test procedure, rollback

**Status:** field-test candidate for review. **Not field accepted. Not
flashed. Flashing Otto requires separate explicit operator authorization**
per `AGENTS.md` §2 — this document does not grant it.

## A. Build report

| Item | Value |
|---|---|
| Board target | `esp32:esp32:esp32` (same FQBN as `NAVI_RVM_SHAPE_MANUAL`) |
| Toolchain | `arduino-cli compile --fqbn esp32:esp32:esp32` (`tools/navi_tracer_0_1/build.sh`, mirrors `tools/navi_rvm_shape/build.sh` exactly) |
| Arduino libraries | `WiFi`, `WiFiUdp`, `Wire`, `esp_timer` (bundled with the esp32 core); `PubSubClient`; `Adafruit_INA219` |
| Build command | `tools/navi_tracer_0_1/build.sh --credentials-dir DIR` |

**Result: NOT YET RUN.** The environment this port was implemented and
tested in could not install `arduino-cli` — its egress policy denies
`downloads.arduino.cc` (confirmed: `curl` to that host returns a proxy
policy rejection, not a transient failure). This is an environment
limitation of the implementing session, not a property of the firmware.

**What was verified instead, in this environment:**
- The complete recognition core (`NaviTracerCore.h`) compiles cleanly on
  the host with `c++ -std=c++17 -O2 -Wall -Wextra -Werror` — zero warnings,
  zero errors — both standalone and as included by the unit-test and
  equivalence-test binaries (`tests/navi_tracer_0_1/`).
- The `.ino` itself was **not** compiled in this environment (it requires
  Arduino core headers and the esp32 toolchain), so syntax or API-usage
  errors specific to the Arduino/ESP32 glue code (as opposed to
  `NaviTracerCore.h`) are **not** ruled out by anything in this report. The
  glue code (hardware/MQTT/telemetry handling, task setup) is unchanged
  from `NAVI_RVM_SHAPE_MANUAL` except for the recognizer-specific sections
  documented in `docs/NAVI_TRACER_0_1_BASE_SKETCH_ASSESSMENT.md`, which
  itself compiled and ran correctly on real Otto hardware.

**Required before flashing:** run `tools/navi_tracer_0_1/build.sh` on a
machine with `arduino-cli` and the `esp32` board core + `PubSubClient` +
`Adafruit_INA219` libraries installed (the same setup `NAVI_RVM_SHAPE_MANUAL`
already builds on), resolve any compile errors, and update this report with
the actual result before proceeding to flash.

## B. Behavioral equivalence — see `docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md`

Summary: 14/14 synthetic Python-vs-C++ equivalence runs pass (7 fixtures ×
2 duration configurations), plus 13/13 unit tests ported directly from the
Python reference's own test suite. No real recorded Hall data was available
to test against in this environment — see that report's limitations section.

## C. Recognition cannot reach the motor (verified)

Grep-confirmed in this sketch: `writePwm()`/`requestPwm()`/`ledcWrite()` are
called only from `serviceRamp()` (operator ramps, E-STOP clamp, low-voltage
stop), `handleCommand()` (operator throttle/stop commands), `serviceBattery()`
(low-voltage stop), and `setup()`/`haltStartup()`. `NaviTracerCore.h` itself
has zero references to any Arduino, motor, or hardware API — it is a pure
state machine over `(t, hall, pwm)` triples that only ever *reads* `pwm`.
The recognition event path (`feedRecognizer`, `applyControl`, `rvmTask`,
`serviceNavEvents`) never calls any motor-write function. This mirrors
`NAVI_RVM_SHAPE_MANUAL`'s own structure exactly.

## D. Field-test procedure (do not execute without separate flashing authorization)

### Preparation
1. Confirm the firmware builds successfully (§A, pending).
2. Preserve the currently-running firmware image for rollback (§F).
3. Flash Otto with `NAVI_TRACER_0_1`.
4. Verify manual throttle operation (throttle, direction, brake, stop, estop)
   exactly as on `NAVI_RVM_SHAPE_MANUAL` — no behavior here has changed.
5. Verify Hall telemetry: `ngr/loco/9950011/state/nav` should report
   `"baseline_established":0` while stationary pre-declaration, and
   `"declared":0` / `state":"UNSET"` before the first `start_mm`/
   `start_interval` command.
6. Establish a known starting geographic position: send `start_mm` or
   `start_interval` with `session_direction` already set, exactly as before.
   Confirm `ngr/loco/9950011/state/nav` reports `"declared":1` and the
   expected `mm`/`target`.

### Initial run
Operate Otto forward at PWM 90 (the recording's own operating condition) over
a short, familiar section of track with several known magnets. Watch
`diag/tracer_event` (MQTT) / the `[TRACER]` Serial log for `OPEN_CANDIDATE`,
`ACQUIRED`, `CLOSE_AND_ADVANCE` events and compare each `CLOSE_AND_ADVANCE`
against the physical magnet just passed.

### Extended run
If the short run matches physical reality, continue over a longer section
including curves, straight track, and varied magnet spacing.

### Stop and restart
Stop Otto manually (PWM to 0), wait, then resume. Watch specifically:
- `diag/ramp_watch` events (every `0 < PWM < 15` transition) — this is the
  acknowledged, open field-test question (decision 0023, item 1): does
  Otto's real ramp-down time at this threshold band perturb the recognizer
  in practice? There is no code change to evaluate here — only telemetry.
- Whether the candidate open at the moment of the stop (if any) survives the
  stall and is recognized correctly on resumption (mirrors
  `test_stop_mid_candidate_preserves_evidence` and the equivalence suite's
  `stop_restart_mid_candidate` fixture).

### Evaluation
Per the assignment: do not declare success merely because the magnet counter
increases. Confirm, from the physical observations and the Hall telemetry
together:
- Each physical magnet produces exactly one recognized encounter.
- Recognition (`ACQUIRED`) occurs early enough to be useful, well before
  `CLOSE_AND_ADVANCE`.
- Distinct encounters remain distinct; no extra encounters are invented.
- Geographic progression remains sequential, one marker at a time.
- `baseline_level`/`baseline_slope`/`departure` in telemetry stay physically
  plausible throughout (no implausible jumps, no silent baseline drift onto
  a magnet).

## E. Telemetry reference

| Topic / field | Content |
|---|---|
| `diag/tracer_event` (MQTT), `[TRACER] ...` (Serial) | One row per recognizer event: kind, timestamp, target_mm, sign, matches_target, start_s, acquired_s, duration_s, peak_departure. |
| `diag/ramp_watch` (MQTT), `[TRACER] RAMP_WATCH ...` (Serial) | Transition into/out of the `0 < PWM < 15` band — the acknowledged stop/resume uncertainty, not a recognition decision. |
| `state/nav` (MQTT, retained) | Current state, mm/target, candidate/baseline status, counts (recognized/advanced/contradictions/disposed), `ramp_watch_blocks`. |
| `state/loopstat` (MQTT) | Block counts, queue drops, `rvm_push_max_us` (recognizer CPU time per block — expected far lower than `NAVI_RVM_SHAPE_MANUAL`'s Model A, since the ported recognizer is O(1) per sample vs. a multi-scale grid search), heap. |
| NAVI sync UDP stream (`NaviSyncRecorder.h`, unchanged) | Full-rate native 5 kHz Hall samples and 20 ms blocks to the Pi — the full-waveform evidence needed to reconstruct and compare physical Hall curves against NAVI's decisions after the run, per the assignment's §8. |

## F. Rollback procedure

1. The currently-running, field-accepted firmware image on Otto (whatever
   predates this field test — confirm its identity and keep a copy of its
   compiled binary or its exact source commit before flashing
   `NAVI_TRACER_0_1`) is the rollback target.
2. To roll back: reflash that preserved image via the same `arduino-cli
   upload` / USB procedure used to flash `NAVI_TRACER_0_1`.
3. `NAVI_TRACER_0_1` itself makes no persistent change to Otto (no NVS
   writes, no EEPROM/flash-partition changes beyond the sketch binary
   itself) — rollback is a plain reflash, nothing to undo on the Pi or in
   MQTT retained state beyond what a normal reboot already clears.
4. This branch (`feature/navi-tracer-0-1`) is not merged into any
   production branch and does not touch `firmware/QUORUM/QUORUM.ino` or any
   other production-control sketch.
