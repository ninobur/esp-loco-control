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
pre-existing Adafruit INA219 enum warnings appeared. The correction has not
been flashed as part of this analysis. The flashed image remains `dd495ab`
until a later upload is explicitly reported.
