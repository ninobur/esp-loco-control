NGR NAVI_SIMPLIFIED — DESIGN PROSPECTUS
1. Purpose
The next NAV restores the navigation judgment deliberately removed from NAVI_ONE.
NAVI_ONE was a developmental instrument. It intentionally reduced navigation to a severe test:

* NAV knows its current marker.
* NAV knows direction.
* NAV determines the expected next marker.
* A detection with expected polarity advances NAV.
* A detection with unexpected polarity produces a strike.

That architecture was useful because it exposed disagreements rather than explaining them away. It is not the intended production navigation architecture.
The next NAV must evaluate an unexpected observation before changing what it believes about locomotive position.
The governing idea is:
I know where I am. Something I just observed does not make sense. Before changing what I know about my position, determine whether that observation is real and what it can mean.
More simply:
If it cannot be the next magnet, ignore it and continue looking for the real magnet ahead. If it can be the next magnet, evaluate it and continue.
The first build is not intended to create a clever black box. It is intended to create a conservative, observable navigation system whose decisions can be understood and evaluated on the actual railway.
2. Core Architecture
The next NAV has three conceptually separate stages.
2.1 Detection
The Hall detector answers:
Did a qualifying magnetic opening occur?
It reports:

* detection time;
* opening polarity;
* associated acquisition data.

It does not decide where the locomotive is.
2.2 Hard Protection
The hard-protection layer answers:
Could the locomotive physically have reached another mapped magnet since the previous accepted detection?
If physics says no, the observation cannot represent the next mapped magnet.
It is suppressed, with full telemetry.
This is where same-magnet protection belongs.
Hard protection should use deliberately conservative physical constraints, not fragile estimates.
2.3 Transparent Judgment
If another mapped magnet could physically have been reached, the observation enters NAV’s judgment system.
NAV asks:
This observation is physically possible, but does it make sense given everything else I know?
NAV may now consider useful but imperfect evidence such as:

* polarity;
* locomotive-clock timing;
* recent measured speed;
* PWM;
* PWM history;
* acceleration/deceleration;
* map sequence;
* recent observations;
* stop/restart state;
* direction;
* competing position hypotheses.

The reasoning and resulting action must be visible.
The central architectural distinction is:
Hard protection uses what we know cannot happen. Judgment uses evidence about what probably did happen.
Do not allow probabilistic or estimator-derived evidence to migrate silently into the hard-protection layer.
3. Development Philosophy
This is a prospective development effort.
Historical telemetry remains useful for:

* discovering possible failure mechanisms;
* understanding earlier architectures;
* identifying useful information sources;
* identifying instrumentation deficiencies;
* generating hypotheses;
* identifying questions the new NAV should answer.

Historical telemetry from mixed architectures must not be treated as ground truth for optimizing or scoring competing new navigation algorithms.
The development cycle is:
DESIGN → INSTRUMENT → RUN → OBSERVE → EXPLAIN → REVISE → RUN AGAIN
The actual railway running the actual new architecture should generate the primary evidence used to improve it.
Do not tune thresholds against heterogeneous historical logs.
4. Evidence Status
CODEX must preserve four categories.
4.1 Established architectural decisions
These are starting constraints.
They may be superseded, but only after explicit justification, discussion, and operator approval.
They must not be silently replaced during implementation.
4.2 Verified engineering facts
Examples include:

* source-code behavior;
* RouteMap contents;
* mapped distances;
* locomotive-clock timestamps;
* physical dimensions;
* independently verified map arithmetic;
* information demonstrably available onboard.

These may directly inform implementation.
4.3 Engineering hypotheses
These may inform instrumentation and experimental design but must not silently become navigation rules.
Examples include:

* proposed `r` thresholds;
* correlations found in retrospective telemetry;
* estimated prevalence of particular failure mechanisms;
* morphology signatures;
* proposed speed-estimator boundaries.

