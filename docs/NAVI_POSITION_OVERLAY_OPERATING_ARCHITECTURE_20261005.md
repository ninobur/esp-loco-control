# NAVI Position/Overlay and Speed-Control Operating Architecture

## Purpose

> **Revision proposed 2026-10-08 — architectural reconciliation candidate.** This document retains the October 5 physical-speed and geographic-control principles while integrating the four-station base program, service overlays, consist envelopes, and dynamic two-train following. The revisions below are design decisions, **not authorization to implement or flash firmware**. Reconcile with decision 0121, the CTO reconstruction architecture, and `NGR_GEOGRAPHIC_CONSIST_AND_MOVEMENT_ARCHITECTURE_20261008.md` before implementation. Experimental parameters and unresolved evidence requirements remain explicitly identified.

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

## 22. Station terminus tile: stop, dwell, restart

The ordinary station STOP tile contains the **complete local station-service instruction**:

from current physical state → complete the final geographic glide path to zero → confirm a station stop at the designated station geography → dwell **5 seconds** in the base program → restart using the applicable current geographic instruction and normal ramp.

The base program is the four-station local: **PATIO, BAMBOO, ARCHES, GRILLERS**, all STOP with five-second dwell. An overlay may alter the station instruction or dwell, but does not replace the base geography.

If the locomotive stops somewhat before the mathematical endpoint, NAVI accepts the physical stop; it does not restart solely to seek a precise IR coordinate or Hall marker. However, a **temporary traffic stop behind another consist is not station-service completion** and must not initiate station dwell. The geographic criterion for recognizing a station stop (including reasonable early stopping) requires explicit field-tested definition; do not invent an entry latch to implement it.

A completed station dwell permits normal station departure only when manual authority and all current traffic restrictions also permit movement. Restart is part of the station tile; it does not require a new dispatcher release or overlay replacement. Execution memory needed to time a dwell or prevent immediate repeated stopping at the same physical visit is permitted, but it must **never** grant operating authority independently of current geography and overlay.

---

## 23. Cold start, STOP/GO, and temporary traffic interruption

A stationary locomotive starting or resuming AUTO within an ordinary deceleration region gradually approaches that region's applicable terminal speed from its **actual** state, not a presumed earlier phase.

A stationary locomotive in the terminus region must distinguish:
- an ordinary station visit whose station-service stop/dwell has been completed;
- a station STOP instruction that still applies to an uncompleted visit;
- a temporary traffic stop imposed over the underlying station geography; and
- an explicit manual STOP/GO intervention.

When a **temporary traffic restriction** relaxes, NAVI immediately reevaluates the **current tile**, current service overlay, and remaining traffic restrictions. There is no traffic-stop dwell, release latch, or leader-speed threshold. Resumption follows the applicable tile objective with normal ramps. The traffic stop does not count as station arrival.

Do not treat a cold restart as authority to skip an uncompleted station stop, and do not let an old procedural station phase retain authority across STOP/GO. The precise minimal execution-state representation for completed dwell and current physical visit must be reconciled before implementation; it must not recreate ARMED, entry-gated, or Hall-permission machinery.

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

---

## 28. Four-station base program and overlay hierarchy (2026-10-08)

The **underlying program** is Four-Station Local. In either direction, NAVI follows geographic tiles for:
- 45-pKPH cruise;
- a **five-MM continuous glide path** from cruise toward 20 pKPH;
- **five MM** of station-zone 20-pKPH travel;
- the final approximately **1.5-MM** geographic glide path to a station stop;
- a **five-second station dwell**, then a normal ramped restart.

These are geographic objectives, not five independent PWM steps. The five-MM station-speed zone applies whether the station is STOP or PASS. The station STOP tile contains its own dwell and restart instructions.

Service overlays change **which station instructions apply**, without changing the underlying geography. Initial overlays include:
- Four-Station Local: STOP at Patio, Bamboo, Arches, Grillers.
- Individual station PASS overrides, with other stations retaining base STOP.
- Circuit Express: PASS at all four stations.
- Future dwell variations, including random dwell, as separately approved modifications of the base five seconds.

The effective service overlay is a complete, atomically replaced instruction set, not an indefinitely accumulated history of commands. It takes effect **immediately** and NAVI evaluates the current tile and actual physical state. The dispatcher owns service intent; NAVI owns movement execution.

For a station PASS, the five 20-pKPH tiles remain. At the tile otherwise designated for final station deceleration, NAVI instead begins a **ramped return toward 45-pKPH cruise**, continuing through the former stop tile. PASS does not override a dynamic traffic restriction.

