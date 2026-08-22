# QUORUM critique and bounded recovery deliverable

Date: 2026-08-21

Status: Proposed. This document does not approve firmware changes or field
testing.

Operational note: Otto was rolled back to production commit `6d35bb7`
(`QUORUM_1_16R_IR_TEST_A`, profile 9950011). No further train-running tests are
requested on the evening of 2026-08-21. That is an evening hold, not a ban on
later operator-directed testing.

## The problem

Six months of phased implementation have not produced reliable navigation.
Another multi-phase program is not an acceptable response.

The system originally promised to retain run data, learn what each locomotive
can physically do, and use that knowledge during navigation. The logs were
retained, but they did not become the continuously maintained operational
database. Production instead relies on fixed thresholds, a provisional linear
PWM timing model, and QUORUM recovery after evidence has already been admitted
or discarded incorrectly.

QUORUM is not the promised solution:

- genuine markers can be removed before QUORUM sees them;
- fixed offsets are not a model of physically reachable position;
- the 12-event budget and two-point margin can stop with the true offset still
  viable;
- route-wide DNA self-resolution can consider places the locomotive could not
  have reached; and
- widening offsets cannot repair unsound upstream event classification.

Repurposing the additional ESP32 as a relay should have triggered an explicit
review of the measurement capability being abandoned. It did not.

## The required model

### Physical reachability comes first

Start from the last confirmed position, time and direction. Use PWM history and
measured locomotive limits to maintain the forward set of positions the
locomotive could physically have reached.

A lost locomotive is not suddenly anywhere on the circuit. Its uncertainty
grows forward from its last known position. It becomes route-wide only after
enough time and motion for a complete circuit to be physically possible.

DNA may distinguish among reachable positions. DNA must never override
physical impossibility.

### Absolute PWM validates timing

PWM does not uniquely control or predict speed, but its absolute value limits
what is possible. For each locomotive, programmed PWM, direction and section,
historical measurements define conservative minimum and maximum speed. Known
section spacing converts those limits into a credible `dt` envelope.

Example: if Otto cannot physically cover a particular interval in 664 ms at
PWM 60, a 664 ms event is invalid even if a comparison with the previous `dt`
would accept it.

Use timing knowledge at three levels:

1. Confirmed position: section-, direction-, locomotive- and PWM-specific.
2. Position uncertain: conservative bounds across only the few physically
   reachable sections.
3. Long-term lost: locomotive-wide bounds for the absolute PWM. The position
   set remains constrained by physical reachability.

### Doubtful observations are preserved

A questionable event should normally remain pending until its successor helps
distinguish genuine-marker and phantom hypotheses. Immediate deletion destroys
evidence and creates label slips that QUORUM cannot undo.

Existing phantom signals—physical timing, amplitude, duration/shape, polarity
and successor consistency—must be measured individually and together. Their
historical genuine/phantom error rates are currently not maintained.

## One bounded deliverable

Build, entirely off-locomotive, a reproducible comparison between current
QUORUM and a navigator that uses:

- a generated locomotive timing database;
- PWM/section/direction timing envelopes;
- a forward reachable-position set anchored at the last confirmation;
- pending hypotheses for doubtful events; and
- DNA matching restricted to the reachable set.

This is one work item. It is not divided into an open-ended series of firmware
releases or field phases.

### Required artifacts

1. A committed log normalizer producing records with locomotive, firmware
   version, direction, route interval, spacing, commanded/actual PWM history,
   voltage when available, `dt`, peak, duration, polarity, baseline drift,
   firmware verdict and later event classification.
2. A committed database generator producing versioned timing envelopes with
   source sessions, sample counts, quantiles, safety margins and generation
   date.
3. A committed host harness implementing reachable-position propagation and
   pending event hypotheses. No production firmware flag is enabled.
4. Committed replay fixtures covering Otto and Toby, with entire untouched
   sessions held out from database construction.
5. A single report comparing current QUORUM with the proposed model, event by
   event and incident by incident.

### Definition of done

The deliverable is complete only when another reviewer can reproduce the
results from committed files and the proposed model demonstrates all of the
following:

- it never selects a position outside the physically reachable set;
- it never relocates behind the last confirmed position without a reversal;
- it performs no route-wide search until a complete circuit is physically
  reachable;
- known genuine acceleration events remain genuine;
- known phantoms do not advance confirmed position;
- frozen runs of genuine `PHANTOM_REJECTED` events disappear;
- known Otto and Toby incidents recover or stop without a false relocation;
- every changed regression expectation has an event-level justification; and
- results hold on sessions excluded from design and envelope generation.

If any condition fails, the result is a failed experiment, not a partially
completed production feature.

### Explicitly out of scope

Until this deliverable exists:

- no additional production threshold tuning;
- no promotion of the measured-ratio timing experiment;
- no promotion of symmetric QUORUM offsets as the solution;
- no route-wide recovery changes;
- no autonomous or full-circuit acceptance run;
- no change to MQTT or the Pi controller contract; and
- no claim that track magnets are the primary cause of Otto's repeated
  NO_QUORUM failures.

## Decision after the deliverable

The operator reviews one comparison and chooses among three outcomes:

1. Reject the proposed model and retain the rollback firmware.
2. Revise the model off-locomotive against a specifically identified failed
   acceptance condition.
3. Approve a separately specified, minimal firmware implementation followed
   by operator-directed bench and limited-section testing.

The deliverable itself does not authorize option 3.

## Bottom line

Stop adding recovery patches to QUORUM. First produce the missing system that
turns retained logs into locomotive knowledge and constrains a lost locomotive
to where it could physically have traveled. Deliver that as one reproducible
pass/fail comparison, not another sequence of phases.
