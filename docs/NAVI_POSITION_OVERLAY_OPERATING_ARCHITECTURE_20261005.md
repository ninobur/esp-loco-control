# NAVI Position/Overlay Operating Architecture

## Purpose

NAVI knows where the locomotive is.

The dispatcher tells NAVI what kind of operation is currently required.

The purpose of this architecture is to make current operating behavior derive from those present facts rather than from a remembered procedural history.

The governing relationship is:

current MM interval + direction + current operating overlay → current operating requirement

The dispatcher owns operating intent.

NAVI owns position and locomotive execution.

NAVI is the engineer.

---

## 1. The Scrabble-tile model

The railroad can be visualized as a row of Scrabble tiles.

Each MM interval is one tile.

For each direction, the current operating overlay assigns an operating requirement to each tile.

Examples of requirements may include:

* normal cruise;
* reduced speed;
* approach;
* stop;
* dwell;
* pass;
* a future dynamically imposed traffic restriction.

The locomotive appears to execute a sequence because it physically encounters successive tiles.

The important distinction is:

The sequence is in the geography, not in remembered procedural authority.

NAVI does not need to remember:

“I entered this procedure earlier, therefore I am now allowed or required to perform its next phase.”

It asks:

What instruction applies where I am now?

---

## 2. A new overlay is a new set of tiles

Changing the operating overlay means replacing the set of tiles.

The old overlay immediately loses operating authority.

NAVI does not need to remember what the previous tiles said.

For example, if an operating assignment changes from STOP to PASS while the locomotive is stopped or paused, the old STOP instruction does not retain authority merely because NAVI previously began executing it.

The current overlay governs.

This principle is fundamental for future operating profiles such as Circuit Express and dynamic traffic control.

---

## 3. Same present state should produce the same current requirement

Given the same:

* current MM interval;
* direction;
* current operating overlay;

NAVI should derive the same current geographic operating requirement regardless of how the locomotive arrived there.

Historical differences such as:

* whether an earlier marker was crossed;
* whether an approach was previously entered;
* whether the locomotive previously stopped there;
* whether a previous station operation completed;
* whether AUTO was paused and restarted;

must not change the geographic instruction that applies now.

This is the central test for removal of procedural remnants.

---

## 4. No operating phase retains authority across STOP/GO

No historical operating phase should survive STOP/GO as authority.

STOP stops the locomotive’s automatic operation.

GO means:

reevaluate current position + current direction + current overlay and execute the instruction applicable now.

GO does not mean:

resume the old procedural phase.

If the current instruction requires a physical maneuver, NAVI may establish the execution state needed to perform that maneuver from the present condition.

That execution state exists because of the current instruction, not because an old phase survived.

---

## 5. ARMED is not an operating concept

NAVI does not need to “arm” a geographic operating requirement.

If the locomotive is on a tile whose current instruction says to begin an approach, the approach instruction applies.

Nothing must first grant permission for it to exist.

Therefore an architecture such as:

Idle
→ detect station
→ ARMED
→ Approach

is rejected as operating authority.

ARMED arose from an older procedural model in which entering a region initiated a remembered operation.

That concept is unnecessary when NAVI already knows its current position.

Do not replace ARMED with another latch serving the same purpose.

---

## 6. Hall observations provide evidence, not operating permission

Hall observations are important NAVI evidence.

They can:

* establish or confirm MM position;
* provide polarity evidence;
* provide precise geographic landmarks;
* establish an IR coordinate for a fine-distance physical maneuver.

But Hall does not grant permission for an operating instruction to exist.

The current position/overlay combination determines the instruction.

There is an important distinction between:

“I need this Hall observation as a physical reference to execute this maneuver accurately.”

and:

“This maneuver has no authority unless I previously crossed this Hall marker.”

The first may be legitimate execution evidence.

The second recreates procedural admission.

---

## 7. IR provides execution evidence, not operating authority

IR provides fine-grained physical information such as:

* distance;
* movement;
* speed;
* physical response during a maneuver.

IR helps NAVI execute the current instruction.

It does not determine whether the instruction applies.

The current tile and overlay determine that.

Sensors provide observations.

NAVI makes the judgment.

---

## 8. Remove persistent phase authority

The inherited station architecture contains a persistent phase machine:

* Idle
* Approach
* Zone
* Ramp
* Dwell
* Depart

These phases currently influence later commands because the machine remembers where it believes it is in a procedure.

That persistent authority is rejected.

