# Otto EWO field test: reboot at the first Grillers approach event

Source: Pi run log `/home/david/NGR/telemetry/runs/9950011_20261005_121329.log`,
read on 2026-10-05. The two short CW attempts occurred in this one log after
the `dd495ab` flash. This is a read-only log diagnosis; no physical run was
started for this analysis.

| Attempt | Last normal report | Pi reconnect | Result |
| --- | --- | --- | --- |
| 1 | 12:24:55.278, MM052→053 CW, PWM 90, AUTO 1, IR ready | 12:24:58.255; new boot ID `3933673DE0606FF3` | PWM 0, position unset, AUTO 0 |
| 2 | 12:26:45.716, MM052→053 CW, PWM 90, AUTO 1, IR ready | 12:26:48.386; new boot ID `FA48A06E6DF5A7B2` | PWM 0, position unset, AUTO 0 |

The `online 0`/`online 1` pair and changed boot IDs establish that Otto
rebooted. Neither last normal report showed an e-stop, low voltage, IR fault,
station brake event, or PWM-down command. The Pi log does not contain a serial
panic trace or ESP32 reset reason, so it cannot by itself establish the reset
cause.

Source review found a deterministic defect that fits both failures. Grillers
is centred at MM063, so CW MM053 is Station −10 and creates the first `ARMED`
station event. In `publishStationEvent()`, the `snprintf` format expected
`brake_phase` as `%s` before numeric `off` as `%d`, but `dd495ab` passed the
numeric offset first. At Station −10, `%s` therefore receives an invalid
pointer derived from `-10`, which can crash the ESP32 before the event is
published. The absent station event in the Pi log is consistent with that path.

The correction swaps those two arguments. The full host suite passed, and the
ESP32 sketch compiled with `--warnings all` and `-Werror=format`; only three
pre-existing Adafruit INA219 enum warnings appeared. The correction had not
been flashed at the end of this analysis; the subsequent upload is recorded
below.

## Corrected flash follow-up

David subsequently requested the corrected sketch be flashed. The checkout was
clean at `4721b64c60f8e351d4a11096977815d70adfc96f`. Before upload, Pi
telemetry showed PWM 0 and AUTO off. The connected ESP32 MAC was
`68:09:47:ac:4e:b4`, Otto's recorded identity. A fresh ESP32 3.3.12 build
passed with `--warnings all` and `-Werror=format`; the application binary SHA-256
was `64397458f8ef60b0743261f9ba81c511c22f7cf4ad840582ce7858c5a046136e`.

`arduino-cli upload` wrote the bootloader, partition table, boot application
selector, and corrected application at 115200 baud. The uploader verified the
hash of each written region. The Pi log
`/home/david/NGR/telemetry/runs/9950011_20261005_122844.log` reported a new
boot at 12:38:32.606 with boot ID `07C32E3D48818352`; post-flash telemetry
showed PWM 0 and AUTO off. No movement or station approach was tested during
this flash. The IR source was absent in the post-flash idle status snapshot.

## Subsequent short run: AUTO withdrawal, not another reboot

The subsequent Otto CW run in the same Pi log used corrected source `4721b64`
and boot ID `C7707E90EC82EC78` from 12:40:46.790. This run reached MM052
at 12:41:30.841 and MM053 at 12:41:32.057. Both were ruled `MISSED_MAGNET`;
nevertheless, NAVI reported `position_reliable=1`, and the MM053 report had
applicable IR at 383 completed pulses and measured speed 47.901 pKPH.

At 12:41:32.058, the first Grillers approach event was
`STATION_ZERO_HALL_REQUIRED` with `off=-10`, `brake_phase=WAITING`, PWM 90,
`reference_ir_um=0`, and `ir_unavailable=0`. AUTO withdrew immediately. The
ordinary withdrawal ramp then took PWM to 85 by 12:41:32.203, 52 by
12:41:33.194, 20 by 12:41:34.204, and 0 by 12:41:35.201, still at MM053.
There was no reboot at this failure point; the corrected station-event
formatter published valid telemetry. This is a separate control-flow defect
from the earlier `snprintf` crash.

Source inspection shows that `StationMachine` discovers Grillers from NAVI's
MM053 position, but `EwoStationStopProfile` remained in `WAITING` unless the
−10 marker had an accepted Hall event. The missed MM053 therefore caused an
inaccurate Station 0 Hall warning and AUTO withdrawal before approach braking.
The follow-up source correction permits the −10 approach to begin from valid
NAVI position plus valid IR distance/speed; it does **not** treat the missed
marker as accepted Hall or let the final brake start without accepted Station 0
Hall. It also makes missing Station 0 Hall while already in approach/hold a
clear withdrawal, and fixes CCW surveyed approach-distance accumulation.
Host tests and ESP32 compilation passed. This follow-up correction has **not**
been flashed or field tested.
