# TEMPLATES Hall Processing Pre-Build Review Plan

## Status and mandatory gate

- This document is investigatory.
- Nothing here is approved for implementation.
- This review must be completed before `TEMPLATES` is built from `QUORUM`.
- Each disposition belongs to the operator.
- Codex may explain, identify consequences and recommend, but may not assign a disposition.
- Review completion does not itself approve implementation. A separate explicit approval is required.

## Governing target concept

The target is the one mile-marker magnet physically possible next. Every other Hall response is noise. Position changes only after evidence identifies the response as that expected marker.

For every mechanism, determine:

1. What target-identification function does it perform?
2. What evidence does it contribute?
3. Is that evidence independent and reliable?
4. Does it help accept the target or reject noise?
5. If not, why does the mechanism exist?

Prefer the simplest method demonstrated adequate by evidence. Evaluate width plus integrated flux before CFAR, matched filtering or other complex classification.

## Current evidence that constrains the review

- All three 2026-08-24 captures were more than 99.94% complete.
- There were no transport gaps or ring-buffer drops.
- Earlier reports of approximately 6% missing UDP data were caused by a decoder accounting error, corrected in commit `078e6d9`.
- Genuine passage-like responses appear broad and smooth.
- Narrow spikes continue while the locomotive is powered but stationary and therefore cannot represent successive magnets.
- Preliminary integrated flux differs greatly between narrow spikes and broad responses.
- Broad responses observed so far last approximately 0.4 to 1.1 seconds, depending on operating conditions.
- No production threshold has been selected.
- Baseline processing can distort slow responses and must be evaluated against complete waveforms.
- Marker repeatability cannot be inferred by pooling responses from different magnets.

## Required review record

For every section below, record:

- Code location
- Plain-language behavior
- Inputs and outputs
- Assumptions
- Interaction with navigation
- Whether it matches Templates
- Questions, corrections and red flags
- Operator disposition
- Exact approval status

Do not combine Hall acquisition, signal processing, navigation, motor control, communications, electrical protection or operator control into one disposition.

## Production Hall path to review

Line numbers below describe the reviewed checkout and may move. Function and symbol names are the controlling anchors.

### 1. Active locomotive profile

- Code: `firmware/QUORUM/LocoConfig.h`, active profile include.
- Present role: selects the locomotive-specific Hall constants used by `QUORUM`.
- Review question: confirm which profile `TEMPLATES` will use and eliminate contradictory selector comments or assumptions.
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 2. Hall threshold configuration

- Code: `firmware/QUORUM/LL_LocoConfig_9950011.h`.
- Symbols: `HALL_DEADBAND_COUNTS`, `HALL_ENTRY_MARGIN_COUNTS`, `HALL_MIN_PEAK_DELTA`.
- Present role: the first two constants establish event-opening and event-closing amplitude thresholds around the baseline. `HALL_MIN_PEAK_DELTA` is reported but does not presently accept or reject events.
- Review questions:
  - What acquisition threshold is necessary to capture candidate waveforms without treating amplitude as target identity?
  - Which values have independent ground truth?
  - Should locomotive-specific parameters exist before repeatable, marker-anchored evidence supports them?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 3. Hardware acquisition definitions

- Code: `firmware/QUORUM/QUORUM.ino`, Hardware section.
- Symbols: `HALL_PIN`, `ADC_SAMPLES` and the ADC configuration in `setup()`.
- Present role: selects Hall input pin 33, uses 12-bit ADC readings and averages eight consecutive conversions into each processed value.
- Review questions:
  - What is the measured conversion and effective sample cadence?
  - Does eight-read averaging preserve spike width, broad-response shape and integrated flux?
  - What signal conditioning occurs electrically before the ADC?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 4. Hall evidence contract

- Code: `firmware/QUORUM/QUORUM.ino`, `struct MarkerEvent`.
- Present fields: opening polarity, peak amplitude, duration, baseline drift, opening timestamp, actual PWM and commanded PWM.
- Present consequence: the full waveform and integrated flux are discarded before navigation receives the event.
- Review questions:
  - What evidence must survive acquisition for target identification?
  - Which measurements are primary evidence and which are diagnostics?
  - Is an event record sufficient, or must waveform evidence remain available through target acceptance?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 5. Layer 1 — Sensor

- Code: `firmware/QUORUM/QUORUM.ino`, section headed `LAYER 1 — SENSOR`.
- Symbols and functions:
  - `MEDIAN_WINDOW`, `MEDIAN_SAMPLE_MS`, `CALIBRATION_MS`
  - baseline and median-ring state
  - `recomputeThresholds()`
  - `readAveragedADC()`
  - `medianOfRing()`
  - `primeMedian()`
  - `updateBaseline()`
- Present role: converts ADC readings into a moving baseline and four baseline-relative amplitude thresholds.
- Known systemic concerns:
  - The median contains the last 128 accepted samples, not necessarily the last 64 seconds of wall-clock data.
  - Baseline insertion is gated by PWM, which is commanded/actuator state rather than proof of physical movement.
  - Samples are inserted without excluding active Hall responses.
  - A 128-entry median cannot follow sustained drift immediately.
  - Baseline movement changes every event-opening and event-closing threshold.
