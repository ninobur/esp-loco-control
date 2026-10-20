# NAVI_CPR_0_1

An isolated offline reference experiment in causal recognition of complete
magnetic encounters. **Research result: not ready for locomotive integration.**
The source code is intentionally small; operational firmware is untouched.

- [Evidence and recommendation](REPORT.md)
- [Architectural reconciliation](ARCHITECTURE.md)
- [Frozen protocol](PROTOCOL.md)
- [Clean implementation](cpr.py)
- [Data provenance](sources/data_manifest.json)

## Reproduce

From the repository root, with Python 3.11+ and NumPy. Matplotlib is needed only
for plotting. Tested locally with the versions recorded in evaluation.json.

```sh
cd research/NAVI_CPR_0_1
python3 check_package.py verify
python3 portable.py restore
python3 -m unittest discover -p test_cpr.py -v
python3 experiment.py evaluate
python3 synthetic.py
python3 assess.py
MPLCONFIGDIR=/tmp/navi-cpr-mpl python3 plots.py
```

`portable.py restore` reconstructs original native bytes from lossless compressed
columns and verifies all four hashes. No Pi access is required. `raw/` is ignored.
`data/` includes original ADC and acquisition timing, plus 20-ms derived tracings.
The model never reads the native PWM/direction fields. They are retained only to
make native-byte provenance verifiable.

The commands above replay the frozen models. To repeat construction/calibration
as a separate reproduction (overwriting generated outputs), run
`python3 experiment.py calibrate` **before** evaluation. It uses only the specified
construction/calibration partitions; it never fits the CCW or Toby material.
The initial model freeze is commit `eeec67a`, with a full model hash in
`results/calibration.json` and `results/evaluation.json`.

Original acquisition, not needed for portable reproduction: `prepare.py` reads a
fixed slice of the Pi archive and the earlier local October 15 construction
binary. The exact source paths, byte bounds, clock offsets, sample counts and
hashes are in the manifest. Pi access was read only. The retained Pi MQTT excerpt
is evidence, not a recognition input.

## Replay another native Hall sequence

Supply a CSV with integer `t_us,adc` columns and preferably `serial` (original
acquisition serial). Samples must be in acquisition order; clocks must strictly
increase. Without serials, only timing/low sample-count gaps can be established.
The reference expects approximately 5 kHz native samples and forms causal 20-ms
means with at least 50 original reads per bin. Do not submit an already averaged
50-Hz tracing as though it were native evidence.

```sh
python3 replay.py --csv /absolute/path/hall.csv --out /absolute/path/cpr-output
```

This writes a chronological `custom_journal.jsonl`, completed-event features and
all three alternatives' scores in `custom_events.json`, and baseline/Hall arrays
in `custom_trace.npz`. No device, network, navigation or motor action is possible.

## Code map

| File / component | Responsibility |
|---|---|
| `prepare.py` | NSR validation, native provenance, causal aggregation, comparison annotations |
| `cpr.py`: `Baseline` | Local level, drift and noise; admitted past background only |
| `cpr.py`: `NativeRule` | Separate 70-count/two-native-sample detection witness |
| `cpr.py`: `Segmenter` | Provisional opening, prehistory, sustained return, completion/censoring |
| `cpr.py`: `describe`, `area_resample` | Physical features and entire-curve 64-phase representation |
| `cpr.py`: `Population` | Empirical families, PCA/covariance and Gaussian comparison; frozen one-class scoring |
| `experiment.py` | Training/calibration/test separation and evaluation-only interval matching |
| `synthetic.py` | Deterministic illustrative success and failure cases |
| `assess.py` | Independent byte roundtrip, missing-data case audit, costs and background accounting |
| `plots.py` | Read-only rendering of frozen results |
| `test_cpr.py` | Mathematical and causal invariants |

## Output conventions

Times are seconds on the locomotive boot clock. Trace timestamps are bin ends,
the earliest availability of the complete average. An event's `start` and `end`
are estimated boundaries; `completed` is the causal decision timestamp. A
`provisional_open` row is not a recognized magnet. `incomplete` is never scored.
Native detection rows retain the second original acquisition sample timestamp.
Gap and EOF censoring remain explicit.

`shape64` preserves all event samples by area integration. Raw ADC amplitude,
duration, polarity, SNR, asymmetry and opposite-polarity area remain separate.
Scores are **distance margins**, not probabilities. Each alternative records a
distance, threshold, normalized margin, and recognition decision. A recognition
requires the separate native witness. Geographic labels occur only in the
historical comparison files and evidence report.

Only the online component state is bounded. Offline plots, source restoration,
training, diagnostic sinks and full replay records intentionally use host RAM and
disk. The implementation assessment must not charge those host-only products to
an ESP32 or claim the whole Python experiment is a bounded embedded executable.
