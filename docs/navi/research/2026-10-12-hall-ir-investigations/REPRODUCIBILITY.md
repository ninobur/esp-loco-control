# Reproduce the October 12 investigations

## Integrity and original files

From the repository root, verify every preserved source byte using only the Python standard library:

```sh
python3 docs/navi/research/2026-10-12-hall-ir-investigations/reproduce.py --verify-only
```

[source-manifest.json](provenance/source-manifest.json) records original absolute source locations, byte counts, original SHA-256, archive paths and stored SHA-256. These paths are provenance, not dependencies on the original Mac. Large raw recordings and logs use deterministic gzip compression. Decompression restores the original bytes, including the NSR capture SHA-256 `3d9f0032d53fc647b1eb36fbdc579b9d6f7e06482bcd39db4505185b9b119cd7`.

Original reports, tables, plots, results and analysis source are preserved without editorial changes. [ARTIFACTS.md](ARTIFACTS.md) provides relative links to every stored source. Some original prose contains historical absolute paths or plain filenames; use the inventory and master index to locate the archived equivalents. The original report date-like folder name `hall-field-analysis-20260929` is retained even though these analyses were performed October 12 and mostly use September 28 measurements.

## Run the analyses

The verified environment was Python 3.9.6, NumPy 2.0.2, Matplotlib 3.9.4 and Clang with AddressSanitizer/UndefinedBehaviorSanitizer. [requirements.txt](requirements.txt) pins the Python dependencies for installation in your chosen compatible environment. SciPy and scikit-learn are not required by these scripts. No network or hardware is required for replay once dependencies are installed. Allow about 150 MB for extracted raw sources plus generated outputs, and memory for the full 1 kHz capture.

Choose a new output directory outside this archive:

```sh
python3 docs/navi/research/2026-10-12-hall-ir-investigations/reproduce.py \
  --output-dir /tmp/navi-research-replay
```

The runner refuses to overwrite an existing directory. It verifies integrity, reconstructs source filenames, stages dependencies, and runs:

1. Continuous Hall reconstruction and morphology summary from the NSR recording and five September 28 MQTT logs.
2. Opening-to-peak measurements, interval offsets and held-out opening calibration.
3. Integer-pulse versus interpolated odometry and geographic candidate comparison.
4. Morphology classification, causal selection replay and explicitly synthetic sensitivity cases.
5. Raw waveform plots and metrics for events 254–257.
6. Type-5 arrival-age remeasurement using the last-arrival-before-Hall rule.
7. Independent verification of circuit endpoint counts and aligned snapshot brackets directly from NSR and MQTT records.
8. Reproduction of the four logic defects in the **superseded submitted** synchronization sketch, using its actual extracted functions and original movement/health headers.

The runner stages working copies and changes only three path constants: the continuous analysis's decoder root, the reporting script's research root, and the submitted-review script's baseline repository root. Computational logic remains unchanged. The extra circuit verifier was written for preservation because the original circuit work used inline commands rather than a saved standalone script. [Those commands are preserved](provenance/pulse-count-session-commands.json), including failed attempts; they are archival text, not a runbook. The new verifier does not independently prove geographic endpoint identities or the no-reversal interpretation; consult the original report and raw logs for those judgments.

Six generated numerical JSON results are checked recursively against their archived originals with relative/absolute tolerance `1e-9`, excluding relocated source paths. All 14 generated CSV files are also checked byte for byte. Plots are regenerated, but renderer-dependent PNG bytes are not used as a numerical acceptance test. Original plots have exact archived hashes.

## Verification performed for this archive

[Reproduction result](verification/reproduction-result.json) records the environment and six matching result files. [Circuit verification](verification/circuit-verification.json) records the independently recovered integer counters and brackets. [Review reproduction log](verification/review-reproduction.log) retains the reproduced defects. The other verification logs contain each analysis's output.

This rerun confirms reproducibility of the archived calculations. It is not a new railway test or independent validation of inferred labels, physical calibration, the clock model, or the research proposals.

## Corrected synchronization experiment

[IR_SYNC_OUTCOME.md](IR_SYNC_OUTCOME.md) identifies the corrected source and separate corrected-regression replay. The original review's expected defect reproductions are not tests of the repaired sketch.

Run the corrected regression in another new directory:

```sh
python3 docs/navi/research/2026-10-12-hall-ir-investigations/reproduce_sync_tests.py \
  --output-dir /tmp/navi-corrected-sync-tests
```

This reconstructs the exact historical include layout, adds the corrected variant, and invokes its unchanged `tests/run_all.py`. The neighboring historical baseline is present, so the original test script's fallback to a Mac path is never needed. [Corrected regression output](verification/corrected-regression.log), [result](verification/corrected-result.json) and [tested source hashes](verification/corrected-tested-source-hashes.json) record the successful archive replay.

The full Arduino build requires Arduino CLI 1.5.1, ESP32 core 3.3.12, FQBN `esp32:esp32:esp32`, PubSubClient 2.8, Adafruit INA219 1.2.3 and Adafruit BusIO 1.17.4. Exact project headers are archived. Local `credentials.h` and private device images are intentionally excluded; the committed build log supplies the original build result. To rebuild, supply your own credentials in a disposable build tree according to the included sketch's relative include path. Do not modify the archived sources or copy device credentials into Git.

No build or replay instruction here authorizes upload, operation, calibration changes or transmitter changes. Historical installation/backup helper scripts, if present among original evidence, are retained for provenance and are not invoked by the archive runner.
