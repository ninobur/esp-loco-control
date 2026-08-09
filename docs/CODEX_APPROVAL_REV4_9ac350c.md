# CODEX approval — Rev 4 specifications at `9ac350c`

Status: **approved as specified below**
Date: 2026-08-09
Reviewer: CODEX
Reviewed revision: `9ac350c0f0fdbf720ca6ee50d4f7c60505c10faf`
Firmware base checked: **QUORUM 1.10 (`c39c439`)**

## Approval

### QUORUM 1.12 transport resilience

**Approved to proceed to A/B diagnostic builds and measurement.**

The approval is for the controlled experiment, not advance approval to flash a
production build. The measured winner, implementation report, both locomotive
profiles, resulting code, and field result remain subject to the sequence and
gates in `QUORUM_1_12_TRANSPORT_RESILIENCE_SPEC.md`.

### ESP-NOW command backup Phase A

**Approved to proceed to implementation from the ruled base, subject to the
bench, commissioning, and field gates in
`NGR_ESPNOW_COMMAND_BACKUP_SPEC.md`.**

Rev 4 resolves the two remaining Rev 3 findings:

- R3-1 now distinguishes locally detectable configuration failures from a
  cross-device LMK mismatch or unreachable locomotive, which an
  acknowledgement-free Phase A cannot diagnose. `BACKUP READY` describes the
  bridge's local health only and never claims verified delivery.
- R3-2 binds the authoritative acted-command identity to
  `(sender MAC, nonce, seq, cmd)`, preventing authorized transmitters from
  suppressing one another through tuple collision. The memory bound and
  two-sender gate are updated consistently.

The wire contract, fail-closed validation order, immediate E-STOP assertion,
encrypted unicast fan-out, disconnected-channel test, bounded dedup design,
bridge-health display, and authority boundary are approved for implementation.

## Limit of approval

Field deployment is **not** pre-approved. It requires:

1. implementation review against Rev 4;
2. successful specified negative, queue-full, encryption, dedup, channel-loss,
   commissioning, and recovery gates;
3. confirmation that unrelated QUORUM behavior and both locomotive profiles
   remain undisturbed.

## Non-blocking editorial note

Where the field gate says “absent/invalid key,” consistently use
“absent/invalid local key” to preserve Rev 4's observability distinction. This
wording cleanup does not block implementation.

No remaining architectural or protocol objection is recorded.
