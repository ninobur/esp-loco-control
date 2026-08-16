# 0037 — A pairing dissolves when nose-tail order inverts

Status: **Proposed** (operator ruling 2026-08-16). Implemented in
`QUORUM_1_16Rb`, **NOT flashed**. Amends 0032 in part.

## Decision

A latched leader/follower pairing dissolves when the nose-tail distances
between the two locomotives **invert** — that is, when the partner a
locomotive claims to lead is measurably ahead of it, or the partner it claims
to follow is measurably behind.

Inversion must be **clear** and **confirmed**:

- clear — the new near side must beat the far side by
  `CTO_ORDER_MARGIN_MARKERS` (12, the pairing range), so a pair sitting near
  the diametric point of the loop, where the two arcs are nearly equal and
  the nearer one is noise, cannot chatter;
- confirmed — the condition must hold for `CTO_ORDER_CONFIRM_N` (3)
  consecutive service passes.

On dissolution the locomotive publishes `CTO_ORDER_INVERTED` with both arcs
and the role being abandoned, then `CTO_UNPAIRED why=ORDER_INVERTED`, and
re-derives a role on the same pass from the geometry that actually exists.
If the two are now too far apart to pair, no role is claimed — which is the
honest answer, not a failure.

Everything else about 0032 stands: roles remain derived, latched and echoed,
never negotiated, and they still persist through ordinary separation, through
staleness, and through a partner's silence.

## Context

Observed in the field, 2026-08-16, during the 1.16R Bubble test. The operator
paused Toby at Grillers. Otto — latched LEADER from an earlier encounter —
ran the entire route and arrived **12 markers behind** the locomotive he was
nominally leading, braking for it (`ZERO_RAMP`, PWM falling) while the
console displayed him as the leader. The pairing was internally coherent and
geometrically false.

0032 latches roles to stop them flapping, and lists exactly four dissolution
conditions: my direction changed since the latch, the partner's direction
changed, `cmd/cto clear`, `cmd/cto off`. **Physical reordering is not among
them**, because the model assumes a linear order.

A loop has no linear order. Whenever one locomotive laps another — or simply
stops while the other continues — the physical ordering genuinely inverts, and
the latched labels then describe a railway that no longer exists.

## What a false role actually costs

**Corrected in review.** The first draft of this record claimed the role drove
exactly one behaviour — the platform dwell — and that traffic protection never
consults it. The first half was wrong. A latched role has three consumers:

1. **The dwell.** A confirmed follower dwells `CTO_FOLLOWER_DWELL_MS` (5 s,
   *shorter*, per 1.14A "the leader stops waiting for the follower"); everyone
   else dwells the ordinary 15 s.
2. **The role-conflict hold, which carries motion authority.**
   `ctoServiceEchoCheck()` raises `ctoEchoConflict` when the partner's echo
   claims *my* role with me as its partner, and `ctoLimitPwm()` returns **0**
   on that conflict. A false role is therefore an input to a full stop.
3. **My partner's copy of both.** `ctoTxEcho()` puts my role on the wire, so
   my claim feeds the partner's confirm/conflict computation — and thus the
   partner's stop authority. Local-only reasoning about "what my role drives"
   is incomplete by construction.

What *is* true, and what the field evidence shows, is that **traffic
protection, the occupancy bubble, separation and the fleet stop are gap-based
and never consult the role** — which is why Otto braked correctly for a
locomotive he believed he was leading. The inversion was not a collision
hazard. It was a false claim feeding a stop authority, a misapplied dwell,
and a console telling the operator something untrue.

That correction changes an obligation, recorded here rather than discovered
later: **dissolving a pairing clears an active role conflict, which releases a
PWM-0 hold.** `ctoDissolve()` now publishes `CTO_ROLE_CONFLICT_CLEARED` when
it does so. Every release of a full stop must be visible; 0037 introduced a
new geometric path into that clear, and in its first draft the path was
silent.

## Alternatives considered

