# NAVI Document B implementation and reconciliation — 2026-10-05

Scope: the first firmware cleanup pass only, explicitly authorized by David.
Repository: `ninobur/esp-loco-control`; branch: `codex/ewo-pwm-zero-localization`.
Starting/rollback commit: `b42e0d83ee98253ac72427198fd37953b11ac526`.
Configuration group: `41d07c5` (`Remove runtime pitch and calibration authority from EWO`).
The following commit contains the dead-code group and this completion report.
No flashing or live deployment was performed.

## Governing boundaries and inspection

Before editing, Codex verified and read [Document B](NAVI_VESTIGIAL_CODE_CLEANUP_ARCHITECTURE_20261005.md),
the [governing index](NAVI_EWO_GOVERNING_DOCUMENTS.md), [Document C](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md),
[0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md),
and [Document A](NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md).
Document B's introduction, `2d738d1ddec16c78be741fc71b6a02e3e5f2b072`, is on this lineage.

This applies the [NGR design principles](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md):
retain observations and genuine measurement protection, remove obsolete
authority, and keep interpretation in NAVI. No departure is introduced.
Station/position-overlay authority, adaptive braking, CTO and Circuit Express
were explicitly excluded by David's implementation instruction.

Inspection covered the integrated sketch and core, base EWO evidence/map
headers, shared Type-5 wire/measurement/detector code, current TX construction,
included hardware/operations/station headers, optical classifier, recorder,
console adapters, host tests and current dashboard/decoder consumers. Searches
included runtime configuration, lifecycle/epoch causes, compatibility,
experimental/developmental state, provisional comments, unused APIs/state,
telemetry and tests. Git history was used to distinguish introductions and
superseded behavior. A concise ledger was presented in the task before edits.
No unresolved operator choice blocked the classified removals.

## Reconciliation ledger

Paths below abbreviate the integrated candidate as `I`:
`firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED`.

| Candidate / prior role and provenance | Actual current trigger or consumer | Classification and disposition |
|---|---|---|
| `I/NaviIntegratedCore.h` pitch/calibration comparisons in `sameFrame`; inherited continuity work through `caf2280` / `d0185be` reset Hall evidence, speed, spatial relationship and epoch, and bypassed ordering | TX constructs `Measurement(movementBoot,0,9.652,true)` once; no setter or runtime calibration procedure exists | Historical/developmental. Removed both identity comparisons. Fixed pitch is validated; raw calibration metadata has no authority. |
| Core epoch identity and frame loss | Transmitter reboot, different received source MAC, sequence/capture-time disorder, completed-pulse rollback, local receive-time regression | Current and necessary. Retained all existing genuine continuity checks and redeclaration requirement. |
| Type-5 shape/CRC, nonzero boot, pitch, cumulative-distance product, pulse/rise consistency, supported reason/validation flag | Corrupt/malformed or incompatible input can actually arrive over ESP-NOW | Current and necessary. Retained checks, with fixed 9652-µm scale validation shared by ingress and core. No fallback for rejection. |
| `IrHealthFault::CalibrationFault` / `CALIBRATION_FAULT` in the shared classifier and core | Invalid metadata; no calibration process exists to fail or recover | Historical interpretation. Removed enum/string; malformed evidence uses existing `PacketInvalid`. Numeric slot 5 remains unused; later diagnostic numbers stay unchanged. |
| `IrOdometryEpoch`, `MmDistanceReference`, old `MovementEvidence` admission barriers and `IrHealthMonitor` | Older NAVI_IR/NAVI_COHERENCE sketches and reference tests; none is included by EWO | Historical prior art outside this production pass. Preserve source/history, remove the old epoch suite from EWO's runner, add current EWO tests and a scoped reference README notice. No supported runtime transition remains in EWO. |
| `IrMovementContract::between`, `ErrorBudget`, calibrated bounded-distance rules | Other/reference measurement consumers; EWO does not call these APIs or construct a budget | Historical/non-executed in this candidate. Leave shared definitions for their other lineage; do not install a bounded-distance or calibration mechanism in EWO. |
| Constant core `storageReady()` and unused `initialCollectionStarted()`; remnants of the earlier startup collection/storage design (`bf7b487`) | No allocation can fail through constant `true`; no caller of the collection API remains | Historical/developmental. Remove APIs and the vacuous boot condition. Real recorder, queue and task allocation checks remain. |
| `Reanchored` and `RESERVED_REANCHOR_EVENT`; Hall-only re-anchor withdrawn in `d0185be` | No event producer | Historical/developmental. Remove production symbol/string; reserve numeric gap 14, preserving current event IDs 15/16. |
| Sketch `lastSeenMac` | Written for every received packet, never read; accepted source MAC is separately recorded by NAVI | Historical/dead diagnostic state. Remove variable and copy; retain actual source provenance. |
| Arduino 2.x `ledcSetup` / channel-based PWM branches | Installed/reviewed target is ESP32 core 3.3.12; the existing `esp_now_recv_info_t` callback already requires the 3.x API | Obsolete compatibility. Remove two conditional branches; current pin-based PWM calls remain byte-identical. No motor-control tuning. |
| TX comment saying wheel confirmation is still pending | Installed wheel and 9.652 mm/pulse are settled | Historical/developmental language. Correct comment only; no transmitter or optical behavior change. |
| Provisional Hall boot reference and spatial collection | First powered native Hall sample, then actual movement through the existing spatial interval | Current and necessary. “Provisional” describes a real current Hall reference, not calibration. Retain all Hall behavior. |
| Optical reason/readiness, raw gap/saturation/abort/inference counters, stationary-PWM warning | Current detector output and acquisition observations | Diagnostic only. Retain raw data; these do not gain epoch or operating authority. No stall detection introduced. |
| Freshness, queue loss, reconnect timers, bounded storage and overflow counters | Missing/delayed packets, Wi-Fi/MQTT outages, finite task queues and heap | Current and necessary. Retain. Cumulative reports can bridge packet loss; stale evidence cannot be applied. |
| `NaviCompatibility` NORMAL/UNSET, speed/null values, MQTT commands | Current `server/ngr_app_v1_11_2.py` dashboard/controller communicates over existing topics; interoperability tests exercise actual adapter output | Current compatibility. Preserve the current console contract and its output-only role. |
| NSR1 legacy median/accept/degraded slots, event numbers, versions and raw Type-5 calibration ID | Current recorder/receiver/decoder plus existing captures; Python tests decode actual C++ records | Current compatibility. Preserve layout, field meanings and current numbers. No new event is emitted in the retired slot. |
| Pairing/coupling commands, NVS pairing display, common-polarity macro explanation | Existing operator MQTT commands, dashboard coupling control, recorded MAC provenance, Otto's selected shared hardware profile | Current diagnostic/configuration support. Retain without turning pairing into source selection or the old polarity macro into navigation authority. |
| StationMachine, ARMED/phases/completedIdx_, MISSED/PHASE_TIMEOUT, pause/resume, overshoot/late entry, ZERO_RAMP, STOP geography and adaptive braking | Current station visits still execute the retained implementation | Explicitly deferred to Document C, even where its authority differs from the target architecture. No changes in this pass. |
| Legacy CTO2/coexistence; TX raw/fusion/peer packets | Known supported coexistence boundary in Document A | Current separate requirement. No protocol, packet or CTO/CE implementation change. |

