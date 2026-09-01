# The console was unreachable because macOS blocked the *client*, not because the Pi was down

**Date:** 2026-09-01 (afternoon, following the morning power-save work)
**Status:** Resolved. Railway running on the spare Pi 4 at `192.168.68.142`.
**Cost:** An afternoon of power cycles, a card swap, and a hardware substitution
that was never needed.

---

## The symptom as the operator saw it

Back from a bike ride: *"console is unreachable."* Chrome would not load the
dashboard. Same words as the morning, same words as the two days before that.

## What was actually wrong

**macOS Local Network privacy blocked the requesting application.** On macOS 15
and later the permission is granted per app. Traffic to RFC1918 destinations from
an app without the grant is dropped silently — no error, no ICMP unreachable,
nothing that distinguishes it from a host that is switched off.

Two applications on this Mac lacked the grant:

- **Google Chrome** — the operator's dashboard client. This is the whole of the
  original complaint.
- **Claude.app** — the process my `ping` and `nc` run inside. This is why every
  probe I ran came back dead.

Traffic to the internet was never affected, so `1.1.1.1` answered and DNS resolved
throughout. That is what made it read as a LAN fault rather than a permission fault.

## The moment it broke open

Safari and DuckDuckGo loaded `http://192.168.68.142:8080/console` while, at the
same moment, my `nc` to `192.168.68.142:8080` reported closed. One of those two
observers is wrong about the world, and it is not Safari.

The control that should have been run first, and was not until late:

```
192.168.68.1     ping NO      <- the Deco itself
192.168.68.59    ping NO
192.168.68.63    ping NO
192.168.68.79    ping NO
192.168.68.81    ping NO
192.168.68.88    ping NO
192.168.68.89    ping NO
```

**Not one host on the LAN was reachable, including the router.** A fault that
takes out every address on the segment simultaneously is not a fault in any one
of them. That single line of evidence would have ended the afternoon before the
first power cycle.

After the operator granted Local Network to Claude.app:

```
192.168.68.1     ping OK
192.168.68.63    ping OK
192.168.68.89    ping OK
192.168.68.142   ping OK  ssh OPEN  8080 OPEN  1883 OPEN
```

Nothing on the Pi changed between those two readings.

## Why ARP made it worse

ARP resolution is performed by the kernel, not by the application, so it was never
gated. Throughout the outage I held correct MAC addresses for every host on the
segment, which *felt* like evidence the hosts were alive while every layer above
was being discarded before it left the machine.

This is the second time in two days ARP has misled this investigation, by a
different mechanism each time:

1. **2026-08-31** — a sleeping `brcmfmac` chip answers ARP from its own firmware,
   and a Deco mesh AP proxy-ARPs for clients in its table. Host asleep, ARP alive.
2. **2026-09-01** — ARP alive because the kernel answers it; everything above
   dropped by an app-level permission on *my* side.

**ARP resolution has now failed as evidence in both directions.** It does not
prove a host is alive, and its absence does not prove much either.

## What was true, and survives

Not all of the day was noise. Two findings stand on evidence that this permission
bug cannot touch:

- **The Wi-Fi power save fix (2026-08-31) is real and is verified.** Confirmed
  again on this boot: `Power save: off`. It is a NetworkManager `conf.d` drop-in
  living on the card, so it travelled to the replacement Pi intact — which is
  exactly why it was written that way rather than onto a connection profile.
- **The original Pi genuinely was off the network this morning.** At the first
  check its MAC was absent from ARP while thirteen neighbours were present. ARP
  is kernel-level and unaffected by the app permission, so that reading holds.
  It matches the operator's red-LED-no-green observation.

Everything I asserted *between* those two points should be discarded.

## What is now running

The card was moved into the spare **Raspberry Pi 4 Model B Rev 1.5**. It booted
without complaint and is serving:

```
hostname   ngr-pi
eth0       192.168.68.142/24 (static, follows the card) + 192.168.68.92/22 (DHCP)
wlan0      192.168.68.91/22
mosquitto  active
ngr-app    active
root fs    /dev/mmcblk0p2 ext4 rw,noatime
card       SR128 119 GiB, UHS-I DDR50 SDXC, enumerated clean
throttled  0x0
power save off
```

Zero under-voltage events and zero I/O or EXT4 errors this boot.

**The SD card is exonerated on evidence, not on assumption.** It boots and runs
correctly in different hardware. The operator said at the outset that cards are
the usual suspects and are then exonerated; he was right, and the card should not
be suspected again without new evidence.

## Still open

- **The original Pi is unconvicted.** Its red-LED-no-green failures are real and
  unexplained. The card is cleared, so the remaining suspects are the board itself
  and its power supply and cable. The A/B that would settle it — the suspect
  supply on the known-good Pi 4 — was never run, because the permission fault was
  found first. It is still the right test.
- **There is still no persistent journal.** `/var/log/journal` exists but journald
  is not using it; `Storage` was never set, so logging remains volatile and every
  event continues to leave no trace. Proposed twice now, not yet approved.

## The standing lesson

Yesterday's lesson was *a verification command that can fail silently is not a
verification*. Today's is its mirror:

> **A negative result is not evidence until a positive control has passed.**

"I cannot reach it" is a statement about the observer until something else on the
same path has been reached successfully. Before reporting any host as unreachable,
reach a host that is known to be up — the gateway will do. If that fails too, the
fault is on this side and no further probing of the target means anything.

Applied to this railway specifically: **before diagnosing the Pi, ping the Deco.**
