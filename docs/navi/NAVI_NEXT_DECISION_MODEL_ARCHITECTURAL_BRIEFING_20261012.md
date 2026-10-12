# NAVI: ten-source briefing for the next navigation decision model

**Date:** 2026-10-12. **Status:** focused design-support analysis; recommendations are PROVISIONAL, not implementation approval. **Inventory baseline:** `63ff4b03ecb2fbaf824047e2d3ba0a87ba84df93`, branch `docs/navi-engineering-continuity-20261012`.

## Conclusion

The strongest common direction is **geographic continuity with independent evidence and continuing identity verification**. NAVI knows a railway and a journey; it is judging successive encounters, not routinely locating an anonymous polarity word anywhere on the map. However, retaining continuity must not protect an already incorrect identity. Distance agreement alone has demonstrably sustained a wrong journey, while sequence recovery has sometimes corrected an earlier false advance.

Before another sketch, specify separately: what was physically observed; whether it can represent a new magnet; which mapped identity is supported; and whether that identity may establish a new geographic/distance anchor. Neither a Hall threshold crossing nor a plausible distance window should automatically perform all four operations. This separation is a **recommended synthesis**, not a newly approved algorithm.

The critical open conflict is authority: October 10 documents allow Hall/DNA/reachability to resolve questionable IR, while retained older EWO rules forbid Hall-only advancement; the scoped 0.3 correction retains a 650-ms fallback. Those scopes cannot be collapsed into one universal current rule. Moreover, the corrected code still does not establish the physical “no missed pulses” condition.

## Scope and reading method

All ten selected revisions below were read in full, including historical text retained beneath amendments. Their cached bytes were checked against the Git blob identities in the preserved inventory. Selection balances enduring principles, approved decisions, actual failures, recovery evidence, and implementation audits; it is not a ranking by date or citation count. Relevant source logic was also inspected as listed in §7.

This completes the requested ten-document reading, **not** the 1,653-revision historical review. Its ledger and checkpoint were not advanced. Field results below come from the identified reports; this assignment did not independently replay their complete raw logs or repeat physical tests. Numerical map properties use the already preserved map verification. Host/build results in source 10 are reported results, not tests rerun here.

