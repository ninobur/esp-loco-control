# EWO PWM-zero within-interval localization correction — 2026-10-02

Candidate **NOT field accepted**. Implement/test/commit/push only. No flashing,
deployment, merge, live MQTT commands or Pi service changes are authorized.
Stop for David and Sam's review.

## Provenance and rollback

Branch: `codex/ewo-pwm-zero-localization`.
Base and immediate rollback: `ab0938b0f01531a38ce4d8a1d9d932ffad4f0334`,
FT2 flashed to Otto with David's explicit approval earlier on 2026-10-02.
Earlier preserved rollback points: `c3c925a` (0117), `d0185be` (pre-FT2 R2).
This candidate uses the same stable sketch path and FT2 runtime name; its
commit and version comment identify the PWM-zero correction. The governing
checkout's unrelated edits and stale `638c635` revision are not the target.

Authority: David's “EWO correction — PWM-zero IR movement retains interval”
instruction, codified in decision 0119. Principles: preserve physical evidence,
keep judgment inside NAVI, distinguish instrument movement from route position.

## Changes

- `NaviIntegratedCore.h` separates unknown within-interval coordinate from true
  IR frame failure. PWM-zero displacement retains interval/MM/target/direction
  and active Hall reference, cancels a coordinate-dependent spatial collection,
  holds distance navigation and records the existing displacement event.
- A fresh powered local origin, subsequent applicable positive IR travel and
  NAVI-owned appropriate Hall absent→present support can confirm the retained
  target without redeclaration. No unlocalized window or Missed Magnet; normal
  physical anchoring, spatial cycle, ±15% windows and miss authority resume at
  confirmation. No Hall-only or timer-based recovery.
- `INTERVAL_KNOWN_IR_POSITION_UNKNOWN` is published through existing nav/loop
  telemetry. MM/context remain retained. IR applicability is a separate fact;
  the unknown-coordinate label persists across staleness. True frame failure
  takes priority and still requires operator redeclaration.
- Sketch warning now describes factual PWM-zero movement without conflating
  it with loss of interval identity. Compatibility comments define the state.
  Repository-only dashboard status shows retained MM and unknown IR position;
  actual firmware output and display code are exercised offline. No deployment.
- Decision 0119, explicit supersession notices on 0112/0116, governing directory,
  integrated README and this change log preserve the revised rule and history.
  FT2 tests and rules, IR source policy, stations, AUTO, NSR1 and normal miss
  authority remain unchanged.

## Boundaries for review / behavior not generalized

1. First powered IR report is the local origin; its incoming delta is excluded
   because the cumulative packet can straddle handling and restart. Recovery
   requires a subsequent powered progression, Hall samples acquired after the
   origin report and a leading Hall landmark beyond that origin. No clock
   synchronization, interpolation or invented route distance is used. Thus a
   magnet reached before that first report cannot itself supply this recovery.
2. An additional PWM-zero report while unlocalized resets pending origin/onset
   evidence, even if unchanged. A normal zero-displacement dwell with a known
   coordinate retains it. Active Hall reference values are never erased by
   PWM-zero movement alone.
3. Existing `reverse()` requires a coherent coordinate. Reversal while unknown
   still takes its existing `FRAME_LOST_REDECLARE` hold path. Reversal math and
   target-selection policy were not redesigned; this separately commanded case
   remains an adjacent limitation for David/Sam, not a PWM-zero requirement.
4. If the intended landmark is missed during reacquisition, a later appropriate
   polarity landmark may silently be labeled as the retained target, as in
   FT2's accepted startup limit. No new interval can be inferred from lifted
   wheel rotation. An operator who moves Otto to another interval must redeclare
   because of that knowledge, not merely because IR moved.
5. Source selection and the known stationary-detector contrast limitation remain
   unchanged. Dry-lab checks establish software behavior, not track acceptance.

## Verification

- Full host suite: PASS with AddressSanitizer/UndefinedBehaviorSanitizer,
  `-Wall -Wextra -Werror`. Includes both map polarity profiles, integrated core,
  recorder, operations/stations, B1/B2 and IR authority regressions, unchanged
  FT2 cases, new coordinate-recovery cases, shared IR architecture and original EWO.
- New regressions: 28 synthetic PWM-zero displacements retain context/reference;
  CW/CCW powered recovery; no unlocalized miss or Hall-only progress; positive
  post-origin progression; initially supported field, queued old Hall, polarity,
  direction, stale IR, repeated stop, normal next-target window/miss, operator
  override, boot/source/scale/order faults, and unchanged known-coordinate dwell.
  These reconstruct invariants, not a replay of David's native field recording.
- Python interoperability: 9 tests PASS (`test_navi_sync_format`,
  `test_ewo_integration`), including actual C++ telemetry → controller state and
  actual dashboard JavaScript status rendering. No live services involved.
- ESP32 compile: PASS, Arduino CLI 1.5.1 / ESP32 core 3.3.12,
  `esp32:esp32:esp32`. Flash **1,012,219 / 1,310,720 bytes (77%)**;
  static RAM **57,660 / 327,680 bytes (17%)**, leaving **270,020 bytes** before
  runtime allocations. Versus flashed FT2: +396 bytes flash, +16 bytes static RAM.
  This is not a runtime heap/stack or hardware-load measurement.

Commands:

```sh
sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path /private/tmp/ewo-pwm-zero-esp32-20261002 --build-property compiler.cpp.extra_flags=-I/Users/davidbrown/esp-loco-control firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED
```

The compiler uses private local credentials by include path; no credentials are
copied or committed. No hardware port is opened by this correction task.
