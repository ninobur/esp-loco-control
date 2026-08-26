# TEMPLATES Revision 2 — Target-identification and bounded recovery design

**Status:** Independent design synthesis for operator and peer review.  
**Authority:** Design-only. This document authorizes no firmware change, build,
flash, field operation, or IR control authority.  
**Date:** 2026-08-26  
**Evidence baseline:** commits `240f1bf`, `d05a8b4`, and `5be338d`.

## 1. Result first

TEMPLATES should remain a target-identification navigator, not a threshold
counter followed by a general relocation algorithm. The locomotive begins
from an operator-grounded interval, direction, and orientation. It seeks the
one next mapped target that the available physical evidence can identify.

Revision 2 retains the strongest parts of the prior proposals:

- artifacts are terminally excluded from navigation;
- a physical passage is acquired before map reasoning;
- expected-map polarity is checked before position changes;
- primary position and recovery observations are structurally separate;
- uncertainty ends in bounded observation or a safe hold, never a guessed
  coordinate.

Revision 2 replaces the persistent `ContradictionRing` and score-based offset
adoption with a **causal uncertainty episode**. An episode is anchored to one
last-confirmed target and contains only observations collected while that one
uncertainty remains unresolved. While an episode is active, there is no
ordinary position advance. Every credible passage—including one whose
polarity matches the nominal expectation—must resolve, extend, or terminate
that episode first.

This single change addresses both demonstrated structural failures in the
raw-sample candidate:

1. unrelated contradictions cannot pool across successful navigation until
   clean Toby stops; and
2. a later same-polarity marker cannot silently masquerade as the omitted
   expected marker merely because polarity matches.

Revision 2 also adds a map-reachability envelope. PWM may bound the maximum
distance the locomotive could have travelled; it may not assert that movement
occurred. Valid local IR may later narrow the distance envelope, but IR may
never identify a magnet or advance position by itself.

## 2. Established observations

These are evidence statements, not proposed behavior.

### 2.1 Hall signal populations

- Toby is the clean control. Its 2026-08-24 CCW trace contained 1,202 opened
  excursions, one sub-40-ms rejection, one timing-phantom decision, and one
  disagreement. The operator also reports a successful field run exceeding
  1,100 magnets with one disagreement.
- Otto is the hostile case. Its 2026-08-24 traces contain hundreds of
  predominantly 20–22-ms pulses in addition to broad real-magnet responses.
- Otto's broad agreeing responses closely resemble Toby's: median duration is
  approximately 163–168 ms and median peak approximately 180–198 counts.
- Otto's added pulse population nearly disappears at PWM 0 and can occur while
  the locomotive is physically stationary with motor PWM applied. Motor/PWM
  electrical interference is therefore the leading explanation.
- A simple 40-ms duration floor is insufficient. At least one 43-ms,
  39-count response received navigation standing in QUORUM.
- Raw-sample reconstruction independently reproduced the narrow-pulse
  population without reading QUORUM navigation decisions.

### 2.2 Baseline and passage-boundary behavior

- Duration alone does not identify a passage. Raw reconstruction found
  apparent events lasting 32.8, 77.4, and 137.6 seconds during frozen-baseline
  behavior.
- Integrated absolute flux helps distinguish many spikes, but the closest
  observed clean-genuine versus contaminated-spike margin is only about 1.6x
  (1,211 versus 740 count-ms). Flux is evidence, not a universal identity.
- Genuine passage width changes with speed. Fixed time templates cannot be
  treated as speed-invariant.
- Opposite- and same-polarity companion responses occur around broad
  passages. In Otto CW, 358 responses met the experimental merge rule;
  companion amplitude ratio had median 0.219 and observed values up to 0.463.
  A fixed 350-ms merge ceiling selected the population and therefore did not
  independently validate itself.
- A Hall sensor can remain deflected when stopped over a magnet. Long duration
  is therefore not categorically an artifact unless the approach/history and
  baseline evidence establish that conclusion.

### 2.3 QUORUM failure

- QUORUM advances `navMm` and inserts an event into its evidence ring before
  comparing observed and mapped polarity.
- A response can therefore be reported as a disagreement after it has already
  changed the coordinate system and recovery evidence.
- QUORUM's recovery is then asked to repair false inclusions, although its
  bounded offset model was primarily justified by omissions.
- In Otto's final CW failure the operator independently observed roughly
  19 markers of disagreement between physical and believed position.

### 2.4 Preserved replay results

- Codex's `tools/templates_replay.py` is an event-summary baseline. It consumes
  QUORUM-produced event summaries and is not an independent full-waveform
  classifier.
