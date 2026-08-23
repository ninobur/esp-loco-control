# Templates design interview record

**Status:** Recovered discussion record. This document records questions and
operator answers; it is not an implementation approval.

**Planning limitation:** These decisions establish planning direction. They are
not "THE LAW." Every decision remains subject to review, correction, or
replacement before implementation.

**Source:** Codex chat of 2026-08-22/23, recovered from the actual local chat
transcript on 2026-08-23. The original discussion numbered 23 questions, reused
Question 10 after an IR/Hall detour, and revised several questions. Those facts
are preserved below rather than silently regularized.

## Governing design statement added after the interview

Templates is a target-identification navigation system. The locomotive seeks
the one mile-marker magnet that can physically occur next. Everything else is
noise. Any navigation feature that does not demonstrably contribute to target
identification shall be considered for deletion.

## Recovered questions and answers

### Question 1 — Input boundary

**Question:** Should every measurable Hall-sensor excursion be delivered to
Templates for classification, with no marker-validity decision made beforehand?

**Operator answer:** Yes. A raw-data approach may be unworkable and should be
tested.

**Meaning:** Preserve the evidence for Templates. Do not let an upstream marker
decision silently exclude genuine responses. Raw processing is an option to
test, not yet the approved operational architecture.

### Question 2 — First experiment

**Question:** Should the first experiment compare continuous raw-sample
processing against a permissive excursion collector, measuring whether each
captures every magnet response without overrunning ESP memory, processing time,
or queues?

**Operator answer:** Yes, with a correction to the framing. The locomotive is
not looking for every Hall response or classifying by waveform alone. It is
hunting for one particular target, like recognizing a traffic signal by its
location and context rather than treating every red light as a stoplight.

### Question 3 — Timing search window

**Question:** Should the expected travel interval from the last confirmed marker
define a search window during which Templates actively looks for the next
marker's signature?

**Operator answer:** At steady speed, yes. Timing is confirmatory in conjunction
with the other target identifiers. It is not sufficient during acceleration,
deceleration, departure from a stop, or a speed change caused by encountering
another locomotive.

**Later correction:** An impossibly short interval is always exclusionary.

### Question 4 — Physical minimum interval (revised question)

**Question:** Should the physically impossible minimum interval be calculated
from the known distance to the next marker and the locomotive's demonstrated
maximum possible speed?

**Operator answer:** Yes—maximum possible speed for the given PWM. At startup,
the remaining distance to the next magnet is unknown.

### Question 5 — First marker after operator start

**Question:** Should the first confirmed marker after an operator-defined start
be identified using polarity, flux signature, direction, and expected marker
identity, with timing beginning only after that first confirmation?

**Operator answer:** Yes.

### Question 6 — Wrong polarity

**Question:** If a candidate response has the wrong polarity for the expected
next marker, should Templates always classify it as noise, regardless of timing
or flux magnitude?

**Operator answer:** Yes. It cannot be the target. With position, orientation,
and direction specified, there is only one degree of freedom.

### Question 7 — Flux as exclusion or confirmation

**Question:** For a response with the correct polarity, should flux magnitude be
exclusionary when it falls outside that marker's demonstrated range, or should
flux initially be confirmatory because sensor gain and alignment can vary?

**Operator answer:** The operator questioned the assumed failure mode and
identified the more fundamental issue: the locomotive must identify the magnet
at maximum flux. The ESP had reportedly been accepting the first measured value
as the magnet's signal. A weak response behind the locomotive could otherwise
be confused with the expected target during startup.

**Status:** The later discussion established minimum flux as a hard gate, but a
valid maximum-flux measurement method must first be established.

### Question 8 — Complete excursion

**Question:** Should one candidate response remain open until the magnetic field
returns to baseline, with its identity determined by the strongest absolute flux
reached during the complete excursion?

**Operator answer:** The ESP cannot know the maximum field strength at the first
threshold crossing. Event closure should not occur so early. The design must
allow an accurate reading, whatever that requires.

### Question 9 — Evidence for event closure

**Question:** Should the oscilloscope-car recordings be used to measure the
longest near-baseline gap within one magnet's waveform and the shortest gap
between two separate magnets, then set the event-closing rule between those
limits?

**Operator answer:** “We can,” followed by a question about whether complete raw
readings existed or only final strength and polarity conclusions.

**Finding:** Existing HallProbe logs contain time-series readings but at only
about 9–10 samples per second and with weak marker identification. The supplied
oscilloscope record was for the IR wheel sensor, not the Hall sensor. The
available evidence is insufficient to set a Hall event-closing rule.

### Question 10A — Identify the old Hall session

**Question:** Do you remember which Hall-probe session or date contained the
oscilloscope-car run that established the magnet-strength and polarity findings?

**Operator answer:** The apparent answer “9. Sure” was a reference back to
Question 9, not a June 9 date. No Hall session was identified. The subsequently
supplied oscilloscope document concerned IR, not Hall.

### Question 10B — High-rate Hall oscilloscope

**Question:** Should the first Templates experiment be a Hall-sensor oscilloscope
using the proven IR_SCOPE architecture, sampling the locomotive Hall input at
1 kHz?

**Operator answer:** No direct approval. The existing HallProbe programs were
examined, but the operator then called this a rabbit hole and directed the
discussion to continue.

**Status:** Deferred; not approved.

### Question 10C — Pass/fail rules or confidence score

**Question:** Should Templates use clear pass/fail rules for each available
identifier rather than combining them into a confidence score?

**Operator answer:** During development, record the evidence. The odds of a
response being right are greater than being wrong, but the evidence categories
are not all equal.

### Question 11 — Separate telemetry for each identifier

**Question:** During development, should telemetry record every category
separately for each response so we can measure how often each identifier is
correct, wrong, or unavailable before assigning it weight?

