# 2026-08-13 — ngr-pi went read-only; under-voltage, not the SD card

**Second "SD card failure" in two days. The evidence says it is the power,
not the cards.** Recorded because the obvious response — fit a third card —
would likely reproduce this within a day.

## What happened

Found while trying to deploy the dashboard: `scp` failed with
`dest open … Failure` on a filesystem with 106 GB free. The root filesystem
had remounted read-only.

Kernel log, `mmcblk0` = SanDisk SR128, manufactured 04/2026, in service since
the rebuild at 14:38 on 2026-08-12 — under eleven hours:

```
Aug 13 01:27:32  mmc0: error -110 writing Cache Flush bit
Aug 13 01:27:32  I/O error, dev mmcblk0, sector 5630552 op 0x1:(WRITE)
Aug 13 01:27:32  Aborting journal on device mmcblk0p2-8
Aug 13 01:27:32  JBD2: I/O error when updating journal superblock
Aug 13 01:32:30  EXT4-fs error: Detected aborted journal
Aug 13 01:32:30  EXT4-fs: I/O error while writing superblock
Aug 13 01:32:30  EXT4-fs: Remounting filesystem read-only
```

At the same time:

```
vcgencmd get_throttled  ->  0x50005
```

Decoded: bit 0 **under-voltage NOW**, bit 2 **currently throttled**, bit 16
under-voltage has occurred, bit 18 throttling has occurred.

## Reading

`error -110` is ETIMEDOUT — the card stopped answering writes. Under-voltage
produces exactly this signature on a Pi: the card browns out mid-write, the
journal cannot be updated, and ext4 protects itself by going read-only. The
card was eleven hours old and is not obviously at fault.

The unit note in `server/ngr-app.service` records that the previous card also
"failed", on 2026-08-12. Two cards in two days on one host, with under-voltage
flagged, points at the supply, the cable, or something drawing off the Pi —
not at the cards.

## Cost

`ngr-runlog` stayed *running* and could not write for roughly ten hours,
throwing `[Errno 30] Read-only file system` per message:

```
Aug 13 11:18:57 [ngr_runlog] on_message error: [Errno 30] Read-only file
  system: '/home/david/NGR/telemetry/runs/9950012_20260813_111857.log'
```

**The morning's session is not in the runlog** — including the Toby AUTO
problem that produced the v1.11.1 findings. The dashboard itself was
unaffected throughout: it serves from RAM and never writes.

## Open

- **What is powering this Pi?** Untested as of writing. A Pi 5 wants 5 V/5 A;
  a marginal supply or a long thin USB-C cable will do this under load.
- After the reboot at ~12:0x, `get_throttled` read `0x0` — clean, but that is
  five minutes of evidence against a card that lasted eleven hours. Worth
  re-checking after a few hours of running:

```bash
ssh david@192.168.68.142 'vcgencmd get_throttled; mount | grep " / "'
```

- A service that keeps running while unable to write is its own finding.
  `ngr_runlog` logs the error per message and carries on; nothing surfaces
  on the dashboard. A locomotive can be run all morning believing it is
  being recorded when it is not.
