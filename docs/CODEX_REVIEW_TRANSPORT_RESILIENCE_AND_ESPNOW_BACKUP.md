# CODEX review — transport resilience and ESP-NOW command backup

Status: **review complete; specification amendments required before ESP-NOW implementation**
Date: 2026-08-09
Reviewer: CODEX
Reviewed commit: `61d90d2`
Firmware base checked: **QUORUM 1.10 (`c39c439`)**

## Verdict

`QUORUM_1_12_TRANSPORT_RESILIENCE_SPEC.md` is sound enough to proceed to
the restrained-drain A/B measurement after the clarifications below.

`NGR_ESPNOW_COMMAND_BACKUP_SPEC.md` has the right scope and authority
boundary, but the three blocking findings below must be resolved in the
specification before implementation. In particular, the backup E-STOP must
preserve QUORUM 1.10's immediate assertion path; merely placing it on
`cmdQueue` would regress an intentional safety property.

No implementation is authorized by this review.

## Blocking findings — ESP-NOW Phase A

### C1 — preserve the immediate E-STOP assertion path

The proposed receiver validates the frame and enqueues it onto the existing
`cmdQueue`. That is correct for PAUSE, and it preserves the established command
handler and chamber rules, but it is not sufficient for an engaging E-STOP.

QUORUM 1.10's MQTT callback sets `estopped=true` immediately, before attempting
the queue send, and then also queues the command for complete loop-thread
handling. This was deliberately introduced so the locomotive stops even if the
queue is full or its handler is delayed. The ESP-NOW receive path must provide
the same two-stage behavior:

1. after authentication, structural validation, target validation, and dedup,
   an engaging E-STOP immediately asserts the E-STOP latch;
2. the command is also enqueued so the existing handler performs the complete
   state transition, station reset, and reporting;
3. PAUSE remains queue-only;
4. CLEAR-E-STOP, if separately approved later, remains queue-only and must never
   use the assertion fast path.

The callback must restrict itself to the same callback-safe operation used by
the existing MQTT path. Full command handling remains on the loop thread.

### C2 — define radio behavior after loss of the EAP

The bridge is specified as fixed to the EAP's channel, while the Phase A field
gate powers the EAP off. After STA disconnection, the locomotive may scan or
otherwise leave that channel. The field gate therefore does not follow from the
current channel design.

The specification must define and test a disconnected-state channel policy.
One candidate is to restore or pin the designated ESP-NOW channel whenever the
STA is disconnected, then resume following the AP channel after reassociation.
The implementation must demonstrate that this does not obstruct normal WiFi
recovery. The exact mechanism should be selected only after a focused bench
test against the ESP32/Arduino core version used by QUORUM.

### C3 — encrypted all-locomotive delivery requires unicast fan-out

The mandatory LMK protection and an all-locomotive command cannot be expressed
as one encrypted ESP-NOW broadcast. `target=0` must therefore be a console or
bridge semantic: the bridge sends the command separately to every configured
encrypted peer. Each locomotive receives five unicast copies carrying the same
logical command identity. No unencrypted fallback is permitted.

The field gate must verify all-peer fan-out as well as single-peer isolation.

## Major findings — ESP-NOW Phase A

### C4 — sequence identity must survive bridge restart and wraparound

A bare `seq u32` with “newer always acts” can reject valid commands after the
bridge reboots and its counter returns to zero. The protocol must specify a
restart-safe authenticated command identity, for example a sender boot/session
nonce paired with a sequence, or a safely persisted monotonic value. Sequence
comparison must explicitly handle wraparound.

Deduplication applies to the five RF copies generated for one operator command.
A later button press is a new logical command and receives a new identity, even
when its command and target are unchanged. The field-gate phrase “repeated
presses are deduped” should be changed to “the five transmissions from one
press cause one handler action.”

### C5 — require bridge-health observability in Phase A

