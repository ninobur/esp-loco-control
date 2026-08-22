# Autonomous position acquisition and recovery — design specification

Date: 2026-08-22. Corrected the same day under operator directive (correction
pass on b2c3332).
Status: Proposed. This document authorises no firmware change, no flash and no
train run. T remains rejected, O remains archived, Otto remains on rollback
commit `6d35bb7`.

Supersedes the fixed-offset QUORUM recovery model. It is a replacement, not a
patch: `QUORUM_OFFSETS`, the 12-event evaluation budget, the two-point margin
and the fixed `±REACQ_WINDOW_MARKERS` fence are removed rather than widened.

Governing records: decision 0042, `docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md`,
`docs/QUORUM_NAVLAB_ITER3_REPORT.md`, `docs/NAVLAB_DT0_SEMANTICS.md`.

### What this correction pass changed

1. **Timing is branch-local, computed from raw detection times.** The previous
   draft measured `dt` since the previous *accepted* event and then folded a
   doubtful event's `dt` into its successor. Those two mechanisms double-count
   elapsed time. Both are removed (§3.6).
2. **Orientation-only acquisition may not move merely because a peer is
   stopped** (§4.2, §3.12).
3. **`UNLOCATED` autonomy is stated honestly** and its policy is put to the
   operator as an explicit choice, not decided silently (§4.4).
4. **Safety and usefulness are separately gated.** A navigator that always
   stops is safe and does not meet the goal (§8, and the test plan).
5. **Occupancy publication is conservative by construction** and can no longer
   understate uncertainty with a single arc (§3.12).
6. **Statistical constants moved out of the operator decision list** into
   engineering parameters requiring calibration evidence (§9, §10).
7. **The operator no longer confirms a moving locomotive's position**, and
   crawl-to-cruise is automatic and gated on evidence, not on a GO (§7).

## 1. Operational goal

A locomotive normally establishes or recovers its own position from track
observations. Manual MM declaration is an optional authoritative shortcut and
an emergency fallback. It is not a routine requirement.

Two things are true at once and the design must serve both:

- **Stopping is always available and always safe.** No state demands an
  operator declaration in order to become safe.
- **Stopping is not success.** An unscheduled navigation stop is a safe failure
  outcome, not normal operation. A navigator that stops whenever anything is
  uncertain satisfies every safety property in this document and fails its
  purpose. §8 gates the two separately.

## 2. Normative principles

Binding on every clause below. Where a later clause appears to conflict, the
principle wins.

- **P1 — Reachability first.** A lost locomotive is not automatically anywhere
  on the circuit. Uncertainty grows forward from the last trustworthy anchor at
  a physically credible rate, and becomes route-wide only when a full circuit is
  physically possible or when no anchor exists at all.
- **P2 — DNA is a filter, never an authority.** Polarity sequence may
  distinguish among physically reachable positions. It may never override
  physical impossibility, and it may never introduce a position the
  reachability set excludes.
- **P3 — Direction-consistent is not position-confirmed.**
- **P4 — Firmware labels are not ground truth.** No component consumes a
  firmware position label, a firmware verdict, or an MQTT receipt time as
  evidence about position. **Firmware acceptance or rejection of an event may
  not determine physical elapsed time** (§3.6).
- **P5 — Completeness gates confirmation.** No confirmation from a hypothesis
  set that is not known to contain every physically possible true position. A
  truncated, capped or under-approximated set is `INCOMPLETE` and confirmation
  authority is suspended while it is.
- **P6 — Unknown time is unknown.** An elapsed-time discontinuity means elapsed
  time is unknown. It does not mean zero travel and it does not mean one
  interval.
- **P7 — Authoritative and internal declarations are different events.** An
  operator declaration is external ground truth. An internal firmware re-anchor
  is not, and never borrows an MM label from firmware state.
- **P8 — Hold, do not delete.** A questionable event remains pending until
  successor evidence resolves it. Amplitude and duration are grounds for
  holding and ranking, never for irreversible immediate deletion.
- **P9 — Robust envelopes.** Speed limits come from robust empirical
  distributions. A single minimum, or a dwell- or ghost-contaminated sample,
  may not set a bound.
- **P10 — Orientation-declared acquisition searches only the declared
  direction.**
- **P11 — Uniqueness, not repetition.** Route-wide reacquisition requires a
  genuinely unique observed sequence, and the uniqueness must be *verified on
  the committed map*, not asserted (§3.3).
- **P12 — Motion follows knowledge.** Movement authority is a function of the
  worst case over the hypothesis set, including the peer's. If safe movement
  cannot be authorised, the locomotive stops. It does not demand a manual MM
  declaration.
- **P13 — Navigation state is not speed state.** Entering a recovery state does
  not by itself command a speed change (§7).
- **P14 — The operator does not verify a moving locomotive's position.**
  Visual observation and the MQTT console are both delayed; an operator cannot
  reliably confirm an exact MM while the locomotive is moving. Operator
  authority is defined in §7.5 and excludes confirming self-acquired positions.

## 3. Common substrate

### 3.1 Route constants

171 markers (`DNA_N`), circuit 52,150 mm, per-interval spacing table (nominal
~305 mm).

### 3.2 Evidence record

Each **raw detection** carries, and the navigator consumes only:

| field | use |
|---|---|
| `t_detect` | on-device 64-bit extended monotonic detection time, ms |
| `clock_epoch` | incremented on every boot; see §3.7 |
| `polarity` N/S | DNA filter |
| `peak` | ghost prior (§3.9) |
| `duration_ms` | ghost prior (§3.9) |
| `pwm_actual_history` | PWM profile, timestamped on the same clock |
| `baseline_drift` | detector health, telemetry only |

**There is no `dt` field.** Elapsed time is not a property of an event; it is a
property of an (event, branch) pair, computed per §3.6. This is the correction
that removes the double-count.

Explicitly not consumed: firmware `mm` label, firmware verdict, firmware
accept/reject state, MQTT receipt time (P4).

### 3.3 Verified map uniqueness

`W_UNIQUE` is **not** a constant asserted in this document. It is computed by a
committed prerequisite check over the committed map, and the check is a
blocking precondition for the whole design (test P0 in the test plan):

