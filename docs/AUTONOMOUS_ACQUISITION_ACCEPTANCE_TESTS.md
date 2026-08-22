# Host-model acceptance tests — written before implementation

Companion to `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`. Status: Proposed.
Corrected 2026-08-22; correction pass 3 follows the operator rulings at
6fba58c.
These tests are written and committed **before** the navigator they test. They
run entirely off-locomotive.

## 0. Ground rules

- **Generated cases with known ground truth.** Every case is synthesised on the
  real committed map with the true position and direction known by
  construction. No verdict depends on a firmware label, a heuristic
  classification, or an existing log.
- **Existing Toby and Otto sessions are development data.** They may be
  replayed for regression and to make generated event distributions realistic.
  A replay result is never an acceptance verdict.
- **Two independent gate sets.** Safety gates (§S) and usefulness gates (§U).
  A build must pass both. **A navigator that always stops passes every safety
  gate and fails the suite.**
- **"Or stops" is not a pass on a clean stream.** Where a family below is
  marked **CLEAN**, stopping is a failure. Where it is marked **AMBIGUOUS**,
  stopping is an acceptable outcome and confirming wrongly is not.

## P0 — prerequisite: verified map uniqueness (blocking)

Automated, run before any other test, over the committed map. Not an
assertion inherited from prior documents.

- For window length `W = 1 … 24`, and for **every rotational starting position**
  and **both travel directions**, enumerate the 342 observed polarity windows.
- Report `W_dir` = least `W` at which all 171 windows are unique within a
  single direction plane, and `W_both` = least `W` at which all 342 windows are
  unique across both planes.
- Report the full collision table for every `W` below those values.
- **Blocking:** if `W_dir` or `W_both` does not exist at any `W ≤ 24`, the map
  contains a route-wide alias and the corresponding acquisition mode is not
  implementable; it must be reported as such, not approximated.
- `W_dir` and `W_both` computed here are the values used by §3.11 confirmation
  and by usefulness gate U2. The received claim that `W ≥ 10` is unique and
  `W = 9` collides four ways is **re-derived here**, not assumed.

## S — safety gates (must never fail)

- **S1. No false confirmation.** Across every case in every family, and across
  a randomised sweep of at least 10,000 generated sessions, the navigator never
  enters `POSITIONED` at a position or direction other than the true one.
  Asserted at every confirmation, not summarised.
- **S2. No under-approximation.** At every detection, the true (position,
  direction) is a member of `H` whenever `H` is marked `COMPLETE`. Checked
  continuously. This is the property the withdrawn one-interval grant violated.
- **S3. No unsafe movement.** No authority is granted that could bring the two
  locomotives inside the decision-0033 bound under **any** pair drawn from the
  two candidate sets.
- **S4. Stopping is always available.** No case reaches a state whose only
  action is to demand an operator declaration.
- **S5. Elapsed time is never double-counted.** For every branch and every
  detection, `elapsed` equals `t_detect − branch.last_genuine` exactly; the sum
  of elapsed intervals along any single branch equals the wall-clock span of
  that branch. Asserted directly against the generator's own clock.

## U — usefulness gates (must be met)

- **U0.** Exact-MM startup remains available and enters `POSITIONED`
  immediately (T0).
- **U1.** Launch-region acquisition on a clean stream acquires the **correct**
  position from every MM in MM030–MM055 in both orientations (T1a); route-wide
  orientation-known startup does so from every MM in both directions (T1).
- **U2.** Acquisition completes within `W_dir` clean observations, or `W_both`
  in `UNLOCATED`, using P0's computed values (T1, T2).
- **U3.** Orientation-unknown acquisition succeeds on clean streams whenever
  movement is externally authorised (T2).
- **U4.** Powered-run recovery **retains the last trustworthy anchor** and on
  clean evidence **reacquires rather than stopping** (T3).
- **U4b.** A single doubtful detection causes **no speed-state transition** and
  no AUTO cancellation (T16).
- **U4c.** Manual operation without position permits movement, activates
  neither AUTO nor station behaviour, and reports a self-acquired position
  without starting AUTO (T21).
- **U5.** An isolated ghost, and a single missed marker, cause neither
  permanent loss nor an unscheduled stop (T7, T10).
- **U6.** Normal launch, launch-region acquisition, clean generated operation
  and recoverable isolated faults produce **zero** unscheduled navigation stops
  and no repeated crawl/cruise cycling (T1a, T16, T22).