- Claude's `tools/templates_replay_spec.py` reconstructs candidates from SAMPLE
  fields and is the stronger acquisition evidence base. It remains a rejected
  navigation candidate.
- The raw-sample candidate's artifact-boundary audit found no known artifact
  disposition entering `EXPECTED_ADVANCE` or `RESYNC_ADOPTED` in the tested
  corpus. That audit establishes bookkeeping integrity, not absolute position
  correctness.
- The same candidate stopped clean Toby after only 30 expected advances. Its
  persistent contradiction ring pooled 12 mostly isolated observations across
  successful advances and forced a terminal stop.
- Raw Toby passage deletions demonstrated that two- and three-marker omissions
  can be silently hidden by a later same-polarity passage, producing a
  two-marker coordinate error with clean internal audits.
- One naturally occurring two-marker omission produced a correct recovery
  adoption, but one success is not adoption validation.
- The current raw-sample tool's liveness calculation produces warnings close
  to its threshold on ordinary steady Toby and Otto runs. Its PWM/speed model
  is not calibrated well enough for safety authority.
- The Grillers sustained-stall evidence is not currently replayable by the
  raw-sample tool because its schema differs. Claims based on that replay are
  not yet validated.

### 2.5 IR evidence available today

- The IR sensor observes an unpowered ten-spoke wheel. This avoids driven-wheel
  spin as the primary speed error, although skid, lift, contamination, optical
  loss, and false edges remain possible.
- Synchronized August 10 and August 12 logs show useful opportunistic IR
  measurements when valid: median IR/Hall speed ratio about 0.99, median
  absolute disagreement about 5–6%, and observed valid moving coverage about
  74–82%.
- MQTT delivery can lag by 32–44 seconds. Any navigation use must consume
  source-timestamped IR locally on the locomotive, not through MQTT.
- A controlled stationary test under direct bright illumination produced a
  false-valid 37.27 mm/s report. IR truthfulness Gate A is therefore failed,
  and IR cannot presently carry navigation or control authority.
- A matte-white backing behind the black spokes is a promising physical
  contrast improvement, but it is not yet validated evidence.

### 2.6 Prior operator directions that constrain later calibration

The recovered design interview records the following operator directions. They
do not supply the measurements themselves:

- earliest arrival is interval-specific, not based on the fastest interval
  elsewhere on the route;
- route intervals are to be measured magnet-center to magnet-center along the
  track centerline, to the nearest 5 mm;
- maximum-speed testing is per locomotive, both directions, fresh battery, no
  consist, PWM 50 through 120 in increments of 10, one lap per PWM/direction;
- the fastest valid interval-derived speed at each applicable PWM becomes the
  measured maximum, with a 10% faster allowance for the exclusion bound;
- ordinary timing is confirmatory only when its operating assumptions hold;
- the first confirmed target after a declared start establishes the timing
  anchor; and
- maximum-flux identity and timing are not established. A minimum target-flux
  hard exclusion cannot become active until the locomotive can measure the
  relevant full-passage maximum repeatably and the minimum is grounded per
  target or justified population.

Revision 2 preserves these directions while keeping all resulting numerical
tables unapproved until collected and replayed.

## 3. Governing principles

### 3.1 Error asymmetry

A doubtful response is omitted rather than admitted. A rejected artifact is
not quarantined, scored, or resurrected. This preserves the operator's accepted
doctrine: uncertainty should become the modeled omission error, not a false
coordinate insertion.

### 3.2 Target identity is conjunctive

No single observation means “target.” Target identity requires a compatible
set of evidence:

1. a complete, distinct, credible physical passage;
2. physical reachability;
3. declared route direction and ordering;
4. expected polarity;
5. continuity evidence showing that a later same-polarity marker is not also
   a viable explanation; and
6. no unresolved causal episode that the passage fails to resolve.

Polarity is necessary when polarity is available and trusted. It is not
sufficient.

### 3.3 Evidence may eliminate; it may not manufacture

- A maximum-speed envelope may prove a marker impossible to have reached.
- A valid distance observation may eliminate mapped candidates outside its
  uncertainty interval.
- Absence of Hall activity, PWM demand, or invalid IR may reduce confidence or
  cause a safe hold. None identifies a marker or an omission count.

### 3.4 Causal evidence only

Observations from separate uncertainty episodes never vote together. There is
no global contradiction pool and no time-decay rule that silently converts old
evidence into irrelevance. Every episode ends with an explicit disposition.

### 3.5 No retrospective operational repair

TEMPLATES must prevent an unverified advance. It must not issue station,
traffic, or stopping actions from a provisional coordinate and later claim the
coordinate was repaired. Location-dependent operations are inhibited while
position is unresolved.

## 4. Evidence taxonomy

