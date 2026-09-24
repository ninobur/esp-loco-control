# NAVI Position-Based Station Requirements R1

## Status and Scope

Toby development-control candidate, not a production promotion.
Boot name: `NAVI_COHERENCE_0_6_POSITION_STATIONS_R1`.
Same `NAVI_COHERENCE_0_6_IR_HEALTH` Arduino folder and NGR-Files shortcut.
Prepared on `agent/toby-1-13-flash` from `6c64204`; no flash, deployment,
Pi modifications or locomotive commands in this work. Toby's last verified
installed build remains `PROXIMAL_R1_IR_SPEED_R3`.

The operator authorized implementation after the final run, then clarified:
"What is required at mm15 should not depend on a trigger at MM25."
See decision 0101. Independent review is required before proposing a flash.

## Cause and Change

The logged CCW pause crossed MM25 at 22:05:20.519 while AUTO was paused.
MM24 followed; GO at 22:05:55.632 requested cruise90. The old station machine
could begin only at offset -10, so Patio's routine never started. This was
not failure to locate Toby. It was failure to apply known location to control.

Local requirements are computed from the existing route and station maps,
not copied into a second 171-entry table:

- Outside station service, `cruisePwmAt(mm, direction, base)` supplies the
  existing section target, including the Patio curve and Grillers grade.
- An unserved station region is recognized anywhere from offset -10 through
  +5, regardless of prior marker observations. No exact-entry trigger is needed.
- Before offset -5, the existing approach curve selects the target for the
  actual offset, retaining the entry-PWM smoothing convention.
- In the zone, station PWM applies immediately, including a restart from zero.
- At or beyond the configured zero-ramp point within that region, target is
  zero. Starting at MM15 CCW therefore does not launch and wait for another
  magnet. MM15 CW still means 60 PWM until the distinct CW ramp point MM16.
- Approach, zone, stopping, dwell and departure all continuously return their
  applicable target. Events are emitted only on transitions or changed approach
  markers, not every loop.
- GO no longer writes cruise90. `stationService()` chooses the position/visit
  target before `serviceRamp()` can take a motor step.
- The correction-only `armAfterCorrection()` workaround is removed. Corrections
  and ordinary travel share the position rule; a zone-to-approach correction
  updates the target rather than retaining the old zone requirement.
  A correction upstream out of an approach clears that unfinished approach
  and looks up the new position's requirement; ordinary section routing applies
  if it is outside every station region.

## Preserved Settings

| Station | Direction | Approach begins | Zone begins | Zone PWM | Zero ramp begins |
| --- | --- | --- | --- | --- | --- |
| Patio | CW | MM005 | MM010 | 60 | MM016 |
| Patio | CCW | MM025 | MM020 | 60 | MM015 |
| Grillers | CW | MM053 | MM058 | 60 | MM062 |
| Grillers | CCW | MM073 | MM068 | 72 | MM064 |
| Arches | CW | MM098 | MM103 | 60 | MM108 |
| Arches | CCW | MM118 | MM113 | 60 | MM108 |
| Bamboo | CW | MM147 | MM152 | 60 | MM158 |
| Bamboo | CCW | MM167 | MM162 | 60 | MM158 |

Zero-ramp points are not claims about the precise physical resting point.
The existing 200-ms/count stop/departure ramps and five-second dwell remain.
Grillers CW departure remains110; other departures use section cruise.
The usual 90-to-60 approach still asks for 84,78,72,66,60 over offsets -10..-6.
An uninterrupted Patio CCW arrival at105 retains 96,87,78,69,60. A fresh restart
at MM24 from zero uses78, not a claim that every approach requires60.

## Visit and Pause Memory

A visit still needs state: an unfinished stop, an ongoing dwell and a departure
are different obligations at the same location. Pause does not discard them.
On completion at the existing stop-offset+3 boundary, a completed-station ID
suppresses repeat service while still within that station region. Leaving
the region clears this memory; the next lap can serve the station again.
Explicit session/direction resets retain their existing reset authority.