**Leave it; fix only the display.** Rejected. The console would then be
honest about a state the firmware still holds wrongly, and the dwell would
still be misapplied. The defect is in the model, not the rendering.

**Dissolve on separation beyond a multiple of pairing range** (the first
proposal). Rejected by the operator in favour of the inversion test, and
rightly: separation is a proxy. Two locomotives can separate widely and
re-converge with their order intact, in which case dissolving discards a
still-true pairing; and they can invert while remaining close, in which case
separation never fires. The inversion test names the actual fault.

**Re-derive roles continuously, abandoning the latch.** Rejected — that is
precisely the flapping 0032 exists to prevent, and near the diametric point
it would thrash every pass.

**Dissolve on a bare arc comparison, no margin or confirmation.** Rejected on
the same grounds: at `gap_ahead ≈ gap_behind` the comparison is noise.

## Consequences

- **0032 amended in part**: the dissolution list grows from four conditions to
  five. Its principle — derived, latched, echoed, never negotiated — is
  untouched; what changes is that the latch is no longer permitted to outlive
  the geometry it asserts.
- **0031 untouched and explicitly protected**: a stale or silent partner still
  does *not* dissolve a pairing. That remains the fleet stop's jurisdiction,
  and `test_cto_roles.py` pins it so 0037 cannot poach it.
- **CTO gained its first tests, ever.** Until this change the pairing layer
  had no coverage at all: the `esp_now` shim drops every send and never fires
  a receive callback, so the peer registry was permanently empty and every
  pairing path was dead code under replay. The harness now injects peer status
  and role-echo packets (`peer`, `peer_echo`, `cto`, `cto_dump`), and
  `test_cto_roles.py` pins formation, all five dissolutions, the confirmation
  gate, the anti-chatter margin, and the dwell.
- **A documentation regression was found on the way** and corrected: the
  `ctoDwellMs()` comments claimed a 20 s follower dwell against a 5000 ms
  constant. 1.14A had corrected that wording; the 1.14A "rebuilt clean" commit
  reintroduced it. The test failed against the comment and was right to.
- **An ordinary lap produces a window with no pairing at all**, and this is
  intended rather than incidental. Outside the bound-overlap band the two arcs
  sum to 159, so a follower's inversion test fires once the partner is ~92
  markers ahead, while re-latching needs it within pairing range — leaving
  roughly 61 of 171 positions where the locomotives are dissolved to NONE and
  stay there until they close up again. No role is claimed because none is
  true; the dwell reverts to solo rules and traffic protection is unaffected.
- **The inversion test uses only trusted geometry.** It is gated on the same
  three conditions as latching — fresh, same direction, and the partner's own
  navigation sound. A partner in NO_QUORUM still transmits boundaries derived
  from a position it does not believe, and in the first draft those boundaries
  could dissolve a pairing on the very pass fleet stop declared NO_POSITION.
  A discredited partner is 0031's business.
- **A staleness gap restarts the confirmation run.** Two inverted samples
  either side of an unobserved silence are not three consecutive
  observations; the counter is reset whenever usable partner evidence is
  missing, and `ctoDissolve()` owns the reset so no future dissolution path
  can inherit a partial count.
- **Bound overlap is a known blind spot**: when the consists are within about
  five markers the arcs wrap and neither inversion condition can fire. At that
  range the contact guard in `ctoLimitPwm` governs, and a role claim is the
  least of the operator's concerns. Stated, not fixed.
- **Not flashed.** Gate: operator and CODEX review, then supervised track
  time with `CTO_ORDER_INVERTED` watched — the field scenario that produced
  this record is easy to reproduce deliberately (pause one locomotive, let the
  other lap).

## References

- `field-records/20260816_QUORUM_1_16R_SECOND_SESSION.md` — the observation
- `firmware/QUORUM/tests/test_cto_roles.py` — the pinned behaviour
- Decisions 0031 (fleet stop by absence), 0032 (roles latched and echoed),
  0033 (separation is the bubble plus six)
