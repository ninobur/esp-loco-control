# Independent magnetic shape comparison — 18 October 2026

**Neither tested model provides reliable continuous MM/BL recognition.** The geographic models offer little advantage on prepared intervals and produce more baseline detections in the continuous focused comparison. See [the report](REPORT.md), [the frozen protocol](PROTOCOL.md), and [the continuous waveform comparison](figures/continuous_failure.png).

This directory is the complete portable research package. It contains Hall-only 20-ms observations, reference/evaluation annotations, model parameters, all decisions, waveform plots and executable research code. No firmware, configuration, service, dashboard or hardware is changed. The inherited firmware elsewhere in this branch is prior project state, not a product of this investigation.

## Reproduce without the original recording archive

Python 3.12 was used. From this directory, with a Python environment containing [the pinned dependencies](requirements.txt):

```sh
python evaluate.py
python analyze.py
python validate.py
```

`evaluate.py` fits A and B from the included training intervals before evaluating the reserved recording partitions. It overwrites only `models/` and `results/` in this directory. `analyze.py` produces descriptive tables and `figures/` without changing either model. `validate.py` checks evidence hashes, scope, denominators and normalization arithmetic. The original model's SHA-256 is `1ae49f93c7569560e62a816a9793598fbf0e8311f51bd0fb3464ff0791d7d490`.

`python verify_portable.py` performs the same run in a fresh temporary directory containing only the delivered portable inputs and code, then compares regenerated models, result tables and plots byte for byte. [The recorded check passed](results/portable_reproduction.json).

For an isolated environment, use `python -m venv .venv`, then `.venv/bin/python -m pip install -r requirements.txt`; substitute `.venv/bin/python` in the commands above. There is no network dependency during evaluation once packages are installed.

## Reconstruct the portable dataset from preserved native arrays

```sh
python prepare.py --source-root /path/to/NGR-Files
```

The required local archive layout and SHA-256 hashes are preserved in [input_manifest.json](sources/input_manifest.json). The command verifies the native NSR slice and array hashes, makes nonoverlapping 20-ms sample-weighted Hall means, and regenerates all provisional annotations. It does not import any prior recognizer. The original 1.3-GB NSR slice is not duplicated on GitHub; the portable observations are included so Claude can reproduce all reported model results without access to David's filesystem.

The original file `/home/david/NGR/navi_sync/navi_sync_20261015_100403.nsr`, bytes `[3331022388,4718850776)`, plus its 24-byte capture header, is the provenance root. Native wire layout is `<IQhBB`: serial uint32, boot-clock microseconds uint64, ADC int16, PWM uint8, direction uint8. The last two fields are present in the archival record but never exported as model inputs. Previous payload/CRC validation is preserved in the manifest; this investigation rehashed the identical NSR and array bytes rather than claiming a new CRC-decoding run.

## Files for review

| Artifact | Content |
|---|---|
| [REPORT.md](REPORT.md) | Findings, quantitative comparison, failures, limitations |
| [models.json](models/models.json) | A/B means, variances, parameters and reference IDs |
| `data/hall20_*.csv.gz` | Every 20-ms Hall bin across both boot clocks, including loss masks |
| `data/annotation_seeds.csv` | All 1,195 preserved selected-MM annotation seeds |
| `data/annotations_tail_*.csv` | Every provisional MM/BL example, its provenance and unresolved reason |
| `data/native_frame_index.csv.gz` | Native row ranges mapped to source NSR frame offsets |
| [interval decisions](results/interval_decisions.csv) | A, oracle B, label-free B; unresolved rows retained |
| [continuous events](results/continuous_events.csv) | Every selected interval and unmatched annotated ensemble |
| [continuous summary](results/continuous_summary.csv) | Whole evaluation exposure and matched focused A/B exposure |
| [validation](results/validation.json) | Reproducibility and evidence-accounting checks |
| [additional recording inventory](sources/additional_recordings.csv) | Nearby evidence candidates not used in either model |

All reported MM identities remain annotation hypotheses. Reviewers should assess the baseline false detections and grouping failures before interpreting prepared-interval accuracy.
