# CTO3 — design notes carried forward

Source: Codex review of `SOLONAV_2_5.ino`, 2026-07-27, plus the SOLONAV 2.x
development session. **None of this belongs in SOLONAV.** SOLONAV is a
single-locomotive sketch; peer awareness was added to it during the 2.5 round
and then removed as out of scope. These notes exist so that work is not lost
and not repeated.

Nothing here has been field tested. The peer code that motivated it never
compiled.

---

## 1. A silent locomotive must not become empty track

**The problem.** A peer-expiry timeout answers "is this locomotive still
talking?" It does not answer "is it still on the rails in front of me?" With a
simple timeout, a locomotive that loses Wi-Fi, derails, or runs its battery
flat disappears from the peer table and the layout then looks clear — which is
precisely backwards, because a dead locomotive is the obstruction most worth
knowing about.

Retained MQTT does not solve this. It delivers the last message to a late
subscriber but does not distinguish a current broadcast from stale retained
state.

**The design.** Separate two things that a single timeout conflates:

- **Communicating** — heard within the timeout. Its motion, direction, speed
  and intent may be trusted and used for dynamic spacing.
- **Occupying** — last known position bound. This does **not** expire on
  silence. It persists as an unresolved obstruction.

After the timeout, stop trusting the peer's *motion*, but keep its last
occupancy bound as a block that must be treated as occupied until cleared by an
operator action or by a stronger observation (for example, another locomotive
passing through that section and reporting clear).

Failure direction matters: a stale obstruction costs a delay, a forgotten one
costs a collision.

---

## 2. Measure separation from the follower's own front bound

**The problem.** The 2.5 implementation measured the gap from the follower's
dead-reckoned position (`navMm`). A locomotive that is misreading markers may
also be *missing* them, so it can be ahead of its own odometer. Measuring from
`navMm` leaves that known uncertainty out of the safety calculation.

**The design.** Use `front_bound_mm` — dead reckoning plus the forward margin —
as the follower's leading edge, and the leader's `rear_bound_mm` (its last
*confirmed* marker, not its dead reckoning) as the target. Both bounds are
already computed; using the optimistic ones is a choice, not a limitation.

Conservative on both sides:

```
gap = trackDistance(follower.front_bound  ->  leader.rear_bound)
```

Codex's view, which I share: prefer the conservative bound unless field logs
show the resulting caution actually prevents useful reacquisition.

---

## 3. `LOST_MIN_GAP_MM` is a measurement, not a constant

**The problem.** 4000 mm was a guess with no physical basis.

**What it has to cover.** Stopping distance at search speed, communications
latency between broadcast and reaction, the leader's own bound uncertainty,
train length, and margin.

**How to establish it.** Instrument before trusting. Log the computed gap at
the moment of halt, and the actual distance travelled between the halt request
and standstill. Set the constant from the measurements, with margin. Until
those numbers exist, treat any value as provisional and err long.

---

## Related, from the same session

**Navigation may observe always; navigation may act only in AUTO.** Marker
detection, odometer advance, confidence scoring and publishing should run in
manual mode — that is what makes manual test runs produce data, and it means
switching into AUTO needs no position re-declaration. But no navigation path
may request PWM unless `autoRunning` is true. The operator has authority in
manual. In `SOLONAV_2_5` three paths violated this: the creep on entering LOST,
and both budget halts.

**Hoist `struct Peer` into the top type block.** Arduino generates function
prototypes above mid-file type definitions, so `peerActive(const Peer&)`
fails to compile otherwise. Same trap that caught `MarkerEvent` and
`StationPhase` in the 2.0 round.

**The alert payload already carries what a follower needs.** `rear_bound_mm`,
`front_bound_mm`, `envelope_mm` summed from `spacingMm[]`, `est_mm_s`,
`halted`, `candidate_mm`, and the age of the last confirmed fix. Whatever CTO3
uses for transport, that field set is a reasonable starting contract.

---

# Reconciliation with Sam's CTO2 harvest

Sam produced `CTO2_HARVEST.md` on 2026-07-28, working from the CTO2 development
history. These notes were written from code review and the SOLONAV field logs.
The two were independent. Where they agree, treat it as settled; where they
differ, the reason is usually that one is reasoning about solo operation and
the other about traffic.

## Settled by independent agreement

**A silent peer is an obstruction, not clear track.** Reached three times over —
by Codex from code review, by Sam from CTO2 experience, and here. No further
argument needed.

**Boundary math was right; its inputs were not.** Front/rear consist boundaries
and same-direction nearest-ahead are correct concepts. Everything that fed them
bare marker positions must be rewritten.

**Audit before port.** Neither side proposes rebuilding CTO2 from memory, and
neither proposes porting it unexamined.

**LOST must end AUTO under CTO.** Already recorded as M4.

## New from Sam, and worth adopting

**Traffic is an interruption, not a role.** The station mission continues to
exist while traffic preempts it, and resumes afterwards. This is the general
form of a failure SOLONAV hit independently — a phase entered on a condition
that might never recur, with no way back to the mission. Worth applying beyond
traffic.

