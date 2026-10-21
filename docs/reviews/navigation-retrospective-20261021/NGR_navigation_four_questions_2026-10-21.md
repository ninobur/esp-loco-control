# NGR navigation models: four questions for each

**Basis:** sketches, Git history, field records, replay packages, and preserved ChatGPT discussion passages.
**Rule of reading:** “worked” means field-demonstrated on Otto or Toby. Replay, compilation, and a retrospective alignment are identified as such.

## 1. DNA navigation (171-magnet map)

### What did we learn?

- Magnetic polarity is useful only as an **ordered, direction-aware sequence** on the actual 171-marker loop; one N/S observation is geographically ambiguous.
- The real railway is not nominal 300 mm geometry. Measured segment lengths and a physically audited polarity map matter.
- A complete physical map, sensor height/alignment, and direction-state reset are preconditions. Several apparent software disagreements were map mismatch, incomplete conversion territory, or missed pickup rather than a DNA-decoding theorem.

### What actually worked?

- Bounded live stretches gave clean map agreement and locked progression; preserved discussion records include Hans MM064–106 and later Otto MM060–82 examples.
- Hall polarity, wiring and map interpretation were thereby demonstrated to be usable as an on-rail location signal.

### What caused failure?

- There was no completed, field-proven full-loop DNA operating result before the line changed direction.
- The physical map was incomplete/inconsistent over some territory; later modifications also mishandled reversal and could retain an old-direction buffer.
- These are primarily **precondition/integration failures**, not proof that ordered-map recognition is unsound.

### What deserves preservation?

- The circular, surveyed, direction-aware route map.
- Ordered-sequence matching rather than single-marker identity.
- Measured `spacingMm[]` as physical data, with map audit/versioning separate from firmware assumptions.
- The discipline that a map match is a candidate; continuity and subsequent evidence determine whether it becomes position.

## 2. TEMPLATES / early autonomous acquisition

### What did we learn?

- Timing, polarity, and a short sequence can form a location hypothesis, but a reversal and an imperfect sensor event make an unanchored word ambiguous.
- Failure must be attributable: the early target/acquisition work produced explicit incident evidence rather than merely a claim that a pattern “looked unique.”

### What actually worked?

- Individual acquisition incidents succeeded, and the timing database/host acceptance harness gave a concrete basis for later route reasoning.
- The broader experiment was explicitly marked failed by its own criterion; it is not evidence of reliable operation.

### What caused failure?

- Reversal blindness and insufficiently explicit ambiguity/recovery handling were structural gaps.
- It also lacked enough field evidence to separate a flawed model from a flawed episode.

### What deserves preservation?

- Predeclared acceptance tests and held-out evaluation.
- Explicit candidate-versus-accepted-location distinction.
- The principle that a navigation incident must leave enough data to reconstruct it.

## 3. SOLONAV / CTO2

### What did we learn?

- The crucial insight was **declare → count → refine**: position is a continuity state, and a Hall event is evidence about it, not an automatic teleport.
- CTO2 taught that a locomotive is an occupied front/rear envelope, not a point at its Hall sensor.
- Most importantly, collision avoidance cannot exceed the trustworthiness of the position reports it consumes.

### What actually worked?

- The local coordination concepts—nearest same-direction peer, front/rear geometry, traffic as a temporary interruption of a station mission—were meaningful and became the basis for later work.
- CTO2 itself was not a safe operational success: repeated rear-end collisions caused it to be abandoned.

### What caused failure?

- The follower’s clearance arithmetic could be correct while the leader position was wrong. CTO2 reported position with unjustified certainty and used fixed geometry without an uncertainty term.
- This is an **architectural safety failure**, not merely a tuning problem.

### What deserves preservation?

- Front/rear consist geometry, freshness, direction, and an uncertainty-bearing peer report.
- “Silent peer = unresolved obstruction,” not empty track.
- Separate traffic interruption from route/station mission.
- The rule that AUTO with lost position cannot continue position-dependent operation.

## 4. QUORUM

### What did we learn?

- A plurality of candidate offsets with a leader-margin rule is safer than immediately accepting a surprising reading.
- Real Hall/magnet faults can be made diagnosable: the MM008 migrated/stacked magnet episode showed that quarantining an implausible event can reveal both the missing and duplicated location.
- The later forensic result was decisive: many apparent missed-magnet bursts were **cascades after a label slip**, not independent physical misses.

### What actually worked?

- On 15–16 August, both locomotives ran long supervised field sessions; QUORUM maintained normal state through real faults and exercised quarantine/adoption. The MM008 case was a live, useful diagnosis.
- Bubble/CTO behavior and Circuit Express service were demonstrated in episodes: actual roles, stops, dwells and service cycles occurred.
- Those successes do **not** establish collision-free two-locomotive operation; later operator evidence of repeated rear ends controls that verdict.

### What caused failure?

