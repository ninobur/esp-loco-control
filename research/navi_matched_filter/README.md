# navi_matched_filter — offline replay harness (research only)

This directory holds a continuous Hall waveform recognizer: a normalized matched filter, plus its evaluation against NSR field captures. No ESP32 hardware is needed. The findings are in `REPORT.md`, and the corpus is listed in `CORPUS_MANIFEST.md`.

## Requirements

You need Python 3 with `numpy` and `matplotlib`. SciPy is not needed.

## Reproduce

```bash
cd research/navi_matched_filter
export NGR_NSR_NPZ=/path/to/npz          # where extracted captures go
export NGR_MF_CACHE=/path/to/cache       # conditioned traces + reference (pickles)
python3 extract_nsr.py "$NGR_NSR_NPZ" ~/Pi_Log_Downloads/9950011_20261025_133357.nsr ~/Pi_Log_Downloads/navi_session_*.nsr
python3 exp_sweep.py dev                 # selection, dev only -> results/sweep_dev.json
python3 exp_stress.py validation         # Question A -> results/stress_validation.json
python3 exp_overlap.py                   # Questions B/C (synthetic) -> results/overlap_*.json
python3 exp_continuous.py                # final scores, ledger, latency, scale, Level 4, geography
python3 make_figures.py                  # figures/
python3 make_manifest.py                 # CORPUS_MANIFEST.*
```

On a laptop, the whole pipeline takes about 15 minutes. The first `corpus.load` of each session builds and caches its reference.

The frozen configuration is in `config.py` and the method is described in `mf.py`. The reference that scores it is in `truth.py`; it does not use the template. All random draws use fixed seeds.

## Caution

- The reference is route-anchored oracle evidence, not physical truth.
- The synthetic overlap results (`exp_overlap.py`) describe algorithm behaviour, not field reliability.
- No firmware in this repository was changed.
