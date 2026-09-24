# NAVI owns the shared train-state judgment

Recorded 2026-09-23 at the operator's request: "Save that thought."
This preserves design intent; it does not authorize a firmware or control change.

## Operator's principle

> It represents an elevation of NAVI as not just a conduit for sensor readings
> but as an intelligent decision maker reporting what is actually happening.
> If NAVI does not know the true state of the train, NAVI cannot control the train.

## Agreed direction

NAVI owns the train-state judgment. Sensors supply evidence; the dashboard
reports NAVI's conclusion; control should act on that same conclusion.

Maintain one shared assessment of stopped, moving or uncertain, informed by
commands, applied PWM, Hall, IR and recent history. Do not create independent,
potentially contradictory interpretations for each dashboard tile and control.
Raw readings remain available for diagnosis, distinct from interpreted state.

A disagreement prompts investigation without erasing established knowledge.
Use the preponderance of evidence and decisional inertia, while stating real
uncertainty honestly. A shared assessment is not infallible: preserve its basis
and contrary evidence so a mistaken conclusion can be identified and corrected.

## SAM's refinement, supplied by the operator

The architecture is Detection -> Hard Protection -> Transparent Judgment.
Hall, IR, PWM, commands, map position, history, direction and later wayside
observations are evidence. No individual input is the train's state.
Hard protections remain enforceable; judgment does not override them.

"Stopped / moving / uncertain" describes only the motion dimension, not the
whole train-state model. NAVI must retain what it knows and why, independently
for each relevant dimension: motion, speed, direction and route/MM position.
The supporting evidence, freshness, confidence and any disagreement should
remain inspectable. This does not prescribe numerical confidence scores or
a new implementation framework.

Uncertainty in one dimension must not unnecessarily destroy certainty in
another. Toby can be moving at a well-supported speed while its precise MM
identity is uncertain. Toby can be known stopped while the IR instrument
truthfully reports INADEQUATE_CONTRAST. Neither situation requires discarding
the rest of the established trajectory or inventing a sensor measurement.

The eventual shared NAVI model must serve both operator reporting and control.
They may use different portions of that judgment, but must not maintain
contradictory private versions of the train's state. Otherwise a dashboard
could report a known stop while control behaves as though that knowledge
does not exist. Commit 427c68b is an initial implementation of this direction,
not a completed shared-state architecture.

Attribution: this refinement was supplied as SAM's commentary in the task
after the operator requested that the principle be saved. Recorded here as
design guidance, not as an already implemented capability.

## IR measures movement; NAVI supplies spatial meaning

Operator clarification: "IR never measures location" and "sensors make
observations; NAVI maintains knowledge."

- Hall/MM observations can establish or constrain an absolute landmark
  association. An observation is evidence, not automatically a unique MM identity.
- IR supplies relative wheel travel and derived speed, not an MM number or
  route position. Route direction is supplied by NAVI's other evidence.
- Commands and PWM describe intended and applied propulsion state.
- NAVI combines these observations with history, direction and the map to
  maintain the train-state estimate and explain its basis.

Illustrative example, not a field measurement: trustworthy MM029 plus 417 mm
of subsequent IR travel in an established direction lets NAVI infer a position
417 mm beyond MM029. That position belongs to NAVI, not to the IR instrument.

If the MM association becomes uncertain, otherwise valid IR travel and speed
remain useful. NAVI may know that Toby has travelled 630 mm since an observation,
is moving at 18 pKPH and has not stopped, while retaining uncertainty about
which absolute MM hypothesis is correct. A later credible MM observation can
reconcile that relative trajectory with the map. Losing the spatial reference
must not, by itself, reset valid movement history or the instrument's epoch.
An actual measurement continuity break remains distinct: preserve known
segments without inventing travel across a blind interval.

This is central to NAVI_SIMPLIFIED ambiguity resolution: loss of absolute
location does not imply loss of motion knowledge. Conversely, the stop-display
work shows that loss of an IR speed measurement need not imply loss of
motion-state knowledge when other evidence supports a stop. These are two
directions of the same principle, not exceptions to it.

## What exists and what remains

R3's IR speed display is a first step, not completion of this architecture.
The operator confirmed that IR speed stayed at zero while Toby was paused.
The same pause left the MM-derived pKPH tile displaying its old interval speed.

Next intended correction: when NAVI concludes STOPPED, both operator speed
tiles should report zero. Keep the last magnet-to-magnet measurement in
diagnostics rather than presenting it as current train speed.

That MM display correction has NOT been implemented by this record. Nor has
R3's display-only stopped classifier been promoted into a shared control-state
authority. Unifying display and control requires a deliberate, tested change,
not silently feeding a display inference back into odometry or navigation.

## References

- `NAVI_STOP_DISPLAY_20260923.md`: implementation and raw/interpreted separation.
- `NAVI_STOP_DISPLAY_FIELD_CHECK_20260923.md`: observed stop/restart telemetry.
- `NAVI_DECISIONAL_INERTIA_CLARIFICATION_20260923.md`: retained trajectory.

No code, hardware, live dashboard or operating behavior changed in this record.
