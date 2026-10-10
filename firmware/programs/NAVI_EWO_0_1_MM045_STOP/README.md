# NAVI_EWO_0_1_MM045_STOP

Open **[NAVI_EWO_0_1_MM045_STOP.ino](NAVI_EWO_0_1_MM045_STOP.ino)** in Arduino IDE.
The folder, filename and reported sketch name are identical.

**Status:** host-tested, built for Otto 9950011, independently reviewed.
Track calibration remains pending. First CW trials are operator-reported;
see the [field observations](../../../field-records/20261009_MM045_CW_FIRST_TWO_STOPS.md).
The running image/device is not independently verified. Not merged or field accepted.
The independent review predates the later restart-rate correction.

## What it does

Under the existing AUTO/GO controls, ordinary cruise is PWM 90 with the existing
geographic grade settings. A repeating eleven-MM STOP profile approaches the
same point from either direction: 150 mm CW from MM045, midway to MM046.
It holds five MM sections at PWM 50, uses the settled penultimate reduction,
then ramps continuously to zero at **310 ms per PWM count** (initial track
calibration; about 9.3 seconds from PWM30). Ordinary AUTO acceleration and
STOP restart both use **150 ms per PWM count**. Five seconds at applied PWM zero releases
back to cruise through the existing actuator at **150 ms per PWM count**
for restart (approximately 13.5 seconds from zero to PWM 90). It repeats on the next
circuit.

Established IR location remains valid on entry and STOP/GO. Only a new start or
reposition uses operator declaration truth until the first MM contact. This
trial implements no CTO/CE negotiation or Four-Station Local service.

The [implementation report](../../../docs/NAVI_MM045_STOP_IMPLEMENTATION_20261009.md)
contains the complete profile, governing decisions, calibration limits and
rollback points. The [previous README](HISTORY.md) is retained separately as
historical evidence; its older station descriptions are not this trial.

## Local location

In Finder open **davidbrown → esp-loco-control → CURRENT → firmware → programs →
NAVI_EWO_0_1_MM045_STOP**. This is a complete review checkout on branch
`codex/navi-cruise-stop-20261009`, associated with
[PR #9](https://github.com/ninobur/esp-loco-control/pull/9).

## Build requirements

Keep the repository together: this sketch includes shared configuration, map
and navigation headers from sibling directories. Copying only this directory
or only the `.ino` is not sufficient.

- ESP32 Arduino core **3.3.12** was used for validation; board `esp32:esp32:esp32`.
- Selected profile: **Otto 9950011**, from the existing NAVI_ONE_X22 configuration.
- Libraries: PubSubClient and Adafruit INA219 with their dependencies.
- A local, untracked `credentials.h` is required. The pattern is
  [credentials_template.h](../../config/credentials_template.h). Verification
  builds use placeholders outside the repository; they are not deployment images.
- Opening or compiling the sketch does not flash hardware. Deployment requires
  David's separate authorization. The recorded adapter upload speed is 115200.

## Host checks

Run from the repository root:

```sh
sh firmware/programs/NAVI_EWO_0_1_MM045_STOP/run_tests.sh
python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration
```

The broader pulse-integration and dashboard test fixtures each have one
pre-existing failure, reproduced on the pre-rename source. The
[cleanup validation record](../../../docs/NAVI_MM045_STOP_IMPLEMENTATION_20261009.md#naming-cleanup-validation)
documents the exact failures; those fixtures were not changed beyond paths.

The rename preserves the reviewed firmware source from `f40e388b5fcfa5d6be8cba8aab6dc02d07a21312`
byte for byte; test consumers now use the matching path. Git history retains the
former name `NAVI_EYES_WIDE_OPEN_INTEGRATED`.
