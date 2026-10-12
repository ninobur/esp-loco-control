# NGR Navigation Architecture Design Brief

## Version 1.2 — June 6, 2026

### David Brown | Claude (Anthropic) | Sam (ChatGPT)

-----

## Project History and Underlying Philosophy

The NGR project has followed a consistent internal logic since its beginning, though that logic only became fully visible in retrospect.

The earliest architecture used RSSI — signal strength between locomotives and trackside nodes — to establish relative position awareness. It is tempting in hindsight to view this as a failed branch. It was not. The RSSI work established the foundational principles that everything since has built on:

- Locomotives can collect information themselves
- Locomotives can maintain state
- Locomotives can make local decisions
- Intelligence does not have to reside in trackside infrastructure
- Distributed reasoning is possible

The trackside ESP/reed architecture that preceded Hall sensors was still fundamentally **the railroad telling the locomotive what is happening.** The Hall sensor architecture flipped that relationship: **the locomotive observes the railroad directly.** That is a profound shift, and everything since has been an extension of it.

Once the Hall sensor proved reliable, the obvious next question was: if the locomotive can observe one thing, what else can it observe?

First came block boundaries. Then truth clusters. Then missed markers. Now direction. Eventually hemispheric location. And perhaps someday confidence estimates about its own position.

The mile markers are not the goal. They are information infrastructure. The magnets themselves are simply evidence — physical facts embedded in the railroad that a sufficiently attentive locomotive can read.

A human engineer does not know where he is because of one observation. He knows because he continuously accumulates evidence:

> I just left Patio. I passed three mileposts. I’m traveling CCW. I crossed Arches. The grade is descending. I expect Grillers next.

Each observation reinforces the others. That is what the mile-marker system is beginning to provide.

The PWM discussion that prompted this brief fits the same pattern. PWM context is not a separate idea — it is another layer of evidence. Each addition has followed the same underlying philosophy: **better decisions require more information.** RSSI added information. Hall sensors added information. Truth clusters added information. Missed-MM detection added information. PWM context will add information.

None of these are separate projects. They are all manifestations of the same discovery: give the locomotive enough evidence that it can form an increasingly accurate understanding of its own situation.

**The version 2.0 designation is appropriate** because the jump from interval classifier to a navigation-aware system is a genuine architectural boundary. The 1.x series proved the locomotive could read the railroad. The 2.x series will prove it can reason about what it has read.

-----

## The Architectural Turning Point (June 6, 2026)

During v1.8 field testing, the design discussion crossed from “better timing classification” into a true navigation architecture. The turning point was a single sentence:

> **Timing provides confidence context around the count — not permission to count.**

This inverts the 1.x relationship:

**1.x world:**

```
Magnet → Timing classifier → Navigation
```

**2.0 world:**

```
Magnet → Navigation count (always) → Timing confidence layer
```

The locomotive never un-sees a magnet. A Hall event is a physical observation. The timing model may say high confidence, medium confidence, low confidence, or unexpected — but it does not get veto power over the observation.

-----

## Purpose

This brief defines the 2.0 architecture for the NGR milepost monitor system. It was prompted by field testing of MM_Loco_1_8_Truth and a collaborative design discussion identifying a fundamental flaw in the current model: **timing state and navigation state are being treated as the same thing. They are not.**

This brief defines how to separate them, and what each layer should contain, survive, and do.

-----

## The Core Problem

The current sketch uses a single unified state model. When timing breaks — due to ramping, station stop, speed change, or grade — the entire model resets. The locomotive forgets not just its speed estimate but its location awareness.

David’s freeway analogy captures this precisely:

> “When I am traveling at 70 mph, I know exactly where I am. But if I slow down or take an exit, I am completely lost.”

A navigation system that responds to a speed change with “position unknown” is fundamentally wrong. **The speed estimate changed. The position did not.**

Additionally, the current RAMPING_MM classification makes the locomotive perceptually blind during speed transitions. This is backwards: landmarks are most important during braking and acceleration, not least. A locomotive approaching a station while decelerating needs truth-cluster recognition more than at any other time.

Field testing of v1.8 confirmed this: five separate runs demonstrated complete truth clusters detected during active ramping, both acceleration and deceleration. The architecture is already arguing with itself — the code simultaneously claims it doesn’t trust timing and uses timing to identify a landmark. That contradiction must be resolved in 2.0.

-----

## The Two-Layer Architecture

### Layer 1 — Navigation State (persistent)

