# NAVI_COHERENCE — external prior art survey

Requested 2026-09-21: search outside literature and this repo's own record for
decision models similar to NAVI_COHERENCE, and report what's transferable.
This is a research survey only — nothing here is a proposal to change
mechanism, let alone policy. Policy (continuity-first, coherence-not-
perfection) is already ruled on by the operator and Sam; see the references.
What follows is input for whoever weighs the open questions the
[0.1 sketch](NAVI_COHERENCE_0_1_DESIGN_SKETCH_20260920.md) names at its end.

## Where our own model already stands

Chain of custody, from this repo's own docs: decision
[0089](decisions/0089-navi-ir-joint-evidence.md) (one-fault-per-branch
hypothesis forking) → the
[optimistic continuity principle](NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md)
(2026-09-20, three tiers: normal imperfection / meaningful uncertainty /
actual loss) → the
[NAVI_COHERENCE 0.1 sketch](NAVI_COHERENCE_0_1_DESIGN_SKETCH_20260920.md)
(same three tiers, plus two unconditional hard vetoes).

The model description this survey was given goes further than anything
currently committed to docs/: it names four evidence classes explicitly
(CONFIRMATORY / NONCONFIRMATORY / PHYSICALLY INCOMPATIBLE /
MODEL_CHALLENGING, splitting the hard-veto case out as its own class rather
than leaving it implicit), names STATION_MARKER as a future evidence source,
and states the ten-landmark DNA and the reversal-preserves-position behavior
more explicitly than the 0.1 sketch does. That's a further evolution of the
same lineage, not yet its own dated record — noted here for accuracy, not
acted on.

## The closest real-world precedent: railway train positioning itself

This is the one domain where the physical setup matches almost exactly:
fixed track, point landmarks, continuous distance sensing between them,
safety-critical position estimation.

**ETCS odometry (European Train Control System).** A train's position is
tracked as a *confidence interval*, not a point: minimum and maximum safe
front-end estimates, seeded from the last relevant balise group (LRBG) —
a trackside transponder, the direct analogue of a mapped Hall magnet. The
interval starts narrow (roughly the balise's own location-accuracy error,
~±12 m in German deployments) and grows continuously with distance
travelled: the spec bounds the growth at `5 m + 5% of distance travelled`
in each direction, compounding wheel-sensor drift and wear. At ~1000 m
between balises that's a ~120 m-wide envelope by the time the next balise
is due. Crossing the next balise resets the interval back to near the
balise's own accuracy and the cycle restarts.

**Positive Train Control (PTC, US).** GPS plus wheel tachometer dead
reckoning, corrected against an onboard geo-referenced track database
whenever GPS is degraded or the track geometry itself disambiguates
position (which segment/curve the train must be on).

**What's transferable:** NAVI's `IrDistance: closest==0` hard veto
("haven't gone far enough to be MM56 yet") is a binary version of what
ETCS computes as an explicit, growing numeric envelope. The 0.1 sketch
names two open questions it can't yet resolve — the "competitive margin"
for tier-2 rivals, and the consecutive-incoherence threshold for tier-3 —
and both are exactly the kind of constant ETCS derives from a measured
error-accumulation rate instead of guessing. If IR's actual mm/count error
rate is ever characterized on the bench, the same `fixed_floor +
measured_rate × distance` shape gives a principled number instead of a
tuned-by-feel one, and the growing envelope replaces a binary veto with
something that degrades gracefully as travel accumulates.

## The general framework this all sits inside: Bayes filters