## Epoch causes before and after

EWO has a numeric epoch and applicability state, not the reference package's
`IrEpochBreak` enum. The following describes the actual core, not that archive.

| Event | Starting behavior | Final behavior |
|---|---|---|
| First valid observation | Start epoch | Same |
| Received source MAC changes | New epoch; invalidate mapped relationship/speed/evidence | Same |
| Transmitter boot changes | New epoch; invalidate mapped relationship/speed/evidence | Same |
| Calibration ID changes | Treated as a new frame, allowing ordering bypass and epoch/relationship reset | Raw metadata only; all normal ordering checks still apply |
| Pitch changes, even internally self-consistent | Treated as a new frame and operational continuity transition | Reject nonconfigured pitch as invalid evidence; no pitch transition |
| Same-stream sequence/capture time does not increase, completed pulses roll back, or local receive time regresses | End current epoch/applicability; next admissible report starts a new epoch; mapped coordinate stays invalid until declaration | Same |
| Invalid scale/boot delivered directly to core | End applicability/epoch; generic invalid-frame handling | Same protection, reported as `PacketInvalid`; includes pitch not equal to installed configuration |
| Malformed frame rejected by production ingress | Count invalid packet; do not feed it to NAVI or refresh freshness | Same, now also covers nonconfigured pitch |
| Staleness or queue loss | Suspend stale applicability/reset affected Hall window; continuous cumulative counter can bridge gap | Same; neither invents a reset |
| Optical diagnostics, ordinary zero-movement dwell, declaration, reversal | No measurement epoch change solely for these events | Same |

Named `CalibrationChanged`/`PitchChanged` still appear in explicitly historical
reference material. They are absent from the compiled EWO dependency graph.
Other archived epoch causes (health/readiness, diagnostic-counter changes,
transport gaps and identity exhaustion) were not imported into EWO.

## Changes, tests and telemetry

Production/header changes remove **35 lines and add 26** across five files
(net nine fewer). The purpose is removal of authority, not maximum deletion.
Removed mechanisms are the two configuration identity comparisons, their
associated runtime reset/ordering-exemption behavior, calibration-fault state
and string, two dead startup APIs, un-emitted re-anchor event/string, write-only
MAC state/copy, and both Arduino 2.x PWM branches. Existing real continuity
reset code is retained. There are no new fallbacks, timeouts, governors,
recovery modes, warning states or protocol variants.

No live JSON field was removed: `ir_calibration` is useful raw evidence,
`ir_epoch`/`ir_epoch_active` describe genuine continuity, and `ir_invalid`
counts rejected input. Malformed core data now reports diagnostic 3
(`PACKET_INVALID`) instead of 5. Raw Type-5 fields and NSR1 sizes are unchanged.

