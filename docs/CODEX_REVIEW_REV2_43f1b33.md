# CODEX review — Rev 2 specifications at `43f1b33`

Status: **QUORUM 1.12 approved for A/B testing; ESP-NOW Phase A not yet approved for implementation**
Date: 2026-08-09
Reviewer: CODEX
Reviewed revision: `43f1b3342449fcf0f10a24212146fdcc058ce926`
Firmware base checked: **QUORUM 1.10 (`c39c439`)**

## Disposition

### QUORUM 1.12 transport resilience

**Approved to proceed to the A/B diagnostic-build and measurement stage.**

Rev 2 fixes the material ambiguities in the original proposal:

- `pubmax` retains the exact QUORUM 1.11 status-drain measurement scope, so
  results remain comparable with the evidence that found the failure;
- the acceptance rule now requires no regression on any metric, a strict gain
  on at least one stability metric, and no p95 or worst-case command-latency
  regression;
- latency reporting includes median, p95, and maximum.

This is approval for the A/B experiment, not advance approval to flash a 1.12
production build. The measured winner, implementation report, both profiles,
and resulting code remain subject to the review and field sequence in the spec.

Two implementation-report clarifications remain non-blocking for the A/B build:

1. define `time-to-stable-session` before collecting data, including the
   continuous-connected interval and observable broker/console conditions;
2. record how the one-per-pass behavior is implemented, while preserving
   inbound-first service, marker priority, and no-quorum reconciliation's
   existing priority and generation protection.

### ESP-NOW command backup Phase A

**Not yet approved for implementation.**

Rev 2 resolves the original three blocking findings: immediate E-STOP assertion,
disconnected-channel behavior as a mandatory gate, and encrypted unicast
fan-out for all-locomotive commands. It also correctly excludes CLEAR-E-STOP,
distinguishes RF-copy dedup from separate button presses, and makes bridge
health visible without claiming delivery.

Two protocol matters still require specification changes.

## E1 — blocking: fail-closed validation and key contract is still absent

The spec says the callback validates the struct, CRC, nonce, and sequence, but
does not freeze the wire or validation contract. Before implementation it must
specify:

- packed wire layout, byte order, exact accepted length, protocol version, and
  command enum;
- CRC algorithm and exact covered bytes;
- authenticated sender/peer allow-list and target validation;
- validation order before either the immediate E-STOP assertion or queueing;
- key provisioning and rotation without committing keys;
- fail-closed behavior for missing/invalid keys, unknown peers, bad length,
  bad version, bad CRC, invalid command, wrong target, and stale/replayed IDs.

There must be no plaintext fallback. A key or peer configuration failure makes
the backup visibly unavailable.

## E2 — blocking: the nonce rule permits old-epoch re-adoption

Rev 2 says any nonce different from `last-nonce` is accepted as a bridge reboot
and adopted. A delayed repeat from the preceding epoch can therefore replace
the current epoch; a later current-epoch repeat can replace it again. Randomness
makes nonce collision unlikely but does not order epochs or prevent this bounce.

Use a dedup design that does not treat every different nonce as automatically
new forever. Acceptable designs include an authenticated session-establishment
rule plus sequence tracking, or a bounded cache/window of recently acted command
identities `(sender, nonce, seq, command class)` sized to cover the retry and
reboot overlap. The chosen rule must state memory bounds, expiry, wrap behavior,
and receiver-reboot behavior.

## Required Phase A gates to add explicitly

- An engaging ESP-NOW E-STOP stops the motor with `cmdQueue` deliberately full.
- PAUSE remains enrolled-only and produces `STOP_IGNORED` when not enrolled.
- All-locomotive E-STOP is verified as encrypted unicast to each configured
  peer; no plaintext transmission occurs.
- Unknown sender, wrong target, bad length, bad version, bad CRC, invalid
  command, stale/replayed identity, and absent/invalid key all fail closed and
  increment the appropriate observability counter.
- A missing/invalid key or failed peer setup marks the backup unavailable at
  the dispatcher.
- Delayed copies from a previous bridge epoch do not act after a new epoch is
  established.
- Receiver and bridge reboot cases both preserve the documented dedup behavior.

Once E1 and E2 are incorporated, the ESP-NOW specification may return for a
focused protocol review. No objection is raised to its command scope,
authority boundary, or overall two-path architecture.