- `W_dir` = the smallest window length such that every one of the 171 windows
  is unique **within a single direction plane**, at every rotational start.
- `W_both` = the smallest window length such that every one of the 342
  (position, direction) windows is unique **across both direction planes**, at
  every rotational start.

`ACQUIRING_ORIENTED` uses `W_dir`; `UNLOCATED` uses `W_both`. If either does
not exist at any window length — that is, if the map contains a genuine
route-wide alias — the corresponding acquisition mode is not implementable on
this map and must be reported as such rather than approximated. The received
claim that windows of length ≥ 10 are unique and W = 9 collides four ways is
treated as a hypothesis to be re-derived, not as an input.

### 3.4 Anchors and provenance

An **anchor** is a position statement with provenance and a time:

- `OPERATOR_DECLARED` — external authority, made under §7.5 conditions.
- `SELF_CONFIRMED` — produced by §3.11 confirmation.
- `INTERNAL_REDECLARED` — **not an anchor**; recorded for telemetry, carries no
  position (P7).

The last trustworthy anchor is the most recent `OPERATOR_DECLARED` or
`SELF_CONFIRMED` statement. It is retained across every state transition and
discarded only by a newer anchor.

### 3.5 Candidate representation

The hypothesis set `H` is a **bitmap over (marker, travel-direction)**:
171 × 2 = 342 bits = 44 bytes — the complete state space. Consequences:

- Growth is bounded by construction at the whole route. There is no budget to
  exceed, so `INCOMPLETE` is never raised by memory pressure on `H`.
- "Route-wide" is not a mode; it is `H` with many bits set.
- Union, intersection and cardinality are word operations; propagation is
  O(171 × small) per event at roughly one event per second.

Alongside `H` the navigator keeps a **branch list** (§3.10). Each branch is its
own `H` bitmap **plus its own `last_genuine` timestamp** (§3.6). The branch
list is the only capped structure; exceeding the cap raises `INCOMPLETE`.

#### 3.5.1 Reachability propagation

Position is **not** tracked as a monotonically growing corridor. The
iteration-3 corridor grew only on confirmation resets and so outran the
locomotive by 4-6x; it is replaced by **per-event local propagation**.

For a detection `e` evaluated against branch `b`, take `elapsed(e, b)` from
Sec. 3.6 and convert it, via the PWM profile over that interval and the robust
envelopes of Sec. 3.8, into a distance window `[d_lo, d_hi]`. Then

```
H'(b) = { q : exists p in H(b), dir(q) = dir(p),
               dist(p -> q, dir) in [d_lo, d_hi],
               dna[q] = polarity(e) }
```

`q` need not be the immediate successor of `p`: any pole-matching marker whose
distance from `p` falls in the window is admitted, which is how missed markers
are absorbed without special-casing. `d_lo` is a genuine lower bound (zero
whenever standstill cannot be excluded) and `d_hi` a genuine upper bound; both
are over-approximations by construction, so `H'` contains the truth whenever
`H` did -- the invariant P5 depends on. When `elapsed` is `UNKNOWN`,
`d_lo = 0` and `d_hi = infinity` (Sec. 6.2).

Uncertainty therefore contracts as often as it grows: a short elapsed admits
few `q` per `p`, and the polarity filter halves the survivors on average.

### 3.6 Branch-correct elapsed time

This replaces the previous draft's "dt since the previous accepted event" and
its dt-folding, which together could count the same interval twice.

**Rule.** Each branch `b` retains `b.last_genuine` — the `t_detect` of the last
detection **that branch** considers a genuine marker crossing — and
`b.epoch`, the clock epoch that timestamp belongs to. For a candidate detection
`e` evaluated against branch `b`:

```
elapsed(e, b) = e.t_detect − b.last_genuine        if e.clock_epoch == b.epoch
elapsed(e, b) = UNKNOWN                            otherwise
```

- The **genuine** branch of a fork advances `last_genuine` to `e.t_detect`.
- The **phantom** branch does **not** advance it. Its next candidate therefore
  measures from the same origin, and the full interval is counted exactly once
  on each branch. No dt is folded, accumulated or carried forward.
- Elapsed time never depends on whether the firmware accepted or rejected an
  event, and never on MQTT receipt time (P4). A detection the firmware rejects
  still carries a valid `t_detect`.
- An operator declaration sets `last_genuine` to the declaration's own
  `t_detect` on every live branch, and collapses the branch list to one.

**Worked contrast.** Genuine markers at t = 0, 1200, 2400; a ghost at 1250.
Old rule: the ghost consumed dt 50, then folded 50 into the successor's 1150,
and the 1200→2400 interval was represented twice across the two hypotheses.
New rule: the phantom branch keeps `last_genuine = 1200` and measures the
2400 detection as 1200 ms; the genuine branch keeps `last_genuine = 1250` and
measures it as 1150 ms. Each branch is internally consistent and neither
double-counts.

### 3.7 Timer reset, boot and timestamp discontinuity

`t_detect` is maintained on-device as a 64-bit extended counter over the 32-bit
millisecond timer, so ordinary rollover is not a discontinuity.

- **Boot.** `clock_epoch` increments and the counter restarts. Every retained
  branch's `b.epoch` is now stale, so every `elapsed` is `UNKNOWN`. A cold boot
  has no trustworthy anchor either, so the navigator enters `UNLOCATED` (or
  `ACQUIRING_ORIENTED` on an operator orientation declaration). Strategy A
  (§6) is not reached, because there are no pre-boot hypotheses worth retaining
  across a physical power interruption of unknown duration.
- **Internal redeclaration** (self-relabel, firmware re-anchor, a firmware
  timer being re-zeroed). Under this rule it is **not** a timing event at all:
  the monotonic counter is unaffected, so `elapsed` remains known and exact.
  Only the *label* is discarded, per P7. This is the single largest effect of
  the correction — most of what the previous design treated as unknown-time
  exposure simply is not.
- **Genuine discontinuity** (counter observed to move backwards, or an epoch
  mismatch mid-session). `elapsed` is `UNKNOWN` and §6 applies.
