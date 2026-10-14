# NAVI — Known-Route Geographic Decision Model

**Date:** October 13, 2026  
**Status:** Architectural conception for review; **not** an implementation-ready specification or authorization to change firmware  
**Experimental reference:** `NAVI_EWO_0_1_MM045_STOP`, commit `85a2cf2a16d262abdf7258c60f60508b823913ee`  
**Scope:** Geographic judgment on NGR's ordered magnet route. Operating controls and station machinery are outside this document's implementation scope.

## Purpose

NAVI knows the railroad's ordered mile-marker map, its direction of travel, and its last well-supported geographic position. During ordinary forward movement it therefore knows which marker comes next. The central question is **when the expected next magnet has been reached**, not which of 171 unrelated locations might explain an unusual signal.

A discrepant observation is first a reason to examine that observation, **not** a reason to imagine the locomotive has relocated. Route order is physical infrastructure; NAVI's estimate of progress along that route is an evidence-based interpretation and can be corrected.

The primary operational challenge is **false magnetic encounters**: extra Hall openings or secondary field activity being mistaken for another mapped magnet. Missing infrastructure is a different, exceptional maintenance problem.

## Governing process

**Known route and direction → Expected target → Observe → Preserve → Evaluate qualifications and exclusions → Compare viable candidates → Decide → Predict next target → Test and, when warranted, revise.**

1. **Observe and preserve.** Record every detector-qualified Hall opening and its provenance, including repeated and rejected openings. Retain applicable IR movement evidence, opening time, polarity, waveform data, PWM/direction history, and the decision made. The intended detector contract is a ≥70-count departure on two consecutive qualifying median-of-five sampling ticks, with the operational opening at the second tick; reference and rearm semantics still require reconciliation with the selected firmware.
2. **Evaluate the known target.** Ask whether this opening can represent the next mapped magnet. An observation with an established disqualifying characteristic is **invalidated as that target**; its physical source need not be diagnosed. Do not turn definitive target incompatibility into vague uncertainty.
3. **Exclude impostors.** Established wrong opening polarity (with valid reference and mapping), demonstrably impossible travel under applicable bounds, or an individually validated morphology/encounter exclusion can rule out a target interpretation. Unusual shape alone is not yet a validated exclusion rule. Preserve excluded observations for audit and context-sensitive reconsideration.
4. **Compare only viable candidates.** All applicable necessary qualifications must be met before an observation competes. If the only material difference among qualifying candidates is distance from the expected opening, select the closest; do **not** average competing positions. An early candidate at 280 mm for a 300-mm target can be compared through the symmetrical boundary at 320 mm. A better candidate at 299 mm changes that boundary to 301 mm. This comparison is subject to causal data availability and measurement uncertainty; the numerical operating rules remain to be approved.
5. **Make a timely working decision.** NAVI may adopt the best-supported qualifying target interpretation despite a non-definitive discrepancy, preserve the discrepancy, and advance its target. A candidate with a definitive exclusion cannot win merely because a decision is due. The locomotive's one working position is supplied to the operating machinery; no second geographic controller is created.
6. **Test every decision at the next magnet.** Each accepted target predicts the next marker's polarity, approximate spacing, and sequence. The next observation either corroborates the working interpretation, reveals a questionable measurement, or supplies affirmative reason for a local revision. No identification is immune to later evidence.

## Evidence-specific credibility and decisional inertia

Repeated successful, genuinely discriminating predictions establish **decisional inertia**: accumulated support for NAVI's current geographic interpretation. One non-definitive discrepancy should increase **attention to the doubtful evidence** rather than destroy otherwise coherent location knowledge. Conversely, genuinely discriminating contradiction can overturn a longstanding interpretation; simply counting NAVI's own accepted assignments is not independent validation.

Record **which** element is questionable: sensor transport/health, measurement applicability, coordinate association, waveform interpretation, or geographic consistency. Do not automatically convert a distance residual into an IR failure, nor let a questionable local coordinate veto an otherwise coherent longer geographic span.

### Example: two consecutive intervals

Mapped MM090→091 and MM091→092 are each 300 mm. Observed intervals are 270 and 330 mm. The first is 30 mm short, the second 30 mm long, but the MM090→092 cumulative span is 600 mm, matching the map.

