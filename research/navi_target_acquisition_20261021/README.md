# NAVI non-IR target-acquisition experiment (PWM 90)

Offline, causal implementation and experimental validation of
`docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md` against real Otto Hall
recordings. Research code only -- no firmware, no production code, no
governing specification changed. See `REPORT.md` for the full engineering
report and findings; this file is reproduction instructions.

## Layout

| File | Purpose |
|---|---|
| `route_map.py` | Read-only copy of the surveyed polarity/spacing map (from `RouteMap.h`, commit `3a8953c`). |
| `target_acquisition.py` | The recognizer: `CLOSE -> TARGET -> BASELINE -> ACQUIRE -> CLOSE AND ADVANCE`, causal, no IR, no automatic LOST transition. |
| `annotations.py` | Loads historical OPENING annotations for **diagnostic comparison only** -- never fed into the recognizer. |
| `replay.py` | Causal replay harness over the real `otto_train.bin` stream. |
| `analyze.py` | Offline-only diagnostic comparison, timing statistics, and example plot, run after `replay.py`. |
| `tests/test_target_acquisition.py` | Synthetic unit tests (stdlib `unittest`; no recorded data). |
| `results_110ms/` | Primary run: the assignment's own 110 ms starting-point duration gate. |
| `results_departure_floor/` | Exploratory run: the same gate evaluated at its data-driven floor (see `REPORT.md` §7/§11). |

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
python3 -m unittest tests.test_target_acquisition -v

# Primary run (110 ms duration gate, the assignment's own starting point)
python3 replay.py --data-root <path-to-NAVI_CPR_0_1> --out results_110ms \
    --min-acquire-duration 0.11

# Exploratory run (duration gate at the departure-criterion floor; see REPORT.md)
python3 replay.py --data-root <path-to-NAVI_CPR_0_1> --out results_departure_floor \
    --min-acquire-duration 0.02

# Diagnostic analysis + plot, for either run
python3 analyze.py --data-root <path-to-NAVI_CPR_0_1> --results results_110ms \
    --label "110ms primary"
python3 analyze.py --data-root <path-to-NAVI_CPR_0_1> --results results_departure_floor \
    --label "duration-floor exploratory"
```

Requires `numpy` and `matplotlib` (both already present in the
environment this experiment was built in). No `pytest` dependency --
tests use stdlib `unittest`.
