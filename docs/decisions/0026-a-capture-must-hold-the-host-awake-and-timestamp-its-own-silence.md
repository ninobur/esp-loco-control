# 0026 — A capture must hold the host awake and timestamp its own silence

Status: Accepted (2026-08-12)

## Decision

Telemetry capture on the Mac is subject to three requirements, all now
implemented in `field-records/tools/capture.sh`:

1. **The capture holds the host awake.** `caffeinate -i -m -s` is held for the
   life of the capture and released when it stops. Starting on battery prints a
   warning and writes it into the capture file, because a closed lid still
   sleeps the machine and no assertion prevents that.
2. **The capture timestamps its own silence.** A watchdog independent of the
   subscriber writes `# STALL <ts> no data for Ns` and `# RESUME <ts>` lines
   into the capture. Silence is recorded as it happens rather than reconstructed
   afterwards.
3. **The capture subscribes to `ngr/#`**, plus a small set of `$SYS/broker`
   counters. The counters double as a ~10 s heartbeat, which is what makes the
   watchdog's judgement of "silence" unambiguous instead of a guess about
   whether the railway happened to be idle.

**A gap in a capture is not evidence of quiet running until the capture says it
was receiving.**

## Context

The 2026-08-12 session recorded 214 of ~1075 markers. The whole CW leg between
mm 5 and the NO_QUORUM failure near mm 120 was missing, which is why that fault
could not be root-caused.

`field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md` §5 attributed the
loss to the degraded Pi SD card silently dropping QoS 0 publishes, and decision
0025 carried that forward as "the likely reason ~860 of 1075 markers were never
delivered". **Both are wrong.** The measured evidence:

- The loss is not diffuse. It falls inside **three total-silence windows** —
  1117 s, 626 s, 374 s — that contain no message on any topic from any
  publisher. Random QoS 0 dropping does not produce clean 18-minute holes.
- **The locomotive never reconnected.** Its own `mqtt_attempts` counter, in
  `state/loopstat`, stayed at 1 for the whole boot spanning all three windows,
  and `queue_high_water` stayed 0. Only two `reason:"MQTT_CONNECT"` alerts exist
  in the file, both at boot.
- **The broker never restarted**, and `uptime_ms` in the 1 Hz alert advanced
  exactly in step with wall time across every window: the firmware ran
  continuously and kept navigating, its `agree` counter advancing 511, 244 and
  80 markers into the three holes — 835 of the ~860 missing.
- Each window ends with a burst consisting **only of retained topics** — the
  `state/*` block, `status/online`, `profile/status` still carrying its old
  `published_at`. No alert, no marker, no loopstat, because those are not
  retained. That is the signature of a **subscriber resubscribing**, not of a
  publisher resuming.
- `pmset -g log` closes it. Every boundary matches a sleep/wake transition to
  within seconds: `Idle Sleep` at 22:32:55 against last data 22:32:56;
  `DarkWake` 22:51:32 against resumption 22:51:33; `Sleep Service Back to Sleep`
  22:53:03 against 22:53:04; `Maintenance Sleep` 23:03:37 against 23:03:38. On
  battery, unattended, overnight.

**The MacBook went to sleep three times. The SD card had nothing to do with it.**

The reason nothing was logged is that the script's stated premise was false.
Its comment read: *"mosquitto_sub exits when the broker drops it; this restarts
it and appends."* mosquitto_sub 2.1.2 calls `mosquitto_loop_forever`, which
**reconnects and resubscribes internally**. The process never exited, the
`# RECONNECT` path never ran, and 18 minutes of silence was indistinguishable
in the file from 18 minutes of a stationary locomotive.

The tool written specifically so that "a capture that dies silently is worse
than no capture" died silently, and its own detector could not fire.

## Consequences

- **The Pi SD card is exonerated for this loss.** It should still be replaced —
  it genuinely cost evidence on 2026-08-11, when subscribers writing to the Pi
  died on a read-only remount — but replacing it would **not** have saved the
  2026-08-12 CW leg, and it is **not** a blocker for re-running CW. The card
  blocks the Console E-stop work, which needs to write to the Pi. It does not
  block capture, which now runs on the Mac.
- Decision 0025's bullet and the verdict's §5 are corrected in place, with the
  original claim left visible and struck rather than deleted.
- The phantom verdict's §2 and §3 are unaffected: they rest on markers that did
  arrive, and full route coverage 0–170 was achieved inside the windows that
  were captured.
- **The capture is now part of the evidence, not a neutral observer of it.** A
  future analysis that finds a gap must check the `# STALL` lines before
  attributing the gap to the railway.

## References

- `field-records/20260812_CW_NOQUORUM_INVESTIGATION.md` — the re-run this enabled
- `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md` §5 — corrected
- `docs/decisions/0025-*` — corrected
- `field-records/tools/capture.sh` — the implementation
