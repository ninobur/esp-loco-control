# NAVI_RVM statistical shape recognition — Claude, 2026-10-18

Research package for
[`docs/NAVI_RVM_STATISTICAL_SHAPE_RECOGNITION_CLAUDE_20261018.md`](../../docs/NAVI_RVM_STATISTICAL_SHAPE_RECOGNITION_CLAUDE_20261018.md).
It contains no firmware or operational change.

## Dataset inventory and provenance

All sources live under `~/NGR/NGR-Files/` and were read only. The sha256 hashes
were taken on 2026-10-18.

| File | Role | sha256 |
|---|---|---|
| `hall-ground-20261015/derived/native_15549e9385faa4d5.bin` | Native Hall ADC, Otto (9950011), boot 1, 2026-10-15 evening, 3,843 s, about 5 kHz. **The only recognizer input** | `a3cc9c6538e9aa3871a4e3357e96c7a76ef4c3f97ba282c52c610e718c7c4002` |
| `hall-ground-20261015/derived/encounters_adc_only.csv` | Frozen ADC-only encounters, used as annotation **extents** | `41bdbe5314e9cbe0898d85a35f6921a4945437567b57243965b68b411a8a2e61` |
| `hall-ground-20261015/derived/ground_intervals.csv` | Frozen ADC-only ground intervals, used for **false-positive** classification | `1d14005156639415151c5978c274a52bf70736b35b0420487a7206527a8f20d4` |
| `navi-rvm-replay-20261016/derived/selections_scoring_only.csv` | NAVI openings (MM, direction, polarity), used for annotation **identity** only | `6184d3f9050fd564cd6c8582d91e2ac15d52559e6ceae760360cda0bbcfe7782` |
| `navi-rvm-replay-20261016/derived/map_positions.csv` | Map polarity per MM, derived from NAVI's opening polarity | `be9f842b7bec624cfd8bd5ff461b76cc533699179abaac2e459fde0a08579c00` |
| `navi-ewo-0-3-ir-validity/field-records/20260828_magnet_types.json` | Bar/disk type, used **after the fact** to describe families | `e156423ea0941ce59dc99fc230a790df5614f8d3e1dd834bb8258ec82f7d60b2` |

**Boot 1 sessions**

| Session | Time | Contents |
|---|---|---|
| CW | 128–2,680 s | About 4.3 laps. Declarations at 104 s (063-064), 263 s (071-072), 975 s (021-022) and 1,403 s (123-124) |
| CCW | 2,727–3,773 s | About 2.1 laps. Declarations at 2,733 s (121-122) and 3,668 s (143-144); obstruction episode from about 3,564 s |

**Annotation counts** (`derived/annotation_boot1.csv`, built by `s02_annotate.py`)

| Category | Count |
|---|---|
| OPENING | 1,087 |
| SEQUENCE | 2 |
| NO_EXTENT (opening without an encounter) | 21 |
| UNLABELED supported activity | 8 |
| UNCERTAIN instability | 198 |

All 171 magnets have between 5 and 9 annotated passages: 739 CW and 350 CCW.

**MM050–MM070 region passes** (`results/experiment_folds.json`). Each magnet
has 5–7 repeats.

| Pass start | Direction | Coverage |
|---|---|---|
| 127 s | CW | MM064–070 only (partial) |
| 1,020 s | CW | MM050–070 |
| 1,643 s | CW | MM050–070 |
| 2,062 s | CW | MM050–070 |
| 2,483 s | CW | MM050–070 |
| 2,856 s | CCW | MM070–050 |
| 3,273 s | CCW | MM070–050 |

**Sources not used**

- **Boot 2 (`4d8083de79a0e299`).** This is the reserved holdout. It is
  untouched here.
- **The 2026-10-13 records.** They hold openings and counts, not a continuous
  native Hall tracing.

**Prior exposure.** Boot 1 has been examined in earlier NAVI research: the
10-16 replay, Hall-ground and the RVM manual matcher. The evaluation is
leave-one-pass-out within that one evening.

## Code (`scripts/`)