Every item is recorded with source, source time, age, validity, completeness,
quality, and uncertainty where applicable.

### 4.1 Measurement

Directly observed quantities:

- Hall ADC samples and evidence-qualified baseline;
- sample cadence, late samples, gaps, saturation, queue loss;
- actual and commanded PWM and declared direction;
- local IR pulse/distance/speed observations when valid;
- operator anchors and declarations.

Measurement does not contain a marker number unless the operator supplied it.

### 4.2 Passage inference

Derived solely from one continuous Hall acquisition episode:

- opening/closing evidence and polarity;
- peak and peak time;
- signed and absolute area;
- rise, crest, decline, and settled-exit features;
- baseline quality and drift;
- passage completeness;
- companion/edge-lobe grouping;
- motion context during the passage.

A passage inference may be `ARTIFACT`, `CREDIBLE_PASSAGE`, `HELD_FIELD`, or
`INCOMPLETE`. It has no map position.

### 4.3 Map inference

Derived by comparing a credible passage with the route map and motion envelope:

- mapped targets physically reachable from the last confirmed anchor;
- polarity-compatible targets;
- target-specific fingerprint compatibility if such fingerprints are later
  independently established;
- hypotheses explaining omissions or non-target credible observations.

Map inference is private observation state. It has no station, traffic, or
motor authority.

### 4.4 Navigation state

Only the following may mutate authoritative navigation state:

- an operator declaration/anchor;
- a uniquely identified expected target in normal tracking; or
- a uniquely established omission recovery satisfying Section 9.

Artifacts, invalid IR, PWM, elapsed time, and unresolved hypotheses cannot
mutate primary position.

## 5. Passage acquisition and artifact boundary

### 5.1 Acquisition contract

Acquisition operates continuously; it is not disabled solely because PWM is
low. PWM does not prove physical rest, and the locomotive may coast, be pushed,
or approach a station below a motor threshold.

The acquisition layer shall:

1. preserve the high-rate sample record or an auditable sufficient statistic;
2. freeze or otherwise protect the evidence reference while a passage is open;
3. keep a passage open across measured internal returns and defensible lobes;
4. record PWM, direction, motion-source state, and baseline quality throughout,
   not only at opening;
5. mark sample loss or session discontinuity explicitly; and
6. produce one physical-passage record at most once.

Numerical entry, exit, flux, duration, and lobe limits remain replay-selected,
not approved here.

### 5.2 Artifact

An artifact is a response for which the physical evidence is insufficient or
incompatible with a distinct track-magnet passage. Examples include:

- narrow electrical spikes;
- incomplete waveforms or sample-gap overlap;
- categorical physical impossibility;
- baseline-reference failure without a witnessed passage approach;
- saturation or acquisition corruption;
- a separately detected response proven to be part of the current passage.

Artifact disposition is terminal for navigation:

- no primary mutation;
- no observation-episode insertion;
- no hypothesis creation or score;
- no later resurrection.

Artifact counts and reasons remain diagnostic.

### 5.3 Held field and stopped-over-magnet behavior

Revision 2 rejects the prior categorical `MAX_OPEN_MS=3000 -> ARTIFACT` rule.
A long-open response has at least two physically different explanations:

- the sensor stopped over a real magnetic field; or
- the baseline/reference drifted while no passage occurred.

If a credible approach/rise was observed during independently supported motion
and motion then stops while the field remains elevated, acquisition enters
`HELD_FIELD`. It retains one passage identity, creates no repeated markers,
and waits for exit or an explicit session/quality failure.

If the response opened without credible motion/approach evidence and the
baseline is untrustworthy, it remains diagnostic and receives no navigation
standing. Valid local IR could later distinguish these cases; PWM alone cannot.

The current captures establish the ambiguity but do not validate the final
held-field thresholds.

### 5.4 Companion and edge lobes

A companion is merged only when passage continuity evidence supports one
physical field. Time proximity and amplitude ratio are useful features but are
not sufficient alone. When valid, distance travelled through the field is the
preferred normalizing context.

Merged companions:

- enrich the primary passage's shape diagnostics only where doing so does not
  alter its opening polarity or launder a weak primary;
- never become a second map observation; and
- never enter recovery separately.

An unresolved broad opposite-polarity response is not automatically an
artifact. It closes or extends the current physical passage if continuity
supports that interpretation; otherwise it becomes one credible contradiction.

## 6. Motion and reachability evidence

### 6.1 Maximum-reach envelope

For each last-confirmed target, TEMPLATES maintains a conservative maximum
distance that the sensor could have travelled along the declared route:

```text
D_max(t) = integral of validated maximum_possible_speed(
             locomotive, PWM, direction, orientation, power envelope) dt
```

