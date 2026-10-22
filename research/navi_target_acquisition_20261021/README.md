# NAVI non-IR target-acquisition experiment (PWM 90)

Offline, causal implementation and experimental validation of
`docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md` against real Otto Hall
recordings. Research code only -- no firmware, no production code, no
governing specification changed. See `REPORT.md` for the full engineering
report and findings -- **read its "Corrective investigation addendum
(2026-10-22)" section first**; it supersedes several interpretations in
the original report below it. This file is reproduction instructions.

## Layout

| File | Purpose |
|---|---|
| `route_map.py` | Read-only copy of the surveyed polarity/spacing map (from `RouteMap.h`, commit `3a8953c`). |
| `target_acquisition.py` | The recognizer: `CLOSE -> TARGET -> BASELINE -> ACQUIRE -> CLOSE AND ADVANCE`, causal, no IR, no automatic LOST transition. Unchanged since the first commit -- the corrective investigation examined it and found no defect (see `REPORT.md` item 1). |
| `annotations.py` | Loads historical OPENING annotations for **diagnostic comparison only** -- never fed into the recognizer, and (per the 2026-10-22 operator correction) known to carry identification errors inherited from the old IR-dependent system. Not geographic ground truth. |
| `replay.py` | Causal replay harness over the real `otto_train.bin` stream. Fixed 2026-10-22: geographic progression now begins only after the seed annotation's own encounter closes (`seed_established_index`), not at the trial's raw start. |
| `analyze.py` | Offline-only diagnostic comparison (now interval-overlap based, matching `98db802`'s own method), timing statistics, and example plots, run after `replay.py`. |
| `investigate_anomaly.py` | Detailed actual-vs-counterfactual trace for the Trial 0/MM129 284.58 s open candidate (2026-10-22 investigation). |
| `tests/test_target_acquisition.py` | Synthetic unit tests (stdlib `unittest`; no recorded data), including a regression test locking in the frozen-baseline behavior. |
| `tests/test_replay_seeding.py` | Regression tests for the seed-timing fix. |
| `results_110ms/`, `results_departure_floor/` | **Original** runs (110 ms gate / its data-driven floor), kept for before/after comparison; re-analyzed in place with the corrected diagnostic method. |
| `results_corrected_110ms/`, `results_corrected_departure_floor/` | **Corrected** runs (seed-timing fix applied) -- see `REPORT.md`'s addendum for what changed and why. |
| `results_anomaly_investigation/` | The Trial 0/MM129 detailed trace and plot. |

## Data

This experiment replays `raw/otto_train.bin` (Otto, boot
`15549e9385faa4d5`, the `otto_train` stream), cross-checked against
`data/train_annotations.csv`, from `research/NAVI_CPR_0_1`
(`sources/data_manifest.json`, commit `98db802`'s own lineage). **That
raw data has never been committed to this repository, on any branch** --
`research/NAVI_CPR_0_1/.gitignore` excludes it, and `git log --all` over
`research/NAVI_CPR_0_1/raw` returns nothing. It was located for this
experiment at two on-disk copies outside the repo:

- `/Users/davidbrown/NGR/NGR-Files/navi-cpr-0-1/research/NAVI_CPR_0_1`
- `/Users/davidbrown/Documents/Codex/2026-10-21/navi-wolf-pack-research/research/NAVI_CPR_0_1`

Both verified identical by SHA-256 of `raw/otto_train.bin`
(`b366bff8f17d28c64f7b666b68e171cef8521dcd1a791ecf6b147713506d0b20`,
matching `sources/data_manifest.json`'s own recorded hash). Point
`--data-root` at either.

## Reproducing

```bash
# Unit tests (synthetic only, no recorded data needed)
python3 -m unittest discover tests -v

# Corrected runs (seed-timing fix applied -- see REPORT.md's addendum)
python3 replay.py --data-root <path-to-NAVI_CPR_0_1> --out results_corrected_110ms \
    --min-acquire-duration 0.11
python3 replay.py --data-root <path-to-NAVI_CPR_0_1> --out results_corrected_departure_floor \
    --min-acquire-duration 0.02

# Diagnostic analysis + plots, for any results directory
python3 analyze.py --data-root <path-to-NAVI_CPR_0_1> --results results_corrected_110ms \
    --label "corrected: 110ms"
python3 analyze.py --data-root <path-to-NAVI_CPR_0_1> --results results_corrected_departure_floor \
    --label "corrected: duration-floor"

# Detailed Trial 0 / MM129 anomaly trace (284.58 s open candidate)
python3 investigate_anomaly.py --data-root <path-to-NAVI_CPR_0_1> --out results_anomaly_investigation
```

Requires `numpy` and `matplotlib` (both already present in the
environment this experiment was built in). No `pytest` dependency --
tests use stdlib `unittest`.
