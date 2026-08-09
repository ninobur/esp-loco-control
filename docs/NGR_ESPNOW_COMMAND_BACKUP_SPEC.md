# ESP-NOW command backup — Phase A: the dispatcher keeps its stop

Status: **Rev 2 — CODEX review of 2026-08-09 incorporated (findings 1–5
and its recommendations on all three open questions). No code written.**
Date: 2026-08-09
Motivating case: 2026-08-08 — the WiFi/MQTT path degraded to 33 % loss
and commands, **including E-STOP, became undeliverable for minutes** while
the locomotive ran. The operator: *"all of this time and effort and money
means nothing without a reliable connection,"* and, on scope: *"ESP
dispatcher backup connection could be limited to basic commands and still
be useful."* This spec is that limitation, made precise.

## Principle

A second command path that shares nothing with the first except the
radio chip: **no AP association, no DHCP, no TCP, no broker, no
sessions.** Every failure mode observed on 2026-08-08 except raw RF
jamming is absent by construction. Under the measured 25–33 % frame loss,
a five-fold-repeated stateless frame delivers with near-certainty — the
same conditions that killed MQTT for minutes.

## Phase A scope — commands only, three of them

| Command | Addressing | Firmware handling |
|---|---|---|
| **E-STOP** | all, or one locomotive | **CODEX finding 1 (blocking, adopted): the receive callback asserts `estopped=true` IMMEDIATELY on a valid engaging E-STOP frame — mirroring 1.10's MQTT pre-scan — and then queues for full handling.** The fast path exists so E-STOP survives a full `cmdQueue` or a delayed loop pass; the backup path must not be slower than the primary. Door 1, both chambers |
| **PAUSE** (dispatcher STOP) | one locomotive | queue-only, same handler as dispatcher stop — enrolled only, `STOP_IGNORED` otherwise. (CLEAR-E-STOP, if ever approved, is also queue-only — clearing is never fast-pathed) |

**Deliberately excluded:** BEGIN (a backup path exists to make trains
stop and stay seen, never to launch them); everything else.
**CLEAR E-STOP: excluded from Phase A** (CODEX recommendation, adopted —
un-stopping over a lossy link waits for Phase B's acknowledgement path at
the earliest).

**No new authority anywhere.** Frames terminate in the *existing* command
handlers behind the *existing* four doors. A second wire into the same
law, not new law.

## Architecture

```
Dispatcher console (Flask) ──USB serial──► TX-bridge ESP32 ──ESP-NOW──► locomotives
        (one of the two Pi-attached ESP32s; the other stays reserved
         for the Phase B telemetry receiver)
```

- **Frame** (fixed struct, nothing parsed): magic, version, **sender
  boot-nonce u32 + seq u32** *(CODEX finding 4, adopted)*, target id u32,
  cmd u8, CRC. Each operator command transmitted **×5** over ~250 ms —
  the five repeats carry the SAME (nonce, seq); a new button press is a
  new seq and always acts. Dedup therefore applies to the frames of one
  command, never to separate presses.
- **Dedup and epoch semantics** *(finding 4)*: the bridge draws a random
  boot-nonce at startup; the locomotive keeps (last-nonce, last-seq) per
  command class. A frame acts iff its nonce differs from last-nonce
  (bridge rebooted — accept and adopt the new epoch) or its seq is newer
  under **wrap-safe serial comparison** (`(int32_t)(new-last) > 0`). No
  persistent storage needed; a nonce collision is a 1-in-2³² event per
  reboot.
- **Security** *(finding 3, adopted — blocking)*: ESP-NOW LMK-encrypted
  **unicast only**. Encrypted broadcast is not supported, so `target=all`
  is realised as **encrypted unicast fan-out: the bridge sends the frame
  set to every configured peer individually, ×5 per peer.** Keys in
  `credentials.h`, gitignored. An unencrypted stop channel is a stop
  channel anyone owns.
- **Channel** *(finding 2, adopted — blocking)*: while the locomotive's
  WiFi is **associated**, ESP-NOW rides the STA channel (the EAP's, 11),
  and the bridge matches it. While the locomotive is **disconnected** —
  precisely when the backup matters — the STA may scan across channels,
  so the firmware **pins the radio to the last-associated channel during
  disconnection** (scan/reconnect attempts are periodic, not continuous;
  the pin applies between attempts), and releases the pin on
  reassociation. The bridge never moves on its own. **The
  disconnected-state delivery is itself a mandatory field-gate case.**
- **Acknowledgement**: none in-band, and *(finding 5)* **the console must
  not imply receipt** — the UI wording is "SENT ×5", never "delivered".
  Observable effects are the locomotive stopping and, when MQTT lives,
  the `state/station` response and *(CODEX recommendation, adopted)*
  **ESP-NOW receive/dedup/reject counters published via MQTT** for
  observability. Phase B's telemetry return makes acks first-class.

## Components and process

1. **TX-bridge** — new `test-programs/ESPNOW_CMD_TX/` instrument
   (catalogued per 0018): serial line-protocol in, ESP-NOW frames out,
   channel + peer table fixed, prints every frame sent.
2. **Locomotive RX** — QUORUM addition (rides the 1.12 line or its own
   bump, base per catalog): `esp_now` init as STA-coexistent, receive
   callback validates struct/CRC/nonce/seq, **asserts `estopped=true`
   immediately for an engaging E-STOP (finding 1)**, and enqueues onto
   the existing cmdQueue — the same queue MQTT commands use, so
   ordering, chamber rules, and refusal publication are untouched.
3. **Console** — dispatcher E-STOP/PAUSE routes additionally write the
   command to the bridge's serial port. No UI change: the same buttons,
   now double-pathed.

Audit note per the CTO2 postmortem: the archived r12 ESP-NOW transport is
**reference for the audit, not a port** — the frame, dedup, and security
design above are new and minimal.

## Phase B (outline only, separate approval)

Self-truth telemetry downlink (CTO3 §3 / M5 packet, built early) → second
Pi ESP32 as receiver → Pi reconciler merging MQTT + ESP-NOW by seq.
Yesterday's evidence: it would have kept the dashboard live at ~70 %
delivery through the worst of the failure. Not in Phase A's scope.

## Field gate (Phase A)

With WiFi deliberately killed (EAP powered off) and a locomotive running
in AUTO: dispatcher PAUSE stops it **while disconnected — this exercises
the finding-2 channel pin and is the gate's centrepiece**; dispatcher
E-STOP stops it in MANUAL too, via the immediate fast path; the five
repeats of one press act once, while two presses act twice (finding 4
semantics); a frame addressed to Toby does not touch Otto; a bridge
reboot mid-session does not deadlock dedup (nonce epoch accepted); the
console shows BACKUP OFFLINE within the heartbeat window when the bridge
is unplugged; all transmissions verified on bridge serial. Then WiFi
restored, the channel pin released on reassociation, and normal
operation confirmed undisturbed.

## Open questions — resolved by the 2026-08-09 CODEX review

1. CLEAR E-STOP: **excluded from Phase A.**
2. Receive/dedup/reject counters: **yes, published via MQTT when alive.**
3. ~~Bridge watchdog~~ **RESOLVED (finding 5, adopted — mandatory in
   Phase A):** the bridge heartbeats on serial; the dispatcher console
   displays bridge-connected, heartbeat age, configured radio channel,
   and last-send status, and shows **BACKUP OFFLINE** when the heartbeat
   ages out. Serial output proves transmission attempts only — the
   display must say so.