- **A detection with no prior `last_genuine` on a branch** (first event after a
  declaration in a fresh epoch) has `elapsed = UNKNOWN` for that branch only.

`clock_epoch` and the extended counter are published so a host replay computes
identical elapsed values from the same record.

### 3.8 Timing envelopes (P9)

Fast bounds are a **robust lower quantile** of the admitted distribution, not
`min × (1 − margin)`.

- Admission filters both ends. The existing filter rejects slow, dwell,
  stationary and PWM-uncovered samples and rejects nothing at the fast end; it
  gains a fast-end filter dropping ghost-signature (sub-floor peak) and
  dwell-signature (over-ceiling duration) samples.
- `fast_dt = quantile(admitted, q_fast) × (1 − margin)`.
- A tier with fewer than `MIN_N` admitted samples produces no bound; the next
  broader tier (section → direction → locomotive) is used.
- A bound implying a corridor speed above `SANITY_RATIO` × the bucket's median
  speed is rejected as contaminated and the broader tier used.

`q_fast`, `margin`, `MIN_N` and `SANITY_RATIO` are **engineering parameters**
(§10), not operator decisions. Envelope generation stays off-locomotive; the
device consumes a versioned table.

### 3.9 Amplitude and duration (P8)

The navigator gains the amplitude/duration criterion it currently lacks
entirely. It is a **prior on the pending branch, never a delete**:

- `GENUINE_LIKE`: peak ≥ `PEAK_FLOOR` and duration ≥ `DUR_FLOOR`.
- `GHOST_LIKE`: otherwise.

A `GHOST_LIKE` detection is never applied directly to `H`. It forks a branch
(§3.10) whose phantom hypothesis is preferred for telemetry ranking only. It is
not discarded — a weak reading of a real marker is possible, and deleting it
produces the label slips the recovery plan indicts. `PEAK_FLOOR` and
`DUR_FLOOR` are engineering parameters (§10). They are classification inputs
downstream of the entry threshold, which decision 0040 keeps as the only
detection gate; they suppress no detection.

### 3.10 Pending branches (P8)

A detection that cannot be applied cleanly — timing-vetoed, `GHOST_LIKE`,
polarity-unmatched, or ambiguous — forks:

- **genuine branch**: `H` propagated per §3.5/§3.6; `last_genuine = t_detect`.
- **phantom branch**: `H` unchanged; `last_genuine` unchanged.

Both are carried. The successor arbitrates: a branch whose propagation is empty
dies. If both survive, both are kept and `|H|` is their union — so no
confirmation, because uniqueness fails. Ties break toward phantom for ranking
only, never by deleting the genuine branch.

`PENDING_DEPTH_MAX` consecutive unresolved detections. Beyond that the branch
list collapses to its union and `INCOMPLETE` is raised — the design's one
truncation, made visible, and it costs confirmation authority only, not
hypotheses. `PENDING_DEPTH_MAX` is an engineering parameter (§10).

### 3.11 Confirmation (P3, P5, P11)

`SELF_CONFIRMED` requires **all** of:

1. `H` is `COMPLETE`.
2. Confirmation authority is not suspended (§6).
3. Either
   - **collapse**: `|H| = 1`, advanced coherently for `K_CONFIRM` successive
     detections at exactly the predicted successor each time, and `H` never
     exceeded `COLLAPSE_MAX_SET` since the anchor; or
   - **uniqueness**: the last `W_dir` (or `W_both` in `UNLOCATED`) accepted
     detections form a sequence matching exactly one route position, with no
     pending branch and no admitted missed marker inside the window. A window
     containing an admitted skip is not unique evidence — the observed string is
     then not the route string.
4. The confirmed position lies inside `H` (P2).

Route-wide reacquisition uses uniqueness only. Repetition inside a corridor is
never sufficient (P11).

### 3.12 Occupancy publication and two-locomotive safety

**On-device truth is the bitmap.** All fleet safety arithmetic on the device
operates over `H` — every candidate-position pair, or a demonstrably
conservative equivalent. A `H` bitmap may hold **disjoint islands** of
candidates and may **wrap across marker 170/0**; a single start/end arc can
therefore understate uncertainty badly, and the previous draft's single span is
withdrawn.

**Published representation.** In order of preference, whichever the contract
can carry:

1. **Multi-arc.** Up to `OCC_ARCS_MAX` circular arcs whose union covers every
   set bit, obtained by splitting at the `OCC_ARCS_MAX − 1` largest gaps in the
   bitmap. Arcs wrap; the wrap case is explicit, not an edge case.
2. **Single conservative covering arc.** The shortest circular arc containing
   every set bit. If that arc's length exceeds `ROUTE_WIDE_FRACTION` of the
   circuit, or if two distinct minimal covering arcs exist (a genuine tie, so
   the choice is ambiguous), the span is published as **route-wide**.

Every published form must satisfy the **conservatism invariant**: the published
occupancy is a superset of the true candidate set expanded by train extent, so
authority derived from the published form is never greater than authority
derived from the bitmap. This is a gated test (§8, test T15.5), not an
assertion.

**Separation.** Decision 0033 (bubble plus six markers clear) is evaluated over
the worst-case pair drawn from the two occupancies. Consequences:

- Two `POSITIONED` locomotives behave exactly as today.
- An `INCOMPLETE` occupancy is treated as route-wide regardless of how few bits
  are set.
- If either occupancy is route-wide, no separation can be guaranteed and both
  locomotives stop. This generalises decision 0031 from a state test to a
  geometric one.
- The uncertain locomotive yields; a `POSITIONED` peer retains authority up to
  the separation bound against the uncertain occupancy.

**A peer without a known location is not a safe peer.** Motion state is not a
safety property in either direction: a stopped peer whose occupancy is bounded
and disjoint from our candidates is safe to acquire around, and so is a moving
one; an unbounded peer is unsafe whether it is stopped or not. Stopping is a
claim about velocity and the hazard is about position.

