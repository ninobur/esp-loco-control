# 2026-08-30 — the Pi went down mid-session, and where it stands

## What happened

`NAVI_ONE_0_3` was flashed to Toby and he was running. Minutes later the
dashboard stopped loading, the packet log stopped streaming, and `online` stayed
green on a stale retained value.

The Pi was off the network completely — no ICMP, no SSH, no 1883, no 8080, on
both `.142` and the `.73` DHCP fallback. The Mac's own LAN and internet were
fine throughout, so this was the Pi, not the network and not the firmware. A
locomotive's MQTT client cannot take down sshd and the NIC.

Power-cycled. It did not come back. LEDs: **PWR solid red, ACT green blinking
only briefly and occasionally** — not the heavy irregular flicker of a boot.

## What the evidence says so far

**The card is NOT dead.** Read on the Mac: partition table intact, `bootfs`
mounted, and every boot file reads cleanly — `start4.elf`, `fixup4.dat`,
`kernel8.img`, all device trees. `config.txt` is stock with **no overclock**
(`arm_boost=1` is the Pi 4 default, not an overclock). A failing card throws
read errors right there and this one throws none.

So the damage is on the ext4 root, which macOS cannot mount. Most likely
filesystem corruption rather than media failure. `cmdline.txt` carries
`fsck.repair=yes`; when the journal is too far gone fsck drops to an emergency
shell instead, which on a headless Pi looks exactly like these LEDs.

**The morning health report was clean**: `throttled=0x0` after 5 days up, disk
9%, runlog active. That register is sticky since boot, so it rules out
undervoltage up to 07:00 — but the failure came after, so it does not clear the
supply for today.

## The PSU theory, and the correction to it

The 2026-08-12 runbook warns that a marginal supply produces this exact
signature with the card as symptom, and predicts a replacement card failing
"inside a week". This one lasted 18 days, which looked like confirmation.

The operator uses a genuine Raspberry Pi supply, which lowers that prior a long
way, and it was over-weighted in the first assessment. The better explanation
for two cards in 18 days is that **decision 0027 removed only part of the write
load**: it stopped our telemetry logging, but journald, syslog, mosquitto's
persistence file and a full desktop install still write to the card
continuously. Every hard power-cut then corrupts ext4 mid-write.

## The proposed fix, not yet done

**Read-only root via overlayfs** — `raspi-config` → Performance Options →
Overlay File System. Writes go to RAM and vanish on reboot, so card wear stops
and power-cutting becomes harmless. The Pi serves and brokers and nothing else,
which is exactly what 0027 says it is for, so it has no reason to write. Costs
nothing. Editing the app means toggling overlay off, changing it, toggling back.

If a permanent move is wanted instead: boot from USB SSD, which Pi 4 and 5 both
support natively.

## Next steps, in order

1. **Monitor on the Pi, card back in, power up.** `fsck.repair=yes` may simply
   fix it. If not, the screen says why in words. Cheapest test by far.
2. **Do not put Raspberry Pi Imager anywhere near this card.** Imager only
   writes; it would wipe the evidence and the root filesystem.
3. If a deeper look is wanted, the ext4 superblock carries error count, first
   and last error with timestamps and kernel function, and lifetime kilobytes
   written — the last of which settles the wear question outright:
   ```
   sudo dd if=/dev/rdisk5s2 bs=1024 skip=1 count=1 of=/tmp/sb.bin
   ```
   Raw disk reads need root, which is why this is an operator step.

## Recovery materials

`docs/PI_REBUILD_RUNBOOK.md`, `tools/provision_pi.sh` and
`server/ngr-app.service` existed only on `agent/phantom-verdict-20260812`
(commit d83452a) and are now on this branch. The dashboard app is safe at
`server/ngr_app_v1_11_2.py`, including both of yesterday's edits. Telemetry is
mirrored to `~/ngr-telemetry`, 3.1 GB, current to 07:00 today.

## Toby

Running `NAVI_ONE_0_3`, flashed successfully, and **still without a field test** —
the Pi died before any of it could be observed. Nothing about 0.3 has been
watched on the dashboard. He needs nothing and will reconnect on his own once a
broker exists.
