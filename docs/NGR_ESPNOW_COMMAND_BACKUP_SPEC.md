# ESP-NOW command backup — Phase A: the dispatcher keeps its stop

Status: **Rev 3 — CODEX Rev-2 review (`CODEX_REVIEW_REV2_43f1b33.md`)
incorporated: E1 wire/validation/key contract frozen, E2 dedup redesigned
as a bounded acted-identity cache, required gates added. Awaiting the
focused protocol review. No code written.**
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

- **Wire contract (E1 — FROZEN):** packed little-endian, **exact length
  22 bytes**, any other length rejected:

  | offset | size | field | value |
  |---|---|---|---|
  | 0 | 4 | `u32 magic` | `0x4E475243` |
  | 4 | 1 | `u8 version` | `1` — any other rejected |
  | 5 | 1 | `u8 cmd` | `1` = E-STOP ENGAGE, `2` = PAUSE — **closed enum**, all else rejected |
  | 6 | 2 | `u16 reserved` | must be `0` |
  | 8 | 4 | `u32 nonce` | bridge boot epoch (random at bridge start) |
  | 12 | 4 | `u32 seq` | per-command counter |
  | 16 | 4 | `u32 target` | receiver's numeric LOCO_ID (e.g. 9950011). **No broadcast value exists on the wire** — "all" is bridge-side fan-out only, so `target` must equal the receiver's own ID exactly |
  | 20 | 2 | `u16 crc` | CRC-16/CCITT-FALSE over bytes 0–19 |

  Each operator command transmits **×5 over ~250 ms**, all five carrying
  the same (nonce, seq); a new button press is a new seq.
- **Validation order (E1)** — completed **before** the E-STOP fast-path
  assertion and before queueing, in this order, each failure **closed**
  (frame dropped, nothing asserted, nothing queued) and counted on its
  own observability counter: (1) exact length; (2) sender MAC on the
  configured allow-list — ESP-NOW delivers frames from unknown senders
  to the callback, so registration alone is not the filter; (3) magic +
  version; (4) CRC; (5) cmd in the closed enum, reserved == 0; (6)
  target == own LOCO_ID; (7) dedup/replay (below). Only a frame passing
  all seven reaches the finding-1 fast path.
- **Dedup (E2 — redesigned as the bounded acted-identity cache):** the
  receiver keeps a ring of the last **16** acted identities
  `(nonce, seq, cmd)` with receive timestamps; a frame **acts iff its
  identity is not present with age < 60 s**. Bounds stated: 16 × 13 B ≈
  208 B fixed; expiry 60 s (covers the 250 ms retry burst and any
  bridge-reboot overlap with two orders of margin); no wrap arithmetic
  is load-bearing (identities are compared for equality, not order); no
  epoch ordering exists to bounce (E2's re-adoption defect is dissolved,
  not patched — a delayed old-epoch repeat is simply found in the cache
  and dropped; a genuinely un-acted old-epoch command acts once, which
  is correct: it was a real command). **Receiver reboot:** cache clears;
  the only exposure is a command still inside its own 250 ms retry burst
  acting once more — a stop-class command repeating is the safe
  direction. The (nonce, latest-seq) pair may be kept as a fast-path
  shortcut but the cache is the authority.
- **Security and key contract** *(finding 3 + E1)*: ESP-NOW LMK-encrypted
  **unicast only**; `all` is bridge-side fan-out, ×5 per configured peer.
  PMK + per-peer LMKs are 16-byte arrays in `credentials.h` (gitignored)
  on both ends; provisioning and rotation are manual edit-both-ends and
  reflash, documented in the runbook — no over-the-air rotation in
  Phase A. **No plaintext fallback exists**: if key material is missing
  or invalid or `esp_now_add_peer` fails at boot, ESP-NOW RX/TX is
  disabled entirely, the locomotive reports `espnow:unavailable` in its
  counters, the bridge reports the same in its heartbeat, and the
  dispatcher console shows **BACKUP OFFLINE**. A configuration failure
  makes the backup *visibly absent*, never silently open.
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

## Field gate (Phase A) — including the CODEX-required cases

With WiFi deliberately killed (EAP powered off) and a locomotive running
in AUTO: dispatcher PAUSE stops it **while disconnected — the finding-2
channel-pin centrepiece**; dispatcher E-STOP stops it in MANUAL too, via
the immediate fast path; the five repeats of one press act once, two
presses act twice; a frame addressed to Toby does not touch Otto; the
console shows BACKUP OFFLINE within the heartbeat window when the bridge
is unplugged; WiFi restored, pin released, normal operation undisturbed.

Additionally (CODEX Rev-2 required gates, adopted verbatim):

- an engaging ESP-NOW E-STOP stops the motor **with `cmdQueue`
  deliberately full** (the fast path proven, not assumed);
- PAUSE remains enrolled-only and produces `STOP_IGNORED` when not
  enrolled;
- all-locomotive E-STOP verified as encrypted unicast to each configured
  peer, with **no plaintext transmission observed on the air**;
- unknown sender, wrong target, bad length, bad version, bad CRC,
  invalid command, replayed identity, and absent/invalid key **each fail
  closed and increment their own counter**;
- a missing/invalid key or failed peer setup marks the backup
  unavailable at the dispatcher;
- delayed copies from a previous bridge epoch do not act after their
  identities are cached;
- receiver reboot and bridge reboot both preserve the documented dedup
  behaviour.

## Open questions — resolved by the 2026-08-09 CODEX review

1. CLEAR E-STOP: **excluded from Phase A.**
2. Receive/dedup/reject counters: **yes, published via MQTT when alive.**
3. ~~Bridge watchdog~~ **RESOLVED (finding 5, adopted — mandatory in
   Phase A):** the bridge heartbeats on serial; the dispatcher console
   displays bridge-connected, heartbeat age, configured radio channel,
   and last-send status, and shows **BACKUP OFFLINE** when the heartbeat
   ages out. Serial output proves transmission attempts only — the
   display must say so.