- Required outcome: determine the complete baseline and signal-conditioning conception for `TEMPLATES`; do not preserve individual mechanisms merely because they already exist in `QUORUM`.
- Operator direction: the explanatory introductory paragraph is useful and is not marked for deletion.
- Operator disposition for implementation: rewrite the Layer 1 sensor processing for `TEMPLATES` using an improved conception of magnet detection; replacement design remains to be reviewed.
- Exact approval status: provisional design direction; no replacement design or implementation is approved.

### 6. Layer 2 — Detector

- Code: `firmware/QUORUM/QUORUM.ino`, section headed `LAYER 2 — DETECTOR`.
- Symbols and functions:
  - `EVENT_EXIT_HOLD_MS`, `EVENT_FLOOR_MS`, `HALL_TASK_TICK_MS`
  - active-event state and counters
  - `detectorSample()`
- Present role:
  - opens an event when the averaged ADC value crosses an amplitude threshold;
  - fixes polarity from the opening direction;
  - retains maximum excursion;
  - closes after returning inside the exit band for 20 ms;
  - rejects events shorter than 40 ms;
  - emits a `MarkerEvent`.
- Review questions:
  - What measured rule defines the beginning and end of a physical passage response?
  - Can width plus integrated flux reject powered-stationary spikes without excluding slow genuine responses?
  - Does opening polarity reliably describe the target?
  - What evidence is lost by retaining only peak and duration?
  - Can one response be divided into multiple events or multiple responses be merged?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 7. Hall sampling task

- Code: `firmware/QUORUM/QUORUM.ino`, `hallTask()`.
- Present role: repeatedly calls `detectorSample()`, requests a one-millisecond delay and records scheduling gaps.
- Review questions:
  - What cadence is actually achieved, including the eight ADC conversions?
  - What timing guarantees are required for width and integrated-flux measurement?
  - What happens when task execution is delayed?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 8. Startup calibration

- Code: `firmware/QUORUM/QUORUM.ino`, `calibrate()` and Hall setup statements in `setup()`.
- Present role: averages readings for two seconds, seeds all 128 median entries with that average and computes initial thresholds. The operator is instructed to keep the sensor clear of magnets.
- Review questions:
  - What happens if startup occurs over or near a magnet?
  - How is an invalid initial baseline detected?
  - Is startup calibration compatible with ordinary railway operation?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 9. Event queue and task creation

- Code: `firmware/QUORUM/QUORUM.ino`, `eventQueue` allocation and `hallTask` creation in `setup()`.
- Present role: provides a 256-event boundary between Hall detection and loop-thread processing.
- Review questions:
  - What evidence is lost if the queue fills?
  - Does the replacement evidence contract change storage requirements?
  - Must raw or summarized waveforms cross this boundary?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

### 10. Hall-event handoff

- Code: `firmware/QUORUM/QUORUM.ino`, `drainMarkers()`.
- Present role: removes each `MarkerEvent`, passes it to `navOnMarker()` and publishes selected event fields.
- Boundary rule: this is the transition from Hall-response processing to navigation. It must not silently convert every detector event into a physical marker.
- Review questions:
  - At what point has a response earned the name “marker”?
  - What evidence must be published for independent verification?
  - Should navigation receive candidates, accepted targets or both?
- Operator disposition: undecided.
- Exact approval status: unapproved; investigatory only.

## Separate downstream navigation review

The following code does not sense the Hall signal. It decides whether a reported response affects position and must be reviewed separately:

- `navOnMarker()` and its timing gate
- `acceptEvent()`
- `processNormalComparison()`
- `handleValidationResult()`
- QUORUM evidence-ring, scoring, adoption and reacquisition functions

Required navigation question: does each mechanism help identify the one physically possible next marker, or does it merely react after an unverified response has already advanced position?

Operator disposition: undecided.

Exact approval status: unapproved; investigatory only.

## Diagnostics and publication review

The status and boot publications expose baseline, averaged reading, delta, timing gaps, threshold values, queue drops and short-event rejections. They do not change detection, but they determine whether failures can be reconstructed.

Review questions:

- Do published values preserve enough independent evidence to verify target acceptance?
- Are rejected candidates observable?
- Are width, integrated flux, baseline history and waveform completeness available?
- Can telemetry distinguish signal-processing rejection from navigation rejection?

Operator disposition: undecided.

Exact approval status: unapproved; investigatory only.

## Mandatory completion checklist

Before `TEMPLATES` is built from `QUORUM`, the operator must have reviewed and recorded a disposition for:

- [ ] Active profile and Hall configuration
- [ ] ADC acquisition and effective sampling cadence
- [ ] Baseline conception
- [ ] Event-opening rule
- [ ] Event-closing rule
- [ ] Width measurement
- [ ] Integrated-flux measurement
- [ ] Polarity evidence
- [ ] Powered-stationary spike rejection
- [ ] Slow-response preservation
- [ ] Hall evidence contract
- [ ] Sampling task and queue behavior
- [ ] Startup calibration
- [ ] Hall-to-navigation handoff
- [ ] Downstream target acceptance
- [ ] Diagnostics required to verify failures
- [ ] Simpler-method evidence before any CFAR or matched-filter proposal
- [ ] Exact approval status of the resulting design

Until every item is reviewed, `TEMPLATES` Hall processing remains unapproved and must not be implemented from `QUORUM`.