A label such as:

* APPROACHING
* BRAKING
* DWELLING

may remain useful as instantaneous telemetry describing what NAVI is presently doing.

But a diagnostic label must not become a remembered permission structure.

Current operating requirements must be derived from current state.

---

## 9. Remove remembered station identity as authority

The inherited architecture retains a selected station through state such as idx_.

That is unnecessary for geographic operating authority.

Given current:

* MM;
* direction;
* overlay;

NAVI can determine which local operating requirements apply.

A currently executing physical maneuver may retain the information it needs to complete that maneuver.

That is execution context, not historical station identity.

---

## 10. Remove historical entry PWM as station authority

The inherited architecture retains entryPwm_ from the beginning of an approach.

That belongs to the older PWM/procedural approach model.

It should not determine what instruction applies later.

A current physical controller may legitimately know its own starting PWM or speed because that information is needed to execute the maneuver.

Again:

execution state is not procedural authority.

---

## 11. Remove lastOff_ procedural progress

The inherited architecture uses previous station-relative offset to track progression through an approach and generate commands/events.

Current geographic position already identifies the applicable MM interval.

Do not preserve historical marker progression merely to establish where NAVI is in a procedure.

Telemetry generation must not become control authority.

---

## 12. Remove completed-visit suppression

The inherited completedIdx_ concept suppresses another operation while the locomotive remains within a geographic region after completing a previous visit.

That violates the current-state model.

At the same:

* MM;
* direction;
* overlay;

the current geographic instruction should not change merely because NAVI remembers having previously completed an operation there.

Therefore:

There is no governing “one stop per visit” latch.

This is especially important for future traffic operation.

A current dynamically required stop must never be suppressed because NAVI remembers that it previously completed a geographically unrelated service stop.

---

## 13. Remove station-specific pause/resume history

State such as:

* paused_;
* pausedAtMs_;

exists to preserve the old phase machine through STOP/GO.

That is contrary to the governing STOP/GO rule.

GO reevaluates the present operating situation.

It does not resume historical station procedure state.

---

## 14. MISSED is not a relevant operating concept

The inherited MISSED concept means, approximately:

a previously initiated station procedure has now passed beyond the location where the remembered procedure expected something to happen.

That depends upon the rejected historical procedure model.

Under the current architecture, NAVI asks:

What instruction applies at my current location now?

If a current instruction exists, execute it.

If no instruction exists, do not manufacture a historical failure merely because an earlier procedural expectation was not satisfied.

Therefore station-procedure MISSED is removed as an operating concept.

---

## 15. PHASE_TIMEOUT is not a relevant station concept

The inherited PHASE_TIMEOUT measures how long NAVI has remained in a remembered station phase.

That is not a useful diagnosis once the persistent phase architecture is removed.

A genuine failure to move or locomotive stall is a broader physical/evidence problem.

It should eventually be handled by a general movement-detection architecture if required.

It is not a station-specific phase problem.

Therefore:

Do not preserve or recreate a station-specific phase watchdog.

---

## 16. Procedural overshoot/late-entry logic is rejected

An older station procedure may reason:

“I should have entered the procedure earlier; I am now beyond its expected window; therefore the station was missed or abandoned.”

That reasoning depends upon historical sequence.

The current architecture instead asks what the present geographic instruction requires.

Geographic boundaries remain legitimate.

Historical procedural expectations do not.

Do not confuse:

this tile has no STOP instruction

with:

I failed to enter the STOP procedure correctly earlier.

Those are fundamentally different concepts.

---

## 17. PWM is not the language of the operating overlay

The operating overlay describes railroad requirements.

It should express concepts such as:

* cruise;
* reduced speed;
* approach;
* stop;
* dwell;
* pass;
* future traffic restrictions.

PWM is an actuator output chosen by NAVI to execute those requirements.

Historical PWM tables, marker-triggered PWM sequences, and fixed PWM procedural profiles do not define the operating architecture.

NAVI may use PWM internally as necessary to control the locomotive.

But:

PWM does not define what the railroad is asking NAVI to do.

---

## 18. Legitimate execution state remains possible

The rejection of historical procedural authority does not require NAVI to be mathematically stateless.

Some physical operations require short-lived state.

The governing distinction is:

Execution state exists to carry out an instruction that applies now. It does not make an old instruction continue to apply.

Examples include:

### A physical braking maneuver

A smooth braking operation may require:

* a geographic/IR reference;
* current ramp state;
* movement observations;
* current physical-response estimates.

That state exists because NAVI is actively executing the current braking instruction.

### Dwell

Dwell is genuinely time based.

Once the current operating requirement establishes a dwell, NAVI needs to know how much of that currently applicable dwell has elapsed.

That timer is execution state.

It is not evidence that an old station phase retains authority forever.

---

## 19. Overlay change governs execution-state validity

When the overlay changes, NAVI must reevaluate whether an existing execution state still corresponds to the current instruction.

An execution state does not survive merely because it exists.

For example:

* if a STOP requirement is replaced by PASS, historical STOP authority disappears;
* if a current braking maneuver is no longer applicable under the new overlay, it cannot remain authoritative merely because it had already begun.

The precise actuator transition required when an overlay changes may depend on the physical operation involved.

But the authority question is already settled:

The current overlay governs.

---

## 20. Dispatcher and NAVI responsibilities

The dispatcher owns the operating assignment.

NAVI does not independently decide:

* which stations are active;
* whether it is Express or Local;
* whether a station should be skipped;
* what service pattern the railroad is running.

The dispatcher supplies operating intent.

NAVI knows:

* where it is;
* which direction it is traveling;
* what physical evidence is available;
* how the locomotive is responding.

NAVI determines how to execute the current instruction intelligently.

The relationship is:

Operator
→ Dispatcher
→ operating overlay
→ NAVI execution

---

## 21. The same model supports future dynamic traffic requirements

Station service is only one source of operating requirements.

Future Close Train Operations will create requirements based on another train’s physical position.

For example, a following locomotive may need to:

* reduce speed;
* follow;
* stop behind another consist.

Those requirements are dynamic rather than geographically fixed.

They should nevertheless fit the same general model:

current position + current operating requirements → current behavior

This is one reason historical station-visit latches must not control NAVI.

A current traffic restriction must not be suppressed because of unrelated historical service state.

---

## 22. Tests must follow the architecture

Tests that encode obsolete procedural behavior are not governing authority.

Review and replace tests that require:

* ARMED;
* phase progression as permission;
* completed-visit suppression;
* pause/resume of historical station phases;
* MISSED;
* PHASE_TIMEOUT;
* one-stop-per-visit behavior;
* prerequisite marker history for current geographic authority.

Tests should instead establish properties such as:

### Same-position history independence

Different histories that arrive at the same:

* MM;
* direction;
* overlay;

produce the same current geographic requirement.

### STOP/GO reevaluation

After GO, the current requirement derives from present:

* position;
* direction;
* overlay.

### Overlay replacement

Replacing the overlay replaces the old operating authority.

### Cold/current-position entry

NAVI can determine the current operating requirement without having traversed a prerequisite procedural sequence.

### Evidence versus authority

Hall and IR may provide required execution evidence without becoming admission latches.

---

## 23. Telemetry should describe current reasoning

Telemetry should expose useful current information such as:

* current MM;
* direction;
* current overlay;
* current derived operating requirement;
* current execution controller;
* relevant physical evidence;
* current actuator command;
* current execution state where necessary.

Telemetry should not preserve obsolete procedural concepts merely because older dashboards or logs used them.

Review concepts such as:

* ARMED;
* historical station phase;
* MISSED;
* PHASE_TIMEOUT;
* completed-visit state;
* armed_after;

and remove or redefine them where they imply obsolete authority.

Telemetry should explain:

What does NAVI believe is required now, and why?

---

## 24. Acceptance test

The architecture can be tested with two questions.

### Question 1

Can two locomotives at the same:

* MM;
* direction;
* current overlay;

receive different geographic operating requirements solely because their historical station procedures differ?

The answer must be:

NO.

### Question 2

Must NAVI remember that an operation was previously:

* armed;
* entered;
* visited;
* missed;
* completed;

before it can determine what operating requirement applies at its current location?

The answer must be:

NO.

Short-lived physical execution state may affect how NAVI carries out the current instruction.

It does not determine which geographic instruction has authority.

---

## 25. Governing summary

The architecture is:

The dispatcher supplies the operating tiles.

NAVI knows which tile it occupies.

The current tile tells NAVI what is required now.

Sensors provide evidence.

NAVI decides how to execute the requirement.

Physical execution may require short-lived state.

Historical procedural state does not retain operating authority.

Or, in compact form:

current MM interval + direction + current overlay → current operating requirement

That is the governing NAVI position/overlay operating model.
