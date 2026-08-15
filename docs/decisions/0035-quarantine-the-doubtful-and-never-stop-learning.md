# 0035 — Quarantine the doubtful, and never stop learning

Status: **Proposed** (2026-08-14). Implemented as `QUORUM_1_16`, revised to
`QUORUM_1_16R` the same day after CODEX's seven-finding review (all seven
accepted — see the Review round section and
`docs/QUORUM_1_16R_REVIEW_RESPONSE.md`), proven in the harness against every
capture and an adversarial set, **NOT flashed**.
Supersedes 0023 in part and 0024 in part. The phantom-source maintenance
record's containment prohibition is addressed, not violated — see
Consequences. (That record is numbered 0025 on branch
`agent/phantom-verdict-20260812` and collides with this branch's 0025, the
console-roster record; renumbering is queued for the branch consolidation.
CODEX finding 7 caught the ambiguous citation.)

## Decision

Three changes to the navigator, from the operator's rulings of 2026-08-14
(*"we are deliberately making the locos ignore what they know"*; *"the record
should come from the next magnet reading with good credentials"*) and CODEX's
seven-point diagnosis of why one phantom poisons QUORUM:

1. **Quarantine.** An event with doubtful credentials is HELD, not committed.
   The decisive test is the **350 ms physical floor** — over the route's
   shortest spacing (280 mm), an event arriving sooner than 350 ms after its
   predecessor requires 800 mm/s: **1.81x** the fleet's demonstrated top
   speed (441 mm/s p99.9 from 38,671 clean marker intervals; 402 mm/s from
   17,584 independent IR readings; the two agree within 10% and neither
   involves the PWM model). The 2x safety factor was applied on the way IN —
   441/2 over 280 mm gives 317 ms, rounded up to 350 — so the margin the
   floor actually enforces is 1.81x, and this record says so rather than
   rounding the rhetoric up (CODEX finding 7). Above the floor, a corroborating conjunction — opposite pole
   AND interval < 0.40× trailing median AND (flux < 0.55× median OR duration
   > 3.5× median), all against ACCEPTED events, gated to the steady-running
   band it was calibrated in (600–4000 ms trailing-median interval; the
   original PWM ≥ 40 condition was removed on CODEX finding 5 — PWM is a
   request, not a measurement, which is this record's own §Alternatives
   argument turned back on it) — catches the slow phantom families the floor
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
   the 2026-08-14 incident sat 32 markers outside the fence). A unique match that
   holds across **three consistent matches in all** — the match plus two
   further advancing confirmations — relabels the navigator and returns it
   to NORMAL (`SELF_RESOLVED`). (CODEX finding 6: this record originally
   said "3 further", which the code never did. The CODE stands and the
   WORDS were corrected: each match is a route-wide-unique 12-window
   re-verified under coherent advance, and on the 2026-08-10 capture the
   stricter counting would have pushed the only real-data recovery — which
   lands on the session's final event, fresh = 17 — past the end of the
   record.) **Motor policy is
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
| toby_0813_s02 (since 1.16R) | 2 crawl events at PWM 24/19 with 20 s durations quarantined — finding 5's exemption removed; disagree 24→4 |
| three clean segments | **byte-identical** — the no-touch proof |

Totals (recomputed from the enumeration file itself for 1.16R): **5,544
markers, 17 held, 17 discarded, 0 committed, zero adverse changes** — every
segment's final position matches the legacy binary except toby_0814_s01,
where the corrected count is the change. This record's first draft said
"5,644 markers, 14 held" — numbers that did not match its own cited
enumeration (5,544 / 15 pre-review); they are corrected here against the
file, in the same spirit as CODEX finding 7's fixture miscount.

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
healed end-to-end by self-resolution. The review round added seven more —
the suffix-rescue set, the vouched-commit gate proof, the slow phantom
family, and the resume interlock — bringing the suite to **30 synthetic
fixtures** (the first draft of the implementation report said 24 of 23;
CODEX finding 7 caught the miscount too).

