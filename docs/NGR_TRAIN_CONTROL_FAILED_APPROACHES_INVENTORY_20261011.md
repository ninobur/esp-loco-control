# NGR Train Control — Inventory of Failed Attempts and Approaches

**Recorded:** 2026-10-11. **Provenance:** David supplied this 32-item retrospective in the Train Control discussion and requested repository preservation.

**Status:** Historical engineering evidence and lessons learned. **Not a current NAVI_EWO_0_4 specification, a substitute for decision records, or implementation authorization.** This inventory combines actual field failures, architectural failures, source-review defects, discontinued experiments, and development-process mistakes. These are **not 32 separate railway incidents**. Historical resolutions are reported as retrospective findings, not silently treated as current approved requirements. This document is paired with [the 14-item architectural retrospective](NAVI_ARCHITECTURAL_INNOVATIONS_THAT_DID_NOT_WORK_20261011.md).

## I. Hall sensor and baseline failures

### 1. Continuously adaptive Hall baseline
- **Attempt:** Allow the baseline to follow measured magnetic conditions.
- **Failure:** During slow movement or rest in magnetic fringe, the baseline migrated toward the magnet. Later changes looked like new magnets.
- **Lesson:** A stable signal may be a stable magnetic field, not ordinary track.

### 2. Time-based quiet detection
- **Attempt:** Require sufficiently quiet Hall observations for a prescribed period.
- **Failure:** Long magnetic shelves could satisfy quiet-time tests.
- **Lesson:** Time alone does not establish that the sensor is away from a magnet.

### 3. Freezing the baseline at a stop
- **Attempt:** Stop adaptation when applied PWM became zero.
- **Failure:** Baseline contamination could occur during the preceding slow approach; the freeze preserved the wrong value.
- **Lesson:** Freezing a corrupted reference cannot restore its physical meaning.

### 4. Capturing a baseline from signal flatness
- **Attempt:** Accept low-spread sample populations as ordinary background.
- **Failure:** Flat magnetic shelves also have low variance.
- **Lesson:** Statistical stability does not identify the physical cause of the signal.

### 5. Fixed startup baseline
- **Attempt:** Keep the initial Hall reference indefinitely.
- **Limitation:** Environmental and session drift could erode amplitude margin and cause missed detections.
- **Lesson:** Renewal needs physical justification; neither unrestricted adaptation nor infinite freezing is sufficient.

### 6. X22 locked-baseline implementation
- **Status:** **Not a failed architecture.** It was a productive response to earlier baseline problems.
- **Mechanism:** Establish a per-interval reference after a magnet, then lock it through the next encounter.
- **Known concerns:** Subthreshold shelves and reference aging remained questions. NAVI_COHERENCE field operation later provided substantial evidence that this approach worked.
- **Lesson:** Retain successful insights without automatically importing the old authority structure.

## II. Hall detection and magnet identification

### 7. Toby's inherited 38-count threshold
- **Problem:** The old threshold admitted weak secondary magnetic events, including false openings in roughly the 38–63 count band.
- **Response:** A provisional 70-count excursion over two consecutive 1-kHz samples.
- **Caveat:** A selected threshold is an operating convention that must be validated on the installed railway, not treated as a universal law.

### 8. Magnet closure as navigation event boundary
- **Failure:** Magnetic tails, shelves, secondary lobes and reference displacement made close/reopen transitions unreliable.
- **Response:** Use the opening as the detection event; later waveform behavior is diagnostic.

### 9. The 400-ms waveform window determining identity
- **Failure:** Later waveform portions could overwhelm or reverse the opening's apparent polarity.
- **Response:** Opening polarity establishes the observation; window morphology has no independent NAV authority.

### 10. Secondary detections from one magnet
- **Failure:** Secondary Hall excursions, some around 402 ms after the genuine crossing, could be mistaken for new landmarks.
- **Response:** Use physically demonstrated magnet separation and trustworthy measured travel; a simple elapsed-time allowance cannot prove a new marker.

## III. Timing and physical movement

### 11. Reusing the historical 500-ms guard
- **Problem:** A value designed for a closure-related timing reference was reused after the architecture changed to detection-to-detection timing.
- **Response:** Reconsider the interval against actual event timing; a provisional 650-ms value was later selected for a different configuration.
- **Distinction:** The historical 500 ms itself was not intrinsically a defect. Neither 500 nor 650 ms proves physical separation.