- **U7.** Every family reports acquisition latency in observations and in
  millimetres travelled, unscheduled-stop rate, and the §7.4.1 speed-change
  counters. Reported, not gated, except where a family names a bound.

## T0 — exact-MM startup remains available — **CLEAN**

Mode 1 (§4.0): an exact MM or interval declaration at every marker, both
directions, locomotive stationary.

- Pass: `POSITIONED` immediately, `|H| = 1`, AUTO available, station arming
  subject only to §7.3.
- Pass: no acquisition sequence is required or attempted first. Self-acquisition
  being optional means mode 1 costs nothing.

## T1a — launch-region acquisition — **CLEAN**

Mode 2 (§4.0): `ACQ_LAUNCH_REGION`, `H` seeded to MM030–MM055 in the declared
direction, true start swept over **every marker MM030–MM055 in both
orientations (52 cases)**, clean stream.

- **Pass: every case reaches `POSITIONED` at the true marker.** Stopping is a
  failure — this family exists to show launch-region acquisition is *useful*,
  not merely safe (U1).
- Pass: zero unscheduled navigation stops across the family (U6).
- Pass: on acquisition, `STATION_LOOKAHEAD_MARKERS` is applied per T18 — cases
  acquiring near MM055 CW must skip Grillers.
- Report: latency in observations and mm. A 26-bit seed should resolve
  faster than the 171-bit seed of T1; both numbers are reported so the
  operational benefit of declaring the launch region is measured, not assumed.

## T1b — launch region is never presumed — **safety**

- Pass: a locomotive booting with no startup selection is in mode 3
  (`UNLOCATED`), **not** mode 2. A build that defaults to the launch-region
  seed fails.
- Pass: mode 2 selected while the locomotive is truly outside MM030–MM055
  produces either a contradiction (§5) or no confirmation — **never** a
  confident wrong position. This is the operator-error case the explicit
  selection exists to bound, and S1 covers it.

## T1 — orientation-known route-wide startup at every possible MM — **CLEAN**

`ACQUIRING_ORIENTED`, declared direction correct, true start swept over all 171
markers × both directions (342 cases), clean stream, movement externally
authorised.

- **Pass: every case reaches `POSITIONED` at the true marker.** Stopping is a
  failure (U1).
- Pass: within `W_dir` observations in every case (U2).
- Report: latency distribution in observations and mm.

## T2 — orientation-unknown startup — **CLEAN**

`UNLOCATED` from all 342 starts, clean stream, movement supplied by the
operator's manual throttle (§4.4.3) — the navigator authorises none from an
unknown position, and after the 2026-08-22 ruling it never will (§4.4.2).

- Pass: every case resolves to the true (marker, direction) within `W_both`.
- Pass: orientation resolves no later than position in every case; a position
  confirmed while direction is still ambiguous is a defect.

## T3 — normal loss and recovery — **CLEAN**

From `POSITIONED`, force `RECOVERING` (pending overflow), then a clean stream.
Swept over all 171 markers × both directions × loss at 1, 3 and 8 markers.

- **Pass: every case reacquires. Stopping is a failure (U4).**
- **Pass: the last trustworthy anchor, direction and reversal history,
  per-branch `last_genuine`, PWM/motion history and pending evidence all
  survive the transition intact**, asserted field by field against the
  pre-loss snapshot. A recovery that re-seeds route-wide on a mid-run loss
  fails outright.
- Pass: S2 at every detection.
- Pass: no case reaches route-wide extent on a clean stream — that would
  reproduce the iteration-3 corridor defect.
- Pass: `movement_state` stays `RECOVERING_WITH_AUTHORITY` throughout where the
  candidate set stays narrow and no peer is present; a speed reduction here is
  a defect, not a safe choice (§7.4).

## T4 — reversal during acquisition — **CLEAN**

Motor flips mid-acquisition, in `ACQUIRING_ORIENTED` and `RECOVERING`, at 1–10
observations after entry.

- Pass: hypotheses preserved and travel direction reversed; the truth stays in
  `H`; acquisition still completes.
- Pass: a would-be unique window straddling the reversal is **not** treated as
  unique — the observed string is not a route string across a reversal.

## T5 — declaration-accompanied timing origin (Case D) — **CLEAN**

An operator declaration mid-run, swept over all 171 markers, both directions,
PWM buckets 40/60/90/100.