This is an upper bound, not dead reckoning. It may prove that a target is not
yet reachable. It may not prove actual movement or assign location.

The initial table is to follow the operator's recorded calibration direction:
fresh battery, no consist, each locomotive, both directions, PWM 50–120 by 10,
using the fastest valid interval-derived speed and a 10% faster allowance.
Every route interval is measured centerline, magnet center to magnet center, to
the nearest 5 mm. Direction/orientation/grade-specific refinements are added
only if the resulting evidence demonstrates they are necessary. The
provisional QUORUM linear PWM model is insufficient for authority and remains
replay-only evidence.

If the applicable bound is unavailable, use a looser physically defensible
global maximum. If even that is unavailable, reachability is unknown and the
navigator must not invent exclusivity.

### 6.2 Reachable target set

Let `S(k)` be cumulative mapped track distance from the last confirmed target
to the `k`th forward target. At a credible passage, a target is physically
reachable only if its distance is compatible with the maximum-reach envelope
and any valid measured-distance interval.

The normal expected target is exclusive only when no later polarity-compatible
mapped target is also a viable explanation. This is the critical protection
against same-polarity omission masking.

At ordinary stable operation, a calibrated per-PWM maximum-speed envelope may
show that the expected marker is reachable while the following marker is not.
After a long delay, stall, gap, or uncertain motion, several targets may become
reachable; correct polarity then ceases to identify the expected target by
itself.

### 6.3 What PWM may and may not do

PWM may:

- select a validated maximum-speed bound;
- show that tractive effort was commanded;
- contribute to a conservative liveness warning; and
- label ramps and operating context.

PWM may not:

- prove movement;
- provide a minimum travelled distance;
- count omitted markers;
- identify a target; or
- advance position.

### 6.4 Prospective IR distance envelope

An admitted IR observation may narrow actual travelled distance to
`D_ir +/- U_ir` only when all of the following are true:

- it is computed locally from the unpowered wheel;
- source sequence and source time are current;
- its validity/optical-quality state has passed an approved truthfulness gate;
- uncertainty includes missed/extra pulses, wheel skid/lift, and calibration;
- no saturation, false-valid, or transition fault contaminates the interval.

IR may then:

- confirm movement or supported rest;
- narrow the reachable target set;
- normalize Hall waveform shape by distance;
- detect movement without credible landmarks; and
- challenge a nominal same-polarity match.

IR may never:

- determine magnet polarity or identity;
- create a marker event;
- advance primary position by itself;
- override a Hall artifact disposition;
- turn invalid/no pulses into zero speed; or
- enter the navigation control path through delayed MQTT telemetry.

IR currently fails its stationary false-valid gate. Revision 2 therefore
defines the interface but does not depend on IR for Hall-only acceptance.

## 7. Navigation state model

```text
UNDECLARED --operator declaration--> TRACKING_CONFIRMED
                                         |
                    non-unique target / contradiction / continuity loss
                                         v
                                  UNCERTAINTY_ACTIVE
                                    /           \
                    unique resolution             hard bound / integrity loss
                                  /                 \
                   TRACKING_CONFIRMED             SAFE_HOLD
                           ^                          |
                           +------ operator anchor --+
```

### 7.1 `UNDECLARED`

No operator-grounded interval/direction is available. Hall passages are
diagnostic only. Position-dependent automation is unavailable.

### 7.2 `TRACKING_CONFIRMED`

The last confirmed target, direction, route, and evidence epoch are known.
There is no unresolved episode. TEMPLATES seeks the one next target.

### 7.3 `UNCERTAINTY_ACTIVE`

One causal uncertainty episode is open and anchored to the last confirmed
target. Primary position remains frozen. Every credible passage is processed
by the episode hypothesis set; none takes an ordinary advance path.

Location-dependent station and traffic actions are inhibited. Motor behavior
during this state is an operational choice in Section 14.

### 7.4 `SAFE_HOLD`

The episode reached a safety bound or evidence integrity was lost. Primary
position remains the last confirmed target with an explicit `position_age` and
uncertainty reason. A safe hold does not clear the episode or manufacture a
location.

Exit requires an operator anchor or a separately validated episode resolution.
A reversal alone is not a general reset.

### 7.5 Evidence epochs

Every declaration, reboot, direction reversal, material sample gap, and
operator anchor creates or terminates an evidence epoch. Timing and motion
evidence never cross an epoch boundary unless the rule explicitly proves
continuity.

## 8. Normal target identification

