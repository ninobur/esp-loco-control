# Manual Throttle: Full Hardware Range

Operator decision, 2026-09-23: there is no reason to cap manual controls at
the experimental navigation-profile ceiling. Manual requests now use 0..255.

The inherited NAVI_SIMPLIFIED profile defines NAVI_MAX_OPERATING_PWM=120.
Previously both the throttle handler and shared requestPwm function enforced
it. Removing only the handler clamp would therefore have had no effect.

Both layers are addressed in NAVI_COHERENCE_0_6_IR_HEALTH:

- The manual handler passes the parsed request directly, with explicit manual
  authority. Negative/over-255 requests still clamp to the hardware range.
- requestPwm enforces the experimental ceiling only on non-manual requests.
- All AUTO/station callers keep their default limited path.
- Existing enrollment, e-stop, low-voltage and ramp behavior is unchanged.
- No new route, distance, speed limit or IR dependency is introduced.

Build identifier: NAVI_COHERENCE_0_6_PROXIMAL_R1_IR_SPEED_R2.
Same Arduino folder and NGR-Files shortcut. This source change is NOT flashed;
the running R1 firmware still has the 120 cap until an authorized update.

Verification: tools/test_manual_pwm.py extracts the real requestPwm function,
compiles it with ASan/UBSan and tests 311 values through manual, AUTO and
safety-stop paths. It also checks the real manual handler retains admission
checks and explicitly chooses the full-range path. ESP32 core 3.3.11 build
passed: 1,017,227 bytes flash, 73,068 bytes static RAM. Only the three existing
INA219 library enum warnings appeared. No motor commands were sent during testing.

The IR-car stop-retention change is separate work in progress and must not
be confused with this manual-throttle correction or deployed by this commit.