- Pass: `POSITIONED` retained, `H = {declared}`, `last_genuine` set to the
  declaration's own `t_detect` on every live branch, branch list collapsed to
  one, and subsequent tracking normal.
- Pass: a transition to `RECOVERING` here is a failure.

## T6 — elapsed-time discontinuity (Case I) — **AMBIGUOUS**

Successor to `probe_dt0_unknown_time.py`, and the test the frozen navigator
fails. Sweep all 171 starts × both directions × true elapsed gaps of 0, 1, 2,
3, 4, 6, 10 and 20 intervals during the unknown time.

- **Pass: zero confirmed-wrong outcomes at every gap size**, including gaps far
  outside any assumed domain. The frozen navigator produces 7–25 wrong
  confirmations per 171 at gaps ≥ 2; the replacement must produce none.
- Pass: S2 at every gap size.
- Pass: gap-bearing is marked and confirmation authority suspended until §3.11
  uniqueness clears it.
- Control: the no-discontinuity run must be correct 171/171.
- Report: observations to uniqueness per gap size — the measured operational
  cost of Strategy A.

## T6b — Case R is not a timing event — **CLEAN**

Internal redeclarations (self-relabel, firmware re-anchor, firmware timer
re-zero) injected mid-run at every marker, with the monotonic counter
**unaffected**.

- Pass: `elapsed` remains known and exact across the redeclaration; no
  gap-bearing flag; no corridor grant.
- Pass: the firmware's MM label is discarded and never enters `H` (P7).
- Pass: no unscheduled stop and no speed reduction (U6).
- This family is the direct check on the §3.6 correction: under the withdrawn
  design these cases were treated as unknown-time exposure.

## T6c — branch timing arithmetic — **CLEAN**

Interleaved genuine markers and ghosts with generator-known true times,
including the §3.6 worked case (genuine 0/1200/2400, ghost at 1250) and chains
of 1–3 ghosts between genuine markers.

- Pass: S5 exactly — per branch, elapsed intervals sum to the branch's
  wall-clock span, with no interval counted twice and none omitted.
- Pass: the phantom branch's `last_genuine` never advances; the genuine
  branch's always does.
- Pass: firmware accept/reject state, injected as a decoy field, changes no
  computed elapsed value.

## T7 — missed markers during unknown and known time — **mixed**

1–15 consecutive markers producing no detection, with and without an
accompanying discontinuity, at every start position.

- Pass (S2): the truth remains in `H` throughout. A missed run widens `H`,
  never shifts it off the truth.
- Pass: no confirmation from a window containing an admitted skip.
- **CLEAN sub-family — a single missed marker (U5):** must not cause permanent
  loss and must not cause an unscheduled stop; recovery proceeds without
  operator action.
- **AMBIGUOUS sub-family — 5+ missed markers:** stopping is acceptable.

## T8 — same-magnet rereads — **AMBIGUOUS**

Stationary or crawling re-reads of one magnet, 2–20 times, at every marker,
with and without a discontinuity straddling the run.

- Pass: no forward advance claimed; the stay hypothesis survives.
- Pass: no confirmation by repetition — repeated identical evidence is one
  observation, not `K_CONFIRM` of them.
- Pass: polarity alternation within a run is evidence of **motion** (a
  stationary re-read cannot alternate pole) and the stay hypothesis dies.

## T9 — genuine acceleration — **CLEAN**

dt shortening sharply between consecutive genuine markers, at every PWM bucket,
including 40→90 and 60→99 transitions within one interval.

- Pass: genuine events stay genuine. None is vetoed outright; at worst it forks
  and the successor restores it.
- Pass: no frozen run of consecutive rejections in any generated case.
- Pass: no unscheduled stop, no speed reduction.

## T10 — weak short-duration ghosts — **CLEAN**

Single ghosts below the §3.9 floors, injected between genuine markers at every
position and PWM bucket. Includes the exact signature that stopped the boot1
replay: peak 44, duration 42 ms, 83 ms after a genuine event at the same
marker.

- **Pass: the ghost neither advances position nor stops the run (U5).** It
  forks; the successor resolves it to phantom; tracking continues.
- Pass: the ghost is present in published evidence with its classification —
  not deleted.
- Pass: no speed reduction from a single ghost (§7.4.1).

## T11 — repeated ghosts — **AMBIGUOUS**

Families of 2–6 consecutive ghosts, at slow and low-PWM stretches.