4.4 Open questions
Where the design record does not establish an answer, CODEX must expose the question rather than invent one.
5. X18–X21 Detector Inheritance
5.1 Opening detection — TWO samples
The standing detector rule is:
A Hall departure of at least 70 counts for two consecutive 1-kHz samples constitutes detection. The second qualifying sample declares detection.
Source history confirms that every relevant flown X19–X21 implementation used two samples.
The later:
`overCount_ >= 5`
appears in untracked, unflown `NAVI_ONE_SIMPLE` code without a recorded architectural justification.
Therefore five samples is not an open alternative for this build.
Changing from two samples requires change control.
The 70-count threshold itself is an inherited engineered threshold, not a physical constant. It remains subject to future evidence and change control.
5.2 Opening polarity
Capture polarity at opening/detection.
The sign of the opening excursion supplies the polarity observation presented to NAV.
X18–X21 found opening polarity more trustworthy than polarity inferred later from the whole waveform.
5.3 No morphology authority
NAV must not determine whether an observation is a magnet from:

* waveform shape;
* width;
* duration;
* rise/fall behavior;
* area/integral;
* peak;
* residual;
* lobes;
* apparent physical width;
* or similar morphology.

These quantities may be recorded diagnostically.
Recording a quantity does not grant it navigation authority.
5.4 No closure requirement
A magnet does not have to “close” or produce a completed passage before it becomes navigation evidence.
Closure belonged to the discarded morphology architecture.
Navigation must not wait for closure.
5.5 Immediate opening event
Once the two-sample opening criterion is satisfied, detection and captured opening polarity are available immediately.
NAV must not wait for the subsequent waveform before learning that detection occurred.
5.6 Diagnostic waveform
Continue recording approximately 400 ms of Hall behavior after detection where practical.
This is valuable forensic telemetry.
It has no authority to revoke or redefine the original opening detection.
5.7 No stitching
Do not stitch separate Hall activity together to manufacture a single passage.
This is particularly important across stopping and restarting.
5.8 Stationary suppression
Actual/ramped PWM zero must not manufacture navigation events.
A candidate that straddles the transition into the stopped state must not subsequently become a navigation advance.
5.9 Stop-on-magnet behavior
A locomotive can stop while physically within the field of the previously encountered magnet.
Restart logic must prevent that same physical magnet from being counted again.
5.10 Same-magnet protection begins at detection
Protection associated with the just-detected physical magnet begins when detection occurs.
It does not wait for a closure boundary.
Protection remains applicable while physical evidence says another mapped magnet could not yet have been reached.
This is a reachability problem, not a passage-closure problem.
Do not introduce signal return-to-baseline, waveform completion or an equivalent disguised closure criterion to terminate same-magnet protection.
5.11 Detection and judgment are separate
The Hall detector reports approximately:
A qualifying ≥70-count, two-sample magnetic opening occurred at locomotive time T with opening polarity P.
NAV determines what that observation means in position and map context.
5.12 Diagnostic information remains diagnostic
Peak, waveform, duration, morphology, residuals, closure-like diagnostic values and similar measurements may help explain failures after a run.
They do not acquire navigation authority merely because they are available.
6. Hard Physical Reachability
The old 645-ms guard must not simply be inherited.
Its history is informative, but the new architecture should implement the underlying physical principle directly:
If the locomotive could not physically have reached the next mapped magnet, the observation cannot be the next mapped magnet.
For mapped distance `D` and a defensible conservative physical maximum velocity `Vmax`:
`minimum_possible_dt = D / Vmax`
If:
`elapsed_dt < minimum_possible_dt`
then the observation is physically incapable of representing the next mapped magnet.
It belongs to hard protection.
6.1 Vmax requirement
Do not define `Vmax` merely as the fastest speed ever observed in historical telemetry.
Historical locomotive-clock telemetry has observed speeds up to approximately 417 mm/s, but this is only a lower bound on whatever true physical ceiling is chosen.
Prefer deriving a conservative kinematic upper bound from physical locomotive characteristics where practical, such as:

* motor maximum/no-load speed;
* gearing;
* wheel circumference;
* relevant electrical limitations.

Apply an appropriate engineering margin.
The purpose is not to estimate likely speed.
The purpose is to establish a speed the locomotive cannot plausibly exceed.
The hard test must err toward allowing an uncertain observation into judgment rather than falsely declaring a physically possible observation impossible.
6.2 Detector refractory and physical protection are different
A short detector-level refractory period may still be appropriate for electrical/acquisition reasons.
If used, it must be separately identified and justified.
Do not conflate:

* detector refractory behavior;
* same-magnet physical protection;
* discrepancy judgment.

7. Safeguards Before Judgment
Known preventable problems should preferably be stopped before they become higher-level navigation discrepancies.
Examples include:

* stationary manufactured events;
* immediate detector retriggering;
* rereading the magnet on which the locomotive stopped;
* detections occurring before another mapped magnet could physically have been reached.

