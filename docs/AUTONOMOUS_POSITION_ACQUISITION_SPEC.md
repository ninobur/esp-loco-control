# Autonomous position acquisition and recovery — design specification

Date: 2026-08-22
Status: Proposed. This document authorises no firmware change, no flash and no
train run. T remains rejected, O remains archived, Otto remains on rollback
commit `6d35bb7`.

Supersedes the fixed-offset QUORUM recovery model. It is a replacement, not a
patch: `QUORUM_OFFSETS`, the 12-event evaluation budget, the two-point margin
and the fixed `±REACQ_WINDOW_MARKERS` fence are removed rather than widened.

Governing records: decision 0042, `docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md`,
`docs/QUORUM_NAVLAB_ITER3_REPORT.md`, `docs/NAVLAB_DT0_SEMANTICS.md`.

## 1. Operational goal

A locomotive normally establishes or recovers its own position from track
observations. Manual MM declaration is an optional authoritative shortcut and
an emergency fallback. It is not a routine requirement, and no state in this
design demands one in order to become safe: every state has a safe behaviour
that requires no operator input, and that behaviour is "stop" wherever
knowledge is insufficient to authorise motion.

## 2. Normative principles

These are binding on every clause below. Where a later clause appears to
conflict, the principle wins.

- **P1 — Reachability first.** A lost locomotive is not automatically anywhere
  on the circuit. Uncertainty grows forward from the last trustworthy anchor at
  a physically credible rate, and becomes route-wide only when a full circuit is
  physically possible or when no anchor exists at all.
- **P2 — DNA is a filter, never an authority.** Polarity sequence may
  distinguish among physically reachable positions. It may never override
  physical impossibility, and it may never introduce a position the reachability
  set excludes.
- **P3 — Direction-consistent is not position-confirmed.** Agreement with
  powered direction is a consistency check and carries no positional claim.
- **P4 — Firmware labels are not ground truth.** No component of this design
  consumes a firmware position label, a firmware verdict, or an MQTT receipt
  time as evidence about position.
- **P5 — Completeness gates confirmation.** No confirmation is permitted from a
  hypothesis set that is not known to contain every physically possible true
  position. A set that has been truncated, capped, or built on an
  under-approximation is marked `INCOMPLETE` and confirmation authority is
  suspended while it is.
- **P6 — Unknown time is unknown.** A dt-chain reset means elapsed time is
  unknown. It does not mean zero travel and it does not mean one interval.
- **P7 — Authoritative vs internal declarations are different events.** An
  operator declaration is external ground truth. An internal firmware
  re-anchor (boot, watchdog, self-relabel, timer reset) is not, and must never
  borrow an MM label from firmware state. An internal reset either suspends
  confirmation authority or enters an explicit unknown-position state.
- **P8 — Hold, do not delete.** A questionable event remains pending until
  successor evidence resolves it. Amplitude and duration are grounds for
  holding and for ranking hypotheses, never for irreversible immediate
  deletion.
- **P9 — Robust envelopes.** Speed limits come from robust empirical
  distributions per locomotive, PWM bucket, section and direction. A single
  minimum, or a dwell- or ghost-contaminated sample, may not set a bound.
- **P10 — Orientation-declared acquisition searches only the declared
  direction.**
- **P11 — Uniqueness, not repetition.** Route-wide reacquisition requires a
  genuinely unique observed sequence. Repeated consistency inside an aliased or
  possibly-incomplete corridor is not a substitute.
- **P12 — Motion follows knowledge.** Movement authority is a function of state
  and of the worst case over the hypothesis set, including the other
  locomotive's hypothesis set. If safe movement cannot be authorised, the
  locomotive stops. It does not demand a manual MM declaration.

## 3. Common substrate

### 3.1 Route constants

171 markers (`DNA_N`), circuit 52,150 mm, per-interval spacing table (nominal
~305 mm). Every DNA window of length ≥ 10 is route-wide unique; W = 9 collides
four ways. `W_UNIQUE = 12` is used throughout, retaining the existing two-marker
safety margin over the proven bound.

