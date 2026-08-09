# CODEX review — Rev 3 specifications at `0963192`

Status: **QUORUM 1.12 approved for A/B testing; ESP-NOW Phase A requires two narrow Rev 4 corrections**
Date: 2026-08-09
Reviewer: CODEX
Reviewed revision: `0963192282baac53e500784f3738bf89d9e98851`
Firmware base checked: **QUORUM 1.10 (`c39c439`)**

## Disposition

### QUORUM 1.12 transport resilience

**Approved to proceed to A/B diagnostic builds and measurement.**

Rev 3 pre-registers `time-to-stable-session` and requires the implementation
report to preserve inbound-first service, marker priority, and the no-quorum
reconciliation protections. Together with Rev 2's fixed `pubmax` scope and
acceptance rule, this resolves the outstanding specification questions.

This is approval for the experiment, not advance approval to flash a production
build. The measured winner, implementation, both profiles, and field result
remain subject to the sequence in the specification.

### ESP-NOW command backup Phase A

**Not yet approved for implementation.** Rev 3 resolves E1 and E2 in substance:
the wire format and pre-action validation order are concrete; the acted-identity
cache removes the epoch-bounce defect; and the negative and safety gates are
testable. Two narrow contract corrections remain.

## R3-1 — blocking: cross-device key mismatch is not locally observable

The specification promises that missing or invalid key material disables the
path and produces `BACKUP OFFLINE`. A device can detect missing or malformed
local key material and a local `esp_now_add_peer` failure. It cannot, without a
return-path exchange, determine at boot that its locally valid LMK differs from
the peer's locally valid LMK. Phase A explicitly has no in-band acknowledgement.

Revise the contract and gate to distinguish:

- missing/malformed local key or local peer-registration failure: disable the
  path and report `BACKUP OFFLINE`;
- cross-device LMK mismatch or unreachable locomotive: delivery is unconfirmed
  and is established by commissioning test, observed train response, MQTT
  evidence when available, or an explicitly added return-path health handshake.

The UI must not claim that the bridge can diagnose a remote key mismatch when
the protocol provides no evidence from the receiver.

## R3-2 — major: bind dedup identity to the authorized sender

Validation permits a sender MAC allow-list, but the authoritative cache identity
is only `(nonce, seq, cmd)`. With more than one authorized transmitter, two
senders can generate the same tuple and one command can suppress the other.

Choose and freeze one of these rules:

1. exactly one transmitter MAC is authorized, and the configuration rejects
   additional senders; or
2. the acted identity is `(sender MAC, nonce, seq, cmd)`.

The second rule is preferred because it preserves the stated allow-list design.
Update the fixed memory estimate and the corresponding collision/dedup gate.

## Approval path

No architectural redesign is required. After R3-1 and R3-2 are incorporated,
the ESP-NOW Phase A specification is suitable for a short confirmation review.
No objection remains to its command scope, E-STOP fast path, validation order,
encrypted unicast fan-out, channel-loss field gate, bounded dedup approach, or
authority boundary.
