# NAVI_COHERENCE_0_3

Review-corrected initial Manual field-test candidate for Toby (9950012).

This is the third packaged NAVI_COHERENCE firmware artifact. 0.2 is preserved unchanged as the rollback/review artifact. 0.3 makes only corrections and observability cleanup identified by the independent 0.2 review; it does **not** introduce a new navigation architecture.

## Changes from 0.2

1. **MQTT client identity fixed** — `NAVI_IR_%s` is replaced by `NAVI_COHERENCE_%s`, preventing an accidental broker client-ID collision with the earlier NAVI_IR sketch.
2. **Boot identity fixed** — the second boot line is derived from `SKETCH_NAME` / `BUILD_CLASS`; it no longer falsely prints `NAVI_COHERENCE 0.1`.
3. **Missed-and-advance preserves final Hall polarity evidence** — if physical travel says one or more mapped MMs were missed and the final Hall event has the wrong polarity, NAVI still advances coherently but records `MISSED_OBSERVATION+POLARITY_DISCREPANCY` instead of silently dropping the discrepancy.
4. **Leftover NAVI_IR operator strings fixed** — compile/AUTO/GO text now says NAVI_COHERENCE.
5. **IR comparison observability expanded** — `nav/ir_compare` now publishes candidates 0 through 10, matching the navigator's ten-landmark forward search rather than showing only 0 through 2.
6. **Arduino packaging name fixed** — the sketch file is `NAVI_COHERENCE_0_3.ino` inside folder `NAVI_COHERENCE_0_3/`, so Arduino's sketch/folder naming rule is satisfied.
7. **Focused host test extended** — explicitly checks that missed-and-advance with wrong final polarity retains both facts.

## Navigation behavior intentionally unchanged from 0.2

- NAVI alone controls MM advancement.
- Operator + direction + map are sufficient initial authority.
- A magnet is a declared point, not an extended magnetic-field zone.
- Valid IR uses the deliberately tight **±10% per-interval experimental eligibility window**, reset at every declared MM.
- IR unavailable falls back to the 500 ms timing/recent-motion gate; there is no ungated Hall-advance path.
- A Hall event before a mapped magnet can physically exist is `NON_LANDMARK_HALL` regardless of polarity.
- Wrong polarity at the expected physical location is diagnostic, not a location crisis.
- Missed mapped landmarks preserve continuity and mapped-history alignment.
- Stop preserves position.
- Reversal preserves positional authority/history and starts a fresh unsigned-IR frame.
- Unresolved position cannot drive position-dependent AUTO/station logic.
- Rolling DNA is mapped progress, not Hall-callback count.
- `STATION_MARKER` remains a reserved optional evidence source.

## Evidence architecture

NAVI operates from sufficient available evidence:

**OPERATOR · DIRECTION · MAP · HISTORY · IR · HALL · TIMING · PWM · STATION_MARKER**

Evidence sources may appear, disappear, improve, degrade, contradict, or fail without changing the navigation architecture. Absence of a source is not evidence against the current coherent model.

## First-test scope

**Manual only. AUTO is not authorized by this README.**

The ±10% IR window is a stress-test setting, not a proposed production tolerance. Historical noon data indicate it should reject roughly 10% of genuine single-pass intervals, deliberately exercising continuity handling.

## Repository dependencies / build placement

This patch contains the files changed for NAVI_COHERENCE itself. In the NGR repository it must be placed where the existing shared dependencies resolve. The sketch includes these existing headers and does not vendor independent copies of them:

- `HallObserver.h`
- `MovementEvidence.h`
- `LocoConfig.h`
- `Ops.h`
- `RouteMap.h`
- `Stations.h`
- common `IrMovementWire.h` transitively used by `MovementEvidence.h`

Before flashing, assemble it in the repository using the same dependency sources independently verified for 0.2, then run the real host suite and both ESP32 compile configurations. Do not infer flash-readiness from this patch alone.

## Review checklist before flash

- MQTT client ID begins `NAVI_COHERENCE_`.
- Serial boot output identifies `NAVI_COHERENCE_0_3`.
- Early Hall with unavailable IR still fails the 500 ms fallback gate.
- Early Hall with valid insufficient IR travel is rejected.
- Correct-location/wrong-polarity Hall advances with discrepancy telemetry.
- Missed-and-advance/wrong-final-polarity reports both `MISSED_OBSERVATION` and `POLARITY_DISCREPANCY`.
- Stopped duplicate Hall activity cannot advance.
- `nav/ir_compare` exposes the full 0–10 candidate search.
- Unresolved position cannot feed stale MM to station/AUTO logic.

## Normal model

**KNOWN MM → EXPECT NEXT MM → MEASURE PROGRESS → CONFIRM ARRIVAL → NAVI ADVANCES → REPEAT**

> I know where I was. The track tells me what comes next. Movement tells me how far I have gone. Hall confirms that I arrived. NAVI alone decides when I advance.