```text
Hall samples
    -> physical passage acquisition
        -> ARTIFACT / INCOMPLETE: diagnostic only
        -> HELD_FIELD: retain one passage, await exit/quality disposition
        -> CREDIBLE_PASSAGE
             -> build reachable mapped-target set
                 -> one exclusive expected target, no active episode: advance once
                 -> contradiction or non-unique identity: uncertainty episode
                      -> unique expected resolution: advance once
                      -> unique omission history with corroboration: adopt once
                      -> unresolved safety bound: safe hold
```

In `TRACKING_CONFIRMED`, a credible passage advances exactly once only if:

1. acquisition classifies one complete distinct passage;
2. the passage is not categorically too early;
3. the expected next target is within the reachable set;
4. observed polarity matches the expected target;
5. no later same-polarity target is also viable under the reachability and
   measured-distance evidence;
6. no sample, direction, baseline, or motion discontinuity makes continuity
   unknown; and
7. no uncertainty episode is active.

Failure of a physical/acquisition gate produces `ARTIFACT`. Failure of target
identity or uniqueness opens `UNCERTAINTY_ACTIVE`; it does not advance.

At startup the operator declares the occupied interval, direction, and
orientation. Timing from a previous marker is unavailable. The first target
may be confirmed only if the declaration plus maximum-reach envelope excludes
a later compatible target. Otherwise startup remains unresolved and requires
an anchor or additional evidence.

## 9. Causal uncertainty episodes

### 9.1 Episode triggers

An episode opens on any of the following:

- a credible passage whose polarity contradicts the expected target;
- a credible expected-polarity passage for which more than one mapped target
  remains viable;
- maximum-reach or valid measured-distance evidence showing the expected
  target may have been passed without confirmation;
- material sample loss while movement could have occurred;
- direction reversal after position continuity became uncertain; or
- target fingerprint evidence contradicting the expected marker, if such
  fingerprints are later established.

A rejected artifact does not open an episode merely because it was observed.
Artifact rejection remains terminal. An omission becomes observable through
subsequent reachability, credible-passage, or liveness evidence—not by
remembering an artifact as a possible magnet.

### 9.2 Hypothesis set

The episode creates only hypotheses physically reachable from its fixed
anchor:

- `H_EXPECTED`: contradiction observations are non-target credible noise and
  the expected marker has not been omitted;
- `H_OMIT(k)`: exactly `k` mapped targets were omitted before an observed
  mapped passage;
- `H_UNRESOLVED`: available evidence cannot yet assign one or more credible
  observations to a mapped target.

Each hypothesis records:

- current mapped target under that hypothesis;
- cumulative map distance and maximum-reach compatibility;
- every observation assigned as mapped or non-target;
- polarity and any independently validated fingerprint result;
- valid IR-distance residual where available;
- explicit elimination reasons; and
- the exact evidence epoch.

The omission range is derived from physically reachable map distance and the
safe operational bound. It is not a permanent `{+1..+12}` constant.

### 9.3 Update rule

Every credible passage updates every live hypothesis, even if its polarity
matches the nominal expected target. The implementation may branch a
hypothesis between “mapped passage” and “non-target observation” only within a
strict bounded episode. It prunes branches that violate:

- route order or direction;
- maximum reach;
- valid measured-distance interval;
- passage polarity;
- an independently established marker fingerprint; or
- observation ordering and data-integrity constraints.

Artifacts update nothing.

### 9.4 Resolution

An episode resolves only through an explicit disposition:

1. **Expected target confirmed.** `H_EXPECTED` is uniquely supported and every
   omission hypothesis is eliminated. The resolving target advances primary
   position once; preceding contradictions are recorded as non-target
   observations; the episode is closed.
2. **Omission recovery adopted.** One `H_OMIT(k)` is uniquely supported under
   Section 9.5. Primary position moves once to the last mapped passage
   established by that hypothesis; omitted markers are recorded explicitly;
   the episode is closed.
3. **Operator re-anchor.** The operator supplies position/direction; the old
   episode is archived, not reinterpreted.
4. **Safe hold.** Bounds are reached without unique resolution. The episode
   remains attached to the frozen coordinate for diagnosis.

There is no silent timeout/decay and no clearing merely because a later
passage has expected polarity. A later passage may resolve the episode, but it
must eliminate the competing same-polarity omission explanations first.

### 9.5 Omission adoption standard

An omission hypothesis may be adopted only when:

- it is the sole physically and map-compatible hypothesis, including
  `H_EXPECTED`/non-target explanations;
- all supporting observations belong to the same episode;
- no artifact contributes;
- the evidence includes independent corroboration beyond one polarity bit;
- the complete evidence path and eliminated rivals are reportable; and
- adoption occurs before an applicable safety/operations bound is exceeded.

Minimum independent corroboration is one of:

- at least two successive credible mapped passages whose combined route
  sequence and reachability uniquely identify the hypothesis; or
