# Otto flashed with NAVI_RVM_SHAPE_MANUAL: 2026-10-18

Flashed over USB at David's request ("When available, please flash Otto via
USB"). The candidate is not field accepted.

## What was flashed

| | |
|---|---|
| Sketch | `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/NAVI_RVM_SHAPE_MANUAL.ino` |
| Branch / commit | `dev/navi-rvm-shape-manual-20261018` @ `30271a6` |
| Credentials | `firmware/config/credentials.h` (private, git-ignored) |
| Build | arduino-cli, esp32:esp32 3.3.12, FQBN `esp32:esp32:esp32`; 982,103 B flash, 121,292 B static RAM |
| Port | `/dev/cu.wchusbserial110` (CH340); Otto's MAC is 68:09:47:ac:4e:b4 |
| Upload | 115200 baud ("Hash of data verified") |

**Upload speed.** The CH340 corrupted flash reads at 460800 and 230400 baud.
Use 115200 with this adapter.

## Rollback

The image Otto ran before this flash was **NAVI_EWO_0_6** (strings `NAVI_EWO_0_6`,
`EWO_9950011`, loco id 9950011). The full 4 MB flash image is saved at:

`~/NGR/NGR-Files/otto-flash-backups/otto_before_NAVI_RVM_SHAPE_MANUAL_30271a6.bin`

sha256 `53cb9275b4756c6edc3f4185098e6d7224ea6d7dc09deea0be6ee76210dfc354`

To restore it byte for byte:

```sh
~/Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool --chip esp32 \
  --port /dev/cu.wchusbserial110 --baud 115200 write-flash 0 \
  ~/NGR/NGR-Files/otto-flash-backups/otto_before_NAVI_RVM_SHAPE_MANUAL_30271a6.bin
```

## Verification after flashing

Read from the MQTT broker, read-only. Serial output could not be captured
through this adapter.

| Topic | Content |
|---|---|
| `ngr/loco/9950011/state/bootid` | `"sketch":"NAVI_RVM_SHAPE_MANUAL"`, `"class":"MANUAL_OBSERVATION_ONLY_NOT_FIELD_ACCEPTED"`, `"model":"330cdc007da3445b"`, `"navigation":"OBSERVATION_ONLY"`, `"auto":"ABSENT"` |
| `.../online` | `1` |
| `.../state/nav` | `"state":"UNSET"` (awaiting a declaration), `"waveform_paused":1` (PWM 0) |
| `.../state/loopstat` | 1 Hz, `block_q_drop 0`, `pub_drop 0`, `cmd_drop 0` |

## Observations requiring attention before running

1. **`state/warning`: "INA219 NOT FOUND — no battery protection this session".**
   The sketch looks for the INA219 only at boot. Low-voltage protection is
   **inactive** until Otto reboots with the INA219 present. This may be because
   Otto was on USB power on the bench; check it with track power before running.
2. **Free heap is low.** About 26–28 KB free at idle, and a lowest value of
   **8,816 B** since boot (`heap_free`, `heap_min` in `state/loopstat`).
   - No failure was seen. A long run with Wi-Fi reconnects could exhaust it.
   - The largest heap users that could be reduced are the MQTT publish queue
     (24 × 773 B) and the 8 KB hold-back buffer.
   - Watch `heap_min` in the first field session.
3. **Pi recording loss at boot.** At boot: `nsr_native_drop 102`,
   `nsr_udp_fail 11`, `nsr_hall_drop 1` (Pi recording only, while Wi-Fi
   connected). Watch whether these keep rising in operation.
