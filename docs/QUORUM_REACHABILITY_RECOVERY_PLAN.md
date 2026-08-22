# QUORUM critique and reachability recovery plan

Date: 2026-08-21

Status: Proposed. This document records the operator's intended navigation
model and a plan to restore it. It does not approve firmware changes or field
testing. The operator's request not to run trains applies to the evening of
2026-08-21, not to later controlled validation.

Operational note: Otto was rolled back to production commit `6d35bb7`
(`QUORUM_1_16R_IR_TEST_A`, profile 9950011). No further train-running tests are
requested on the evening of 2026-08-21.

## Summary

The current navigator does not deliver the system originally intended. Run
logs were supposed to become a continuously improving, locomotive-specific
knowledge base. The locomotives were then supposed to use that knowledge to
judge whether marker observations and travel times were physically possible.
Instead, the production sketch relies on fixed thresholds, a provisional PWM
formula, and a short fixed-offset quorum after errors have already entered the
position record.

QUORUM consequently resembles an improved version of the old recovery
approach: it avoids some unsafe behavior, but it still loses position and stops
repeatedly. It is being asked to recover from classification errors that
should have been prevented or held for more evidence upstream.

The replacement model is straightforward:

1. Preserve observations instead of immediately discarding doubtful events.
2. Use empirical timing limits learned from the logs.
3. Constrain position to what is physically reachable from the last confirmed
   location, in the declared direction.
4. Use DNA only to distinguish among reachable positions.
5. Keep QUORUM, if retained at all, as a tie-breaker inside that reachable
   corridor—not as a route-wide relocation mechanism.

## What the current sketch does

The production sketch maintains a logical marker odometer. Every
navigation-accepted marker advances it by one. When observed polarity stops
matching the DNA map, QUORUM scores a fixed list of offsets around that logical
odometer. Production currently uses `{-1, 0, +1, +2, +3, +4}` and allows 12
accepted observations to establish a winner by a two-point margin.

Before QUORUM sees an observation, two upstream layers may alter the evidence:

- Quarantine can hold and later commit or discard an event using timing,
  amplitude, duration, polarity and successor evidence.
- The conservation gate can label an event `PHANTOM_REJECTED`, preventing it
  from advancing position or entering QUORUM's evidence ring.

When NO_QUORUM is reached, the self-resolution path can use a route-wide DNA
window match. That search is not derived from the last confirmed position or
from the distance the locomotive could physically have traveled.

## Critique of the current design

### 1. The promised learning system was not completed

The logs exist, but they are analyzed episodically. They have not become the
continuously maintained locomotive database that was promised. Operational
limits remain embedded as hand-selected constants rather than generated from
Otto's and Toby's accumulated measurements.

Repurposing the additional ESP32 as a relay should have triggered an explicit
review of the capability being abandoned. It did not. That allowed a temporary
architecture to replace the intended measured system without an operator
decision.

### 2. PWM information is used incorrectly

The conservation gate turns PWM into one predicted velocity using a linear
formula. Field data proves that one value is too precise: grade, direction,
load, voltage, rail condition and acceleration all change the result.

Absolute PWM is nevertheless valuable. For each locomotive and programmed PWM
there is a measured range of possible speeds. Combined with the known spacing
of a particular section, that range defines minimum and maximum credible `dt`
values. A 664 ms interval at PWM 60 can be rejected when the locomotive cannot
physically cover that interval at PWM 60; it should not be judged solely by its
ratio to the previous interval.

PWM is evidence for timing validity. This proposal does not give PWM authority
to control speed.

### 3. Phantom tests are not functioning as an audited decision system

The sketch has several signals—physical interval floor, amplitude, duration,
polarity, successor evidence and conservation timing—but there is no maintained
confusion matrix showing how each signal performs on known genuine and phantom
events. Recent captures show both failure directions: genuine markers are
discarded, while probable phantoms are sometimes committed.

Changing one threshold at a time has moved errors rather than solved the
classification problem. Otto's 500 ms quarantine-floor trial is an example:
it caught a probable 410 ms phantom but is also implicated in discarding a
genuine event and seeding a position slip.

### 4. Fixed offsets are not a physical position model

A small offset fence near the logical odometer is not equivalent to the
operator's requirement. Possible positions should be derived from:

- the last confirmed location;
- declared direction;
- elapsed time;
- PWM history;
- empirical minimum and maximum speed;
- route topology and interval spacing; and
- observations received since confirmation.

A locomotive that was confirmed moments ago cannot be at every position whose
DNA happens to match. It can only be within a forward physically reachable
corridor. The corridor may widen as time and uncertainty accumulate, but it
must not become route-wide merely because a 12-event evaluation budget expired.

### 5. Route-wide self-resolution reverses the authority order

Physical reachability must determine where the locomotive can be. DNA may then
distinguish among those positions. A route-wide pattern match lets DNA propose
positions that motion says are impossible. That is the opposite authority
order from the intended system.

### 6. QUORUM cannot restore discarded evidence

Once an upstream gate rejects a genuine marker, QUORUM never receives it. The
logical label then slips, and QUORUM is asked to infer the damage from a short
polarity window. Widening its offsets may recover some incidents, but it cannot
make the upstream classification sound. QUORUM should be the last recovery
layer, not the main defense against phantom classification errors.

## Required behavior

### A. Maintain a forward reachable corridor

Keep the last confirmed marker, confirmation time and direction. Propagate the
minimum and maximum distance the locomotive could have traveled using the
recorded PWM history and empirical speed envelopes.

