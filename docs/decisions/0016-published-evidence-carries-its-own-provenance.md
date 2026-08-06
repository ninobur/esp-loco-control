# 0016 — Published evidence carries its own provenance; an unmeasured quantity is published as unknown, never as a value

Status: Accepted (2026-08-06). Implemented in QUORUM 1.7.

## Decision

Two rules, applied first to the INA219 restoration (0012) and normative for
every sensor CTO3 adds afterwards:

1. **A quantity the locomotive did not measure is published as `null`, or
   not published at all — never as a default value.** No `0.00 V` standing
   in for "no sensor"; no `lowvolt = 0` standing in for "nothing is
   watching."
2. **The mm/marker line is the evidence record for an interval.** Every key
   needed to bin that interval for the CTO3 §9 speed model rides the same
   message, so a consumer never has to join two topics on a timestamp. This
   **amends the R21 §5.1 payload contract** ("the raw event fields plus dt,
   timing_gate, dt_expected, dt_conserve_ratio. NOTHING else") to admit
   `pwm` and `v`.

## Context

Restoring the INA219 produced two places where the obvious implementation
would have published a fabricated measurement. Both were caught in review of
the first cut, not in the field:

- `v` on the marker line read `0.00` for up to five seconds after boot,
  before the first I²C sample — and markers can be detected in that window.
  The §9 table normalizes by `v_now / v_ref`; those intervals would have
  been divided by a zero that looked like data.
- The connect-time retained reseed published `state/lowvolt 0` from a
  locomotive whose INA219 had never answered. Otto, whose sensor is
  currently faulted, would have announced "voltage is fine" on the authority
  of nothing.

The second is the CTO2 failure in miniature. CTO2 did not crash; it
converted an *absence* of qualified peer information into `TRAFFIC_CLEAR`
and cruised a follower into a near-miss. CTO3 §0 and §14 name that inversion
as the one durable requirement to survive CTO2 — and it is not a
traffic-layer rule. It is a rule about what a locomotive is permitted to
assert. A defaulted zero is the same lie in a different subsystem.

The §5.1 amendment is separate housekeeping with the same root: the CTO3
spec asked for `v=` "alongside `pwm=`/`dist=`", describing a SOLONAV-era
`key=value` line the QUORUM JSON contract had already replaced. `pwm` was
not in fact on the line, and the §9 table cannot be keyed without it.

## Alternatives considered

- **Publish `0.00` and let the Pi-side parser filter it.** Rejected: it
  moves the burden to every future consumer, and a consumer that forgets is
  silently wrong rather than visibly missing data. `null` is unignorable.
- **Publish `lowvolt 0` and rely on the operator noticing the power tile is
  stale.** Rejected for the same reason CTO2's `TRAFFIC_CLEAR` was wrong: a
  confident-looking value suppresses the question. An absent topic prompts
  it.
- **Suppress `state/lowvolt` entirely until a low-voltage response is
  actually implemented.** Rejected: the dashboard already binds the topic,
  and the measurement is useful to the operator before it is useful to the
  controller.
- **A separate `telem/marker_context` topic instead of amending §5.1.**
  Rejected: it recreates the timestamp-join problem the marker line exists
  to avoid, and doubles the message rate on the one path with a dedicated
  drop-newest queue.

## Consequences

- Every CTO3 evidence source — IR, Hall fusion, future sensors — must be
  able to say "I do not know," and its published form must have a
  representation for that. A sensor whose telemetry cannot express absence
  is not finished.
- Consumers (the Pi parser, the dashboard, and later the peer registry) must
  handle `null`. The §9 calibration parser must **reject** rather than
  impute intervals with `"v":null`.
- This generalizes to §3 self-truth broadcast: "navigation validity + truth
  source" already carries provenance by design, and §5's "stale ≠ absent ≠
  clear" is the peer-registry statement of the same rule. 0016 makes it the
  house rule for evidence rather than a per-subsystem habit.
- R21 §5.1 must be amended in
  `docs/QUORUM_v3_0_implementation_spec.md` once CODEX has reviewed the
  payload change. Worst-case payload is now 198 bytes against a 320-byte
  buffer.
- Does **not** decide what a low-voltage flag should *do*. Nothing reads it.
  That remains open, and like the brake question in 0011 it must answer the
  bicameral question (0002) before it actuates anything.

## References

- `docs/QUORUM_1_7_IMPLEMENTATION_REPORT.md`; commit `6daaa05`
- Decision 0012 (INA219 restored); 0011 (brake — same "restored, semantics
  still open" shape); 0002 (bicameral)
- `docs/CTO3/CTO3_SPEC.md` §0, §3, §5, §9, §14
- R21 §5.1 marker payload contract (amendment pending review)