However, safeguards must themselves be observable.
Whenever a qualifying Hall observation is suppressed, telemetry must record:

* that the observation occurred;
* boot ID;
* event serial;
* locomotive timestamp;
* observed polarity;
* current PWM;
* relevant timing;
* safeguard invoked;
* reason for suppression.

The absence of a navigation event must be distinguishable from the successful rejection of one.
8. Motion Evidence
Once an observation has passed hard protection, NAV may use broader motion evidence.
Potential evidence includes:

* locomotive-clock detection-to-detection `dt`;
* mapped distance;
* recent marker-to-marker measured speeds;
* current PWM;
* recent PWM history;
* whether PWM is increasing, steady or decreasing;
* direction;
* stop/restart state;
* time since previous accepted observation.

No single quantity should initially be assumed infallible.
8.1 Measured speed
Marker-to-marker `dt` provides a real recent motion measurement.
Its weaknesses include:

* it describes recent rather than necessarily current motion;
* acceleration/deceleration can make it stale;
* a displaced navigation count can corrupt the map distance used to convert `dt` into speed.

Therefore measured speed is evidence, not hard physical truth.
8.2 PWM
PWM supplies an independent estimate of expected motion.
It is not actual speed.
Grade, load, battery condition, locomotive characteristics and acceleration state affect the relationship.
Historical data also demonstrate that speed is not simply monotonic with commanded PWM across all railway conditions.
PWM remains useful evidence precisely because its weaknesses differ from those of marker-derived speed.
8.3 Combined evidence
Agreement and disagreement among motion estimates may themselves be informative.
Do not prematurely reduce motion evidence to one ratio.
A derived value such as:
`observed_dt / expected_dt`
may be recorded and prospectively evaluated.
It must not automatically become the navigation algorithm.
8.4 Slow-speed evidence
Evidence that the locomotive has been moving slowly may strongly support a same-magnet interpretation after the hard physical boundary has expired.
That evidence belongs in judgment, not in the hard-impossibility layer.
Hard protection must not claim impossibility merely because a recent speed estimator or PWM model predicts slow motion.
9. Normal Navigation
Normal navigation should remain simple.
NAV begins from a declared known marker and direction.
Therefore it knows:

* current believed marker;
* direction;
* mapped next marker;
* mapped spacing;
* expected polarity.

When the next observation agrees with expectation and no safeguard or other contradiction exists, normal navigation should advance without invoking elaborate discrepancy reasoning.
Exceptional reasoning should not make ordinary navigation unnecessarily complicated.
10. Direction Consistency
Direction is navigation evidence and must not be accepted blindly from one state variable.
The implementation must inspect how the existing system derives:

* session direction;
* commanded motor direction;
* NAV direction.

A contradiction among these must be observable and treated as a navigation discrepancy rather than silently ignored.
A known historical failure involved AUTO beginning with motor motion opposite to the direction implied by `sessionDir`.
CODEX should specifically inspect the existing direction derivation before implementation.
11. Discrepancy Evaluation
When an observation conflicts with expectation after passing hard protection, NAV must not immediately conclude that position has been lost.
The observation is physically possible.
Now NAV must determine what it means.
Relevant evidence may include:

* elapsed locomotive-clock time;
* mapped distance;
* recent measured speed;
* PWM-derived motion expectation;
* PWM trend;
* acceleration/deceleration state;
* stop/restart state;
* observed polarity;
* expected polarity;
* recent accepted sequence;
* direction;
* map sequence;
* competing position hypotheses;
* other onboard evidence whose relevance is justified.

The system must not be an opaque classifier.
12. Possible Failure Mechanisms
The following are hypotheses useful for design and instrumentation.
They are not a mandatory decision tree.
Event-level possibilities

* same physical magnet detected again;
* stop/restart re-entry into old magnet field;
* stationary manufactured event;
* real magnet missed;
* one physical magnet generating multiple openings;
* spurious magnetic observation;
* event/queue delivery failure;
* incorrect opening-polarity observation.

Navigation-state possibilities

* NAV previously advanced when it should not have;
* NAV previously failed to advance when it should have;
* seeded/declaration position was incorrect;
* direction state is incorrect;
* present observation is legitimate but NAV count is displaced.

Reference/detector possibilities

