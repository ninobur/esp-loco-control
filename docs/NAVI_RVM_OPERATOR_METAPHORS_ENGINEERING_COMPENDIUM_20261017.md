# NAVI_RVM — Operator Metaphors and Their Engineering Meaning
**Conceptual handoff to Codex · 17 October 2026 · Interpretation, not a new firmware specification**

**Purpose:** Codex received written requirements, but did not participate in the repeated discussions that established what those requirements mean. This document preserves the operator's metaphors, the misunderstandings they corrected, and their implementation consequences. Read it alongside the governing [architecture](../NAVI_Rear_View_Window_Architecture.md), [M1](../NAVI_Rear_View_Window_Mathematical_Specification_M1.md), and the [behavioral authority audit](../audits/navi-rvm-authority-20261017/REPORT.md). Where an earlier document conflicts with the operator's later explicit clarification, **surface the conflict for decision**; this interpretive note does not silently amend governing sources.

## 1. The children's jigsaw puzzle with a printed backing

**Operator's concept:** A linear jigsaw puzzle has a board showing exactly where every piece belongs. The map and fuzzy magnetic landscape are the board; correctly placed earlier pieces are the assembled history. The locomotive moves forward through one expected place at a time. It does not try a new piece at 171 locations. A misshapen piece cannot redraw the board.

**Engineering meaning:** A fixed ordered geographic expectation plus a persistent **trusted waveform alignment** constrains the meaning of incoming observations. The next MM–BL–MM sequence is compared at its expected position. An atypical MM may still represent progression but cannot become a new trusted measurement origin for the next piece. In the MM050–060 → unusual 061 → ordinary 062 example, 062 must fit the board/history through 060, not a location freshly anchored to distorted 061.

**Anti-pattern:** Carrying a variable named `trustedMM` that changes only in telemetry while the actual alignment frontier is free to drift based on the anomalous MM. The authority audit identifies exactly this risk in **F06**. A persistent counter or DP array alone does not prove the puzzle-board invariant.

## 2. Overlapping puzzle pieces: MM–BL–MM

**Operator's concept:** MM060–BL–MM061 completes one piece as the new MM appears. MM061–BL–MM062 completes the next. The shared MM is the seam that proves continuity, **not two independent observations**.

**Engineering meaning:** One geographic token for each newly recognized physical progression, with a new-part quality contribution assessed once. The preceding MM remains context for the next piece. The operator accepted the **five-new-magnet** counting convention, but explicitly meant **collective failure of the continuing pattern**, not five separate amplitude/quality threshold vetoes.

**Anti-pattern:** Five local `quality > threshold` results automatically triggering LOSS OF PATTERN. Audit **F02** identifies this substitution. The five-feature horizon is provisional; do not make it an independently authoritative detector cascade.

## 3. The ECG: whole beat and continuing rhythm

**Operator's concept:** A beat can have an early initiating deflection yet a principal event at roughly the expected time. Its morphology and intervals vary; occasional PAC/PVC-like irregularities do not require defibrillation. The next beats often show that the rhythm never failed. The point is **recognize the whole beat in its rhythm**, not classify a single blip.

**Engineering meaning:** The waveform ensemble can have leading lobes, quiet internal plateaus, polarity changes and trailing recovery. A flat leading lobe displaced from established baseline is still magnetic evidence; a flat trailing lobe may resemble BL without being a new inter-MM interval. The continuous reference and preceding/following context decide. Do not reward earliest opening detection. Prefer convincing, potentially later recognition; avoid false operational stops due to an isolated odd waveform.

**Anti-pattern:** Reintroducing Hall openings, local lobe-width gates, morphology vetoes or hard refractory timers as **independent geographic authorities**. Signal filtering and feature measurements are allowed as subordinate evidence, not as replacement navigation logic.

## 4. The fuzzy brush, hand skin, and statistical boundaries

**Operator's concept:** One need not define the boundary between individual epidermal cells to recognize skin. Each lap paints a fuzzy stroke over a familiar landscape; many strokes define a region of ordinary variation.

**Engineering meaning:** Preserve a continuous, statistically tolerant waveform rather than forcing every quiet core into a binary BL/non-BL class. Variation in amplitude, shape, timing and apparent extent should be accommodated. The October 17 baseline-extent work found useful *supporting* spatial evidence, not a universal cutoff. A flat region alone does not prove BL.

**Anti-pattern:** Making baseline classification an isolated mandatory gate, or over-smoothing until significant magnetic structure disappears. Initial RVM deliberately uses **one fixed reference**; automatic lap learning and model succession remain deferred until the recognition concept works.

## 5. The zipper with a misplaced tooth

**Operator's concept:** MM050–060 fit; tooth 061 is misplaced. Keep assembling the zipper and test 062 against the original zipper, not against the bad tooth. If 062 also differs, examine 063. The pattern is king; individual observations must fit the board, not reshape it.