The symmetric half matters just as much and is easier to miss: in
`ACQUIRING_ORIENTED` the binding constraint is usually **our own** unbounded
candidate set. No property of the peer — stopped, immobilised, or precisely
known — makes it safe to move when we might already be inside the separation
bound. Safety is a property of the **pair** of occupancies, and both must be
bounded and provably separated. Permitted acquisition contexts are enumerated
in §4.2.

#### 3.12.1 Peer condition vocabulary

The word "stopped" is not used as a safety predicate anywhere in this design.
Three distinct conditions are, and they are never inferred from the peer's own
navigation claims:

- **`PEER_COMMANDED_STOPPED`** — a commanded zero within freshness. It asserts
  only that a command was issued: the peer may still be running down the
  200 ms/step brake ramp, its own stationary belief comes from the stack under
  review (P4), and the report reaches us with 500 ms cadence and up to
  `CTO_PEER_STALE_MS` of permitted staleness. **Telemetry and operator display
  only. It grants no authority.**
- **`PEER_IMMOBILISED`** — movement prevented by a mechanism outside the peer's
  own control: an unpowered protected section, or a siding with points set
  against it. Declared by configuration or by the operator, latched, and
  cleared only explicitly.
- **`PEER_BOUNDED(region)`** — the peer's conservative occupancy (§3.12) is
  provably contained in a declared region.

`PEER_BOUNDED` is the load-bearing one. `PEER_IMMOBILISED` matters because it
keeps a bound from expiring, not because immobility is itself protective — an
immobilised peer of unknown position is an obstacle everywhere. Context 2 of
§4.2 therefore requires **both**: `PEER_IMMOBILISED ∧ PEER_BOUNDED(region)`.

## 4. The four states

Common to all four: the last trustworthy anchor, its time, direction history,
the evidence ring and the branch list are retained across every transition
(P1). Nothing below discards them.

### 4.1 POSITIONED

Exact MM and orientation trusted; normal navigation.

- **Entry.** Operator declaration under §7.5; or §3.11 confirmation from any
  other state.
- **Retained evidence.** Anchor with provenance and time; evidence ring;
  direction history; per-branch `last_genuine`.
- **Candidates.** `|H| = 1`.
- **Polarity.** Must match the predicted successor; a mismatch forks rather
  than relocating.
- **Spacing.** The interval table gives expected distance to the successor.
- **PWM timing.** Envelope veto per §3.8 on `elapsed(e, b)` from §3.6. Within
  `MARGINAL_SLACK` of the bound the detection forks instead of being vetoed.
- **Amplitude / duration.** `GHOST_LIKE` forks; position does not advance.
- **Pending.** Held per §3.10; position does not advance while unresolved. The
  odometer's claim is suspended, not rewritten.
- **Confirmation.** Successor agreement maintains `SELF_CONFIRMED`.
- **Contradiction.** All branches empty → §5.
- **Exit.** To `RECOVERING` on pending overflow, `|H| > 1` after propagation,
  an elapsed-time discontinuity (§3.7), or an eventless gap exceeding the
  envelope's one-interval bound. To `UNLOCATED` on contradiction. To
  `ACQUIRING_ORIENTED` on an operator orientation declaration clearing
  position.
- **Movement authority.** Full, including AUTO, at the previously authorised
  operating speed.
- **Station stopping.** Armed, subject to the ≥ `STATION_LOOKAHEAD_MARKERS`
  rule of §7.3.
- **Telemetry.** `nav_state=POSITIONED`, `movement_state=FULL_AUTHORITY`, `mm`,
  `dir`, anchor provenance and age, `pending_depth`, occupancy = 1 marker +
  extent.
- **Manual override.** A declaration under §7.5 re-anchors and clears pending
  state.
- **Two-locomotive.** Normal CTO separation.

### 4.2 ACQUIRING_ORIENTED

Operator declares orientation; position unknown. The locomotive acquires
without an MM declaration. This is the **smaller first operational target** of
the whole design.

- **Entry.** Operator orientation declaration with no position; or boot with a
  retained session direction and no trustworthy anchor.
- **Retained evidence.** Declared direction. An earlier anchor is retained but
  does not seed `H` unless the operator states the locomotive has not moved.
- **Candidates.** `H` = all 171 markers in the **declared direction only**
  (P10). 171 bits, never 342.
- **Polarity.** The first detection halves `H`; successive detections filter
  multiplicatively. Uniqueness arrives at `W_dir` clean observations (§3.3).
- **Spacing / PWM timing.** Each detection's `[d_lo, d_hi]`, from
  `elapsed(e, b)` and the envelopes, prunes hypotheses whose next pole-matching
  marker is out of window. This is what separates acquisition from a naive DNA
  string match: a hypothesis surviving the string can still die on physics.
- **Amplitude / duration, pending.** As §3.9/§3.10.
- **Confirmation.** §3.11 **uniqueness only**. Collapse is not accepted here: a
  collapse from a 171-wide set is exactly the aliasing case P11 forbids.
- **Contradiction.** `H` empty from a 171-wide start means the declared
  orientation or the map is wrong: stop and go to `UNLOCATED`.
- **Exit.** To `POSITIONED` on unique acquisition, with the automatic
  crawl-to-cruise ramp of §7.2 — **no operator GO**. To `UNLOCATED` on
  contradiction or orientation withdrawal. To `POSITIONED` immediately on an
  operator MM declaration.