* inappropriate baseline;
* baseline drift;
* baseline starvation or inability to update appropriately.

Physical-world possibilities

* track magnet moved;
* magnet weakened or disappeared;
* additional magnetic source exists;
* locomotive was physically handled or repositioned without redeclaration.

These possibilities should remain alternatives the model can consider rather than classifications imposed before evidence exists.
13. Do Not Hard-Code Retrospective Classifiers
Do not implement rules merely because retrospective analysis proposed them.
Examples of values that are not currently approved architectural constants include:

* `r < 0.4`;
* `r < 0.55`;
* `1.55 <= r < 2.55`;
* PWM ≤ 30 as a universal discriminator;
* amplitude near 70 versus 150;
* particular peak or spread boundaries.

These may be logged, displayed or prospectively evaluated.
They are not established NAV truth.
14. Transparent Reasoning Requirement
This is a primary requirement.
There must be no opaque correction.
For every significant discrepancy, telemetry must make it possible to reconstruct:

1. what NAV believed before the observation;
2. what NAV expected;
3. what was observed;
4. what evidence was available;
5. what alternative explanations NAV considered;
6. what evidence supported or contradicted those alternatives;
7. what conclusion NAV reached;
8. what action NAV took;
9. whether ambiguity remained.

Conceptually:
OBSERVATION → EVIDENCE → ALTERNATIVES → EVALUATION → DECISION → ACTION
If NAV cannot distinguish two plausible explanations, that fact should be visible.
If NAV maintains multiple hypotheses temporarily, their evolution must be visible.
A correct outcome for an incorrect reason is not considered a fully successful developmental decision.
15. Temporary Ambiguity
NAV may encounter an observation for which available evidence does not immediately support one unique explanation.
The architecture must not force CODEX into only:

* guess; or
* stop.

It should permit temporary ambiguity if justified.
Earlier QUORUM work demonstrated the general usefulness of retaining competing position hypotheses.
The next NAV does not have to reproduce QUORUM.
However, its architecture should not prevent later retention of two or more plausible interpretations and use of subsequent observations to resolve them.
Any such mechanism must remain transparent.
15.1 Station behavior during ambiguity — OPEN OPERATOR DECISION
The prospectus does not currently specify what authority NAV has to arm or execute a station approach while its position remains unresolved.
CODEX must not invent this operating policy.
Before implementing behavior that affects station stopping during unresolved navigation, present the alternatives and obtain operator approval.
16. Map Polarity and Count-Error Visibility
RouteMap polarity is useful evidence but is not an instantaneous absolute-position code.
Independent arithmetic on the existing map shows that for the analyzed ±1 and ±2 count offsets:

* median polarity-detection latency: 1 marker;
* 90th percentile: 3 markers;
* worst case: 7 markers, approximately 2.15 m.

The worst case is associated with the long same-polarity region around the Arches area; an offset acquired around MM106 can remain polarity-consistent through multiple subsequent markers.
This matters because Arches is also a station-approach region.
These figures describe the structural behavior of the existing polarity assignment.
They do not imply that NAV should limit ambiguity to three markers merely because that is the 90th percentile.
16.1 Alternative polarity map
Independent verification of a candidate 171-marker polarity sequence has established that a map exists with:

* circular maximum run: 4;
* all 171 ten-marker circular windows unique;
* 159 unique nine-marker windows;
* all 171 eleven-marker windows unique;
* N/S balance 84/87.

Therefore the current seven-marker blind region is a property of the present polarity assignment, not a mathematical necessity of a 171-marker unique-ten-window map.
This is an engineering fact, not a recommendation to change the physical magnets.
Any future map redesign should optimize explicit objectives, including the cost of altering existing magnets, rather than adopting an arbitrary candidate merely because it satisfies these properties.
17. Wayside Absolute Position Fixes
A future/parallel architecture is planned using trackside ESP devices with Hall sensors.
Locomotives can carry magnet identity patterns.
A wayside unit may therefore establish independently:

* locomotive identity;
* absolute known wayside location;
* observation time.

Conceptually:
`TOBY observed at ARCHES_FIX at T`
This information is categorically different from onboard relative navigation.
It is an independent absolute observation.
The next NAV should provide an interface for receiving such fixes even if they are not implemented during the first flight.
A wayside fix should not be required for competent onboard navigation.
Its intended roles include:

* synchronization;
* correction when appropriate;
* independent ground truth;
* validation of NAV’s internal reasoning.