The locomotive’s awareness of where it is and where it is going. This layer survives speed changes, ramps, station stops, reseeds, and timing relocks.

**Contents:**

- `absoluteTruthPosition` — position set by last confirmed truth cluster. Truth never bends to count; count bends to truth.
- `relativeMagnetCount` — running count of every detected magnet, advancing regardless of timing confidence
- `provisionalCountOffset` — running tally of provisional missed magnets awaiting confirmation
- `navPolarityHistory[]` — rolling buffer of last N polarities from MILE and MM_EXIT events
- `navLastLandmark` — last confirmed truth cluster identity
- `navDirection` — CW / CCW / UNKNOWN
- `navPattern` — last recognized polarity pattern (NNS, SNN, NNN, SSS, etc.)
- `navigationConfidence` — decaying score, not binary flag

**Every detected magnet advances relativeMagnetCount** — MILE, TRUTH, MISSED_MM, RAMPING_MM, all of them. No magnet is un-seen.

**Truth clusters update absoluteTruthPosition immediately.** The count corrects to match the landmark. The correction delta is logged as diagnostic data — how far off was dead reckoning before the landmark corrected it? That error is valuable information.

**What reduces navigationConfidence:**

- Full RESET when PWM is above locomotion threshold (suspicious gap)
- Multiple consecutive timing relocks without intervening landmarks
- Prolonged operation without a truth cluster confirmation

**What does NOT reset navigation:**

- Speed change
- Ramp start or end
- TIMING_RELOCK
- Station stop (PWM commanded to zero — timing pauses, navigation preserved)
- Grade or load change
- Seed buffer clear

### Layer 2 — Timing Model (disposable)

The locomotive’s current estimate of when the next magnet should arrive. This is a measuring instrument, not a source of truth. It may be paused, reset, or recalibrated without affecting Layer 1.

**Contents:**

- `predictedIntervalMs` — fusion of observed history and PWM intent (see below)
- `intervalHistory[]` — last 8 valid MILE intervals (excludes truth, gaps, anomalies)
- `intervalTrend` — slope of recent interval deltas
- `intervalVariance` — recent spread
- `predictionConfidence` — HIGH / MEDIUM / LOW
- `predictionWindowLow`, `predictionWindowHigh` — elastic bounds
- `consecutiveMissedLike` — missed-MM relock counter
- `consecutiveSeedRejects` — seed-stall relock counter
- `timingRelockPending` — flag set at ramp start

**What resets the timing model:**

- Ramp start (throttle target changes)
- TIMING_RELOCK (repeated missed-like or seed-stall)
- PWM commanded to zero → TIMING_PAUSE (not RESET — navigation preserved)
- Long gap when PWM is above locomotion threshold → RESET

**What does NOT reset the timing model:**

- Navigation count
- Polarity history
- Absolute truth position

-----

## PREDICTED_INTERVAL — The Core of the Timing Model

The 1.x series treated the seed as truth. It is not. The railroad is truth. The seed is an early estimate that should be continuously refined.

**PREDICTED_INTERVAL** replaces the static seed. It is the locomotive’s best estimate of when the next magnet should arrive, built from two sources:

### Source 1 — Observed interval history

Last 8 valid MILE intervals (excluding truth clusters, reset gaps, obvious anomalies):

- `recentMean` — average of the 8
- `recentTrend` — slope of recent deltas (positive = slowing, negative = speeding)
- `recentVariance` — spread

Example — steady speed:

```
800, 805, 795, 810, 802, 799, 806, 804
→ predicted ≈ 803ms, tight window
```

Example — decelerating:

```
800, 850, 900, 960, 1030, 1110, 1200, 1300
→ predicted ≈ 1400ms (trend-adjusted upward)
```

Example — accelerating:

```
1500, 1350, 1220, 1100, 980, 900, 840, 800
→ predicted ≈ 750ms (trend-adjusted downward)
```

### Source 2 — PWM context (intent)

PWM is not position and not speed. It is **intent**. It tells the locomotive what the operator is commanding, which shapes how to interpret the next observation.

- `pwmCurrent`, `pwmTarget`, `dPWM`, `pwmRate`
- `locoMinMotionPwm` — per-loco config value below which no magnets are expected
- Whether PWM is rising (predict shorter intervals), falling (predict longer), or steady (predict near mean)

**Combined prediction:**

```
PREDICTED_INTERVAL =
    recentMean
    + trendAdjustment
    + pwmContextAdjustment
```

Where `pwmContextAdjustment` bends the prediction based on commanded direction of speed change.