| Script | What it does |
|---|---|
| `common.py` | Paths. `RVM_VARIANT` selects an ablation (`shape_only`, `causal_level`) and writes its results under `results/ablation_<v>/` |
| `s01_tracing.py` | Builds the 20 ms tracing: non-overlapping means of the native reads. A block is NaN if it touches a serial jump |
| `s02_annotate.py` | Builds the annotation, frozen before modelling |
| `rvm_shape.py` | **The recognizer:** windows, features, Gaussian mixtures (numpy EM), LLR scan and best-explanation grouping |
| `s03_experiment.py` | Runs the leave-one-pass-out evaluation of Model A (all magnets) and Model B (MM050–070), plus held-out separation |
| `s04_casefig.py` | Case-plot helper |
| `s05_summarize.py` | Writes `results/summary.json` |
| `s06_families_figures.py` | Runs the family analysis and draws the figures |
| `s07_causal_fig.py` | Draws the trailing-level miss figure |
| `run_all.sh` | Reproduces everything: `PY=/path/to/python3.12 scripts/run_all.sh` (needs numpy, pandas and matplotlib) |

## Results (`results/`)

- `summary.json` has the headline counts, strata, boundary and midpoint errors,
  and separation. The same file exists for each ablation under
  `ablation_shape_only/` and `ablation_causal_level/`.
- `truth_*.csv` hold one row per annotated passage per model:
  - `n_det` counts recognitions: 0 = missed, 1 = recognized, 2 or more = split.
  - `pol` is the recognized principal polarity.
  - `d_*_ms` are the boundary and midpoint errors.
- `unmatched_*.csv` hold recognitions with no annotated identity:
  - `on_ground` means the recognition lies on a frozen baseline, i.e. a false
    positive.
  - `near_other` means it lies on UNLABELED or UNCERTAIN activity.
- `detections.csv` lists every recognition, by fold and model.
- `families.json` and `families_assignment.csv` hold the BIC results, the
  family cross-tabulations and the per-passage family.
- `experiment_folds.json` lists the folds, the region passes and the training
  sizes.

## Field-candidate work, 2026-10-18: the local baseline

This section belongs to
[`docs/NAVI_RVM_SHAPE_MANUAL_FIELD_CANDIDATE_20261018.md`](../../docs/NAVI_RVM_SHAPE_MANUAL_FIELD_CANDIDATE_20261018.md).
Its results are in `results/local/`.

| Script | What it does |
|---|---|
| `rvm_local.py` | The local-baseline sequential recognizer. It is the reference for `RvmShape.h` |
| `s10_local_experiment.py` | Leave-one-pass-out evaluation of Model A with the local baseline. Same folds and scoring as `s03` |
| `s11_nav_replay.py` | Builds the deployable model (`model_A.json`, all of boot 1), then replays navigation from the recorded declarations |
| `s12_nav_harness.py` | Evaluation harness, not firmware. It re-declares after each CONTRADICTION from the annotation, attributes every break to a cause, and checks STOP/GO |
| `s13_export_model.py` | Generates `firmware/.../RvmShapeModel.h` |
| `s14_firmware_parity.py` | Compiles `tests/navi_rvm_shape/replay.cpp` with the sketch headers and runs it on the native boot-1 binary. It compares the result with `rvm_local.py` and writes `parity.json` |
| `s15_hold_characterization.py` | **Synthetic** characterization: what happens when the Hall level is held inside an ensemble. It is software behaviour, not railway evidence |

To reproduce, run these in order: `s10`, `s11`, `s12`, `s13`, `s14`, `s15`.

## STOP/GO continuity, 2026-10-18

This section belongs to
[`docs/NAVI_RVM_SHAPE_MANUAL_STOPGO_20261018.md`](../../docs/NAVI_RVM_SHAPE_MANUAL_STOPGO_20261018.md).

| Script | What it does |
|---|---|
| `stopgo.py` | Python reference of `RvmPause.h`: the STOPPED rule (PWM < 15; first ruling < 25), startup admission, and the map from the waveform back to block ids |
| `s16_stopped_threshold_evidence.py` | Evidence only. Reads the PWM at Otto's last and first motion at the 35 station stops, using the IR odometer for description |
| `s17_stopgo_replay.py` | Boot 1 with the pause: leave-one-pass-out, the continuous replay for `s12` (`RVM_NAV_SUFFIX=_pause`), and C++ parity |
| `s18_stopgo_suite.py` | The required STOP/GO cases on 18 real ensembles, run in Python and the firmware headers. Includes the measured-departure, measured-arrival and zero-shift variants |

Results are in `results/local_pause/`, plus `results/local/stopped_threshold_evidence.*`.
