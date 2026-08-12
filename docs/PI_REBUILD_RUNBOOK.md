# Rebuilding ngr-pi from a blank card

Written 2026-08-12, when the SD card failed and it became clear that nothing in
this repo described how to put the Pi back together. Everything below was
reconstructed from the app source and from `ngr-runlog.service`; items marked
**UNVERIFIED** were never captured off the running Pi and are best-guess.

## What failed

2026-08-11: the card threw EXT4 I/O errors, aborted its journal and remounted
read-only mid-session. Every subscriber writing to it died at 18:11:51 (see
commit 60ec7ea).

2026-08-12: Flask died while running trains. A reboot brought it back for a few
minutes, then it died again. After that the Pi stopped answering entirely — no
ping, ARP incomplete, `ssh: No route to host`.

That progression — read-only remount, then boots-then-dies, then no boot — is a
card failing progressively, not a software fault. **The fix is a new card, not a
re-image of the old one.**

## But rule out the power supply first

A failing or undersized PSU produces this exact signature, with the card as the
*symptom* rather than the cause: the Pi browns out during writes, which corrupts
the filesystem, which remounts read-only. Both failures so far happened **while
running trains** — i.e. under load, which is when a marginal supply sags.

This matters because if the PSU is the root cause, a new card fails the same way
inside a week.

Two checks:

- On the old card, if it is rescued: `/var/log/syslog` and `/var/log/kern.log`
  will contain `Under-voltage detected! (0x00050005)` if this is what happened.
  That is the deciding evidence, and it is another reason to image the card
  before wiping it.
- On the new card once it boots: `vcgencmd get_throttled` — a nonzero result
  means it is browning out. Bit 0 set = currently undervolted, bit 16 = has been
  since boot. Also `dmesg | grep -i voltage`.

If either shows undervoltage, replace the supply (official 5 V unit, and not a
long thin USB cable) before trusting the rebuild.

## Do not re-use the failed card

Rewriting a fresh image onto failing flash frequently *appears* to succeed —
Imager's verify pass can read back clean from blocks that are still good, or
from cache — and then fails again within days. Use a new card.

Keep the old card intact and unwritten. It is the only copy of everything in the
next section.

## What existed only on that card

Safe in this repo: the app source (`server/ngr_app_v1_10_*.py`), `ngr_runlog.py`,
`ngr-runlog.service`, `hall_ir_marker_bridge.py`, all field records and tools.

Not in this repo, lost if the card is wiped:

| Item | Notes |
|---|---|
| `/etc/systemd/system/ngr-app.service` | now reconstructed at `server/ngr-app.service` — **UNVERIFIED** |
| `/etc/systemd/system/ngr-profiles.service` | vestigial (SOLONAV ≥2.22 never publishes profile/request); can be dropped |
| `/etc/mosquitto/` config | listener/anonymous settings **UNVERIFIED** — needs to accept LAN clients on 1883 |
| mosquitto persistence / retained topics | the orphaned retained ghosts die with it. That is a *cleanup*, not a loss |
| `/home/david/ngr_app.py` | which repo version was live is **UNVERIFIED**; per memory it is a v1.10.x with the polarity-agreement panel and CAL RECORDING, so ≥1.10.2 |
| `/home/david/NGR/logs/cal_*.txt` | CAL recordings |
| `/home/david/dualcap_claude.log` | combined Otto + IR capture |
| network config | whether .142 is static on the Pi or a DHCP reservation on the router is **UNVERIFIED** — check the router first |
| `~/.ssh/authorized_keys` | re-provisioned by Imager (below) |

## Rescuing the old card

macOS cannot read ext4, so the root partition will not just mount. The boot
partition is FAT32 and will.

Do not mount the old card read-write. Image it whole, then extract at leisure:

```bash
diskutil list
```

Identify the card, unmount it (do not eject), and image it — replace `diskN`:

```bash
sudo diskutil unmountDisk /dev/diskN && sudo dd if=/dev/rdiskN of=~/ngr-pi-failed-card.img bs=4m conv=noerror,sync status=progress
```

`conv=noerror,sync` keeps going past unreadable blocks and pads them, so one bad
sector does not abort the rescue. Expect it to be slow and to log errors — that
is the point. If it stalls badly, `ddrescue` (`brew install ddrescue`) is better
at this than `dd`.

## Chosen path (2026-08-12): same Pi, new card

A spare Pi was available; the operator chose to keep the original board and give
it a fresh card. That is the lower-risk option here, and it sidesteps the
address problem in the next section — the board keeps its MAC, so any DHCP
reservation for `.142` still matches.

The trade is that it does **not** rule out the power supply. Keeping the
original board and supply means that if the PSU is the real cause, the new card
degrades the same way. The undervoltage check below is therefore not optional on
this path — it is the one step that decides whether this is actually fixed.

## The host must come up on 192.168.68.142

This is not cosmetic. The broker address is **compiled into the locomotive
firmware** — `firmware/QUORUM/QUORUM.ino:370`:

```c
#define MQTT_BROKER "192.168.68.142"
```

Otto and Toby both carry it. If the rebuilt host lands on a different address,
neither locomotive can reach the broker, and the only remedies are reflashing
both locos or moving the address back.

Reusing the original board should preserve the address. Verify it anyway on
first boot (`hostname -I`) before concluding the rebuild worked — a fresh OS
install can come up with a different hostname, and some routers key reservations
on hostname rather than MAC.

**This would bite if the spare board were ever used instead**, because a
different board has a different MAC and would not inherit a MAC-keyed
reservation. In that case, add a reservation for the new MAC or pin the address
on the Pi.

