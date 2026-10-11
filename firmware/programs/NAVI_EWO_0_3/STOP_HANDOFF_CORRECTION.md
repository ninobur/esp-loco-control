# NAVI 0.3 accepted-position handoff to STOP — 2026-10-11

**Scoped software correction. Not ready for field deployment.** Other independent
review findings remain open, including the physical missed-pulse limitation in
[IR_VALIDITY_CORRECTION.md](IR_VALIDITY_CORRECTION.md).

Branch: `codex/navi-ewo-0-3-stop-integration`.
Starting commit and rollback: `253faa464692b97ab48beb0aa621aa11372aaa55`, the
published `codex/navi-ewo-0-3-ir-validity` revision. Its remote tip was verified
through the GitHub connector. This isolated checkout excludes the unrelated
uncommitted edits in the existing IR-review checkout. The preserved STOP reference
is `85a2cf2a16d262abdf7258c60f60508b823913ee`.

Implementation authority is the October 11 station STOP integration assignment.
This applies the established sole-NAVI-authority and retained-knowledge principles:
an accepted geographic MM remains usable without a fractional IR offset. No new
navigation judgment, operating policy, or safeguard is introduced.

## Root cause and complete path

Line references below refer to this correction's source; the replaced adapter
condition was at the same line 101 in `253faa4`.

1. `NAVI_EWO_0_3.ino:301`, `hallTask()`, acquires native Hall samples and queues
   their raw ADC, timestamp, serial, applied PWM and direction. The loop consumes
   these through `navi.observeHall(observation, now)` at line 1171, then calls
   `serviceAutoCruise()` at line 1176.
2. `NaviIntegratedCore.h:220`, `observeHall()`, supplies the mapped target,
   direction, polarity and opening IR association to the evidence bridge.
   `NaviEvidenceCore.h:20`, `HallQualifier::observe()`, enforces the existing
   70-count/two-sample detection, continuous-field latch, +/-15% qualification
   and 650-ms fallback. It does not assign an MM.
3. `NaviEvidenceBridge.h:23`, `observe()`, records qualified encounters and calls
   `NaviSequentialJudgment.h:24`, `judgeExpected()`. Only the core commits the
   accepted geographic identity, after direction, declaration, frame and DNA
   checks (`NaviIntegratedCore.h:270`). Contradictory observations remain pending.
4. `NaviIntegratedCore.h:529`, `confirm()`, sets `mm_` to the accepted mapped
   target and `positionReliable_=true`. It also sets
   `physicalPositionAnchored_=true`, even when the Hall boundary has no usable
   IR coordinate. In that case `targetOriginValid_` and `relationshipReliable_`
   are false (lines 544–545). This is an accepted magnetic identity without a
   measured fractional offset; the MM identification itself succeeded.
5. `NAVI_EWO_0_3.ino:745`, `serviceAutoCruise()`, passes the live NAVI object to
   `NaviStopOverlay.h:87`, `stopView()`. The declared/reliable/PWM-zero guards
   pass. `physicalOffsetUm()` (`NaviIntegratedCore.h:384`) returns false because
   the IR relationship/origin is unavailable, or the IR snapshot is stale.
   The old fallback at line 101 required `!navi.physicalPositionAnchored()`.
   Because confirmation had set that flag true, neither branch produced a view.
6. `NaviStopOverlay::command()` therefore receives `geographyAvailable=false`
   and the default geography, not usable MM045 geometry. Its final-braking
   transition at `NaviStopOverlay.h:172` cannot fire. The existing unavailable-
   geography branch at line 194 retains `currentCommand`, which is cruise 90
   in the reproduced route. No later STOP execution condition rejects an
   otherwise valid MM045 view.

**A versus B:** At the execution interface, A is the observed failure: STOP never
receives usable accepted-MM geography. The cause is a validity gate inside the
adapter, not a missing NAVI update or missing service call. If “overlay” includes
`stopView()`, it has access to the live NAVI object but excludes its accepted MM
through the obsolete anchored/declaration condition. It is not a braking/dwell
admission failure after successful geographic handoff.

## Minimal correction

In `stopView()`, change `else if (!navi.physicalPositionAnchored())` to `else`.
The existing fine-offset path remains preferred. Otherwise the existing MM lookup
uses NAVI's accepted `mm()` and `direction()`, with `available=true`, `fine=false`.
The original declared/reliable/PWM-zero guards remain. No raw Hall count is used
by STOP, and no state or qualification rule is added.

`recordStop()` at `NAVI_EWO_0_3.ino:735` labels this accepted, anchored MM source
`NAVI_MM`, retaining `OPERATOR_MM` for declarations and `HALL_IR` for fine views.
The fields, topics, motor commands and binary protocols are unchanged. Coarse
views continue to report null physical position/error, avoiding false precision.

