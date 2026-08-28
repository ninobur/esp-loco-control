# Toby — QUORUM 1.13 flash record (downgrade from 1.16R), 2026-08-28

**Locomotive:** Toby (9950012)
**From:** `QUORUM_1_16R_IR_TEST_A`  →  **To:** `QUORUM_1_13`
**Source:** commit `f1e6978` — the exact tree Toby's original 1.13 was built
from on 2026-08-13. Extracted to a scratchpad build directory; the live working
tree was not checked out and not modified.
**Operator authorization:** explicit ("upload").

## Why the source was taken from a commit rather than the working tree

The working tree is `agent/toby-1-13-flash` at `3602ad3`, whose `QUORUM.ino`
declares `SKETCH_NAME "QUORUM_1_16R_IR_TEST_A"`. There is no 1.13 in the tree to
build. `f1e6978` is the commit that recorded Toby's first 1.13 flash, so it is
the image with a field record already attached to it.

## Pre-flight

| check | result |
|---|---|
| Sketch version | `SKETCH_NAME "QUORUM_1_13"` — read from the extracted source |
| Active profile | `#include "LL_LocoConfig_9950012.h"  // Toby` — read from the `#include`, not the header comment |
| Port | `/dev/cu.usbserial-10` — the only USB serial device present; the same port Toby used on 2026-08-13 |
| Identity confirmed | operator: "the toby esp connected to the mac by usb" |
| `credentials.h` | gitignored, not in the commit; copied from the working tree |

The LocoConfig trap that fired on 2026-08-12 (header said Toby, active include
said Otto) did **not** recur: `f1e6978` is the commit that fixed it, so the Toby
profile was already active. Verified by reading the include line anyway.

## Build

```
arduino-cli compile --fqbn esp32:esp32:esp32
Sketch uses 983171 bytes (75%) of program storage space.
Global variables use 52468 bytes (16%) of dynamic memory.
```

**983171 bytes — identical to the 2026-08-13 record**, which is the strongest
available evidence that this is the same image and not a reconstruction.

## Upload

```
arduino-cli upload -p /dev/cu.usbserial-10 --fqbn esp32:esp32:esp32:UploadSpeed=115200
Wrote 983312 bytes (642562 compressed) at 0x00010000 in 63.3 seconds
Verifying written data...
Hash of data verified.
```

Uncompressed size matches 2026-08-13 exactly (983312). Compressed differs by one
byte (642562 vs 642563) — compressor variance, not an image difference.

## Post-flight verification

Retained bootid, `ngr/loco/9950012/state/bootid`:

```json
{"sketch":"QUORUM_1_13","loco":"9950012","deadband":25,"entry_margin":13,
 "min_peak":35,"floor_ms":40,"baseline":"median_128_at_500ms",
 "quorum_trigger":3,"quorum_margin":2,"quorum_max":12}
```

`sketch` reads **QUORUM_1_13** and `loco` reads **9950012** — right version, right
profile. `ngr/loco/9950012/online` = 1.

Liveness sampled three times per the 2026-08-11 lesson that one reading cannot
distinguish a running locomotive from a frozen retained message. Three
independent subscriptions to `state/loopstat`:

| sample | loop_max_gap_ms | hall_task_age_ms | nav | mm | pwm |
|---|---|---|---|---|---|
| 1 | 22 | 1 | UNSET | 0 | 0 |
| 2 | 22 | 1 | UNSET | 0 | 0 |
| 3 | 23 | 1 | UNSET | 0 | 0 |

`loop_max_gap_ms` advanced 22 → 23 across the samples, so these are live
publishes, not one retained message read three times. `nav` UNSET awaiting a
declaration, counters zeroed. Healthy fresh boot.

## OPEN — Hall sensor reads zero

`loopstat` reports `"baseline":0,"raw":0,"delta":0`.

Every historical capture in `field-records/logs/` shows a baseline in the
**1795–1903** range with `raw` tracking it within a few counts. Zero is not a
value that appears anywhere in the record.

The Hall task itself is running — `hall_task_max_gap_ms` 2, `hall_task_age_ms` 1
— so this is not a stalled task. The ADC is returning zero.

**Most likely benign:** the locomotive is on the bench powered from USB, and the
Hall sensor rail is fed from the track/motor supply, which is off. That would
produce exactly this.

**Not yet ruled out:** a disconnected or failed sensor, or a wiring disturbance
from handling the locomotive for the USB connection.

**This is not a flash fault** — the image verified by hash and the correct
profile booted. But it MUST be re-checked before the locomotive is trusted to
navigate. The check: put Toby on the track under normal power and read
`ngr/loco/9950012/state/loopstat`. `baseline` must land in the 1795–1903 band.
If it is still 0, the sensor is the fault and no run should start.

## What Toby gave up in this downgrade

- **Quarantine and NO_QUORUM self-resolution** (1.16 / 1.16R, decision 0035).
  Doubtful events are promoted into the record again rather than held and judged
  by their successor.
- **The ESP-NOW send callback and channel alarm** (1.16Ra). The tx counter again
  counts `esp_now_send()` returning ESP_OK — queued, not sent — with no send
  callback registered. The failure that fleet-stopped Toby on "peer STALE" on
  2026-08-15 becomes invisible again.
- **CTO peer coordination entirely** (1.14 and later). 1.13 predates it: no
  pairing, no leader/follower, no fleet stop by absence.
- **1.16Rb** pairing dissolution on nose-tail inversion (decision 0037), and
  **IR_TEST_A**.

If Toby is meant to run paired with Otto, that is now impossible. Solo running
is the intended use of this image — 1.14's own notes state solo behaviour was
meant to be identical to 1.13.

## Not done

- **No track test run.** This record covers the flash only.
- Otto was not touched and its state was not read.
- `firmware/QUORUM/LocoConfig.h` carries an uncommitted modification (Otto →
  Toby) that predates this job. It was **not** the source of the flashed image
  and is left as found.

## References

- `field-records/20260812_TOBY_QUORUM_1_13_flash.md` — the original 1.13 flash
- `field-records/20260811_QUORUM_1_13_flash_and_test1.md` — Otto's flash
- `docs/decisions/0023-*` — the exact-or-silent HARD_BOUND advisory 1.13 carries
- `docs/decisions/0024-*` — a request counter is not a measurement