- The shipped fence `{-1,0,+1,+2,+3,+4}` was built around an incorrect failure distribution: up to four misses but only one phantom.
- It could not express frequent phantom-driven `−2`/`−3` over-counts. The Bamboo episode let two phantom events create a −2 truth, followed by a wrong +3 adoption and a five-marker error.
- Upstream timing gates could reject real magnets; once the count slipped, stale expectations reclassified subsequent genuine events as failures, amplifying the error.
- CTO consumption of still-insufficiently bounded position then made the navigation defect a collision hazard.

### What deserves preservation?

- Multiple explicit hypotheses, winner/runner-up margin, and honest `NO_QUORUM` rather than guessing.
- Quarantine, event-level diagnostics, immutable event records, and publishable candidate scores.
- A symmetric, evidence-derived candidate space—not the original one-sided fence. Holdout replay of `{-3…+4}` recovered −2/−3 cases and reduced `NO_QUORUM` 10→6 without new `NO_QUORUM`, but it remains replay evidence, not field acceptance.
- Never let a stale expected label manufacture evidence that later real magnets were “missed.”

## 5. NAVI_ONE (0.1 through X21)

### What did we learn?

- Hall passages have structure: tails, lurches, station dwell over a magnet, startup baseline capture, thermal movement, and loco-specific waveform differences all matter diagnostically.
- Otto and Toby cannot share an assumed Hall/motor profile. Acquisition/calibration must be locomotive-specific.
- A detector can be informative without being authoritative: morphology often explained failures better after the fact than it predicted them before they occurred.

### What actually worked?

- Basic Hall/map progression and station behavior operated in real field sessions; Toby supplied useful survey and navigation evidence.
- The X18 station-stop behavior, programmed ramps, dwell and per-direction stopping calibration are genuine operational assets.
- Recording raw waveform/passage evidence made later fault analysis unusually strong.

### What caused failure?

- Its deliberate development policy—one wrong polarity is a strike/stop—was unsuitable for normal railway operation. The record counts 115 wrong-magnet stops in 71 sessions, while the more sophisticated `CONTRADICTED` path never fired.
- Later baseline/shape/closure corrections made the detector increasingly stateful: parking on a magnet, lurching from a stall, tails, and baseline latches could create false shutdowns or reject real magnets.
- Thus the architecture’s useful instrumentation was undermined mainly by **over-authorizing diagnostic waveform judgments**.

### What deserves preservation?

- Per-locomotive survey/configuration and raw passage capture.
- Separate raw capture, diagnostic classification, and NAV decision.
- The measured station geometry/ramp/dwell behavior where independently preserved.
- Build identity, event telemetry, and one-change-per-field-test discipline.
- Shape/morphology as an alarm and investigation aid—not a routine veto near its uncertain boundary.

## 6. NAVI_SIMPLIFIED

### What did we learn?

- The cleanest architectural separation was stated here: **Detection → Hard Protection → Transparent Judgment**.
- Electrical rearm, waveform closure, and physical arrival at another mapped magnet are different claims.
- Ambiguity should be visible and bounded, not silently converted into certainty or an opaque strike.

### What actually worked?

- The architecture and its requirements were reviewed/reconstructed, but the model did not receive a valid successful field run.
- Its one railway run stopped after one physical magnet while claiming ten later observations; that run is evidence of a defective implementation, not operational success.

### What caused failure?

- Same-magnet rereads were admitted as new observations after the implementation disabled physical reachability when position became unknown.
- Those rereads consumed the recovery word and caused `UNRESOLVED_LIMIT_STOP` although the locomotive had not travelled ten new magnets.
- This is an **implementation/interaction failure**; it does not refute separation of detection, protection and judgment.

### What deserves preservation?

- Timestamped polarised sensor openings as facts; later waveform data diagnostic only.
- Detection-anchored same-magnet protection that survives stop/restart/ambiguity.
- Physically grounded hard suppression only; unknown bounds must be permissive rather than pretending impossibility.
- Explicit alternatives, event IDs, monotonic queue/drop/suppression counters, and a transparent ambiguous state.

## 7. NAVI_COHERENCE / geographic-station work

### What did we learn?

- A station requirement should derive from current position, direction and current overlay—not accumulated “visit history,” old phase assumptions, or compatibility code.
- Traffic and station logic need explicit ownership; a traffic hold is not automatically a completed station visit.

### What actually worked?

- This line is chiefly architectural/reconciliation work. It clarified ownership and exposed state-machine hazards; it is not a separate field-proven navigation winner.

### What caused failure?

- Historical procedural machinery allowed phase, timeout, pause/resume and visit-latch state to acquire authority that did not correspond to a current physical condition.

### What deserves preservation?

- Geographic/current-state authority, explicit overlays, and separation of navigation knowledge from station/traffic actuation.
- Keep an already legitimate station operation distinguishable from a newly armed operation based on uncertain position.
- Delete or quarantine obsolete state authority only after preserving tests and field evidence.

## 8. EWO (Eyes Wide Open)

### What did we learn?

- Independent observations must be recorded separately and their actual contribution made visible. A boolean such as `position_reliable` is not a calibrated description of what the locomotive knows.
- Stop/restart and PWM-zero movement are not edge cases; they need explicit localization semantics.
- Geographic stations can operate from maintained current position rather than a fresh Hall event at every marker.

### What actually worked?

