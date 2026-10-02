# NAVI EWO R2 — first target after declaration change log

Status: **NOT field accepted; not flashed.** Await David's review.
Authority: David's scoped 2026-10-02 change instruction and request to document,
commit and push this change log. Implemented and host-tested by Codex.

## Provenance and rollback

- Repository: `ninobur/esp-loco-control`.
- Base: `origin/codex/ewo-ir-authoritative` at
  **`d0185be25ec51f9ba6d58567458a59d3f1c289ca`** (rollback point).
- Task branch: `codex/ewo-first-target-after-declare`.
- Sketch: `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino`.
- Version comment identifies the R2 first-target revision; runtime sketch name,
  MQTT payloads and NSR1 layouts are unchanged. The commit containing this log
  identifies the correction artifact; its exact SHA accompanies the handoff.

## Change

`NaviIntegratedCore.h` adds first-target and observed-absence state. Declaration
sets the exception and clears observed absence. Only a full actionable Hall
window acquired strictly after declaration can establish absent support;
subsequent support can then qualify. An initially supported field must drop
and return. While the exception is active, confirmation accepts
`0 < traveled <= expectedCumulativeUm + interval * 15 / 100`.

Confirmation, a recorded Missed Magnet, and reversal clear the exception.
The miss test is unchanged: equality at the upper bound is not a miss; travel
beyond it is. Physical-origin handling, ±15% tolerance, polarity, ±70 threshold,
direction, median-of-five, landmark selection, spatial reference, command
parsing, station logic, MQTT and NSR1 remain unchanged.

## Files

- `NaviIntegratedCore.h`: sole runtime behavior change.
- `tests/test_first_target_after_declare.cpp`: new scoped regressions.
- `tests/test_integrated_core.cpp`, `tests/test_ir_authority.cpp`,
  `tests/test_review_regressions.cpp`: existing confirmation fixtures gain an
  actual post-declaration neutral window. The backlog regression now exercises
  a subsequent target, where the ordinary lower bound still applies.
- `run_tests.sh`: includes the new regression executable under ASan/UBSan.
- Sketch `.ino`: version comment only. Sketch README: rule, provenance,
  tests, status and known limitation.
- Decision **0117**: David's first-target rule and narrow qualification of
  0116's ordinary lower bound. This file: change log.

## Verification

The unmodified base suite and nine Python tests passed before implementation.
After the change:

- Complete integrated host runner: **PASS**, including EWO, map polarity,
  operational admission, station behavior, IR authority/architecture, recorder
  and the new regressions. Existing detector stationary-contrast limitation is
  still reported by its test; it was not changed.
- New cases: reported CCW 45→44 at 106 mm, followed by 43 in its normal window;
  first target at normal distance CW/CCW; parked supported field; absent first
  target and restored subsequent lower bound; wrong polarity; zero and exact
  upper/lower bounds; old/equal-timestamp queued samples; redeclaration;
  reversal clears the exception.
- Python NSR1 and actual firmware/controller/dashboard interoperability:
  **9 tests PASS**.
- `git diff --check`: **PASS**.
- Actual ESP32 compile: **PASS**. Flash **1,011,815 / 1,310,720 bytes (77%)**;
  static RAM **57,644 / 327,680 bytes (17%)**, leaving **270,036 bytes** before
  runtime allocations. The static figure excludes dynamically allocated
  recorder storage, queues, network buffers and task stacks; it is not a
  measured runtime heap margin.

Commands:

```sh
sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-path /private/tmp/ewo-first-target-esp32-20261002 \
  --build-property compiler.cpp.extra_flags=-I/Users/davidbrown/esp-loco-control \
  firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED
```

The compile uses installed ESP32 core 3.3.12 and the base's Otto profile, with
the existing private credentials include directory. No credentials are copied
or committed. The field case is reconstructed from David's supplied evidence
for `9950011_20261002_120424.log`, not replayed from a full native NSR capture.

## Limits and handoff

**Known limit, deliberately unchanged:** reversal before first confirmation
still computes position from the assumed declaration origin. No adjacent
runtime changes were made. Existing source-selection and hardware/recorder-load
limitations remain as documented in the base README.

Host tests and compilation do not establish track behavior. David decides when
to flash and runs one CW and one CCW lap with NSR1 enabled. No locomotive was
flashed; no running Pi service was touched. The governing checkout's unrelated
changes were preserved by working in an isolated worktree.
