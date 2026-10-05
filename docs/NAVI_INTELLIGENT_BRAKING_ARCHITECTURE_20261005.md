# NAVI Intelligent Braking Architecture

Status: Governing architecture for NAVI station stopping/braking (2026-10-05).

Related: [`NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md`](NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md) (NAVI position/overlay model, §4–§5).

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

During braking, NAVI should behave similarly to an experienced human operator bringing the locomotive to a stop:

* begin braking at a known location;
* know approximately how the locomotive normally coasts/decelerates;
* observe physical progress;
* judge whether the developing stop is tending long, correct, or short;
* adjust braking accordingly;
* continue observing and refining the braking trajectory;
* accept the physical stop when it occurs.

The objective is not to force the locomotive to match a succession of instantaneous speed targets.

The objective is to manage a stopping trajectory.

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

This architecture supersedes that control concept.

## Braking ramp as the primary mechanism

NAVI uses a smooth PWM-down ramp as the primary braking mechanism.

The existing manual-control deceleration ramp is the starting reference because its physical behavior is already known.

When operated manually, that ramp produces a smooth coast that tends to carry the locomotive too far unless the human operator applies additional braking.

That is desirable as a starting point.

The nominal NAVI braking ramp should therefore be biased somewhat long.

This gives NAVI room to increase braking as physical observations show how the stop is developing.

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

Relevant evidence includes:

* distance traveled since the braking reference;
* current measured IR speed;
* recent speed history;
* recent ramp history;
* current PWM;
* remaining distance to the desired stopping point.

NAVI uses this evidence to judge the developing stopping trajectory.

The central question is:

> **Given where I am, how fast I am moving, and how I have been decelerating, am I presently tending to stop long, approximately on target, or short?**

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

The nominal ramp remains the primary braking trajectory.

IR provides bounded correction.

IR-based judgment may alter the nominal ramp rate by no more than:

```
±20% of the nominal/planned ramp rate.
```

This means NAVI may make the braking ramp up to 20% steeper or 20% shallower than nominal.

It does not mean that IR may command PWM ±20%.

It does not authorize an increase in PWM during final braking.

The adjustment applies to ramp slope.

## Station STOP operating profile

The current intended station STOP profile has three regions.

### Region 1 — approach braking

At:

```
Station −10
```

NAVI begins a smooth deceleration from cruise speed.

The desired result at:

```
Station −5
```

is:

```
25 pKPH
```

This is one continuous braking ramp from Station −10 to Station −5.

There are no intermediate MM speed steps.

IR observations during this region allow NAVI to adjust the ramp slope so that the locomotive approaches approximately 25 pKPH at Station −5.

The Hall/MM observations establish the geographic boundaries.

IR provides the finer observations between them.

### Region 2 — station cruise

From:

```
Station −5 through Station 0
```

the locomotive operates at:

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

starts the final stopping ramp.

The current initial target for field development is:

```
35 completed IR pulses after Station 0.
```

At the current IR pitch:

```
35 × 9.652 mm ≈ 337.8 mm
```

Thus the initial desired stopping point is approximately 338 mm beyond the Station 0 Hall reference.

This value is a field-development target and may later be tuned from operational evidence.

## Final braking behavior

At Station 0:

1. NAVI establishes the IR distance reference.
2. NAVI begins the nominal smooth PWM-down braking ramp.
3. The nominal ramp is intentionally biased somewhat long.
4. Each useful IR pulse provides new distance and movement evidence.
5. NAVI evaluates whether the developing stop is LONG, ON TARGET, or SHORT.
6. NAVI adjusts the ramp slope within ±20% of nominal.
7. PWM itself may only hold or decrease.
8. NAVI continues this process until physical movement stops.

The desired stopping distance guides the trajectory.

It is not a mandatory odometer checkpoint.

## Physical stop is terminal

A locomotive may physically stop before the nominal target distance because PWM has fallen below the level required to sustain movement.

That is normal physical behavior.

Therefore:

> **If the locomotive physically stops during the final braking maneuver, the stop is complete.**

NAVI must then:

1. accept the physical stop;
2. command PWM 0;
3. begin the applicable dwell.

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
* repeated physical landmarks approximately every 9.652 mm.

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

## Architectural summary

The station braking architecture is:

```
Hall tells NAVI when to begin.
The ramp provides smooth deceleration.
IR tells NAVI how the stop is developing.
NAVI adjusts the ramp slope to converge on the desired stopping point.
PWM never increases during final braking.
A physical stop ends the maneuver.
```

The design objective remains:

> **A smooth, visually natural deceleration that stops the locomotive close to the intended physical location.**
