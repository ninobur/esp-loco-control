# QUORUM 1.12 — transport resilience: reconnect restraint and the wander tripwire

Status: **Rev 2 — CODEX 2026-08-09: cleared to proceed to A/B testing,
findings 6–7 incorporated. No code written.**
Date: 2026-08-09
Base: **QUORUM 1.10 (`c39c439`)** — operator ruling 2026-08-08; 1.11 was a
diagnostic stub and is not a base.
Evidence: `FIELD_20260808_INAUGURAL_FINDINGS.md`,
`field-records/logs/20260808_otto_serial_netdiag.log` (2,876 [DIAG]
samples), CODEX Pi-side measurements (33 % loss, 885 ms, FIN-WAIT-2
zombies), CODEX transport analysis bracketing v2.19–v2.22.

## The disease this treats

2026-08-08 established a two-layer failure. The **initiator** is physical
(channel interference — treated by the EAP move, and outside firmware
reach). The **amplifier** is ours: on an impaired-but-alive link,

1. `mqtt.publish()` blocks up to ~10 s on a stalled socket (measured:
   `pubmax=10014` with the 32-slot queue pinned and **zero** WiFi loss);
2. each MQTT reconnect immediately adds ~13 state-reseed publishes
   (`publishAllStatesRetained()` + online + boot + alert) and the drain
   then pushes up to 4 status + 8 marker publishes per 5 ms pass;
3. recovery traffic therefore lands as a burst on the very path that just
   proved it cannot carry traffic, sustaining the failure and displacing
   `online 1` behind a zombie LWT.

Moderate loss becomes functional collapse. The locomotive is never at
risk (navigation is link-independent — field-proven); what collapses is
supervision and command delivery.

## T1 — restrained reconnect drain (the A/B CODEX proposed)

**Variant A (current):** on reconnect, full reseed burst + drain at
4 status/pass.
**Variant B (proposed):** for the first **10 s after each MQTT connect**:

- reseed publishes spread at **one per drain pass** (≈5 ms apart is still
  <100 ms for all thirteen on a healthy link — imperceptible; on a sick
  link it stops the pile-up);
- status drain capped at **1 per pass** (markers unchanged — they are the
  non-re-derivable stream and already bounded at 8);
- after 10 s, revert to current behaviour exactly.

No change to queue sizes, priorities, message content, or steady-state
behaviour. The 10 s restraint window is config, not constant.

**A/B protocol:** both variants built from 1.10 with the 1.11
instrumentation temporarily re-applied (diag build, stub rules apply);
stationary locomotive; impairment induced by parking the EAP back on the
Deco-contested channel for the test window (we own a reproducible
interference source — an unusual luxury); measure time-to-stable-session,
pub_drops, reconnect count, and command-delivery latency (timed STOPs)
across ≥5 impairment cycles per variant, recording **median, p95, and
maximum** for the latency metric rather than single values. **Pass rule
(CODEX finding 7 wording, adopted): B must be no worse than A on every
metric, strictly better on at least one transport-stability metric
(time-to-stable-session, pub_drops, or reconnect count), and must not
increase worst-case or p95 command latency.**

**Pre-registration for the A/B (CODEX Rev-2 non-blocking items, adopted
before data collection):** *time-to-stable-session* is defined as the
interval from an MQTT connect to the start of the first **60 s window
containing no reconnect, no `pub_drops` increment, and no broker-side
loopstat delivery gap > 3 s** — measured at the broker capture, stated
per cycle. The implementation report must record how one-per-pass
restraint is implemented, showing that inbound-first service, marker
priority, and the no-quorum reconciliation's slot priority and
generation protection are untouched.

## T2 — the wander tripwire (permanent loopstat fields)

The Decos auto-select channels and cannot be pinned; only the EAP can
dodge. Early warning must therefore be permanent. Add to `state/loopstat`:

- `"rssi"` — `WiFi.RSSI()` at publish time (int dBm);
- `"pubmax"` — windowed max `mqtt.publish()` duration ms, reset per
  loopstat window. **Scope pinned per CODEX finding 6: exactly the 1.11
  measurement — the status-queue drain publishes in `networkTask` only**
  (not markers, not the no-quorum reconciliation, not the direct
  netdiag path), preserving comparability with the 2026-08-08 evidence
  base. Widening the scope later is a deliberate spec change, not a
  drive-by.

Signature to watch (console/operator, no automation yet): **pubmax
spiking while rssi stays strong** = the mesh has wandered onto the EAP
again. Two fields, ~22 bytes worst case; loopstat budget re-check in the
implementation report. These two fields are the entire permanent footprint
of the 1.11 stub — everything else stays retired.

## Explicitly not in scope

Queue resizing; publish-on-change redesign (Change 3 lineage is already
the policy); any navigation, station, or authority change; the ESP-NOW
backup (own spec); zombie-LWT firmware mitigation (neutralised
console-side in v1.10.11).

## Sequence

Spec review → A/B diag builds and field measurement → implement the
winning variant + T2 as QUORUM 1.12 from `c39c439` → both profiles →
CODEX review → flash. Librarian: 1.12 enters the catalog as production
control, *built* until the field gate; the A/B builds are stubs and die
after the measurement.
