# Architectural reconciliation

19 October 2026. Research only. Governing source files and all firmware remain
unchanged. The operator's assignment explicitly authorizes this independent
offline architecture, not its promotion into NAVI.

## Trace → Discover → Complete → Recognize

`prepare.py` validates NSR CRCs and preserves native acquisition samples.
`Baseline` in `cpr.py` predicts a local level and noise from admitted past
background. `Segmenter` opens a provisional event after a sustained departure,
retains prehistory, and freezes the baseline during development. It waits for
sustained return, then emits one completed object. `describe` measures that
object. `Population` evaluates its complete curve against empirical training
variation. It never receives MM identity, direction, IR, PWM, a route map, or a
geographic prediction. Geographic identifiers in `experiment.py` are evaluation
annotations and never enter these components.

A fixed-duration persistence test is not a fixed-duration classification window:
event duration is discovered from the signal and can vary up to the memory
limit. No competing baseline class, multiscale window scan, failure taxonomy,
motor response, geographic advancement, or model adaptation is implemented.

## Relationship to existing intent and authority

| Source reviewed | Relationship and unresolved issue |
|---|---|
| Operator metaphors compendium, `docs/NAVI_RVM_OPERATOR_METAPHORS_ENGINEERING_COMPENDIUM_20261017.md`, at `9e7b920` | Whole-beat metaphor supports preserving principal/secondary lobes, shoulders and recovery. Frozen empirical families implement tolerance rather than one ideal curve. The printed-board/zipper metaphors also require persistent geographic context; CPR does not implement that later stage. Recognizing individual completed events alone is not complete RVM conformance. |
| Architecture/M1 and authority audit F01 | CPR reports absent/incomplete evidence but has no authority to fabricate a magnet, skip geography, stop a locomotive, or decide a defensible location. Horizon expiration is incomplete. The geographic missing-evidence policy remains outside scope and unresolved. |
| Audit F02 | Per-sample departure and baseline return are provisional measurement machinery. There is no catalogue of observation failures or five-threshold collective-stop substitute. Event scores have no motion authority. |
| Audit F06 | There is no geographic alignment frontier or trusted anchor to corrupt. The learned population is immutable during replay. This avoids that path in this isolated experiment; it does not solve trusted-history integration. |
| M1 appearance timing vs current CPR completion timing | Material scope difference: the earlier spec permitted expected-MM commitment while a piece developed. This assignment requires completion before recognition. That is authorized for this experiment; it does not amend M1 or operational commitment timing. Any integration needs the operator's explicit disposition. |
| Independent shape comparison, `research/statistical-shape-20261018` at `804f1a2` | Normalized shape alone generated many quiet-baseline detections; arbitrary windows did not establish event extent. CPR does neither a window scan nor a magnet-vs-baseline likelihood ratio. |
| 70-ADC repeat at `8c40d6d` | Restoring physical amplitude removed baseline-contained proposals on those reused records, but did not validate complete boundaries. CPR retains amplitude separately and preserves a two-sample acquisition witness. The earlier repeat's single maximum-amplitude test is not equivalent to the governing two-sample requirement. |
| Whole-ensemble research at `cac8724`, especially REPORT.md and ARCHITECTURE.md | Joint continuous fitting retained useful principal/secondary landmarks but failed the frozen complete-unit test. Statistical state partitions are not physical field boundaries. CPR uses that as a warning, not as a validated reference implementation. |
| `docs/navi_rvm_manual/REVIEW.md` at `6aea978` | The stopped draft demonstrated geographic drift despite retained matching history. It is distinct from the October 19 continuity-manual delivery. CPR never imports its alignment kernel. |
| October 19 continuity-manual delivery and recognizer review at `9e7b920` | Delivery proves specific replay/firmware parity, not general magnetic truth. The recognizer review documents fixed-width classification and unsupported 12-ADC tail likelihood. CPR's segmentation and one-class population distance directly investigate those concerns. No continuity block withholding or PWM time compression is imported. |

The Grok/Gemini/DeepSeek proposals are summarized in the assignment; their full
texts and executable claims were not supplied. No results from them are treated
as validated evidence or governing requirements.

## The 70-count/two-sample coexistence proposal

`NativeRule` evaluates supplied original acquisition samples against the causal
baseline available before their 20-ms block. Two consecutive same-polarity
departures of **at least 70** produce a detection witness at the **second native
timestamp**. Serial discontinuity resets confirmation. Repeated threshold
crossings can produce multiple witnesses, but witnesses never declare magnetic
objects or advance geography. One completed object yields at most one recognition
per alternative recognizer. The three alternatives are experiments, not three
independent recognition publications.

Discovery may open below 70. Recognition requires both a completed event containing
a native witness and an in-population score. This preserves the requested
detection floor while keeping statistical discovery provisional. A completed weak
object is still logged even if it cannot qualify. Normalization never changes
the physical amplitude or the witness.

The reference does not silently equate raw-native and historical filtered sample
semantics. The exact original sample filter and historical baseline may differ.
Its witnesses use the new causal CPR baseline, held constant within each block;
the offline consumer receives the block at its end. Witness timestamps are not
a target latency measurement. Operator approval is required before wiring this
coexistence proposal into the established detector, changing sample semantics,
or allowing any weaker event to obtain geographic authority.

## Known component limits, preserved without repair

The robust linear alternative extrapolates frozen slope through long candidates;
it can drift far from the real background and remain unable to rearm. EMA avoids
that particular runaway but can absorb slowly changing weak signal and cannot
resolve a persistent new baseline from an unfinished magnetic plateau. Toby
exposes that ambiguity directly. Startup and recording-loss recovery require
one second of warmup, whose baseline identity is unverified. That blind interval
must not become a hidden success criterion.

Return persistence alone cannot distinguish secondary lobes from sufficiently
close independent fields. The synthetic close-pair case demonstrates merging.
Actual quiet gaps, slow recovery and baseline curvature require independently
reviewed field ownership. No post-evaluation rescue rule was added.

The one-class distance can abstain when an event is unlike the learned population;
it does not output a calibrated probability of magnetism. A 99th-percentile
positive-calibration threshold is not a controlled false-recognition rate. The
learned amplitude contribution still rejects a legitimate synthetic 78-ADC case;
retaining amplitude has not solved the weak-magnet population problem.
