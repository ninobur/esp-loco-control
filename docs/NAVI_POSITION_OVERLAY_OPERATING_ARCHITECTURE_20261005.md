# NAVI Position/Overlay and Speed-Control Operating Architecture

## Purpose

NAVI knows where the locomotive is.

The dispatcher tells NAVI what kind of operation is currently required.

The purpose of this architecture is to make current operating behavior derive from present railroad requirements and present physical conditions rather than from remembered procedural history.

The governing authority relationship is:

current MM interval + direction + current operating overlay → current operating requirement

The dispatcher owns operating intent.

NAVI owns position and locomotive execution.

NAVI is the engineer.

The operating overlay speaks in railroad terms.

It specifies physical objectives such as:

* target speed;
* gradual acceleration or deceleration;
* geographic stopping objective;
* dwell;
* pass;
* future traffic restrictions.

It does not prescribe PWM.

NAVI uses PWM as an actuator to achieve those physical objectives.

---

## 1. The Scrabble-tile model

The railroad can be visualized as a row of Scrabble tiles.

Each MM interval is one tile.

For each direction, the current operating overlay assigns an operating requirement to each tile.

Examples include:

* cruise at a defined physical speed;
* reduced speed;
* gradual acceleration;
* gradual deceleration;
* continuation of a geographically defined glide path;
* stopping;
* dwell/restart behavior;
* pass;
* future dynamically imposed traffic restrictions.

The locomotive appears to execute a sequence because it physically encounters successive tiles.

The important distinction is:

The sequence is in the geography, not in remembered procedural authority.

NAVI does not need to remember:

“I entered this procedure earlier, therefore I am now allowed or required to perform its next phase.”

It asks:

What instruction applies where I am now?

---

## 2. Tiles specify physical objectives, not actuator values

A tile or operating region describes what the railroad physically requires.

It does not specify a PWM value.

For a speed requirement, the governing form is:

Target physical speed = X. Transition gradually toward X from the locomotive’s actual physical state.

A speed target is an objective.

It is not an instruction to achieve that speed instantaneously at a geographic boundary.

PWM is beneath the overlay layer.

NAVI determines how PWM must change to satisfy the physical instruction.

Therefore:

The overlay says what physical condition the railroad requires. NAVI, as engineer, determines how to achieve it.

---

## 3. Current physical state is the starting condition

When an operating instruction becomes authoritative, NAVI begins from the locomotive’s actual physical state at that moment.

It does not assume that a preceding tile produced an expected:

* speed;
* PWM;
* ramp rate;
* physical response.

The previous geography may explain how the locomotive arrived in its present physical condition.

It does not supply current operating authority.

Therefore:

The current instruction operates on the locomotive that actually exists now, not the locomotive a previous instruction expected to deliver.

---

## 4. A new overlay is a new set of tiles

Changing the operating overlay means replacing the set of tiles.

The old overlay immediately loses operating authority.

NAVI does not need to remember what the previous tiles said.

If an operating assignment changes from STOP to PASS while the locomotive is stopped or moving, the old STOP instruction does not retain authority merely because NAVI previously began executing it.

The current overlay governs.

This principle is fundamental for future operating profiles such as Circuit Express and dynamic traffic control.

---

## 5. Same present state produces the same current requirement

Given the same:

* current MM interval;
* direction;
* current operating overlay;

NAVI derives the same current geographic operating requirement regardless of how the locomotive arrived there.

Historical differences such as:

* whether an earlier marker was crossed;
* whether an approach was previously entered;
* whether the locomotive previously stopped there;
* whether a previous station operation completed;
* whether AUTO was paused and restarted;

must not change the geographic instruction that applies now.

This is the central test for removal of procedural remnants.

---

## 6. Speed is NAVI’s controlled physical variable

The current baseline physical speed targets are:

Cruise: 45 pKPH

Station speed: 20 pKPH

Stop: 0 pKPH