## The review round (2026-08-14, CODEX; all seven findings accepted)

Full point-by-point response: `docs/QUORUM_1_16R_REVIEW_RESPONSE.md`.
What matters at decision level:

1. **The resume interlock is now enforced.** `enterNoQuorum()` never drops
   `autoRunning` (field evidence: 364 NO_QUORUM alerts with auto 1 in
   `field-records/logs/20260811_QUORUM_1_13_beta_otto.log`), so "AUTO stays dropped" was a promise with no mechanism —
   `serviceStations()` would have re-requested cruise on the next loop pass
   after SELF_RESOLVED. Self-resolution now drops AUTO itself, publishes
   the warning, and resuming requires a deliberate BEGIN. The harness never
   calls `serviceStations()`, which is why no fixture could catch this;
   the dump now reports the flag and a fixture pins it.
2. **A vouched commit is exempt from the legacy conservation gate.** CODEX
   predicted commit-then-PHANTOM_REJECTED from the armchair; the 2026-08-10
   capture confirms it was LIVE at mm 85. The map's authentication now
   outranks the PWM model. Consequence, pinned in the goldens: the mm 85
   commit lands, its genuine successor is then eaten by the same legacy
   gate (0024's defect, biting the other event of the pair — count
   identical, ring polarity different), and the second pass through the
   defective stretch now ends in an honest HARD_BOUND terminal at mm 87,
   cleared four events later by the operator's declare already present in
   the record. Same session destination: NORMAL at mm 8, SELF_RESOLVED
   13 → 8.
3. **The witness is credential-checked before it may testify**
   (`SUCCESSOR_SUSPECT`), which is also how slow consecutive phantom
   families fold up — each unfit witness discards its elder and is held in
   turn.
4. **The conjunction lost its PWM condition** (finding 5), and the capture
   paid out immediately: two toby_0813_s02 events at PWM 24 and 19 with
   20-second durations — exempt from scrutiny under the old `pwm >= 40`
   test — are now quarantined and discarded; that segment's disagreements
   fall 24 → 4 with the same final position.
5. **Suffix-rescue ambiguity is impossible on NGR_DNA1**, proven in the
   suite rather than estimated: the DNA's longest self-agreement run at
   candidate lags 1–5 is 6, so no two candidates can share a clean
   7-suffix, and `SUFFIX_RESCUE_N = 7` is the minimum length with that
   guarantee. The 1/128 probability argument is retired. An excluded
   candidate is refused even on a perfect 12-entry suffix (fixture-pinned);
   recovery from that corner belongs to self-resolution, which starts from
   fresh evidence and no exclusions.

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
2026-08-10 capture — see the Review round for what the second commit
revealed), and a wrongly discarded one leaves a genuine advance unrecorded:
the odometer runs BEHIND, the true position is one marker AHEAD, and the
cost is offset **+1** in the direction of travel — inside the fence, adopted
routinely (the sign was stated backwards in this record's first draft;
CODEX finding 7). The asymmetry favours rejection; the recovery makes it
safe.

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
- **The phantom-source maintenance record addressed, not violated** (it is
  0025 on `agent/phantom-verdict-20260812`; this branch's 0025 is the
  console-roster record — renumbering queued): that record forbade
  containment while the *source* was unfixed, lest it mask a maintenance
  fault. The sources it concerned are fixed; what remains is containment of
  *sparse residual* phantoms, and every quarantine verdict is published with
  full credentials — the dig signal is louder, not suppressed.
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
- `docs/QUORUM_1_16R_REVIEW_RESPONSE.md` — the CODEX review round, point by point
- `docs/QUORUM_1_16R_STATEFUL_DIFF_ENUMERATION.txt` — the re-run enumeration after the review fixes
- CODEX seven-point diagnosis and seven-finding review, 2026-08-14 (operator-relayed, in session record)
- Decisions 0023, 0024; the phantom-source maintenance record (0025 on `agent/phantom-verdict-20260812`)