Only positions within that forward corridor are candidates. A direction change
starts a new propagation frame. Stopping halts growth from motion. Positions
behind the confirmed location are not reconsidered unless the operator reversed
direction.

If uncertainty persists long enough for a full circuit to become physically
reachable, the corridor may eventually cover the circuit. It must not do so
immediately on entering a LOST or NO_QUORUM state.

### B. Use three levels of timing knowledge

1. **Normal/confirmed:** use the locomotive-, PWM-, direction- and
   section-specific timing envelope.
2. **Position uncertain:** consider only sections inside the physically
   reachable corridor and use a conservative timing bound across those local
   possibilities.
3. **Long-term lost:** use the locomotive-wide range for the absolute PWM.
   Location remains constrained by the reachable corridor; a generic timing
   range does not mean the locomotive may be anywhere.

### C. Preserve doubtful observations

A timing or signal anomaly should normally create a pending hypothesis rather
than immediately deleting evidence. The successor can help decide whether the
pending event was a phantom or a genuine marker. Both hypotheses must update
their own reachable state until evidence eliminates one.

### D. Put evidence in the correct authority order

1. Direction and physical reachability
2. PWM-conditioned timing possibility
3. Sensor credentials: amplitude, duration and shape
4. DNA polarity continuity
5. QUORUM-style scoring, if ambiguity remains inside the reachable corridor

A DNA match never overrides physical impossibility.

## Locomotive knowledge database

The retained logs should feed a versioned dataset containing, at minimum:

- locomotive ID;
- firmware/configuration version;
- direction;
- expected route interval and spacing;
- commanded and actual PWM history;
- voltage and consist/load context when available;
- observed `dt`;
- peak, duration, polarity and baseline drift;
- gate/quarantine decision; and
- later classification as genuine, phantom, missed or unresolved.

From this dataset, generate conservative speed and timing envelopes. Generated
tables must record their source sessions, sample counts, quantiles, safety
margin and generation date. A new table is tested and promoted as a versioned
artifact; firmware must not silently teach itself from unverified events.

Open design decision: whether promoted tables are compiled into firmware or
delivered from the Pi. Changing the Pi/controller contract requires explicit
operator approval.

## Implementation plan

### Phase 0 — Freeze and preserve

- Keep Otto on rollback commit `6d35bb7` until the operator requests testing.
- Do not flash the experimental measured-timing or symmetric-offset flags.
- Preserve current logs, shadow-replay results and rollback provenance.
- Record the rollback in the field log before the next run.

### Phase 1 — Audit existing phantom evidence

- Replay known genuine and phantom events through each existing test
  independently.
- Produce a confusion matrix per locomotive and PWM band.
- Identify which rules discard genuine markers and which admit phantoms.
- Treat unresolved events as unresolved; do not manufacture ground truth.

### Phase 2 — Build the promised database and envelope generator

- Normalize retained telemetry into the schema above.
- Derive maximum and minimum credible speed per locomotive, PWM, direction and
  route section.
- Generate conservative generic locomotive-wide fallbacks.
- Hold out entire days/sessions from envelope construction for validation.

### Phase 3 — Implement reachability in the host harness only

- Represent the reachable corridor explicitly, preferably as a 171-position
  bitset plus minimum/maximum travel bounds.
- Propagate candidates forward from the last confirmed position.
- Intersect candidates with timing possibility and observed polarity.
- Maintain parallel hypotheses for pending doubtful events.
- Remove route-wide relocation from the experimental path.

### Phase 4 — Replay before firmware

Replay all preserved Otto and Toby sessions, including untouched hold-outs.
The experimental design must demonstrate:

- no position outside the reachable corridor is selected;
- no relocation behind the last confirmed point without reversal;
- known genuine acceleration events remain accepted;
- known phantoms do not advance confirmed position;
- the long frozen-rejection runs disappear;
- known NO_QUORUM incidents recover or stop without a false adoption;
- no route-wide search occurs until a full circuit is physically reachable;
- the result is reproducible from committed scripts and fixtures; and
- all changed regression expectations are individually justified.

### Phase 5 — Decide and implement

- Review replay evidence with the operator.
- Write a decision record before changing production behavior.
- Implement the smallest approved firmware change behind a disabled flag.
- Preserve existing MQTT contracts unless the operator approves a Pi change.
- Compile both locomotive profiles and run reset, stop, reversal, ramp, dwell
  and long-gap scenario tests.

### Phase 6 — Controlled validation

Only when the operator requests it:

1. Bench replay with no motor authority.
2. Stationary sensor tests.
3. Short supervised manual movement over a limited section.
4. One locomotive, one direction, one PWM envelope at a time.
5. Review the capture before expanding scope.

No autonomous or full-circuit trial is an initial acceptance step.

## Decisions still required

- Exact evidence required to label historical events genuine or phantom.
- Envelope safety margins and minimum sample counts.
- Whether Otto's quarantine floor returns to 350 ms.
- Whether symmetric offsets remain useful as a bounded fallback after
  reachability is implemented.
- Whether the knowledge tables are compiled or delivered by the Pi.
- What role, if any, the additional ESP32 should resume in measurement.

## Bottom line

The immediate problem is not a lack of another QUORUM threshold. The system
needs to restore the operator's original premise: accumulated measurements
define what each locomotive can physically do, and a lost locomotive remains
constrained by where it was and where it could have traveled. The logs become
operational knowledge; DNA refines physical possibilities; QUORUM no longer
guesses across the railway after upstream evidence has been discarded.
