# Wi-Fi power save disabled on ngr-pi — and three days of misreading it

**Date:** 2026-09-01
**Host:** ngr-pi (192.168.68.142 on eth0, 192.168.68.55 on wlan0)
**Status:** Applied and verified. One reboot still needed to prove it persists.

## The symptom, as the operator saw it

Three mornings running, the console was unreachable from Chrome. Each time the
Pi had been running well the night before. Each time the fix was a power cycle.

## What was actually wrong

`brcmfmac` enables Wi-Fi power save at every boot. From this boot's log:

```
[   10.999348] brcmfmac: brcmf_cfg80211_set_power_mgmt: power save enabled
```

A sleeping station stops servicing inbound traffic. Everything above IP goes
silent — no ICMP, no SSH, no MQTT, no dashboard — which is the entire system,
because the locomotives and the console both reach the broker by IP.

## Why it took three days

Three separate failures of method, all mine.

**1. `iw` was not installed.** Every verification ran

```
iw dev wlan0 get power_save
```

and every one of them errored. On 2026-08-30 the error was read as
`Power save: on`. On 2026-09-01 the same error, wrapped in `|| echo "no
wlan0"`, was read as *the wireless interface is missing* — and reported to the
operator as such. `ip -br link` showed `wlan0 UP` the whole time. A command
that fails is not a measurement, and it was treated as one twice.

**2. `nmcli` was trusted as proof.** `802-11-wireless.powersave` reports
intent, not driver state. It read `0 (default)` after being set on 08-30,
which was correctly noticed; but the underlying assumption — that the property
is the thing to check — was never questioned until `iw` was installed.

**3. ARP was read as proof of life.** With the Pi unreachable, ARP resolved to
its MAC while nothing above IP answered. This was reported to the operator as
"the Pi is alive, its radio answered ARP." Two mechanisms produce that result:
`brcmfmac` firmware ARP offload, *and* proxy ARP from the Deco mesh for a
client still in its table. The second was not considered. On 2026-08-31 the Pi
was in fact **powered off** and ARP was still resolving — the operator found it
off the next morning. The claim was withdrawn, but only after it had been
acted on.

A control was eventually run — pinging two addresses known to be absent, which
returned `no entry` where the Pi returned a MAC. That control established that
*something* answered, and nothing more. It did not establish *what*. It was
presented as though it had.

## What was applied

```
/etc/NetworkManager/conf.d/wifi-powersave-off.conf
    [connection]
    wifi.powersave = 2

nmcli connection modify NGR 802-11-wireless.powersave 2
sudo apt-get install -y iw
```

The `conf.d` file is the durable half: it covers every Wi-Fi connection and
survives profile deletion, which matters because the NGR profile has already
been deleted and recreated once (2026-08-30, clearing a half-created profile
that was rejecting the PSK). The per-connection property is belt and braces.

Repo copy: `server/NetworkManager/wifi-powersave-off.conf`.

## Verification

```
BEFORE bounce   Power save: on
AFTER  bounce   Power save: off
profile         802-11-wireless.powersave:  2 (disable)
```

Bounced with `nmcli connection down NGR && nmcli connection up NGR`, run over
SSH on eth0 so the wireless bounce could not sever the session.

**The check that counts is `sudo iw dev wlan0 get power_save`.** `nmcli` output
is not evidence.

**Still outstanding:** this has not survived a reboot yet. The `conf.d` file
should make it hold, but that is a prediction, not a result. Confirm at the
next restart.

## What this does NOT explain

The Pi was found **switched off** on the morning of 2026-09-01, and again
failed to come up on the first power cycle (red LED, no green — the card was
not read). The second power cycle booted normally. Power save does not switch
a Pi off.

Evidence gathered after it came up, none of which convicts anything:

- `throttled=0x0` — but only covers the 22 minutes since boot, so it does
  **not** clear the supply for the overnight event
- root mounted `rw`, no EXT4 errors, no I/O errors in `dmesg`
- card enumerates clean: `mmc0: new UHS-I speed DDR50 SDXC card`, 119 GiB
- boot took 19s total (`systemd-analyze`: 2.6s kernel + 16.4s userspace)

The 15-minute gap the Mac-side watcher measured was time before the Pi was
powered, not slow booting. That was also misreported at the time.

**There is no persistent journal on this host.** `journalctl --list-boots`
returns only boot 0, so nothing survives a restart and the shutdown that
switched it off overnight left no trace. Enabling persistent journald is the
single change that would make the next occurrence diagnosable rather than
speculative. Not done — it is a config change and has not been approved.

## The standing lesson, restated

The runbook already says the Pi's configuration should not live only on the
Pi. This adds a second: **a verification command that can fail silently is not
a verification.** Check that the tool exists before trusting what its absence
appears to say.