**Engineering meaning:** Separate (a) evidence of **physical progression** from (b) quality of the observed piece. An odd MM may count and be logged, while being barred from **trusted reanchoring**. Later coherent pattern restores confidence without erasing the discrepancy. No sticky Hall/IR guilt. Persistent history and current quality are distinct.

**Anti-pattern:** Every accepted event resets the next distance origin, or the winning phase silently follows the outlier while the trusted label remains unchanged. Audit **F06** is a direct test of this principle.

## 6. A familiar road, not teleportation

**Operator's concept:** A driver stopping at the top of a hill knows that the road continues downhill. After an hour stopped, the driver does not forget the road or declare a new starting point. Likewise NAVI has one declared location and known forward route; it does not teleport between unrelated places.

**Engineering meaning:** Operator declaration immediately establishes map and direction. STOP/GO preserves current MM, developing ensemble and successful pattern history, even stopped on a magnet for an hour. PWM-zero sensor activity has no **automatic geographic authority**. Resume expects continuation, not a new baseline or position declaration. A genuinely missing entire ensemble remains an **unresolved inference problem**; knowing the route does not prove whether the train moved when sensors supply no evidence. Audit **F01** requires an honest disposition, not arbitrary PWM-based tile fabrication.

**Anti-pattern:** Resetting the reference on STOP, treating a PWM-zero plateau as BL, or assigning every subsequent magnetic bump a new location by blind counting.

## 7. Wheel rotation is not necessarily railroad movement

**Operator's physical explanation:** Otto leads; the Hall sensor and IR wheel are on trailing cars. The operator sometimes moves cars back and forth at PWM zero to see magnets underneath. Two IR-count bursts ~10 seconds apart are plausibly handling, not locomotive coasting. A 35-pulse count (~338 nominal wheel mm) does not prove forward geographic displacement.

**Engineering meaning:** Preserve observations but do not manufacture MM advancement or reset geographic state from PWM-zero IR alone. The operator explicitly **deferred a new coasting subsystem**. There is a source-governance conflict to resolve: the audit cites historical decision H120 requiring a hold for *credible measured displacement*, while the later discussion rejects automatic action on unexplained PWM-zero counts. Audit **F04** proves that even an optically invalid packet can currently latch a hold. Do not quietly pick one policy; request a narrow operator disposition before changing this path. Independently preserve real E-STOP and motor safety.

## 8. Code blue versus an isolated irregular beat

**Operator's concept:** During a public demonstration, an unintended NAVI STOP is a major operational failure, not a harmless safety success. The railroad should continue through ordinary variations. A station landing one MM late may be preferable to a gratuitous withdrawal that requires restarting the demonstration.

**Engineering meaning:** Optimize **uninterrupted correct operation**, not the number of conservative interventions. LOSS OF PROGRESSION is a genuine physical stall (E-STOP); LOSS OF PATTERN is sustained, collectively indefensible geographic continuity (controlled STOP). They are different. Retain independent physical protections, but do not allow local waveform anomalies, missing optional diagnostics or inherited station fallbacks to stop a coherent train without architectural justification. Audit **F02/F03/F05/F09/F11** matter particularly.

**Anti-pattern:** Treating every unusual Hall or IR measurement as a reason to halt, or treating a stop triggered by five isolated local thresholds as proof of failed whole-pattern coherence.

## 9. What the metaphors do *not* authorize

They do **not** authorize inventing missing Hall observations, silent tile corrections, unrestricted geographic recovery, overriding a genuine physical E-STOP, assuming IR distance is accurate, or guaranteeing a reference can resolve an indistinguishable repeated pattern. They do not select DTW, CUSUM, HMM or any numerical threshold. Those remain engineering proposals to validate.

## 10. How to use this compendium in the correction dialogue

For each audit finding, Codex should explain:
1. The **reachable code path** and the observable action it causes.
2. Which metaphor/architectural requirement authorizes that action.
3. Whether the mechanism is subordinate measurement machinery or has acquired **independent authority** to count, veto, stop, reset, reanchor, or alter station control.
4. The smallest change that restores architectural fidelity, **without inventing a new subsystem**.
5. A test that would demonstrate the intended behavior and would fail for the adulterated version.

Prioritize audit F01 (missing evidence), F02 (collective pattern versus local quality gates), F06 (trusted context actually constrains alignment), F07 (recorded stall), and F03/F05/F08 (station integration). The PWM-zero authority conflict F04 requires explicit operator clarification; do not bury it in a coding preference.

**The central rule:** NAVI is not qualifying isolated magnets and then hoping they form a route. It is recognizing a familiar continuous route, with each imperfect observation interpreted in that route's persistent context.

**Status:** explanatory handoff only. No firmware changes or correction authorization.
