# Toby — QUORUM 1.13 flash record, 2026-08-12

**Locomotive:** Toby (9950012)
**From:** `QUORUM_1_6`  →  **To:** `QUORUM_1_13`
**Source tree:** `dashboard-redesign` @ `a3c312f`; `QUORUM.ino` last modified by
`f084d9d`. Verified byte-identical to the tree Otto's 1.13 image was built from
(`git diff agent/phantom-verdict-20260812 HEAD -- firmware/QUORUM/QUORUM.ino`
empty), so both locomotives now run the same navigator.

## Pre-flight

| check | result |
|---|---|
| Sketch version | `SKETCH_NAME "QUORUM_1_13"` |
| Advisory present | `quorumAdvisoryMarker()` / `advisoryAllowed` found in tree |
| Otto's state | `online 0`, retained bootid `QUORUM_1_13` — off, not disturbed |
| Toby's state before | `online 1`, retained bootid `QUORUM_1_6` |
| Port | `/dev/cu.usbserial-10` (Otto was flashed on `usbserial-0001`) |
| Identity confirmed | operator: "Toby is connected. 9950012" |

### Defect found and corrected before building

`firmware/QUORUM/LocoConfig.h` carried a header saying **`TARGET: Toby
(9950012)`** while the active include was **`LL_LocoConfig_9950011.h` (Otto)**.
Building as found would have flashed Toby with Otto's identity, putting two
locomotives on the same MQTT topics and Otto's calibration on Toby's hardware.

Corrected to `LL_LocoConfig_9950012.h`, and the header comment rewritten to state
the boot-line check and to record that this exact trap occurred. Backup of the
original at `/tmp/LocoConfig.h.bak_20260812`.

**This file is a live selector that must be edited per flash and cannot be
trusted from its comments.** Read the active `#include`, not the header.

## Build

```
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all
```

Core 3.3.11. Clean apart from the pre-existing `-Wvolatile` deprecation warnings
on `actualPwm++`, `pubWindowCount++` and `cmdDrops++` — unchanged from prior
builds, no new diagnostics.

```
Sketch uses 983171 bytes (75%) of program storage space.
Global variables use 52468 bytes (16%) of dynamic memory.
```

## Upload

```
arduino-cli upload -p /dev/cu.usbserial-10 --fqbn esp32:esp32:esp32:UploadSpeed=115200
```

```
Wrote 983312 bytes (642563 compressed) at 0x00010000 in 63.1 seconds
Verifying written data...
Hash of data verified.
```

## Post-flight verification

Retained bootid after reboot:

```json
{"sketch":"QUORUM_1_13","loco":"9950012","deadband":25,"entry_margin":13,
 "min_peak":35,"floor_ms":40,"baseline":"median_128_at_500ms",
 "quorum_trigger":3,"quorum_margin":2,"quorum_max":12}
```

**`loco` reads 9950012** — the correct profile compiled in.

Liveness sampled three times rather than once, per the 2026-08-11 lesson that a
single uptime reading cannot distinguish a running locomotive from a frozen
retained message:

| sample | uptime_ms | nav | auto | agree | disagree |
|---|---|---|---|---|---|
| 1 | 42533 | UNSET | 0 | 0 | 0 |
| 2 | 47533 | UNSET | 0 | 0 | 0 |
| 3 | 52533 | UNSET | 0 | 0 | 0 |

Uptime advancing at the publish interval; counters zeroed; `nav` UNSET awaiting a
declaration. Healthy fresh boot.

## What Toby gains over 1.6

- The **exact-or-silent HARD_BOUND advisory** (decision 0023), published on
  `ngr/loco/9950012/mm/no_quorum` with the `adv`/`advw`/`advr`/`advn` audit
  fields. Exercised successfully on Otto 2026-08-12.
- All navigator changes between 1.6 and 1.13. Toby has been the *control*
  locomotive through the phantom investigation precisely because it was on 1.6;
  **that control is now spent.** Cross-locomotive comparisons against Toby-on-1.6
  captures remain valid as history but cannot be repeated.

## Profile differences worth knowing during the test

`HALL_POLARITY_INVERTED` is `true` for Otto and `false` for Toby, and **this is
dead config with no effect**. Per `docs/CLAUDE.md`, the Hall sensors are mounted
identically on both locomotives (operator, 2026-08-04) and **no firmware reads
the symbol** — QUORUM derives polarity solely from which threshold was crossed
(`evOpenPole = (raw >= northEnter) ? 1 : 0`).

**Correction, 2026-08-13:** an earlier revision of this record claimed the two
locomotives read the field with opposite sign convention and that raw marker
streams were not directly comparable. That was wrong, and it is the same
speculation `CLAUDE.md` warns this symbol has already generated once. The
streams are directly comparable.

Toby's Hall profile is the measured original (NGR Hall Probe, 2026-06-26): noise
mean 4.2 counts, observed max 19, weakest real magnet 39. Otto's thresholds were
later copied *from* Toby's.

## Not done

- **No track test run.** This record covers the flash only.
- **mm 99 is still installed inverted** (see the 2026-08-12 phantom verdict).
  Expect one disagreement per lap at mm 99 on any CCW run, on either locomotive,
  until that disk is flipped. It is not a firmware fault.
- The magnets at mm 99–102 and 61–63 are the new disks; 149/150 corrected
  earlier. Any *other* phantom is a pointer to a doubled magnet — dig.

## References

- `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md`
- `field-records/20260811_QUORUM_1_13_flash_and_test1.md` — Otto's flash
- `docs/decisions/0023-*` — the advisory Toby now carries
