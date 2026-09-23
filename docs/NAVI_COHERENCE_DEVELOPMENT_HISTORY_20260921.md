# NAVI_COHERENCE — Development History and Decisions
## Record through 2026-09-21, before the next firmware revision

### Purpose
This records the development path into NAVI_COHERENCE and the decisions governing the next firmware artifact. Version numbers apply only to firmware artifacts that actually existed; design discussions/documents alone do not consume a firmware version.

## Development lineage

### NAVI_ONE / X20 / X21
Earlier work repeatedly tried to make each Hall event authoritative enough to advance a single MM counter. Authority moved among opening polarity, later-window polarity, waveform shape, timing/refractory rules, baseline schemes, and strike rules. X20/X21 demonstrated that neither opening nor later-window interpretation was universally authoritative. Median-of-five ADC sampling did solve a real acquisition defect at its source.

**Lesson:** fix known acquisition defects, but do not require Hall to become an infallible position sensor.

### NAVI_SIMPLIFIED
Established the separation between sensor observation and navigation judgment. The desired reasoning became:

**OBSERVATION → EVIDENCE → EVALUATION → DECISION → ACTION**

The two-consecutive-sample opening convention defines a Hall detection instant: the second of two consecutive 1-kHz samples meeting the excursion criterion.

### IR development
The unpowered optical wheel created an independent physical channel: Hall measures magnetic field; IR measures wheel travel. Cumulative ESP-NOW counts mean lost intermediate packets do not automatically lose distance. Invalid/stale/rebooted evidence becomes unavailable rather than invented movement.

**Lesson:** IR is movement evidence, not another navigator.

### NAVI_IR 0.1
First Hall+IR joint navigator. It used retained hypotheses for ordinary progression, false Hall events, missed magnets, and polarity errors. The first field attempt exposed pairing/optical issues and a navigation bug: unavailable IR could consume recovery authority as though it contradicted position.

### NAVI_IR 0.2
Separated unavailable distance evidence from contradictory evidence and preserved the declared reference. Independent review confirmed that repair. But the broader branch/fault architecture remained.

### NAVI_IR 0.3–0.5 development
Unflashed development artifacts explored IR vetoes, movement reseeding, removal of ONE STRIKE, and continuity-first handling. Reviews repeatedly showed that routine sensor imperfections could create competing worlds and unnecessary ambiguity.

**Governing conclusion:** NAVI is the sole decision maker. Hall, IR, timing, PWM, map, history, operator input, direction, and future wayside observations are evidence.

## Human decision model
The operator supplied the model that became NAVI_COHERENCE:

- where Toby has been constrains where he can be;
- authoritative knowledge has inertia;
- ordinary confirmation reinforces the coherent model;
- unusual information is suspect before the coherent model is suspect;
- suspect evidence is recorded and must prove itself through corroboration;
- multiple independent contradictions can eventually force revision;
- the locomotive is on rails, so continuity is unusually strong.

A missed observation is not loss of position. A spurious Hall event is not automatically a magnet.

## NAVI_COHERENCE firmware history

### NAVI_COHERENCE_0_1
First packaged coherence implementation. It removed the ordinary hypothesis tree and implemented a simpler continuity-first navigator.

Independent review found:
1. the 500-ms timing fallback disappeared when the IR anchor was unavailable;
2. `Uncertain` could feed stale position into AUTO/station logic;
3. recovery history was based on Hall callbacks rather than mapped landmarks, so missed markers could misalign the DNA word.

It was not field tested.

**Versioning rule going forward:** every materially distinct firmware artifact that actually exists consumes the next sequential `NAVI_COHERENCE_0_n` number, whether or not flashed. Design-only documents do not. Numbers are never reused. Before creating the next artifact, check the repository for the highest existing NAVI_COHERENCE firmware subversion.

## Additional decisions after 0.1

### Evidence-source architecture
Explicit evidence sources:

**OPERATOR · DIRECTION · MAP · HISTORY · IR · HALL · TIMING · PWM · STATION_MARKER**

**NAVI maintains a coherent position from sufficient available evidence. Evidence sources may appear, disappear, improve, degrade, contradict, or fail without changing the navigation architecture. Absence of a source is not evidence against the current model.**

At startup, OPERATOR + DIRECTION + MAP are sufficient.

### Point-landmark definition
A navigation magnet is a declared **point**, not an extended magnetic-field zone. Once an MM point is declared, NAVI is in the following interval even if the Hall sensor remains physically within that magnet's field. Residual field activity has no navigation authority until sufficient progress makes another mapped magnet possible.

### Per-interval IR window
Reset IR distance origin at every declared MM so odometry errors do not accumulate around the railway.

First controlled Manual test: **±10% of the mapped interval**, deliberately tight as a stress-test setting. Historical noon data indicate this would exclude about 10% of genuine single-pass intervals; that is intentional for testing continuity handling. It is not a production tolerance.