These are physical speed objectives.

They are not aliases for predetermined PWM values.

NAVI uses IR-measured movement/speed as physical feedback and adjusts PWM to achieve and maintain the applicable target.

Thus:

NAVI controls speed by adjusting PWM.

PWM remains important.

Its architectural role has changed.

PWM is not the railroad instruction.

PWM is NAVI’s actuator.

---

## 7. Speed targets are approached gradually

If actual speed is below the current target:

accelerate gradually toward the target.

If actual speed is above the current target:

decelerate gradually toward the target.

If actual speed is approximately at target:

maintain the target using steady-speed homeostasis.

Crossing an MM boundary does not justify an instantaneous change in speed or PWM.

The analogy is a locomotive engineer encountering a speed restriction.

A reduction from 45 to 20 pKPH means:

begin an appropriate gradual deceleration toward 20.

It does not mean:

instantly become 20 pKPH at the sign.

Likewise, a stationary locomotive on a 45-pKPH cruise section gradually accelerates toward 45.

---

## 8. Basic AUTO actuator ramp

The initial baseline for ordinary AUTO acceleration and deceleration should use the same established basic ramp behavior as current Manual operation.

Do not retain a separate faster AUTO actuator response merely because older AUTO code used one.

This establishes a known smooth physical starting behavior.

Special adaptive physical maneuvers may modify the ramp as described below.

The architectural distinction is:

The target is physical speed. The ramp is one mechanism NAVI uses to move PWM smoothly while pursuing that target.

---

## 9. Geographic glide paths

Some speed changes have a defined geographic objective.

For those operations, NAVI uses a distance-defined glide path.

At glide-path initiation NAVI knows:

* actual measured physical speed;
* physical starting location;
* physical endpoint;
* terminal target speed.

The glide path expresses:

From the speed I actually have here, converge smoothly on the required terminal speed at the defined physical endpoint.

There is no requirement for predefined intermediate speed steps at MM markers.

---

## 10. IR supplies fine-resolution glide-path evidence

IR provides frequent physical observations between Hall/MM landmarks.

At each useful IR observation NAVI knows:

* physical progress;
* distance remaining;
* measured speed;
* terminal speed objective.

NAVI can therefore calculate the speed appropriate to the developing trajectory.

Intermediate desired speeds are not fixed values stored in the overlay.

They are calculated from:

actual starting condition + physical progress/remaining distance + terminal-speed objective

The approximately 9.652-mm IR pulse spacing provides much finer feedback than the MM map.

The MM map defines geographic authority.

IR provides the fine physical evidence NAVI uses to execute that authority.

---

## 11. Adjust the trajectory, not instantaneous PWM

Do not implement:

instantaneous speed error → immediate proportional PWM correction

That control model previously produced undesirable hunting, large PWM excursions, overshoot, and stopping.

Instead:

IR evidence tells NAVI whether the developing physical trajectory is fast, appropriate, or slow relative to the glide path.

NAVI adjusts the rate of the developing PWM ramp/trajectory.

For deceleration:

Tending fast

The locomotive is tending to reach the endpoint too fast.

Gradually steepen the deceleration ramp.

On trajectory

The physical response is appropriate.

Maintain the current ramp.

Tending slow

The locomotive is losing speed too rapidly.

Gradually flatten the deceleration ramp.

During deceleration, a slow judgment normally means less braking, not an immediate positive-throttle correction.

The objective is stable convergence.

---

## 12. Glide-path anti-hunting

The glide-path controller must not react violently to individual IR observations.

The governing principle is:

Correct the developing trajectory, not each instantaneous speed observation.

Glide-path control should therefore include:

* tolerance/deadband around the calculated trajectory;
* bounded ramp adjustment;
* incremental correction;
* persistence/hysteresis before reversing correction direction;
* convergence behavior that holds a successful trajectory rather than continually trimming it;
* no direct proportional PWM chasing.