- **Movement authority.** Motion is required to acquire. It is permitted only
  in one of three **permitted acquisition contexts**, and otherwise refused:

  1. **Alone.** No peer is enlisted, and none has been seen fresh within
     `CTO_PEER_STALE_MS`. Absence of a peer must be established by the
     membership rules of decision 0031, not inferred from silence alone.
  2. **Bounded, immobilised peer.** `PEER_IMMOBILISED ∧ PEER_BOUNDED(region)`
     (§3.12.1) — the peer is on a siding or protected section, held there by a
     mechanism outside its own control, and its occupancy is confined to that
     region by a configured declaration independent of the peer's own
     navigation state. Immobility alone does not qualify: the region is what
     makes the disjointness test computable.
  3. **Disjoint by independent constraint.** An independently authoritative
     starting-region constraint — an operator-declared placement region, or a
     configured protected region the acquiring locomotive is known to be
     within — makes the candidate set provably disjoint from the peer's
     conservative occupancy under §3.12, with the separation margin of 0033
     intact for every candidate pair.

  **None of these contexts is created by the peer being stopped**, and none is
  defeated by the peer moving. Each works by bounding both occupancies and
  showing them separated: context 1 by there being no second occupancy,
  contexts 2 and 3 by `PEER_BOUNDED(region)` (§3.12.1) together with a
  constraint on our own candidate set. `PEER_COMMANDED_STOPPED` appears in none
  of them and grants no authority.

  Without one of the three, orientation-only acquisition **does not move**: the
  locomotive stands, publishes `STOPPED_FOR_NAVIGATION_SAFETY` with the reason,
  and waits. It does not demand a manual MM declaration.

  Where motion is permitted, it is at `ACQ_SPEED` with no AUTO mission.
- **Station stopping.** Prohibited; the station machine is disarmed. An
  approach cannot be armed from a position that is not known.
- **Telemetry.** `nav_state=ACQUIRING_ORIENTED`, `movement_state`, `dir`,
  `|H|`, occupancy, `acquisition_context` (which of the three, or none),
  observations since entry, `W_dir` remaining, `complete`, movement reason
  code.
- **Manual override.** An MM declaration short-cuts acquisition and is
  authoritative. It is never demanded.
- **Two-locomotive.** Occupancy is route-wide at entry and shrinks with each
  observation. Under §3.12 and the three contexts above, acquisition alongside
  a non-isolated peer is not permitted while moving.

### 4.3 RECOVERING

Position became uncertain during operation.

- **Entry.** From `POSITIONED` on ambiguity, pending overflow, elapsed-time
  discontinuity, or an over-long eventless gap. **Entry is a navigation-state
  change only and does not by itself command any speed change** (P13, §7.4).
- **Retained evidence.** The last trustworthy anchor and its time; direction
  history; accumulated motion evidence; the pre-loss `H`; the evidence ring;
  branches with their `last_genuine` timestamps. This retention is the point of
  the state and is what distinguishes it from `UNLOCATED` (P1).
- **Candidates.** `H` seeded from the pre-loss hypotheses and propagated
  forward. Not route-wide, and not reseeded route-wide unless elapsed evidence
  makes a full circuit physically possible.
- **Polarity, spacing, PWM timing, amplitude, pending.** Identical to
  `POSITIONED` — there is no separate recovery algorithm, which is the
  structural difference from QUORUM.
- **Confirmation.** §3.11, with authority suspended while the chain is
  gap-bearing (§6). Collapse is accepted only when not gap-bearing and `H`
  never exceeded `COLLAPSE_MAX_SET` since the anchor; above that, uniqueness is
  required.
- **Contradiction.** §5 → stop and `UNLOCATED`.
- **Exit.** To `POSITIONED` on confirmation, **automatically restoring the
  previously authorised speed** (§7.4) with no operator GO. To `UNLOCATED` when
  `H` reaches route-wide extent, on contradiction, or at `RECOVER_WINDOW`
  expiry.
- **Movement authority.** Governed by §7.4, not by a fixed crawl. In summary:
  continue at the current authorised speed while the `COMPLETE` set stays
  narrow enough that braking, separation and station authority all hold for
  every viable candidate; reduce to a **derived** ceiling when they do not;
  stop when they cannot be satisfied at any speed.
- **Station stopping.** New approaches are not armed. An approach already in
  `ST_FINAL` at entry completes, because aborting a final brake is worse than
  finishing it; any earlier phase is abandoned and the zone speed held.
- **Telemetry.** `nav_state=RECOVERING`, `movement_state` (one of
  `RECOVERING_WITH_AUTHORITY`, `SPEED_LIMITED_FOR_UNCERTAINTY`,
  `STOPPED_FOR_NAVIGATION_SAFETY`), anchor and age, `|H|`, occupancy,
  `gap_bearing`, `confirmation_authority`, `pending_depth`, entry reason,
  `speed_ceiling` and its binding constraint, `RECOVER_WINDOW` remaining.
- **Manual override.** An MM declaration under §7.5 resolves it instantly.
  Expiry leads to a stop and `UNLOCATED`, not to a prompt.
- **Two-locomotive.** Occupancy published per §3.12; the peer's authority
  reduces against it; when the two cannot be separated, both stop.

### 4.4 UNLOCATED

Neither position nor orientation is trusted.

- **Entry.** Cold boot with no retained anchor; contradiction; `RECOVERING`
  expiry or route-wide extent; operator command; loss of orientation
  confidence.
- **Retained evidence.** The evidence ring and last trustworthy anchor are
  retained **for telemetry and forensics only** and do not seed `H`.
- **Candidates.** `H` = all 342 bits. This is the only state that starts
  route-wide, and it does so because there is genuinely no anchor — not because
  a corridor was allowed to grow (P1).
- **Polarity.** Filters both direction planes; the reversed DNA read is a
  different string, so the planes decay at different rates and orientation
  usually resolves before position.
- **Spacing / PWM timing / amplitude / pending.** As above, per plane.
- **Confirmation.** §3.11 uniqueness only, at `W_both`, unique across **both**
  planes.
- **Contradiction.** `H` empty from a 342-bit start is a map or detector fault,
  not a navigation error: stop, latch a fault, publish, do not re-seed.
- **Exit.** To `POSITIONED` on unique two-plane acquisition (automatic ramp per
  §7.2); to `ACQUIRING_ORIENTED` on an operator orientation declaration; to
  `POSITIONED` on an operator MM declaration.
- **Station stopping.** Prohibited.
- **Telemetry.** `nav_state=UNLOCATED`, `|H|` per plane, occupancy, entry
  reason, observations since entry, whether orientation has resolved, and an
  explicit `manual_declaration_required=false` — the operator is informed, not
  summoned.
- **Manual override.** Both declaration forms accepted and authoritative.
- **Two-locomotive.** Occupancy is route-wide by definition, so no autonomous
  motion is authorised for either locomotive while an `UNLOCATED` peer is on
  the railway.

