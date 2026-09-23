# NAVI_COHERENCE_0_2

Initial Manual field-test candidate for Toby (9950012).

This is the second packaged NAVI_COHERENCE firmware artifact. `NAVI_COHERENCE_0_1` remains the prior rollback artifact. The sketch/folder identity stays `NAVI_COHERENCE`; only the sequential firmware subversion advances.

## What changed from 0.1

0.2 implements the simplified governing model developed after the independent 0.1 review:

- **NAVI alone controls MM advancement.** Hall, IR, timing, PWM, map, direction, history, operator input, and future `STATION_MARKER` observations are evidence.
- **Operator + direction + map are sufficient at declaration.** Missing sources are unavailable evidence, not contradictory evidence.
- **Point-landmark semantics.** Once an MM is declared, NAVI is in the following interval even if the Hall sensor remains physically inside the old magnet's field.
- **No ungated Hall advance.** If IR cannot assess the interval, the 500 ms minimum-marker timing gate remains active.
- **Experimental IR eligibility window: +/-10% of mapped distance.** This is intentionally tight for the first Manual stress test, not a production tolerance.
- **Distance origin resets at every declared MM.** Ordinary odometry error therefore does not accumulate lap after lap.
- **Too-early Hall = `NON_LANDMARK_HALL`.** Correct polarity does not rescue a Hall signal at a physically impossible location.
- **Wrong polarity at the right physical location is a discrepancy, not a location crisis.** NAVI advances and records `POLARITY_DISCREPANCY`.
- **Missed observations preserve continuity.** If IR travel places the current Hall event at a later mapped MM, intervening mapped MMs are recorded in rolling DNA as physically passed and the final event is accepted as `MISSED_AND_ADVANCED`.
- **Rolling DNA is mapped progress, not Hall-callback count.** It is never cleared merely because certainty falls.
- **Stop preserves position.** Repeated Hall activity without sufficient travel cannot become another MM.
- **Reversal preserves positional authority and rolling history.** The point last passed in the old direction becomes the first expected point in the new direction; `movement.newFrame()` starts a fresh unsigned-IR account.
- **Unresolved position cannot drive station/AUTO logic.** Manual may continue; position-dependent AUTO is withdrawn.
- `STATION_MARKER` is reserved as a future optional evidence source; it is not required for 0.2.

## Normal model

`KNOWN MM -> EXPECT NEXT MM -> MEASURE PROGRESS -> CONFIRM ARRIVAL -> NAVI ADVANCES -> REPEAT`

Or:

> I know where I was. The track tells me what comes next. Movement tells me how far I have gone. Hall confirms that I arrived. NAVI alone decides when I advance.

## +/-10% test window

For a mapped interval `D`, valid IR makes the next MM eligible only when measured travel lies inside `0.90D ... 1.10D`.

This value was deliberately selected as a stress test. The September 20 noon dataset indicates a +/-10% hard single-pass window would exclude about 10% of otherwise ordinary historical intervals. That is useful here: the first Manual run should show whether continuity handles those fallouts without drama. The eventual operating window is expected to be wider and will be chosen from new leading-edge declaration data.

If a genuine expected MM falls outside the window, NAVI does not immediately abandon its position. It keeps the existing point anchor. A later Hall event that lands in the cumulative window for a later mapped MM can advance there while recording intervening MMs as missed observations.

## Rolling DNA

The rolling ten entries represent **mapped landmarks NAVI concludes Toby physically passed**.

Example:

- MM55 CONFIRMED
- MM56 MISSED_OBSERVATION
- MM57 CONFIRMED

is spatially aligned history. It is not a three-Hall-callback abstraction.

The first 0.2 field test is not intended to prove full `LOCATION_UNRESOLVED` relocalization. The 10-landmark history is preserved for that next layer; 0.2 makes unresolved status conspicuous and prevents stale AUTO use rather than inventing a complex recovery mechanism before the normal loop is proven.

## Initial-test scope

**Manual only. AUTO is not authorized by this README.**

The first run should verify:

1. early/spurious Hall lobes are rejected;
2. real mapped magnets are accepted near their expected physical distance;
3. wrong polarity at the right location does not derail NAVI;
4. deliberately tight-window fallouts recover naturally at a later mapped MM;
5. stopped-in-field Hall activity does not double count;
6. rolling mapped DNA remains aligned;
7. reversal preserves position and starts a fresh unsigned-IR frame;
8. IR loss falls back to the timing gate rather than creating an ungated Hall path.

## Verification performed here

A focused native C++17 sanitizer-backed test was run against a minimal `MovementEvidence` test stub and the real route map. It passed:

- declaration + no-IR early timing rejection;
- first plausible Hall acceptance;
- valid-IR too-early rejection;
- wrong-polarity advance at the correct physical location;
- stopped duplicate rejection;
- one missed mapped landmark followed by later coherent advance;
- reversal preserving position/history.

This is **not** a substitute for the repository's real host suite or ESP32 build. Before flashing, run the real tests against the repository's `MovementEvidence.h` / `IrMovementWire.h`, compile the Toby ESP32 target, and independently inspect the actual state transitions and telemetry.
