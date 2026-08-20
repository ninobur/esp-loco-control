# 0039 — A third node may relay peer truth, under freshness and novelty gates

Status: Proposed (operator direction 2026-08-20; built, flashed to the spare
ESP32, bench-verified; no field verdict)

## Context

Layer 5 coordination assumes each locomotive hears the other's 2 Hz
`CtoPeerPacket` directly. The 2026-08-19 evidence says that assumption fails in
a way no single-cause model has survived:

- Distance is excluded — 99.9% delivery at 72 markers separation, the same
  separation that gave 0% earlier the same evening; 99–100% sustained at 65–80.
- A pairwise line-of-sight rule (mm 040–070 against mm 160–016) explains 7 of
  34 STALE events. It accounts for total collapses, not the 60–90% partial loss
  at intermediate geometries.
- Measured baseline over ~1000 packets each way: Otto→Toby 83.2%,
  Toby→Otto 87.0%. Both nodes on channel 11, `chg=0`, `txf=0` — not a channel
  divergence.

Three single-cause models were proposed and each was too simple for the data.
The agreed response was to stop fitting models to convenience data collected
from inside the two failing endpoints, and to instrument instead.

## Decision

A **third ESP32** joins the railway as an independent observer, and may relay.
It is a diagnostic instrument (`firmware/test-programs/ESPNOW_REPEATER/`), not
a QUORUM capability, and it holds **no authority**: it originates no packet of
its own, subscribes to no locomotive command topic, and cannot command, stop,
or steer anything.

It has three modes, switchable at runtime on `ngr/survey/<node>/cmd`:

| mode | receives | records | transmits |
|---|---|---|---|
| `listen` | yes | yes | never |
| `shadow` | yes | yes, **including what it would have relayed and why** | never |
| `repeat` | yes | yes | qualifying frames only |

**Relaying is permitted only under three gates, and each is load-bearing:**

1. **Freshness** — a frame older than 100 ms at the forwarding decision is
   dropped, never relayed late.
2. **Novelty** — per source, only a *strictly greater* sequence number is
   relayed. No duplicate and no rollback can leave the node.
3. **Rate** — a 15 ms floor between transmissions (which *defers*, it does not
   discard) and a hard cap of 24 transmissions per second.

## Why the gates are not optional

`ctoAcceptPeer()` in `QUORUM.ino` applies **no sequence-monotonicity test**:
whatever `CtoPeerPacket` arrives last becomes peer truth. Peer `hallMm` and the
marker boundaries drive the 18/12/6 decel ladder and the follower hold, and
`rampFalling` compares consecutive samples. An ungated relay could therefore
roll a peer's believed position backwards or distort deceleration detection —
it would be injecting false truth into the braking path, not adding coverage.
The freshness and novelty gates are what make relaying merely *redundant* paths
rather than *new* claims.

This is stated as a constraint on the relay because fixing it in QUORUM would
be a wire-and-acceptance change to a frozen contract, which this decision does
not make. If relaying is ever promoted out of `test-programs/`, a
monotonicity test in `ctoAcceptPeer()` should be reconsidered on its own merits.

## What the relay must prove before it is trusted

The operator's criterion, unchanged by switching relaying on:

> It is not enough for the monitor to hear "most" packets. It must hear the
> locomotive whose packet the other locomotive is missing, during those
> specific multi-second gaps.

A high aggregate delivery figure from the third node proves nothing on its own.
Only per-episode coincidence does — which is why every mode, `repeat`
included, keeps the full per-frame record (source, sequence, receive time,
RSSI, and the sender's own `hallMm`/`mapDir` decoded from the packet).

## Placement

The dispatcher position (mm 116–160) is **not** the survey location of record.
It sits inside the observed shadow and corresponds to the Bamboo/Arches trouble
area; a relay that shares the blind spot adds no path diversity. It may serve
as a sensor documenting coverage *inside* the known shadow, or as one node in a
future two-relay arrangement with the spare on the opposite side.

## Constraints honoured

- `ESPNOW_VERSION`, `CTO2_VERSION` (3) and `CTO3_ECHO_VERSION` (1) unchanged;
  the structs are copied read-only and frames are relayed byte-for-byte.
- No MQTT topic or payload used by QUORUM or the console is touched. The node
  publishes only under `ngr/survey/<NODE_NAME>/`.
- All devices remain on the locomotives' ESP-NOW channel (11). The node
  measures its own channel and publishes `CHANNEL_MISMATCH` on divergence,
  because a receiver on the wrong channel is silent in exactly the way a
  perfect radio shadow is silent.

## Single-relay assumption

With one repeater there is no relay loop: locomotives never forward. The
novelty gate stops a node re-relaying a sequence it has already relayed, but a
**second** repeater is a different problem and must be reviewed before
deployment.

## Consequences

- Coverage evidence now comes from a node that is not one of the two failing
  endpoints — the first measurement in this investigation that is not
  self-reported by a party to the failure.
- If relaying materially raises delivery, the direct-path question remains open
  and must not be considered answered; the relay would be masking it.
- Recording continues in every mode, so `repeat` sessions remain admissible
  evidence rather than a treatment that destroys the measurement.
