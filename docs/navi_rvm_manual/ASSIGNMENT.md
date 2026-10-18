NAVI_RVM_MANUAL_1_0 — Implementation Assignment for Codex

1. Objective

Build a manually controlled locomotive sketch that demonstrates one capability:

Recognize the railroad’s magnetic landscape by continuously matching filtered Hall observations against a fixed, fuzzy geographic reference.

The locomotive is controlled entirely by the operator. Navigation observes and reports but has no authority over motor operation.

This is a new experimental implementation, not a repair of the previous NAVI_RVM candidate.

2. Why we are simplifying

Our previous development repeatedly introduced mechanisms intended to detect, classify, prevent, or recover from navigation failures before establishing that the fundamental pattern-recognition concept worked.

Those mechanisms accumulated until they obscured the original objective.

The October 17 Behavioral Authority Audit confirmed that the previous candidate contained inherited behaviors and independently authoritative quality decisions inconsistent with the intended architecture.

We are therefore deliberately removing everything that does not contribute directly to the recognition experiment.

The hypothesis is that the continuous magnetic landscape contains sufficient information to identify geographic progression without elaborate failure-management machinery.

We want to test that hypothesis directly.

If it succeeds, we have established the foundation for future navigation.

If it fails, we need to understand why. Adding recovery mechanisms before establishing recognition would conceal the answer.

This is not an attempt to build a complete automatic navigator.

It is an attempt to establish whether the simplest version of our navigation concept actually works.

3. Base firmware

Use MANUAL_1_1 as the structural reference:

firmware/test-programs/MANUAL/MANUAL.ino

Preserve its manual-control architecture while verifying the necessary hardware and motor-control details against Otto’s current configuration.

Retain the established manual throttle, direction control, PWM ramping, operator STOP/E-STOP, battery protection, and MQTT communication.

Replace the QUORUM navigation mechanism completely.

Do not transplant the old QUORUM decision system or its recovery machinery.

Do not assume that MANUAL_1_1 is immediately flashable without checking its hardware configuration and dependencies.

4. Magnetic recognition

The recognizer receives:

* Continuous Hall measurements, filtered to reduce noise while preserving waveform structure.
* Applied PWM and elapsed time.
* The fixed fuzzy magnetic reference.
* The known geographic map.
* The operator-declared starting MM and direction.

A magnetic ensemble consists of associated positive and negative waveform structures belonging to one physical magnet, separated from neighboring ensembles by baseline regions.

Flat lobes, shoulders, and multiple peaks may belong to the same ensemble.

Recognition concerns the entire developing pattern, not independently qualifying individual lobes.

Polarity contributes directly to waveform matching.

PWM supplies an approximate movement prediction. It cannot independently establish geographic progression.

The observed Hall landscape must support the match.

5. Persistent puzzle-board alignment

The geographic map and fixed fuzzy waveform form the printed puzzle board.

Previously matched waveform portions establish the assembled history.

Together, they constrain the placement of subsequent observations.

An unusual magnetic ensemble must not redefine the board or erase previously established alignment.

A missing ensemble leaves a missing portion of the pattern. It does not automatically shift every subsequent ensemble backward by one MM.

The matcher may represent uncertainty internally, but it must not introduce an independent geographic recovery subsystem.

The implementation must demonstrate that previously established waveform-to-map registration actually constrains subsequent matching.

A variable containing the last trusted MM is not sufficient.

6. Navigation output

The matcher reports:

* Current recognized MM.
* Expected next MM.
* Match quality.
* Observed and expected polarity.
* PWM-estimated distance.
* Current alignment with the fixed reference.
* Any unresolved matching ambiguity.

Use the existing MQTT geographic reporting interface so that recognized MM positions appear on the Pi dashboard as they do today.

Preserve sufficient Hall/PWM observations and matching decisions on the Pi to examine the results.

Reporting must not affect navigation decisions or manual motor control.

7. Explicit prohibitions

Do not carry forward vestigial firmware merely because it existed in the source sketch.

In particular, do not include:

* QUORUM navigation or geographic recovery.
* DART/EWO navigation evaluators.
* IR navigation or IR credibility machinery.
* Station stops or station timing.
* AUTO enrollment, automatic throttle, or dispatcher control.
* Five-strike or five-mismatch stopping.
* LOSS OF PATTERN motor intervention.
* Automatic geographic redeclaration.
* Automatic reference learning or model succession.
* Independent Hall-opening acceptance gates.
* Separate lobe classifiers with geographic authority.
* Unused compatibility interfaces, state machines, or telemetry schemas.
* Placeholder implementations for features we have deliberately excluded.

Do not retain dormant subsystems for possible future use.

If a function has no role in manual control, physical protection, Hall acquisition, PWM movement estimation, continuous pattern matching, or reporting, it should not be included.

Do not replace removed mechanisms with new mechanisms serving the same purpose under different names.

8. Separation of authority

The firmware has two independent responsibilities.

Manual controller: Executes operator commands and retains established physical/electrical protections.

NAVI_RVM observer: Interprets Hall observations and reports geographic progression.

The observer cannot write motor PWM, change direction, stop the locomotive, withdraw manual control, or require redeclaration.

This separation must be enforced by the code structure, not merely by conditional checks.

9. Verification

Before delivery, demonstrate:

1. Successful compilation for Otto’s ESP32 configuration.
2. Manual throttle, direction, STOP, and E-STOP remain operational.
3. Continuous Hall acquisition reaches the matcher.
4. The fixed reference is loaded correctly.
5. The matcher identifies successive MM patterns.
6. A single unusual ensemble does not reset established alignment.
7. PWM prediction alone cannot advance the geographic count.
8. Navigation output reaches the existing dashboard interface.
9. No navigation-originated path can modify motor operation.

Focused software checks are appropriate. A comprehensive simulation campaign is not required before a controlled field test.

10. Deliverables

Create a new sketch named:

NAVI_RVM_MANUAL_1_0

Work on a dedicated development branch.

Commit and push:

* Complete sketch and necessary supporting files.
* Fixed magnetic reference.
* Concise build and verification report.
* Code-to-architecture reconciliation.
* Dashboard reporting details.
* Short field-test instructions.

Make every deliverable accessible through GitHub for independent review by Claude.

Do not flash Otto or modify operational firmware.

⸻

Final instruction to Codex

Do not build another navigation framework. Build a magnetic-pattern matcher attached to a manually controlled locomotive.

The purpose of this experiment is to determine whether a filtered Hall waveform, interpreted against a known fuzzy magnetic landscape with polarity and PWM movement context, can reliably identify the locomotive’s geographic progression.

We are intentionally withholding failure-recovery mechanisms because they would obscure the result.

A mismatch is information, not a command to intervene.

A missing magnet is an incomplete portion of the puzzle, not permission to move the puzzle board.

Previously placed pieces and the printed backing jointly determine where subsequent pieces belong.

Keep the implementation small enough that we can identify every mechanism contributing to the geographic decision.

If the concept works, we can build upon it. If it doesn’t, we need to know that before adding anything else.

Architectural fidelity and simplicity take precedence over preserving previous code.