17.1 Two-magnet identity
Two ordered polarity magnets provide four patterns when direction is independently known:

* NN;
* NS;
* SN;
* SS.

If direction is unknown, reversal makes NS and SN equivalent.
Therefore two magnets provide only three reversal-distinguishable identities without independent direction information.
17.2 Two-sensor wayside possibility
A wayside unit with two Hall sensors at known separation deserves investigation.
Sensor firing order could independently establish direction.
Timing between the sensors could also provide an independent wayside speed measurement.
If direction is established independently, the four ordered two-magnet identity codes become distinguishable.
This is a design possibility, not yet an approved hardware specification.
17.3 Physical interference checks
Before deployment verify experimentally that:

* locomotive identity magnets do not interfere with the locomotive’s onboard Hall sensor;
* nearby track magnets do not falsely trigger the wayside identity detector;
* sensor and identity-magnet geometry provides reliable discrimination in both directions.

18. Instrumentation Requirements
Instrumentation is part of the architecture.
Critical physical timing must use the locomotive’s own clock.
Broker receive timestamps must not substitute for locomotive event timing when evaluating movement.
18.1 Event identity
Every relevant Hall event must have a composite identity:
`(boot_id, event_serial)`
`event_serial` alone is insufficient because it resets after reboot.
The same identity must correlate:

* acquisition;
* suppression;
* NAV judgment;
* discrepancy reasoning;
* action;
* diagnostic records.

Do not depend on approximate timestamp matching to reconstruct one event across topics.
18.2 Hall-opening record
For each qualifying Hall opening preserve at minimum:

* boot ID;
* event serial;
* `opened_ms`;
* open-to-open `gap_ms`;
* opening polarity;
* raw opening value;
* baseline at opening;
* PWM at opening;
* direction;
* whether event reached NAV;
* whether event was suppressed;
* suppression reason.

18.3 NAV judgment record
For each NAV judgment preserve:

* boot ID;
* event serial;
* pre-event believed MM;
* expected target MM;
* expected polarity;
* observed polarity;
* direction;
* mapped spacing;
* relevant recent `dt` values;
* recent speed estimate(s);
* PWM and useful PWM history;
* motion state if calculated;
* safeguards evaluated;
* discrepancy evidence actually used;
* alternatives considered;
* ruling/decision;
* resulting MM/state;
* action taken.

18.4 Suppression reliability
Suppression records must not depend exclusively on a lossy diagnostic publication path.
The current architecture contains queue-space-dependent diagnostic publication.
If a suppression record can be dropped, the system must still make that loss detectable.
At minimum maintain monotonic counters, preferably per suppression reason, together with publication/drop counters.
The telemetry must allow us to distinguish:

* no event occurred;
* event occurred and was suppressed;
* suppression record itself was dropped.

18.5 Dedicated discrepancy telemetry
Do not assume the existing `PubMsg` payload can contain the complete reasoning record.
Current payload limits are approximately 704 bytes.
Use a dedicated discrepancy topic or equivalent structure keyed by `(boot_id,event_serial)`.
Multipart records are acceptable if necessary, provided they can be reconstructed deterministically and missing parts are detectable.
18.6 Queue/drop accounting
Preserve and expose queue/drop counters.
A missing record must not masquerade as missing physical activity.
19. Diagnostic Telemetry
Diagnostic telemetry may be richer than navigation telemetry.
It may include:

* approximately 400-ms Hall waveform;
* peak;
* spread;
* residual;
* closure-like diagnostic timestamps;
* waveform morphology;
* baseline behavior;
* other experimental measurements.

These are intentionally useful during post-run forensic analysis.
The boundary is explicit:
Morphology has no NAV authority. Morphology is nevertheless intentionally available for post-run explanation of what physically occurred.
CODEX should maintain a clear source-level distinction between:
NAVIGATION EVIDENCE
and
DIAGNOSTIC-ONLY EVIDENCE
so future edits cannot casually cross that boundary.
20. First-Flight Objective
The first flight tests the architecture, not every conceivable recovery strategy.
Primary objectives:

1. normal magnets are detected and navigated correctly;
2. the two-sample detector behaves as specified;
3. known safeguards prevent obvious false navigation advances;
4. hard protection correctly handles physically impossible next-magnet observations;
5. physically possible but contradictory observations enter transparent judgment;
6. the evaluator has access to the intended evidence;
7. its reasoning is completely observable;
8. ambiguous cases remain identifiable as ambiguous;
9. telemetry is sufficient to determine afterward whether each important decision was correct;
10. direction contradictions are visible;
11. suppressed events remain reconstructable even under telemetry pressure.