- one credible mapped passage plus independently validated IR distance or an
  independently established target-specific fingerprint.

The exact number of successive passages is replay-selected. A fixed score
margin or `MIN_SUPPORT=2` is not itself proof of identity.

### 9.6 Episode lifetime

An episode begins at its first causal trigger and ends only through Section
9.4. It cannot coexist with normal advances. Consequently:

- isolated Toby contradictions cannot accumulate across successful targets;
- a successful expected-target resolution closes its own episode explicitly;
- observations from a later episode start from a new anchor; and
- old evidence never fills a global ring.

## 10. Liveness and bounded indecision

Liveness is independent of candidate admission and has two evidence classes.

### 10.1 Distance-backed liveness

When valid local IR or another approved physical witness supplies distance,
TEMPLATES compares travelled-distance interval with map distances and
uncertainty. Movement far enough to make a later target viable opens an
uncertainty episode. Movement beyond the approved observation distance without
resolution causes `SAFE_HOLD`.

### 10.2 Command/time-backed liveness

Without valid distance, sustained tractive command and elapsed moving-context
time may warn that continuity can no longer be guaranteed. Because PWM does
not prove motion, this path may reduce confidence, inhibit location-dependent
operations, or request a safe hold; it may not infer distance or omission
count.

Arrival gating and liveness must not share one PWM statistic. Arrival uses a
maximum-speed upper envelope. Liveness uses source-tagged current context and
empirically validated operating envelopes. A burst followed by creep must not
inherit the burst speed as its timeout pace.

### 10.3 Bounds

Every episode is bounded independently by:

- physically reachable map distance;
- number of credible observations;
- valid measured movement where available;
- elapsed tractive-command uncertainty;
- proximity to a station, traffic boundary, or other location-dependent
  operation; and
- data-integrity/session boundaries.

The numerical bounds remain replay and operator decisions. Reaching any hard
bound produces one durable reason and `SAFE_HOLD`; it never selects the
best-looking hypothesis.

## 11. Special operating cases

### 11.1 Direction reversal

A reversal clears direction-specific arrival timing but does not erase
position uncertainty.

- If reversal occurs from `TRACKING_CONFIRMED` and the reachability envelope
  proves the next forward marker could not have been reached, the first target
  in reverse is normally the last confirmed marker being re-encountered. It
  reconfirms the anchor without a second coordinate advance; subsequent
  targets proceed in reverse.
- If the forward next marker may have been reached, an episode is active, or a
  material gap occurred, reversal enters `UNCERTAINTY_ACTIVE` or retains
  `SAFE_HOLD`. It does not automatically resume.
- Direction-changing edge/lobe activity remains part of acquisition and cannot
  independently reset navigation.

The raw candidate's unconditional “reversal lifts stop” behavior is rejected.

### 11.2 Dwell and station stop

An independently supported stationary dwell preserves the last confirmed
position, invalidates interval timing, and stops growth of measured-distance
state. The next passage starts with fresh timing/reachability context.

PWM 0 alone does not prove stationary. If the locomotive can be pushed or
coast, lack of a valid motion witness leaves continuity uncertain. Station
logic may provide an operational stationary declaration only if its physical
assumption is separately approved.

### 11.3 Stall

High PWM with no target does not prove distance travelled. The maximum-reach
envelope grows conservatively, while valid IR may show zero/low motion. Without
a valid witness, TEMPLATES eventually loses continuity and safely holds rather
than adopting omissions from PWM.

### 11.4 Sample/capture gap

- A gap overlapping a passage makes that passage incomplete and inert.
- A gap while independently stationary invalidates timing but may preserve the
  position anchor.
- A gap while movement could have occurred opens uncertainty; the first
  post-gap sample cannot silently create a complete passage.
- A reboot/session boundary requires a new operator-grounded declaration.

### 11.5 Low PWM, coasting, and manual movement

Hall acquisition continues at low PWM. Motion evidence is labeled separately.
Manual movement without an admitted local motion/distance witness invalidates
automatic continuity and requires an episode or operator anchor. No low-PWM
threshold may silently discard thousands of crossings without diagnostics.

### 11.6 Startup

Startup requires an interval, direction, and orientation declaration. A
baseline taken over a magnet is detected as low-quality/ambiguous rather than
accepted as ordinary zero field. The first target has no previous-marker timing
but remains subject to passage quality, polarity, and later-target exclusivity.

## 12. Boundary between observation, inference, and authority

