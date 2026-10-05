# NAVI Intelligent Braking Architecture

Status: Governing architecture for NAVI station stopping/braking (2026-10-05).

Revision 2 (2026-10-05): adaptive stopping model. The braking model is adaptive
during the stop itself and is not station-calibrated. See
[Superseded concepts](#superseded-concepts) for what this revision replaces.

Related: [`NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md`](NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md) (NAVI position/overlay model, §4–§5).

> **Respect the intelligence of NAVI.**
>
> **Sensors provide observations.**
>
> **NAVI interprets those observations in context, predicts the physical consequence of its current action, and adjusts its action accordingly.**
>
> **For braking: The ramp provides smoothness. IR provides evidence. NAVI provides judgment.**

## Purpose

NAVI must bring a locomotive to a smooth, visually natural stop at a desired physical location.

These are the two primary objectives:

1. smooth deceleration;
2. accurate stopping location.

The braking architecture separates these responsibilities:

> **The ramp provides smooth deceleration. IR provides physical evidence that allows NAVI to adjust the ramp so that the locomotive stops where intended.**

IR does not directly operate the throttle.

NAVI remains the decision-making agent.

## Governing principle: respect the intelligence of NAVI

IR is a sensor and source of evidence.

Hall is a sensor and source of position evidence.

Neither sensor is an operating agent.

NAVI is the engineer.

Sensors provide observations. NAVI interprets those observations in context, predicts the physical consequence of its current action, and adjusts its action accordingly.

The objective is not to force the locomotive to match a succession of instantaneous speed targets.

The objective is to manage a stopping trajectory.

## Human-engineer model

This architecture is intended to reproduce the essential judgment used by an experienced human operator.

When manually stopping the locomotive, the operator does not continually command an exact instantaneous speed.

Instead, the operator:

1. begins a known smooth deceleration;
2. observes how rapidly the train is slowing;
3. observes how much braking is being applied;
4. observes physical progress toward the desired stopping point;
5. subconsciously estimates whether the current braking trajectory will stop long, on target, or short;
6. adjusts the brake accordingly;
7. observes the response and adjusts again.

In NAVI, there is no separate brake actuator.

Brake modulation means changing the slope of the PWM-down ramp.

IR provides NAVI with the frequent physical observations that the human operator obtains visually.

The operator then accepts the physical stop when it occurs.

## Adaptive model requirement: works anywhere

The braking system must be adaptive during the stop itself.

> **The NAVI braking model must be adaptive rather than station-calibrated.**

NAVI should not depend upon station-specific braking calibrations, direction-specific stopping tables, predetermined stopping PWM values, or a previously learned braking model for a particular location.

The objective is a braking controller that can work anywhere on the railroad because NAVI observes how the locomotive is responding during the braking maneuver currently underway.

Do not design the architecture around:

* Grillers CW braking constants;
* Arches CCW braking constants;
* station-specific stopping PWM;
* direction-specific PWM tables;
* fixed assumptions about the PWM at which a particular locomotive stops.

Field data from particular locations may be used to evaluate the architecture, but it must not become required location-specific control data unless future evidence demonstrates that such specialization is necessary.

Each braking maneuver provides NAVI with new evidence about the current physical response.

The current stop should help NAVI improve that same stop.

## Lesson from the first IR-controlled stopping experiment

The first experimental implementation directly compared measured IR speed with a continuously changing target speed and adjusted PWM upward or downward to chase that target.

Field operation produced visible undulation:

* locomotive slowed;
* target controller attempted correction;
* locomotive stopped prematurely;
* controller increased PWM;
* locomotive restarted;
* process repeated.

The fundamental conceptual error was:

> **IR was adjusting speed/PWM rather than informing NAVI how to adjust the braking ramp.**

That architecture is rejected. This architecture supersedes that control concept.

## Ramp control, not speed control

The adaptive model modifies the ramp slope. It does not use IR speed error to change PWM directly.

* If the projected stop is **LONG**: steepen the PWM-down ramp.
* If **ON_TARGET**: maintain the current ramp slope.
* If **SHORT**: flatten the PWM-down ramp.

During final braking:

> **PWM itself may hold or decrease. It must not increase.**

NAVI may ease braking by flattening the ramp, but it does not reapply throttle.

## Braking ramp as the primary mechanism

NAVI uses a smooth PWM-down ramp as the primary braking mechanism.

### Nominal ramp

The existing manual-control deceleration ramp remains the starting model because its physical behavior is already known.

Its known characteristic is useful: left substantially unmodified, it tends to produce a smooth but overly long coast. When operated manually, the human operator applies additional braking to stop where intended.

That is desirable as a nominal starting point because NAVI can apply additional braking as evidence develops.

The nominal ramp should therefore aim somewhat long.

NAVI then trims the developing trajectory toward the desired stopping point.

The nominal ramp is a generic starting trajectory, not a station- or direction-specific calibration.

## What NAVI controls

During an active braking maneuver, NAVI controls the slope of the PWM-down ramp.

The ramp slope may be adjusted in either direction:

* steeper ramp = PWM is removed faster = stronger braking;
* shallower ramp = PWM is removed more slowly = weaker braking.

This does not mean that NAVI reverses braking and applies throttle.

Once final braking has begun:

> **Commanded PWM may remain unchanged or decrease. It must not increase.**

Thus:

* ramp slope is adjustable in either direction;
* PWM remains monotonic non-increasing.

The “brake” is therefore not a separate actuator.

Brake application means adjustment of the PWM-down ramp slope.

## Role of IR

IR provides NAVI with frequent physical observations during the braking maneuver.

With the current wheel/sensor geometry:

```
1 completed IR pulse ≈ 9.652 mm of locomotive travel.
```

At each new completed IR pulse, NAVI gains another landmark.

IR observations are necessarily retrospective: a completed pulse reports approximately 9.652 mm of travel that has already occurred.

## Governing adaptive relationship

During braking, NAVI observes two related changing quantities.

**Control action:**

* PWM is declining according to the current braking ramp.

**Physical response:**

* IR-measured locomotive speed is declining in response.

The useful relationship is therefore the recent response of physical speed to the PWM reduction:

```
Δ IR speed / Δ PWM
```

This relationship allows NAVI to estimate how the locomotive is responding to the braking effort during this particular stop.

It must not be treated as a universal locomotive constant.

The relationship may differ because of:

* grade;
* direction;
* battery condition;
* consist/load;
* mechanical condition;
* track condition;
* locomotive differences;
* other physical circumstances.

NAVI does not need to identify which factor caused the difference.

It observes the resulting physical response.

## Forward-looking judgment

The purpose of the adaptive model is not merely to describe past deceleration.

NAVI should use the recent relationship between:

* current PWM;
* PWM ramp slope;
* current IR speed;
* recent change in IR speed;
* recent change in PWM;
* current IR distance;
* remaining distance to the desired stop;

to answer:

> **Given my present position, present speed, and the way the locomotive is responding to my current braking ramp, where am I presently tending to stop?**

That is the fundamental braking judgment.

### Avoid excessive backward-looking filtering

Do not make a median-of-three IR speed estimate the governing basis for braking judgment.

IR observations are already necessarily retrospective because a completed pulse represents approximately 9.652 mm of travel.

A multi-sample median adds further delay.

During a short stopping maneuver, excessive smoothing can make NAVI respond to where the locomotive was rather than where its current trajectory is taking it.

The braking architecture should favor the most recent useful evidence and recent changes.

Noise handling may still be necessary, but it should not destroy responsiveness.

The design objective is:

> **recent evidence used to make a forward-looking prediction**

rather than:

> heavily filtered historical evidence used to describe the past.

## NAVI braking judgment

At each useful IR observation, NAVI classifies the developing stop.

### LONG

The current trajectory indicates that the locomotive is tending to stop beyond the desired stopping location.

NAVI increases braking:

> steepen the PWM-down ramp.

### ON TARGET

The current trajectory is consistent with the desired stopping location.

NAVI:

> maintains the current ramp slope.

### SHORT

The current trajectory indicates that the locomotive is tending to stop before the desired stopping location.

NAVI reduces braking:

> flatten the PWM-down ramp.

Flattening the ramp does not authorize increasing PWM.

NAVI simply removes PWM more slowly.

## Bounded IR authority

The nominal ramp remains the primary stopping trajectory.

IR allows NAVI to adapt that trajectory modestly to actual physical conditions.

IR-informed adjustment is limited to:

```
±20% of the nominal ramp rate.
```

This means NAVI may make the braking ramp up to 20% steeper or 20% shallower than nominal.

This limit applies to ramp slope, not PWM magnitude.

It does not mean that IR may command PWM ±20%.

It does not authorize an increase in PWM during final braking.

## NAVI reasoning cycle

The intended control loop is:

```
OBSERVE → JUDGE → ADJUST RAMP → OBSERVE AGAIN
```

More specifically:

1. Begin with the known nominal braking ramp.
2. Receive a new IR physical observation.
3. Compare current physical response with recent PWM reduction.
4. Estimate the developing stopping trajectory.
5. Judge LONG / ON_TARGET / SHORT.
6. Adjust ramp slope within the ±20% authority.
7. Continue the smooth monotonic PWM reduction.
8. Observe the next physical response.
9. Repeat until physical stop.

This is transparent judgment, not sensor authority.

## Station STOP operating profile

The current station STOP geography has three regions.

### Region 1 — approach braking

At:

```
Station −10
```

NAVI begins one continuous approach braking ramp from cruise speed.

The desired arrival at:

```
Station −5
```

is station speed:

```
25 pKPH
```

There are no intermediate MM speed steps.

IR observations during this region allow NAVI to adjust the ramp slope so that the locomotive approaches approximately 25 pKPH at Station −5.

The Hall/MM observations establish the geographic boundaries.

IR provides the finer observations between them.

### Region 2 — station cruise

From:

```
Station −5 through Station 0
```

the locomotive maintains approximately:

```
25 pKPH
```

This is a stable station-approach speed.

There is no progressive MM-by-MM crawl through 20, 15, 10, and 5 pKPH.

The locomotive reaches Station 0 in a stable and predictable operating condition.

### Region 3 — final braking

The accepted Hall observation at:

```
Station 0
```

begins the final braking ramp.

The initial field-development stopping target is:

```
35 completed IR pulses after Station 0.
```

At 9.652 mm/pulse:

```
35 × 9.652 mm ≈ 337.8 mm
```

This is a target stopping location, not a mandatory odometer checkpoint.

This value is a field-development target and may later be tuned from operational evidence.

## Final braking behavior

At Station 0:

1. NAVI establishes the IR distance reference.
2. NAVI begins the nominal smooth PWM-down braking ramp.
3. The nominal ramp is intentionally biased somewhat long.
4. Each useful IR observation provides new distance and movement evidence.
5. NAVI compares the current physical response with the recent PWM reduction and estimates where the locomotive is tending to stop.
6. NAVI judges whether the developing stop is LONG, ON TARGET, or SHORT.
7. NAVI adjusts the ramp slope within ±20% of nominal.
8. PWM itself may only hold or decrease.
9. NAVI continues this process until physical movement stops.

The desired stopping distance guides the trajectory.

It is not a mandatory odometer checkpoint.

## Predicting physical stop

A locomotive does not provide a positive IR event saying “I have now stopped.”

When movement ceases, new spoke pulses cease.

Therefore physical stop detection is inherently partly based on absence of subsequent movement evidence.

NAVI should not wait until PWM reaches zero to decide that the locomotive has physically stopped.

The locomotive may physically stop while substantial PWM remains—for example, field observation indicates Otto may cease movement around PWM 30 under some conditions.

> **PWM = 0 is not the definition of physical stop.**

The PWM at which movement ceases is itself an observation of the current stop, not a fixed locomotive constant.

Instead, during braking NAVI should already have a forward-looking estimate of where movement is tending to cease.

The recent relationship between declining PWM and declining IR speed can support an estimate of the PWM region and physical position at which IR speed will reach zero.

Conceptually:

```
current physical response + current braking slope → predicted physical stop
```

The eventual absence of further IR movement then confirms the prediction.

## Physical stop is terminal

A locomotive may physically stop before the nominal target distance because PWM has fallen below the level required to sustain movement.

That is normal physical behavior.

Once NAVI determines that physical movement has ceased during the final stopping maneuver:

1. the stop is complete;
2. command PWM 0;
3. begin the applicable dwell;
4. do not restart the locomotive to reach an exact odometer target.

This remains a governing rule.

NAVI must not:

* increase PWM;
* restart the locomotive;
* hunt forward for another IR pulse;
* attempt to reach an exact mathematical stopping distance.

This rule prevents the stop/restart undulation observed in the first experimental controller.

## Hall and IR responsibilities

Hall and IR have complementary roles.

### Hall

Hall provides authoritative MM landmarks that initiate or delimit operating maneuvers.

For the current station profile:

* Station −10 starts approach braking;
* Station −5 marks arrival at station cruise;
* Station 0 starts final braking.

### IR

IR provides fine-grained movement evidence between MM landmarks.

IR contributes:

* distance;
* measured movement/speed;
* repeated physical landmarks approximately every 9.652 mm;
* the physical response to the current braking ramp.

IR informs NAVI’s judgment.

IR does not independently command propulsion.

## Relationship to the position/overlay architecture

This braking architecture is consistent with the governing NAVI position/overlay model.

The dispatcher owns the operating overlay.

NAVI is the engineer and executes the applicable operating instruction.

A STOP overlay defines the station operating requirements geographically.

The station approach therefore remains position-defined:

* approach braking begins at the applicable MM;
* station cruise applies across its defined MM intervals;
* final braking begins at the defined stopping reference.

The short-lived braking execution state exists because smooth physical braking occurs at finer resolution than MM intervals.

It does not establish station identity.

It does not create historical station authority.

It does not justify persistent station latches or phase authority across unrelated STOP/GO or overlay changes.

## Relationship to PWM

PWM is an actuator output.

PWM is not the operating language of the station overlay.

The overlay defines operating requirements such as:

* cruise;
* station speed;
* stop;
* dwell;
* pass.

NAVI determines the PWM required to execute those instructions.

During braking, NAVI deliberately controls the rate at which PWM is removed.

## Telemetry and transparent judgment

The braking process should be observable.

Telemetry should make NAVI’s judgment understandable.

Useful fields include:

* current MM / station-relative position;
* IR pulse count since braking reference;
* IR distance since reference;
* measured IR speed;
* current PWM;
* recent change in IR speed and recent change in PWM (the observed physical response);
* nominal ramp rate;
* current adjusted ramp rate;
* permitted ±20% adjustment range;
* remaining target distance;
* projected stopping location or stopping error;
* judgment:
  * `LONG`
  * `ON_TARGET`
  * `SHORT`
* physical-stop recognition;
* dwell transition.

The purpose is not merely debugging.

Transparent telemetry allows the operator to understand why NAVI changed the braking ramp.

## Simplicity requirement

Do not assume that this architecture requires a PID controller or another elaborate generic control system.

The intended reasoning is simple:

> **Start with a known smooth ramp. Observe the developing stop. Adjust the ramp modestly. Observe again.**

The implementation should preserve that conceptual simplicity unless field evidence demonstrates that additional complexity is necessary.

## Architecture versus implementation

This document defines what NAVI must reason about and what authority each component has.

It deliberately does not define the exact implementation mathematics.

The exact predictive estimator—how NAVI computes the projected stopping location and predicted physical stop from recent PWM and IR evidence—is an implementation design to be reviewed separately against this architecture.

## Superseded concepts

This architecture supersedes any language, prior design, or implementation suggesting that:

* a median-of-three (or other heavily smoothed, multi-sample) IR speed estimate should govern braking judgment;
* PWM must reach zero before physical stop can be recognized;
* IR should directly adjust PWM to chase an instantaneous speed target;
* stopping requires reaching an exact pulse/distance checkpoint;
* station-specific or direction-specific calibration (stopping PWM, braking constants, or a fixed stall PWM) is the intended long-term braking model.

## Architectural summary

The station braking architecture is:

```
Hall tells NAVI when to begin.
The ramp provides smooth deceleration.
IR tells NAVI how the stop is developing and how the locomotive is responding to braking.
NAVI predicts where the current trajectory will stop.
NAVI adjusts the ramp slope to converge on the desired stopping point.
PWM never increases during final braking.
A physical stop ends the maneuver.
```

> **The ramp provides smoothness. IR provides evidence. NAVI provides judgment.**

The design objective remains:

> **A smooth, visually natural deceleration that stops the locomotive close to the intended physical location.**