- Pass: no position advance from the family.
- Pass: at `PENDING_DEPTH_MAX` overflow the branch list collapses to its union,
  `INCOMPLETE` is raised, confirmation authority suspends, and **no branch is
  discarded**.
- Pass: recovery once genuine markers resume, without an operator declaration.

## T12 — ambiguous DNA sequences — **AMBIGUOUS**

Cases built from P0's **measured** collision table: windows at every `W` below
`W_dir` that P0 shows to collide, and aliased sub-sequences inside otherwise
unique windows.

- Pass: no confirmation while more than one route position matches.
- Pass: a collapse to one hypothesis inside an aliased corridor does **not**
  confirm when the set exceeded `COLLAPSE_MAX_SET` since the anchor.

## T13 — route-wide reacquisition — **mixed**

From `UNLOCATED` and from `RECOVERING`-degraded-to-route-wide: clean streams
(**CLEAN**), one missed marker inside the window (**CLEAN**, U5), a ghost
inside the window (**CLEAN**, U5), and 3+ faults inside the window
(**AMBIGUOUS**).

- Pass: acquisition only on a genuinely unique sequence; silence otherwise.
- Pass: on the CLEAN sub-families the true position is acquired. On the
  AMBIGUOUS sub-family, acquiring nothing is acceptable; acquiring an aliased
  position is not.

## T14 — permitted acquisition contexts with a peer — **AMBIGUOUS**

`ACQUIRING_ORIENTED` with a peer present, swept over relative placements from
adjacent to half a circuit, in six configurations:

1. peer enlisted and moving; our seed `ACQ_ROUTE_WIDE`.
2. peer enlisted and `PEER_COMMANDED_STOPPED`, occupancy unbounded; our seed
   `ACQ_ROUTE_WIDE`.
3. **peer `PEER_IMMOBILISED ∧ PEER_BOUNDED(region)`; our seed
   `ACQ_ROUTE_WIDE`.**
4. peer `PEER_BOUNDED(region)`; our seed `ACQ_LAUNCH_REGION`, region
   **overlapping** the peer's region or within the 0033 margin of it.
5. peer `PEER_BOUNDED(region)`; our seed `ACQ_LAUNCH_REGION`, regions disjoint
   with the 0033 margin intact for every candidate pair.
6. as 5, but the peer is mobile and its bound expands by §3.12.2 during the
   authorised movement until separation can no longer be proven.

- **Pass: configurations 1, 2, 3 and 4 do not move.**
- **Configuration 3 is the correction this family now exists to enforce.** A
  bounded, immobilised peer is **not sufficient**: our own candidate set is
  route-wide, so we may already be standing inside the peer's protected region.
  Bounding the peer says nothing about where we are. A build that authorises
  movement here fails, and it must fail for the stated reason — C2 condition 1
  unsatisfied — not incidentally.
- Pass: configurations 1, 2 and 3 are refused with **the same** published
  reason class (occupancy not bounded on at least one side), not three
  different ones.
- **Pass: configuration 5 may move**, and only while all four C2 conditions
  hold continuously.
- **Pass: configuration 6 moves and then has authority withdrawn** as the
  peer's stale bound expands — smoothly, by the §7.4 calculation, not by an
  alarm or a latched hold.
- Pass (S4): every refusal publishes its reason and **no demand for a manual MM
  declaration**, and sets no latch that must later be cleared.

## T14b — peer motion information may only enlarge or invalidate — **safety**

Generated peer report streams: fresh/stale, stopped/moving, agreeing and
disagreeing with the bound, including a peer reporting a precise position it
has no authority to claim.

- **Pass: no peer report ever creates a bound.** A peer reporting "stopped at
  MM100" with no `PEER_BOUNDED(region)` narrows our authority calculation by
  exactly nothing.
- **Pass: no peer report independently grants authority.** Asserted directly: a
  build in which any combination of peer motion fields unlocks motion that the
  bounded-occupancy test refuses fails.
- **Pass: reports enlarge or invalidate.** Increasing report age monotonically
  expands the peer's conservative occupancy per §3.12.2 and monotonically
  reduces our authority, to zero when separation can no longer be proven.
- **Pass: `PEER_IMMOBILISED` holds the expansion at zero** while latched, and
  expansion resumes the moment the latch is cleared.

## T15 — occupancy representation — **safety**

Exercises the §3.12 publication rules against the on-device bitmap.

- **T15.1 disjoint candidate islands.** `H` holding two or three separated
  clusters. The published form must cover every cluster; a single arc spanning
  the gaps is acceptable (conservative), a single arc covering one cluster is a
  failure.