#### 4.4.1 What "autonomous" honestly means here

Four capabilities are distinct and the previous draft blurred them:

| capability | `UNLOCATED` status |
|---|---|
| **autonomous position reasoning** | **yes** — the navigator forms, filters and resolves hypotheses with no operator input |
| **autonomous movement authority** | **policy decision below** — under option A, no |
| **operator-commanded crawl** | yes — the operator may command motion, which supplies the observations reasoning needs |
| **manual MM declaration** | optional shortcut, never required |

Under option A the state is autonomous in reasoning and **not** autonomous in
movement: it cannot gather the observations it needs without an operator
commanding motion. That is a real limitation and is stated rather than
implied.

#### 4.4.2 Operator policy decision — UNLOCATED crawl

**Option A — never moves autonomously.** The locomotive stops and stays
stopped. The operator may command a crawl at `ACQ_SPEED`, under fleet hold if a
peer is present, and that crawl supplies the observations.

**Option B — automatic crawl when provably alone or isolated.** The locomotive
may crawl on its own at `ACQ_SPEED` when, and only when, one of the permitted
acquisition contexts of §4.2 is established by the same independent means. It
stops at `W_both` acquisition or at any loss of the context.

| | A | B |
|---|---|---|
| safety | motion always follows a human decision; the human is present and can see the railway | motion depends on the isolation evidence being correct with no human in the loop, in the state where the locomotive knows least |
| recovery burden | the operator must notice and act; a locomotive can sit unlocated indefinitely | self-clearing when alone |
| failure mode | a stopped locomotive | a moving locomotive with no position knowledge and a wrong isolation belief |

**Recommendation: A**, and it is a recommendation, not a decision — §9 records
it as an operator choice. The reasoning: `UNLOCATED` is precisely the state
where the locomotive's own beliefs are least constrained, so it is the worst
state in which to also trust its belief about being alone. Option B's benefit
is convenience; option A's cost is a human decision that a human is already
well placed to make. **Orientation-declared acquisition (§4.2) remains the
smaller first operational target**, and it delivers most of B's operational
benefit under an operator-supplied orientation, which is cheap to give.

## 5. Contradiction

All branches empty means the model is wrong — a missed-marker model failure, a
map fault or a detector fault. Never invent a position, never permanently halt:

1. Movement authority drops to zero; stop under the station brake ramp.
2. State becomes `UNLOCATED`, `H` re-seeded to all 342 bits.
3. The evidence ring, last trustworthy anchor and a complete diagnostic
   snapshot are latched and published (§7.6).

Recovery is by §3.11 uniqueness or an operator declaration. Neither is
demanded.

## 6. Elapsed-time discontinuity: the two safe strategies

### 6.1 The correction shrinks this problem

Under branch-correct timing (§3.6, §3.7), the three cases the previous draft
conflated separate cleanly:

**Case D — operator declaration.** Position authoritative, `last_genuine` set
by the same act. Not a loss and not an uncertainty event. `H = {declared}`.

**Case R — internal redeclaration.** The monotonic counter is untouched, so
elapsed remains **known and exact**. Only the label is discarded (P7). Under
the previous draft this was the dangerous case; under branch-correct timing it
is not a timing event at all. Most of what the iteration-3 record counted as
dt-chain resets falls here.

**Case I — genuine discontinuity.** Boot, epoch mismatch, or an observed
backwards counter. Elapsed is `UNKNOWN`. Only this case needs a strategy — and
boot, its commonest instance, has no retained anchor anyway and goes straight
to `UNLOCATED`/`ACQUIRING_ORIENTED`.

### 6.2 Strategy A — bounded growth, confirmation authority suspended

Retain the pre-discontinuity hypotheses. Propagate the affected detection with
`d_lo = 0` and `d_hi = ∞`, admitting every pole-matching marker ahead in each
live direction — an honest over-approximation. Mark the chain **gap-bearing**.
While gap-bearing, `H` filters normally on every subsequent detection but
**confirmation authority is suspended**: a collapse to one hypothesis may not
confirm. Authority returns only on §3.11 uniqueness, which clears the flag.

### 6.3 Strategy B — immediate explicit unknown-position state

Treat any Case-I discontinuity as loss of positional authority: go to
`UNLOCATED` (or `ACQUIRING_ORIENTED` if orientation is independently trusted)
and require full reacquisition.

### 6.4 Comparison

| axis | A | B |
|---|---|---|
| false confirmation | impossible while gap-bearing; the only exit is the same uniqueness test B uses | impossible; no position claimed |
| over-approximation | preserved (`d_hi = ∞` is a true upper bound, unlike the withdrawn one-interval grant, which was an *under*-approximation and could exclude the truth) | preserved trivially |
| information retained | pre-discontinuity hypotheses and direction survive; if the gap was short, `H` stays small and uniqueness returns quickly | discarded; every discontinuity costs a full `W` window |
| movement | `RECOVERING` rules (§7.4) | `UNLOCATED` rules |
| operational burden | proportionate to the actual gap | fixed and large per event — **but Case I is now rare**, so this argument is much weaker than in the previous draft, where Case R was wrongly counted here |
| ESP32 cost | one flag plus one wide propagation, ~44 bytes | marginally cheaper per event; needs the identical acquisition machinery to return, so saves no code |
| failure if implemented wrongly | a missed suspension re-opens false confirmation | a missed transition leaves a stale position |

### 6.5 Recommendation: **A**, with automatic degeneration to B's state

**Strategy A is retained as the recommendation**, on the corrected reasoning:

1. **B is A minus information at equal safety.** Both re-enter `POSITIONED`
   only through the same uniqueness test. B differs only by discarding the
   pre-discontinuity constraint, which can never make a confirmation safer —
   only later.
2. **ESP32 feasibility is equal.** B needs the whole acquisition path anyway.
3. **The operational argument is now smaller and is stated as such.** The
   previous draft justified A partly on Otto boot16's 21 dt-chain resets. Under
   §3.6 most of those are Case R and cost nothing under either strategy. A's
   remaining advantage is the information argument, which stands on its own.