### 3.2 Evidence record

Each marker event carries, and the navigator consumes only:

| field | use |
|---|---|
| `t_detect` | on-device detection time |
| `polarity` N/S | DNA filter |
| `peak` | ghost prior (§3.7) |
| `duration_ms` | ghost prior (§3.7) |
| `dt_ms` since previous **accepted** event, or `RESET` | timing filter (§3.6) |
| `pwm_actual_history` over the dt window | timing filter |
| `motor_dir`, `session_dir` | effective travel direction |
| `baseline_drift` | detector health, telemetry only |

Explicitly not consumed: firmware `mm` label, firmware verdict, MQTT receipt
time (P4).

### 3.3 Anchors and provenance

An **anchor** is a position statement with a provenance class and a time:

- `OPERATOR_DECLARED` — external authority. Position true, time origin true.
- `SELF_CONFIRMED` — produced by §3.9 confirmation. Position trusted, time
  origin true.
- `INTERNAL_REDECLARED` — **not an anchor**. Recorded for telemetry and
  explicitly carries no position (P7).

The **last trustworthy anchor** is the most recent `OPERATOR_DECLARED` or
`SELF_CONFIRMED` statement. It is retained across every state transition,
together with its time, the direction history since, and the accumulated
motion evidence since. It is discarded only by a newer anchor.

### 3.4 Candidate representation

The hypothesis set `H` is a **bitmap over (marker, travel-direction)**:
171 × 2 = 342 bits = 44 bytes. This is the complete state space of the
problem. Consequences that matter:

- Hypothesis growth is bounded by construction, at the whole route. There is
  no budget to exceed and therefore no truncation of the *position* dimension,
  so the `INCOMPLETE` flag of P5 is never raised by memory pressure on `H`.
- "Route-wide" is not a special mode; it is `H` with many bits set.
- Union, intersection and cardinality are word operations. Propagation is
  O(171 × small) per event, at roughly one event per second. This is
  comfortably inside the ESP32's budget alongside the existing loop.

Alongside `H` the navigator keeps a small **branch list** for pending events
(§3.8), each entry being its own `H` bitmap plus an accumulated dt. The branch
list is the only capped structure in the design, and exceeding its cap raises
`INCOMPLETE`.

### 3.5 Reachability propagation

Position is **not** tracked as a monotonically growing corridor. The
iteration-3 corridor grew only on confirmation resets and therefore outran the
locomotive by 4–6×; it is replaced by **per-event local propagation**:

For an event `e` with elapsed `dt`, compute the distance window
`[d_lo, d_hi]` the locomotive could have covered during `dt`, from the PWM
history distributed over the dt window and the robust envelopes of §3.6. Then

```
H' = { q : ∃ p ∈ H, dir(q) = dir(p),
           dist(p → q, dir) ∈ [d_lo, d_hi],
           dna[q] = polarity(e) }
```

`q` need not be the immediate successor of `p`: any pole-matching marker whose
distance from `p` falls in the window is admitted, which is exactly how missed
markers are absorbed without special-casing. `d_lo` is a genuine lower bound
(zero whenever standstill cannot be excluded), `d_hi` a genuine upper bound.
Both bounds are over-approximations by construction, so `H'` contains the truth
whenever `H` did — the invariant P5 depends on.

Uncertainty therefore contracts as often as it grows: a short dt admits few
`q` per `p`, and the polarity filter halves the survivors on average.

### 3.6 Timing envelopes (P9)

Envelopes are rebuilt with the fast bound taken as a **robust lower quantile**
of the admitted dt distribution, not `min × (1 − margin)`. Explicit choices:

- Admission filters both ends. The existing filter rejects slow, dwell,
  stationary and PWM-uncovered samples and rejects nothing at the fast end;
  it gains a fast-end filter that drops samples carrying a ghost signature
  (peak below the genuine floor) or a dwell signature (duration above the
  genuine ceiling) — the contamination that put every one of the twenty fastest
  PWM-90 samples outside physical plausibility.