- Added `test_ir_configuration.cpp`: calibration metadata cannot reset epochs,
  clear speed or bypass order; fixed pitch/overflow/distance validity; genuine
  source/boot/order boundaries; diagnostics, freshness and cumulative gaps.
- Added a Python integration case that compiles the **actual** sketch ingress
  and CRC functions with queue/clock/recorder stubs. It exercises invalid
  magic/version/type/size/CRC, boot, pitch, distance-validation flag, reason,
  pulse/rise consistency, overflow and nominal distance; rejection cannot
  refresh the source or create an epoch. Metadata cannot bypass same-stream order.
- Revised navigation/speed fixtures to use installed-wheel pulses instead of
  1-µm/1-mm pitches. Existing navigation outcomes remain protected at physically
  representable positions on either side of bounds. The mixed PWM-zero pitch
  case now exercises invalid configuration, not a valid reconfiguration.
- Removed the historical epoch executable from EWO's runner; its archived
  tests remain available independently. Ported relevant current continuity and
  classifier coverage into EWO's test. Updated the shared classifier's existing
  malformed-boot assertion from calibration fault to packet invalid.
- Recorder assertions protect current NSR1 event IDs and raw calibration-ID
  retention. Station/adaptive-braking tests are unchanged.

## Validation

The starting EWO runner passed before editing. The configuration group passed
the complete host runner and Python suite before commit. The dead-code group
then passed the final complete checks:

```sh
sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all \
  --build-path /private/tmp/ewo-vestigial-cleanup-esp32-20261005 \
  --build-property 'compiler.cpp.extra_flags=-I/Users/davidbrown/esp-loco-control -Werror=format' \
  firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED
git diff --check
```

- **PASS:** 13 host executables (12 ASan/UBSan, plus base EWO regression),
  including e-stop, operations, station and adaptive-braking checks.
- **PASS:** 11 Python tests, including actual ingress, console/dashboard and
  NSR1 interoperability, command admission and PWM-zero withdrawal.
- **PASS:** standalone historical architecture test as a shared-classifier
  compatibility check, not an EWO architectural requirement.
- **PASS:** ESP32 compilation, installed core **3.3.12**, selected **Otto
  9950011** profile. Flash **1,018,187 / 1,310,720 bytes (77%)**; static RAM
  **57,964 / 327,680 bytes (17%)**. Three deprecation warnings originate in
  installed `Adafruit_INA219.cpp`; none in candidate code.
- Actual compiler dependency file includes `IrInstrument.h` and the Otto
  profile, and excludes `IrOdometryEpoch.h`, `MmDistanceReference.h`,
  `IrHealthMonitor.h`, `MovementEvidence.h` and `ProximalRecovery.h`.
- Boundary verification: 13 protected implementation/test/governing files
  are byte-identical to the starting commit. Station, battery, ramp, command,
  PWM-zero hold, Hall acquisition and receive-callback function bodies are
  unchanged. TX executable text is unchanged after stripping comments.

## Files changed

- `docs/NAVI_EWO_GOVERNING_DOCUMENTS.md`
- `docs/NAVI_VESTIGIAL_CODE_CLEANUP_IMPLEMENTATION_20261005.md` (this ledger/report)
- `docs/decisions/0122-type5-pitch-is-configuration-and-calibration-metadata-has-no-runtime-authority.md`
- `firmware/common/IrMovementWire.h`
- `firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`
- `I/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino`
- `I/NaviIntegratedCore.h`
- `I/README.md`
- `I/run_tests.sh`
- `I/tests/emit_telemetry_contract.cpp`
- `I/tests/test_first_target_after_declare.cpp`
- `I/tests/test_integrated_core.cpp`
- `I/tests/test_ir_authority.cpp`
- `I/tests/test_ir_configuration.cpp`
- `I/tests/test_pwm_zero_localization.cpp`
- `I/tests/test_recorder.cpp`
- `I/tests/test_review_regressions.cpp`
- `firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/IrInstrument.h`
- `firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/README.md`
- `firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/tests/test_ir_architecture.cpp`
- `tools/tests/test_ewo_integration.py`

## Retained limitations and deferred work

No unresolved Document B removal requires an operator decision. Historical
non-EWO engines are deliberately retained as history; their presence does not
authorize reintroduction. Existing source admission remains unchanged: pairing
is display-only, so a second valid sender can disrupt continuity. Selecting a
new source policy would be new architecture and was not attempted.

Document C's station/position-overlay cleanup remains wholly deferred,
including every station mechanism listed above. Adaptive braking, CTO/CE,
dispatcher overlays, Hall/IR detection and motor performance were not redesigned.
All previous decision bodies and Documents A/B/C are preserved. Decision 0122
records this narrow implementation and its cost: different installed wheel
geometry would require an explicit configuration change. Host/build validation
is not field acceptance. Work stops after commit and push; no second pass begins.