**`valid_for_traffic` as an explicit boolean, with a reason when false.** The
2_5 payload published bounds and left consumers to judge them. Making the
locomotive state its own fitness, and say why not, is better: it puts the
judgement where the evidence is.

**`source` as a first-class field** — dispatcher declared, odometer counted,
map-refined, wheel-assisted. Provenance of a position is not the same as
confidence in it, and a consumer may reasonably treat them differently.

**A DEGRADED navigation state, distinct from LOST.** This answers something
left open in the SOLONAV analysis: the band between confident and lost was
invisible, with the locomotive behaving normally while quietly not being
trusted. Naming it, and widening the published envelope rather than stopping,
is the right resolution. A train can remain traffic-valid while degraded *if
the envelope it publishes still contains the truth*.

**The peer state machine** — FRESH_BOUNDED / FRESH_UNBOUNDED / STALE_KNOWN /
SILENT_KNOWN / UNKNOWN — is a cleaner formalisation than the freshness timeout
sketched in these notes. Only FRESH_BOUNDED grants clearance.

**"Declare → count → refine"** states the SOLONAV architecture more compactly
than anything written here, and is the reasoning behind
`CONFIDENCE_DECLARED 10` versus `CONFIDENCE_REACQUIRED 4`.

## One genuine conflict — station gating is mode-dependent

Sam's §2.4 requires station action to be gated on confidence above a threshold.
That gate existed in SOLONAV and **was deliberately removed on 2026-07-27**, on
the operator's instruction: refusing to arm meant driving past the station
rather than stopping in a slightly wrong place, which suppressed the behaviour
the sketch exists to produce and hid the sensor problem behind it.

Both positions are correct for their mode:

| | solo | CTO |
|---|---|---|
| stop on poor evidence | visible, informative, harmless | leaves a train at rest on shared track at an uncertain place |
| skip the station | silent, hides sensor trouble | safe |

**Resolution:** the gate stays off for solo operation and returns for CTO,
alongside the M4 LOST rule. Both are cases where solo permissiveness becomes
unsafe once there is a second train. Record it as a mode difference rather than
letting one silently overwrite the other later.

## Where these notes still add something

The **measurement discipline** in `ROAD_TO_CTO.md` — every milestone crossed by
a test rather than a judgement — is not in the harvest, and it is the specific
thing CTO2 lacked. Sam's §7 interprets the milestones; it does not replace
their crossing tests.

The **speed floor** is a hard constraint the harvest does not mention: below
roughly 2.5 s per marker, magnet occupancy of the median window exceeds 50%,
the baseline is pulled into the magnets, and navigation fails. Any traffic
behaviour that slows a train — holds, creeps, bubble following — must respect
it. A coordination scheme that produces a crawl will destroy the navigation it
depends on.

---

# Governing principle — reliability, not degraded modes

Operator direction, 2026-07-28, and it supersedes parts of both this document
and the harvest.

> If a locomotive doesn't know its position, it cannot function. I don't want
> to plan for multiple failure modes. That is the perspective that leads to
> gridlock. Making the system reliable is useful. Planning for failure is not.

## One failure mode, one response

**No position → no autonomous operation.** Stop, publish, hand to the operator.
There is no partial competence to design behaviour around.

This is not caution, it is the opposite. Every degraded mode is a state that
must be reasoned about, tested, and interacted with every other state. Six
navigation states and five peer states produce thirty combinations, most of
which will never be exercised and any of which can wedge. This project's entire
history is guards that stopped the train — the 26-minute lockout was three of
them interacting, and Sam's bootstrap trap was two.

**Consequently, out of scope:**
- `DEGRADED` as a navigation state distinct from LOST (harvest §6.1). A widened
  envelope that is still "traffic-valid" is a partial competence. Either the
  position is good or it is not.
- `FRESH_UNBOUNDED` as a peer state (§6.2). A peer that cannot bound itself is
  not usable; it does not need its own category.
- Any behaviour designed for a lost locomotive that is still moving.

**In scope instead:** M1, M2 and M3 — the work that stops the locomotive
getting lost in the first place. Reliability is where the effort belongs.

## Leader and follower from the peer table

The concept from the original CTO, and it is enough.

**Relative order cannot change.** On single track with no passing, if Toby is
ahead of Otto he remains ahead of Otto, regardless of what either believes
about its own position. Absolute position can be destroyed by bad reads; order
cannot, because reversing it requires a physical event that cannot occur.

So roles are assigned once, from the peer table, and stay assigned. The
follower holds behind the leader. That is the whole coordination model, and it
does not need to be re-derived from geometry on every packet or re-established
after a fault.

## Two kinds of stopping, and only one is safe

Worth stating because both are "stopping" and they are not equivalent.

**Stopping because you are lost** is deliberate, announced, and *freezes the
uncertainty*. Every marker crossed while lost widens the envelope; standing
still stops it growing. A stationary obstruction with a fixed envelope is a
solvable problem for a follower. A moving one with an envelope expanding at
250 mm/s is not. The stop must be published — `halted`, bounds, reason — so the
follower is holding clear of a known stationary thing.

**Stopping at a station you think you have reached** is an arbitrary halt at an
unverified place, based on a position you have already stopped trusting.

The first is safety. The second is a guess. M4 should say so explicitly.
