# 0066 — The Grillers climb runs at 110 CW, and backs off one PWM count at a time

Status: Proposed  (2026-09-01)

## Decision

The throttle follows position on one stretch of the railway, clockwise only:

| where | throttle |
|---|---|
| MM65–79 | **110** |
| MM80 | 106 |
| MM81 | 102 |
| MM82 | 98 |
| MM83 | 94 |
| MM84 | 90 |
| MM85 onward | 90, base cruise |

Four PWM per marker, applied **one count at a time** at 280 ms a count. CCW is
untouched — the same stretch is downhill and needs nothing. Everywhere else, in
both directions, the throttle is the base cruise as before.

`cruisePwmAt(mm, dir, baseCruise)` in `RouteMap.h` is a pure function of
position and direction. It is consulted once per advance, and only when its
answer changes, so a ramp in progress is never restarted.

Ships as NAVI_ONE 0.6.

## Context

Operator's ruling, 2026-09-01:

> "Exiting Grillers, the loco has a slow ramp up to 120 pwm to allow it to
> overcome grade and stiction. Really, 120 is too much. It zooms up the grade.
> Lets make the throttle setting for the Grillers grade 110 PWM ... at MM 80,
> start backing off on the throttle. 4 PWM per MM down to 90 PWM, but decreasing
> 1 PWM at a time evenly across each MM so that there is not an abrupt change of
> speed. At MM 85 and thereafter, except at stations, PWM is 90. on CW runs."

Two things were wrong before this. NAVI_2 ran the climb at 120, which overdoes
it. And whatever the figure, it was surrendered in a single request the moment
the section ended — a step, not a ramp, and the operator could feel it.

NAVI_ONE 0.5 had neither problem and neither remedy: it has no position-dependent
throttle at all, and has been taking the climb at a flat 90. That works light.
NAVI_2's note on the same grade — "stalled at 100 with three coaches" — says it
will not work loaded, which is what this decision is really for.

## Alternatives considered

- **Keep 120.** Rejected by observation: "it zooms up the grade."
- **Per-marker pacing derived from a speed model.** Worked out in full: 260, 273,
  287, 303, 322 ms a count, from `speed = 3.990 x (PWM - 25.1)` fitted to 4,617
  measured samples (Otto with two coaches, 2026-06-30). **Rejected as false
  precision.** The same calibration shows marker time varying **31% at a fixed
  throttle** depending only on where the locomotive is on the loop — larger than
  the 24% the speed change across this ramp accounts for. A five-constant table
  chases a smaller effect than the one it ignores, and MM80–85 is immediately
  after the steepest climb, where a loop-average curve is at its worst.
- **Pace each count off the previously measured marker time.** More accurate
  still, and self-calibrating for load and battery state. Rejected by the
  operator: "K.I.S.S." The smoothness comes from one count at a time, not from
  the interval being right to the millisecond.
- **A ramp on entry to the climb.** Not needed — "if it doesn't stop, constant
  PWM works." The section simply requests 110 and the normal AUTO step-up rate
  gets there.

## Consequences

- The first position-dependent throttle in this sketch. `RouteMap.h` now carries
  a speed as well as a geometry, which is a widening of what "the surveyed truth"
  means. It is a pure function and gate-tested, but it is a new kind of thing.
- **A throttle number can cause a navigation failure.** NAVI_2 records exactly
  that: departing a station too slowly stretched magnet events to four seconds,
  the baseline was starved, and navigation was lost — the latch of finding 08,
  misattributed at the time. 110 is faster than the surrounding cruise, so this
  decision moves away from that hazard rather than toward it, but the coupling is
  real and belongs on the record.
- Backing off uses 280 ms a count. Every other downward path — `withdraw()`,
  e-stop, low voltage — sets its own rate explicitly at its own call site, so
  none of them can inherit this one. **A strike stop is unaffected and stays
  brisk**, which the sketch's own header requires.
- AUTO only. MANUAL keeps operator authority, and `autoRunning` is already false
  by the time a struck locomotive would reach this code, so it cannot restore
  power to a locomotive that has stopped.
- The 280 ms figure is a judgement, not a measurement. Measured marker times over
  this speed range are 0.9–1.3 s, so four counts fit inside a marker either way.
  If the ramp is later felt to finish early or late, this is the number to move.

## Verification

All nine host gates pass. Compiles clean against Toby's core (esp32:esp32:esp32
3.3.11) with no warnings from NAVI_ONE sources — 966,895 bytes flash (73%),
59,188 bytes globals (18%), unchanged from 0.5.

- **gate 9**, new, 453 checks: base cruise before MM65; 110 across MM65–79; the
  exact ramp 106/102/98/94/90 at MM80–84; base cruise from MM85; the ramp is
  monotonic and never falls below base cruise; **CCW unchanged at all 171
  markers**; an unset direction never raises the throttle; the profile holds for
  base cruises other than 90; and the marker index wraps safely.
- Gates 1–8 unchanged and passing, including the 172-advance lap replay.

Not field-tested. Not ratified.

## References

- `firmware/test-programs/NAVI_ONE/RouteMap.h` — `cruisePwmAt()`
- `firmware/test-programs/NAVI_ONE/NAVI_ONE.ino` — the advance handler
- `firmware/test-programs/NAVI_ONE/tests/gate_section_cruise.cpp`
- `NGR-Files/Calibration Data OTTO with two 2 axle passenger cars/` — the
  measured PWM/speed data behind the rejected pacing table
- `docs/NAVI_ONE_FIELD_FINDING_08_BASELINE_LATCH_SWALLOWS_MARKERS.md`