A carries B's outcome as its floor: a gap-bearing `H` that reaches route-wide
extent exits to `UNLOCATED`, which *is* strategy B.

**This choice is against the known replay, not for it.** Under A the eight Otto
boot16 route-wide recoveries do not become confirmations — they were repetition
inside an unvalidated corridor, and §3.11 refuses them. A produces *fewer*
confirmations on that session than the frozen navigator, and several stops
where it currently relocates. Passing a known replay is not evidence for this
design and is not claimed as such.

## 7. Movement authority, speed and the operator's role

### 7.1 Navigation state is not speed state (P13)

The navigator publishes both, separately:

- `nav_state` ∈ {`POSITIONED`, `ACQUIRING_ORIENTED`, `RECOVERING`,
  `UNLOCATED`}
- `movement_state` ∈ {`FULL_AUTHORITY`, `RECOVERING_WITH_AUTHORITY`,
  `SPEED_LIMITED_FOR_UNCERTAINTY`, `STOPPED_FOR_NAVIGATION_SAFETY`}

Entering `RECOVERING` sets `RECOVERING_WITH_AUTHORITY` by default and commands
no speed change. A single doubtful detection must not trigger crawl and must
not cancel AUTO.

### 7.2 Crawl-to-cruise, ACQUIRING_ORIENTED → POSITIONED

**The unique `W_dir` acquisition is itself the stability gate.** No arbitrary
settling delay and no operator confirmation is added on top of it. On entry to
`POSITIONED`, if fleet separation is satisfied for the now-single position, no
safety-relevant pending branch remains, and movement is otherwise authorised,
the navigator **automatically ramps** from `ACQ_SPEED` to the previously
authorised operating speed, using the existing station ramp rates.

Station stopping remains disabled until `POSITIONED` and is then subject to
§7.3.

### 7.3 Station lookahead after acquisition

Operationally the locomotive is placed at least `STATION_LOOKAHEAD_MARKERS`
(proposed: 12) before its intended first station stop. On entering
`POSITIONED` from any acquisition or recovery state, the navigator checks
whether the intended station is at least `STATION_LOOKAHEAD_MARKERS` ahead in
the travel direction:

- **Yes** — arm that approach normally.
- **No** — do not attempt that stop. Target the following permitted station and
  publish the substitution with its reason. A stop attempted from inside the
  braking distance is worse than a stop skipped.

### 7.4 Speed while RECOVERING — derived, not fixed

The previous draft imposed PWM 60 on every recovery. That is withdrawn. The
ceiling is **derived from the worst case over all viable candidates and the
peer's conservative occupancy**:

```
v_ceiling = max v such that, for EVERY candidate c in the COMPLETE set H:
    braking_distance(v) ≤ distance from c to the nearest
        (a) separation bound against the peer's published occupancy (§3.12),
        (b) armed station's final-brake entry point, and
        (c) any configured protected-region boundary
```

with `braking_distance` taken from the existing `STATION_DOWN_STEP_MS` ramp.
Consequences that matter:

- A narrow `H` far from the peer yields `v_ceiling ≥` the current speed, so
  **recovery on clean evidence proceeds at line speed** and
  `movement_state` stays `RECOVERING_WITH_AUTHORITY`.
- The ceiling falls only when uncertainty *materially* limits reachability,
  station or separation authority — then `SPEED_LIMITED_FOR_UNCERTAINTY`.
- If no `v > 0` satisfies the condition, `STOPPED_FOR_NAVIGATION_SAFETY`.
- On confirmation, the previously authorised speed is **automatically
  restored**. No operator GO.

### 7.4.1 Hysteresis — no CTO2-style oscillation

One unresolved detection may not cause crawl/cruise cycling:

- A reduction requires the binding constraint to hold for
  `SPEED_HYST_EVENTS_DOWN` consecutive detections.
- A restoration requires it to have cleared for `SPEED_HYST_EVENTS_UP`
  consecutive detections, with `SPEED_HYST_EVENTS_UP > SPEED_HYST_EVENTS_DOWN`.
- A reduction of less than `SPEED_STEP_MIN_PCT` is not commanded at all.
- These are engineering parameters (§10).
- Exception: a constraint that would be violated *before* the next detection
  arrives is acted on immediately. Hysteresis delays discretionary reductions,
  never a genuine safety brake.

**Counters**, published and reported: `nav_speed_reductions`,
`nav_speed_restorations`, `nav_unscheduled_stops`, each with the binding
constraint that caused them.

### 7.5 The operator's role (P14)

The operator **may**:

- declare starting **orientation**;
- make an **authoritative MM declaration**, but only after deliberate
  stationary placement or direct stationary verification — the declaration
  interface must record which, and a declaration issued while the navigator
  observes motion is rejected with a reason rather than silently accepted;
- **authorise initial movement**;
- issue **STOP / E-STOP** at any time;
- **authorise restart** after an unscheduled navigation stop.

The operator **does not**:

- confirm self-acquired or recovered MM positions — visual observation and the
  MQTT console are both delayed, so this is not a check the operator can
  reliably perform on a moving locomotive;
- supply a GO merely because the navigator entered `POSITIONED` while already
  moving. Routine operator position confirmation and routine post-recovery GO
  are **removed from the design**.

A GO is required only after the locomotive **actually stopped**, entered fleet
hold, hit a contradiction, or explicitly cancelled AUTO. The existing
`nqDropAutoInterlock()` behaviour is retained for exactly those cases and is
not applied to a recovery that never stopped.

E-stop clearing must not force leaving AUTO; that operator requirement is
unchanged by this design.

### 7.6 Unscheduled navigation stops

A stop caused by navigation uncertainty is a **safe failure outcome and not
normal successful operation**. Each one:

- latches a complete diagnostic snapshot — `H`, branch list with per-branch
  `last_genuine`, evidence ring, anchor and provenance, envelope lookups used,
  binding constraint, peer occupancy at the time — for later investigation;
- obeys all fleet, station and braking rules;
- **does not demand an immediate manual MM declaration**;
- requires a deliberate operator restart after investigation.

The design objective is that navigation stops are safe, rule-compliant and
**rare**. §8 gates rarity separately from safety.