**Operator answer:** A genuine target should align on three always-available
criteria: correct polarity, not arriving before the physical minimum time, and
the target's minimum flux. Once the locomotive is moving from a confirmed
marker, a fourth timing/pattern comparison becomes available. Wrong polarity,
too early, or too little flux means the response is not the expected target and
must not move the count.

### Question 12 — Provisional count

**Question:** If a response passes all three hard gates but lacks full timing or
pattern confirmation, should it advance the marker count provisionally, subject
to confirmation by following magnets, or should the count remain at the last
confirmed marker?

**Operator response:** Asked for a specific example. After an example with an
unjustifiably large timing spread, the operator rejected the premise and defined
how the physical minimum should instead be established.

**Status:** Not answered. Whether a passing but incompletely confirmed response
may move the count remains unresolved.

### Question 13 — Specificity of minimum-time table

**Question:** Should the minimum-time table be specific to locomotive, PWM,
travel direction, and individual mile-marker interval?

**Operator answer:** Not unless testing proves it necessary. Variation will
normally make the locomotive slower, not faster.

### Question 14 — Use the fastest route interval as the limit

**Question:** Should the fastest observed interval anywhere on the route
establish the minimum-time limit for that locomotive and PWM?

**Operator answer:** No.

### Question 15 — Interval minimum calculation

**Question:** Is the intended calculation: measure each locomotive's maximum
speed at each PWM with fresh batteries and no consist; use the actual length of
the upcoming interval; calculate time as interval distance divided by maximum
speed; then allow 10 percent faster before rejecting an arrival as impossible?

**Operator answer:** Yes. The result is interval-specific. Track changes after
the Patio Cover painting/rebuild mean the intervals need remeasurement.

### Question 16 — Scope of route remeasurement

**Question:** Should every route interval be remeasured, or only intervals
affected by the Patio Cover rebuild?

**Operator answer:** All intervals.

### Question 17 — Physical measurement method

**Question:** Should each interval be physically measured in millimeters along
the track centerline from one magnet center to the next?

**Operator answer:** Yes.

### Question 18 — Measurement precision

**Question:** What measurement precision should be recorded: nearest millimeter,
5 mm, or 10 mm?

**Operator answer:** Nearest 5 mm.

### Question 19 — PWM test range

**Question:** Should maximum speed be measured only at PWM values used by
automatic operation, or across a regular PWM range with interpolation?

**Operator answer:** Program each locomotive to increase by 10 PWM per lap. Test
Otto and Toby in both directions at PWM 50, 60, 70, 80, 90, 100, 110, and 120.

### Question 20 — Number of laps

**Question:** How many laps should each locomotive run in each direction through
the PWM 50–120 sequence?

**Operator answer:** One lap at each PWM in each direction: 16 laps per
locomotive, 32 total.

### Question 21 — Maximum-speed value

**Question:** For each locomotive and PWM, should the fastest valid
interval-derived speed observed across both directions become the maximum-speed
value?

**Operator answer:** Yes.

### Question 22 — Flux measurement during speed tests

**Original question:** Should the 16 laps per locomotive also establish a
locomotive-specific minimum flux value for every mile-marker magnet?

**Operator answer:** No, unless flux can be measured from the locomotive Hall
sensor.

**Revised question:** Should diagnostic firmware record full-excursion positive
and negative Hall maxima during the PWM tests without using them for navigation?

**Operator response:** First establish what point defines mile-marker timing. If
the design is to use maximum magnetic flux, the ability to identify that point
must be established before timing or flux tables are collected.

**Status:** Measurement during the speed tests is not yet approved.

### Question 23 — First Templates experiment

**Question:** Do you approve building a high-rate Hall oscilloscope for the
existing diagnostic car as the first Templates experiment?

**Operator response:** Asked whether current QUORUM was still using threshold
opening. Code inspection confirmed that it was: the event timestamp and polarity
were based on the first threshold crossing, not the maximum-flux point.

**Status:** The experiment was not approved. The discovery caused the design
discussion to stop and shifted attention to verifying mechanisms and their
assumptions.

## Decisions established by the interview

- Templates performs contextual target identification, not general waveform
  classification.
- Operator-provided position, orientation, and direction identify the one
  physically possible next marker.
- Wrong polarity, below-minimum target flux, and physically impossible early
  arrival are hard exclusions and must not move the marker count.
- Ordinary timing is confirmatory at steady speed; acceleration, deceleration,
  startup, and peer-caused speed changes can make it unavailable or unreliable.
- The first confirmed marker after an operator-defined start establishes the
  timing anchor.
- The physical earliest-arrival rule uses the upcoming interval's measured
  length and the locomotive's demonstrated maximum speed at the applicable PWM,
  with a 10 percent faster allowance.
- All route intervals must be remeasured centerline, magnet center to magnet
  center, to the nearest 5 mm.
- Otto and Toby maximum-speed tests use fresh batteries, no consist, both
  directions, and PWM 50–120 in increments of 10, one lap per PWM per direction.
- Maximum-flux identification and its timestamp must be established before new
  timing or flux evidence is treated as valid.
- Evidence categories must be recorded separately during development; they are
  not equal votes.

## Questions still unresolved

1. Exactly how a complete Hall excursion begins and ends.
2. Whether raw-sample processing or a permissive excursion collector is the
   workable ESP input boundary.
3. Whether a response passing all hard gates but lacking full confirmation may
   move the count provisionally.
4. How minimum flux is measured and established for each target magnet.
5. Whether the maximum-flux point is sufficiently stable and repeatable to be
   the navigation timestamp.
6. Whether direction-, orientation-, grade-, or interval-specific speed models
   are necessary after testing.
