# The Pi's power supply, not its SD cards — root cause, 2026-08-13

**Verdict: a failing 5 V supply destroyed two filesystems in 36 hours. Neither
SD card was faulty. Replacing the supply fixed it; both cards work.**

> **Companion record.** `field-records/20260813_PI_READONLY_UNDERVOLTAGE.md` was
> written independently in a concurrent session and reached the same conclusion
> from the 01:27 failure alone — worth reading as corroboration arrived at
> separately. It stops at the diagnosis. This record adds the second card, the
> fresh-boot measurement that removed the remaining doubt, the verification
> after the supply was replaced, and the wrong turns taken along the way.

---

## The decisive measurement

A fresh boot, a bare machine — no USB devices attached at all — on the card that
had been written off as dead:

```
[  16.58]  hwmon hwmon1: Undervoltage detected!
[ 152.51]  I/O error, dev mmcblk0, sector 13648134 op 0x1:(WRITE)
[ 153.99]  Aborting journal on device mmcblk0p2-8
[ 155.72]  EXT4-fs (mmcblk0p2): Remounting filesystem read-only
```

Undervoltage at 16 seconds. Filesystem dead at 156 seconds.

After swapping to a Geekworm 20 W / 5 V 4 A supply, same Pi, same card:

| | before | after |
|---|---|---|
| `vcgencmd get_throttled` | `0x50005` | **`0x0`** |
| undervoltage events per boot | 1, at t+16s | **0** |
| root filesystem | read-only within 156 s | **`rw,noatime`**, writable |
| `emergency_ro` in mount flags | present | **cleared by fsck** |
| `ngr-runlog` | dead | **enabled, active, recording** |

`0x50005` decodes as bit 0 (under-voltage now), bit 2 (throttled now), bit 16
(under-voltage since boot), bit 18 (throttled since boot). Temperature was 35-37 °C
throughout, so thermal throttling was never in it.

## Two cards, one fault

The two "failed" cards are unrelated hardware, five months apart in manufacture:

| | card A | card B |
|---|---|---|
| model | SE32G, 32 GB | SR128, 128 GB |
| serial | `0x5c9886c9` | `0x4b94d59f` |
| manufactured | 12/2025 | 04/2026 |
| survived | 156 seconds | ~11 hours |

Both failed by **I/O timeout on cache flush** (`mmc0: error -110`), which is a
card not answering — the signature of interrupted power, not of wear or bad
blocks. Card B is the card now running the railway, unmodified, repaired by its
own boot-time fsck.

## Why this took three days to find

Each step was locally reasonable and collectively wrong.

**2026-08-11.** First filesystem failure. Diagnosed as a bad card. The card was
replaced and the Pi rebuilt.

**2026-08-12 14:49.** After the rebuild, `throttled=0x0` was measured and
recorded: *"Power was clean. The undervoltage hypothesis is disproved for this
failure; the card was the cause, not the symptom."* **That reading was true when
taken.** The error was treating a point measurement as a permanent property. A
supply that degrades partway through a day reads clean in the morning and kills
a card at night.

**2026-08-12 15:04.** `ngr-runlog` enabled — the card began being written
continuously for the first time. This did not cause the failures, but it changed
the exposure: a power dip against an idle filesystem does nothing, and the same
dip against one being written every second aborts the journal. It is why six
prior quiet months were not evidence the hardware was sound.

**2026-08-13 01:27.** Second failure, on the new card.

**Wrong turns taken during diagnosis**, recorded so they are not repeated:

- Blamed the SD card for the 2026-08-12 marker loss. That was the MacBook
  idle-sleeping — already established in decision 0026 — and the claim was
  repeated anyway without re-reading the record.
- Advised abandoning the Pi capture for a Mac-side capture, when `ngr-runlog`
  was installed, enabled and working. The operator corrected this.
- Blamed the two ESP32s on the Pi's USB rail. They were real load and worth
  removing, but the fault persisted with nothing attached.
- Blamed a damaged SD socket after cards appeared to sit at an angle. The
  operator identified the actual reason: the card was being inserted upside
  down.
- Asserted "the Pi is being browned out as we speak" from a `throttled` reading
  taken 21 hours after the event, when the sticky bits could not be told apart
  from live ones without a reboot.

The measurement that settled it — `get_throttled` on a freshly booted, bare
machine — was available from the first hour.

## Corrections to the record

- The 2026-08-11 "genuinely bad card" verdict is **withdrawn**. That card boots.
- Decision 0027's premise that the card was at fault is superseded in that
  respect; its conclusion about where logging belongs is unaffected.
- Nothing here changes decision 0026 (the Mac sleeping) or 0028 (continuous
  recording). Both stand.

## What is now in place

- **Supply:** Geekworm 20 W 5 V 4 A, wall socket, captive cable, no hub.
- **USB:** nothing attached. Neither the dispatcher nor the runlog needs an
  ESP32 — both are Python on the Pi. The two boards were load, not function.
- **Nightly retrieval:** `tools/fetch_pi_telemetry.sh` via launchd at 00:00,
  pulling to `~/ngr-telemetry` outside the git working tree. It records card
  serial, `throttled`, mount flags and runlog state beside the data, and warns
  on read-only root, `throttled != 0x0`, or an inactive runlog. Verified against
  the live Pi: `OK 192.168.68.142 — 46 file(s) within 3d, archive now 54M`.
- **Gap markers work.** This boot recorded `# DISCONNECT` then `# RECONNECT`,
  so the silence is timestamped rather than inferred.

## Data

Nothing was lost. 58 MB was rescued from the read-only card before any recovery
was attempted — `ngr_app.py`, `ngr_runlog.py`, both systemd units, and the
complete `all_20260812.log`. Nothing had been written to the card after it went
read-only, so the rescue was complete by construction.

## References

- `docs/decisions/0029-*` — clear the supply before diagnosing storage
- `docs/decisions/0026-*` — the Mac sleeping; unaffected
- `docs/decisions/0028-*` — continuous recording; unaffected
- `tools/fetch_pi_telemetry.sh`, `tools/com.ngr.fetch-telemetry.plist`
