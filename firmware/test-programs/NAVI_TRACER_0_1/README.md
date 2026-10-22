# NAVI_TRACER_0_1 — manual Otto, observation-only validated recognizer

**Status:** field-test candidate for review. **Not field accepted, not
flashed.** Otto (9950011) only.

Hardware/manual-control foundation: `NAVI_RVM_SHAPE_MANUAL`
(`dev/navi-rvm-shape-manual-20261018`). Recognition engine: a direct C++
port (`NaviTracerCore.h`) of the independently validated Hall recognizer at
commit `6efc952`
(`research/navi_target_acquisition_20261021/target_acquisition.py`).

Read, in order:
- `docs/NAVI_TRACER_0_1_BASE_SKETCH_ASSESSMENT.md` — why this hardware
  foundation, and exactly what was kept vs. replaced.
- `docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md` — the Python/C++
  equivalence results, and precisely what they do and do not cover.
- `docs/NAVI_TRACER_0_1_FIELD_TEST_AND_ROLLBACK.md` — build status, the
  first field-test procedure, and the rollback procedure.
- `docs/decisions/0023-navi-tracer-adopts-the-validated-hall-recognizer.md`
  — the decision record.

## Layout

| File | Purpose |
|---|---|
| `NAVI_TRACER_0_1.ino` | The sketch: hardware/manual-control from `NAVI_RVM_SHAPE_MANUAL`, recognition glue rewritten for `NaviTracerCore`. |
| `NaviTracerCore.h` | The recognizer: a direct, documented port of `target_acquisition.py`. No Arduino dependency — compiles standalone on the host. |
| `RvmTracing.h` | 20 ms Hall tracing from 5 kHz native ADC samples. Unchanged from `NAVI_RVM_SHAPE_MANUAL`. |
| `RouteMap.h` | The surveyed route map. Unchanged — confirmed byte-identical to the research's own `route_map.py`. |
| `RvmOps.h`, `NaviEstop.h`, `NaviWordStorage.h`, `NaviSyncRecorder.h` | Hardware/telemetry/safety infrastructure. Unchanged. |
| `LL_LocoConfig_9950011.h`, `LocoConfig.h`, `credentials.example.h` | Otto's hardware profile. Unchanged. |

Deliberately **not** carried forward from `NAVI_RVM_SHAPE_MANUAL`:
`RvmShape.h`, `RvmShapeModel.h`, `RvmNavigator.h` (the old "Model A"
statistical shape classifier) and `RvmPause.h` (its STOP/GO waveform
hold-back buffering) — see the base-sketch assessment and decision 0023 for
why.

## Build (compile only — not yet verified in this environment, see the field-test doc)

```sh
tools/navi_tracer_0_1/build.sh                       # inert placeholder credentials
tools/navi_tracer_0_1/build.sh --credentials-dir DIR # DIR/credentials.h is your private file
```

## Verify without hardware

```sh
tests/navi_tracer_0_1/run_equivalence.sh   # unit tests + Python/C++ event-stream equivalence
```