### The elastic confidence window

The window within which an observed interval is considered plausible:

```
allowedGap = PREDICTED_INTERVAL × marginForCurrentPWMState
```

The margin widens when:

- PWM is changing rapidly
- Recent interval variance is high
- Locomotive/consist is newly calibrated
- Direction or grade is known to be variable

The margin narrows when:

- PWM is stable at target
- Recent MM intervals are consistent
- The loco has many good MILE observations
- Confidence is high

**The fixed 3000ms RESET threshold is replaced by this elastic threshold.** A 3500ms gap means something very different at PWM 80 vs PWM 0.

### The log shows numbers, not buckets

```
dt=955  pred=940  window=780-1120  pwm=108->108  conf=HIGH  class=MILE
dt=1775  pred=1350  window=950-1800  pwm=62->110  conf=LOW  class=MILE_TREND
```

Categorical states (STARTING, MOVING_STEADY, DECELERATING) are derivable for human readability but the decision logic uses the actual numbers.

-----

## Provisional Missed Magnets — Dead Reckoning

When PREDICTED_INTERVAL elapses with no magnet detected:

1. Log `PROVISIONAL_MM` — count advances by 1
1. Watch next 3 observed magnets
1. If pattern confirms: `CONFIRMED_MM` — provisional becomes permanent
1. If pattern contradicts (truth cluster appears too soon, impossible polarity sequence): `RETRACT_MM` — count corrected, correction logged

This mirrors how a human engineer reasons:

> “I expected a marker by now. I’ll assume I missed one and keep watching.”

The provisional system allows dead reckoning between observations while remaining correctable by evidence.

-----

## Truth Clusters as Navigation Anchors

This is the most important architectural change from 1.x to 2.0.

**In 1.x:** Truth clusters are a special timing pattern to be detected.

**In 2.0:** Truth clusters define reality. They are landmarks that correct the count, not the other way around.

When a truth cluster is detected:

```
absoluteTruthPosition = this landmark
relativeMagnetCount corrected to match
correctionDelta logged as diagnostic data
navigationConfidence → HIGH
```

**Truth never bends to count. Count bends to truth.**

The correction delta is not discarded — it is the diagnostic record of how much dead reckoning drifted before the landmark corrected it. This data becomes increasingly valuable as the system matures: it reveals grade effects, battery behavior, magnet placement accuracy, and classifier performance.

Sam’s refinement: “Truth points reset navigation certainty” — not history. The locomotive retains the full record of where it thought it was and how far off it was. That audit trail is valuable.

-----

## Station Stops

A station stop (PWM commanded to zero) is fundamentally different from a lost-pattern event (PWM nonzero, no magnets arriving).

**Current behavior (wrong):** Both produce `RESET reason=long_gap`. Navigation context lost in both cases.

**Correct behavior:**

- PWM = 0, long gap → `TIMING_PAUSE reason=station_stop`. Timing model clears. Navigation preserved. `absoluteTruthPosition`, `relativeMagnetCount`, `navDirection`, `navigationConfidence` all survive.
- PWM > locoMinMotionPwm, long gap → `RESET reason=unexpected_gap`. Timing and navigation confidence both affected.

The distinguishing information is already on the ESP32 — `pwmTarget` and `rampCurrent`. If PWM is zero because it was commanded to zero, that is a station stop. If PWM is nonzero and no magnets are arriving, that is suspicious.

-----

## Per-Interval PWM Logging

Each magnet interval carries the PWM state across that interval. This turns raw dt into a contextual measurement.

**Log both endpoints:**

- `pwmA` — PWM at previous magnet
- `pwmB` — PWM at current magnet
- `target` — current ramp target
- `dPWM` — pwmB - pwmA (signed)

**Example:**

```
MAG 140  S  dt=1377  pwm=76->63  target=42  dPWM=-13  pred=1200  window=900-1600  conf=LOW  class=MILE_TREND
```

**PWM is not portable.** The same PWM produces different speeds in different locomotives due to motor characteristics, battery state, gear ratios, and load. The Toby vs Hans timing run confirmed this. Per-interval PWM context only becomes useful after local calibration for that specific locomotive, consist, and run.

-----

## Per-Locomotive Calibration

At the start of each session, a calibration run establishes the motion profile for this locomotive at this run:

- `locoMinMotionPwm` — PWM below which no motion occurs (config variable, not derived)
- Expected MM interval at each operating PWM
- Normal variation at fixed PWM (grade, curve, battery drain, load)
- Variation by direction (CW vs CCW — grade asymmetry)
- Ramp response curve