If Hall, polarity, morphology and sequence evidence support both identities, NAVI can retain MM091 and accept MM092 while noting that the middle opening coordinate or interval association is questionable. **Compensation is an arithmetic observation, not proof of a particular sensor fault or proof of identity by itself.** Continuous pulse history and the earlier supported checkpoint must remain available; accepting MM091 does not automatically certify its coordinate as a new trusted distance anchor.

If the intervals are instead 270 and 270 mm, the cumulative shortfall is 60 mm: that is a different pattern requiring scrutiny of counting, calibration, references, opening positions, and geographic interpretation.

## Local reconsideration, not global relocation

Keep a bounded record of the recent decisions, supporting/contradictory evidence, associated cumulative coordinates, and current candidates. Evaluate local, physically plausible alternatives when the next observation contradicts prediction: an extra earlier opening, a missed expected encounter, or a questionable coordinate. Revise only when **new affirmative evidence** supports the alternative. Do not search all 171 positions, invent pulses, silently shift anchors, or relabel old evidence to fit the map.

An observation's **target interpretation** may be validated, invalidated, or unresolved. Its eventual geographic assignment may be corroborated or revised. These are descriptive evidence/decision records, not separate navigation authorities or mandatory operating states.

## Suspected versus established missed magnets

Passing the estimated position of MM091 without a valid observation creates **suspicion**, not automatic proof of a miss. The estimated distance may be wrong. If a subsequent observation is independently identifiable as MM092 from cumulative movement, polarity, and route context, NAVI has evidence that MM091 was passed without a recognized encounter. Matching polarity alone is not sufficient when the markers are otherwise ambiguous.

A **credibly established missed magnet** is treated as a probable infrastructure or detection fault requiring a **controlled stop, operator notification, investigation, and explicit resumption**—not routine automatic missed-marker traversal. The distinction between a suspected overshoot and an established miss must be defined and tested before implementation; do not stop solely because an estimated distance threshold expired.

## Evidence channels and boundaries

- **Hall:** establishes magnetic activity; polarity and validated encounter/morphology characteristics help determine whether it qualifies as the target. Repeated openings may belong to one physical encounter.
- **IR:** provides useful cumulative movement evidence. Type-5 and Type-6 are reporting paths from the **same instrument**, not independent physical witnesses. Use the freshest causally applicable evidence with source, boot/epoch, count continuity, timestamps, and reporting latency preserved. Do not pretend later packets were available at an earlier opening. A healthy stream does not prove no physical pulses were missed.
- **Movement history:** applied PWM, measured speed, stops, braking, reversals and grades provide contextual estimates. PWM alone does not prove movement; the foliage obstruction demonstrated this.
- **Morphology:** may gain **experimental exclusion authority only through individually evaluated and approved rules** with measured false-exclusion risk. The historical classifier's false exclusions prohibit adopting it wholesale.
- **Map/DNA:** supplies the expected order, polarity and approximate spacing; does not grant permission for unrestricted geographic relocation.

Evidence should be assessed for its actual meaning and dependencies, not counted as independent votes. The fixed 650-ms fallback is not positive evidence of reaching a new magnet. Geographic identity and a precise IR opening coordinate are distinct facts.

## Engineering status and open decisions

This document records the **conceptual decision model**, not a coded algorithm. Before implementing it, reconcile the EWO 0.1 detector/reference behavior with 70×2 detection; establish causal Type-5/Type-6 association and coordinate-consistent commitment; test specific morphology exclusions and their false-rejection rates; define practical candidate boundaries under measurement uncertainty; specify bounded decision-history retention and local revision rules; establish the missed-magnet stop/notification evidence standard; and test navigation-to-MM045 STOP behavior without changing its existing braking, dwell or departure machinery.

Develop through deterministic host tests, focused historical replay, and **physical railway trials as primary behavioral evidence**. Preserve original EWO 0.1 and manifest-verified COHERENCE R2 as baselines/rollback. Do not treat this document as superseding numbered governing decisions without a separate approved reconciliation.

## Core statement

> NAVI knows the road. Its job is to recognize the next road sign as it arrives. Repeated agreement builds justified geographic confidence; a discrepancy focuses attention on the questionable evidence. The next magnet tests the previous decision. Extra signals are impostors to reject; a credibly missing infrastructure magnet is a maintenance fault, not a reason to invent geographic progress.