| Layer | May record | May eliminate hypotheses | May change primary position | May command location-dependent operation |
|---|---|---:|---:|---:|
| Raw Hall/IR/PWM measurement | source values and integrity | No | No | No |
| Passage acquisition | artifact/credible/held/incomplete | Only physical impossibilities | No | No |
| Map inference / uncertainty episode | reachable targets and assignments | Yes | No | No |
| Target confirmation | uniquely identified expected target | Yes | Yes, once | Yes, after confirmation |
| Omission recovery adoption | uniquely established mapped history | Yes | Yes, once | No retroactive actions |
| Liveness | warnings and safe-hold reason | No | No | Stop/hold request only |
| Operator anchor | declared physical truth | Replaces inference epoch | Yes | Yes under existing authority rules |

An omission adoption never replays a station stop, traffic transition, or
mission action that would have occurred at an omitted marker. The required
operational response is a David decision in Section 14.

## 13. Mandatory invariants

1. A rejected artifact cannot advance, enter an episode, alter a hypothesis,
   or later re-enter navigation.
2. One physical passage causes at most one coordinate mutation.
3. Polarity is checked before any target admission.
4. A nominal polarity match cannot bypass an active uncertainty episode.
5. No observation from one episode contributes to another.
6. Recovery includes the expected/non-target explanation, not only omission
   offsets.
7. PWM supplies maximum-reach context but never actual distance or location.
8. Invalid/stale IR abstains; it is never zero speed.
9. IR alone cannot identify or advance a marker.
10. Reversal, reboot, or gap cannot silently clear unresolved position.
11. Location-dependent operations use confirmed position only.
12. Every resolution names its supporting evidence, eliminated rivals, and
    resulting coordinate mutation.
13. Every safe hold freezes the last confirmed coordinate and retains the
    unresolved reason.
14. Internal self-consistency is not accepted as physical truth; anchors and
    deliberate truth cases remain separate.

## 14. Unresolved operational decisions for David

These are genuine policy choices with more than one technically defensible
answer. Revision 2 does not choose defaults.

### 14.1 Movement while uncertainty is active

Options include:

- stop immediately on the first unresolved credible contradiction; or
- continue at a restricted observation speed for a bounded distance to obtain
  another target, with station/traffic actions inhibited.

Immediate stop minimizes travel under uncertainty. Restricted observation may
recover without operator intervention but requires an approved speed, distance
bound, traffic treatment, and stopping rule.

### 14.2 Recovery after passing an operational landmark

If recovery proves that a station or other operation-triggering marker was
omitted, options include stopping immediately, completing a controlled bypass,
or requiring manual authority. TEMPLATES will not trigger the missed action
retroactively.

### 14.3 Re-anchor procedure after uncertain manual movement or reversal

The operator may prefer an immediate position declaration, a controlled return
to a named anchor, or a bounded observation maneuver. The technical design can
support each but should not silently choose railway operating practice.

### 14.4 Future IR authority

After the IR stationary false-valid problem is corrected and validation gates
pass, David must decide whether IR remains diagnostic/shadow-only or may
participate in reachability and liveness elimination. Marker identity and
position advance remain Hall/map responsibilities in either case.

## 15. Technical uncertainties not delegated to David

These require evidence or engineering work, not an operator preference:

- final passage opening/closing and baseline method;
- numerical morphology/flux thresholds;
- held-field discrimination;
- companion grouping limits;
- calibrated maximum-speed envelope by locomotive/PWM/direction/orientation;
- target-specific Hall fingerprints and whether they are repeatable enough to
  use;
- IR false-valid repair, uncertainty calibration, and white-backing effect;
- schema adaptation for Grillers evidence;
- uncertainty-episode observation and distance bounds; and
- minimum corroboration shown adequate by adversarial replay.

Until resolved, the replay must expose these as provisional parameters or
unavailable evidence—not invented precision.

## 16. Explicit disagreements with earlier designs and reviews

### 16.1 Persistent contradiction ring

**Earlier:** retain contradictions until adoption, reversal, session reset, or
ring-full stop. One proposed repair was clearing/pruning on expected advances.

**Revision 2:** no normal advance exists while an episode is unresolved. The
later credible passage must explicitly resolve or extend the same causal
episode. This prevents both unrelated pooling and same-polarity bypass; merely
clearing on advance would fix Toby's stop while preserving the silent omission
error.

### 16.2 Weighted contradiction credibility

**Claude finding/proposal:** use stored peak/duration/PWM fields to weight
contradiction scoring, or explicitly choose equal weighting.

**Revision 2:** morphology first earns `CREDIBLE_PASSAGE`; absent repeatable
marker-specific fingerprints, larger peak/flux does not make one map offset
more likely. Correlated morphology fields must not be converted into positional
votes. They remain gates/diagnostics unless anchored evidence establishes a
target-specific likelihood.

### 16.3 Score margin and fixed support

**Earlier:** offsets `{+1..+12}`, score margin 2, support 2; Codex baseline used
six perfect observations within `{0..+4}`.