### 12. PWM-dependent 700–1000-ms timing guard proposal
- **Result:** Compared alternatives did not outperform the fixed guard on the available events.
- **Disposition:** Additional PWM-dependent complexity was declined; this was an **unadopted proposal**, not a failed field deployment.

### 13. Elapsed time mistaken for physical distance
- **Problem:** Different actual segment lengths, speed changes and missing magnets can yield similar Hall-to-Hall elapsed times.
- **Lesson:** Interpret timing using surveyed geometry and measured motion, not raw time as interchangeable distance.

### 14. PWM mistaken for exact speed
- **Problem:** Location, grade, direction and load changed physical speed at the same PWM. The analysis documented about 31% variation by location.
- **Disposition:** PWM may provide contextual evidence, not measured travel.

## IV. IR measurement and navigation

### 15. TRACKING equated with valid distance
- **Mechanism:** `MovementEvidence::between()` required both endpoint detector states to be TRACKING.
- **Failure:** Normal deceleration/stopping changed detector state and could invalidate useful cumulative distance.
- **Lesson:** Detector classification and measurement integrity are different questions.

### 16. SIGNAL_STALE equated with instrument failure
- **Mechanism:** A 2.5-second period without a new completed optical pulse was reported as SIGNAL_STALE.
- **Failure:** An intentionally stopped wheel could be misreported as a failed instrument.
- **Lesson:** Repeated fresh packets containing an unchanged count establish zero **observed** incremental movement; lost communication means packets themselves cease arriving.

### 17. Ten-magnet IR trust establishment
- **Attempt:** Require ten matching geographic observations before IR was trusted.
- **Rejected premise:** A measuring instrument does not improve intrinsically because NAVI has recognized ten landmarks.
- **Response:** Health/readiness assessed continuously. Ten-marker navigation-history reasoning is a separate matter (0094).

### 18. Losing IR continuity through ordinary stops
- **Failure:** No pulse activity during deceleration/dwell was sometimes treated as an odometry discontinuity.
- **Response:** A normal stop does not end the cumulative measurement frame.

### 19. Confusing resumed IR operation with restored MM-reference distance
- **Failure of concept:** Fresh wheel counts after a genuine measurement outage cannot reconstruct travel that was not recorded.
- **Response:** Fresh measurement can resume without implying restoration of the previous mapped origin; resynchronization requires an actual valid landmark.

## V. Position recovery and NAVI judgment

### 20. Whole-route 171-MM polarity search
- **Mechanism:** Search entire route for a sequence match without sufficiently hard physical bounds.
- **Failure:** A unique matching magnetic word could imply a physically impossible jump. One documented incident placed Toby physically at MM029–030 while the dashboard displayed MM163.
- **Caution:** The supplied retrospective explicitly notes that the endpoint mismatch alone does **not** prove which earlier correction caused it; the repository has a separate logged MM065→166 relocation.
- **Historical resolution:** Physically possible before statistically plausible; a ±10-MM outer-bound proposal superseded whole-map recovery in the NAVI_COHERENCE lineage (0098). EWO later set a different, target-only architecture (0109).

### 21. One contradictory observation undermining geographic identity
- **Failure:** A single bad observation could erase substantial coherent route knowledge.
- **Response:** Decisional inertia: preserve observations, hold the best-supported incumbent, and revisit only with persuasive physically possible evidence.

### 22. Confusing missing observations with wrong observations
- **Failure:** No Hall report might be scored as a contradictory polarity, or a premature event might advance the expected marker.
- **Response:** Missed Hall identity is UNKNOWN, not an invented pole. Early nonmatching evidence does not itself move geography.

### 23. IR disagreement treated as an unconditional veto
- **Observed result:** NAVI_COHERENCE 0.5's ±15% rule rejected a likely genuine Hall magnet when IR read 251 mm against a mapped 300 mm.
- **Response:** Keep disagreement visible and investigate the instrument; avoid broad geographic relocation solely to force agreement.

## VI. C++ architecture and implementation failures

### 24. IR architecture 0.1: permanently lost continuity
- **Defect:** After a discontinuity, `continuous_ = false` lacked an ordinary path to recovery.
- **Result:** Future valid MM distance references could remain impossible without an explicit reset.
- **Discovery:** Claude source review.

