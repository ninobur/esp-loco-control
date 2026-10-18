#!/bin/sh
# Reproduce everything. PY = a Python 3.12 with numpy, pandas, matplotlib (no sklearn/scipy needed).
set -e
PY=${PY:-python3}
cd "$(dirname "$0")"
$PY s01_tracing.py
$PY s02_annotate.py
$PY s03_experiment.py
$PY s05_summarize.py > /dev/null
for v in shape_only causal_level; do
  RVM_VARIANT=$v $PY s03_experiment.py
  RVM_VARIANT=$v $PY s05_summarize.py > /dev/null
done
$PY s06_families_figures.py
$PY s07_causal_fig.py