- `fast_dt = quantile(admitted_dt, q_fast) × (1 − margin)`, with
  `q_fast = 0.02` and `margin = 0.15`. **Both values require calibration and
  are listed in §9 as open.**
- A tier with fewer than `MIN_N = 8` admitted samples does not produce a
  bound; the next broader tier (section → direction → locomotive) is used.
- A bound that would imply a corridor speed more than `SANITY_RATIO = 3×` the
  same bucket's median speed is rejected as contaminated and the broader tier
  is used instead. This is a backstop against a bucket whose contamination
  survives the signature filters.

Envelope generation stays off-locomotive; the device consumes a versioned
table.

### 3.7 Amplitude and duration (P8)

The navigator gains the amplitude and duration criterion it currently lacks
entirely. It is a **prior on the pending branch, never a delete**:

- `GENUINE_LIKE`: peak ≥ `PEAK_FLOOR` and duration ≥ `DUR_FLOOR`.
- `GHOST_LIKE`: peak < `PEAK_FLOOR` or duration < `DUR_FLOOR`.

A `GHOST_LIKE` event is never applied directly to `H`. It opens a pending
branch (§3.8) whose phantom hypothesis is preferred at arbitration ties. It is
not discarded, because a weak reading of a real marker is possible and
deleting it produces the label slips the recovery plan indicts. The floors are
derived from the genuine/ghost signature separation already measured, and are
listed in §9 as open — the entry threshold remains the only *detection* gate
(decision 0040); these floors are classification inputs downstream of it and do
not suppress any event.

### 3.8 Pending branches (P8)

An event that cannot be applied cleanly — timing-vetoed, `GHOST_LIKE`,
polarity-unmatched, or ambiguous — does not resolve. It forks:

- **H-genuine**: the event was a marker crossing. Propagate per §3.5.
- **H-phantom**: the event was spurious. `H` unchanged; the event's dt is
  accumulated and folded into the next event's dt so the timing test on the
  successor sees the true interval.

Both branches are carried. The successor event arbitrates: a branch whose
propagation is empty dies; if both survive, both are kept and `|H|` is their
union, which means no confirmation (uniqueness fails). Ties are broken toward
H-phantom only for *ranking* in telemetry, never by deleting H-genuine.

`PENDING_DEPTH_MAX = 3` consecutive unresolved events. Beyond that the branch
list is collapsed to its union and `INCOMPLETE` is raised — the design's one
truncation, made visible rather than silent, and it costs only confirmation
authority, not the hypotheses.

### 3.9 Confirmation (P3, P5, P11)

A position becomes `SELF_CONFIRMED` only when **all** hold:

1. `H` is `COMPLETE` — no `INCOMPLETE` flag anywhere in the current chain.
2. Confirmation authority is not suspended (§6).
3. Either
   - **collapse**: `|H| = 1` and it has advanced coherently — each of
     `K_CONFIRM = 3` successive events left `|H| = 1` at exactly the successor
     the previous one predicted; or
   - **uniqueness**: the last `W_UNIQUE = 12` accepted events form a sequence
     matching exactly one route position **and** no pending branch and no
     missed-marker admission occurred inside that window. A window containing an
     admitted skip is not unique evidence, because the observed string is then
     not the route string.
4. The confirmed position lies inside `H` — a confirmation may never name a
   position the reachability set excluded (P2).

Route-wide reacquisition uses criterion 3-uniqueness only. Repetition inside a
corridor is never sufficient (P11), which is precisely the pathway that
produced eight unsupported route-wide recoveries in one session.

### 3.10 Contradiction

`H` empty on every branch means the model is wrong — a missed-marker model
failure, a map fault or a detector fault. The response is never to invent a
position and never to permanently halt the navigator:

1. Movement authority drops to zero and the locomotive stops under the station
   brake ramp.
2. State becomes `UNLOCATED` with `H` re-seeded to all 342 bits.
3. The evidence ring, the last trustworthy anchor and the contradiction
   snapshot are retained and published.

Recovery is by §3.9 uniqueness or by an operator declaration. Neither is
demanded.

