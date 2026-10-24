# NAVI conference — "The Power of Normal," governing record

**Status: all ten propositions closed by consensus (David, Sam/Codex,
Claude). This is the architectural authority for implementation fidelity
review and the historical replay. It is not an implementation and changes
no firmware.** Numerical thresholds named below are explicitly open research
questions, not settled values.

This record supersedes informal conference discussion as the durable
reference. It builds on, and does not revise, the prior **Ten Principles of
NAVI** record and the Proposition 1-5 discussion that preceded this
prospectus, except where Proposition 7 explicitly amends Proposition 6A's
shape requirement (noted below).

## 1. The agreed navigation cycle

1. **Expect** — the map (sequence, polarity, spacing) and recent movement
   history identify exactly one legitimate next magnet, at a predicted
   distance and arrival time. There is no second identity to consider.
2. **Observe** — Hall values are recorded continuously and unconditionally,
   independent of navigation judgment.
3. **Qualify** — a candidate magnetic encounter, sentineled by the 70x2
   onset marker, is evaluated **retrospectively, after the Hall signal
   itself establishes completion** (observed return to baseline), against:
   correct opening polarity, a minimum physically plausible width
   (necessary, not sufficient), and observed completion. General waveform
   shape (including Gaussian goodness-of-fit) is retained as diagnostic
   evidence only; it does not veto an otherwise-qualified encounter
   (Proposition 7, amending Proposition 6A).
4. **Confirm and advance, or continue waiting** — a qualifying encounter at
   the expected position confirms the target and advances the sequence. A
   nonqualifying observation (wrong polarity, insufficient width, etc.) is
   not itself a failure; NAVI does not search alternative identities or
   attempt geographic recovery. It simply continues observing for the same
   expected target.
5. **Stop-and-hold** — only when the empirically validated latest
   permissible arrival for that specific mapped interval is exceeded without
   confirmation does NAVI stop commanded movement, retain the last confirmed
   position, and continue recording Hall observations. This is a
   motor-authority action; Hall observation continues underneath it. NAVI
   does not diagnose the cause (firmware defect, physical magnet condition,
   loss of traction) at the moment of stopping; that is a subsequent
   investigation, and historical evidence (see §3) favors checking the
   recognizer's own logic first, by base rate.

Repeat from step 1 at the newly confirmed (or retained) position.

## 2. The ten propositions

**Prop 1 — Target-acquisition framing.** NAVI's recognition problem is
target acquisition at an expected location, not open-ended waveform
classification. Closed by consensus.

**Prop 2 — Recovery is explicitly out of scope.** Measuring the distance
between adjacent magnets would make geographic recovery possible in
principle; this conference does not pursue it. Closed by consensus
(standing position, reaffirmed).

**Prop 3 — Historical timing reference verified from source, not assumed.**
Otto's SOLONAV calibration corpus timestamps are RTB (return-to-baseline)
anchored; Toby's NAVI_COHERENCE(X22) automatic logs are onset-anchored —
opposite sides of the peak. Two real Otto waveforms measured onset-to-peak
and peak-to-RTB splitting almost exactly 50/50. Full detail:
[`NAVI_CONFERENCE_PROP3_TIMING_REFERENCE_VERIFICATION_20261023.md`](NAVI_CONFERENCE_PROP3_TIMING_REFERENCE_VERIFICATION_20261023.md).
Closed by consensus.

**Prop 4 — The expected neighborhood.** Prediction (map + movement history)
locates the expected opening; the magnetic ensemble verifies the target.
Movement-derived timing is residence at the navigation level, not a
replacement for Hall evidence. Closed by consensus.

**Prop 5 — No architectural change required** to carry Prop 4 forward.
Closed by consensus.

**Prop 6A — Recognize the complete magnetic ensemble, retrospectively.**
Four characteristics agreed: shape (later amended by Prop 7), dominant
polarity (via peak), a **minimum** physically plausible width (necessary,
not sufficient — an excursion must exceed this to be a candidate magnet;
exceeding it does not prove one, and the minimum does not determine where
the waveform ends, only the observed Hall signal does that), and observed
completion (evidence-based, not timer-based). Closed by consensus.

**Prop 6B — Protective stop on failed normal progression.** Each mapped
interval has its own empirically established, pace-normalized latest
permissible arrival time. Exceeding it without confirmation triggers
stop-and-hold (motor authority only; Hall recording continues). Normal
variation in arrival time is not itself abnormal. The geographic-profile /
current-pace-scaling structure is independently supported by Otto's 70x2
onset data (5.5-5.9ms MAE vs 71-87ms uniform baseline, both directions,
~171 positions) and by Toby's automatic-running data (69ms MAE, 1.86% CV).
A candidate three-sigma boundary was tested and **rejected**: descriptive
median+3SD was exceeded in 4.19% (CW) and 2.95% (CCW) of eligible Otto
predictions — far above what a true one-sided three-sigma tail predicts,
and with per-interval sample sizes of only 6-12 observations, too thin to
separate a genuinely fat tail from small-sample estimation error. The
numerical boundary-setting method remains open. Closed by consensus.

**Prop 7 — Shape is diagnostic, not an admission gate.** Explicitly amends
Prop 6A. On real Otto PWM-90 data, a known non-target secondary lobe
(rebound) passed a general Gaussian fit 122/149 and a simpler bell/
smoothness test 149/149 — comparable to genuine events, so shape alone
cannot discriminate a primary magnet from a rebound. A hard 0.13 residual
ceiling would still have rejected 5 of 1,528 known-genuine accepted events.
The actual discriminator in the record is amplitude (rebound peaks fall
below the 70-count sentinel threshold) and proximity/sequence position —
already part of the architecture. Historical false negatives (MM110, MM146,
MM169, MM113) were caused principally by morphology-based rejection in
earlier lineages; MM110 and MM146 have no retained raw waveform, so their
physical cause is unknown and not recoverable from the record. Gaussian/bell
fitting remains available as a retrospective diagnostic and calibration
tool; it cannot veto an otherwise-qualified encounter. Closed by consensus.

