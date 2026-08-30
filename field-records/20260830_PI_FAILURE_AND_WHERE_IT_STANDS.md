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

---

# Resolution, same day

## It was never the card

The Pi boots to the desktop, on the same card. Everything is back: ping, SSH,
mosquitto on 1883, the dashboard on 8080. `ngr-app` and `mosquitto` both active.

The sequence that matters:

1. died mid-session
2. power-cycled → **would not boot**: PWR solid, ACT only occasional flicker
3. card pulled and read on the Mac → **perfect**, every boot file readable
4. card put back in → **boots normally**

Step 4 is the diagnosis. Reseating the card fixed it, which means the fault was
**contact, not media**. And that reframes 2026-08-12: that "card failure" was
also resolved by putting a *different card in the same socket* — which is also a
reseat. Two card failures in eighteen days may well be one intermittent SD
socket, and replacing the card the first time fixed it for the same reason
reseating fixed it today.

Consistent with everything observed: an SD card that loses contact under a
running kernel takes the whole machine down at once — SSH, MQTT and HTTP die
together because the filesystem is simply gone — and then will not boot until
the contact is remade.

The apparent network fault seen at the TV was the operator having unplugged the
Ethernet to move the Pi. Not a fault.

## There is no evidence, and that is by design

`journalctl --list-boots` shows only the current boot. `/var/log/journal` exists
but is empty. There is no `syslog`, no `kern.log`, no `messages`.

**This Pi keeps no logs at all.** That is decision 0027 working exactly as
written — the Pi serves and brokers and does not write to its own card, because
continuous telemetry writes are what was blamed for the first failure. Today is
the first time that trade has cost anything, and what it cost was the entire
post-mortem: there is no record of the midday failure and there never was one.

Worth revisiting, because the trade was made on a premise that now looks wrong.
If the fault is the socket rather than write wear, the Pi is paying for card
life it was never losing. A small capped persistent journal
(`SystemMaxUse=50M`) would have answered today's question outright.

## Not the PSU, and not undervoltage

`vcgencmd get_throttled` = `0x0`, and the 07:00 report had it at `0x0` after
five days up. The operator uses a genuine Raspberry Pi supply. The PSU theory
carried from the 2026-08-12 runbook was over-weighted and should be set aside
unless new evidence appears.

## Toby

`state/bootid` retained on the broker confirms the flash took:

```json
{"sketch":"NAVI_ONE_0_3","loco":"9950012","entry":38,"exit":25,"floor_ms":40,
 "amp_floor":0.34,"resid_ceil":0.13,"guard_ms":200,"seq_n":10,
 "offsets":0,"quorum":0,"velocity_model":0,"motion_gate":0,"ir_votes":0}
```

He is **not currently connected** — no live traffic in a 12 s subscribe. Powered
off, presumably. **0.3 still has no field test.**

One stale retained ghost to note: `ngr/loco/9950012/alert` holds a QUORUM-era
payload (`moving`, `lostm`, `lc_mm`, `losts` — none of which 0.3 publishes) at
`uptime_ms 88695`. NAVI_ONE publishes `alert` non-retained, so a live publish
will never clear it; it will be delivered to the console on every reconnect
until someone empties the topic deliberately. Not done — that is a change to
shared broker state and wants the operator's word.

## What is actually open

1. **The SD socket.** If it happens again, the fault is the socket and the fix
   is USB boot, which bypasses it entirely — and which is now a far
   better-motivated recommendation than it was as a wear remedy.
2. **Logging.** Decide whether to keep 0027 absolute or allow a capped journal.
3. **Overlayfs** — still a good idea for hard-power-cut safety, but no longer
   justified by the wear argument.
4. **Toby's field test of 0.3**, not yet begun.

---

# Second correction: it was the Ethernet, and the Pi is now wireless

## What actually happened

Mosquitto keeps its own log on disk, rotated back to 15 August. It shows the
broker **running and saving its database every 30 minutes** — 11:07, 11:37,
12:07, 12:37, 13:07, 13:37, 14:07 — straight through the hours the Pi appeared
dead. Local clients on 127.0.0.1 stayed connected the whole time. Every client
that had to arrive over the network vanished at 10:57 and none returned until
the cable was replugged.

**The Pi never went down.** Its Ethernet link did.

Everything else diagnosed that afternoon was wrong, and wrong in a way worth
recording:

- The card was never failing. It read perfectly on the Mac and boots fine.
- The socket was not intermittent. Reseating the card fixed nothing; plugging
  the cable in did.
- The PSU was never implicated. `get_throttled` is `0x0` and the supply is a
  genuine Pi unit.
- "Not booting" was inferred from "no network", and then the LEDs were read to
  match. **Solid red PWR with occasional green ACT is a healthy idle Pi.**

The reasoning failed the same way twice: a conclusion was formed early and the
evidence was read to fit it. The mosquitto log was there the whole time.

## And the logging claim was wrong too

The Pi does keep logs — mosquitto's, on its own card. `ngr-runlog` is `enabled`
and `active`. Decision 0027, as it was described that afternoon, is not in force.
That decision is also **not present on this branch at all**, and the operator
states he did not make it. The whole decision log needs auditing; that is being
taken up separately.

## Wi-Fi, which should have been there all along

The operator's question — *why are we using an ethernet link when the AP is
inches away* — had no good answer. The radio was always fine:

```
NGR   DC:62:79:D9:E5:9C   CHAN 11   2462 MHz   signal 90-100
```

`wlan0` was UP, unblocked, and simply had no connection profile. A stale
half-created profile from a first attempt was what kept refusing the password;
`nmcli connection delete NGR` followed by `nmcli --ask device wifi connect NGR`
worked immediately. Autoconnect is on, so it survives power cycles.

`.142` was then added to the wireless profile, because Toby's firmware has the
broker address compiled in:

```bash
sudo nmcli connection modify NGR +ipv4.addresses 192.168.68.142/22
```

Both `.142` and `.55` now answer on `wlan0`, ports 22, 1883 and 8080. The cable
is optional. Had this been done in August, the day would have been a
five-minute diagnosis over the wireless lifeline instead of an afternoon of
SD cards.

## One trap for next time

`ssh` to this Pi requires `IdentityFile ~/.ssh/id_ed25519_github`. `~/.ssh/config`
had a stanza only for `.142`, so connecting to the same host on any other
address offered the default key and fell through to a password prompt — which
looks exactly like a broken Pi and is not. Stanzas for `.55` and `.73` added.