### 3.11 Occupancy span and two-locomotive safety

Each locomotive publishes an **occupancy span**: the arc covered by `H`
expanded fore and aft by its configured train extent, expressed as
(start marker, end marker, arc length mm, `COMPLETE`/`INCOMPLETE`, state).
This replaces the single `mm` in the separation computation.

Separation (decision 0033: bubble plus six markers clear) is evaluated over the
**worst case pair** drawn from the two occupancy spans. Consequences:

- Two `POSITIONED` locomotives behave exactly as today — each span is one
  marker plus extent.
- If either span is route-wide, no separation can be guaranteed and both
  locomotives stop. This generalises decision 0031 (NO_QUORUM on any
  locomotive stops every locomotive) from a state test to a geometric one.
- An `INCOMPLETE` span is treated as route-wide for safety purposes,
  regardless of how few bits are set.
- The uncertain locomotive is the one that yields: a `POSITIONED` peer retains
  authority up to the separation bound against the uncertain span.

## 4. The four states

Common to all four: the last trustworthy anchor, its time, the direction
history since, the evidence ring and the pending branch list are retained
across every transition (P1). Nothing below discards them.

### 4.1 POSITIONED

Exact MM and orientation trusted; normal navigation.

- **Entry.** Operator declaration of position; or §3.9 confirmation from any
  other state.
- **Retained evidence.** Anchor with provenance and time; evidence ring;
  direction history.
- **Candidates.** `|H| = 1`. One marker, one travel direction.
- **Polarity.** Each event's polarity must match the predicted successor;
  a mismatch does not relocate, it opens a pending branch.
- **Spacing.** The interval table gives the expected distance to the successor.
- **PWM timing.** The envelope test is a veto: an event arriving faster than
  `fast_dt` for the interval, bucket and direction cannot be the successor.
  Within `MARGINAL_SLACK = 0.25` of the bound it becomes pending rather than
  vetoed.
- **Amplitude / duration.** `GHOST_LIKE` opens a pending branch and does not
  advance position.
- **Pending.** Held per §3.8. Position does not advance while a branch is
  unresolved; the odometer's claim is suspended, not rewritten.
- **Confirmation.** Ordinary successor agreement maintains `SELF_CONFIRMED`.
  Re-confirmation is not required event by event.
- **Contradiction.** Both branches empty → §3.10.
- **Exit.** To `RECOVERING` on: `PENDING_DEPTH_MAX` exceeded; `|H| > 1` after
  propagation; an internal redeclaration or dt-chain reset (P7); or an elapsed
  gap with no event exceeding the envelope's one-interval bound. To
  `UNLOCATED` on contradiction. To `ACQUIRING_ORIENTED` on an operator
  orientation declaration that clears position.
- **Movement authority.** Full, including AUTO.
- **Station stopping.** Permitted; the station machine is armed.
- **Telemetry.** `state=POSITIONED`, `mm`, `dir`, anchor provenance and age,
  `pending_depth`, span = 1.
- **Manual override.** A declaration re-anchors and clears pending state.
- **Two-locomotive.** Span is one marker plus extent; normal CTO separation.

### 4.2 ACQUIRING_ORIENTED

Operator declares orientation; position unknown. The locomotive acquires
without an MM declaration.

- **Entry.** Operator orientation declaration with no position; or boot with a
  retained session direction and no trustworthy anchor.
- **Retained evidence.** Declared direction; any earlier anchor is retained but
  is not used to seed `H` unless the operator states the locomotive has not
  been moved since.
- **Candidates.** `H` = all 171 markers in the **declared direction only**
  (P10). 171 bits, never 342.
- **Polarity.** The first event halves `H` immediately. Successive events
  filter it multiplicatively; on a route where W = 10 is unique, a clean run
  reaches uniqueness in ~10–12 events.
- **Spacing / PWM timing.** Each event's `[d_lo, d_hi]` prunes hypotheses whose
  next pole-matching marker is out of window. This is what distinguishes
  acquisition from a naive DNA string match: a hypothesis surviving the string
  can still die on physics.
