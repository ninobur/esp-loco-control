# Implementation map — QUORUM component disposition

Companion to `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`. Status: Proposed.
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
| `dt` measurement between accepted events | **RETAIN** | now the sole time source (P4) |
| `pwm_actual` / `commandedPwm` history | **RETAIN** | consumed by §3.5 propagation |
| `RingEntry {polarity, navMm}` evidence ring | **REPLACE** | ring retained; the stored `navMm` label is dropped and replaced by `{polarity, peak, duration, dt, pwm}`. Storing a position label in the evidence is what let firmware belief re-enter as evidence |
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
| `§3Q` quarantine hold (`qPending`, `qHist`, `Q_FLOOR_MS`, conjunction test) | **REPLACE** | the *hold* is retained and generalised; the discard-on-tie primary verdict is removed — `AMBIGUOUS_DEFAULT_PHANTOM` becomes "keep both branches" |
| conservation timing gate (this `dt` vs previous `dt`) | **REPLACE** | empirical PWM/section/direction envelopes (§3.6) |
| `dnaMatch()` fenced to `±REACQ_WINDOW_MARKERS` | **REMOVE** | the fence is arbitrary; §3.5 reachability is the correct constraint |
| `dnaMatchWide()` route-wide unique window | **RETAIN**, corrected | kept as the uniqueness primitive. Corrections: it must not run on an `INCOMPLETE` set, must refuse a window containing an admitted missed marker, must intersect its result with `H` rather than stand alone, and in `UNLOCATED` must be unique across both direction planes |
| `nqLearn()` / `nqCandidate` / `nqConfirm` / `NQ_CONFIRM_N` self-resolution | **REPLACE** | the "never stop learning" behaviour is retained; the three-consecutive-advance confirmation is replaced by §3.9, which additionally requires completeness and unsuspended authority |
| `quorumAdvisoryMarker()` HARD_BOUND advisory (decision 0023) | **REMOVE** | acquisition becomes first-class, so an advisory-that-never-acts has no remaining role |
| `dnaPush()` / `dnaBuf` streaming remnant | **REMOVE** | already dead by instruction |
| `nqDropAutoInterlock()` | **RETAIN** | "knowledge recovery is not motion recovery" is preserved verbatim: re-entering `POSITIONED` from any recovery state drops AUTO and requires a deliberate GO |
| NO_QUORUM snapshot / retained-state machinery (`desiredRetainedNoQuorum`, generations) | **RETAIN** | forensic record, re-pointed at the new entry reasons |

## Firmware — motion, stations, fleet

| component | disposition | note |
|---|---|---|
| PWM ramps, `STATION_UP_STEP_MS` / `STATION_DOWN_STEP_MS`, brake enforcement | **RETAIN** | unchanged |
| station table, `effStopOffset()`, phase machine | **RETAIN** | unchanged |
| station **arming** gate | **REPLACE** | armed only in `POSITIONED`; `ST_FINAL` completes on entry to `RECOVERING`, earlier phases abandon (§4.3) |
| CTO2/CTO3 transport, roles, echo, membership (0031/0032/0034/0037/0039) | **RETAIN** | |
| CTO payload position field | **REPLACE** | single `mm` becomes the occupancy span of §3.11 |
| separation computation (0033: bubble + six markers) | **REPLACE** | evaluated over the worst-case pair of the two spans; `INCOMPLETE` or route-wide span forces fleet stop |
| `ctoFleetHold` | **RETAIN** | now also raised by an `UNLOCATED` peer |
| MQTT topic/field contract | **RETAIN** | additive fields only (§7), and additions are gated on operator approval |

## Host model — `tools/navlab`

| component | disposition | note |
|---|---|---|
| `normalize_log.py` | **RETAIN** | record schema extended with peak/duration already present; drop firmware label from navigator input |
| `build_timing_db.py` | **REPLACE** | `fast_bound = min × (1 − margin)` → robust quantile with fast-end contamination filters and the sanity ratio (§3.6) |
| `Envelopes` tier fallback (t1→t2→t3, `MIN_N = 8`) | **RETAIN** | the loosest-adequately-populated-tier rule is sound |
| `Navigator.anchor` + monotonic `hi` corridor | **REPLACE** | per-event local propagation; the corridor that grew 4–6× faster than the locomotive goes away with it |
| `Navigator.P` (Python set) | **REPLACE** | bitmap over (marker, direction), mirroring the firmware structure so host and device agree bit for bit |
| `maybe_reverse()` native reversal handling | **RETAIN** | correct and carried over; hypotheses preserved, direction of travel reversed |
| dt=0 one-interval grant (`DT_RESET` branch) | **REMOVE** | the demonstrated defect; replaced by §6 Case D / Case I with Strategy A |
| `pending` / `PENDING_MARGINAL` / `MARGINAL_SLACK` | **RETAIN**, generalised | extended to the branch list of §3.8 with folded dt |
| `PHANTOM_SUSPECT` positional test (`hi + 30 < spacing`) | **REPLACE** | amplitude/duration classification (§3.7) plus reachability; the positional-only test is what let a peak-44 / 42 ms event stop the boot1 replay |
| `CONTRADICTION` terminal stop | **REPLACE** | stop, then re-seed to `UNLOCATED` and retain the snapshot (§3.10) — recoverable instead of terminal |
| `LOST_FULL_CIRCLE` + 12-window + `CONFIRM_N` reacquisition | **REPLACE** | §3.9 uniqueness with the completeness gate and the missed-marker refusal |
| `acceptance_checks.py` evidence-`basis` machinery | **RETAIN** | it is the checker for the replacement too (decision 0042) |
| `probe_dt0_unknown_time.py` | **RETAIN** | becomes acceptance test T5/T6 (see the test plan) |

## Sequencing

1. Host model first, entirely off-locomotive: bitmap propagation, robust
   envelopes, branch list, §3.9 confirmation.
2. The acceptance tests of `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`
   written **before** the navigator they test, against generated cases with
   known ground truth.
3. Only after those pass is a candidate frozen and the untouched capture of
   `docs/AUTONOMOUS_ACQUISITION_VALIDATION_PROTOCOL.md` spent on it.
4. Firmware implementation is a separate, separately approved work item. This
   map does not authorise it.
