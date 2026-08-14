# CTO2 audit — disposition inventory

**Subject:** `archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino`, 2346 lines.
**Purpose:** M6's first task, per `ROAD_TO_CTO.md` — *an audit, not a port.*
**Status:** first pass, 2026-08-13. Reading only; nothing ported, nothing built.

---

## Headline finding: CTO2 already abandoned negotiated leader/follower

Operator direction, 2026-08-13: *"Determine who is leader and who is follower.
This is a loco negotiated behavior. This is also in CTO2."*

**r12 states the opposite, three times, deliberately:**

> *"No peer transmits a brake point, motor command, pair assignment,
> **leader/follower identity**, or station instruction."* (line 237)

> *"Traffic is not a role. It is a temporary interruption to the local station
> mission."* (line 292)

> *"not a role, pair, or assigned partner. All fresh locomotives are
> relevant."* (line 701)

This was not an oversight. `CTO2_HARVEST.md` records that the Circuit Express
pairing algorithm ran from **v1.0 to v8.1** before reaching working status, and
that the resolution was *"restrict CE pairing to CE_EXPRESS only; **let geometry
create the pairing sequence after CE**."*

So the lineage tried negotiated roles, spent eight major versions on it, and
concluded that **geometry should determine order instead of negotiation**. r12
is the design that came after that conclusion: every locomotive independently
derives who is ahead from boundary geometry, and nobody is told their role.

The mechanism already exists — `peerIsAheadByBoundaries()`:

> *"A peer is ahead only when the forward gap from self front to peer rear is
> smaller than the opposite gap from peer front back to self rear."*

Which is precisely what `CTO3_INTENT_BASELINE.md` requires: *"traffic reasoning
concerns the gap between occupied envelopes along a valid route. Point-to-point
Hall distance is insufficient."*

**Recommendation:** keep geometry, do not reintroduce negotiation. It costs
nothing to decide later — geometry gives the same answer without a handshake to
lose, and a negotiated role is exactly the kind of state that goes wrong when a
peer drops. If negotiation is still wanted, it should be argued against this
history rather than assumed absent.

### Amendment, 2026-08-13 — the invariant was stated too absolutely (Sam)

The operator had Sam trace the invariant's provenance. Sam's finding, adopted
here: the rule "no packet may transmit leader/follower identity" (r9 originally,
carried into r12) was an implementation discipline — **packets carry truth, not
authority** — that overshot by lumping a *relationship* in with *commands*. A
motor command says "you do this"; a latched leader/follower relationship is
shared coordination state, the same category as position. CTO3 had already
re-adopted persistence: `CTO3_DESIGN_NOTES.md:231-237` — relative order cannot
change; roles are assigned once from the peer table and stay assigned; "that is
the whole coordination model."

**r12 itself already crosses the purist line, in the healthy direction.** The
frozen `CtoPeerPacket` carries `trafficStopForId` — "I am stopped *for
locomotive X*" — a conclusion about a peer relationship, broadcast as
self-truth, and consumed by the other side's restart logic
(`p.trafficStopForId == LOCO_ID`). A broadcast "I have concluded I follow X" is
the same category of fact.

The rule as restated (Sam's phrasing, adopted): *a locomotive may not transmit
another locomotive's required behavior. Each locomotive broadcasts its own
physical and operational truth. Leader/follower relationship may be established
from shared peer information and retained — and stated — as coordination
state.*

**Design consequence.** Role assignment is derived, not negotiated: each
locomotive computes the same gap comparison from the same pair of broadcasts
(smallest my-rear-to-your-front leads), with a ~12-marker hysteresis band
resolved by lower-loco-ID, latched at formation, dissolved on peer staleness.
Pure derivation has a race: near a tie, the two locomotives may latch from
*different packet pairs* and each conclude it leads — both wait, silent
deadlock. The corrected rule permits the fix: **after latching, each broadcasts
its derived role as its own state.** Disagreement between the two broadcasts is
then a detectable fault with a conservative response (hold, do not form),
rather than an undetectable split-brain. This supersedes the paragraph above
only in what "negotiation" covers: handshake-assigned roles stay abandoned;
persistent derived roles, stated on the wire as self-truth, are legitimate and
were part of the original CTO's working design (persistent LEADER/TRAILING
profiles; `trailingLeaderId` in the v3.2 CircuitExpress lineage — Sam's
citation, not present in this repository).

---

## Disposition inventory

### Port largely as-is

| item | detail |
|---|---|
| `CtoPeerPacket` | 26 fields, packed, versioned. `CTO2_MAGIC 0xC4`, `CTO2_VERSION 3`. **`docs/CLAUDE.md` forbids changing this struct or version** — dispatcher and peer compatibility depend on it. |
| `PeerEntry` / `peerRegistry[8]` | Full per-peer record. `MAX_CTO_PEERS 8`. A later report from one locomotive cannot overwrite another's record — satisfies the CTO3 registry requirement directly. |
| Freshness machinery | `CTO2_PEER_STALE_MS 3000`, `peerFreshIndex()`, `freshPeerCount()`, `peerIndexById()`, `peerSlotForId()` with LRU eviction. This is the foundation the fleet-stop rule needs. |
| Broadcast cadence | `CTO2_STATUS_INTERVAL_MS 500` — 2 Hz self-truth, independent of mode, motion or station state. |
| Radio health counters | `peerRxAccepted`, `peerTxAttempts`, `peerTxImmediateErrors`, carried **in the packet** as well as locally, so each locomotive can see its peer's radio health rather than only its own. |
| Version rejection | r9-and-below packets are rejected rather than guessed. Correct and worth keeping. |

