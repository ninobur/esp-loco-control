# NAVI_RVM build and test report

Date: 2026-10-18 UTC / October 17 Pacific. Branch: `codex/navi-rvm-field-candidate-20261017`.

**Result:** the Otto ESP32 sketch compiles. Deterministic software checks pass. Recorded progression excerpts agree with the legacy geographic sequences, but a recorded stationary case fails to trigger the candidate's stall E-STOP and one progression excerpt has commitments up to 0.982075 s before the old trigger. This is a review candidate with known behavioral deficiencies, not a successful navigation/field test. No firmware was flashed and no operational service or firmware path was modified.

## Build and reproduction

Sketch: [`firmware/test-programs/NAVI_RVM/NAVI_RVM.ino`](../firmware/test-programs/NAVI_RVM/NAVI_RVM.ino). All non-secret headers and fixed reference data are adjacent. Otto's active profile is 9950011, PWM GPIO 4, DIR GPIO 16, Hall GPIO 33. Inherited IR contract: 9,652 µm nominal spoke pitch and type-5 110-byte wire snapshot.

Executed from the repository root:

```sh
python3 tools/navi_rvm/build.py \
  --credentials-dir /private/tmp/navi-ewo-0-5-deployment-config \
  --build-path /private/tmp/navi-rvm-build
```

The exact underlying compile command was:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-path /private/tmp/navi-rvm-build \
  --build-property 'compiler.cpp.extra_flags=-I/private/tmp/navi-ewo-0-5-deployment-config -Werror=format' \
  firmware/test-programs/NAVI_RVM
