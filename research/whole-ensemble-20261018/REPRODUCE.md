# Reproduce the frozen test

Use the branch containing this package, keeping both previous experiment directories beside it. All recognition and eligibility replay inputs are committed: original portable 20-ms ADC sources, opening hypotheses, map polarity, movement snapshots and quality, source-only morphology review. Recognition requires no connection to the railway, MQTT, controller or external services.

From repository root with Python and the versions in `requirements.txt`:

```sh
export OPENBLAS_NUM_THREADS=1
export OMP_NUM_THREADS=1
export MPLCONFIGDIR=/tmp/ngr-whole-matplotlib
python -m pip install -r research/whole-ensemble-20261018/requirements.txt
python research/whole-ensemble-20261018/eligibility.py
python research/whole-ensemble-20261018/inventory.py
python -m unittest discover -s research/whole-ensemble-20261018 -p 'test_*.py' -v
python research/whole-ensemble-20261018/experiment.py train
python research/whole-ensemble-20261018/experiment.py evaluate
python research/whole-ensemble-20261018/assess.py
python research/whole-ensemble-20261018/check_integrity.py
python research/whole-ensemble-20261018/verify_replay.py
python research/whole-ensemble-20261018/plots.py
```

These commands replace only generated files under this new research directory. There is no search over hyperparameters and no evaluation-dependent training update. `functional.py` consumes ADC arrays and frozen model parameters only. `experiment.py` extracts each entire eligible span, calls recognition, and only afterward adds geographic metadata. Each model evaluates the identical 89 evaluation passages. Both model JSONs are frozen before the first evaluation call.

`source_review.py` and `focus_review.py` regenerate source-only review panels without predictions; `review_coverage.py` regenerates the training eligibility audit panels. The handwritten source morphology CSV and uncertainty intervals are frozen evidence, not refitted from predictions. Their pre-evaluation commit is `be4c6a3`.

To repeat the original movement metadata extraction rather than its portable replay, pass `eligibility.py --archive /path/to/NGR-Files`. The original archive must contain the three source files listed and hashed in `sources/movement_manifest.json`. That extraction is optional; their complete movement/quality snapshot table is included. To re-extract the historical lobe challenge table from the local archive, pass the same option to `source_review.py`. Its 14 relevant original rows are also preserved.

Outputs include complete state partitions with posterior coefficient distributions (`training_*.json`, `evaluation_*.json`), human-readable state tables, frozen A/B references, explicit overlapping unit object IDs, eligible/excluded source ranges, every evaluation annotation, reviewed field/baseline/unit outcomes, and all A/B background/field/noise-precision fits (`fits_A_0.npz`, `fits_B_0.npz`). NPZ keys are `<span_id>__baseline`, `__field`, `__precision`; indices are the span's original 20-ms source indices.

The arithmetic tests compare a marginal likelihood to a dense Gaussian calculation, the conditional path to exhaustive enumeration, the background smoother to a dense second-difference prior solve, and whole-function endpoint continuity. Integrity checks compare old control files to `8c40d6d`, reconstruct eligibility in a separate temporary directory byte-for-byte, and verify both A/B partitions cover every eligible ADC bin exactly once without crossing an exclusion. They validate implementation and preservation; source morphology establishes the available behavioral evidence.

No physical survey or independent operator field-boundary ground truth is manufactured by reproduction. Unreviewed or movement-censored observations stay unresolved. Replaying reused recordings is not a new blind field trial.
