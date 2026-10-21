# Phase 2 synthetic competition

All figures below are **synthetic performance**, not field validation.

| Model | Mean correct fraction | Worst condition |
|---|---:|---:|
| DNA_EXPERIMENTAL | 0.682 | 0.047 |
| QUORUM_EXPERIMENTAL | 0.682 | 0.047 |
| NAVI_SIMPLIFIED_EXPERIMENTAL | 0.682 | 0.047 |
| RVM_CONTINUITY_ADAPTER_EXPERIMENTAL | 0.682 | 0.047 |
| CONTINUOUS_LANDSCAPE_ADAPTER_EXPERIMENTAL | 0.753 | 0.199 |
| CPR_ADAPTER_EXPERIMENTAL | 0.682 | 0.047 |
| WOLF_RVM_PLUS_IR_EXPERIMENTAL | 0.682 | 0.047 |

## Otto signal-set, by difficulty

| Level | DNA | QUORUM | RVM | Continuous | CPR | RVM + IR |
|---|---:|---:|---:|---:|---:|---:|
| clean | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| speed | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| stops | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| sensor | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |
| imperfect | 0.0468 | 0.0468 | 0.0468 | 0.2807 | 0.0468 | 0.0468 |
| combined | 0.0468 | 0.0468 | 0.0468 | 0.2807 | 0.0468 | 0.0468 |

## Interpretation

Clean, speed, stop, and sensor-variation conditions do not separate these coarse adapters: all retain perfect synthetic progression. Under the test-defined missing/opposite-event condition, the continuous adapter is less brittle (0.281) but still has 117 false recognitions; this is not a winning claim. QUORUM's permissive forward scan produces more false recognitions (155) than DNA (88) in the same Otto imperfect case.

The RVM + IR adapter exactly matches RVM here, so this ablation finds no demonstrated synthetic benefit for that added mechanism. It should be removed from any putative winner until a different, measured test demonstrates value.

The simulator's truth remains isolated in `synthetic_truth.json`; no model reads it. Every adapter is an experimental reconstruction. The historical RVM/CPR sources are not claimed to run unmodified here.
