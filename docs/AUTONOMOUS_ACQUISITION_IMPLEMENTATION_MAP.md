# Implementation map — QUORUM component disposition

Companion to `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`. Status: Proposed.
Corrected 2026-08-22, passes 2 and 3. Pass 3 (operator rulings 6fba58c) adds
the launch-region seed, the protected-region declaration, the peer-motion
expansion rule, and the explicit absence of any `LAUNCH_HOLD`. Pass 2: the
timing source
and evidence-ring fields (§3.6 branch-local elapsed time), the occupancy
publication, and the separation of navigation state from speed state.
Authorises no firmware change. Line references are to
`firmware/QUORUM/QUORUM.ino` at `QUORUM_1_16R_IR_TEST_A`, and to
`tools/navlab/reachability_nav.py` at 75fa0ee.

Three dispositions: **RETAIN** (unchanged, or extended additively),
**REPLACE** (the function survives, the mechanism does not), **REMOVE** (the
function itself goes away).

## Firmware — detection and evidence

| component | disposition | note |
|---|---|---|
| Hall front end, entry threshold, baseline median (`strengthPct`, decision 0040) | **RETAIN** | the only detection gate; unchanged |
| polarity determination, peak, duration measurement | **RETAIN** | now also feed §3.7 classification |
| `dt` measurement between **accepted** events | **REMOVE** | acceptance state must not determine physical elapsed time (P4). Replaced by branch-local elapsed |
| — | **NEW** | 64-bit extended monotonic `t_detect` on every **raw** detection, plus a `clock_epoch` counter incremented at boot. This is the sole time source; `elapsed(e, b) = e.t_detect − b.last_genuine`, or UNKNOWN on epoch mismatch (spec §3.6, §3.7) |
| `pwm_actual` / `commandedPwm` history | **RETAIN** | consumed by §3.5 propagation |
| `RingEntry {polarity, navMm}` evidence ring | **REPLACE** | ring retained; the stored `navMm` label is dropped and the entry becomes `{t_detect, clock_epoch, polarity, peak, duration_ms, pwm_actual}`. **No `dt` field** — elapsed time is a property of an (event, branch) pair, not of an event, which is what removes the double-count. Storing a position label in the evidence is what let firmware belief re-enter as evidence |
| `irObs*` IR TEST A observers | **RETAIN** | observation only; reset hooks re-pointed at the new state transitions |

## Firmware — navigator state

| component | disposition | note |
|---|---|---|
| `NAV_NORMAL` | **RETAIN** as `POSITIONED` | |
| `NAV_EVALUATING` | **REMOVE** | it exists only to run the offset vote |
| `NAV_NO_QUORUM` | **REPLACE** | split into `RECOVERING` and `UNLOCATED`, which differ in whether an anchor survives |
| — | **NEW** | `ACQUIRING_ORIENTED`; no equivalent exists today |
| `navMm` scalar | **REPLACE** | 342-bit hypothesis bitmap plus a derived `mm` published only when `|H| = 1` |
| `lastConfirmedMm` / `lastConfirmedMs` / `haveConfirmed` | **RETAIN** | become the anchor of §3.3, gaining a provenance field |
| `navDeclare()` | **RETAIN**, narrowed | remains the authoritative operator path (`cmd/start_mm`, `cmd/start_interval`). Its reuse for internal re-anchoring is **REMOVED** (P7) |
| `applyDirection()`, `motorDirection`/`sessionDir`/`navDir` | **RETAIN** | effective direction derivation is sound and is carried over, including native reversal handling |

## Firmware — the QUORUM recovery machinery