- **Amplitude / duration.** `GHOST_LIKE` events open pending branches as
  elsewhere. During acquisition the phantom branch is comparatively cheap —
  `H` is already large — so no shortcut is taken.
- **Pending.** Per §3.8. `INCOMPLETE` at depth overflow, which in this state
  costs nothing extra: authority is already restricted.
- **Confirmation.** §3.9 **uniqueness only**. Collapse-plus-`K_CONFIRM` is not
  accepted here, because a collapse from a 171-wide set is exactly the aliasing
  case P11 forbids.
- **Contradiction.** `H` empty with a 171-wide start means the declared
  orientation is wrong or the map is wrong: stop, and transition to
  `UNLOCATED` (which evaluates both orientations).
- **Exit.** To `POSITIONED` on unique acquisition. To `UNLOCATED` on
  contradiction or on operator withdrawal of the orientation. To `POSITIONED`
  immediately on an operator MM declaration.
- **Movement authority.** Motion is required to acquire, so it is permitted,
  bounded: `ACQ_PWM_MAX` (the station-zone speed, PWM 60, is the proposed
  value), no AUTO mission, and only while §3.11 authorises it. With a peer
  present whose span overlaps the acquiring span's worst case, motion is
  refused and the locomotive stands.
- **Station stopping.** Prohibited. The station machine is disarmed: an
  approach cannot be armed from a position that is not known.
- **Telemetry.** `state=ACQUIRING_ORIENTED`, `dir`, `|H|`, span, events since
  entry, estimated events to uniqueness, `COMPLETE` flag, movement authority
  and its reason code.
- **Manual override.** An MM declaration short-cuts acquisition immediately and
  is authoritative. It is never demanded.
- **Two-locomotive.** Span is route-wide at entry and shrinks with each event.
  Under §3.11 that means: alone on the railway, the locomotive acquires while
  moving; with a peer present, it acquires only if the peer holds, and
  otherwise stands still and waits, publishing why.

### 4.3 RECOVERING

Position became uncertain during operation.

- **Entry.** From `POSITIONED` on ambiguity, pending overflow, dt-chain reset,
  internal redeclaration, or an over-long eventless gap.
- **Retained evidence.** The last trustworthy anchor and its time; direction
  history since; accumulated motion evidence since; the pre-loss `H`; the
  evidence ring; pending branches. This retention is the whole point of the
  state and is what distinguishes it from `UNLOCATED` (P1).
- **Candidates.** `H` seeded from the pre-loss hypotheses, propagated forward
  per §3.5. Not route-wide, and not reseeded to route-wide unless the elapsed
  evidence makes a full circuit physically possible.
- **Polarity, spacing, PWM timing.** As §3.5, unchanged — the same propagation
  serves ordinary tracking and recovery. There is no separate recovery
  algorithm, which is the structural difference from QUORUM.
- **Amplitude / duration.** As §3.7. In recovery a `GHOST_LIKE` event is the
  common case (loss is frequently caused by the phantom family that produced
  it), so the phantom branch is load-bearing here.
- **Pending.** As §3.8.
- **Confirmation.** §3.9, with confirmation authority suspended whenever the
  chain is gap-bearing (§6). Collapse-plus-`K_CONFIRM` is accepted only when
  the chain is not gap-bearing and `H` never exceeded
  `COLLAPSE_MAX_SET = 8` hypotheses since the anchor; above that, uniqueness is
  required. **`COLLAPSE_MAX_SET` is open (§9).**
- **Contradiction.** §3.10 → stop and `UNLOCATED`.
- **Exit.** To `POSITIONED` on confirmation. To `UNLOCATED` when `H` reaches
  route-wide extent, on contradiction, or when `RECOVER_WINDOW` (proposed:
  the lesser of 90 s and 40 marker events) elapses without confirmation.