- **T15.2 wraparound.** Candidates straddling markers 170/0, including a set
  entirely within `{168…170, 0…2}`. The arc must wrap; a published arc computed
  as `min…max` in linear index space is a failure.
- **T15.3 two possible covering arcs.** A candidate set with a genuine tie
  between two minimal covering arcs. Must publish route-wide, not pick one.
- **T15.4 small count, wide spread.** 3–5 candidates spread over most of the
  circuit. Published occupancy must reflect the spread, not the count.
- **T15.5 compression never grants authority.** For at least 10,000 generated
  candidate sets, compute the movement authority granted from the published
  form and from the complete bitmap. **Assert `authority(published) ≤
  authority(bitmap)` in every case.** A single violation fails the suite.

## T16 — no crawl/cruise oscillation — **CLEAN**

Ordinary operation streams carrying isolated doubtful detections, isolated
ghosts and harmless Case-R redeclarations, over long runs (≥ 500 detections)
at each PWM bucket, with and without a `POSITIONED` peer.

- **Pass: zero unscheduled navigation stops (U6).**
- **Pass: a single doubtful detection produces no speed-state transition at
  all** — no reduction, no crawl, no hold, no AUTO cancellation (U4b). A
  reduction requires the binding constraint to hold for
  `SPEED_HYST_EVENTS_DOWN` consecutive detections (§7.4.1).
- **Pass: entering `RECOVERING` on its own changes no speed.** `nav_state` and
  `movement_state` transition independently, asserted as a pair on every
  detection.
- Pass: no reduce/restore cycle repeats more than once per run; a cycling
  pattern is the CTO2 failure this family exists to prevent.
- Pass: `nav_speed_reductions`, `nav_speed_restorations` and
  `nav_unscheduled_stops` are published with their binding constraints and
  match the generator's expectation exactly.

## T17 — recovery speed is derived, not fixed — **CLEAN**

`RECOVERING` with a narrow candidate set, no peer, no armed station, far from
any protected boundary.

- **Pass: the speed ceiling permits the current authorised speed** and
  `movement_state` stays `RECOVERING_WITH_AUTHORITY`. A blanket reduction to
  `ACQ_SPEED` here is a **failure** — it is the withdrawn fixed-PWM-60 rule.
- Pass: as the set widens or a peer approaches, the ceiling falls smoothly and
  is attributable to a named binding constraint (separation, station final
  entry, or protected region).
- Pass: on confirmation the previously authorised speed is restored
  automatically, with no operator GO recorded.

## T18 — station lookahead after acquisition — **CLEAN**

Acquisition or recovery completing at every marker, with each of the four
stations as the intended stop.

- Pass: where the intended station is ≥ `STATION_LOOKAHEAD_MARKERS` ahead, the
  approach arms normally.
- Pass: where it is closer, that stop is **not attempted**; the following
  permitted station is targeted and the substitution published with its reason.
- Pass: no case arms an approach from inside its braking distance.
- **Pass, launch-region cases specifically:** acquiring CW from the upper end
  of MM030–MM055 finds Grillers (MM063) fewer than 12 markers ahead and **must
  skip it**, targeting Arches (MM107); acquiring CW from MM030 finds Grillers
  33 markers ahead and uses it. CCW cases use Patio (MM015) as the reference.
  These are the same first-station-clear references the §7.7 launch procedure
  uses.

## T19 — operator role boundaries — **safety**

- Pass: a declaration issued while the navigator observes motion is **rejected
  with a reason**, not silently accepted (§7.5).
- Pass: entering `POSITIONED` from `ACQUIRING_ORIENTED` or `RECOVERING`
  **while still moving** requires no GO, and none is recorded.
- Pass: a GO is required after — and only after — an actual stop, fleet hold,
  contradiction, or explicit AUTO cancellation.
- Pass: every unscheduled stop latches the complete §7.6 snapshot and none
  publishes a demand for a manual MM declaration.
- Pass: clearing an E-stop does not force leaving AUTO.

## T20 — two-locomotive safety while one position is uncertain — **safety**

Two simulated locomotives, one `POSITIONED` and one in each of
`ACQUIRING_ORIENTED`, `RECOVERING` and `UNLOCATED`, swept over separations from
adjacent to half a circuit, and over the T15 candidate-set shapes.