| component | disposition | note |
|---|---|---|
| `QUORUM_OFFSETS`, `QUORUM_CANDIDATES`, symmetric/asymmetric variants | **REMOVE** | fixed offsets are not a model of reachable position |
| `scores[]`, `leaderIdx`, `runnerUpIdx`, `quorumMargin`, `QUORUM_MARGIN` | **REMOVE** | voting over offsets is replaced by set propagation |
| `QUORUM_MAX = 12` evaluation budget, `QUORUM_TRIGGER = 3` miss wake | **REMOVE** | a budget can expire with the true offset still viable |
| `candidateExcluded[]` | **REMOVE** | |
| offset adoption rewind (`navMm -= navDir*failed`, ring rewrite) | **REMOVE** | rewriting the evidence ring's labels to match an adoption is label slip by construction |
| `PHANTOM_REJECTED` irreversible rejection | **REPLACE** | becomes a pending branch (§3.8); the event is held, never deleted |
| `§3Q` quarantine hold (`qPending`, `qHist`, `Q_FLOOR_MS`, conjunction test) | **REPLACE** | the *hold* is retained and generalised into the branch list; the discard-on-tie primary verdict is removed — `AMBIGUOUS_DEFAULT_PHANTOM` becomes "keep both branches". Each branch carries its own `last_genuine` timestamp; the phantom branch does not advance it |
| `qPendingDt` fold-into-successor | **REMOVE** | folding a held event's dt into its successor double-counts elapsed time against the branch that already measured from the earlier origin (spec §3.6) |
| conservation timing gate (this `dt` vs previous `dt`) | **REPLACE** | empirical PWM/section/direction envelopes (§3.6) |
| `dnaMatch()` fenced to `±REACQ_WINDOW_MARKERS` | **REMOVE** | the fence is arbitrary; §3.5 reachability is the correct constraint |
| `dnaMatchWide()` route-wide unique window | **RETAIN**, corrected | kept as the uniqueness primitive. Corrections: it must not run on an `INCOMPLETE` set, must refuse a window containing an admitted missed marker, must intersect its result with `H` rather than stand alone, and in `UNLOCATED` must be unique across both direction planes |
| `nqLearn()` / `nqCandidate` / `nqConfirm` / `NQ_CONFIRM_N` self-resolution | **REPLACE** | the "never stop learning" behaviour is retained; the three-consecutive-advance confirmation is replaced by §3.9, which additionally requires completeness and unsuspended authority |
| `quorumAdvisoryMarker()` HARD_BOUND advisory (decision 0023) | **REMOVE** | acquisition becomes first-class, so an advisory-that-never-acts has no remaining role |
| `dnaPush()` / `dnaBuf` streaming remnant | **REMOVE** | already dead by instruction |
| `nqDropAutoInterlock()` | **RETAIN**, narrowed | applied only when the locomotive **actually stopped**, entered fleet hold, hit a contradiction or explicitly cancelled AUTO. A single doubtful detection triggers none of these, and AUTO begins after acquisition only if the operator had requested it. It is **not** applied to a recovery that never stopped: re-entering `POSITIONED` while still moving automatically restores the previously authorised speed with no operator GO (spec §7.2, §7.4, §7.5) |
| NO_QUORUM snapshot / retained-state machinery (`desiredRetainedNoQuorum`, generations) | **RETAIN** | forensic record, re-pointed at the new entry reasons |

## Firmware — motion, stations, fleet

| component | disposition | note |
|---|---|---|
| PWM ramps, `STATION_UP_STEP_MS` / `STATION_DOWN_STEP_MS`, brake enforcement | **RETAIN** | unchanged |
| station table, `effStopOffset()`, phase machine | **RETAIN** | unchanged |
| station **arming** gate | **REPLACE** | available only in `POSITIONED`, and **unavailable rather than inhibited** elsewhere — an approach needs an exact distance-to-centre, which does not exist while `|H| > 1`, so nothing suppresses the station machine and nothing is released later; `ST_FINAL` completes on entry to `RECOVERING`, earlier phases abandon (§4.3). On entering `POSITIONED` from acquisition or recovery, an intended station closer than `STATION_LOOKAHEAD_MARKERS` is skipped in favour of the next permitted one (§7.3) |
| requested-PWM authority while navigating | **REPLACE** | `nav_state` and `movement_state` become distinct (P13). Entering `RECOVERING` commands no speed change; the ceiling is derived from the worst case over all viable candidates and the peer's occupancy (§7.4), not fixed at PWM 60. Hysteresis (`SPEED_HYST_EVENTS_DOWN`/`_UP`, `SPEED_STEP_MIN_PCT`) prevents CTO2-style crawl/cruise cycling, and reductions/restorations/stops are counted and published |
| CTO2/CTO3 transport, roles, echo, membership (0031/0032/0034/0037/0039) | **RETAIN** | |
| CTO payload position field | **REPLACE** | single `mm` becomes the conservative occupancy of §3.12: up to `OCC_ARCS_MAX` wrapping arcs if the contract can carry them, otherwise one covering arc, with a tie between minimal covering arcs or an over-long arc published as route-wide. Disjoint islands and 170/0 wraparound are ordinary cases, not edge cases |
| separation computation (0033: bubble + six markers) | **REPLACE** | evaluated on-device over the **complete bitmap** — every candidate-position pair, or a demonstrably conservative equivalent — never over a minimal-looking arc that excludes a viable hypothesis. `INCOMPLETE` or route-wide occupancy forces fleet stop. Published compression may never grant more authority than the bitmap (test T15.5) |
| `ctoFleetHold` | **RETAIN** | now also raised by an `UNLOCATED` peer, or by any peer whose occupancy is unbounded |
| peer motion/stopped flags as an authority input | **REPLACE** | motion state is not a safety property and may not create a bound or grant authority. `PEER_COMMANDED_STOPPED` becomes telemetry only. Peer speed, direction, authority and **report age** are retained for one purpose: they **enlarge or invalidate** a bound (spec §3.12.2), expanding the peer's conservative occupancy at its own envelope fast bound between valid reports, so a silent peer degrades our authority smoothly. `PEER_IMMOBILISED` holds that expansion at zero while latched |
| — | **NEW** | protected-region declaration: a configured or operator-supplied region bound, for the peer (`PEER_BOUNDED`) and for ourselves when outside the launch region. **No mechanism exists in firmware today**, and §4.2 context C2 is unreachable without it — C1 (alone) would be the only usable acquisition context. Listed as open operator decision 4 |
| — | **NEW** | launch-region seed: `H` = MM036–MM045 in the declared direction, selected explicitly by the operator and **never defaulted** (spec §4.0). This is the bound that makes C2 reachable for the acquiring locomotive |
| `LAUNCH_HOLD` or any launch-ordering state | **NOT ADDED** | recorded here as a deliberate non-component. Sequential launch is operator-supervised (spec §7.7); the trailing locomotive is stationary because no command was issued. Adding a latch would create an ordering to enforce and a failure mode when it is wrong |
| MQTT topic/field contract | **RETAIN** | additive fields only (§7), and additions are gated on operator approval |