- A Toby operating run from roughly 18:41 to 19:53 maintained reported position through a Hall-loss interval and completed a Grillers stop.
- Otto later completed many station stops in the EWO line; station operation and logging were real, not simulation.

### What caused failure?

- The Hall-loss result did not prove the presumed IR recovery: the forensic record shows `ir_inferred_added=0` during the cited Toby misses. The state flag overstated the explanatory certainty.
- Some failures were plainly implementation defects: the MM053 incident was an Otto reboot from station-event argument order; EWO 0.5 had a startup-memory failure and rollback.
- EWO therefore supplied useful operational evidence but no clean proof of reliable fused navigation.

### What deserves preservation?

- Independent Hall/IR evidence streams; publish whether an inference was actually made.
- Current geographic station overlay, controlled ramps, and explicit stop/restart semantics.
- Field-forensic method: distinguish a reboot, sensor loss, rejected event, inferred movement and commanded stop before assigning a navigation cause.
- Replace coarse reliability booleans with inspectable confidence/state components.

## 9. RVM shape manual and continuity

### What did we learn?

- The useful unit of Hall evidence can be a complete evolving ensemble/landscape, not merely a threshold edge.
- Native 5 kHz Hall samples condensed into a 20 ms tracing preserve information needed to distinguish moving passage, flat hold and baseline.
- A stop/stall should not cause a pre-existing magnet ensemble to be thrown away; continuity of evidence matters.

### What actually worked?

- This has the strongest later single-locomotive localization evidence: Otto recorded a 169/169 CW full lap and a 171/171 exact lap.
- The continuity build recorded 272/272 claims from MM024–124 with four long stops preserved and no reboot; slow-crawl work reported 86/86 claims.
- Firmware-path replay matched the reference implementation across the recorded runs, strengthening the interpretation of the field data without replacing it.

### What caused failure?

- The original manual build could backlog/starve the watchdog; this was an implementation/runtime problem, later addressed by bounded feeding/continuity.
- The navigation core still had an irreversible failure boundary: one wrong-polarity accepted ensemble caused contradiction, with no recovery model.
- Slowest crawl, Toby/CCW/environmental variation and true cross-locomotive portability remained incomplete.

### What deserves preservation?

- Full native tracing, bounded real-time feed, and raw-recording retention.
- Ensemble/landscape recognition as a richer observation source.
- Continuity across legitimate stops/stalls, with direct field evidence—not a generic PWM pause heuristic.
- Firmware-parity replay as a release gate, plus the discipline of proving the passive core before attaching commands.

## 10. RVM AUTO stations and bubble (r1–r5.1)

### What did we learn?

- Adding station and peer behavior makes navigation errors more consequential: a frozen/wrong MM changes both station operation and train separation.
- The successive r1–r5.1 corrections exposed feedback loops: continuity depended on recognizer baseline while the recognizer depended on what continuity admitted.

### What actually worked?

- Individual stops, pairing/peer communication, and early bubble behavior occurred on the railway.
- They are insufficient for acceptance. The line remained explicitly experimental.

### What caused failure?

- Subsequent integration changes—not the manually proven RVM core—caused the major incidents: Toby stopped over a magnet (MM064 contradiction), follower release was too early, r4’s creep plus a frozen Otto position caused the Arches collision, and r5 baseline feedback preceded MM038/MM049 contradictions.

### What deserves preservation?

- Explicit peer freshness/direction/role telemetry and source/flash records.
- The simple inspectable station-carpet concept only after location and braking authority have independently passed field evidence.
- Most importantly: preserve the separation between a field-proven localization observer and downstream station/peer actuation. Do not import r1–r5.1 as a package.

## 11. Whole-tracing aligner (Oct 16) and CPR research

### What did we learn?

- A whole-history path model can revise earlier interpretations when later data arrives; this directly addresses irreversible marker-count cascades.
- Motion/PWM/IR can be a soft preference rather than a hard event gate.
- Frozen inputs, source hashes and causal component comparisons reveal whether an improvement is real or simply tuned to a recording.

### What actually worked?

- **Replay only.** The aligner was not flashed or operated in real time. CPR was research/replay work with preserved negative results, not an Otto/Toby field model.

### What caused failure?

- Neither was field-failed because neither became a field operating candidate.
- Their limitation is evidence: the aligner used Otto-specific historical templates and could retrospectively revise a path; that cannot be treated as proof of causal real-time navigation.

### What deserves preservation?

- Fixed-lag / multi-hypothesis path reasoning as a research comparator to irreversible counting.
- Soft, non-veto movement priors.
- Freeze-before-evaluate provenance and component-level negative results.

## Cross-model conclusion

The durable gains are not any one sketch. They are: a real surveyed map; ordered evidence; continuous position with explicit uncertainty; independent, immutable sensor records; candidate-based rather than one-sample correction; physical same-magnet protection; field-calibrated station behavior; and a hard separation between localization, station action, and collision avoidance.

The recurring error to avoid is also consistent across the history: letting a model’s own stale expectation convert ordinary later observations into proof of failure, then using the resulting cascade as evidence that its assumptions were right.