```

`credentials.h` was a private external include. No credential values or deployable image are committed. A reviewer can execute `python3 tools/navi_rvm/build.py` to compile using the committed inert placeholder definitions. That command requires installed dependencies but no account, controller, network credentials, recording archive or locomotive. Neither build mode uploads anything.

| Build item | Result |
|---|---|
| Arduino CLI | 1.5.1, Homebrew |
| ESP32 Arduino core | 3.3.12 |
| Xtensa C++ compiler | crosstool-NG esp-14.2.0_20260121, GCC 14.2.0 |
| Libraries used | PubSubClient 2.8; Adafruit INA219 1.2.3; Adafruit BusIO 1.17.4; core WiFi/Wire/Preferences/ESP-NOW |
| Compile exit | 0 |
| Program storage | **1,120,603 / 1,310,720 bytes (85%)** |
| Static RAM | **72,660 / 327,680 bytes (22%)** |
| Reported remaining RAM before runtime allocations/stacks | **255,020 bytes** |
| Target global adapter | **16,752 bytes**, from the ESP32 ELF symbol `navi`; [symbol evidence](../tests/navi_rvm/results/target-symbol-sizes.txt) |
| Host object sizes | Recognizer 12,592 bytes; controller adapter 16,816 bytes (host ABI, not a target allocation measurement) |

[`build.json`](../tests/navi_rvm/results/build.json) records the executed absolute command, source SHA-256s, tool/library inventory and final binary/ELF hashes. [`compile-final.txt`](../tests/navi_rvm/results/compile-final.txt) is the compiler output. `compile-initial.txt` is explicitly an earlier build, not the final measurement.

The dynamic-programming update visits at most 256 cells and 13 predecessor choices per cell (3,328 candidate transitions); the usual 20 ms step uses at most five choices. Two static frontiers carry six floats per state. The fixed CW/CCW cell arrays occupy about 84 kB of read-only flash, plus annotations and speed tables. Update storage does not grow with lap count. Host replay means were 2.09–3.97 µs per block, maximum 23.958 µs in this run; see individual runtime files. **These are host measurements, not ESP32 deadlines.** No target CPU timing, heap endurance, radio coexistence or live queue-loss measurement is available. The inherited `diag/resource` instrumentation reports Hall observation duration, queue residence, loop latency, heap and stack watermarks for a future authorized test. The source controller already had resource issues under investigation; static RAM headroom alone does not close them.

## Reproducible tests and results

```sh
python3 tools/navi_rvm/check.py
```

This builds C++17 tests and replay with `-O2 -Wall -Wextra -pedantic`. No numpy or private archive is needed. It writes textual evidence and deliberately exits **1** for the recorded stall failure. The pure deterministic test executable exits 0. No test assertion or calibration threshold was relaxed after the evaluation result.

[`deterministic.txt`](../tests/navi_rvm/results/deterministic.txt) records:

- Four geographically separated declarations, ordinary progression and wrap; every MM and ordinal checked.
- Split/leading/trailing-lobe synthetic waveform; one token per ensemble. A flat reference plus movement priors creates no token.
- A powered 60-minute PWM-zero pause with varying Hall values preserves phase, offset and ordinal, and resumes the same sequence.
- Ten corrupted **20 ms blocks**, +800 ADC: each of six committed identities and source timestamps equals the clean run; no premature token. This is not a claim that every corruption placement is harmless, nor that these blocks are ten original 5 kHz samples.
- Weak MM004, then normal MM005: MM004 counts once, final quality 0.618336 exceeds 0.5, trusted context stays MM002; MM005 clears the mismatch streak without using MM004 as an anchor. Five successive weak new MMs stop once their five final assessments exist.
- An older queued E-STOP release cannot clear a newer assertion; malformed emergency command asserts stop. Stationary synthetic Hall plus valid stationary IR triggers the stall decision; missing IR alone does not.
- IR's own error-counter increment makes that packet ineligible; the next intact packet is immediately eligible. Actual-map adapter declarations choose the correct CW/CCW successor.
- Manual/AUTO command admission, direction lockout, unchanged four station IDs, final braking, five-second dwell, release, and manual command preservation. These are pure-function/controller-header checks, not a hardware-in-the-loop test of the full Arduino loop.

All recorded decisions and per-token timing comparisons are in [`results/`](../tests/navi_rvm/results/), with the machine-readable [`summary.json`](../tests/navi_rvm/results/summary.json). Negative timing means RVM preceded the legacy opening; it is **not automatically a physical false-positive label**.

| Excerpt | Declared state; source interval (boot-relative s) | Tokens / legacy labels | RVM minus legacy trigger | Interpretation |
|---|---|---|---|---|
| `calibration_cw` | MM124 CW; boot `15549e9385faa4d5`, 1840.8–1865.0 | 21 / 21 (125…145) | −0.000711…+0.100312 s | Calibration data, not validation. |
| `heldout_cw` | MM146 CW; boot `4d8083de79a0e299`, 38–69 | 10 / 10 (147…156) | **−0.982075…+0.015112 s** | Historical filename retained; already inspected during development, not an untouched holdout. Early MM148, 150, 154, 156 require waveform/physical review. |
| `evaluation_wrap_cw` | MM157 CW; second boot, 96.5–124.5 | 17 / 17 (158…170, 0…3) | +0.008890…+0.092270 s | Separate excerpt evaluated after threshold freeze. |
| `evaluation_ccw` | MM121 CCW; first boot, 3167.6–3197.5 | 12 / 12 (120…109) | −0.156938…+0.082865 s | Different pass from CCW reference; shared evening and CW-only calibration limit generalization. |
| `multilobe_cw` | MM156 CW; first boot, 1487–1506 | 7 / 7 (157…163) | −0.051539…+0.077074 s | Reference-source excerpt, useful regression but not independent validation. Declaration was corrected from a setup typo (146) to the preceding legacy state (156) before evaluation. |
| `grillers_stall` | Test setup MM063 CW; first boot, 400–407 | 0 / 0 | No comparator openings | **FAIL: no stall E-STOP.** Hall block means 1968.12–1973.82 ADC; PWM 90 throughout; recorded IR speed zero. This is a stationary-signal reproduction, not new operator-certified track truth. |

All emitted progression ordinals were checked, with no missing/extra legacy IDs in these short excerpts. All finalized recorded qualities were below the frozen atypical threshold. That does not prove a useful anomaly discriminator: no independent real-fault quality corpus was validated. Some excerpts end before the last MM's trailing quality assessment; the pending ordinal is retained, not counted twice or silently finalized with future data.

The stationary failure is preserved as a behavioral failure even though “zero advancements” is correct. The uninterrupted three-second stall condition can be restarted by Hall jitter or model-phase motion. No alternative stall algorithm or retuned limits were substituted after this observation. Completely missing ensembles are another unresolved failure mode: the code can remain on the expected identity and retain motor instructions without a defensible timeout/skip decision. These deficiencies preclude claiming unrestricted AUTO readiness.

## Reference, calibration and source provenance

The implementation base is `6038dc5b3ecf81a0c16f34928ac92271ba14e7f6`; the original worktrees and operational branches were untouched. The self-contained hardware/controller closure was read from local `navi-ewo-dart-experiment-1/firmware/programs/NAVI_EWO_0_6` and its quoted includes. That checkout's HEAD was `1529df5`, but EWO_0_6 files were locally untracked. **That commit does not identify those source bytes.**

[`source_provenance.json`](../tests/navi_rvm/source_provenance.json) identifies the actual source paths and hashes. [`baseline_control.ino.txt`](../tests/navi_rvm/baseline_control.ino.txt) and [`controller-integration.diff`](../tests/navi_rvm/controller-integration.diff) let a remote reviewer inspect motor/manual integration without the local checkout. Include paths were flattened; the integrated recognition class was replaced, the retired `NaviCore.h` was reduced to payload types, and the local profile selector now explicitly selects Otto. Legacy DART telemetry structures remain for schema compatibility; they do not perform recognition.

The recorded deployed-source manifest differs from the imported working snapshot in **five files**: `.ino`, `NaviIntegratedCore.h`, `NaviDartEvidence.h`, `NaviDartOperations.h`, and `NaviDartTelemetry.h`. See [`source_comparison.json`](../tests/navi_rvm/source_comparison.json) and [`baseline_deployed_source_manifest.json`](../tests/navi_rvm/baseline_deployed_source_manifest.json). Thus “preserved controller” means preserved against this disclosed working snapshot, not proven byte-identical behavior against the installed binary. The prior build's recorded binary SHA-256 was `e930f621bb488fab8f7169804725407e20550d5d3f51b33ba1088f1eb8b44243`; [`baseline_build.json`](../tests/navi_rvm/baseline_build.json) identifies it. Its existence is a rollback provenance lead, not a declaration of field acceptance.

The fixed model uses inherited October 15 `CW_L1` (1420.658776–1840.213501 s) and `CCW_L1` (2745.450837–3167.014139 s) templates on the 52,264 mm map. [`reference_provenance.json`](../tests/navi_rvm/reference_provenance.json) records template/map/speed hashes and pass metadata. One pass per direction is not a learned multi-lap statistical corridor. The fixed scale is `sqrt(1.5² + (2.5*slope)² + (0.02*deflection)²)`. Peaks/seams annotate the reference waveform only; they do not edit RouteMap. Local ADC baselines and PWM speed priors are fixed recording-derived data.

Calibration follows Decision 23's empirical direction, with narrower evidence than requested: 20 adjacent label pairs in CW lap 2 separate quiet evidence (maximum 0.158375) from appearance evidence (minimum 1.07384). Their midpoint, 0.616107, is frozen in `RvmCalibration.h`. Final new-MM calibration quality has maximum 0.413065; its upper envelope rounded to one decimal is 0.5. See [`calibration.json`](../tests/navi_rvm/results/calibration.json), scores, labels and full-quality rows. This contrast is not a probability or a promised false-advancement rate. The corridor and other numerical engineering constants were not established on held-out days. No automatic adaptation is present.

Raw source provenance is retained in [`recording_source_manifest.json`](../tests/navi_rvm/recording_source_manifest.json): original Pi NSR/MQTT/optical paths, byte ranges and SHA-256s. The large raw archive is not copied. Each committed fixture JSON identifies its native excerpt byte range/hash and separate legacy scoring labels. Its CSV contains causal 20 ms native Hall means with original serial/time bounds. Replay consumes only past/current input; reference preparation may smooth archived training data offline. Fixture IR speed uses preceding received snapshots and a backward window, but only freshness is available in that derived table, not the runtime's full error counters. Fixture blocks use the last PWM/direction per 20 ms bin; live firmware additionally splits on setting changes. These are focused algorithm replays, not bit-for-bit reproduction of every acquisition/network boundary.

## Review and field limits

Review the [reconciliation](NAVI_RVM_IMPLEMENTATION_RECONCILIATION.md) for all implemented, partial and deferred requirements. In particular: stall failure; absent-MM indefinite waiting/misassignment; incomplete statistical reference validation; early appearance timing and unvalidated fine-IR origin; CW-only inherited station timing fallback; and unmeasured target resource behavior. No station landing or uninterrupted lap is claimed. The [proposed field procedure](NAVI_RVM_FIELD_TEST_PROCEDURE.md) requires separate operator authorization and resolution/review of these limitations before powered deployment.
