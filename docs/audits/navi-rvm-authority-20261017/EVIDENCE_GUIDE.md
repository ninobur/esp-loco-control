# Evidence guide and coverage inventory

Audited immutable candidate: `d18fe67e52deda91720be4384cd77c8f53c62cf0`. The audit preserves both unfavorable results and successful checks. Nothing here asserts that a host reproduction occurred on railway hardware.

## Reproduce without modifying candidate code

From this repository checkout:

```sh
python3 docs/audits/navi-rvm-authority-20261017/evidence/reproduce.py
```

Requires Python 3 and a C++17 compiler named `c++`; the recorded run used Apple clang 21 on arm64 macOS. The script compiles unchanged candidate headers and existing tests into a temporary directory, writes result files only under this audit's `evidence`, and extracts three actual shell service functions verbatim into mocks for one actuator-request witness. It does not invoke the reference generator/calibration scripts, which would change candidate artifacts. It never opens a device, network controller or motor interface.

- [verification.json](evidence/verification.json): exact file hashes, compiler, fixture hashes, exit codes and all-column comparison with candidate committed replay results. Runtime numbers vary by host/run.
- [deterministic.txt](evidence/deterministic.txt): existing checks, including geographically separated declarations, token-by-token corruption test, stopped memory, E-STOP ordering, header admission/station checks. Passing test names are assertions by the existing test; their coverage limitations are assessed in the report.
- Six `*-decisions.csv` and `*-runtime.txt` pairs: all six replay decisions match candidate evidence; Grillers has no stall E-STOP. No fixture was retuned or edited.
- [probes.cpp](evidence/probes.cpp) and [authority-probes.txt](evidence/authority-probes.txt): six exact software witnesses. Exit 0 means all descriptions reproduced, not acceptable operation. CCW timing and CW station witness supply explicit state to unchanged headers; they do not simulate the locomotive's physical trajectory. The manual-stop witness executes source-extracted shell functions with output-recording mocks. Console witness uses the actual fixed console class with a representative overlength row.
- `*-compile.txt`: compilation diagnostics preserved, including a harmless unused extracted service warning in the witness harness. No claim of warning-free target rebuild is made.
- [data-and-build-source-check.json](evidence/data-and-build-source-check.json): generated-array bounds/counts and all 38 source hashes matched to the committed build manifest. This links previously reported target compilation to candidate source bytes; it is not a new target compilation or timing measurement.
- [supplemental-provenance.json](evidence/supplemental-provenance.json): original local paths/hashes and tracked-commit verification for H120–H122; H6 explicitly untracked. The copied historical files remain byte-for-byte evidence. Their internal relative links refer to their original directories and were not rewritten or represented as links within this audit.

## Evidence strength

| Observation | Supported conclusion | Not established |
|---|---|---|
| Exact functions/branches and complete local call closure | Reachable software consequences and authority assignments | Frequency, physical occurrence or safety level |
| Six deterministic host witnesses | Described state/output is reproducible in unchanged functions | Real train stopped, moved, miscounted or failed a field station |
| Replay equality on six supplied fixtures | Audit reproduces candidate's recorded-data decisions, including known stall failure | Independent held-out-day generalization or full-controller/RTOS timing |
| Existing deterministic suite passes | Covered invariants hold under those stimuli | Stronger trusted-context invariant merely because trusted label stays unchanged |
| Generated data/hash inspection | Data-only layout, byte identity and previous target-build provenance | Actual target execution budget, fresh build or field acceptance |
| Historical authority copies | Exact documented historical intent and inheritance | Automatic supremacy over later RVM assignment or permission to implement |

## Per-file closure

Every active local header and sketch was inspected at its actual callers. Generated waveform cells were checked as structured data; they have no hidden control branches. Unselected Toby and example credentials are inventoried without attributing active behavior to them. External Arduino/ESP-IDF, MQTT, Wi-Fi/radio, Preferences and INA219 libraries stop the local closure; versions are recorded in the candidate build report. Their internals, live server settings and installed dispatcher firmware are not claimed audited.

