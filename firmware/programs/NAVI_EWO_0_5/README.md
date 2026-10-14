# NAVI_EWO_0_5

October 14, 2026. **Unfinished experimental continuation; not field ready, not
flashed, and not fully validated.**

The operator's EWO_0_x numbering names the tested DART increment `NAVI_EWO_0_4`
and this continuation `NAVI_EWO_0_5`. The original foundation remains
`NAVI_EWO_0_1_MM045_STOP` at `85a2cf2a16d262abdf7258c60f60508b823913ee`.
The directory, primary `.ino` and runtime `SKETCH_NAME` agree.

## Source and status

The first-increment navigation code is preserved separately in
[`NAVI_EWO_0_4`](../NAVI_EWO_0_4/README.md), with its original rollback snapshot
and manifest in `dart-field-validation/`.

This directory preserves the later paused draft edits in `NaviIntegratedCore.h`,
`NaviDartEvidence.h` and `NaviDartOperations.h`. Those edits contain unfinished
historical timing and stopping behavior. They have not passed complete
validation or received approval for their undefined operating parameters.
They must not be treated as an operational build.

`NaviIrCredibility.h` implements the subsequently approved numerical rules:
up to 10 mm is consistent; more than 10 mm but below five installed IR pulses
is inconsistent but possible; exactly five pulses or more withdraws geographic
measurement authority immediately. Restoration requires a possible interval
from a new independently identified reference to the following independently
identified opening. The 9.652 mm installed pulse pitch remains unchanged.

The credibility evaluator is tested separately and is **not yet called by the
navigator**. It neither qualifies Hall candidates nor changes their identity.
Endpoint tracking, transitions between comparison methods and complete
credibility telemetry remain integration work. The approved rules and test
scope are recorded in
[`STATUS.md`](../../../dart-field-validation/ir-credibility/STATUS.md).

## Focused verification

From the repository root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  firmware/programs/NAVI_EWO_0_5/tests/test_ir_credibility.cpp \
  -o /tmp/navi-ewo-0-5-ir-credibility-test
/tmp/navi-ewo-0-5-ir-credibility-test
```

The full `run_tests.sh` also contains inherited and DART tests. The unfinished
operating-policy draft has not passed that full suite; the earlier 0_4 verdict
must not be applied to 0_5. ESP32 core 3.3.12 is installed; the integrated 0_5
sketch has not been compiled or tested on hardware.

## Decisions still required

- Historical timing sufficiency, prediction method and stop/restart context.
- The no-Hall-observation condition for declaring a missed target and stopping.
- Permitted STOP OVERLAY behavior when geographic identity is known but fine
  IR position is unavailable.

No default is authorized by the version naming instruction. Retain the map,
calibration, 70×2 detection, dart model, STOP OVERLAY geometry and programming,
manual controls and electrical protections. No commit, push, flash or live
operation is authorized by this change.