### Rolling DNA
Maintain the last **10 mapped landmarks passed**, continuously. Push once per mapped landmark, not per Hall callback. Inferred missed landmarks remain in the correct mapped position as `MISSED_OBSERVATION`.

Never clear rolling DNA merely because certainty falls. **Uncertainty is when history is most valuable.**

Recovery can occur after 1–10 mapped landmarks. Ten is the exact backstop. After 10 mapped landmarks without restored coherence: `LOCATION_UNRESOLVED`.

### Stop and reversal
Stopping preserves position.

Reversal preserves positional authority from the preceding coherent directional record. It opens a new directional movement account from the reversal point and resets the unsigned IR distance frame. It does not require relocalization.

### Repeating anomalies
A repeated same-place Hall anomaly is diagnostic information and may become a known temporary landmark characteristic. It does not become a mapped MM automatically.

### STATION_MARKER
Future trackside station Hall sensors may identify locomotives by magnet patterns and broadcast a fixed-location observation with timestamp. `STATION_MARKER` is simply another optional evidence source. Its absence is harmless; its presence joins the body of evidence without changing the architecture.

---


## Current governing design

# NAVI_COHERENCE — revised governing design before field test

## Purpose
NAVI is the decider. Hall, IR, elapsed time, PWM, route map, direction, operator declaration, and recent history are information sources. A Hall event does not ask NAVI where Toby is. In normal operation NAVI already knows where Toby was and therefore what mapped landmark comes next.

**Normal loop: KNOWN LANDMARK → EXPECT NEXT LANDMARK → MEASURE PROGRESS → CONFIRM ARRIVAL → REPEAT.**

## Governing principles
1. Operator declaration is authoritative initialization.
2. Where Toby has been constrains where Toby can be. He is on rails.
3. The route map defines ordered landmarks and mapped distance.
4. Valid IR measures actual physical progress.
5. Hall normally confirms the expected landmark; it does not relocalize Toby.
6. Information inconsistent with a coherent physical model is suspect before the model itself is suspect.
7. Unavailable evidence is not contradictory evidence.
8. Perfection is not required. Missed magnets, wrong polarity, and spurious Hall lobes are ordinary imperfections while the physical trajectory remains coherent.

## Evidence authority
- **Operator declaration:** authoritative starting reference and direction.
- **Route map:** authoritative ordered geometry.
- **Recent mapped history:** strong continuity evidence, including landmarks inferred as passed even when Hall missed them.
- **IR:** independent physical-distance evidence.
- **Elapsed time / recent speed:** fallback and corroborating reachability evidence. The 500 ms minimum-marker gate remains active whenever IR cannot provide the spatial gate.
- **PWM / ramp-down:** commanded motion, useful for prediction but not proof of movement.
- **Hall:** landmark confirmation. Polarity/amplitude/waveform can confirm or be anomalous; Hall alone does not redefine position.

## Normal decision loop

Assume MM55 confirmed, CW, MM56 next.

### Hall before MM56 is physically reachable
If usable IR says Toby has not traveled far enough to reach MM56, classify the event `NON_LANDMARK_HALL`. Do not advance position or move the distance anchor. Continue expecting MM56. Polarity is irrelevant: there is no mapped magnet at that physical location.

If IR is unavailable, the timing/recent-motion fallback applies. A Hall event inside the minimum plausible interval is rejected.

**There is never an ungated Hall-advance path.**

### Hall at the mapped MM56 region
The event confirms MM56. If Hall characteristics agree, record `CONFIRMED`. If polarity is unexpected but movement, timing, map continuity, and history place Toby at MM56, accept MM56 and record `POLARITY_DISCREPANCY`. Then expect MM57.

### MM56 not observed
Continue tracking physical progress. If progress reaches MM57's mapped region, record MM56 as `MISSED_OBSERVATION`, include MM56 in mapped history as physically passed, and continue from MM57 when supported. Missing an observation is not losing position.

### Stopped over MM80
Without sufficient travel toward MM81, further Hall activity cannot be MM81. Record it as non-landmark activity. It cannot advance navigation.

## Confirmation model
NAVI maintains a strong working belief from starting point, direction, map, physical progress, and recent mapped history.

Incoming evidence is classified as:
- `CONFIRMATORY`
- `NONCONFIRMATORY`
- `PHYSICALLY_INCOMPATIBLE`
- `MODEL_CHALLENGING`

A single unusual sensor observation is provisional information. It must prove itself before changing the coherent model.

## Rolling affirmation
The recent ten **mapped landmarks** form a rolling affirmation and are part of normal navigation. This is mapped progress, not the last ten Hall callbacks. The history buffer advances once per mapped landmark NAVI concludes Toby physically passed. A Hall-confirmed landmark stores the mapped MM plus its actual Hall diagnostic result. A physically passed but unobserved MM still occupies its correct place as `MISSED_OBSERVATION`, with the map's expected polarity retained in the mapped sequence.

One wrong polarity or missed Hall confirmation does not reset navigation. NAVI looks at what comes next.

The ten-landmark window is not a ten-strike shutdown counter.