**Revision 2:** derive candidates from physical reachability, retain the
expected/non-target explanation, and adopt only unique identity with independent
corroboration. Fixed score margins and observation counts are replay parameters,
not architecture.

### 16.4 Low-PWM acquisition suspension

**Earlier raw-sample candidate:** do not open candidates at PWM <=20.

**Revision 2:** continue Hall acquisition and label motion/baseline quality.
PWM does not prove rest; suppression can hide coasting, pushed, or station
approach passages. The thousands of suppressed Otto threshold crossings must
be characterized rather than made invisible.

### 16.5 Three-second maximum-open artifact

**Earlier raw-sample candidate:** force-close and discard every passage open at
3,000 ms.

**Revision 2:** distinguish baseline failure from `HELD_FIELD`. A Hall sensor
can remain deflected indefinitely when stopped over a magnet. Long duration is
not sufficient for artifact identity.

### 16.6 Reversal reset

**Earlier raw-sample candidate:** reversal clears the contradiction ring and
unconditionally resumes from a replay-local stop.

**Revision 2:** direction timing resets, but unresolved physical position does
not. Resume is allowed only when reachability proves the reverse encounter or
an anchor/episode resolution re-establishes position.

### 16.7 Retrospective extra-event correction

**Earlier navigation concept:** later sequence verification may identify and
discard an extra already included in accounting.

**Revision 2:** an unverified event never receives operational coordinate
authority. Later evidence resolves observation state, not past physical actions.

### 16.8 Spatial-only waveform processing

**Gemini review:** transform waveform sampling from time domain to encoder
distance domain.

**Revision 2:** retain high-rate time samples and add valid distance context
when available. IR is intermittent and currently false-valid under bright
illumination; it cannot replace the Hall time record.

### 16.9 Minimum flux as a hard exclusion

**Recovered operator interview:** below-minimum target flux is intended as a
hard exclusion.

**Revision 2:** preserve that direction but do not activate the rule yet. The
interview also records that maximum-flux measurement and its timing were not
established. Existing pooled flux evidence is not marker-specific and has only
a narrow worst-case separation. The later replay must first demonstrate a
repeatable full-passage maximum and a defensible minimum for the target or
target population.

## 17. Conceptual case outcomes

| Case | Revision 2 outcome |
|---|---|
| Toby ordinary broad expected passage | Advance once when expected target is exclusive; no episode remains. |
| Toby isolated credible mismatch followed by uniquely identified expected target | Open one episode, resolve contradiction as non-target, advance resolving target once, close episode. It cannot accumulate later. |
| One deliberately omitted marker, later opposite polarity | Reachability/mismatch opens episode; recover only after unique corroboration or hold safely. |
| Two/three omissions followed by same-polarity marker | Expected-polarity event cannot bypass episode/reachability ambiguity; no silent advance. Resolve uniquely or hold. |
| Otto 20–22-ms motor spike | Artifact; diagnostic only; no episode or position effect. |
| Otto marginal 43-ms/39-count response | Must earn full passage status; if doubtful, artifact. If credible but non-unique, observation only. |
| Otto recurring broad echo near genuine markers | Each echo belongs to the current passage, is artifact, or opens a local episode; resolved episodes cannot pool globally. |
| Station dwell | Preserve anchor only with supported rest; reset timing; never treat invalid IR as zero movement. |
| High-PWM stall | PWM grows maximum reach but not actual distance; warn/hold rather than infer omissions. Valid IR zero motion may preserve continuity within its uncertainty. |
| Stop over magnet | One `HELD_FIELD`, no repeated advances; finalize on exit or quality failure. |
| Sample gap inside passage | Incomplete artifact; no navigation standing. |
| Gap while movement possible | Uncertainty episode/hold; post-gap sample cannot silently restore continuity. |
| Clean reversal before next marker reachable | Re-encounter last confirmed marker without double count, then track reverse. |
| Reversal after uncertain travel | Preserve uncertainty; no automatic reset/resume. |
| IR invalid or stale | Abstain; Hall-only rules continue or confidence degrades. |
| IR valid after future approval | Narrow reachability/distance; never identify magnet alone. |

## 18. Design verdict

Revision 2 is a reviewable architecture, not an implementation approval. Its
central safety mechanism is causal target identity: normal position cannot
advance around unresolved evidence, and recovery cannot borrow unrelated
observations. Existing Hall captures can test much of the Hall-only behavior,
but they cannot certify IR-assisted distance, all stopped-over-magnet cases, or
absolute recovery correctness without denser physical anchors.

The next authorized technical phase, after review, is one corrected offline
replay. Firmware remains on hold.
