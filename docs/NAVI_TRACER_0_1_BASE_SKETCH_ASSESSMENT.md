# NAVI_TRACER_0_1 — base-sketch assessment

**Status:** field-test candidate for review. Not field accepted, not flashed.
**Branch:** `feature/navi-tracer-0-1`.
**Scope:** this document records the comparison and decision behind
`firmware/test-programs/NAVI_TRACER_0_1/`, per the implementation
assignment's §2 and deliverable C.

## Candidates inspected

1. `codex/navi-rvm-manual-1-0-20261018` — `firmware/test-programs/NAVI_RVM_MANUAL_1_0/`
2. `dev/navi-rvm-shape-manual-20261018` — `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/`
   and `firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL/`

## Decision: `NAVI_RVM_SHAPE_MANUAL` (dev branch)

**`codex/navi-rvm-manual-1-0-20261018` was eliminated first, on direct inspection:**
`NAVI_RVM_MANUAL_1_0/` contains four headers (`FixedReference.h`,
`ManualControl.h`, `ReferenceTypes.h`, `WaveformObserver.h`) and no `.ino` at
all — it is not a complete, flashable sketch.

**Decisive evidence for `NAVI_RVM_SHAPE_MANUAL` over `NAVI_RVM_CONTINUITY_MANUAL`:**
the validated recognizer's own module docstring
(`research/navi_target_acquisition_20261021/target_acquisition.py`, commit
`6efc952`) names its polarity convention authority as *"Otto convention
(RvmNavigator.h, commit `3a8953c`)."* Checked directly:

- commit `3a8953c` is an ancestor of `dev/navi-rvm-shape-manual-20261018`
  and **not** of `codex/navi-rvm-manual-1-0-20261018`.
- `RvmNavigator.h` does not exist on the codex branch at all.
- `3a8953c`'s own commit message is a `NAVI_RVM_SHAPE_MANUAL`-specific field
  fix (STOP/GO Hall-queue overflow), reporting real ESP32 flash/RAM numbers
  (982,499 B flash / 121,292 B static RAM) and 864/864 field STOP/GO parity
  cases — i.e. an actually field-exercised build, not a design sketch.
- `NAVI_RVM_SHAPE_MANUAL`'s own commit history (6 commits: field-test
  candidate → STOP/GO continuity → an **operator ruling** on the stopped-PWM
  threshold → pattern-based resumption → an arrival-mirror fix marked
  **"(approved)"**) shows sustained field iteration with explicit operator
  sign-off. `NAVI_RVM_CONTINUITY_MANUAL` has a single commit ("unified
  Hall-only waveform continuity") with no comparable record.

`route_map.py` in the validated research commit states it is "copied
verbatim" from `RouteMap.h`, commit `3a8953c` — confirmed byte-for-byte
identical against this sketch's `RouteMap.h` (both the 171-entry
`ROUTE_POLARITY` and `ROUTE_SPACING_MM` arrays), so the map carries forward
with zero adaptation and zero risk of drift between the research and the
firmware.

## Hardware inspection (against the assignment's §2 checklist)

| Mechanism | Finding | Disposition |
|---|---|---|
| ESP32 board / pins | `esp32:esp32:esp32`. `MOTOR_PWM_PIN=4`, `MOTOR_DIR_PIN=16` (Otto's board was physically rewired from the fleet-common pin 2 — documented in `LL_LocoConfig_9950011.h` with the commit that fixed it), `HALL_PIN=33`, I2C 21/22 (INA219). | Retained unchanged. |
| Hall sampling | 5 kHz native `analogRead`, 5 reads per 1 ms FreeRTOS tick (`vTaskDelayUntil`), pinned to core 0, priority 3 (above recognition). 12-bit ADC. | Retained unchanged. |
| Hall → 20 ms binning | `RvmTracing.h`: plain mean of every native read in `[20k ms, 20(k+1) ms)` of the boot clock; NaN if a block holds fewer than half its nominal 100 reads or straddles a serial gap. This is exactly the 20-ms-bin-mean granularity `target_acquisition.py`'s own thresholds (70 ADC / 2 bins, etc.) are stated against. | Retained unchanged — no adaptation needed. |
| PWM / motor driver | `ledcWrite`/`ledcAttach`, 20 kHz / 8-bit, ramped one count per `rampUpMs`/`rampDownMs` in `serviceRamp()`. Every motor write funnels through `writePwm()`. | Retained unchanged. Verified (grep, this assessment's companion equivalence doc) that `writePwm`/`requestPwm` are called only from `serviceRamp()`, `handleCommand()`, `serviceBattery()`, `setup()`/`haltStartup()` — never from the recognition path. |
| Manual throttle/direction | MQTT commands via `RvmOps.h` admissibility checks (pure functions). | Retained unchanged. |
| Wireless/telemetry | WiFi+MQTT (PubSubClient) for commands/state; separate UDP "NAVI sync" stream (`NaviSyncRecorder.h`, `NaviWordStorage.h`) for high-rate Hall/action logging to a Pi. | Retained unchanged — confirmed (grep) neither file references any recognizer type. |
| Watchdog/safety | `OrderedEstop` (order-preserving E-STOP latch across async MQTT callback vs. main loop), low-voltage controlled stop, `haltStartup()`. | Retained unchanged. |
| Startup/shutdown | PWM forced to 0 and pin held LOW before anything else runs. | Retained unchanged. |

## Recognition engine: replaced completely

`RvmShape.h` / `RvmShapeModel.h` / `RvmNavigator.h` implement **"Model A"**:
a pre-trained Gaussian-mixture log-likelihood-ratio shape classifier, scored
across 40+ window widths against a continuously-extending least-squares
baseline. This is architecturally unrelated to the validated
`target_acquisition.py` state machine (fixed 70-ADC/2-bin departure, frozen
baseline, duration floor, 6-bin return-to-baseline) and is exactly the kind
of "old NAVI recognition decision" the assignment instructs not to carry
forward. It, and `RvmPause.h` (the STOP/GO waveform hold-back machinery
built specifically to keep that statistical recognizer's window-indexed
blocks in exact sequence), are deleted from this sketch, not adapted.

`NaviTracerCore.h` (new) is a direct, documented, line-by-line port of
`target_acquisition.py` — see its own header comment for every deliberate
difference from the Python reference, and
`docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md` for the verification that
the port is behaviorally faithful.

## Approved decisions carried into this implementation

(NAVI_TRACER_0_1 authorization, 2026-10-22; see the commit this document
ships with.)

1. **Stop/resume:** the validated `pwm == 0` gate is ported literally. No
   `WaveformPause`-style block buffering, no new stopped-PWM threshold.
   Telemetry (`diag/ramp_watch` MQTT topic, `ramp_watch_blocks` counter)
   distinctly identifies every block with `0 < PWM < 15`, an acknowledged,
   not-yet-resolved field-test uncertainty.
2. **ΔBIC:** omitted from this port. Diagnostic only per the research
   `REPORT.md`; not part of any recognition decision.
3. **Acquisition duration:** the independently validated **duration-floor**
   configuration (`0.02 s`, i.e. no separate gate beyond the 2-consecutive-
   bin departure criterion) is the default (`navi_tracer::DURATION_FLOOR_S`).
   The original 110 ms figure remains available by defining
   `NAVI_TRACER_USE_LEGACY_110MS_DURATION` at build time; it does not alter
   the default code path.
4. Hardware/manual-control/telemetry/safety infrastructure retained as
   inspected above; recognition logic replaced completely.