NAVI's own plain-English description of its normal decision — "I expect
MM56... there it is... confirmed... now I expect MM57" — is a predict/update
cycle: the textbook shape of
[recursive Bayesian estimation](https://en.wikipedia.org/wiki/Recursive_Bayesian_estimation)
(the "Bayes filter" of mobile-robotics literature), where the posterior
belief after one step becomes the prior for the next. Monte Carlo
Localization (particle filters) is the dominant concrete implementation of
this idea for landmark-based mobile-robot localization.

One piece of that literature maps onto NAVI's hardest case by name: the
[**kidnapped robot problem**](https://en.wikipedia.org/wiki/Kidnapped_robot_problem).
It's explicitly *not* the same as starting lost — the robot has a
confident, well-supported belief that happens to be wrong, which is harder
to detect and recover from than having no belief at all. That is exactly
the shape MODEL_CHALLENGING is reaching for. The standard robotics fix —
continuously inject a small population of random-position hypotheses so
recovery is always available — is a real, working solution, but it's
incompatible with NAVI's explicit design goal of one coherent belief rather
than a standing population of alternatives. Worth naming as a deliberate,
known trade-off (simplicity and explainability vs. a formal recovery
guarantee), not an oversight to fix.

## Data association literature: formalizing the hard-veto/soft-evidence split

**Gating.** Multi-target tracking systems reject impossible measurements
before scoring anything, using an
[ellipsoidal validation gate on the Mahalanobis distance](https://en.wikipedia.org/wiki/Mahalanobis_distance)
between a candidate measurement and the predicted state — a formal, tunable
version of "physically impossible, not just unlikely." The gate threshold
is chosen from a chi-squared distribution at a target confidence level, not
guessed; the same principled-threshold point as the ETCS interval above.

**GNN-with-gating vs. MHT.** Target-tracking systems split roughly into
Global Nearest Neighbor (commit to the single best-scoring hypothesis every
cycle) and Multiple Hypothesis Tracking (defer the decision, keep several
candidate tracks alive, prune later as more data arrives). GNN-with-gating
is recognized as the computationally cheap, "good enough outside heavy
clutter" choice; MHT is strictly more capable and strictly more expensive.
NAVI_COHERENCE's "one preferred position, bounded alternatives only on a
genuine tie" (Refinement 1 of the continuity principle) is GNN-with-gating
by another name — a known, deliberate simplification with a name and a
documented failure mode (it struggles exactly in high-ambiguity/high-clutter
conditions), not an ad hoc shortcut. That failure mode is worth field-testing
directly: MeaningfulUncertainty is where GNN-shaped systems are known to be
weakest.

## Evidence-combination theory: naming the four-way split

**Dempster-Shafer evidence theory** formally separates *uncertainty* (no
evidence either way) from *conflict* (evidence that actively disagrees) —
the same distinction NAVI_COHERENCE draws between NONCONFIRMATORY and
MODEL_CHALLENGING. It's a useful vocabulary check on the four-way
classification. It also carries a documented warning worth taking
seriously here: Dempster-Shafer combination is known to produce
counter-intuitive results specifically when combined evidence is highly
conflicting — which is precisely NAVI's hardest case (several independent
sources disagreeing at once). That's a reason to field-test
MODEL_CHALLENGING deliberately under genuine multi-source-disagreement
scenarios rather than assuming the same "suspect it, ask for corroboration"
logic that works for one bad reading degrades gracefully when several
sources disagree simultaneously — the nearest formal analogue is on record
as *not* degrading gracefully there by default.

**AGM belief revision theory** (the standard philosophy/AI account of how a
rational agent should update beliefs) supplies the formal name for NAVI's
central rule — *information inconsistent with the coherent model is suspect
before the coherent model itself is suspect* — as the **minimal change**
principle: prefer the revision that disturbs existing belief least. It also
has a concept NAVI doesn't yet use explicitly: **epistemic entrenchment**,
the idea that not all beliefs resist revision equally — some are more
load-bearing than others. That's a plausible formal grounding for treating
a ten-landmark DNA streak as more entrenched (harder to overturn) than a
position just reached one landmark ago, if that distinction is ever wanted
explicitly rather than left implicit in "suspect must corroborate."

## Map matching: a different shape for resolving ambiguity

[Newson & Krumm's HMM map-matching](https://www.microsoft.com/en-us/research/publication/hidden-markov-map-matching-noise-sparseness/)
(the standard algorithm behind turning noisy GPS traces into a road-network
path) faces a structurally similar problem — noisy point observations,
a graph of physically possible positions, continuity constraints — and
solves it differently from NAVI_COHERENCE: rather than committing a verdict
the instant each point arrives, it holds a short window and resolves the
*most likely whole path* through recent points at once (via the Viterbi
algorithm), once a little more evidence has accumulated.

NAVI_COHERENCE 0.1 commits a tier verdict per observation, immediately —
consistent with "NAVI alone decides when to advance," which sounds like a
deliberate real-time constraint (a model railway needs a same-cycle
answer, not a windowed one) rather than an oversight. Worth naming as a
genuine alternative shape anyway: a short bounded-lag re-solve specifically
for MeaningfulUncertainty (not the tier-1 or tier-3 cases) would trade a
small, bounded delay for using slightly more future evidence before
retaining alternatives — the opposite trade from "decide now, don't hedge,"
and only worth it if MeaningfulUncertainty turns out to be common enough
in the field to matter.

## What doesn't have a clean precedent

The DNA mechanic itself — a rolling record of the last ten *confirmed*
landmarks (not raw sensor events), used both as a continuity anchor and as
something that must be rebuilt fresh, ten clean in a row, to fully re-earn
trust after a break — didn't turn up a named match in this search. The
closest relatives are fixed-length smoothing/sliding windows in SLAM
back-ends, but the specific "N-in-a-row to re-certify" mechanic reads as
this project's own design rather than an adopted technique. Worth knowing
that's original, rather than assuming a citation exists for it.

## Summary

| NAVI_COHERENCE concept | Closest precedent | What's transferable |
|---|---|---|
| Growing "haven't traveled far enough" veto | ETCS odometry confidence interval (`5m + 5%d`, resets at balise) | A quantified, growing envelope instead of a binary check — could answer the 0.1 sketch's open "competitive margin" / incoherence-threshold questions |
| Predict → confirm → advance cycle | Bayes filter / recursive Bayesian estimation | Vocabulary only; NAVI's version is qualitative, not probabilistic — see caveat below |
| "Confident but wrong" recovery (MODEL_CHALLENGING) | Kidnapped robot problem (Monte Carlo Localization) | Names the difficulty precisely; standard fix (random-hypothesis injection) is knowingly incompatible with NAVI's single-belief design |
| Hard veto vs. soft contradiction | Kalman/track gating, Mahalanobis + chi-squared threshold | A principled way to derive gate thresholds instead of tuned-by-feel constants |
| One preferred position + bounded alternatives | Global Nearest Neighbor with gating (vs. full MHT) | Validates the design choice as a known, named trade-off; also imports its known weak spot (high-ambiguity conditions) as a field-test target |
| NONCONFIRMATORY vs. MODEL_CHALLENGING | Dempster-Shafer: uncertainty vs. conflict | Vocabulary check, plus a documented warning that conflict-combination misbehaves under multi-source disagreement — test that case specifically |
| "Suspect the evidence before the model" | AGM belief revision: minimal change, epistemic entrenchment | Formal grounding; entrenchment could justify weighting DNA streak length explicitly, if ever wanted |
| MeaningfulUncertainty resolution | HMM map matching (Newson & Krumm), Viterbi over a window | A genuine alternative (windowed re-solve) to instant per-observation commitment — opposite trade-off, worth naming even if not adopted |
| DNA ten-in-a-row re-certification | No clean match found | Treat as original to this project |

## One honest caveat

Every precedent above except the railway-specific ones (ETCS, PTC) is
probabilistic or numeric under the hood — particle weights, Mahalanobis
distances, belief-mass functions. NAVI_COHERENCE, as specified in every
version of this repo's record, is deliberately qualitative: a rule-based
coherence judgment ("does the preponderance of evidence agree"), not a
number NAVI computes. That's very likely correct for this hardware and this
failure-visibility requirement — cheap, and every verdict is explainable in
a telemetry line rather than a covariance matrix. But it means the
analogies above are structural, not implementations to port: none of this
literature runs as-is on an ESP32 reading a Hall sensor and an IR wheel,
and none of it should be adopted just because it has a citation. The
transferable parts are the *shapes* of the solutions (growing envelopes,
principled gates, named failure modes to test for), not the machinery.

## References

- [ETCS odometry (confidence interval, over/under-reading)](https://de.wikipedia.org/wiki/Odometrie_(ETCS))
- [Recursive Bayesian estimation](https://en.wikipedia.org/wiki/Recursive_Bayesian_estimation)
- [Kidnapped robot problem](https://en.wikipedia.org/wiki/Kidnapped_robot_problem)
- [Mahalanobis distance / gating](https://en.wikipedia.org/wiki/Mahalanobis_distance)
- [Hidden Markov Map Matching Through Noise and Sparseness — Newson & Krumm, 2009](https://www.microsoft.com/en-us/research/publication/hidden-markov-map-matching-noise-sparseness/)
- [Belief revision (AGM theory)](https://en.wikipedia.org/wiki/Belief_revision)
- Dempster-Shafer conflict-vs-uncertainty in sensor fusion — multiple 2020-2023
  survey papers surfaced consistently on this point (evidence-distance and
  belief-entropy conflict measures); no single canonical source, general
  finding across the field.
- This repo: decision [0089](decisions/0089-navi-ir-joint-evidence.md),
  [optimistic continuity principle](NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md),
  [NAVI_COHERENCE 0.1 sketch](NAVI_COHERENCE_0_1_DESIGN_SKETCH_20260920.md)