Deliberate inactive-AUTO time is excluded from the 120-second phase watchdog.
It does not renew the already consumed active-time budget. An already started
dwell keeps its existing wall-clock rule. A pause before dwell has been observed
does not retrospectively certify dwell completion: on resume PWM0 begins it.
Ordinary active-phase timeout and overshoot still cause controlled withdrawal
through `stationConsensus()`, not an automatic cruise escape.

No speed governor, IR detector/epoch/navigation changes, display changes, new
physical-unit conversions or manual restrictions are introduced. A stop already
being executed is completed; this is not a general mission replanner after
manual relocation. Unknown location still withdraws position-dependent AUTO.

## Compatibility and Transparency

MQTT topics and payload shapes are unchanged. Existing `state/station` events
show station, phase, offset and requested PWM. The historical event name
`ARMED` is retained for approach entry compatibility; it now means the approach
was recognized at the current position, not a prerequisite crossing.
`SEQUENCE_CORRECTED.armed_after` remains present and is always0 because no
separate correction-triggered arming occurs. Station action follows on the
ordinary service pass. These legacy names must not be mistaken for architecture.
`POSITION_REEVALUATED` is a new event value in the existing station payload
when an upstream correction leaves an unfinished approach for ordinary routing.

## Verification

All checks below passed against the final source revision.

- `tests/test_station_position.cpp`: 128 cold-entry positions across four
  stations/two directions; 136 pause/resume checks; all342 route/direction
  positions; six policy laps with24 stops; watchdog, wrap, correction,
  completed-visit suppression and consensus failure checks. Policy laps assume
  immediate PWM settling and are not a physical motion simulation.
- `tests/test_station_correction.cpp`:32 corrected approach positions, now
  through the same entry path as ordinary updates.
- `tools/test_position_station_integration.py`: extracts and compiles the
  actual GO, station service, requestPwm and serviceRamp code with host stubs.
  Checks no transient generic-cruise launch, long pause, repeated GO, dwell,
  section/departure targets, admission refusal, no-position withdrawal,
  E-stop and low-voltage priority. Also checks command/service/ramp order.
- Existing navigator, proximal recovery, health, IR speed and NAVI stopped
  display C++ suites rerun under ASan/UBSan with strict warnings.
- `tools/test_manual_pwm.py`:311 manual/AUTO/safety cases.
- Health JSON test:13 real payloads, maximum638 bytes.
- ESP32 target `esp32:esp32:esp32`, core3.3.11:1,018,947 bytes flash,
  73,196 bytes static RAM. Compile only; no upload.

Local build directory: `/private/tmp/navi-position-stations/esp32-build`.
Application image SHA256:
`cd458c4ab29ae934975c023c52d94e918888551eb68b3df263c7b93a16db0ae0`.

Reproduce from repository root with `clang++ -std=c++17 -Wall -Wextra -Werror
-fsanitize=address,undefined -fno-omit-frame-pointer -I <variant>
<variant>/tests/test_station_position.cpp -o /tmp/test_station_position`, then
run that executable. Run the integration test using
`python3 tools/test_position_station_integration.py`. `<variant>` is
`firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH`.

## Independent Review and Field Gate

Review current-position authority, completed-visit memory, absence of GO's
cruise request, pause/watchdog behavior, late-entry boundary and unchanged
protection/manual authority. Check that all changed behavior is declared above.
Do not treat host tests or an ESP32 compile as field acceptance.

After independent clearance and explicit flash authorization: stationary boot
verification, then supervised pause/resume before/inside a station zone and
at its stop region, followed by an uninterrupted lap. In particular reproduce
Patio CCW MM25->24 while paused, and restart inside its 60-PWM zone. Check one
stop per visit, correct departure, next-lap service and no repeated station
event flood. Stop if behavior differs from the operator's expectation.
