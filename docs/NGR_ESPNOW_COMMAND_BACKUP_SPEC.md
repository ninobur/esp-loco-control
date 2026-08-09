# ESP-NOW command backup — Phase A: the dispatcher keeps its stop

Status: **proposal for review (CODEX, Sam). No code written.**
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
| **E-STOP** | all, or one locomotive | same handler as `cmd/estop` — door 1, both chambers |
| **PAUSE** (dispatcher STOP) | one locomotive | same handler as dispatcher stop — enrolled only, `STOP_IGNORED` otherwise |

**Deliberately excluded:** BEGIN (a backup path exists to make trains
stop and stay seen, never to launch them); everything else.
**Held for review as an open question:** CLEAR E-STOP — un-stopping over
a lossy link deserves its own argument before inclusion.

**No new authority anywhere.** Frames terminate in the *existing* command
handlers behind the *existing* four doors. A second wire into the same
law, not new law.

## Architecture

```
Dispatcher console (Flask) ──USB serial──► TX-bridge ESP32 ──ESP-NOW──► locomotives
        (one of the two Pi-attached ESP32s; the other stays reserved
         for the Phase B telemetry receiver)
```

- **Frame** (fixed struct, nothing parsed): magic, version, **seq u32**,
  target id u32 (0 = all), cmd u8, CRC. Each command transmitted **×5**
  over ~250 ms.
- **Dedup**: locomotive keeps last-seen seq per command class; repeats
  are dropped; a *newer* seq always acts.
- **Security**: ESP-NOW LMK encryption mandatory (keys in
  `credentials.h`, gitignored). An unencrypted stop channel is a stop
  channel anyone owns.
- **Channel**: ESP-NOW must ride the STA's current channel — the
  TX-bridge is configured to the EAP's channel (11) and **must follow if
  the EAP ever moves** (operational note in the runbook; the bridge
  reports its channel on serial at boot).
- **Acknowledgement**: none in-band. The observable effects are the
  locomotive stopping and, when MQTT lives, the normal `state/station`
  response (P13 seq). Phase B's telemetry return makes acks first-class.

## Components and process

1. **TX-bridge** — new `test-programs/ESPNOW_CMD_TX/` instrument
   (catalogued per 0018): serial line-protocol in, ESP-NOW frames out,
   channel + peer table fixed, prints every frame sent.
2. **Locomotive RX** — QUORUM addition (rides the 1.12 line or its own
   bump, base per catalog): `esp_now` init as STA-coexistent, receive
   callback validates struct/CRC/seq and **enqueues onto the existing
   cmdQueue** — the same queue MQTT commands use, so ordering, chamber
   rules, and refusal publication are untouched.
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
in AUTO: dispatcher PAUSE stops it; dispatcher E-STOP stops it in MANUAL
too; repeated presses are deduped (one action); a frame addressed to Toby
does not touch Otto; all repeated ×5 transmissions verified on the bridge
serial. Then WiFi restored and normal operation confirmed undisturbed.

## Open questions for review

1. CLEAR E-STOP over the backup: include, exclude, or include-with-
   confirmation?
2. Should the locomotive publish (via MQTT, when alive) a counter of
   ESP-NOW commands received, for observability?
3. Bridge watchdog: if the bridge ESP32 dies, how does the console learn
   its backup path is gone? (Proposal: bridge heartbeats on serial; Flask
   surfaces a BACKUP OFFLINE chip on the dispatcher console.)