This profile becomes the foundation for `PREDICTED_INTERVAL` and the elastic confidence window.

-----

## Truth Detection During Ramping

**Field-validated finding (June 6, 2026):** Five separate runs confirmed complete truth clusters detected during active ramping, both acceleration and deceleration. Ratios remained well below 0.50 in all cases (0.24–0.46).

**The conclusion:** Truth cluster detection is not dependent on steady-state timing. The ~100mm truth spacing produces intervals that are physically short regardless of train speed. Ramping does not make truth clusters invisible.

**Two implementation options** (to be validated by Phase A instrumentation data before choosing):

- **Option A** — Absolute threshold (e.g. dt < 450ms always → truth candidate). Simple but may fail at very slow approach speeds.
- **Option B** — Local ratio from recent RAMPING_MM intervals. More robust, requires small local buffer.

Option B is the expected final choice. Option A may serve as a transitional implementation.

-----

## Sam’s Critiques of v1.1 (incorporated)

1. **Do not implement navigation state yet.** Phase C is architecturally correct but premature. A wrong counter creates false confidence. Validate ramping truth and station stop behavior first.
1. **Phase A is exactly right.** Per-interval PWM logging only. Pure instrumentation.
1. **“Truth is speed-invariant” needs proof.** Now partially validated by field data. Continue validating at very low speeds and station approaches.
1. **Option A absolute threshold is risky.** Log first, then choose. Option B is probably more durable.
1. **Navigation confidence should decay, not binary reset.** Incorporated — `navigationConfidence` is a decaying score.

-----

## Implementation Sequence

**Phase A — Instrumentation (MM_Loco_1_9)**

- Log `pwmA`, `pwmB`, `target`, `dPWM` on every MAG event
- No classification changes
- Pure data collection to establish per-loco PWM/interval relationship
- Validate truth cluster ratios during ramping at various PWM rates

**Phase B — PREDICTED_INTERVAL and elastic threshold (MM_Loco_2_0)**

- Replace EMA seed with 8-interval history + trend + PWM context
- Replace fixed 3000ms RESET with elastic threshold
- Introduce TIMING_PAUSE for station stops (PWM = 0)
- Truth detection during ramping using Option A or B (decided from Phase A data)

**Phase C — Navigation state layer (MM_Loco_2_1)**

- Add `absoluteTruthPosition`, `relativeMagnetCount`, `provisionalCountOffset`, `navigationConfidence`
- Separate clear paths: `clearTimingModel()` vs `clearNavigationState()`
- Provisional missed magnets with 3-observation confirmation
- Truth clusters correct count and log correction delta

**Phase D — Pattern recognition (MM_Loco_2_2)**

- Rolling polarity window: last 5-7 non-truth positions
- Recognize NNS, SNN, NNN, SSS
- Log pattern and confidence — no operational decisions yet

**Phase E — Direction awareness (MM_Loco_2_3)**

- After sufficient pattern confirmations, log `navDirection = CW` or `CCW`
- Confirmed by truth landmark consistency

-----

## What MM_Loco_1_8_Truth Resolved

v1.8 field tests (June 6, 2026) — all passed:

|Test                               |Result                           |
|-----------------------------------|---------------------------------|
|Baseline sanity — MILE/TRUTH rhythm|✅ PASS                           |
|Ramp-start clear — no stale seedAvg|✅ PASS                           |
|Seed-stall relock                  |✅ PASS (confirmed twice in field)|
|Station stops — timing recovers    |✅ PASS                           |

**Additional findings not in original test plan:**

- Five complete truth clusters detected during ramping — both acceleration and deceleration
- MAG 74/84 exposed the design contradiction: code simultaneously claims it doesn’t trust timing and uses timing to identify a landmark
- Station stop data made the timing/navigation separation argument concrete rather than theoretical

v1.8 is the final 1.x sketch. 2.0 begins with Phase A above.

-----

## Hemisphere Boundary Architecture

The NNN/SSS transition marker concept is abandoned. Pattern changes inferred from MM rhythm alone are ambiguous. Named truth points authorize hemisphere transitions.

**MM rhythms:**

- South-to-north hemisphere: `SSN`
- North-to-south hemisphere: `NNS`

**Named boundary markers:**

- Northpoint: `NN` truth pair
- Southpoint: `SS` truth pair

**Validation rule:**

