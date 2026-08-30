# 0060 — Battery policy belongs to the locomotive, and low voltage ramps

**Status:** Accepted
**Date:** 2026-08-30
**Decided by:** implements the operator's ruling of 2026-08-29 on stop ramps,
and restores the profile policy that was already his.

## Context

NAVI_ONE 0.1 carried its own battery policy, in the sketch:

```cpp
static constexpr float LOW_VOLTAGE_V = 14.4f;
bool low = busV > 0.1f && busV < LOW_VOLTAGE_V;
```

Toby's profile carried a different one, complete and older, and it was dead
code: `SHUTDOWN_VOLTAGE 13.25`, `RECOVERY_VOLTAGE 14.0`,
`DISCONNECTED_VOLTAGE_THRESHOLD 12.5`, `VOLTAGE_COUNTER_LIMIT 5`. Two policies
existed, one of them invisible, against the repo's own rule that per-locomotive
values belong in the config headers.

The sketch's version did three wrong things:

1. **It could not tell a flat battery from no battery.** On 2026-08-29 the
   operator disconnected the pack and the locomotive announced LOW VOLTAGE.
   The profile has had a threshold for that case for months: below 12.5 V there
   is no pack, only bench power.
2. **Trip and recovery shared one number.** A pack sagging around 14.4 V under
   load chatters in and out every 5 s poll, dropping AUTO each time.
3. **Low voltage inherited the e-stop's instant cut**, in MANUAL as well as
   AUTO — `if (estopped || lowVoltage) { actualPwm = 0; ... }`. One glitched
   INA reading hard-stopped a hand-driven locomotive with no ramp at all.

And if `ina219.begin()` failed, `inaReady` stayed false, `serviceIna()` returned
forever, and nothing anywhere said so: no warning, no status field, not even a
serial line. A loose I²C wire turned "protected" into "unprotected" invisibly,
for a whole session.

## Decision

1. **The policy is the profile's.** Trip below `SHUTDOWN_VOLTAGE` after
   `VOLTAGE_COUNTER_LIMIT` consecutive readings; clear at `RECOVERY_VOLTAGE`;
   treat anything below `DISCONNECTED_VOLTAGE_THRESHOLD` as no pack rather than
   a flat one. Nothing about the battery is written in the sketch.
2. **Low voltage ramps; only e-stop cuts.** The operator ruled on 2026-08-29:

   > One strike/low battery. Steep ramp is informative. It signals that
   > something is wrong.

   So low voltage now requests `AUTO_STEP_DOWN_MS` — 31 ms per step, ~2.8 s
   from cruise. Steep, and unmistakably not a normal stop, but not a dead
   short, and the same in MANUAL. E-stop keeps its instant cut.
3. **Absent protection is announced.** If the INA219 is not found, the boot
   prints it in capitals, a retained warning says it, and `"ina":0` appears in
   every status line. A session with no battery protection is now a session
   that says so once a second.
4. **The throttle is refused while low voltage stands**, with a reason, rather
   than being accepted and then silently zeroed by the ramp service.

## Cost

Toby will now run down to 13.25 V where 0.1 stopped him at 14.4 V. That is not
a relaxation invented here; it is the number that was measured for this pack
and has governed every QUORUM season. If it is wrong, it is wrong in the
profile, which is where it can be changed once for every sketch.

## Verification

Gate 4 pins the interlocks (`admitThrottle` refuses on low voltage; `admitAuto`
and `admitGo` refuse on the safety interlock). The band, the counter and the
disconnected case are arithmetic over the profile's own constants.

## References

- `firmware/test-programs/NAVI_ONE/NAVI_ONE.ino` — `serviceIna()`, `serviceRamp()`
- `firmware/test-programs/NAVI_ONE/LL_LocoConfig_9950012.h`
- `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md` — Finding 10
- `docs/NAVI_ONE_NEXT.md` — the operator's ruling on stop ramps