### Port after review

| item | why review |
|---|---|
| `peerIsAheadByBoundaries()` | Reasoning is sound and matches CTO3 intent. Needs QUORUM's bound as input instead of CTO2's fixed offsets — see rewrite. |
| `peerGenuinelyStopped()` | Distinguishing stopped from silent is right; verify the criteria against QUORUM's motion state. |
| Traffic approach ladder | Stopped-obstruction approach from 18 Hall-to-Hall through 20 → 15 → 10 pKPH, 300 ms/PWM final ramp at 10, restart at 12. Field-proven shape, but the speeds are pKPH throttle targets, not measured speed. |
| `TrafficPhase` state machine | `CLEAR / STOPPED_LEAD_APPROACH / FINAL_RAMP / WAIT_FOR_CLEARANCE`. Small and comprehensible. Traffic as interruption-not-role is the right model. |
| MHE (`mustHoldEligible`) | **Station synchronization only**, explicitly excluded from ordinary traffic proximity, default OFF so an unconfigured locomotive creates no obligation. This is the mechanism the operator's station choreography needs. |

### Rewrite — consumes position without a bound

| item | problem |
|---|---|
| `CTO2_STANDARD_FRONT_OFFSET_MM 5` / `REAR_OFFSET_MM 5` | **Fixed constants with no uncertainty term.** They assert consist geometry as though position were exact. This is the CTO2 failure in one line: a follower computing clearance correctly from a leader position that was confidently wrong. Must become QUORUM's navigation bound composed with the configured extent. |
| Extent values themselves | 5/5 markers contradicts decision 0030, which sets **+2 ahead / −4 behind** and makes it per-locomotive configuration rather than a shared constant. |
| `TRAFFIC_TOUCH_HALL_GAP_MM` | Derived from those two constants, so it inherits the defect. |
| Any consumer reading `opNav` position directly | Must consume a bound, never a point. |

### Discard

| item | why |
|---|---|
| Speed coordination from Hall timing | Superseded. r11's own header already disclaims it: *"does NOT claim continuous moving-train speed matching."* |
| `TrainPacket` `roleId` / pairing fields | Vestigial under the geometry model. `ESPNOW_VERSION 14` and the struct itself must not change, but the role fields need not be populated or read. |

---

## Naming hazard, carried forward

Throughout CTO2, `MM` means **mile marker**, so `CTO2_STANDARD_FRONT_OFFSET_MM`
is 5 *markers*, not 5 millimetres — while `spacingMm[]` in the same file is
millimetres. Decision 0030 flags this; the audit confirms it is pervasive in the
material being ported. Any new field should carry an unambiguous unit.

---

## The operator's station choreography, mapped

Dictated 2026-08-13:

1. Lead stops at platform (current stop behaviour).
2. Follower stops at lead's stop **−5**.
3. Follower arrival releases lead after a **10 second** dwell.
4. Follower moves forward when the area is not occupied.
5. Follower stops at platform.
6. Follower dwells **15 seconds**, then continues to the next station.
7. **Follower does not rear-end the lead if the lead stops.**

**CTO2 already separates the two mechanisms this needs**, which is the strongest
argument for porting rather than rewriting:

- Steps 2, 3 and 5 are **station synchronization** — MHE. The lead holds at the
  platform until the follower is confirmed at its hold point, then dwells and
  departs. `stationHoldingForRear` and `stationHoldRearId` already exist.
- Steps 4 and 7 are **traffic proximity** — the approach ladder and the
  12 Hall-to-Hall restart, active regardless of station state.

Keeping them separate matters: a follower must not rear-end a lead that stops
*anywhere*, not only at a station. r12's decision to make traffic universal and
MHE station-only is what delivers that, and it should be preserved.

**Open questions for the operator**, none blocking:

- Is "lead stop −5" five markers, matching CTO2's boundary geometry?
- The 10 s and 15 s dwells are fixed here; CTO2 supports randomized ranges
  (12–20 s normal, 0–10 s for the lead in a bubble). Fixed or ranged?
- "Area is not occupied" — the audit reads this as the existing 12 Hall-to-Hall
  restart clearance rather than a new concept. Confirm.

---

## What this audit does not cover

- The station-stop template itself, which lives in current QUORUM, not CTO2.
- Whether r12 compiles. Its own header notes it *"has not been compiled in
  David's Arduino environment."*
- Line-by-line review of the DNA navigation in r12 — superseded by QUORUM and
  out of scope.

## References

- `archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino`
- `docs/CTO2_HARVEST.md` — the CE v1.0→v8.1 pairing history and its resolution
- `docs/CTO3/CTO3_INTENT_BASELINE.md`, `docs/CTO3/resources/CTO2_BUBBLE_PRINCIPLE.txt`
- `docs/decisions/0030-*` — extent is +2 / −4 markers, applied by the producer
- `docs/CLAUDE.md` — `ESPNOW_VERSION 14` and `CTO2_VERSION 3` must not change