## When NAVI questions the model
NAVI does not become uncertain because one Hall characteristic disagrees. The model is challenged only when physical progress and multiple independent sources can no longer be reconciled with expected route progression.

Continue the expected trajectory while a physically coherent successor can be constructed. Question the trajectory only when progress has carried Toby beyond where the maintained model can plausibly place him and repeated expected landmark confirmations fail.

NAVI may restore coherence after 1 through 10 mapped landmarks; it does not have to wait for ten. **Ten mapped landmarks is the exact rolling DNA backstop.** If coherence has not been restored after physical progress across 10 mapped landmarks, NAVI reports `LOCATION_UNRESOLVED` conspicuously and continues gathering evidence in Manual. Position-dependent AUTO/station logic may not use a stale MM.

## Relocalization
Relocalization is exceptional:
1. Start from the last securely known mapped landmark and direction.
2. Use accumulated valid IR travel when available; otherwise use elapsed time, recent speed, PWM history, and mapped progression as weaker evidence.
3. Use rolling mapped history to test alignment with the route.
4. Restore one coherent location when the body of evidence supports it.
5. Do not manufacture alternatives merely because they are mathematically possible.

If two locations ever have genuinely equal affirmative evidence, valid IR distance is the preferred physical discriminator.

## Stop, reversal, Manual and AUTO
Stopping does not erase or require reestablishment of position. Reversal preserves the current known position and changes the expected mapped sequence to the opposite declared direction; unsigned IR does not infer direction.

The first field test is Manual only.

`Uncertain` must never be treated as a current MM for position-dependent AUTO/station decisions. Manual movement may continue while NAVI gathers evidence.


## Repeating anomalies
A sensor anomaly that repeats at the same physical place lap after lap is useful diagnostic information, not a new mapped MM. Count and report it by physical location (for example, repeated polarity discrepancy at MM80 or a repeated non-landmark Hall lobe between MM79 and MM80). For 0.1 this is diagnostic only; it does not automatically rewrite the map or navigation rules.

## Core authority
**NAVI controls advances. Hall does not advance MM. IR does not advance MM. Timing does not advance MM. NAVI advances position after evaluating the coherent physical story.**

## Required implementation invariants
Before field testing:
1. IR unavailable + Hall <500 ms after the prior accepted landmark does not advance.
2. Usable IR showing insufficient travel rejects an early Hall event regardless of polarity.
3. A later Hall event at expected mapped distance confirms the expected MM.
4. Wrong polarity at expected mapped location advances with a discrepancy record.
5. A→C with B missed records B as missed and continues from C without a recovery crisis.
6. Repeated Hall activity while stopped does not advance.
7. IR loss/staleness/optical invalidity is visible but does not itself create uncertainty.
8. Ten-landmark history records mapped progress, including inferred missed landmarks.
9. No recovery mechanism treats a nonconsecutive Hall-event word as ten consecutive mapped magnets.
10. `Uncertain` cannot feed stale position into AUTO/station decisions.
11. No ordinary hypothesis tree, strike budget, or fork-on-single-discrepancy mechanism.
12. Operator declaration immediately establishes position; stop does not erase it; reversal preserves position while changing expected direction.
13. Rolling DNA is continuously maintained as part of navigation and can restore certainty after 1 through 10 mapped landmarks.
14. After 10 mapped landmarks without restored coherence, status becomes `LOCATION_UNRESOLVED`.
15. Repeating same-place Hall anomalies are counted/reported diagnostically and never become mapped MMs automatically.
16. NAVI alone controls MM advancement.
17. Telemetry exposes last known MM, next expected MM, mapped distance, progress, Hall observation, IR status, timing status, discrepancy, decision, and resulting expected MM.

## First field-test objective
Test whether:
- valid IR rejects premature Hall excursions;
- real mapped magnets confirm arrival consistently;
- odd Hall characteristics at the correct physical location do not derail NAVI;
- stopped operation avoids duplicate advances;
- the simple loop remains synchronized over complete laps.

This first Manual run does not authorize AUTO.

## Summary
**I know where I was. The track tells me what comes next. Movement tells me how far I have gone. Hall confirms that I arrived. If Hall fires where no mapped magnet can exist, it is not my next magnet. If one sensor says something odd but the physical story still makes sense, record the oddity and keep going.**

Only when the physical story itself stops making sense does NAVI need to solve a localization problem.

## Compact governing model

**NAVI alone controls MM advancement.**

Hall does not advance MM.  
IR does not advance MM.  
Timing does not advance MM.

Normal navigation is:

**KNOWN LANDMARK → EXPECT NEXT LANDMARK → MEASURE PROGRESS → CONFIRM ARRIVAL → NAVI ADVANCES → REPEAT**

Or:

> **I know where I was. The track tells me what comes next. Movement tells me how far I have gone. Hall confirms that I arrived. NAVI alone decides when I advance.**

The project is attempting reliable operation in an imperfect and uncertain world, not perfection from perfect sensors.