## Host model — `tools/navlab`

| component | disposition | note |
|---|---|---|
| `normalize_log.py` | **RETAIN** | record schema extended with peak/duration already present; drop firmware label from navigator input |
| `build_timing_db.py` | **REPLACE** | `fast_bound = min × (1 − margin)` → robust quantile with fast-end contamination filters and the sanity ratio (§3.6) |
| `Envelopes` tier fallback (t1→t2→t3, `MIN_N = 8`) | **RETAIN** | the loosest-adequately-populated-tier rule is sound |
| `Navigator.anchor` + monotonic `hi` corridor | **REPLACE** | per-event local propagation; the corridor that grew 4–6× faster than the locomotive goes away with it |
| `Navigator.P` (Python set) | **REPLACE** | bitmap over (marker, direction), mirroring the firmware structure so host and device agree bit for bit; the branch list carries one bitmap plus one `last_genuine` timestamp per branch |
| `normalize_log.py` `dt_ms` output | **REPLACE** | emit `t_detect` and `clock_epoch` per raw detection instead of a precomputed `dt`, so a host replay derives identical branch-local elapsed values to the device |
| `maybe_reverse()` native reversal handling | **RETAIN** | correct and carried over; hypotheses preserved, direction of travel reversed |
| dt=0 one-interval grant (`DT_RESET` branch) | **REMOVE** | the demonstrated defect. Replaced by the three-way split of §6: Case D (declaration, authoritative), Case R (internal redeclaration — **not a timing event** under branch-local elapsed, so most former dt=0 exposure disappears), Case I (genuine discontinuity → Strategy A) |
| `pending` / `PENDING_MARGINAL` / `MARGINAL_SLACK` | **RETAIN**, generalised | extended to the branch list of §3.10, each branch carrying its own `last_genuine`. The dt-folding is removed, not carried over |
| `PHANTOM_SUSPECT` positional test (`hi + 30 < spacing`) | **REPLACE** | amplitude/duration classification (§3.7) plus reachability; the positional-only test is what let a peak-44 / 42 ms event stop the boot1 replay |
| `CONTRADICTION` terminal stop | **REPLACE** | stop, then re-seed to `UNLOCATED` and retain the snapshot (§3.10) — recoverable instead of terminal |
| `LOST_FULL_CIRCLE` + 12-window + `CONFIRM_N` reacquisition | **REPLACE** | §3.9 uniqueness with the completeness gate and the missed-marker refusal |
| `acceptance_checks.py` evidence-`basis` machinery | **RETAIN** | it is the checker for the replacement too (decision 0042) |
| `probe_dt0_unknown_time.py` | **RETAIN** | becomes acceptance test T5/T6 (see the test plan) |

## Sequencing

0. The blocking map-uniqueness prerequisite (test P0): compute `W_dir` and
   `W_both` over the committed map at every rotation and both directions.
   Nothing downstream is implementable until those values exist.
1. Host model, entirely off-locomotive: bitmap propagation, branch-local
   timing, robust envelopes, branch list, §3.11 confirmation, conservative
   occupancy.
2. The acceptance tests of `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`
   written **before** the navigator they test, against generated cases with
   known ground truth.
3. Only after **both** the safety gates and the usefulness gates pass, and
   after every engineering parameter in specification §10 carries calibration
   evidence, is a candidate frozen and an untouched capture spent on it per
   `docs/AUTONOMOUS_ACQUISITION_VALIDATION_PROTOCOL.md`.
4. Firmware implementation is a separate, separately approved work item. This
   map does not authorise it.

**Two prerequisites are operator/configuration work, not navigator work**, and
both gate how much of §4.2 is reachable: the protected-region declaration
mechanism, and the two new operator commands (declare orientation without
position; select launch-region startup). With neither in place, the design
still runs — mode 1 exact declaration and mode 3 manual operation are
unaffected — but acquisition alongside a peer is limited to C1, alone.