Bridge serial output proves that the bridge attempted transmission; it does not
prove locomotive receipt. The proposed serial heartbeat and dispatcher
`BACKUP OFFLINE` indication should be Phase A requirements. At minimum, expose:

- bridge connected/disconnected state;
- heartbeat age;
- configured/current ESP-NOW channel;
- last command identity, target, and local send result.

Without an in-band locomotive acknowledgement, the UI and runbook must not
describe the backup path as confirmed delivered. Phase B may add that proof.

### C6 — specify frame validation and key/peer operations

Before implementation, freeze the packed wire representation, byte order,
exact length, accepted version, command enum, CRC coverage, sender allow-list,
target check, and validation order. Authentication/peer acceptance must precede
acting on a command. Document key provisioning and rotation without committing
keys, and define the safe response to a missing or invalid key: the backup path
must report unavailable rather than silently fall back to plaintext.

## Findings — QUORUM 1.12 transport resilience

### C7 — define the scope of `pubmax`

QUORUM 1.11 timed the ordinary status-queue `mqtt.publish()` call. The proposal's
phrase “windowed max `mqtt.publish()` duration” can instead be read as covering
every direct publish, including marker and no-quorum reconciliation paths.

For a result comparable to the evidence that diagnosed the fault, either retain
the exact 1.11 measurement scope and say so, or instrument every publish path
and record that the metric's scope changed. A small network-task-owned timed
publish wrapper would make the latter definition enforceable, but its own
diagnostic publication must not recursively distort the window.

### C8 — make the A/B acceptance rule unambiguous

Replace “strictly better or equal on all four” with an explicit rule:

> Variant B must be no worse on every measured metric, strictly better on at
> least one transport-stability metric, and must not increase p95 or worst-case
> command-delivery latency.

Record median, p95, and maximum command latency. Define time-to-stable-session
before testing, including the required continuous-connected interval and the
observable broker/console conditions. Use the same interference placement,
locomotive profile, firmware instrumentation, trial timing, and command schedule
for A and B. Five cycles is a minimum, not a guarantee of statistical power.

### C9 — state how reseeds are paced without weakening priority

In 1.10 the reconnect publications are placed on the ordinary status queue and
then drained at up to four per network pass. The implementation report should
state whether Variant B simply changes the status cap to one during the window
or introduces explicit reseed staging. Either way, inbound MQTT remains first,
markers remain ahead of status, and the desired retained no-quorum reconciliation
must retain its existing priority and generation protection.

## Answers to the open questions

1. **CLEAR-E-STOP:** exclude from Phase A. A backup intended to stop trains
   should not acquire restart authority in its first deployment.
2. **Receive observability:** include cumulative accepted, duplicate, rejected,
   wrong-target, and queue-drop counters, made visible through MQTT when that
   path is available. Counters must not create a new high-rate publication.
3. **Bridge watchdog:** require the serial heartbeat and `BACKUP OFFLINE`
   indication in Phase A. Treat stale heartbeat as unavailable, not degraded.

## Required gates added by this review

Before field operation, demonstrate:

- E-STOP assertion stops the motor even when `cmdQueue` is deliberately full;
- PAUSE still obeys enrolment and emits `STOP_IGNORED` when not enrolled;
- the backup works after the EAP is powered off, throughout STA scanning or the
  specified replacement channel behavior;
- WiFi/MQTT recovery still succeeds after the EAP returns;
- all-locomotive E-STOP is encrypted per-peer unicast fan-out;
- a bridge reboot does not cause fresh commands to be rejected as old;
- five copies of one command cause one handler action, while a second operator
  press remains a distinct command;
- wrong target, unknown sender, bad version, bad length, bad CRC, stale command,
  and absent key all fail closed and are counted;
- loss of bridge heartbeat produces `BACKUP OFFLINE` at the dispatcher.

Subject to C1-C9 and these gates, the proposals preserve the intended boundary:
the resilience change reduces recovery amplification, and ESP-NOW adds a second
wire into existing stopping law without adding launching authority.