A successful lap with opaque reasoning is less valuable developmentally than a comprehensible run whose failures can be explained.
21. Prospective Testing
After the build:

1. Run it on the railway.
2. Preserve complete locomotive-side telemetry.
3. Examine every discrepancy.
4. Determine what physically occurred as well as possible.
5. Use diagnostic waveform/morphology where useful for this post-hoc forensic task, without granting it NAV authority.
6. Compare physical evidence with what NAV believed occurred.
7. Determine which navigation evidence helped.
8. Determine which evidence misled it.
9. Modify the model only for a demonstrated reason.
10. Run again.

Do not continuously redesign the model by replaying every new proposal against old mixed telemetry.
22. Change Control
The architectural decisions in this prospectus are constraints, not eternal truths.
Further progress must not be impeded merely because an earlier decision exists.
However:
An established architectural decision may be superseded only with explicit justification, discussion, and operator approval.
If implementation requires violating or changing an established decision, CODEX must:

1. identify the conflict;
2. identify the existing decision;
3. explain why the proposed change may be beneficial;
4. present the proposed replacement;
5. wait for operator approval before implementing it.

Do not silently restore previously discarded architecture.
In particular, do not silently restore:

* closure-dependent navigation;
* morphology-based magnet acceptance;
* waveform-based navigation authority;
* stitching;
* adaptive-rest behavior;
* arbitrary retrospective thresholds;
* the unapproved five-sample detector.

23. Instructions to CODEX Before Writing the Sketch
Do not begin by writing the final sketch.
First inspect the current source and produce an:
IMPLEMENTATION RECONCILIATION REPORT
For each relevant subsystem report:
CURRENT CODE → PROSPECTUS REQUIREMENT → KEEP / CHANGE / OPEN QUESTION
At minimum inspect:

* Hall opening detector;
* confirmation that detection is two consecutive ≥70 samples;
* opening-polarity capture;
* baseline behavior;
* PWM-zero behavior;
* stop/restart behavior;
* same-magnet protection;
* existing 500/645-ms guard or equivalent;
* detector refractory behavior, if any;
* event queues;
* event drop handling;
* `Navigator::judge()`;
* existing `Passage`, `Judged`, or equivalent structures;
* `mm/marker` telemetry;
* `diag/acq` telemetry;
* locomotive-clock timing;
* RouteMap spacing access;
* recent speed calculation;
* PWM and PWM-history information available to NAV;
* direction derivation;
* relationship among session direction, motor direction and NAV direction;
* existing QUORUM remnants or hypothesis structures;
* station-approach behavior during unresolved navigation;
* wayside-fix interfaces, if any;
* payload sizes and publication limits;
* ability to generate `(boot_id,event_serial)` identifiers;
* suppression/drop accounting.

Also identify every place where current code depends on:

* closure;
* morphology;
* waveform completion;
* stitching;
* adaptive rest;
* fixed timing constants that affect navigation authority.

Do not automatically delete such code. State whether it is:

* navigation-active;
* diagnostic-only;
* obsolete;
* or uncertain.

Present conflicts and open architectural questions before implementation.
24. Initial Implementation Bias
Where the prospectus leaves a choice open, prefer:

* simple over elaborate;
* observable over opaque;
* evidence preservation over premature filtering;
* reversible decisions over irreversible guesses;
* physical impossibility over fitted probability when establishing hard safeguards;
* multiple independent evidence sources over a single magic estimator;
* explicit uncertainty over false certainty;
* prospective measurement over retrospective optimization.

Do not add complexity merely because the architecture permits it.
The first version should be the simplest system capable of exercising the central idea:
NAV knows where it believes it is and what should come next. Detection reports what was observed. Hard protection rejects only what physics says cannot be the next magnet. Everything physically possible but contradictory is evaluated transparently before NAV changes what it believes about position.
The railway will tell us what additional sophistication is actually required.
The major change from the first version is that CODEX now has a much sharper boundary: detection establishes the observation; hard protection may reject only physical impossibility; judgment handles uncertainty. That should substantially reduce the opportunity for implementation choices to become accidental architecture.
