# 0035 — Quarantine the doubtful, and never stop learning

Status: **Proposed** (2026-08-14). Implemented as `QUORUM_1_16`, proven in the
harness against every capture and an adversarial set, **NOT flashed**.
Supersedes 0023 in part and 0024 in part; 0025's containment prohibition is
addressed, not violated — see Consequences.

## Decision

Three changes to the navigator, from the operator's rulings of 2026-08-14
(*"we are deliberately making the locos ignore what they know"*; *"the record
should come from the next magnet reading with good credentials"*) and CODEX's
seven-point diagnosis of why one phantom poisons QUORUM:

1. **Quarantine.** An event with doubtful credentials is HELD, not committed.
   The decisive test is the **350 ms physical floor** — over the route's
   shortest spacing (280 mm), an event arriving sooner than 350 ms after its
   predecessor requires more than twice the fleet's demonstrated top speed
   (441 mm/s p99.9 from 38,671 clean marker intervals; 402 mm/s from 17,584
   independent IR readings; the two agree within 10% and neither involves the
   PWM model). Above the floor, a corroborating conjunction — opposite pole
   AND interval < 0.40× trailing median AND (flux < 0.55× median OR duration
   > 3.5× median), all against ACCEPTED events, gated to the steady-running
   band it was calibrated in — catches the slow phantom families the floor
   cannot (the 17:21 crawl doubles at 420–660 ms, two of which were STRONG).
   The NEXT event arbitrates: its polarity is tested against the map under
   both hypotheses. If it vouches for the pending event, the event is
   **committed** — discard is reversible, per CODEX. Otherwise discarded, and
   its interval folds into the successor's so the timing chain still measures
   physical travel.

2. **The insertion hypothesis at the hard bound** (CODEX weakness #4). QUORUM
   scores constant offsets; a phantom is an *insertion* that changes the
   correct offset mid-window, and a straddled window fits no candidate — the
   17:21 board `[8,8,8,4,7,3]` is that shape. At the hard bound, before going
   terminal, the newest 7 ring entries are tested against each candidate: if
   exactly one fits them all, it is adopted (`QUORUM_SUFFIX_RESCUE`). Two or
   none → terminal exactly as before.

3. **NO_QUORUM keeps learning.** Scoring no longer freezes at the terminal.
   Once 12 fresh events have aged the corrupted ring out, every further event
   runs a **route-wide** unique window match (not the ±5 fenced advisory —
   the 2026-08-14 incident sat 32 markers outside the fence). A unique match
   that then advances consistently for 3 further events relabels the
   navigator and returns it to NORMAL (`SELF_RESOLVED`). **Motor policy is
   untouched: AUTO stays dropped.** Knowledge recovery is not motion
   recovery.

## The evidence

**Stateful replay, real firmware, both complete captures** (the bar 0024 set
and never met). Ten of twelve boot-session segments verified at perfect
fidelity against the old build first — zero odometer divergence, decisions
identical — then diffed old-vs-new. Every changed outcome enumerated in
`docs/QUORUM_1_16_STATEFUL_DIFF_ENUMERATION.txt`. Summary:

| segment | change |
|---|---|
| toby_0814_s03 (the 17:21 incident) | **terminal NO_QUORUM eliminated**; 4 crawl doubles discarded; agree +60 |
| otto_0813_s01 | 2 full evaluation cycles avoided; disagree 14→8 |
| otto_0814_s03 | 4 phantoms handled earlier/cleaner; disagree 17→12 |
| toby_0814_s01 | phantom never admitted; final mm now correct; disagree 5→0 |
| otto_0813_s02/s03 | clean 1:1 swaps, late PHANTOM_REJECTED → early quarantine |
| four clean segments | **byte-identical** — the no-touch proof |

Totals: 5,644 markers, 14 held, 14 discarded, 0 committed, **zero adverse
changes**.

**The 2026-08-10 capture** (the worst session on record): incidents A and B
never reach a terminal; the defective mm 66–82 stretch exercises the
**reversibility path on real data** (2 quarantined genuine events committed
back by their successors); incident C — polarity corruption, invisible to
quarantine *by design* — still fires with its terminal board byte-identical
to the legacy pin and advisory 18 intact, and is then healed by
`SELF_RESOLVED`, old 13 → new 8: **the same −5 the advisory diagnosed.** The
session ends NORMAL where the locomotive used to end it stranded.

**Input-invariance** (the strengthened counterfactual): dropping the two
Bamboo phantom events from the input changes nothing about where the
locomotive ends up — final mm 8, NORMAL, both ways.

**Adversarial set**, all passing: maximum genuine acceleration (nothing
quarantined; the legacy PWM-model gate still costs one marker there — 0024's
defect, pinned honestly, untouched); missed marker then acceleration (+1
adopted, no quarantine); the dt-fold; two consecutive phantoms; **the
reinstatement proof** (a genuine event that trips the conjunction is
committed back by its successor); reversal mid-quarantine; forced terminal
healed end-to-end by self-resolution.

## Alternatives considered

**Thresholds by feel.** Rejected; every number is derived: the floor from two
independent speed sensors and `min(spacingMm)`, the ratios from labelled
distributions over 39,103 markers where the threshold sweep moved the outcome
by 8% across a 67% parameter range — the signature of thresholds sitting in a
gap, not on a knife edge.

**A PWM-based plausibility test.** Rejected by decision 0024's own evidence;
the 17:21 failure was the PWM model wrong by ~4× under load, again.

**Irreversible discard.** Rejected on CODEX's requirement: the successor
arbitrates, a wrongly held genuine event is committed back (proven on the
2026-08-10 capture, twice), and a wrongly discarded one costs offset −1 —
inside the fence, adopted routinely. The asymmetry favours rejection; the
recovery makes it safe.

**Route-wide search during EVALUATING.** Rejected — that is the ranking prior
0023 refused, and it stays refused. Wide search runs only in NO_QUORUM, where
the position is formally discredited and the fence protects nothing.

## Consequences

- **0023 superseded in part**: "diagnostic — it never moves the locomotive"
  no longer holds in NAV_NO_QUORUM, where the same exact-or-silent machinery
  (route-wide, uniqueness-gated, confirmation-gated) may now relabel the
  navigator. Everywhere else 0023 stands, including the ±5 fence and the
  HARD_BOUND-only advisory scope — both re-verified by the suite.
- **0024 superseded in part**: its replacement direction (measured intervals,
  not modelled) finally lands, as quarantine's trailing medians. Its analysis
  stands; its PWM-model conservation gate remains in place unchanged and
  still carries its documented defect (`syn_adv_accel_max` pins the cost).
  Removing it is separate work.
- **0025 addressed, not violated**: that record forbade containment while the
  *source* was unfixed, lest it mask a maintenance fault. The sources it
  concerned are fixed; what remains is containment of *sparse residual*
  phantoms, and every quarantine verdict is published with full credentials —
  the dig signal is louder, not suppressed.
- **Identity errors remain out of scope**, exactly as 0024 predicted for any
  order-preserving rule: `syn_pair_weak_then_strong` asserts the limitation
  so nobody mistakes 1.16 for a fix it is not. The recovery for that class is
  self-resolution, as incident C demonstrates.
- The suite is now **era-aware**: legacy-mode assertions preserved intact
  against pre-1.16 binaries; 1.16 goldens pinned for the quarantine era.
  Every future drift from the pinned numbers is a behaviour change owing
  review.
- **Not flashed.** Gate: operator + CODEX review of this record and
  `docs/QUORUM_1_16_IMPLEMENTATION_REPORT.md`, then supervised track time.
  1.15 remains reserved for the CTO mode expansion; the stale transport
  reservation moves to 1.17.

## References

- `docs/QUORUM_QUARANTINE_AND_SELF_RESOLUTION_PROPOSAL.md` — the proposal and derivations
- `docs/QUORUM_1_16_IMPLEMENTATION_REPORT.md` — implementation and verification detail
- `docs/QUORUM_1_16_STATEFUL_DIFF_ENUMERATION.txt` — every changed outcome
- CODEX seven-point diagnosis, 2026-08-14 (operator-relayed, in session record)
- Decisions 0023, 0024, 0025