## 29. Hall-anchored consist envelope

NAVI's Hall sensor is a geographic anchor, not the whole train. Configured consist geometry, current map position, and direction/orientation yield the **leading and trailing physical boundaries**. Reversal changes which end leads without losing geographic identity.

The previously discussed nominal consist offsets are **450 mm ahead of Hall and 1,200 mm behind Hall** (1,650 mm overall); these are reference dimensions requiring verification for the actual locomotive and cars, not a universal train length.

Separation is measured as **clear track between the follower's leading boundary and the leader's trailing boundary**, not between Hall sensors or block boundaries. Location, direction, geometry, and report age/provenance must be available before applying following instructions.

## 30. Dynamic geographic following overlay

The leading train communicates its **position and speed**, with sufficient consist geometry or boundary data to locate its trailing end. NAVI on the following train derives separation and constructs a **temporary dynamic geographic overlay** across the applicable tiles between the two consists.

The dynamic overlay may lower the permitted speed, impose a distance-defined glide path, or designate a temporary stopping point behind the leader. It is **not** a replacement for the base/service tile instructions; it is an additional restriction. NAVI executes the more restrictive currently applicable physical objective using the same ramp, glide-path, and speed-homeostasis mechanisms.

Following can result in slower movement **without stopping**. If a stop is necessary, its destination defines the approach tiles backward toward NAVI, anywhere on the railroad. There is no entry latch, special station procedure, or traffic release message.

When the leader departs and sufficient geographic separation develops, the dynamic restriction progressively relaxes. NAVI then follows whatever base/service instruction applies **at its present tile**. There is **no requirement that the leader first reach cruise speed**.

A traffic-related stop is analogous to a temporary PAUSE, but is released by **geographic separation**, not a console command. It carries **no station dwell**. The follower may stop behind an occupied platform, then proceed toward the platform as separation permits, and subsequently perform the ordinary station stop/dwell if its station tile still requires STOP.

## 31. Speed-dependent minimum separation

The full-cruise **minimum** clear separation is **12 MM, approximately 3,600 mm behind the rear of the leading consist**. It is a **minimum**, not a maximum and not a command to close a larger gap.

Its derivation is the existing planned stopping sequence:
- 5 MM to decelerate from 45 to 20 pKPH;
- 5 MM at 20 pKPH;
- 1.5 MM for final deceleration to zero;
- total 11.5 MM, rounded up to 12 MM.

At lower following speeds, the required minimum may be smaller; the same stopping model, not a separate braking theory, must establish the permitted distance. As speed increases, the required buffer expands; NAVI may accelerate only when the available separation permits the higher proposed speed. Speed-limit compliance and following-distance compliance are **independent simultaneous requirements**, analogous to highway driving.

The intermediate-speed separation schedule, locomotive-specific stopping performance, information latency, and behavior under stale or uncertain leader position **remain unresolved and require explicit approval and validation**. The planned 12-MM maneuver is not a proven emergency-braking guarantee. This document does not authorize a new performance limiter or two-train activation.

## 32. No special release or obsolete procedural authority

The current instruction is determined by **current geography + direction + effective service overlay + current dynamic traffic restrictions + actual physical state**. The underlying tile is never erased by a temporary restriction.

A traffic stop is the zero-speed result of a current separation requirement. When that requirement changes, NAVI reevaluates current geography. No separate leader-cruise-speed release condition, station-arrival release message, retained approach latch, or historical phase is needed.

The former alternating station release policy arose because block occupancy did not reveal position **within** a block. With trustworthy consist-aware location and minimum separation, two trains can occupy the same traditional block. The legacy alternation may remain an optional operating style, not a necessary physical-separation rule.

## 33. Verification and implementation boundaries

The following are architectural acceptance cases, not implemented claims:

1. Four-Station Local stops at all four stations with five-second dwell and smooth departure.
2. Individual PASS and Circuit Express preserve five station-speed tiles and ramp toward cruise over the former final-stop region.
3. An overlay change applies immediately without a stale phase retaining authority.
4. A following train slows or stops behind an occupied platform, then advances as geographic separation grows, and makes its own station stop; the temporary stop does **not** count as station dwell.
5. A moving leader can be followed at reduced speed without unnecessary stopping; minimum clear separation depends on follower speed.
6. Consist boundaries, reversal, leader-report freshness, STOP/GO, and manual command authority remain coherent.
7. No unapproved new governor, cap, PWM restriction, or navigation authority is introduced.

