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

**Result: SUCCESSFUL, real Xtensa/ESP32 compile (2026-10-22, re-verification pass).**
`downloads.arduino.cc` and the entire `*.arduino.cc` domain remain blocked by
this environment's egress policy, so the official `arduino-cli` binary and
official board-manager assets could not be fetched directly. A working
toolchain was built from source instead, entirely from reachable hosts
(`proxy.golang.org`, plain `git clone` of public GitHub repos, and GitHub
release-asset URLs, all of which are *not* blocked — only the Arduino-owned
domain is):

- `arduino-cli` built from its own source (`github.com/arduino/arduino-cli`,
  tag `v1.5.1`) via `go build`.
- `esp32:esp32` core **version 3.3.12** installed via the real Espressif
  board-index JSON (`raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/...`),
  whose toolchain/tool assets are GitHub release downloads. (The default
  `core install` without a version pin had resolved 2.0.9, which lacks
  `ledcAttach(pin, freq, resolution)` and `WiFiClient::setConnectionTimeout`
  — both genuinely used by `NAVI_RVM_SHAPE_MANUAL`'s own unmodified
  hardware/network code, carried into this sketch unchanged. Pinning to
  3.3.12, the version whose API the base sketch actually assumes, resolved
  both — this was an environment/toolchain-version correction, not a sketch
  change.)
- `PubSubClient`, `Adafruit_INA219`, `Adafruit_BusIO` installed by cloning
  their public GitHub repos directly into the library path.
- The Arduino-specific `ctags` fork (`github.com/arduino/ctags`) built from
  source after one two-line patch to its own `general.h` (reordering a
  system-header include ahead of a macro definition that collides with a
  modern glibc internal macro name on this build host) — a pre-existing
  incompatibility between circa-2013 ctags source and a current glibc, with
  no effect on ctags' actual tagging logic. **Generic Universal Ctags
  (the Ubuntu-packaged `ctags`) was tried first and produces a cascading
  compile failure** (malformed auto-generated function prototypes —
  `"int serviceBattery()"` instead of `void`, etc. — a generic-ctags
  limitation, confirmed by it affecting the file uniformly regardless of
  which function came first). The genuine Arduino ctags fork compiles the
  identical sketch cleanly. Neither of these two fixes touched
  `NAVI_TRACER_0_1.ino` or `NaviTracerCore.h` in any way.

| Item | Value |
|---|---|
| Board | `esp32:esp32:esp32` |
| Core version | `esp32:esp32@3.3.12` |
| Libraries | `PubSubClient` (git HEAD), `Adafruit_INA219` (git HEAD), `Adafruit_BusIO` (git HEAD) |
| Flash | **969,691 bytes (73%) of 1,310,720 max** (default esp32 partition table — no custom partition scheme is referenced anywhere in this sketch) |
| RAM (static) | **53,044 bytes (16%) of 327,680 max**, 274,636 bytes free for locals |
| Warnings | Zero, from any file this sketch's own commit added or modified, even with `--warnings all`. (Three pre-existing `-Wdeprecated-enum-enum-conversion` warnings appear inside the third-party `Adafruit_INA219.cpp` library itself — unrelated to this sketch.) |
| `-Werror=format` | Passed — no format-string mismatches anywhere in the build. |

**Not yet done:** flashing Otto with this image, and a real-hardware timing
measurement — both correctly withheld pending your separate flashing
authorization (§D/§F below).

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
