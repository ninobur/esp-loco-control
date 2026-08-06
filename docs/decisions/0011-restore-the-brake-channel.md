# 0011 — The brake channel is restored to the control firmware

Status: Accepted  (operator, 2026-08-06)

## Decision
`cmd/brake` returns to QUORUM: subscription, handler, and a live
`state/brake` replacing the retained inert `"0"`. Restoration is the work
item the CTO3 spec associates with the "QUORUM 1.5" label (the firmware
itself is at QUORUM_1_6; the label names the plan step, not the current
sketch version).

## Context
The dashboard has published `cmd/brake` into the void since the SOLONAV
rewrite: SOLONAV dropped the channel, QUORUM inherited the gap, and the
firmware published an inert retained `state/brake "0"` so the console's
parsing would not break — compatibility maintained with a deleted
capability, the exact silent drift this decision log exists to prevent.
Discovered during the dashboard v1.10.3 archaeology; decision pending
since.

## Alternatives considered
- Remove the brake control from the dashboard — rejected: the operator
  wants the capability, not the amputation.
- Keep publishing into the void, labelled — rejected: a labelled lie is
  still a lie.

## Consequences
Two questions the restoration must answer explicitly, on the record:
**what brake physically means** — in r12 `brakeValue` was stored and
republished but never reached the motor (three uses: declare, store,
publish), so the lineage has never actually implemented braking on a
PWM-only locomotive; and **which chamber it belongs to** (per 0002 —
r12 refused brake under `dispatcherAuto`, which predates the bicameral
ruling and must be re-derived, not copied). Dashboard needs no change;
its publish path was fixed in v1.10.3 and verified to the broker.

## References
`DASHBOARD_1_10_3_IMPLEMENTATION_REPORT.md` (root cause, layer table);
`archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino:1553,2237`;
`docs/CTO3/CTO3_SPEC.md`; decision 0002.
