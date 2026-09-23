# NAVI_IR 0.4 delta

NAVI_IR 0.4 replaces the withdrawn 0.3 patch. It incorporates the independent 0.3 review and the operator's clarified architecture.

## Governing model

NAVI is the sole location decision process. IR is not a second navigator or agent. NAVI evaluates operator declaration, direction, route map and polarity sequence, mapped spacing, elapsed time, recent accepted history, PWM/motor state, Hall observations, and IR wheel movement.

Operator declaration is authoritative initialization. A ten-magnet sequence is not required at startup. Ten clean mapped Hall observations remain a backstop for restoring/renewing certainty after ambiguity; certainty may return earlier when the combined evidence uniquely resolves position.

## Changes from the withdrawn 0.3 patch

- Fixed the mismatched include filenames. `NAVI_IR.ino` includes `Navigator.h`; `Navigator.h` includes `HypothesisNavigator.h`.
- Removed `WaitingDistance` as a navigation state and ruling. IR unavailable is now evidence state (`CANNOT_ASSESS` via the existing `MotionIssue` path), not a NAVI state. NAVI may continue judging from its other evidence.
- Removed the separate ten-interval `IR_CONFIDENT` state. The ten-magnet mechanism is not an IR qualification protocol.
- Retained the temporary 500 ms timing reality gate.
- Valid IR travel is an additional reality gate: if travel from an accepted Hall/movement anchor is still closer to zero mapped intervals than to the next mapped interval, that Hall observation cannot advance NAVI.
- Removed the ONE STRIKE fault cap as terminal navigation authority. Fault counts remain diagnostic provenance; later evidence can restore certainty.
- Uncertainty may invoke wider movement/map relocalization immediately; NAVI does not have to wait for terminal LOST before using movement evidence.
- Movement-assisted relocalization uses the last confirmed Hall/movement anchor, accumulated wheel travel, route direction and map spacing to propose map candidates. The current Hall observation must corroborate candidate polarity. Movement alone never assigns an MM.
- Relocalization handles accumulated travel beyond one circuit by retaining whole-lap count and searching the route using the modulo-lap remainder. It no longer silently searches only one lap against an arbitrarily larger raw distance.
- Consolidated the 0/1/2 mapped-interval distance choice into `classifyDistance()` inside `HypothesisNavigator` so the too-early gate and ordinary distance interpretation share one implementation.
- Corrected recovery advance bookkeeping so movement-assisted reestablishment increments `advances` consistently when position changes.
- Telemetry now describes a NAVI movement-derived prediction (`movement_predict_mm`, `movement_predict_ms`) rather than an "IR navigator" prediction/confidence.

## Intended behavior

1. Operator declares interval and direction; NAVI accepts that as the initial location frame.
2. The first credible expected Hall landmark can advance NAVI and establishes a Hall/movement anchor.
3. Subsequent Hall events are evaluated against map sequence/polarity, the retained timing gate, and usable IR travel. Invalid/stale/unavailable IR does not mean zero movement and does not by itself stop NAVI.
4. A valid Hall event that is physically too early by timing or usable IR travel cannot advance the mapped position.
5. When evidence becomes contradictory, NAVI retains/evaluates alternatives. It may use accumulated movement from the last confirmed anchor to search the map for plausible current candidates; Hall corroborates them.
6. Ten clean Hall/map observations remain a pattern backstop after uncertainty, but NAVI may regain certainty sooner when the evidence uniquely resolves a location.

## Build status

The patch has been source-reviewed for the specific 0.3 findings, but it has NOT been Arduino/ESP32 compiled in this environment because the uploaded source set does not include the repo's `firmware/common/IrMovementWire.h` dependency required by `MovementEvidence.h`. Run the repo's full host suite and both ESP32 compile variants before flashing.