This intentionally changes the adapter's treatment of missing fine IR: a reliable
accepted MM now yields its existing geographic instruction instead of withholding
all geography. Between Hall confirmations this has MM resolution; it does not
retain or extrapolate a fractional offset. A disappearing fine measurement can
therefore select the accepted MM's boundary instruction. No ramp, lookup value,
distance, final-braking rule, dwell or restart constant is changed.

## Before and after verification

The new regression was written and executed before editing either firmware file.
It feeds raw Hall samples into the actual integrated core, checks the accepted
DNA assignments and `StopView`, then executes production `serviceAutoCruise()`,
`requestPwm()` and `serviceRamp()` extracted verbatim from the sketch. Only clock,
hardware outputs and publication are stubbed. It never declares MM045 to make the
CW failure pass: it declares MM033 and navigates through twelve confirmations.

| Check | Before correction | After correction |
| --- | --- | --- |
| Existing full regression suite | Pass, exit 0 | Pass, exit 0 |
| New seven-case handoff regression | Four failures, three passes | Seven passes |
| No IR, CW MM045 | Reliable MM045; available=0; target PWM 90 | Reliable MM045; available=1; fine=0; final target PWM 0 |
| Stale IR, CW MM045 | Same reproduced failure | Same corrected handoff |
| No IR, CCW final entry at MM046 | available=0; target PWM 90 | available=1; final target PWM 0 |
| Usable fine IR, CW route | Pass | Pass |
| Fine-to-MM fallback after stale IR | Fail | Pass, no fractional position claimed |
| Manual STOP / throttle 0, 35, 90, 200, 255 | Pass | Pass |
| Unqualified Hall samples / pending DNA | Pass | Pass; cannot supply new usable geography |

Independent geographic expectations test approach PWM 85, 75, 65, 55, then 50,
followed by final braking at MM045 CW / MM046 CCW, for the unchanged physical
target `stopMarkerUm(45)+150000`. The real actuator is checked at 30/31 ms for
ordinary deceleration and 299/300 ms for final decrements. Applied-zero dwell
is checked at 4999/5000 ms, restart at 149/150 ms, and no repeated stop is allowed
in the same visit. Final braking/dwell/restart completes without further Hall/IR.

The full suite passes 23 existing C++ executable runs and 39 Python unittest
methods (including the seven new shell scenarios), plus native radio transport
and the unchanged 22,940-event pulse replay. ASAN/UBSAN and warnings-as-errors
remain enabled for the C++ regressions; leak inspection remains disabled.

Source-preservation tests compare the complete STOP geometry and execution code
outside `stopView()` with `85a2cf2`. Actuator, manual, AUTO, MQTT and Type-5
functions/constants remain equal under the existing explicitly scoped comparison;
the only additional telemetry exception is the source label. All NAVI detection,
qualification, IR-credibility, DNA, map and calibration sources are unchanged
from `253faa4`. Historical retired station tests remain retired as before.

Reproduce from the repository root:

```sh
python3 firmware/programs/NAVI_EWO_0_3/tests/test_stop_handoff.py
sh firmware/programs/NAVI_EWO_0_3/run_tests.sh
```

Raw evidence: [baseline suite](verification/stop-handoff-baseline.txt),
[new regression before the fix](verification/stop-handoff-before.txt),
[complete corrected suite](verification/stop-handoff-after.txt),
[ESP32 build](verification/stop-handoff-esp32-build.txt).

## ESP32 build and limitations

Full build passed with Arduino CLI 1.5.1, ESP32 Arduino core 3.3.12,
`esp32:esp32:esp32`, inherited Otto 9950011 profile, and installed PubSubClient,
Adafruit INA219 and BusIO libraries. The external credentials include was a copy
of the repository's safe placeholder template; operational secrets were not used.
Flash usage: **1,028,071 / 1,310,720 bytes (78%)**. Globals:
**62,492 / 327,680 bytes (19%)**, leaving **265,188 bytes**.

```sh
mkdir -p /tmp/navi-stop-build-config
cp firmware/config/credentials_template.h /tmp/navi-stop-build-config/credentials.h
ARDUINO_BUILD_CACHE_PATH=/tmp/navi-stop-arduino-cache arduino-cli compile \
  --fqbn esp32:esp32:esp32 --build-path /tmp/navi-stop-esp32-build \
  --build-property compiler.cpp.extra_flags=-I/tmp/navi-stop-build-config \
  firmware/programs/NAVI_EWO_0_3
```

These tests establish deterministic software handoff and actuator behavior, not
physical stopping distance or track performance. No hardware flash, track test,
live MQTT/Pi change or deployment was performed. MM-only operation has no measured
fractional offset. Other review findings, IR missed-pulse discrimination and the
previously documented navigation limitations remain outside this correction.
**No field-deployment readiness claim is made.**