```
SSN → NORTHPOINT NN → NNS   valid
SSN → NNS without NORTHPOINT   suspicious
NNS → SOUTHPOINT SS → SSN   valid
NNS → SSN without SOUTHPOINT   suspicious
```

The locomotive does not infer a hemisphere crossing from a rhythm change alone. It requires the authorizing named point. Three layers of evidence: timing, polarity sequence, named point.

-----

## Station Zone Architecture — Operational Payoff

The navigation system’s ultimate purpose is not knowing where the locomotive is — it is enabling operations that were previously impossible or required constant manual calibration.

**The problem with timer-based stops:** Timers require tuning per locomotive, per consist, per battery state, per grade, per arrival speed. A consist that arrives fast needs more time than one that arrives slow. There is no general solution.

**The MM-based solution:** Count physical distance, not elapsed time. Check actual speed at each marker and adjust. The locomotive self-calibrates on every run.

### Station Zone Model

```
Truth point:              6 MM before station center
Station speed zone entry: 4 MM before station center
Direction-specific stop:  varies by CW/CCW and consist length
Station speed zone exit:  4 MM after station center
```

**At the truth point:** station profile armed, countdown begins, service type determines local vs express behavior.

**Inside the zone:** all trains operate at restricted station speed. Final stop becomes predictable because arrival speed is always the same.

**Direction-specific stop points:** the locomotive is at one end of the consist. Passenger cars must be adjacent to the platform. The engine stops at different offsets depending on direction of travel and consist length.

### Two Service Modes

**Local service:**

```
Truth point → slow to station speed by zone entry
→ stop at direction-specific platform point
→ dwell
→ depart at station speed
→ ramp up after zone exit
```

**Express service:**

```
Truth point → slow to station speed by zone entry
→ pass station at restricted speed
→ ramp up after zone exit
```

This recovers Circuit Express behavior: express respects the zone without stopping.

### Two Implementation Stages

**Stage 1 — Countdown stop** (buildable once Phase C logical MM counting is stable):

```cpp
stopCountdown = 6;
stopArmed = true;
// on each logical MM: decrement, adjust PWM at thresholds, stop at zero
```

**Stage 2 — Glide-path stop** (requires per-loco calibration data from 1.9):
Check actual measured speed at each intermediate marker. Adjust PWM toward planned stop curve. Every marker becomes a correction point.

### StationProfile Struct

```cpp
struct StationProfile {
    const char* name;
    int approachTruthCW;
    int approachTruthCCW;
    int zoneEntryOffsetFromTruth;
    int stationCenterOffsetFromTruth;
    int zoneExitOffsetFromTruth;
    int stopOffsetCW;
    int stopOffsetCCW;
    int stationSpeedPWM;
};
```

-----

## Bidirectional CTO and Self-Determining Locomotives

The navigation architecture enables bidirectional CTO without dispatcher direction of course. Locomotives placed on the track in either direction:

- Self-determine direction from the polarity pattern
- Self-identify position from truth clusters
- Apply correct direction-specific station stop offset
- Self-negotiate roles (leader/follower, local/express)
- Self-detect CE transition and redetermine roles

The dispatcher starts, stops, and initiates CE. Everything else is locomotive behavior. Navigation awareness adds direction and position to what the locomotives already reason about autonomously.

A Y junction becomes viable: a reversing move produces a polarity pattern inversion. The locomotive detects it, updates its direction model, and applies the correct stop offsets for the new direction — without being told.

-----

- **Intelligence stays in the locomotives.** Pi is observer and interface.
- **A magnet detection is always a navigation event.** Timing provides confidence context, not permission to count.
- **The interval is a speed measurement, not a fixed target.** The railroad is truth, the seed is an early estimate.
- **Navigation state is not timing state.** Speed changes affect the measuring instrument, not the locomotive’s knowledge of where it is.
- **Truth clusters define reality.** Count bends to truth. Truth never bends to count.
- **PWM is intent, not speed.** It shapes the confidence window; it does not veto observations.
- **Truth detection is speed-invariant.** ~100mm physical spacing produces short intervals regardless of train speed. Ramping cannot make truth clusters invisible.
- **Station stop is not a lost-pattern event.** PWM context distinguishes them. Timing pauses; navigation does not reset.
- **One clean change at a time.** Each phase above is a separate, field-validated revision.

-----

*Version 1.3 — Added hemisphere boundary architecture, station zone model, bidirectional CTO, and StationProfile struct. David Brown, Claude, Sam, June 6, 2026.*