The research branch subsequently acquired the [magnet-pattern methodology audit](https://github.com/ninobur/esp-loco-control/blob/3b2e44137a2928b402fd5a0ae97a40fb00365af6/docs/navi/NAVI_MAGNET_PATTERN_STUDY_METHOD_AUDIT_20261012.md). Its complete 67-line record was read as supplementary context, outside the frozen ten. It withdraws a 12-magnet reversal recommendation and warns that the simulator did not preserve trusted anchors appropriately. Its underlying ZIP and numeric experiments were not independently examined; they are not promoted to validated navigation evidence here.

## 1. The ten selections

Each link pins the substantive revision actually read. Commit dates may differ from the experiment or decision dates inside the document. A relocation commit is not a new experiment.

| ID | Repository path and exact revision | Why selected; specific knowledge contributed |
| --- | --- | --- |
| T1 | [docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md](https://github.com/ninobur/esp-loco-control/blob/612b791ee90e2404ea8febded675edbd622e063e/docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md), `612b791` | Enduring architecture: NAVI owns contextual judgment; observations must survive disagreement; direct physical evidence precedes proxies; Observe/Hold/Resolve preserves accumulated knowledge. Prevents designing another sensor gate as the whole navigation model. Principles guide design but do not authorize implementation. |
| T2 | [docs/NAVI_DECISION_MODEL.md](https://github.com/ninobur/esp-loco-control/blob/cbdfe9495ca1b52d2165a5ce6be27cbfce1d1439/docs/NAVI_DECISION_MODEL.md), `cbdfe949` | Canonical application of the principles, decision links and D1–D10 reconciliation. Explains geographic continuity, NAVI-owned Hall interpretation, spatial references, and unresolved policy. Its October amendments and retained older body reveal authority conflicts that must be reconciled before design. |
| T3 | [docs/NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md](https://github.com/ninobur/esp-loco-control/blob/5101974615600926b6cb94e5d3df12c0f9475dff/docs/NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md), `5101974` | Preserves the transition from NAVI_ONE/X20/X21 through SIMPLIFIED to COHERENCE. Shows why improving Hall acquisition was useful but insufficient, why rolling geographic evidence replaced single-event authority, and why a missed mapped position differs from a received Hall callback. Essential earlier reasoning, not a current specification in every detail. |
| T4 | [docs/NAVI_EWO_GOVERNING_DOCUMENTS.md](https://github.com/ninobur/esp-loco-control/blob/cbdfe9495ca1b52d2165a5ce6be27cbfce1d1439/docs/NAVI_EWO_GOVERNING_DOCUMENTS.md), `cbdfe949` | Authority directory: governing records versus plans, evidence and prior art. Connects reference acquisition, reversal, first-target exception, IR policy and operator declaration. Also exposes stale median-of-five/IR-only descriptions beside later continuous-DNA amendments. Useful for detecting discrepancies, not mechanically obeying every retained sentence. |
| T5 | [docs/decisions/0094-ir-validity-is-continuous-health-not-earned-trust.md](https://github.com/ninobur/esp-loco-control/blob/d59a332bd63d3ffd8f780a7ee49599b4ff8ba370/docs/decisions/0094-ir-validity-is-continuous-health-not-earned-trust.md), `d59a332`; decision 2026-09-22 | Explicit operator-approved rejection of a ten-magnet IR trust probation, with the original conversation preserved. Separates instrument validity from navigation-history length and from geographic interpretation. Fresh repeated counts can mean measured zero movement; they do not automatically mean a broken sensor. Later refinements limit the authority of diagnostic health flags. |
| T6 | [docs/NAVI_COHERENCE_0_5_FIRST_AUTO_RUN_20260922.md](https://github.com/ninobur/esp-loco-control/blob/06b89dfd345340cfc5c211bdd92f8a56a58b19b7/docs/NAVI_COHERENCE_0_5_FIRST_AUTO_RUN_20260922.md), `06b89df` | Balanced field evidence: Toby completed extensive AUTO operation and sequence recovery corrected a false advance, yet weak/late Hall events and an invalid initial IR interval caused false acceptance. A real-looking strong event was rejected by distance; the uncoupled IR car exposed disagreement between radio freshness and relevance to locomotive travel. |
| T7 | [field-records/verdicts/20260926_toby_20q3_run3.md](https://github.com/ninobur/esp-loco-control/blob/ffde493246fd5a58d35b97722f8124d1fd64496f/field-records/verdicts/20260926_toby_20q3_run3.md), `ffde493`; tested build `c1651ef` | Counterexample to “distance agreement proves identity”: an inherited two-marker error persisted through 32 events, despite coherent IR windows. Actual-pole alignment and operator final MM053 favored the alternate journey. Explains reversal/reference circularity and shows successful distance rejection of nearby extras, including one beyond the 650-ms guard. |
| T8 | [docs/NAVI_MM045_NAVIGATION_INVESTIGATION_20261009.md](https://github.com/ninobur/esp-loco-control/blob/85a2cf2a16d262abdf7258c60f60508b823913ee/docs/NAVI_MM045_NAVIGATION_INVESTIGATION_20261009.md), `85a2cf2`; code branch `codex/navi-cruise-stop-20261009` | Separates stopping symptoms from navigation evidence. Documents a growing map/IR mismatch, unreliable inference from abort counters, and the decision-snapshot versus Hall-opening anchor mismatch. Preserves uncertainty about the physical cause rather than labeling every discrepancy a missed Hall or missed optical pulse. |
| T9 | [docs/NAVI_EWO_DNA_IR_CREDIBILITY_REVIEW_20261010.md](https://github.com/ninobur/esp-loco-control/blob/f3084b6f102125dd6fe8700135b8e58d004d6621/docs/NAVI_EWO_DNA_IR_CREDIBILITY_REVIEW_20261010.md), `f3084b6`, branch `claude/hopeful-edison-jt4qow` | Detailed cross-version architectural analysis: target-only polarity filtering, spatial-collection blindness during IR stalls, false re-anchoring, map-word uniqueness, bounded competing interpretations and pulse-timing proposals. Valuable precisely because its attractive innovations are explicitly unapproved and some causal claims exceed the measurements. |
| T10 | [firmware/programs/NAVI_EWO_0_3/IR_VALIDITY_CORRECTION.md](https://github.com/ninobur/esp-loco-control/blob/c4cd2b2b8edd5641a0c4a7a31191614f7cba153e/firmware/programs/NAVI_EWO_0_3/IR_VALIDITY_CORRECTION.md), `c4cd2b2`, branch `codex/navi-ewo-0-3-ir-validity-review` | Most useful correction/verification boundary: explicit current-time validity, independent Type-5/Type-6 freshness, both distance-window edges at Hall opening, timing questions without automatic veto, and a detectable interrupted optical cycle test. Explicitly leaves entirely undetected cycles, stationary behavior and missing-magnet recovery unresolved. Host/build success is not field acceptance. |

No separate legacy CTO document displaces these ten: T2 already preserves the relevant foundational distinction—earlier leader/next-station waiting arose from limited information, while richer geographic knowledge can support different coordination. This is not an exhaustive CTO assessment. Dispatcher or block recovery cannot make an incorrect locomotive MM correct, and historical CTO restrictions must not be imported into navigation merely because they once worked under different information constraints.

## 2. Established requirements and their limits

“GOVERNING” identifies design authority; “VALIDATED” identifies evidence. They are separate properties.

1. **GOVERNING principles:** NAVI makes the contextual navigation judgment. Sensor readings, expectation, interpretation and decision remain distinguishable; disagreement must not erase evidence. Protect established knowledge without concealing contradictions. T1–T2 support this; they do not prescribe a new scoring system.
2. **GOVERNING, with later scope refinements:** 0094 rejects geographic probation for IR. Ten mapped positions of navigation history are not ten positions required to earn instrument trust. Freshness, counter integrity, distance accuracy and map applicability are different questions (T5, T10).
3. **GOVERNING EWO startup:** [0118](https://github.com/ninobur/esp-loco-control/blob/b8ff3d27add2d10156cea5f3ef756871593c90ac/docs/decisions/0118-first-target-after-declaration-is-found-by-hall-onset-without-a-distance-window.md) exempts the first target from a normal full-interval distance window because an interval declaration supplies no exact magnet origin. Absence followed by onset and movement is required by that record. The admitted risk of missing the first marker and accepting a later same-pole marker is preserved, not resolved by inventing a precise declaration coordinate.
4. **GOVERNING EWO handling:** [0120](https://github.com/ninobur/esp-loco-control/blob/cbdfe9495ca1b52d2165a5ce6be27cbfce1d1439/docs/decisions/0120-pwm-zero-movement-requires-operator-position-verification-and-declaration.md) preserves localization at a motionless PWM-zero dwell, but measured wheel movement at PWM zero requires operator verification/repositioning and declaration. It supersedes 0119 automatic reacquisition. Do not conflate this hold with an instrument fault or clear it through a new interpretation proposal.
5. **GOVERNING concept, incompletely validated geometry:** [0113](https://github.com/ninobur/esp-loco-control/blob/cbdfe9495ca1b52d2165a5ce6be27cbfce1d1439/docs/decisions/0113-the-navi-ewo-hall-reference-is-spatial-and-is-replaced-from-the-leading-field-boundary.md) replaces time-weighted baseline collection with spatial reference acquisition after the leading field boundary. Its 0–100 mm clearance and 100–200 mm collection scheme is a design choice, not a demonstrated universal field-zone bound. Required sample coverage and suitability remain open. Subsequent timeout amendments do not experimentally prove physical departure from a field.
6. **Scoped 0.3 constraints, not a universal next-model settlement:** T10 preserves 70-count/two-sample Hall qualification, ±15% distance qualification when IR is applicable, and 650-ms fallback when it is unavailable. It corrects implementation of these constraints without redesigning DNA, stationary handling or missed markers. It does **not** resolve the contradiction with earlier EWO withdrawal of Hall-only fallback. Its “no missed physical pulses” requirement is not satisfied merely by consecutive delivered counts.

## 3. What the experiments actually establish

| Evidence | Demonstrated success or useful mechanism | Failure and limitation |
| --- | --- | --- |
| T6: Toby, 0.5 AUTO, Sept 22 | Report records 723 Hall events, 710 advances, 13 refusals and 15 station stops; later sequence correction repaired an earlier false MM111 advance. Useful demonstration of partial operational success and recovery. | The interval to the false event began before optical TRACKING; an endpoint-validity rule made distance unavailable and timing admitted the false advance. Another event at about 700 ms and 38.6 mm was correctly rejected by IR, although time alone would admit it. A strong South event at 251/300 mm was rejected; the report interprets it as genuine, but that physical identity is not independently established here. |
| T6: IR car uncoupled | Hall/sequence navigation continued for several stations with the IR car standing still and the radio still live. Demonstrates some resilience under that specific configuration. | Radio freshness did not establish that measured wheel travel described Toby. It does not prove unrestricted Hall-only operation is reliable. Reported station offsets are derived from NAVI evidence, not independent physical verification at every station. |
| T7: Toby reverse run, Sept 26 | Three nearby extras were distance-rejected (about 9.7–19.3 mm). One appeared after 720 ms, demonstrating a useful rejection that a 650-ms guard alone cannot supply. Baseline had 29 locks and 3 rejects. | NAVI inherited MM020 while Toby was physically MM021. Subsequent actual MM022–053 polarity alignment was 32/32; assigned MM020–051 alignment was 17/32, yet all IR ratios were 0.858–1.126. Final operator MM053 versus NAVI MM051 is independent endpoint evidence. Recovery required a reference it could not establish while the incumbent journey disagreed. |
| T8: Otto MM045 investigation, Oct 9 | Isolates actual evidence: at the third reported stop NAVI remained MM044, approximately 8.372 mm short of the final distance bound; no ordinary braking/dwell/restart sequence followed until later dispatcher STOP. Identifies a code-association defect. | Hall support for target MM064 measured 250.952 mm against 325 mm, outside ±48.75 mm tolerance. Later discrepancies grew without new abort increments. Runtime source commit is not encoded in the log; the report inspected `0b2ee2b`, and its later preserved analysis/code is at `85a2cf2`. Physical cause and every assigned marker's true identity remain unestablished. |
| T9: October timing/map analysis | Locates code blindness during reference collection and independently useful DNA evidence. A ten-pole word is unique at all 171 positions in each known direction on the examined map. | The proposed timing ratios/persistence thresholds were fitted to limited same-day cases. A long interval is not a unique signature of a missed optical cycle. “Zero missed” runtime counters do not establish zero physical missed magnets. |
| T10: 0.3 correction | Reported host suite, parser/detector integration and ESP32 build pass. A deliberately interrupted open optical cycle is detected as unusable evidence; stale Type-6 expires despite continuing Type-5; both window edges govern qualification. | These are software/bench-style tests and timing replay, not track acceptance. A wholly undetected cycle can still leave consecutive counters. No raw-Hall DNA replay is claimed. The document explicitly says 0.3 is not ready for field testing. |

T7’s opening assertion that every Hall observation was real is broader than its own detailed evidence: it identifies three false rejected events. Its 32/32 argument concerns the accepted sequence, not all received observations. Preserve that distinction when reusing the report.

**Recovery is a separate outcome from initial correctness.** T6's successful sequence correction does not vindicate the false advance it repaired. Conversely, T7's failed recovery does not invalidate its successful distance rejection or reference acquisition. Counting only final correct position would hide these distinctions.

## 4. Contradictions, supersession and implementation gaps

### 4.1 Hall acquisition is not geographic identification

T3 preserves a genuine improvement from median-of-five acquisition. T4 and old EWO code retained that mechanism, but 0.3 uses raw 70-count/two-sample qualification; median-of-five is diagnostic there. Neither mechanism proves a new physical magnet. A continuous-field latch suppresses uninterrupted threshold support, but one below-threshold sample can reset it: subsequent same-field excursions can look new. Nonzero PWM plus elapsed time does not prove displacement. The stationary repeated-field failure cited by 0116 is historical evidence against treating fallback time as physical departure.

The Oct 9 core observes only expected-polarity support for target confirmation; opposite-sign evidence cannot correct identity through that path. During spatial collection it returns before Hall judgment, allowing an IR stall to suppress Hall interpretation indefinitely. The 0.3 core observes excursions during collection, a useful implementation improvement, but its distance qualifier rejects candidates **before** inserting them into the DNA encounter ledger. Thus “every encounter” means every **qualified** encounter, not every magnetic excursion. Raw telemetry availability is not equivalent to evidence availability inside the decision model. T1's evidence-preservation principle and T10's approved scoped filtering deserve explicit reconciliation in the next design; this briefing does not remove that filter.

### 4.2 IR integrity, accuracy and geographic applicability differ

0094 replaced trust probation with continuous instrument validity. Later 0116 stopped contrast/abort counters from automatically erasing coherent cumulative distance, after false stationary MM progress. That does not make a mathematically ordered stream physically accurate. T10 corrects an opposite mistake: treating long timing intervals as proven missed cycles. The healthy replay contains 136 timing questions and 17 missing-compatible brackets under the runtime algorithm.

T9's offline surrounding-interval probe and its suggested preceding-interval runtime detector are not identical algorithms; their aggregate counts are not interchangeable. Future observations used by an offline centered median cannot be available to a live causal decision. Proposed ratios around 5, persistence of four, or ±20% credibility bounds are **PROVISIONAL**, not replacements for the approved ±15% qualification.

A long interval can arise from speed variation, binding, slip or an undetected cycle. Without independent motion/phase evidence, T9's stronger assertion that each such explanation means IR ceased representing locomotive travel is not established. A fresh zero count is measured zero, not proof either of a stalled locomotive or of sensor failure. The uncoupled-car case is a concrete warning about applicability.

### 4.3 A plausible window can stabilize a wrong anchor

T7 establishes that wrong identities can have acceptable spacing. T8/T9 explain a further mechanism: the old core tests the latest decision-time IR counter, then anchors the accepted landmark using an earlier Hall-leading snapshot. The review identifies 18 confirmations whose anchor distance was outside the nominal window while decision lag admitted them; about 19–29 mm extra lag mattered. Re-anchoring can repeatedly reset residuals, making the incorrect journey appear locally coherent.

The corrected 0.3 qualifier captures the first supporting sample's window and coordinate, then decides on the second sample. This repairs the association mismatch; it does not establish the identity's correctness or eliminate reporting latency. Installed pitch is 9.652 mm and reports are roughly 10 Hz in the reviewed scenario; stored integer micrometres are not micrometre physical accuracy. At 245 mm/s, a 100-ms reporting lag is about 25 mm.

### 4.4 Continuity must permit correction without circular permission

T7's `ProximalRecovery` loses its reference on reversal, sets a settled state, and requires incumbent coherence to establish a new reference. It then refuses candidate evaluation for unavailable travel evidence. If the inherited incumbent is wrong, the evidence needed to correct it is blocked by requiring it to be right first. This is an **architectural deficiency supported by field evidence and code**, not merely a synthetic scenario.

0.5 instead searched alternate offsets after a disagreeing ten-observation word and recovered in T6. That historical success does not approve whole-map search for EWO: later documents reject unrestricted reseeding. Nor does rejection of map-wide reseeding prohibit every bounded interpretation of an already known journey. T9's local alternatives are proposals requiring explicit scope. T1's instruction not to manufacture alternative positions and its Observe/Hold/Resolve principle must be reconciled with any such proposal rather than silently reinterpreted.

### 4.5 Missed physical magnets, rejected observations and inferred positions

The old EWO `evaluateMissing()` advances mapped MM when IR passes the target plus tolerance; it retains the physical IR origin while expanding cumulative expected distance. That preserves an important anchor, but its single MM variable can nevertheless become an inferred geographic state. If IR or an earlier identity is wrong, apparent missing markers are not proof of physical Hall misses.

0.3 removes IR-only progression and comments that missing identities require retained magnetic sequence. Its `resolvePending()` actually considers only two treatments of a leading contradiction—count it as a sequential encounter with wrong polarity, or exclude it as an extra—followed by ten agreeing poles. It does not model arbitrary missing intermediate magnets, stale incumbents or multiple insertion/deletion patterns. Its 12-entry bounded history also constrains what can be resolved. Removal of false IR-only advancement is useful, but is not itself a completed missed-magnet architecture.

The older 0.5 code inserts expected map poles into mapped history for skipped points, while resetting its separate consecutive-observation window on a multi-step advance. This distinction matters: a filled map slot must never be counted as independent observed polarity confirming the map that supplied it.

### 4.6 Governing pages contain unresolved internal chronology

T2/T4 have October 10 amendments but older “last revised” dates and retained target-only/IR-dependent descriptions. 0116 contains a new IR-supporting-evidence paragraph followed by the earlier withdrawal of 650-ms Hall fallback. 0120 preserves that withdrawal for its handling scope. T10's 0.3 scope retains fallback. Do not choose the newest code as the winner or infer universal supersession from implementation. The next design needs an explicit applicability/supersession statement from David.

Supersessions that are clear: ten-magnet IR probation → 0094; ±10% early trial → later ±15% scoped windows; automatic PWM-zero reacquisition under 0119 → operator declaration under 0120; timing-anomaly automatic veto → diagnostic-only correction in T10. Supersessions that are **not** established globally: all Hall-only recovery policies; all missed-magnet policies; all reference acquisition mechanisms; all bounded geographic reinterpretation rules.

## 5. Recommended conceptual architecture — PROVISIONAL

These recommendations consolidate the evidence; they introduce no thresholds, governor, braking restriction, firmware change or approved new rule.

### Keep distinct facts and states

| Concept | Meaning and reason to retain it separately |
| --- | --- |
| Operator declaration | Asserted interval/direction and provenance; not an exact measured marker coordinate. |
| Last supported physical landmark | Mapped identity plus actual Hall-opening evidence and associated IR frame/counter; strength and contradictory evidence remain inspectable. |
| Current measured travel | Unedited pulse count and timing, with instrument integrity, observation age and applicability to this journey assessed separately. |
| Observed field history | Actual excursions, poles, opening/detection times and exclusions; includes contrary evidence rather than only observations already assigned an MM. |
| Provisional journey interpretation | Expected target, possible passed-but-unobserved map slots, and bounded competing explanations; none automatically supplies a new authoritative anchor. |
| Operational permission | Existing AUTO, STOP and declaration policies remain their own approved contract; uncertainty interpretation must not quietly redesign them. |

The proposed reasoning sequence is:

1. **Observe:** retain the magnetic and IR facts, their provenance, timing and exclusions. Distinguish repeated field support from evidence of departure and a new encounter.
2. **Constrain:** apply established direction, mapped sequence/spacing, credible travel, and relevant motor-control history. Physical reachability constrains geographic interpretations; PWM is contextual evidence, not a displacement measurement. Any new quantitative motion envelope needs its own evidence and approval.
3. **Compare explanations:** expected next magnet; same physical field/secondary lobe; an unobserved or previously rejected marker; wrong earlier assignment; suspect IR; or still unresolved evidence. Consider alternatives because a specific contradiction warrants them, not as unrestricted map reseeding.
4. **Hold or resolve:** preserve the last supported anchor while an event remains ambiguous. When evidence supports a correction, state which prior assignment changed and why. Anchor commitment is an explicit act with an evidence trail, not a side effect of selecting a candidate.
5. **Continue checking:** normal distance agreement does not switch DNA scrutiny off. Report initial correctness, contradiction recognition, false-extra exclusion and eventual recovery separately.

“Last supported” is revisable, not infallible. Holding its evidence must not reproduce T7's requirement that the incumbent first become perfectly coherent before correction can be considered. Conversely, choosing a minimum-edit alignment or an attractive alternative must not manufacture a new distance origin before identity is supported. The supplementary pattern audit reinforces this methodological point but supplies no independent track validation.

### Cases the specification must explain

| Case | Design obligation, without selecting a new algorithm |
| --- | --- |
| Reliable 20 mm travel; next mapped magnet about 300 mm away; opposite Hall excursion | Explain why a genuine field observation is not enough for next-marker identity. Preserve the observation and its exclusion; no long polarity word is needed merely to notice the reachability mismatch. |
| Distance inside the window; polarity conflicts with the incumbent | Retain both facts; T7 shows the window cannot overrule accumulated geographic contradiction. Specify when later evidence may correct an earlier assignment. |
| Threshold reappears after a brief dip while movement is unproven | Explain physical encounter separation; timer expiry and latch reset alone are insufficient evidence of a new magnet. |
| Ordered pulses, unusually long interval | Preserve timing as a question; do not label physical missed pulses without adequate evidence. Distinguish instrument observability from map-distance applicability. |
| No received Hall at an expected location | Distinguish no field detection, rejected candidate, reference-phase suppression and missing telemetry. An inferred map slot carries no invented measured pole. |
| Reversal with a possibly wrong incumbent | Preserve relevant evidence and explain how correction avoids the reference/coherence circularity while obeying established reversal and declaration rules. |
| First target after interval declaration | Preserve 0118's lack of an exact distance origin and document residual same-pole ambiguity. Any replacement needs explicit decision authority. |

## 6. Decisions still needed before implementation

1. **IR authority:** what evidence establishes acceptable physical distance integrity, especially entirely undetected cycles? Which invalidity withdraws only a distance veto, and which destroys the geographic relationship? Current freshness/continuity tests cannot answer all of this.
2. **Fallback scope:** reconcile the retained 650-ms 0.3 rule with 0116/0120 withdrawal and the later Hall/DNA/reachability direction. This is a policy decision, not permission to change a constant during implementation.
3. **Evidence admission:** distinguish an observation retained for contradiction analysis from an encounter qualified to advance position. Resolve how out-of-window genuine observations remain usable without weakening the approved advancement criterion.
4. **Anchor commitment/correction:** specify support needed to create or revise an anchor, including startup, wrong incumbent, missed markers and reversal. Avoid both premature re-anchoring and frozen incorrect continuity.
5. **DNA scope:** define mapped slots versus actual observations, permitted bounded alternatives, multiple anomalies, wraparound and history limits. Ten-pole uniqueness assumes correct direction and ten consecutive genuine markers; it is not a ten-event universal recovery guarantee.
6. **Reference/encounter separation:** establish spatial sample coverage, continuous observation during reference work, physical field-departure evidence and the status of historical timeout proposals.
7. **Validation:** draft event-by-event expected judgments for T6–T8, then use causal replay where adequate raw signals exist and focused track tests where they do not. T9 acknowledges insufficient native Hall/optical evidence for a faithful DNA replay of the Oct 9 mechanism. Measure physical ground truth independently of NAVI's inferred MM.

A coherent next specification should settle these scopes before thresholds or another sketch are selected. Existing valid operator holds and STOP behavior are not reopened by this analysis. No pattern change is supported by the ten-source investigation.

## 7. Supporting source verification and provenance

Code inspection established implementation behavior, not physical validation. The following immutable references support the consequential comparisons:

- **0.5 actual logic:** [MovementEvidence.h](https://github.com/ninobur/esp-loco-control/blob/2deadba7998fef663ed06171aa77e88a7d7a8833/firmware/test-programs/NAVI_COHERENCE_0_5_AUTO_ENABLED/MovementEvidence.h), `between()` requires both endpoints TRACKING and unchanged interruption counters; [Navigator.h](https://github.com/ninobur/esp-loco-control/blob/2deadba7998fef663ed06171aa77e88a7d7a8833/firmware/test-programs/NAVI_COHERENCE_0_5_AUTO_ENABLED/Navigator.h), `observe`, `acceptThrough`, `scoreAt`: timer fallback, cumulative distance search, inserted map slots and ten-observation sequence correction. The stale ±10% comment is contradicted by the actual `IR_WINDOW_FRACTION=0.15` constant.
- **Proximal recovery:** [ProximalRecovery.h](https://github.com/ninobur/esp-loco-control/blob/93af2267f1a4609edeca57aecec4b69278e6097d/firmware/programs/NAVI_COHERENCE/variants/NAVI_EYES_WIDE_OPEN/ProximalRecovery.h), blob `6af514060d317a6b6825fb3cbea54b6a9908d30e`; `reverse()` and `evaluate()` establish reference loss, settled state and incumbent-coherence dependency. The full same-content module was inspected for T7's recovery explanation.
- **Oct 9 EWO:** [NaviIntegratedCore.h](https://github.com/ninobur/esp-loco-control/blob/85a2cf2a16d262abdf7258c60f60508b823913ee/firmware/programs/NAVI_EWO_0_1_MM045_STOP/NaviIntegratedCore.h), blob `a1f9cec749c35a81c61688eeaee7cb41ddcfb353`; full module inspected. `observeHall`, `confirm`, `collectSpatial`, `evaluateMissing`, and `reverse` support target-pole filtering, collection blindness, coordinate mismatch, inferred MM progress and frame handling.
- **Corrected 0.3:** [NaviIntegratedCore.h](https://github.com/ninobur/esp-loco-control/blob/c4cd2b2b8edd5641a0c4a7a31191614f7cba153e/firmware/programs/NAVI_EWO_0_3/NaviIntegratedCore.h), blob `0c0a63611e093a6241e5c4ec05364272677b3d2e`; targeted inspection of `observeHall`, `observeIr`, `observePulse`, `irBoundaryAt`, `irMagnetVetoAvailable`, `beginDecision`, and `resolvePending`. The earlier local core at `253faa4` was distinguished from this follow-up; it is not cited as the time-required version.
- **Qualification and evidence:** [NaviEvidenceCore.h](https://github.com/ninobur/esp-loco-control/blob/c4cd2b2b8edd5641a0c4a7a31191614f7cba153e/firmware/programs/NAVI_EWO_0_3/NaviEvidenceCore.h), blob `edc5bd8212382e318c4b770bc59f70e47872f18e`; [NaviEvidenceBridge.h](https://github.com/ninobur/esp-loco-control/blob/c4cd2b2b8edd5641a0c4a7a31191614f7cba153e/firmware/programs/NAVI_EWO_0_3/NaviEvidenceBridge.h), blob `d1514df7a3a8a5e6907673a445e4e0006030ceaf`; [NaviIrEvidence.h](https://github.com/ninobur/esp-loco-control/blob/c4cd2b2b8edd5641a0c4a7a31191614f7cba153e/firmware/programs/NAVI_EWO_0_3/NaviIrEvidence.h), blob `eb16aa2db70ccd6f5cb3d3341e23ca6c66567523`. Full modules inspected: opening/decision distinction, continuous latch, pre-ledger qualification, 12-entry history and diagnostic pulse ratios.
- **Mapped DNA:** [RouteMap.h](https://github.com/ninobur/esp-loco-control/blob/17e8ed116e8efd90d5396f450885a0073916bf7f/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h), blob `4bbd054e9983c6800111b240bae59ef611915d9a`; [preserved verification](research/map_verification.json). There are 171 markers, 52,264 mm total mapped loop, 280–360 mm spans. Unique-position counts for windows of 7/8/9/10 are 65/129/163/171 in each known direction. These are map arithmetic properties, not field reliability rates.
- **Further authority checked in full:** decisions 0113 and 0118 linked above, [0116](https://github.com/ninobur/esp-loco-control/blob/cbdfe9495ca1b52d2165a5ce6be27cbfce1d1439/docs/decisions/0116-ewo-ir-distance-is-authoritative-and-hall-only-navigation-is-withdrawn.md) and 0120. Retained annotations were treated chronologically rather than as simultaneous consistent instructions.

Original experimental records are linked within T6–T10: `field-records/logs/20260922_navi_coherence_0_5_auto_run.log`, `field-records/logs/20260926_toby_20q3_run3.log`, and `field-records/logs/20261009_MM045_EVENING/all_20261009.log.gz`. T8 records the October log SHA-256 as `0946fc37d8bd4b053e4c34e47f565483d3abf6c972b8ee24d255fea1449b2f79`. They remain the next level of evidence, not newly completed full-log reviews in this focused assignment.

### Exact selected content identities

| Source | Git blob SHA-1 |
| --- | --- |
| T1 | `6a4868853ab127f55dbd2a2172eae296e322800a` |
| T2 | `bd710ed9434973bc75990513c3bc41e9d4ce48c4` |
| T3 | `c489c3006f88eec7008cb4a3a996f1b1c723261a` |
| T4 | `a4e2c510950f0febb1ec49668bdf451b49f74dbb` |
| T5 | `dc746db67716fe995928803aae19ed352ba60d37` |
| T6 | `059fc28427a0d4c97d04a23b6d063fb5631f30f6` |
| T7 | `c9356bd94e0aad68e498152d4355ca2e8663ba9c` |
| T8 | `7c64e9b7fb49614f4f259192158f50a1cc0eae42` |
| T9 | `b195e1bca03997d70736cb9d5bdaa4802f66d685` |
| T10 | `b0cc43ea77c14c78a8eb8ab2027f1c386fafca9f` |

**Handover:** this briefing is adequate for a focused architectural reconciliation session, with original evidence reachable and proposals visibly separate from authority. It is not a finalized navigation specification, field acceptance, or exhaustive engineering history. Future approved decisions should explicitly identify the contradictory scopes they replace; new evidence should correct interpretations without deleting the original reports. Only this analysis document is added; firmware, governing specifications and the exhaustive research ledger remain unchanged.
