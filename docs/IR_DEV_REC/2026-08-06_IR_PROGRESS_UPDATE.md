# IR wheel-sensor progress update

**Date:** 2026-08-06
**Status:** field direction established; production mount not yet built; no
locomotive control authority
**Production target:** 27.8 mm diameter, seven-spoke finescale steel wheel

## Executive conclusion

The reflective IR approach is viable enough to move from improvised foam to a
rigid production mount. The field work established four mechanical requirements:
keep the sensor consistently close to the wheel, control axle side-play/runout,
paint the sensor-facing spoke surfaces black, and shield both the sensor/wheel
gap and the view through the spokes. A durable version of the foam "diaper" plus
the outer "curtain" is the present best configuration.

This is not yet production acceptance. The sensor has not been integrated into
QUORUM, the production mount does not exist, and the speed-output quality gate in
`docs/IR_DEV_REQ/QUALITY_GATES_SPEED_OUTPUT.md` remains unimplemented. IR has no
propulsion, navigation, stopping, or distance authority.

## Fixed calibration

- Wheel diameter: **27.8 mm**.
- Nominal circumference: **87.34 mm** (`pi * 27.8`).
- Spokes/pulses per revolution: **7**.
- Nominal travel per pulse: **12.48 mm**.
- Before any speed figure is trusted, one hand-turned revolution must produce
  exactly seven pulses.
- Effective rolling circumference still requires a known-distance measurement
  on the final wheelset under load.

Both `IR_DIAG` and `IR_TEST` carry the seven-spoke/87.34 mm calibration. Old
10-spoke and 109/115 mm figures describe retired wheel configurations.

## What the August 6 field sequence established

### The wheel is optically usable

The finescale wheel produced all seven phase positions with uniform intervals in
the dark test. Outdoors, painting the inward-facing spoke surfaces black removed
the repeatable weak phase previously seen on phase 6. In the first painted-spoke
daylight windows, all seven phases had nearly equal sample counts and balanced
headrooms.

### Shielding materially helps

The initial directional comparison found the following lower-tail improvement:

| received-pulse measure | no foam | foam backing |
|---|---:|---:|
| pulses analysed | 1,121 | 751 |
| marginal pulses | 183 (16.3%) | 0 |
| rise headroom (`rh`) p10 | +63 | +137 |
| fall headroom (`fh`) p10 | +53 | +247 |
| pulses with `fh <= 50` | 107 | 0 |

That comparison was not speed-matched, and the no-foam total includes an
unresolved amount of stationary noise. It supports the shield direction but is
not a controlled acceptance test.

The later runs gave the more useful mechanical comparison. In the stable moving
segment from 17:07:51 through 17:10:58 of the mixed-session 16:53 log, with close,
repeatable sensor spacing and both shield elements present, 39 of 2,050 pulses
(**1.90%**) had `fh <= 50`; 32 (**1.56%**) had `fh <= 0`. Removing the middle
shield (the "diaper") while retaining the outer shield (the "curtain") produced
nearly identical results on two complete runs: **8.37%** and **8.31%** at
`fh <= 50`, including **6.19%** and **6.57%** at `fh <= 0`. Rise headroom stayed
strong. The repeat makes the conclusion more credible than a single
weather-correlated run: the diaper protects the limiting low-side transition,
while the curtain alone does not.

### Spacing and runout matter

The narrow-gauge prototype wheelset has substantial runout and side-play. A spacer
made the sensor distance repeatable; reversing it moved the sensor closer to the
spokes and improved the useful geometry. The final mount must positively locate
the sensor and control axle end movement. Foam tape is suitable for experiments,
not for holding that tolerance through heat, vibration, grit, and maintenance.

### Diagnostic MQTT gaps do not describe the production path

One daylight log stopped receiving IR diagnostic records while its last received
pulses remained valid and the locomotive continued moving. That is a diagnostic
publication-path failure, not evidence of an optical stop. The production sensor
will share the locomotive ESP32 and its speed calculation must be local; MQTT may
publish summaries but may not sit in the measurement or control path.

## Best-known production geometry

The production mount should provide:

1. A rigid truck-mounted sensor carrier with adjustable but lockable standoff.
2. Positive wheelset spacing/end-float control so the target cannot strike or
   escape the optical sweet spot.
3. Black, matte sensor-facing spoke surfaces.
4. A durable inner shroud replacing the foam "diaper."
5. A durable outer backing shield replacing the foam "curtain."
6. Clearance and a drain/weep path for water and garden grit.
7. IR-opaque material or lining; black plastic must not be assumed opaque at the
   sensor wavelength.

The shroud need not reproduce every piece of experimental foam. It must reproduce
the optical functions: block the direct side view into ambient light and give the
inter-spoke aperture a controlled dark background.

## Firmware status and remaining gate

`IR_DIAG` now reports the decision-relevant headrooms (`rh` and `fh`), excludes an
invalid preceding-gap trough rather than printing a false zero, restarts phase
alignment after detected misses, and orders contrast-discard epochs with the
event stream. Those changes make the diagnostic evidence substantially more
honest.

The remaining production defect is structural: `MARGINAL` or `UNAVAILABLE`
quality must suppress the speed number. A downstream consumer must receive
`speed_valid:false` with null/absent speed, never a plausible number accompanied
by a warning label. Detection must continue so the envelope can recover; only the
answer is withheld.

## Next work

1. Design and build the rigid mount with both shroud functions and fixed close
   spacing.
2. Perform the seven-pulses-per-revolution hand check.
3. Repeat a complete bright-sun/shade circuit using local/USB capture, retaining
   pulse count, availability, `rh`/`fh`, phase balance, misses, and latches.
4. Implement and replay-test the speed-output quality gate.
5. After the mount and output gate pass, integrate IR read-only on the locomotive
   ESP with value, validity, freshness, source, and pulse evidence. Give it no
   motor authority in that first integration.
6. Consider creep-speed and distance-stop authority only after read-only agreement
   with independent Hall/motion evidence is demonstrated.

## Evidence

Primary daylight captures used during the session:

- `ir_daylight_20260806_124221.log`
- `ir_daylight_foam_20260806_134824.log`
- `ir_daylight_foam_20260806_151226.log`
- `ir_daylight_foam_20260806_163544.log`
- `ir_daylight_foam_20260806_165318.log`
- `ir_daylight_foam_20260806_171537.log`
- `ir_daylight_foam_20260806_172612.log`

Cross-references:

- `docs/IR_DEV_REQ/QUALITY_GATES_SPEED_OUTPUT.md`
- `docs/IR_SENSOR_NOTES.md`
- decisions 0005-0010