- **Movement authority.** Motion continues at reduced authority while the span
  remains bounded and §3.11 authorises it: `ACQ_PWM_MAX`, no AUTO mission,
  no new station approach. Authority is withdrawn — brake to stop — when the
  span grows past the separation bound against any peer, when `INCOMPLETE` is
  raised, or at `RECOVER_WINDOW` expiry.
- **Station stopping.** Prohibited. An approach already in `ST_FINAL` at entry
  completes, because aborting a final brake is worse than finishing it; any
  earlier phase is abandoned and the zone speed held.
- **Telemetry.** `state=RECOVERING`, anchor and its age, `|H|`, span,
  `gap_bearing`, `confirmation_authority`, `pending_depth`, reason for entry,
  time and markers remaining in `RECOVER_WINDOW`.
- **Manual override.** An MM declaration resolves it instantly and is
  authoritative. It is never demanded: expiry leads to a stop and
  `UNLOCATED`, not to a prompt.
- **Two-locomotive.** Span published per §3.11 and growing; the peer's
  authority is reduced against it. When the two spans can no longer be
  separated, both stop, and this is expected to be the ordinary outcome of a
  long recovery with a peer on the railway.

### 4.4 UNLOCATED

Neither position nor orientation is trusted.

- **Entry.** Cold boot with no retained anchor; contradiction from any state;
  `RECOVERING` expiry or route-wide extent; operator command; loss of
  orientation confidence.
- **Retained evidence.** The evidence ring and the last trustworthy anchor are
  retained **for telemetry and forensics only** and do not seed `H`.
- **Candidates.** `H` = all 342 bits: every marker in **both** travel
  directions. This is the only state that starts route-wide, and it does so
  because there is genuinely no anchor — not because a corridor was allowed to
  grow (P1).
- **Polarity.** Filters both direction planes simultaneously. Because the DNA
  read in reverse is a different string, the two planes decay at different
  rates and orientation usually resolves before position does.
- **Spacing / PWM timing.** Applied per plane; a hypothesis must be physically
  consistent in its own direction.
- **Amplitude / duration.** As §3.7.
- **Pending.** As §3.8.
- **Confirmation.** §3.9 uniqueness only, and the unique match must be unique
  **across both direction planes** — a sequence unique in CW but also matching
  some CCW position resolves nothing.
- **Contradiction.** `H` empty from a 342-bit start is a map or detector fault,
  not a navigation error. Stop, latch a fault, publish, and do not re-seed.
- **Exit.** To `POSITIONED` on unique two-plane acquisition; to
  `ACQUIRING_ORIENTED` on an operator orientation declaration; to
  `POSITIONED` on an operator MM declaration.
- **Movement authority.** **None autonomously.** The locomotive stops and
  stays stopped. Operator-commanded manual crawl at `ACQ_PWM_MAX` is permitted
  and is the mechanism by which an unlocated locomotive gathers the markers it
  needs — under fleet hold if a peer is present.
- **Station stopping.** Prohibited.
- **Telemetry.** `state=UNLOCATED`, `|H|` per direction plane, span, entry
  reason, events since entry, whether orientation has resolved, and an explicit
  `manual_declaration_required=false` — the operator is informed, not
  summoned.
- **Manual override.** Both declaration forms accepted and authoritative.
- **Two-locomotive.** Span is route-wide by definition, so under §3.11 no
  autonomous motion is authorised for either locomotive while an `UNLOCATED`
  peer is on the railway. A stopped unlocated locomotive is a stationary
  obstacle of known extent but unknown place; the safe treatment is fleet hold.

## 5. State transition summary

| from → to | trigger |
|---|---|
| any → POSITIONED | operator MM declaration; §3.9 confirmation |
| POSITIONED → RECOVERING | ambiguity, pending overflow, dt reset, internal redeclaration, over-long gap |
| POSITIONED → ACQUIRING_ORIENTED | operator orientation declaration clearing position |
| ACQUIRING_ORIENTED → UNLOCATED | contradiction; orientation withdrawn |
| RECOVERING → UNLOCATED | contradiction; route-wide extent; window expiry |
| UNLOCATED → ACQUIRING_ORIENTED | operator orientation declaration |
| any → UNLOCATED | contradiction; cold boot with no anchor; operator command |

