# 0069 — NGR improves operation at the layer where the cause lives

Status: Accepted  (2026-09-01)
Decided by: operator — *"We should change everything possible (code, locos,
infrastructure) to improve operation."*

## Decision

This is a standing NGR engineering rule, not a NAVI_ONE rule. Treat the whole
railway as one operating system: firmware, locomotives, rolling stock, consists,
power, radio and network systems, track, magnets, stations, structures, and the
operator interface may all be improved. Fix a problem first at the layer where
its cause lives, then use other layers where they add useful operating margin.
Software safety remains responsible for stopping safely when physical or
operational conditions are wrong.

## Context

The Grillers event is one application of the general rule. Toby with a
three-car consist spun its wheels while starting on the grade, even with a
slower launch and at PWM 40. Moved back to level track, the same train started
normally. The robust correction was therefore to move the CW stopping point
from centre +1 to centre -1, where a platform can be placed on a suitable part
of the curve. Increased grade throttle still begins at MM070; it is not a
substitute for a sound starting location.

Earlier magnet work showed the same principle: uniform magnet size and
placement provided more reliable fields than compensating physically with
larger or doubled magnets, while firmware remains the right place to handle
singular measured signal artifacts.

## Alternatives considered

- Compensate for every physical problem in firmware. Rejected because it adds
  complexity and can make software fight an avoidable physical condition.
- Treat infrastructure and locomotive setup as fixed. Rejected because simple
  physical improvements can create more margin than elaborate control logic.
- Rely only on physical perfection. Rejected because variation and failures
  remain; navigation and safety must still detect uncertainty and stop safely.

## Consequences

- Field diagnosis considers code, locomotive, consist, track, sensor,
  infrastructure, and operator interface rather than assuming a software cause.
- Physical changes are measured and documented because they can invalidate
  stopping points, throttle profiles, sensor calibration, or prior field data.
- Firmware stays as simple as the railway permits, but not simpler than safe,
  dependable automatic operation requires.

## References

- Grillers three-car-consist field test, 2026-09-01
- Decision 0066 — Grillers grade throttle
- Decision 0068 — per-direction station stopping points