These rules apply to every glide-path operation.

They are not station-specific exceptions.

---

## 13. Endpoint targets are not cliffs

A geographic endpoint defines the desired physical condition.

It is not an instantaneous actuator discontinuity.

If NAVI reaches the nominal endpoint somewhat above the terminal target speed:

continue gradual convergence toward the target.

Do not abruptly impose a large correction merely because the geographic boundary was crossed.

If the target speed is achieved somewhat early:

settle into maintaining the target.

---

## 14. Station approach is one continuous glide path

The old five-marker station approach must not be represented as five independent PWM or speed steps.

The station approach is one continuous deceleration objective over approximately five MM intervals.

For the current STOP overlay:

actual physical speed at approach initiation → 20 pKPH at the defined station-speed endpoint

There are no prescribed intermediate marker speeds such as:

40 → 35 → 30 → 25 → 20.

The MM intervals establish the geography.

NAVI manages one continuous physical glide path through them.

---

## 15. Cold start inside an ordinary deceleration region

If AUTO starts or resumes with the locomotive stationary inside an ordinary deceleration region, NAVI does not reconstruct a glide path that began before the locomotive started.

Instead:

gradually accelerate directly toward that region’s terminal target speed.

For the current station approach, a stationary locomotive starting at Station −9 therefore accelerates gradually toward:

20 pKPH

It does not accelerate to 45-pKPH cruise first.

This is an explicit cold-entry rule.

---

## 16. Station-speed region

The station-speed region has:

target speed = 20 pKPH

This is genuine closed-loop speed control.

It is not:

PWM 60 means approximately 20 pKPH.

NAVI uses IR speed evidence and adjusts PWM gradually as necessary to maintain approximately 20 pKPH.

The same steady-speed mechanism used here should also govern 45-pKPH cruise.

Only the target speed differs.

---

## 17. Steady-speed homeostasis

Steady-speed control deliberately separates frequent observation from slow actuator intervention.

For the initial field-test implementation, use the following conservative mechanism.

Evaluation window

Collect:

5 fresh IR beats

Use those observations to derive the representative speed judgment for that window.

Record all observations.

Within target tolerance

If the five-beat evidence indicates speed within the accepted target tolerance:

PWM unchanged.

Immediately begin another fresh five-beat evaluation window.

Meaningfully below target

If the five-beat evidence indicates speed meaningfully below target:

PWM +1

exactly once.

Meaningfully above target

If the five-beat evidence indicates speed meaningfully above target:

PWM −1

exactly once.

This is not proportional control.

The magnitude of one homeostasis correction is:

1 PWM count

---

## 18. Homeostasis correction moratorium

After any ±1 PWM homeostasis correction:

no further homeostasis correction is authorized for the next 5 IR beats.

Continue recording all IR observations during those five beats.

The data are not discarded.

They are simply:

recorded but not acted upon.

After the five-beat moratorium:

begin a new fresh five-beat evaluation window.

Thus the nominal cycle after a correction is:

5 beats evaluate → ±1 PWM → 5 beats observe without acting → 5 fresh beats evaluate again

If no correction is required, there is no moratorium:

5 beats evaluate → no change → next 5 beats evaluate

The purpose is to give the locomotive time and distance to demonstrate the physical effect of a one-count PWM change before NAVI authorizes another correction.

---

## 19. Homeostasis parameters are field-test parameters

The following initial values are experimental:

* five-beat evaluation window;
* ±1 PWM correction;
* five-beat correction moratorium;
* target-speed tolerance/deadband.

They are not immutable architectural constants.

Telemetry must preserve enough evidence to evaluate them after field testing.

The architecture is:

observe frequently, intervene slowly, record everything.

Field evidence may justify changing these values later without changing the fundamental control architecture.

---

## 20. Cruise speed control

Normal AUTO cruise uses:

target speed = 45 pKPH