On Raspberry Pi OS Bookworm and later, networking is NetworkManager, not
`dhcpcd` — most advice found online edits `/etc/dhcpcd.conf`, which does nothing
there. Use `nmcli`:

```bash
nmcli con mod preconfigured ipv4.addresses 192.168.68.142/24 ipv4.gateway 192.168.68.1 ipv4.dns 192.168.68.1 ipv4.method manual
sudo reboot
```

Confirm the connection name with `nmcli con show` first; on a Wi-Fi build it is
usually `preconfigured`. Check the gateway matches your router.

**Never have both the old and new Pi powered on with `.142` at once** — the
address collision will make both behave erratically and look like a fresh fault.

## Do not put the continuous logger back on the card

`ngr_runlog.py` and the old `mosquitto_sub` captures wrote telemetry to the SD
card continuously. That write load is what wears cards out, and it is what died
when the filesystem went read-only.

Capture already moved to the Mac in commit 60ec7ea (`capture.sh` subscribes from
the Mac and writes to the Mac, with reconnect logging). **Leave it there.** The
rebuilt Pi should serve the dashboard and run the broker; it should not be the
thing writing a continuous log to flash.

`ngr-runlog.service` is therefore intentionally *not* enabled below. Per
`STATUS.md` §9 it had never actually run on the Pi anyway.

## Flashing the new card

In Raspberry Pi Imager, **check the device selector points at the new card.**

Use the gear / "Edit settings" panel before writing, so the Pi comes up on the
network without a monitor:

- hostname — match whatever the router expects
- username `david` (the app, the service units and the SSH config all assume it)
- Wi-Fi SSID + password, correct country
- enable SSH, public-key auth, paste `~/.ssh/id_ed25519_github.pub`

## Provisioning

```bash
sudo apt update && sudo apt install -y mosquitto mosquitto-clients python3-flask python3-paho-mqtt
```

`mosquitto-clients` is not optional — the dashboard shells out to `mosquitto_sub`
for the CAL logger.

Let the broker take LAN connections (the locos and the Mac both connect to it):

```bash
printf 'listener 1883 0.0.0.0\nallow_anonymous true\n' | sudo tee /etc/mosquitto/conf.d/ngr.conf
sudo systemctl enable --now mosquitto
```

Directories the app expects:

```bash
mkdir -p /home/david/NGR/logs /home/david/NGR/telemetry
```

Then copy `server/ngr_app_v1_10_11.py` to `/home/david/ngr_app.py`,
`server/ngr_runlog.py` to `/home/david/NGR/telemetry/`, and the two unit files to
`/etc/systemd/system/`:

```bash
sudo systemctl daemon-reload && sudo systemctl enable --now ngr-app ngr-runlog
```

## Verifying

```bash
systemctl status ngr-app --no-pager
curl -sSL -o /dev/null -w '%{http_code}\n' http://192.168.68.142:8080/
mosquitto_sub -h 192.168.68.142 -t 'ngr/#' -v -W 5
```

The dashboard is on **port 8080**, not 80, and `/` answers 302 redirecting to
`/console` — so follow redirects or the check looks like a failure. With the
locomotives powered up, both should return their retained bootid.

## What actually happened on the 2026-08-12 rebuild

Recorded because most of it was not predicted by this runbook:

- **Power was clean.** `throttled=0x0` on the rebuilt host. The undervoltage
  hypothesis is *disproved* for this failure — the card was the cause, not a
  symptom. The check was still worth running; it is what closed the question.
- **Wi-Fi never associated.** `wlan0` came up `NO-CARRIER` despite Imager's
  Wi-Fi settings, and the Pi was invisible on the network until an Ethernet
  cable was run to the Deco. Unresolved — the Pi currently runs wired.
- **Ethernet has a different MAC from the Wi-Fi radio** (`88:a2:9e:c4:58:ca` vs
  `…:cb`), so the router's `.142` reservation did not match and DHCP handed out
  `192.168.68.73`. Fixed by adding `.142` as a *second* address on the wired
  connection, keeping the DHCP address as a fallback so a bad edit cannot lock
  you out with no monitor attached:

  ```bash
  sudo nmcli con mod "Wired connection 1" +ipv4.addresses 192.168.68.142/24
  sudo nmcli con up "Wired connection 1"
  ```

- **The Imager SSH public key was not installed.** Key auth failed on first
  contact; `ssh-copy-id -i ~/.ssh/id_ed25519_github.pub david@<addr>` fixed it.
- **Passwordless sudo is no longer the Pi OS default** on Debian 13 (trixie).
  Provisioning over SSH needs it, or every `sudo` fails with "a terminal is
  required". Granted with `ssh -t … 'echo "david ALL=(ALL) NOPASSWD: ALL" |
  sudo tee /etc/sudoers.d/010-david-nopasswd'`. Note `ssh -t` — without a TTY
  the password prompt cannot be answered.
- **The SSH host key changed**, as it must after an OS reinstall. Clear the old
  entry with `ssh-keygen -R 192.168.68.142`.
- The OS installed was Raspberry Pi OS **Debian 13 (trixie), 64-bit desktop** —
  Lite was not offered in Imager 2.0.7 at the top level.

## The standing lesson

Capture and evidence already moved off the Pi (commit 60ec7ea). This runbook
closes the other half: the Pi's *configuration* should not live only on the Pi.
Anything added to it from here — a new unit file, a broker setting — belongs in
`server/` in this repo at the same time.