**Prop 8 — One expected target; no search, no recovery.** NAVI has exactly
one expected target, from its last confirmed position and the map.
Qualifying evidence confirms and advances; nonqualifying evidence leaves the
confirmed position unchanged while observation continues — this is not
itself a stop trigger. No search among alternative identities. The
established Prop 6B arrival boundary is the sole authority converting
unsuccessful waiting into a protective stop. Introduces no new mechanism;
consolidates Props 1-7. Closed by consensus.

**Prop 9 — Normal operation defines the model; exceptions are minimal,
evidence-justified safeguards.** No additional failure-specific mechanism is
introduced unless field evidence demonstrates a problem the established
cycle and stop-and-hold cannot handle. Demonstrated by this conference's own
process: minimum width (lower bound, not a match target), shape (demoted
from gate to diagnostic once tested against data), and three-sigma
(rejected once tested) were each pulled back the moment evidence didn't
support them. Closed by consensus.

**Prop 10 — Simplicity must be demonstrated, not presumed.** The
architecture is sufficiently defined to require an integrated, read-only
evaluation against historical evidence before further design. Two concrete
next steps, in order: (a) a read-only audit of the actual host sketch for
vestigial recovery or morphology-rejection mechanisms inherited from prior
NAVI lineages, done before or alongside (b) a historical replay of the
complete agreed decision cycle against Otto and Toby recordings, including
comparison against earlier implementations' documented rejection failures
(MM110/146/169/113). A simpler implementation that performs worse is not an
improvement; additional machinery with no measurable benefit should not
survive on theoretical grounds alone. Closed by consensus.

## 3. What remains open (explicitly not settled by this record)

- The minimum-width value itself (must be set from the fastest physically
  plausible speed, with margin for variation — not yet measured).
- The method for setting each interval's latest permissible arrival boundary
  (three-sigma is rejected; an empirical-percentile or other non-parametric
  method on pace-normalized residuals is the likely direction, but untested).
- The current-pace adjustment's own bound — a locomotive already losing
  traction could otherwise inflate its own limit from its own degraded
  recent laps and mask the failure (flagged during Prop 6B, not yet
  addressed).
- All of the above measured so far almost entirely on Toby at PWM 90 (and
  Otto's manual PWM-90 survey for the 70x2 onset study); broader
  direction/speed/locomotive/consist coverage is needed before calibrating
  any number.
- Whether the historical onset-detector and a continuously-updated-median
  detector coincide on real waveforms, and how often a historical onset
  timestamp corresponds to a preliminary lobe rather than the principal
  ensemble — both previously offered as checkable against existing Otto
  data, not yet executed.

## 4. Host sketch for the vestigial-code audit and replay

**`NAVI_RVM_AUTO_STATIONS_BUBBLE`, revision 5.1, commit `5b7a518b`.**

- This is the most recently and actually field-run AUTO firmware: two Otto
  38+Toby runs, 2026-10-21 morning, two-train operation, RVM magnetic
  navigation with the 70x2 requirement, rainbow-carpet station stops
  (Patio, Grillers, Arches, Bamboo), ESP-NOW leader/follower coordination
  and spacing bubble, no IR input. Chosen specifically because it already
  has motor authority and the closest existing relative of the architecture
  just agreed — unlike the observation-only `NAVI_RVM_CONTINUITY_MANUAL`
  lineage or `NAVI_TRACER_0_1` (the failed sketch that triggered this
  conference).
- Revision 6 was built but never flashed; October 21 operations, both runs,
  remained on r5.1. r5 (`fc759b8e`) immediately precedes it.
- **Location:** a separate worktree, not the checkout this conference has
  been running in — `/Users/davidbrown/esp-loco-control-rvmgated/firmware/test-programs/NAVI_RVM_AUTO_STATIONS_BUBBLE`,
  branch `agent/navi-rvm-gated-peer-20261020`. That worktree's HEAD
  (`cd5a6d6`) is one commit ahead of `5b7a518b` and is a documentation-only
  commit (no code change), so the checked-out sketch is exactly the flashed
  r5.1 binary.
- **A finding already exists against this exact sketch**, recorded at
  `cd5a6d6` in `RVM_CAUSAL_COMPARISON_R51_VS_ALIGNER_20261021.md`: r5.1
  discards the operator's standing mid-interval baseline and acquires it on
  the approach to the next magnet. Whether this is a defect, a deliberate
  behavior, or itself in scope for the vestigial-code audit has not yet been
  determined — it should be read before the audit starts, not treated as
  unrelated background.
- Scope question not yet answered: does the audit cover only this sketch's
  own files (`.ino` + its `Rvm*.h` headers), or does it extend to shared
  headers it includes from elsewhere in the repository (e.g.
  `LL_LocoConfig_*.h`)? Needed before Codex's trace begins.

## 5. Next steps, in order

1. Codex audits `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1 for legacy/reachable
   mechanisms inconsistent with §1 (old morphology veto, old geographic
   recovery/search path, inherited state transitions) and reports findings.
   Read-only; no firmware modification; cleanup requires separate explicit
   authorization.
2. Claude performs a second, independent scan of the same sketch after
   Codex's report.
3. Historical replay of the complete agreed decision cycle (§1) against
   Otto and Toby recordings, explicitly including the documented historical
   rejection failures (MM110, MM146, MM169, MM113), to demonstrate — not
   presume — the simplicity achieved.