## 8. Success criteria: safety and usefulness are separate gates

A navigator that always stops passes every safety gate. It fails the design.
The test plan implements both sets; neither substitutes for the other.

**Safety gates (must never fail):**

1. No false confirmation.
2. The true position is in `H` at every detection where `H` is `COMPLETE`.
3. No unsafe movement — no authority granted that could close a separation
   bound under any candidate pair.
4. Stopping is always available.

**Usefulness gates (must be met on the specified case families):**

5. Orientation-known startup on a clean stream acquires the **correct**
   position from **every** MM in **both** directions. "Or stops" does not
   satisfy this.
6. Acquisition completes within `W_dir` clean observations (`W_both` for
   `UNLOCATED`), where `W` is the **verified** map uniqueness length of §3.3.
7. Orientation-unknown acquisition succeeds on clean streams whenever movement
   is externally authorised.
8. Normal recovery on clean evidence **reacquires rather than stopping**.
9. An ordinary isolated ghost, and a single missed marker, do not cause
   permanent loss and do not cause an unscheduled stop.
10. Clean generated operation, orientation-declared acquisition and recoverable
    isolated faults produce **zero** unscheduled navigation stops and no
    repeated crawl/cruise cycling.
11. Acquisition latency (observations and distance), stop rate, and the §7.4.1
    speed-change counters are reported for every family.

Stopping remains acceptable — and expected — for genuinely ambiguous,
incomplete or corrupted cases. The test plan states, per family, which regime
applies.

## 9. Operator policy decisions

These are **operational policy**, not statistics. They require an operator
ruling before implementation.

1. **Uncertain-motion policy.** Whether a locomotive whose position is
   uncertain may move at all (§4.2, §4.3, §7.4).
2. **UNLOCATED crawl policy.** Option A (commanded only) or option B
   (automatic when provably alone/isolated). **Recommendation: A** (§4.4.2).
3. **Permitted acquisition contexts with a peer present.** Acceptance of the
   three contexts in §4.2, and the ruling that a merely stopped peer is not
   one of them.
4. **Fleet stop / yield behaviour.** That an `INCOMPLETE` or route-wide
   occupancy forces a fleet stop, generalising decision 0031 from a state test
   to a geometric one, and that the uncertain locomotive yields (§3.12).
5. **Station behaviour.** That `ST_FINAL` completes on entry to `RECOVERING`
   while earlier phases abandon (§4.3), and the
   `STATION_LOOKAHEAD_MARKERS = 12` substitution rule (§7.3).
6. **Orientation-only command semantics.** A new operator command declaring
   direction without position is required for `ACQUIRING_ORIENTED` to be
   reachable; none exists today. Its name, its effect on a running mission and
   whether it may be issued while moving all need a ruling.
7. **Telemetry and CTO contract changes.** The additive fields of §4 and §7.1,
   and the occupancy representation of §3.12 — multi-arc if the contract can
   carry it, single conservative arc otherwise. The recovery plan places the
   MQTT and Pi controller contract out of scope, so even additive changes need
   an explicit ruling.
8. **Restart policy after an actual safety stop** (§7.6).
9. **Initial movement authorisation** — that the operator authorises the first
   movement of a session, and that no further routine GO is required (§7.5).
10. **Strategy A** for Case-I discontinuities, with automatic degeneration to
    `UNLOCATED` (§6.5).

Routine operator position confirmation and routine post-recovery GO are
**not** on this list: they are removed from the design (§7.5).

## 10. Engineering parameters requiring calibration and test justification

These are **not** operator decisions. Each is an engineering parameter with a
recommended default; each must be justified by committed calibration evidence
and by the acceptance tests **before candidate freeze**. A default that no
evidence supports blocks the freeze.

| parameter | default | justification required |
|---|---|---|
| `q_fast` | 0.02 | envelope sensitivity sweep; the chosen quantile must not admit ghost/dwell-contaminated samples at any bucket |
| `margin` | 0.15 | corridor-growth vs false-veto trade measured on generated cases |
| `SANITY_RATIO` | 3× median | must reject the PWM-90 contamination pattern and accept the clean PWM-100 bucket |
| `MIN_N` | 8 | tier-fallback stability |
| `PEAK_FLOOR`, `DUR_FLOOR` | from measured genuine/ghost separation | separation must be demonstrated, with the false-hold and false-pass rates reported |
| `COLLAPSE_MAX_SET` | 8 | must be small enough that no aliased collapse confirms in T12 |
| `PENDING_DEPTH_MAX` | 3 | ghost-family recovery in T11 without `INCOMPLETE` on ordinary isolated ghosts |
| `MARGINAL_SLACK` | 0.25 | genuine-acceleration retention in T9 |
| `ACQ_SPEED` | station-zone speed (PWM 60) | braking-distance and acquisition-latency trade |
| `RECOVER_WINDOW` | lesser of 90 s / 40 observations | recovery-success vs exposure trade |
| `OCC_ARCS_MAX` | 3 | conservatism invariant plus contract size |
| `ROUTE_WIDE_FRACTION` | 0.6 circuit | authority granted vs bitmap authority, T15.5 |
| `SPEED_HYST_EVENTS_DOWN` / `_UP` | 2 / 5 | no cycling in T16 |
| `SPEED_STEP_MIN_PCT` | 10% | no cycling in T16 |
| `STATION_LOOKAHEAD_MARKERS` | 12 | operator-stated placement practice; braking distance must fit |
| `K_CONFIRM` | 3 | collapse-path false-confirmation rate in T12 |
| `W_dir`, `W_both` | **computed, not chosen** | prerequisite check P0 (§3.3) |

## 11. What this replaces

Fixed-offset recovery is removed, not extended: no offset table, no evaluation
budget, no margin-of-two adoption, no fixed reacquisition fence, no
advisory-only DNA match. Recovery is not a separate algorithm bolted to
tracking — §3.5 propagation *is* tracking, and the four states differ only in
how `H` is seeded and what authority the result carries. Component-level
disposition is in `docs/AUTONOMOUS_ACQUISITION_IMPLEMENTATION_MAP.md`.
