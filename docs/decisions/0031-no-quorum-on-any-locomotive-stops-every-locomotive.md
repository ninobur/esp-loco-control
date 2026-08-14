# 0031 — NO_QUORUM on any locomotive stops every locomotive

Status: Accepted (2026-08-13). Untestable until two locomotives run; that
defers verification, not the decision.

## Decision

During CTO operations, loss of trustworthy position on **any** locomotive
halts **all** locomotives.

Enforced **by absence, not announcement**: each locomotive runs only while
every expected peer is currently fresh (within `CTO2_PEER_STALE_MS`, 3 s) and
reporting a quorum-holding navigation state. A missing, stale, or
position-less peer report is a stop condition in itself. The rule must never
depend on the failing locomotive saying it failed — a locomotive that has
lost position may have lost its radio in the same event, or be powered off.

Stated positively: **run only while every expected peer is affirmatively
known to be navigating.** The stop is the default; running is the condition
that must be continuously earned.

## Context

Operator ruling, 2026-08-13, during the bubble design session.

M4 as written in `ROAD_TO_CTO.md` is a solo shape: LOST ends AUTO *for that
locomotive*. That is correct alone on the railway and insufficient with two
trains, because one locomotive's bad position endangers the **other** one —
CTO2's collisions were a follower computing clearance correctly from a leader
position that was confidently wrong. The follower had no defect; it had a
poisoned input. The only party that can protect a train from a peer's
navigation failure is itself, by refusing to move while any peer's position
is not affirmatively sound.

This inverts a dependency: M4 was the one milestone with no dependencies; the
fleet-wide form depends on peer awareness (M6). Recorded knowingly.

The principle is the lineage's own, extended one step: `LL_LBO_v1_2` (the
first sketch of the line) released a block hold only when the peer
*positively reported* a different block — silence never cleared anything.
This record applies the same logic to motion itself.

## Alternatives considered

**Announcement-based stop** — the losing locomotive broadcasts "I am lost,
stop." Rejected as the mechanism (fine as a supplement): the failure that
takes out navigation can take out the radio; a powered-off peer announces
nothing. CTO3's intent baseline already states that missing, stale, rejected
or inconsistent peer information is never evidence that traffic is clear.

**Leader-only or follower-only stop.** Rejected: which train is endangered
depends on where the error is, and the erroneous train does not know it is
wrong — that is what "confidently wrong" means.

**Degrade instead of stop** (slow to a crawl, hold last separation).
Rejected by the governing filter (operator, 2026-07-28): graceful degradation
is not a substitute for not degrading, and a moving train with an unsound
peer position is the exact CTO2 accident geometry.

## Consequences

- **"Expected peer" needs a definition** before implementation: how a
  locomotive knows the fleet roster (a peer once seen this session? a
  dispatcher-declared roster? enrolment at formation?). This is the open
  design item of the record — `LL_LBO_v1_2`'s ghost-block problem
  (`CMD_CLEAR_ALL` as manual escape) shows what a wrong roster answer costs:
  a removed locomotive that still counts as expected halts the railway until
  cleared. Owed at v1.14 scoping.
- A stopped fleet needs an operator path back: recovery is DECLARE on the
  lost locomotive, and the fleet resumes only when every expected peer is
  again fresh and quorum-holding. No autonomous restart of the lost unit
  (M4 unchanged).
- The 1.13 HARD_BOUND advisory (decision 0023) becomes fleet-relevant: the
  stopped healthy locomotive's console shows *why* the fleet is stopped via
  the lost peer's terminal snapshot.
- Cannot be tested with one locomotive. First exercised in M7's induced-
  failure test (peer powered off mid-lap, handled without contact).
- `docs/CTO3/BUBBLE_V1_SPEC.md` §2.4 carries the operational statement of
  this rule and now cites this record.

## References

- `docs/CTO3/BUBBLE_V1_SPEC.md` — the routine this protects
- `docs/CTO3/CTO3_INTENT_BASELINE.md` — silence is never clearance
- `docs/ROAD_TO_CTO.md` — M4 (solo form), M6, M7 crossing test
- `docs/decisions/0023-*` — the advisory that explains a fleet stop
- CTO2 collision history: `docs/CTO2_HARVEST.md`
