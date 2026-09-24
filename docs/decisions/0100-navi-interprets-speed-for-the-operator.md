# 0100 - NAVI interprets train speed for the operator

Status: Accepted (2026-09-23); implementation awaiting deployment and field check.

## Decision

The operator-facing IR speed tile displays NAVI's interpretation of the train,
not the optical instrument's diagnostic alone. A completed commanded stop,
applied PWM zero, confirmed coupling and fresh quiet observations can support
zero speed even when stationary optics report inadequate contrast.

## Context

The operator observed a stopped train displaying INADEQUATE_CONTRAST. IR, Hall,
motor commands and applied output are parts of one system. IR need not prove
the entire train's state in isolation. Contrary movement evidence cancels the
stopped judgment; it does not erase navigation history.

## Consequences

NAVI publishes separate interpreted fields alongside unchanged raw IR fields.
The initial implementation uses two seconds of qualifying quiet reports after
both commanded and applied PWM reach zero. This is a display settling choice,
not a measured braking distance or new navigation authority. Pulses, Hall
advances, renewed power/command, stale reports or source changes reset it.
Other optical/transport faults are not relabeled as stopped.

Sensor diagnostics, epochs and MM references remain truthful and unchanged.
This refines display handling, not the sensor-only timeout rule in decision0005.
No unavailable raw IR sample becomes valid distance evidence.

## References

`../NAVI_STOP_DISPLAY_20260923.md`; operator clarification in the IR/NAVI task.