- Pass (S3): the worst-case-pair test never authorises motion that could bring
  the two within the 0033 bubble under any member of either candidate set.
- Pass: an `INCOMPLETE` or route-wide occupancy produces a fleet stop.
- Pass: the uncertain locomotive yields; a `POSITIONED` peer keeps authority up
  to the separation bound.
- Pass (S4): where motion cannot be authorised, both stop and neither publishes
  a demand for a manual MM declaration.

## T21 — manual operation without a declared position — **CLEAN**

Mode 3 (§4.0, §4.4.3): `UNLOCATED`, operator drives manually, clean stream,
swept over all 342 starts.

- **Pass: manual throttle moves the locomotive.** The navigator neither
  commands motion nor blocks it. A build that refuses manual throttle because
  position is unknown fails.
- **Pass: no AUTO and no station behaviour is activated**, and neither is
  *inhibited* — asserted structurally: no flag suppresses the station machine,
  no latch is set, and nothing needs releasing. The capability is absent
  because its positional input is absent (U4c).
- **Pass: the navigator keeps observing and self-acquires** at `W_both` on a
  clean stream, then enters `POSITIONED` and **publishes the position without
  starting AUTO**. AUTO begins only if the operator had requested it.
- Pass: STOP and E-STOP remain fully effective throughout; clearing an E-stop
  does not force leaving AUTO.
- Pass: `separation_claimable=false` is published with its reason for the whole
  time position is unknown, and no separation claim is made.
- **Pass: no `HOLD` state exists in any published telemetry** for this
  condition. `MANUAL_NO_POSITION` is descriptive and latches nothing.

## T22 — sequential two-locomotive launch — **CLEAN**

The §7.7 procedure simulated end to end: both consists placed within
MM030–MM055, both in mode 2, leading locomotive started manually, trailing
started after the leader clears Grillers (CW) or Patio (CCW). Swept over
placement pairs within the region and both orientations.

- **Pass: no `LAUNCH_HOLD` state, no launch-order command and no automated
  sequencing appears anywhere.** Asserted structurally, not behaviourally: a
  build that adds one fails even if its behaviour is otherwise correct.
- **Pass: the trailing locomotive is stationary for exactly one reason — no
  movement command was issued to it.** No latch, no order, no navigator-imposed
  restraint.
- **Pass: zero unscheduled navigation stops across the whole procedure** (U6).
- Pass: once both are moving with bounded occupancies, C2 is evaluated
  continuously and authority reduces if the bounds converge.
- Pass: both locomotives acquire and enter `POSITIONED` within `W_dir`
  observations, and each applies §7.3 to its own first station.

## T23 — STOP/HOLD posture — **safety**

A structural audit family, run against the built navigator rather than a
generated stream.

- **Pass: `STOPPED_FOR_NAVIGATION_SAFETY` is the only navigation-commanded
  motion order.** Enumerate every code path that can command a speed change or
  set a latch; any additional hold-like state fails the suite.
- **Pass: no stop is commanded while any of the four §7.8 alternatives remains
  available** — bounded authority, pending evidence, conservative speed, or
  manual operation. Each generated stop is checked against the four.
- Pass: no `HOLD` state exists to represent an unavailable capability —
  absent AUTO, absent station approach, absent position, absent peer bound.
- Pass: every unscheduled stop latches the §7.6 snapshot, demands no MM
  declaration, and requires a deliberate operator restart.

## Stop classification

Every unscheduled stop in every family is classified by the harness into
exactly one of three, and the counts are reported per family:

1. **Safe stop under genuinely unresolvable evidence** — acceptable, expected
   in AMBIGUOUS families.
2. **Unnecessary stop caused by a model defect** — the evidence admitted a
   unique resolution the navigator failed to reach. **Any occurrence in a CLEAN
   family fails the suite.** Normal launch (T22), launch-region acquisition
   (T1a) and recoverable isolated faults (T7, T10) must produce **zero** stops
   of any classification.
3. **No stop; ordinary recovery.** The expected outcome of every CLEAN family.

## Regression, explicitly not acceptance

Replays of existing Otto and Toby sessions run and are reported alongside, for
three purposes only: that a full session processes without crashing, that
behaviour is explicable event by event, and that the operational cost of
Strategy A is measured on a session containing real discontinuities. Under
Strategy A the eight Otto boot16 route-wide recoveries are expected **not** to
reappear as confirmations. A replay producing more confirmations than the
frozen navigator is a reason for suspicion, not a result.
