# NAVI_EWO_0_1: cruise base, retaining geographic grades

**Date:** October 9, 2026

**Status:** Built for Otto 9950011, host-tested; awaiting independent review and
field validation. Not flashed, merged, or field accepted. The universal STOP
overlay is the next increment and is not implemented by this change.

## Authority and starting point

David commissioned implementation following the agreed cruise-base then
universal-STOP development order. During the work he explicitly corrected the
interpretation of uniform base PWM: **“The grade dependent cruise setting should
stay. The grade is still there.”** That latest instruction governs this change.

The mandatory
[primary guide, including the grade-retention correction](https://github.com/ninobur/esp-loco-control/blob/4b8cf3671b896cd0cd8afba1fadb50dde6e601b6/docs/NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md)
and [MM045 bidirectional STOP target](https://github.com/ninobur/esp-loco-control/blob/4b8cf3671b896cd0cd8afba1fadb50dde6e601b6/docs/NAVI_STOP_MM045_BIDIRECTIONAL_TEST_DEFINITION_20261009.md)
are maintained on `docs/navi-cto-bubble-decisions-20261009`. They supersede
conflicting older architectural descriptions in this firmware branch.

Starting firmware: `codex/navi-ewo-0-1` at
`c7c21163b016e443524eb9fa483f8b706ce76e22`, verified live before editing. That
commit removed legacy station operating authority; it is not a field-acceptance
claim. The new isolated branch is `codex/navi-cruise-stop-20261009`.
The exact starting commit is the source rollback point; the prior documented
R2_FT2 field reference remains `ab0938b0f01531a38ce4d8a1d9d932ffad4f0334`.
No rollback or other hardware operation was performed.

This implementation applies the enduring decision-system principles by using
existing geographic knowledge and measured grade settings, preserving NAVI's
navigation authority, and adding no unnecessary controller or state. No
departure from those principles is adopted.

## Resulting behavior

The integrated sketch identifies as **NAVI_EWO_0_1_CRUISE_BASE**. Its AUTO cruise
decision passes **60** as the ordinary base value to the existing geographic
`cruisePwmAt()` function instead of the old profile's ordinary value of 90.

| Geography and direction | Requested geographic cruise PWM |
|---|---|
| Ordinary track, including station locations | 60 |
| CW MM065–MM079 | Existing 110 |
| CW MM080, 081, 082, 083, 084 | Existing 106, 102, 98, 94, 90 |
| CW from MM085 outside other applicable grade settings | 60 through the existing actuator ramp |
| CCW MM033–MM026 | Existing 105 |
| CCW after MM026, toward MM025 and ordinary track | 60 through the existing actuator ramp |

The grade table, boundaries, and transition values are byte-identical to the
starting source. Geographic PWM changes still use `requestPwm()` and
`serviceRamp()`; the requested target may change at a boundary, but the applied
PWM changes through the existing ramp. Returning to ordinary cruise now ends
at 60 rather than 90. Physical performance at this lower default remains for
track evaluation; it is not inferred from a successful build.

This base contains no station STOP/dwell, automatic service pattern, or active
overlay. It introduces no additional PWM cap, speed feedback controller,
navigation estimator, admission latch, or protocol. The existing manual,
STOP/GO, AUTO, E-stop, low-voltage, and PWM-zero movement/redeclaration paths
are preserved. Full manual throttle authority remains unchanged.

## Changes and validation

- Integrated sketch: ordinary base constant, geographic cruise call, runtime
  identity, and provenance comments.
- `tools/tests/test_ewo_integration.py`: compiles and runs the actual sketch's
  cruise, request, ramp, and withdrawal functions with hardware-output stubs.
  Tests 33 explicit geographic/directional cases, including stations, both
  grade boundaries, and every CW transition value; ramped rise/fall; manual
  throttle and STOP; undeclared AUTO withdrawal; E-stop and low voltage.
- A pre-existing integration-test reference to removed `serviceStation()`
  failed on the untouched stationless baseline. It now checks PWM-zero hold
  ordering before the actual `serviceAutoCruise()` call. The protection being
  checked is retained, not weakened.
- Sketch README and firmware catalog identify this candidate and preserve
  historical descriptions below an explicit status notice.

Validation:

1. **PASS:** integrated host runner, 11 executables including the underlying
   EWO regression; existing ASan/UBSan checks retained.
2. **PASS:** `tools.tests.test_navi_sync_format` and
   `tools.tests.test_ewo_integration`, 12 tests including the added actual-sketch
   control test with ASan/UBSan.
3. **PASS:** ESP32 build, installed core 3.3.12, selected Otto 9950011 profile,
   with format warnings treated as errors. Flash 1,015,439 / 1,310,720 bytes;
   static RAM 58,060 / 327,680 bytes. Verification used placeholder credentials;
   this is not a deployment image. The initial full build reported only three
   existing Adafruit INA219 enum-deprecation warnings.
4. **PASS:** whitespace check and source-boundary review. Navigation, map/grade
   data, actuator functions, command handlers, wire formats, IR receiver,
   locomotive profiles, and server code are unchanged.

These checks establish software behavior and buildability, not physical
stopping, grade performance, or readiness to activate two-train operations.

## Next increment

The STOP target remains the midpoint of MM045–MM046, approached from either
direction. The pending behavior questions cover one-time request consumption
and STOP/GO interruption, dwell/departure handoff, and selection of an initial
geographic approach profile. The guide's unresolved IR-position-gap behavior
must also be reconciled without inventing travel or stopping an already
established time-based ramp merely because pulses cease. No STOP algorithm or
unapproved default for these questions is introduced in this base commit.
