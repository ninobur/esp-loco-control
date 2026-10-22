# NAVI — integrated normal-curve recognition and geographic judgment

**Date:** 2026-10-21
**Status:** Architectural review and proposal only. Not a decision record.
No firmware, research code, or specification has been modified to produce
this document.
**Assignment:** David's pasted brief of 2026-10-21, "NAVI — Integrated
Normal-Curve Recognition and Geographic Judgment," responding to Codex
commit `98db802` (`research/navi-normal-curve-20261021`) and its correction
`a15036f` (`research/navi-wolf-pack-20261021`).
**Authorization:** Research and design only, per the assignment. This
document does not authorize implementation (`AGENTS.md` §2).

## How this applies the governing principles

This proposal is written against `NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`
and `NAVI_DECISION_MODEL.md`, which already state almost exactly what the
assignment is asking for:

- Principle 1 ("put judgment where the information is") and NAVI Decision
  Model §1 (0102, "NAVI is the sole navigation decision authority") are the
  direct answer to the assignment's central question — Hall shape, baseline,
  duration, polarity, map and IR must cooperate *in NAVI*, not be pre-decided
  by a standalone recognizer.
- Principle 2 ("do not simplify the decision-maker by withholding
  information") is why the Gaussian recognizer must report its continuous
  evidence (ΔBIC, timing, baseline fit) to NAVI rather than a single
  accept/reject bit.
- Principles 5, 6, 8 (evidence is not a conclusion; Evidence Accumulation
  Before Revision; do not force decisions under inadequate information) are
  the direct answer to the assignment's request for an observation /
  interpretation / decision separation with bounded hesitation.
- Principle 9 ("evaluate evidence relationally, not as interchangeable
  votes") is the direct instruction against "eight independent tests that
  must all pass."
- Principle 15 (transparent judgment: QUESTION → DATA → CONTEXT →
  ALTERNATIVES → EVALUATION → DECISION/HOLD → RESOLUTION EVIDENCE) is the
  shape this document uses for the proposed architecture.

No principle is departed from here. One tension is flagged below (§4): the
proposed "pending" interpretation state is new machinery, and principle 13
("general model first; exceptions from evidence") means it must earn its
place with a demonstrated need, not be added because it is conceivable.

---

## 1. The line–normal-curve–line model, as understood here

The physical claim is narrow and should stay narrow: a Hall sensor moving
through a railway built from discrete point magnets observes long intervals
of slowly varying, low-noise ADC behavior (track, no magnet underneath),
interrupted by shorter intervals where the field departs from that baseline
in a roughly bell-shaped way and returns. That is a statement about the
*sensor signal*, not yet about geography. It says nothing about which
magnet, whether the locomotive is where NAVI believes it is, or whether this
particular departure is large enough to trust unaided.

Two things follow from treating it this way, both already present in the
corrected research:

1. The baseline is itself a measurement with content — a level, a slope
   (thermal drift, slow ADC wander) and a noise estimate — not an idle
   default the recognizer waits in. `delta_bic` already fits line vs.
   line+Gaussian against that baseline rather than a flat reference.
2. "Return to baseline" is a phenomenon to be recognized (sustained
   subsequent linearity), not a timer — consistent with decision 0107,
   written for an unrelated recognizer line but stating exactly this
   requirement.

The corrected width study (`a15036f`, 945 Otto encounters) supports a single
broad population centered near 55–56 mm rather than distinct shape
families, and a preceding-baseline median of 246 mm (minimum ~212 mm) against
the requested 150 mm baseline-observation floor. That is evidence for "one
Gaussian, individually fitted" over a fixed template, and it is Otto-only,
PWM-90-only, CW-majority evidence — not yet a cross-speed or cross-locomotive
claim.

## 2. Codex's Gaussian recognizer as a source of evidence

What it is good for inside NAVI: it is a small, inspectable, *causal*
estimator of one quantity — how much better a line-plus-bump model explains
the last N observed samples than a plain line does, expressed as a signed,
continuous ΔBIC, together with a start time, a peak value, a duration, and
(when the baseline fit also runs) an estimate of how much the pre-candidate
interval already looked like quiet track. That is real, usable evidence: a
quantified physical datum (principle 9), produced without seeing the map,
polarity, or IR.

What it is not, and must not be treated as, per its own REPORT.md and
summary.json:

- Not a geographic identification. It has no map, no polarity, no IR; it
  cannot know *which* magnet, or whether this is the expected next one.
- Not a validated reliability number. 75 declarations against 186 eligible
  historical ADC-derived annotations (72 overlapping, 114 not, 3 declarations
  unmatched) are explicitly "agreement with historical annotations," not
  precision/recall against independent physical truth. The report says this
  plainly and it should not be restated more confidently anywhere downstream.
- Not yet validated outside PWM 90, CW-majority, Otto-only, single-speed,
  constant-speed conditions.

The correct way to use it inside an integrated system is as one evidential
input whose value is a *strength of curvature evidence over time*, available
to NAVI continuously (not only at the moment of its own internal accept/
reject decision) — exactly what principle 2 requires: the recognizer must
not make NAVI's decision by withholding its intermediate state.

## 3. How the measurements should cooperate

Treat the distinct listed inputs (A–H in the assignment) as answering three
different questions, not eight gates:

**What is the physical signal doing?** (Hall shape, baseline quality,
duration.) This is signal evidence. It supports a hypothesis "a magnetic
encounter is in progress / has occurred," with a strength, not a verdict.

**Where does the map say the locomotive should be, and does the candidate
encounter agree?** (Polarity, map, spacing, movement history.) This is
geographic evidence. It supports or weakens a hypothesis "this encounter is
mapped magnet M_(k+1)," given the last trusted position and direction.

**How much has the locomotive actually moved since the last trusted
position, and by what observed means?** (IR when valid, Hall-implied
progress, elapsed PWM-driven interval as a last resort.) This is movement
evidence, consulted per principle 3 (direct measurement before proxy): IR
distance when IR is healthy; absence of a healthy IR reading is not absence
of movement evidence (principle 16 / decision 0108) — Hall-implied spacing
and, only as degraded-mode evidence, elapsed time at the known PWM remain
available.

The assignment's own worked example is the right shape for combining these:
a weak Gaussian signal, an appropriate elapsed distance from the last
trusted magnet, agreeing polarity, and a clean preceding baseline can jointly
support a confident advance that the signal alone would not justify — and a
strong Gaussian signal arriving at an implausible distance, with
disagreeing polarity, should not advance position. This is a *constraint*
relationship (principle 9's own example — "a measured distance does not add
points to a candidate landmark; it determines which landmarks were
physically reachable at all") and should be implemented as one: geographic
and movement evidence narrow or rule out which mapped magnet an encounter
could be; signal evidence establishes whether an encounter happened at all;
neither is a vote added to the other on a shared numeric scale.

Concretely, a coherent small architecture needs, in the state available when
a candidate's declaration is due:

- last trusted mapped position, direction, and the time/IR-distance it was
  established at;
- the expected next magnet(s), their polarity and mapped spacing;
- the Gaussian recognizer's continuous evidence stream for the current
  candidate (ΔBIC history, duration so far, baseline quality just before
  it opened);
- IR health and, if healthy, distance traveled since the last trusted
  position; if IR is unavailable, that fact, not a fabricated distance;
- the observed polarity of the candidate once it is resolvable.

A judgment is then: given where the locomotive could physically be now
(movement + map constrain this to one, a small ordered set, or "no
physically plausible mapped magnet yet"), does the observed signal-plus-
polarity evidence support advancing to a specific one of those candidates,
support staying at the last trusted position (quiet track, or an
unconvincing signal), or leave the question open pending more observation?

## 4. Observation, interpretation, and decision

Following principle 15 / NAVI Decision Model §4 directly:

- **Observation** is what was actually measured: the Hall trace and its
  ΔBIC-vs-time curve, the baseline fit before it, IR health and distance (or
  its unavailability), the candidate's observed polarity once resolvable,
  elapsed time and PWM. These are facts and are retained regardless of what
  NAVI later concludes from them — consistent with the carry-forward
  ledger's point 2: "a sensor event, a navigation interpretation, and a
  movement command are different things."
- **Interpretation** is which physical/geographic explanation the observed
  evidence, taken together, best supports: "an encounter is occurring and it
  is plausibly M_(k+1)"; "quiet track, no encounter"; "an encounter occurred
  but it does not fit the physically reachable candidates"; "insufficient
  evidence yet to say."
- **Decision** is whether that interpretation is strong enough, relative to
  the incumbent trusted position, to update NAVI's trusted geographic
  position — Evidence Accumulation Before Revision (0104) governs this step
  specifically: hold the incumbent position while it remains best supported;
  revise only when accumulated, not single-sample, evidence supports a
  specific alternative.

For "an uncertain observation can remain unresolved temporarily": this
is exactly 0104's OBSERVE → HOLD → RESOLVE cycle, already adopted doctrine,
not new machinery — a candidate whose signal evidence is ambiguous and whose
geographic context does not yet discriminate between plausible explanations
should be held, with the observation preserved, until either more Hall
samples, an IR distance reading, or continued quiet track resolves it. This
is different from, and should not be confused with, the "pending" grid
search inside `delta_bic` itself, which is a numerical-fit detail, not a
navigation state.

One caution, per principle 13: do not build a dedicated multi-tick "held
candidate" sub-state-machine speculatively. The recognizer's own existing
two-second candidate-censoring behavior and NAVI's existing position-held
default already cover most of what "remain unresolved temporarily" requires
in the PWM-90, single-locomotive case this proposal targets (Stage 1). Add
explicit multi-encounter holding only if Stage 1/2 field evidence shows a
case the simple hold does not cover.

## 5. RVM 5.1 failure mechanisms this approach should avoid

Per the retrospective (`docs/reviews/navigation-retrospective-20261021/`),
the two documents distinguish architecture-inherent failures from
subsequent-integration failures. The integrated-from-the-start approach
addresses the integration failures directly, and does not by itself resolve
the inherent ones:

- **Addressed by integration-from-the-start:** RVM AUTO r1–r5.1's "clearest
  corrective chain" — baseline/recognizer/continuity feedback loops that let
  a wrong accepted position feed back into what the recognizer treated as
  baseline, producing the MM038/MM049 contradictions. Designing baseline,
  signal evidence, and geographic decision together from the start, with the
  decision step explicitly checking physical reachability before accepting
  an encounter, is structurally different from bolting a map onto an
  already-decided recognizer output.
- **Addressed by evidence accumulation / hold:** NAVI_ONE's 115 wrong-magnet
  stops from one-strike polarity, and RVM's own "per-event polarity
  advancement is irreversible" inherent limitation (retrospective's own
  architecture table) are exactly what OBSERVE → HOLD → RESOLVE is for:
  disagreement becomes an interpretation to resolve with more evidence, not
  an automatic contradiction/stop.
- **Not automatically solved:** QUORUM's inherent limit (a bounded candidate
  fence cannot self-correct a stationary declaration outside that fence) and
  DNA's inherent limit (an incomplete physical map cannot yield correct
  recognition regardless of algorithm) are architecture-independent. This
  proposal assumes a correct, complete, audited polarity/spacing map; it
  does not solve the case where the map itself is wrong.
- **Not yet addressed, and should not be claimed solved:** nothing in the
  research or this proposal replays the actual 21-October MM038/MM049 Hall
  streams (REPORT.md: "not present in this workspace... not replayed").
  The claim here is architectural — this class of feedback-loop failure is
  structurally excluded by design — not an empirical claim that these two
  specific incidents are now explained or fixed.

## 6. Fundamental problems with this approach

- **It is still a single, point-magnet, line/bump model.** It assumes the
  railway's Hall signature is adequately described as isolated near-Gaussian
  excursions on a slowly varying baseline. The corrected study's own
  note that RVM's published Ensemble.s/e is not a physical boundary
  measurement, and that the accepted window can include baseline, is a
  caution that a similarly convenient fitted window in the new recognizer
  (its grid search over `mu`/`sigma`) could suffer the same confusion between
  "window that fits well" and "actual magnet extent" if its outputs are ever
  read as a physical boundary rather than as curvature evidence.
- **Combining heterogeneous evidence without numeric voting is harder to
  implement and harder to test than a threshold-and-gate pipeline**, and
  that difficulty is exactly why RVM's predecessors kept reaching for
  independent gates. The "compact, understandable process" the assignment
  (and principle 9) calls for has to be demonstrated as actually compact in
  running code, not just as a stated intention; this is a real
  implementation risk, not solved by this document.
- **The width/baseline statistics are Otto-only, PWM-90-only, and CW-
  majority (667 CW vs. 278 CCW; CCW already shows a longer 95th-percentile
  width tail, 0.650 s vs. 0.610 s).** Generalizing the duration-gate table
  and the 150-mm/650-ms baseline fallback to Toby, other speeds, and
  acceleration (Stages 2–5) is unproven work, not a remaining detail.
- **The sustained-ΔBIC-≥-10-for-110-ms gate and the two-second candidate
  timeout are explicitly provisional, exploratory values** (REPORT.md),
  carried over from a single frozen replay of 75 declarations against
  uncertain historical annotations. Treating them as settled thresholds
  anywhere in an integrated design would misstate the evidence.
- **The historical annotation comparison cannot bound false-positive or
  false-negative rates at all** (summary.json's own limitation statement),
  so no numeric claim about how close this gets to 99.9% can be made yet,
  by this recognizer alone or integrated.

## 7. The smallest coherent integrated architecture (Stage 1, PWM 90)

Four parts, each already named in existing NAVI doctrine, composed rather
than invented:

1. **Signal-evidence stream** (the Gaussian recognizer, essentially as
   written, with its continuous ΔBIC/time/baseline-quality output exposed
   rather than collapsed to one boolean before NAVI sees it — principle 2).
2. **Movement-evidence stream** (IR distance when healthy, its own health
   state when not — decision 0094/0108, unchanged).
3. **Geographic constraint** (current trusted position, direction, next
   expected magnet(s), their polarity and spacing — unchanged map
   representation).
4. **NAVI's judgment step**, the only place these combine: given the
   current candidate's signal evidence and the movement+map constraint on
   which magnet(s) are physically reachable right now, decide ACCEPT (advance
   trusted position to a specific mapped magnet), HOLD (preserve the
   observation, keep the incumbent position, await more evidence), or NO
   ENCOUNTER (quiet track). This step is logged in the QUESTION → DATA →
   CONTEXT → ALTERNATIVES → EVALUATION → DECISION/HOLD → RESOLUTION shape
   already specified for NAVI.

Nothing here requires a new state machine class beyond what items 1–3
already produce and what 0104's OBSERVE/HOLD/RESOLVE already describes for
item 4. The architecture's smallness is itself a testable property: if
Stage 1 implementation needs materially more machinery than this, that is a
signal to re-examine the design, not a license to add it quietly.

## 8. Proposed offline evaluation at PWM 90 before other conditions

Per the research's own stated next step and principle 13 ("general model
first; test it... before locomotive-specific accommodation"):

1. Freeze a PWM-90, Otto, CW-and-CCW replay set with **independently
   bounded** magnet encounters and baselines — not the ADC-derived historical
   annotations, which the research explicitly disqualifies as ground truth,
   and not RVM's own published windows, which the corrected study shows are
   not physical boundaries either. If no such independent boundary source
   exists yet, that is itself the first finding to report, not a gap to
   paper over with the existing annotations.
2. Replay the signal-evidence stream, movement-evidence stream, and
   geographic constraint together through the proposed judgment step,
   unchanged from whatever Stage-1 implementation is approved, against a
   held-out split of that set.
3. Report four distinguishable outcomes, not one reliability number:
   correctly identified mapped magnets, missed genuine encounters, false
   encounters, and appropriately-held/unresolved observations — the
   assignment's own Stage-1 measurement request.
4. Compare this against the standalone recognizer's own 75-declaration,
   annotation-only result from `98db802`, specifically to answer the
   assignment's central question: does adding geographic and movement
   context change outcomes the standalone signal evidence alone got wrong or
   left unresolved? That comparison, not a fresh reliability percentage, is
   the useful Stage-1 result.
5. Do not proceed to PWM 60, acceleration, stop/restart, or Toby until this
   comparison is reviewed and the operator authorizes the next stage.

---

## What is established evidence vs. proposed design choice in this document

| Status | Item |
|---|---|
| Established (research) | 945-encounter corrected width population (~55–56 mm, broad bell-shaped), 246-mm/1.05-s median preceding baseline, CW/CCW split, the recognizer's causal BIC method, 75 PWM-90 declarations and their annotation-overlap counts. |
| Established (repository) | The retrospective's classification of RVM AUTO r1–r5.1 integration failures vs. RVM core's inherent limits; existing NAVI decision doctrine (0102, 0104, 0105, 0107, 0108) already stating the cooperation/hold/no-vote principles this assignment asks for. |
| Proposed here, not yet evidenced | The four-part Stage-1 composition in §7; treating "hold" as sufficient without new multi-candidate machinery (§4); the specific offline-evaluation protocol in §8. |
| Explicitly not established, and flagged against overclaiming | Any reliability percentage for the recognizer alone or integrated; that MM038/MM049 are explained by this architecture (their raw streams were not available and were not replayed); generalization beyond Otto/PWM-90/CW-majority. |

## References

- `research/navi_wolf_pack/normal_curve_recognizer.py`,
  `research/navi_wolf_pack/normal_curve_results/{REPORT.md,summary.json}`
  (commit `98db802`, branch `research/navi-normal-curve-20261021`).
- `research/navi_wolf_pack/independent_width_study/REPORT.md` (commit
  `a15036f`, branch `research/navi-wolf-pack-20261021`).
- `docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`, `docs/NAVI_DECISION_MODEL.md`.
- `docs/reviews/navigation-retrospective-20261021/NGR_navigation_model_retrospective_2026-10-21.md`,
  `NGR_navigation_four_questions_2026-10-21.md`,
  `NGR_navigation_carry_forward_ledger_2026-10-21.md`.
- `docs/decisions/0094-ir-validity-is-continuous-health-not-earned-trust.md`,
  `0104` (Evidence Accumulation Before Revision, referenced via
  `NAVI_DECISION_MODEL.md`), `0107`, `0108`.