Cruise is therefore governed by the same steady-speed homeostasis mechanism as the 20-pKPH station-speed region.

NAVI does not simply command a fixed cruise PWM and assume the resulting physical speed.

At 45 pKPH:

* five-beat evidence within tolerance → hold PWM;
* meaningfully slow → PWM +1;
* meaningfully fast → PWM −1;
* after correction → five-beat observation moratorium.

Thus 20-pKPH station operation and 45-pKPH cruise are two applications of the same speed-control mechanism.

---

## 21. Final stopping deceleration uses the same glide-path mechanism

The final approximately tile-and-a-half deceleration is not a separate braking control law.

It is another application of the same distance-defined glide-path mechanism used for the earlier station approach.

Inputs are:

* actual physical speed when the final glide path begins;
* physical starting location;
* available stopping distance;
* terminal physical endpoint;
* terminal target speed.

For final stopping:

terminal target speed = 0 pKPH

Use the same:

physical progress → calculated trajectory → IR comparison → ramp adjustment

mechanism.

The difference is the terminal speed objective, not the control method.

---

## 22. Terminus instruction

The terminus portion of the stopping overlay contains the complete local instruction:

from current physical state → complete the final glide path to zero → accept physical stop → dwell → restart

Physical stop is terminal for the braking portion.

If the locomotive physically stops somewhat before the mathematical endpoint:

accept the physical stop.

Do not restart merely to seek:

* an exact IR-distance target;
* another Hall marker;
* the mathematical endpoint.

Once stopped:

begin dwell.

After dwell:

restart.

Restart is part of the terminus instruction.

It does not require the dispatcher to replace the STOP overlay merely to release an ordinary station dwell.

---

## 23. Cold start/restart inside the terminus region

A stationary locomotive starting or resuming AUTO while already within the terminus region is a special case.

It must not interpret the zero-speed terminal objective as an instruction to remain stopped or perform another station stop.

Instead:

gradually accelerate toward the applicable 45-pKPH cruise target.

This is the explicit cold/restart rule for the terminus region.

It is intentionally different from cold entry into an ordinary deceleration region.

---

## 24. Stopping overlay is reusable

Station service is the first implementation of a more general automatic stopping capability.

A stopping overlay may eventually be applied wherever automatic operation requires a controlled stop, including:

* station service;
* spacing behind another consist;
* holding points;
* dispatcher-directed stops.

The physical stopping mechanism should not be inherently station-specific.

Different uses may define different:

* geographic placement;
* terminal speed;
* dwell;
* release conditions.

The current station implementation establishes the first working form of this general capability.

---

## 25. No operating phase retains authority across STOP/GO

No historical operating phase survives STOP/GO as authority.

STOP stops automatic operation.

GO means:

reevaluate current position + current direction + current overlay and execute the instruction applicable now.

GO does not mean:

resume the old procedural phase.

If the current instruction requires physical execution state, NAVI establishes the state required to execute the current instruction from the locomotive’s present physical condition.

---

## 26. ARMED is not an operating concept

NAVI does not need to arm a geographic operating requirement.

If the current tile/region requires deceleration, speed maintenance, stopping, or another action, that requirement applies.

Nothing must first grant permission for it to exist.

Therefore:

Idle → ARMED → Approach

is rejected as operating authority.

Do not replace ARMED with another latch serving the same purpose.

---

## 27. Hall observations provide evidence, not operating permission

Hall observations can:

* establish or confirm MM position;
* provide polarity evidence;
* provide precise geographic landmarks;
* establish an IR coordinate for a fine-distance physical maneuver.

But Hall does not grant permission for an operating instruction to exist.

The current position/overlay combination determines the instruction.

There is a distinction between:

I need this Hall observation as a physical reference to execute this maneuver accurately

and:

This maneuver has no authority unless I previously crossed this Hall marker.

The first may be legitimate execution evidence.

The second recreates procedural admission.

---