### 25. IR architecture 0.2: reopening an invalid odometry run
- **Defect:** An invalid run could resume under its old identity when measurements restarted.
- **Result:** An old MM coordinate could regain apparent validity despite unmeasured travel.
- **Resolution:** Introduce epoch identity: an ended measurement epoch does not reopen.

### 26. Insufficient use of cumulative fault counters
- **Concern:** Point-in-time optical status could appear healthy although a fault occurred between transmitted snapshots.
- **Historical response:** Examine cumulative gap/saturation/abort/inference counters for hidden events.
- **Current-design caution:** Such counters are relevant diagnostics, **not automatically proof that an otherwise coherent completed-pulse counter is invalid**. Later EWO rulings made that distinction explicit.

### 27. `unreliableSamples` treated as proof of bad distance
- **Failure:** The counter incremented outside TRACKING, including normal stationary operation.
- **Response:** Do not promote it to a continuity veto until its meaning supports that judgment.

### 28. Legacy detector terminology survived architectural change
- **Problem:** Old classifications such as SIGNAL_STALE retained operational implications after the new design rejected those implications.
- **Response:** Distinguish packet freshness, instrument diagnostics, measurement applicability, epoch identity and MM synchronization.

## VII. Engineering-process failures

### 29. Incompatible measurements compared directly
- **Problem:** Hall threshold values such as 63, 96 and 125 were compared across locomotives, mounting conditions, baselines and detector versions.
- **Response:** Preserve acquisition definitions and identify exclusions such as the displaced MM012 magnet before drawing comparisons.

### 30. Reopening already settled questions
- **Problem:** Decisions made during CODEX/X22 work were not consulted before starting new baseline discussions.
- **Response:** Use the repository decision records as durable engineering memory, and trace subsequent supersession before reopening a question.

### 31. Architectural changes without reliable provenance
- **Problem:** New decisions sometimes contradicted older ones without explaining whether the change was operator-approved or merely an agent proposal.
- **Response:** State preceding rule, evidence, conceptual revision, operator ruling, and precise supersession.

### 32. Inventive implementation beyond approved decisions
- **Example:** Whole-route ten-pattern correction extended a reasonable request into an algorithm whose geographic reach exceeded intended physical constraints.
- **Response:** Keep approval of the architecture separate from permission to implement it. Reconcile code against operator-approved rules before changing structural behavior.

## Cross-cutting assessment

The central repeated failure was giving an observation or a rule authority beyond its physical meaning:

- A Hall baseline was allowed to decide what ordinary track was.
- A waveform completion or later shape was allowed to define a landmark.
- Optical activity/diagnostics were allowed to determine cumulative measurement integrity.
- A polarity pattern was allowed to replace established position anywhere on the railway.
- Navigation geography was trusted downstream by station and CTO logic even after its physical premise had become doubtful.

The subsequent conceptual improvement was:

**OBSERVATION → MEASUREMENT INTEGRITY → PHYSICAL POSSIBILITY → NAVI INTERPRETATION → ACTION.**

Important successful or promising developments included X22's locked baseline, the IR epoch/MM-synchronization distinction, and evidence accumulation with geographically bounded reasoning. **Those successes have different levels of field validation.** Do not treat a favorable review or synthetic regression as track acceptance.

## Provenance, limitations, and use

This is an edited **preservation of the operator-supplied retrospective**, not proof that each historical interpretation has been separately verified against complete native field recordings. The dates, numerical observations, distinctions, and reported dispositions should be reconciled with actual source records before being cited as a present build requirement.

In particular, this archive must **not** silently reinstate historical thresholds, diagnostic health vetoes, whole-map recovery, 10-marker trust periods, expired station machinery, or deprecated EWO rules. Operator decisions and their later explicit amendments take precedence.

Relevant records include `docs/NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md`, `docs/NAVI_EWO_GOVERNING_DOCUMENTS.md`, `docs/NAVI_MM045_NAVIGATION_INVESTIGATION_20261009.md`, `docs/NAVI_AUTO_CORRECTION_FAILURE_20260923.md`, `docs/decisions/0092-*`, `docs/decisions/0094-*`, `docs/decisions/0098-*`, `docs/decisions/0109-*`, and `docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`.
