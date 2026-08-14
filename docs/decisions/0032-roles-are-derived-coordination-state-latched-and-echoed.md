# 0032 — Roles are derived coordination state, latched and echoed

Status: Accepted (2026-08-13). Supersedes, in part, the r9/r12 packet
invariant as carried into the CTO2 audit's original recommendation.

## Decision

Leader/follower is **coordination state, not authority**. Four rules:

1. **Derived, never negotiated.** Each locomotive computes its role locally
   from shared self-truth — the GoldCore Q1/Q2 pattern with markers replacing
   blocks: a fresh same-direction peer occupying or immediately preceding my
   position ahead makes me TRAILING; one within pairing range behind makes me
   LEADING; neither leaves me UNPAIRED, a real operating state.
2. **Latched at pairing range**, where geometry is decisive, and persistent
   until dissolution (peer staleness, direction change, CE severance,
   operator reset) — `CTO3_DESIGN_NOTES.md`: assigned once, stay assigned.
   At long range, derived order (smallest my-rear-to-your-front leads,
   ~12-marker hysteresis, lower loco ID breaks ties) decides only *who waits*
   during formation.
3. **Echoed as self-truth.** After latching, each locomotive broadcasts its
   own conclusion ("I follow 9950011") as a fact about its own state.
   Disagreement between the two echoes is a detectable fault: both hold.
4. **The wire carries truth, never authority** — Sam's restatement, adopted
   verbatim: *a locomotive may not transmit another locomotive's required
   behavior. Each locomotive broadcasts its own physical and operational
   truth. Leader/follower relationship may be established from shared peer
   information and retained — and stated — as coordination state.*

## Context

The r9-era comment, carried into r12: the peer packet must not transmit
"brake points, motor commands, pair assignments, leader/follower identity, or
station instructions." The 2026-08-13 audit initially treated this as a
constraint the design had to satisfy. The operator had Sam trace its
provenance; Sam's finding, verified against local sources and adopted:

- The rule's intent was right — packets carry facts, not commands — a
  reaction to the stale-state failures of distributed instruction-passing.
- It overshot by lumping a *relationship* in with *commands*. A motor command
  says "you do this"; a latched relationship says "for this session, Otto is
  ahead of Toby" — the same category as position.
- **r12 itself never held the purist line**: `CtoPeerPacket` broadcasts
  `trafficStopForId` ("I am stopped *for locomotive X*") and the other side's
  restart logic reads it. A role echo is the same category of fact.
- The original CTO ran persistent roles successfully: `LL_CTO_v2_1_GoldCore`
  (read 2026-08-13) self-assigns ROLE_TRAILING/ROLE_LEADING at block arrival
  from the local peer table, latches per encounter with encounter IDs, and
  accepts DEPARTING only from `trailingLeaderId`.
- What the v1.0→v8.1 Circuit Express history condemns is **handshake
  negotiation**, and only that. Geometry-derived, persistently-held roles are
  the design that *followed* that lesson.

Why the echo (rule 3) is required and not decoration: pure derivation has a
race. Near a tie, the two locomotives can latch from different packet pairs
and each conclude it leads — both wait at stations, silently, forever. The
echo converts an undetectable split-brain into a visible disagreement with a
conservative response. Proximity-gated latching (rule 2) makes the race
nearly impossible; the echo makes it harmless.

## Alternatives considered

**Pure derivation, no echo.** Rejected: the split-brain race above. Cheap
insurance against the one failure derivation cannot see.

**Handshake negotiation** ("I propose I lead — confirm"). Rejected by eight
major versions of Circuit Express history and by the stale-state failure
class the r9 rule was written against.

**Dispatcher-assigned roles.** Rejected: spacing intelligence lives aboard
(bicameral, decision 0002). The dispatcher assigns *missions* — CE express/
local is exactly that — never roles or spacing.

**Keeping the invariant as written.** Rejected: it was implementation
discipline stated too absolutely, already breached healthily by r12's own
`trafficStopForId`, and it would forbid the echo that closes the split-brain.

## Consequences

- The CTO2 audit's original "do not reintroduce" recommendation narrows to
  handshake negotiation only (its 2026-08-13 amendment says so).
- The role echo needs a wire slot. Candidate: the vestigial
  `TrainPacket.roleId`, which would mean no struct change and no version
  bump; confirm at v1.14 scoping rather than assume (`CTO2_VERSION 3` and
  `ESPNOW_VERSION 14` are frozen per `docs/CLAUDE.md`).
- Role dissolution on peer staleness composes with decision 0031: a stale
  peer dissolves the pair *and* stops the fleet.
- `docs/CTO3/BUBBLE_V1_SPEC.md` §5 is the operational form of this record.
- CE re-pairing needs no special code: severance returns both to UNPAIRED,
  and the same Q1/Q2 latches the new relationship at catch-up, local ahead,
  local leads.

## References

- `docs/CTO2_AUDIT_DISPOSITION.md` — the amendment this record formalizes
- `docs/CTO3/BUBBLE_V1_SPEC.md` §§4–6, 9
- `docs/CTO3_DESIGN_NOTES.md:231-237` — relative order cannot change
- iCloud `LL_CTO_v2_1_GoldCore.ino` — Q1/Q2, encounter latching, working roles
- `archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` — the invariant and its
  own healthy breach (`trafficStopForId`)
- Decisions 0002 (bicameral), 0031 (fleet stop)