There is no transition into `POSITIONED` that does not go through §3.9 or an
operator declaration, and none that borrows a firmware label (P4, P7).

## 6. dt-chain resets: the two safe strategies

### 6.1 The two cases are not the same event

The iteration-3 rule treats every `dt == 0` alike, which is the root of its
counterexample. This design splits them (P7):

**Case D — reset accompanying an operator declaration.** Position is
authoritative and the time origin is re-established by the same act. This is
not a loss and not an uncertainty event. The navigator stays `POSITIONED`,
`H = {declared}`, no corridor grant is needed or taken. The next event's dt is
measured from the declaration. Ordinary re-declarations therefore cost nothing,
which is the operational property the one-interval grant was reaching for and
obtained by unsound means.

**Case I — reset from an internal redeclaration** (boot, watchdog, timer
re-anchor, self-relabel). Elapsed time is unknown, and any accompanying
firmware MM label is not evidence. This is the dangerous case, and it is the
one strategies A and B address.

### 6.2 Strategy A — bounded hypothesis growth, confirmation authority suspended

Retain the pre-reset hypotheses. Propagate them with `d_lo = 0` and `d_hi = ∞`
for the reset event only, which admits every pole-matching marker ahead in each
live direction — an honest over-approximation. Mark the chain **gap-bearing**.
While gap-bearing, `H` continues to filter normally on every subsequent event,
but **confirmation authority is suspended**: a collapse to one hypothesis may
not confirm. Authority returns only on §3.9 uniqueness, which clears the
gap-bearing flag.

### 6.3 Strategy B — immediate explicit unknown-position state

Treat any Case-I reset as loss of positional authority outright: transition to
`UNLOCATED` (or `ACQUIRING_ORIENTED` if orientation is independently trusted)
and require full reacquisition.

### 6.4 Comparison

| axis | A | B |
|---|---|---|
| **False confirmation** | impossible while gap-bearing: the only exit is route-wide uniqueness, the same test B uses | impossible: no position is claimed at all |
| **Over-approximation** | preserved — `d_hi = ∞` is a true upper bound, unlike the one-interval grant which is an *under*-approximation and can exclude the truth | preserved trivially |
| **Information retained** | pre-reset hypotheses and direction survive; if the unknown time was in fact short, `H` stays small and uniqueness returns in a few events | discarded; every reset costs a full ~12-event unique window |
| **Movement authority** | `RECOVERING` rules: bounded motion while the span is bounded and the peer permits | `UNLOCATED` rules: no autonomous motion at all |
| **Operational burden** | proportionate to the actual gap | fixed and large. Otto boot16 alone contains 21 dt-chain resets; under B that is 21 full stops and reacquisitions in one session, most of them for resets that cost nothing physically |
| **ESP32 cost** | one flag, plus a propagation with an unbounded upper distance — a full 171-bit sweep for that one event, ~44 bytes of state | marginally cheaper per event, but requires the identical acquisition machinery to get back, so it saves no code |
| **Failure mode if implemented wrongly** | a missed suspension re-opens false confirmation | a missed transition leaves a stale position |

### 6.5 Recommendation: **A**, with automatic degeneration to B's state

Adopt **Strategy A**. Three reasons, none of which is replay behaviour:

1. **B is A minus information, at equal safety.** Both permit re-entry to
   `POSITIONED` only through the same route-wide uniqueness test. B differs
   only by discarding the pre-reset constraint, which can never make a
   confirmation safer — it can only make it later. The safety argument does not
   distinguish them; the information argument does, in A's favour.
2. **Operational burden decides it.** Case-I resets are frequent, and B's cost
   is fixed regardless of how brief the reset was. A railway that stops and
   reacquires on every internal timer re-anchor is not operable.
3. **ESP32 feasibility is equal.** B needs the whole acquisition path anyway;
   A adds one flag and one wide propagation.