The geographic stopping model must remain reusable at **any** designated stopping point. Firmware implementation, dispatcher message format, intermediate-speed separation, and two-train field activation require separate authorization and validation.

---

## 34. October 8 settled tile and following decisions

This section records the six settled architectural decisions in [the October 8 decision record](NAVI_DOCUMENT_C_ARCHITECTURAL_DECISIONS_20261008.md). It **refines** earlier proposed wording in §§22–23 and 28–33 where they differ. Earlier descriptions of full-cruise **3,600 mm as the entire minimum following separation are superseded** by the explicit 600-mm clearance **plus** standard stopping distance.

### 34.1 The station tile executes stop → dwell → departure

At the designated station STOP tile, NAVI physically stops, dwells **five seconds** under the Four-Station Local base program, and then **changes the active instruction on that same tile** to ramp toward cruise speed. Departure originates on the stopping tile itself. This is execution progress of the tile's current physical instruction, **not** historical station-entry, ARMED, completed-visit suppression, or independent authority to disregard a current overlay.

### 34.2 A temporary stop suspends, but does not complete, the base tile

If a higher-priority traffic/geographic mandate stops NAVI on the designated base stopping tile, the base instruction is suspended. When the mandate is lifted, the base instruction resumes **from the actual stopped position**: begin the full five-second station dwell **at that moment**, then ramp toward cruise. Do not count time spent waiting under the temporary mandate as station dwell. If stopped on an earlier tile, resume that earlier tile's applicable instruction instead. No duplicate physical stop is required.

### 34.3 Overlays repaint only their footprint

An overlay changes only the geographic tiles it names. Its new footprint **completely replaces** its prior footprint; tiles no longer painted revert to the underlying base layer. A station **PASS modifies exactly two tiles**—the final-deceleration and station-stop tiles—into ramp-toward-cruise instructions. The preceding five 20-pKPH station-speed tiles are unchanged. A complete four-station profile is not required to express a single-station PASS.

### 34.4 Initial consist geometry is fixed

For initial implementation, use **450 mm ahead of Hall** and **1,200 mm behind Hall**, total **1,650 mm**, as the chosen fixed geometry. NAVI derives front and rear consist boundaries from geographic Hall position and direction/orientation. Per-consist configuration is deferred. Field verification of physical dimensions remains necessary before two-train activation.

### 34.5 Last-known leader position is retained

When peer position/speed reports cease, NAVI retains the **last known leader position** and **treats the leader as stopped there** for geographic following decisions. Do not extrapolate unobserved motion. A distant retained position may require no immediate intervention; a close position requires the standard stop/restriction early enough. Fresh valid reports replace the retained position and cause immediate reevaluation. Retained evidence must remain distinguishable from fresh evidence.

### 34.6 Minimum separation includes a 600-mm terminal clearance

**Maintain 600 mm plus the distance required for a standard stop at the follower's current speed.**

The **600 mm** is clear distance between the follower's front boundary and leader's rear boundary **after** stopping. NAVI designates a temporary stopping point 600 mm behind the leader's rear and projects the **existing standard stopping maneuver** backward toward the follower. This also applies to the retained last-known leader position after signal loss.

The planned full-cruise standard stopping envelope is 5 MM deceleration + 5 MM reduced speed + 1.5 MM final stop = 11.5 MM, rounded to 12 MM (nominally 3,600 mm). Adding 600 mm terminal clearance yields **nominally 4,200 mm total minimum clear separation at cruise**, subject to actual mapped distances and field validation. Earlier 3,600-mm-only separation statements describe the stopping component, not the complete minimum.

For intermediate speeds, use the **same standard stopping model**; do not invent an unrelated following/braking formula. The precise current-speed-to-standard-stopping-distance mapping and measured performance still require validation. No separate leader-speed release condition, traffic latch, or special restart procedure is needed: the temporary restriction relaxes as observed geography permits, and NAVI follows its current tile.

### 34.7 Implementation and authority boundary

Decision 0121's rejection of historical procedure authority remains binding; Decision 0120's PWM-zero movement/redeclaration requirement remains binding. Tile-local execution progress is legitimate only to carry out the currently applicable stop/dwell/departure instruction, and must not preserve obsolete operating authority after a change in geography, overlay, or manual STOP/GO. Detailed state lifetime/reset behavior must be verified against these requirements before coding.

These decisions specify architecture, **not approval to implement, merge, flash, or activate two-train following**.
