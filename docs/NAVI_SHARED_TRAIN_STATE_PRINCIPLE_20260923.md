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