A carries B's outcome as its floor: when a gap-bearing `H` reaches route-wide
extent, `RECOVERING` exits to `UNLOCATED` (§4.3), which *is* strategy B. A is
therefore B with the constraint retained while the constraint still constrains.

**This choice is against the Otto replay, not for it.** Under A, the eight
route-wide recoveries in Otto boot16 do not become confirmations — they were
produced by repetition inside an unvalidated corridor, and §3.9 uniqueness plus
suspended authority refuses them. A produces *fewer* confirmations on that
session than the frozen navigator does, and several stops where it currently
relocates. Passing the known replay is not evidence for this design and is not
claimed as such.

## 7. Telemetry

Additive to the existing MQTT contract; no field is removed or repurposed, and
no change is made to the Pi controller contract by this specification (the
additive fields are listed in §9 as requiring approval before implementation).

Per state publish: `nav_state`, `anchor_mm`, `anchor_provenance`, `anchor_age_ms`,
`hypothesis_count`, `span_start_mm`, `span_end_mm`, `span_mm`, `complete`,
`confirmation_authority`, `gap_bearing`, `pending_depth`, `movement_authority`,
`movement_reason`, `dir_hypotheses`, `entry_reason`, and — in acquisition and
recovery — `events_since_entry` and `window_remaining`.

The wording rule from the iteration-3 record carries over verbatim: the
permitted phrasing is **"zero false confirmations detected"**, never "zero
false confirmations".

## 8. What this replaces

Fixed-offset recovery is removed, not extended. There is no offset table, no
evaluation budget, no margin-of-two adoption, no fixed reacquisition fence and
no advisory-only DNA match. Recovery is not a separate algorithm bolted to
tracking: §3.5 propagation *is* tracking, and the four states differ only in
how `H` is seeded and what authority the result carries. The detailed
disposition is in `docs/AUTONOMOUS_ACQUISITION_IMPLEMENTATION_MAP.md`.

## 9. Decisions requiring operator approval

These are engineering choices made explicitly in this document that the
operator must accept, reject or set before implementation begins.

1. **Envelope robustness parameters.** `q_fast = 0.02`, `margin = 0.15`,
   `SANITY_RATIO = 3×`, and the fast-end contamination filters (§3.6).
2. **Amplitude/duration floors.** `PEAK_FLOOR`, `DUR_FLOOR` (§3.7), and the
   ruling that these are classification inputs downstream of the entry
   threshold, which decision 0040 keeps as the only detection gate.
3. **Motion while uncertain.** That `ACQUIRING_ORIENTED` and `RECOVERING` may
   move at all, and at `ACQ_PWM_MAX` = station-zone speed (PWM 60) (§4.2, §4.3).
4. **Recovery window.** `RECOVER_WINDOW` = lesser of 90 s and 40 events before
   `RECOVERING` degrades to `UNLOCATED` (§4.3).
5. **Collapse admissibility.** `COLLAPSE_MAX_SET = 8` — the largest hypothesis
   set from which a collapse-plus-`K_CONFIRM` confirmation is accepted rather
   than requiring uniqueness (§4.3).
6. **Pending depth.** `PENDING_DEPTH_MAX = 3`, and that overflow raises
   `INCOMPLETE` rather than discarding a branch (§3.8).
7. **dt=0 strategy.** Adoption of Strategy A with automatic degeneration to
   `UNLOCATED` (§6.5).
8. **Two-locomotive yielding.** That an `INCOMPLETE` or route-wide span forces
   a fleet stop, generalising decision 0031 from a state test to a geometric
   one, and that the uncertain locomotive yields (§3.11).
9. **Station behaviour on entry to `RECOVERING`.** That an approach already in
   `ST_FINAL` completes while any earlier phase is abandoned (§4.3).
10. **Additive telemetry fields** (§7) and the occupancy-span addition to the
    CTO payload (§3.11) — the recovery plan places the MQTT and Pi controller
    contract out of scope, so even additive changes need an explicit ruling.
11. **Orientation declaration command.** A new operator command that declares
    direction without position is required for `ACQUIRING_ORIENTED` to be
    reachable as designed; none exists today.
