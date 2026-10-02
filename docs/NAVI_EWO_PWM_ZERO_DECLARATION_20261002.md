# EWO PWM-zero movement requires declaration — 2026-10-02

**NOT field accepted. No flash, deployment or merge. Stop for David and Sam.**

## Authority, provenance and rollback

David and Sam's final “Resolution of PWM-zero movement question” instruction
governs this correction, codified in decision 0120. Work remains on
`codex/ewo-pwm-zero-localization`, based on
`b146815b4431f1519046a2706abdfbfd11ffabc4` (unflashed 0119 candidate, now superseded).
The deployed rollback point remains `ab0938b0f01531a38ce4d8a1d9d932ffad4f0334`,
FT2 flashed to Otto earlier with David's separate authorization. Earlier rollback
points `c3c925a` and `d0185be` remain available. No history rewritten.

The sketch path and `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2` runtime name remain
stable; identify this revision by its commit and updated version comment.
No private credentials were copied or committed. The governing checkout's
unrelated work was not changed.

## Implemented correction

- Removed `intervalPositionUnknown_`, recovery-origin/readiness/timestamp fields,
  recovery absence evidence and automatic Hall+IR reacquisition branch.
- Added a single declaration-required movement latch. Coherent IR displacement
  observed at PWM=0 holds navigation. The displacement is still counted and
  recorded; it is never signed route travel. Last MM, interval/target, direction
  and active Hall reference remain diagnostic context. The obsolete coordinate
  and dependent in-progress spatial collection cannot drive navigation.
- While latched, no Hall or IR observation can confirm a magnet, substitute a
  later matching magnet, progress Missed Magnet or automatically restore the
  coordinate. Only a valid operator declaration clears the latch and establishes
  the existing FT2 startup context. Invalid declarations and reversal cannot clear it.
- Sketch service checks the latch after IR ingress and before queued commands
  and station departure. It withdraws AUTO/enrollment through the existing
  controlled-stop path and publishes `state/auto=0`, `state/nav_ready=0` and the
  operator warning. This does not depend on the finite NAVI event queue.
- AUTO/GO admission and periodic readiness publication include the latch.
  Manual positioning remains available after withdrawal; human verification is
  represented by the subsequent declaration, not inferred by NAVI.
- Telemetry uses `PWM_ZERO_MOVEMENT_REDECLARE`; the dashboard has an explicit
  red verify/reposition-and-declare status. The compatibility `NORMAL` label
  continues to carry the retained diagnostic MM, not navigation permission.
  Genuine frame failures retain their separate state and reporting priority.
  MQTT topic/JSON shapes and NSR1 record formats are unchanged.
- Zero-displacement PWM-zero dwell preserves localization without withdrawal.
  No startup, normal ±15%, normal Missed Magnet, source selection, Hall reference,
  IR health, station or reversal policy was redesigned. 650-ms recovery stays absent.

## Why automatic recovery was withdrawn

David and Sam accepted the mapped-geometry counterexample: CW 001→002 is 340 mm;
002→003 is 330 mm; both target markers are South. A South onset at 335 mm can be
the expected 002, or 003 after failing to detect 002 at 5 mm. This is a geometry
demonstration, not a claim that this exact scenario was recorded on track.
The earlier first-powered-report origin also excluded motion that could already
have crossed the expected landmark. No heuristic failure boundary was implemented.

David's observed 28 PWM_ZERO_IR_DISPLACEMENT events during lifting/turning/
repositioning, followed by position establishment at 045–046 and declaration,
are the intended operating model. NAVI does not diagnose why the wheel moved.

0120 supersedes 0119; the latter and its implementation report remain historical.
0112/0116 have explicit follow-on notices; the governing directory and integrated
README point to the final rule.

## Verification

- Full EWO host suite PASS, including ASan/UBSan, both route polarity profiles,
  integrated core, recorder, operating adapters, B1/B2, IR authority, unchanged
  FT2, shared IR architecture, station-position tests and original EWO tests.
- Revised PWM-zero regressions cover retained context/reference, no MM advance
  or miss across repeated movement and later matching onsets, no Hall recovery,
  invalid vs valid declaration, post-declaration FT2/normal windows/misses,
  zero-displacement dwell, reversal not clearing the latch, and distinct genuine
  boot/source/scale/order failures (including combined conditions).
- Python interoperability: **10 tests PASS**. Actual sketch `withdraw`,
  `servicePwmZeroMovementHold` and `opsNow` functions are compiled with hardware
  output stubs and ASan/UBSan. Checks cover active AUTO withdrawal, warning and
  readiness publication, event-queue overflow, refused AUTO/GO, manual positioning,
  declaration, stationary dwell and repeated movement. Actual C++ telemetry and
  dashboard status rendering are exercised offline; no services are started.
- ESP32 final compile PASS: Arduino CLI 1.5.1 / ESP32 core 3.3.12,
  `esp32:esp32:esp32`. Flash **1,012,043 / 1,310,720 bytes (77%)**;
  static RAM **57,644 / 327,680 bytes (17%)**, leaving **270,036 bytes** before
  runtime allocations. Compared with `b146815`: 176 bytes less flash and
  16 bytes less static RAM. This is not a runtime heap/stack measurement.

Commands:

```sh
sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path /private/tmp/ewo-pwm-zero-redeclare-esp32-20261002 --build-property compiler.cpp.extra_flags=-I/Users/davidbrown/esp-loco-control firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED
```

## Remaining boundaries

The operator must actually verify/reposition the locomotive; software cannot
prove that from a declaration command. Reboot still starts undeclared. The
existing controlled stop is used, not a newly introduced emergency brake.
Existing reversal-without-coordinate hold behavior remains; it cannot bypass
the declaration latch. FT2's explicitly accepted first-target limitations after
a fresh declaration remain unchanged. Host tests are not field acceptance;
the pre-existing stationary detector contrast limitation, source-policy and
hardware queue/throughput limitations remain outside this correction.

No locomotive or IR device flashed, no live MQTT commands sent, no Pi service
changed, and no deployment or merge performed.
