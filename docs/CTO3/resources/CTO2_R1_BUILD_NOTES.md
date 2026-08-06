# CTO 2 r1 — Build Notes

Source base: `NGR_LL_DNA_AUTO_4Station_Goldcore_for_CTO2.ino`

## Preserved Goldcore behavior

- Four stations stop every lap.
- Station targets: 20 pKPH through center, 15 at M+1, 10 at M+2, then the established 300 ms/PWM final ramp.
- Eight-second standard dwell.
- Existing proven two-magnet departure-proving ramp.
- No legacy greater-than-45 pKPH outer stopping branch.

## Added CTO 2 r1 behavior

- ESP-NOW locomotive peer status, sent after magnets, station state changes, role changes, and at 500 ms intervals.
- Role is calculated from circular DNA distance and active direction; no locomotive-specific lead assignment.
- Solo mode remains full all-station Goldcore service.
- Trailing train uses an upstream translated Goldcore HOLD stop, nominally M-5, but the final-ramp target is moved earlier when the lead’s last confirmed symmetric +/-4-MM protection interval requires it.
- Lead platform dwell begins only after the trailing locomotive reports its physical hold stop.
- A held trailing locomotive waits for lead movement and an available 8-MM Hall-to-Hall gap before restarting toward its platform stop.
- Pair memory persists through a short radio silence, so a trailing locomotive continues to honor the last known lead position instead of converting to solo behavior.

## Flashing

The sketch remains configuration-generic through `LOCO_ID` / `LOCO_NAME`. Before flashing each locomotive, select its existing profile in `LocoConfig.h` (`LL_LocoConfig_9950011.h` for Otto or `LL_LocoConfig_9950012.h` for Toby).

## Validation performed

This file passed C++ syntax validation using Arduino/ESP32-compatible interface stubs. It has not been compiled in the Arduino IDE against the installed board package and has not been tested on track.