| Candidate file and exact full extent | Runtime role / reachability | Ledger coverage |
|---|---|---|
| [IrInstrument.h:1–74](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/IrInstrument.h#L1-L74) | Active classification | B19 |
| [IrMovementContract.h:1–95](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/IrMovementContract.h#L1-L95) | Wire types/configured product check active; producer/range helpers inactive | B17/B67 |
| [IrMovementDetector.h:1–110](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/IrMovementDetector.h#L1-L110) | Transitively included optical-detector definition, not instantiated | B67 |
| [IrMovementWire.h:1–40](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/IrMovementWire.h#L1-L40) | Decode active; encode not called | B17/B67 |
| [LL_LocoConfig_9950011.h:1–264](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/LL_LocoConfig_9950011.h#L1-L264) | Selected Otto hardware/limits; obsolete constants distinguished | B32/B37/B70 |
| [LL_LocoConfig_9950012.h:1–183](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/LL_LocoConfig_9950012.h#L1-L183) | Unselected Toby profile; inventoried/hash checked, outside executed closure | B70 |
| [LocoConfig.h:1–7](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/LocoConfig.h#L1-L7) | Selects Otto profile | B70 |
| [NAVI_RVM.ino:1–1527](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NAVI_RVM.ino#L1-L1527) | All entry points/tasks/callbacks/actuator paths | B01–B62 |
| [NaviCompatibility.h:1–43](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviCompatibility.h#L1-L43) | Active UI/console serialization | B55 |
| [NaviCore.h:1–12](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviCore.h#L1-L12) | Compatibility target/type facade | B63 |
| [NaviDartEvidence.h:1–64](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviDartEvidence.h#L1-L64) | Active record/types; no DART selection engine | B64 |
| [NaviDartOperations.h:1–91](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviDartOperations.h#L1-L91) | Active stop IDs and timing structs; lap-bank class not instantiated | B65/B66 |
| [NaviDartTelemetry.h:1–85](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviDartTelemetry.h#L1-L85) | Formatting definitions; takeDartRecord always false | B69 |
| [NaviEstop.h:1–34](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviEstop.h#L1-L34) | Active ordered emergency state | B34/B35 |
| [NaviEvidence.h:1–40](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviEvidence.h#L1-L40) | Active Hall/target evidence types | B63 |
| [NaviIntegratedCore.h:1–226](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviIntegratedCore.h#L1-L226) | Active RVM adapter and all IR/state interfaces | B03–B25/B44/B47/B53 |
| [NaviIrCredibility.h:1–78](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviIrCredibility.h#L1-L78) | Evaluator definition inactive; types retained | B64 |
| [NaviMapAdapter.h:1–26](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviMapAdapter.h#L1-L26) | Active fixed-map target construction | B40/B63 |
| [NaviPulseObservation.h:1–183](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviPulseObservation.h#L1-L183) | Type-6/7 diagnostic state only | B58/B59 |
| [NaviPulseTelemetry.h:1–39](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviPulseTelemetry.h#L1-L39) | Active diagnostic serializers | B58/B71 |
| [NaviResourceDiagnostics.h:1–116](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviResourceDiagnostics.h#L1-L116) | Active console/resource accounting | B57/B59 |
| [NaviRunClock.h:1–61](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviRunClock.h#L1-L61) | Active time coordinate removing disabled-AUTO periods | B51 |
| [NaviStartupLap.h:1–218](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviStartupLap.h#L1-L218) | Active fixed CW timing data/compatibility helper | B46/B47 |
| [NaviStations.h:1–46](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviStations.h#L1-L46) | Active station selection and lock/exit logic | B42 |
| [NaviStopOverlay.h:1–217](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviStopOverlay.h#L1-L217) | Active stop coordinate/view and physical execution | B40/B43/B44/B49/B50 |
| [NaviSyncRecorder.h:1–589](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviSyncRecorder.h#L1-L589) | Active native acquisition rings and UDP archive transport | B54/B56/B59 |
| [NaviTileTiming.h:1–87](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviTileTiming.h#L1-L87) | Active fine-to-timed fallback and ramp | B45–B49 |
| [NaviWarnings.h:1–19](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviWarnings.h#L1-L19) | Active warning text | B55/B71 |
| [NaviWordStorage.h:1–62](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/NaviWordStorage.h#L1-L62) | Active static resource placement utility | B59/B71 |
| [Ops.h:1–197](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/Ops.h#L1-L197) | Active parsing/admission; StopArmingPolicy unused | B26–B31/B68 |
| [PulseEventEvidence.h:1–44](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/PulseEventEvidence.h#L1-L44) | Active wire decoder/types; producer not called | B58/B67 |
| [PulseTransportStatus.h:1–19](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/PulseTransportStatus.h#L1-L19) | Active diagnostic wire layout | B58/B71 |
| [RouteMap.h:1–201](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/RouteMap.h#L1-L201) | Active ordered spacings, marker conversion and cruise PWM; polarity metadata unused by recognition | B40/B41 |
| [RvmCalibration.h:1–7](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/RvmCalibration.h#L1-L7) | Active provisional appearance and quality thresholds | B11/B13 |
| [RvmRecognizer.h:1–194](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/RvmRecognizer.h#L1-L194) | Active continuous alignment, appearance, quality, stall | B05–B16 |
| [RvmReference.h:1–22272](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/RvmReference.h#L1-L22272) | Active immutable generated arrays; all rows structurally checked | B07 |
| [RvmTypes.h:1–22](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/RvmTypes.h#L1-L22) | Active model/block/token layouts | B71 |
| [credentials.example.h:1–7](https://github.com/ninobur/esp-loco-control/blob/d18fe67e52deda91720be4384cd77c8f53c62cf0/firmware/test-programs/NAVI_RVM/credentials.example.h#L1-L7) | Build placeholder only; actual credentials external and not copied | B39/B70 |

The candidate `README.md`, complete governing documents, build/reconciliation/field-procedure claims, reference/calibration/replay tools, fixture metadata, source manifests and relevant imported timing/controller evidence were also inspected. The source-generation tools are **offline preparation authorities**: their output influences navigation, but they do not run aboard the locomotive. The active firmware catalog correctly marks this sketch a development/field-test candidate with recorded stall failure, no flash and no production promotion (`firmware/README.md:63`).

## Earlier verification obligations, reconciled with the later assignment

M1's broad ten-item replay agenda is narrower in X's authorized first candidate: focused checks, not exhaustive simulation; no automatic learning/model management. This does not convert untested items into successes.

| M1 verification item | Evidence status at audited candidate |
|---|---|
| 1. Complete held-out laps / all-alternating conceptual map | Focused recorded excerpts and synthetic progression only; no independent complete held-out-day proof. No polarity gate present. |
| 2. 050–060 → atypical061 → normal062, then several anomalies | Synthetic weak-MM and corruption checks; trusted label checked, full trusted-phase invariant unresolved F06. |
| 3. Multilobe/flat leading/trailing one-per-ensemble | Focused reference-source multilobe replay and synthetic flat/corruption checks; general physical correspondence unproven. |
| 4. Genuine BL versus flat impostors | Prior uncertainty preserved; no live hard extent threshold. Reference seam annotation remains provisional. |
| 5. Missing ensemble/gaps/one-MM slip identifiability | Gap handling exists; generalized missing-MM identity/defensible-stop behavior unresolved F01. |
| 6. IR under/overcount/missing beats/immediate recovery | Own-integrity recovery test passes; full-interval and rejected-predecessor problems F08/F10. |
| 7. Commanded stall, 13-second zero-PWM episode, 60-minute pause | Synthetic stall and kernel pause pass; recorded Grillers stall fails. Full controller replay of zero-PWM episode not established. |
| 8. Five collective incoherent pieces / false stops / distance to failure | Five scalar-quality failure test passes its own predicate; collective requirement not implemented F02; field false-stop rate/distance unknown. |
| 9. Station token timing versus opening | Recorded timing comparisons provided; old opening not ground truth, complete physical landing not tested F08/F13. |
| 10. Learning/drift/swap/resources | Learning/swap intentionally deferred; target build memory traceable; real CPU/queues/radio/actuator latency not measured here. |

## Audit scope verification

The candidate checkout remained on its original branch at the audited SHA with a clean working tree. The audit checkout began at that same SHA and adds only this documentation/evidence directory. No firmware, map, config, governing-source file, original test fixture/result, service or device was modified. The historical evidence copies are additions, not edits to those decisions. Host test source in this directory is audit evidence only and is not part of any firmware build.
