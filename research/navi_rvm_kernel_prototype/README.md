# NAVI RVM recognition kernel — exploratory C++ prototype

Build: `g++ -std=c++17 -O2 main.cpp -o rvm_demo && ./rvm_demo`

This is an **initial mechanism prototype, not an operational navigator**. It compares every eligible smoothed Hall sample to a continuous map-indexed waveform and carries a bounded, monotone phase frontier. There is no Hall opening threshold, no region counter, no geographic relocation search, no learning, and no station control. A commitment is triggered by phase passing the expected model landmark with a margin against paths that have not passed it.

## Limitations requiring correction before real replay
- The synthetic harness is not independent validation and does not establish performance.
- The model's `mm_peak` coordinate is a **placeholder** for a learned geographic landmark. The operator's overlap/puzzle definition must be mapped to real waveform coordinates.
- The simplistic per-sample confirmation and fixed phase step are **candidate mathematics**, not an empirically calibrated commitment rule.
- The bounded search uses `last_phase` and may collapse prematurely on a wrong phase. A proper reference-free ablation and mismatch accounting are not yet implemented.
- This initial code has no phase wraparound, gap/clock handling, IR integration, explicit anomalous observation isolation, or calibrated stall model.
- The `best` local score is not an appropriate whole-piece quality measure. The mismatch indicator is illustrative and must **not** be used to trigger LOSS OF PATTERN.
- The same source sample may influence successive DP scores; it must not be treated as independent statistical evidence.
- No raw October 15 recordings were replayed in this demonstration.

The key review question is whether a continuous-waveform phase frontier is the right primitive. If it is, subsequent work should validate it against actual records before implementing operational actions.
