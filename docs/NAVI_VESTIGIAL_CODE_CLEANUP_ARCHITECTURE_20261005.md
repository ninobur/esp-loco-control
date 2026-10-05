# NAVI Vestigial Code Cleanup

## Purpose

The current NAVI/EWO firmware has evolved through a long sequence of experiments, architectural changes, field discoveries, defensive additions, and compatibility work.

Some code in the current working sketch exists because it solves a current operational requirement.

Other code remains because it solved—or anticipated—a problem during an earlier stage of development.

The purpose of this cleanup is to distinguish those two categories and remove code that no longer represents a legitimate part of the current production architecture.

This is not primarily a code-size exercise.

The concern is conceptual residue.

Vestigial code can create future behavior merely because an old condition, field, state, or defensive branch still exists. It can also mislead a future developer or AI agent into believing that an obsolete concept remains part of the intended design.

The governing principle is:

Production code should represent the current railroad and current architecture, not the history of how the architecture was developed.

Historical experiments belong in Git history, decision records, field records, tests where still relevant, and archived/reference material.

They do not need to remain executable production behavior.

## 1. Cleanup standard

For every significant branch, state, field, timeout, transition, compatibility check, defensive condition, or special case in the production sketch, ask:

What legitimate current physical or operational condition requires this code to exist?

A satisfactory answer should identify something real and current, such as:

* a physical sensor condition;
* a current navigation requirement;
* an actual operator command;
* a current protection requirement;
* a current communications requirement;
* a current hardware configuration;
* a legitimate continuity boundary;
* a current execution requirement.

The following are not sufficient justification by themselves:

* “it was already there”;
* “an older version used it”;
* “a test expects it”;
* “it might be useful someday”;
* “another transmitter could theoretically do this”;
* “it is defensive”;
* “it does no harm”;
* “it provides compatibility with a design that is no longer used.”

If a behavior has no legitimate current source, the default should be to remove it rather than preserve it indefinitely.

## 2. Do not confuse robustness with hypothetical complexity

NAVI should be robust against real uncertainty.

That does not mean production firmware should contain executable branches for every hypothetical future configuration.

A defensive branch is justified when it protects against a condition that can actually arise in the current system.

A branch that handles an impossible or abandoned condition creates a different kind of risk:

* future code may accidentally trigger it;
* telemetry may expose meaningless states;
* agents may design around it;
* tests may preserve it;
* documentation may begin explaining it;
* a dead concept may gradually reacquire architectural authority.

Therefore:

Defensive code must have a defensible current threat or failure model.

Hypothetical flexibility is not automatically robustness.

## 3. Configuration is not runtime state

A recurring source of unnecessary complexity is treating settled physical configuration as though it were dynamic runtime state.

If a value describes installed hardware and there is no legitimate mechanism for changing it during operation, it should normally be treated as configuration.

It should not automatically create:

* runtime transitions;
* continuity boundaries;
* restart behavior;
* epoch changes;
* recovery paths;
* challenged states;
* operator-facing warnings.

The existence of a field in a packet does not prove that the field is expected to change during operation.

The existence of a comparison does not prove that the compared transition is legitimate.

Production behavior should follow the actual physical system.

## 4. IR wheel pitch is settled configuration

The current Type-5 IR transmitter uses the installed standard LGB spoked wheel geometry.

The configured pitch is:

9.652 mm per completed pulse

or:

9652 µm per completed pulse

The transmitter constructs its measurement object with that value and does not change it during a normal run.

There is no runtime mechanism that recalibrates wheel pitch.

For the foreseeable architecture, the same LGB spoked wheel type is the standard.

Therefore:

IR pitch is configuration, not runtime state.

The production receiver does not need to model a legitimate same-run PitchChanged event.

A changed source value may still be malformed or inconsistent data, but it does not represent a normal operational transition that deserves its own continuity model.

## 5. Calibration ID is not an active runtime calibration mechanism

The current transmitter initializes calibration ID separately and does not change it during a normal run.

There is no current procedure in which:

* the wheel is recalibrated during operation;
* a new calibration is selected during operation;
* the operator challenges a calibration;
* NAVI negotiates a calibration;
* calibration changes restart an operational epoch.

Therefore language and code implying an active runtime calibration lifecycle are misleading.

Examples include concepts such as:

* “challenged calibration ID”;
* “calibration changed” as a normal epoch cause;
* comments suggesting calibration remains provisional until later confirmation;
* runtime recovery behavior based on a calibration transition that has no current source.

The current physical configuration is settled.

Production code should say so by its structure.

## 6. Remove pitch/calibration transitions as epoch causes

Earlier architecture treated pitch and calibration changes as continuity boundaries.

That was useful defensive/developmental thinking while the measurement architecture was still being explored.

It is not part of the current operational model.

Remove:

* PitchChanged as a legitimate epoch-transition reason;
* CalibrationChanged as a legitimate epoch-transition reason;
* same-boot admission barriers whose sole purpose is handling these supposed runtime transitions;
* tests whose only purpose is proving those transitions;
* documentation presenting them as supported normal runtime behavior.

This removal must be narrow.

It does not authorize removal of legitimate measurement validation.

## 7. Preserve actual IR validity checks

Removing fictional runtime calibration transitions does not mean accepting malformed measurements.

Continue validating current Type-5 measurement integrity where appropriate.

Examples include:

* pitch must be nonzero;
* transmitted cumulative nominal distance must be internally consistent with pulse count and configured pitch;
* packet structure/version must be valid;
* counters must obey legitimate continuity requirements;
* time ordering must remain valid;
* transport freshness must remain valid;
* measurement epochs must still separate genuine source/boot continuity boundaries.

The distinction is:

Validate that the current measurement frame is internally valid. Do not invent a normal operational lifecycle in which the physical wheel geometry changes during the run.

## 8. Epochs should correspond to genuine continuity boundaries

Epoch is useful because it describes a real continuous period of measurement validity.

An epoch should restart only when something occurs that genuinely breaks the ability to combine measurements across the boundary.

Examples may include:

* transmitter boot change;
* counter rollback/reset;
* genuine source replacement;
* unrecoverable ordering discontinuity;
* other actual measurement-frame discontinuities supported by the current architecture.

An epoch should not restart because of a theoretical configuration change that cannot legitimately occur.

The question for every epoch cause is:

What real event on the current railroad produces this boundary?

If there is no answer, the epoch cause should not exist in production.

## 9. Remove developmental language from production code

Comments matter.

A comment can preserve an obsolete concept long after the executable behavior has disappeared.

Production comments should describe:

* what the current code does;
* why a current constraint exists;
* what physical or architectural fact it represents.

They should not preserve unresolved developmental conversations that are already settled.

For example, language such as:

“Calibration id stays zero until the installed wheel is confirmed.”

is inappropriate once the installed wheel and pitch have been established.

Such comments imply that a future runtime or configuration transition remains expected.

If the question has been settled, remove the provisional language.

Git history and decision records preserve the developmental history.

## 10. Remove abandoned compatibility behavior

Compatibility code deserves the same scrutiny.

For every compatibility path, identify:

1. the other currently supported component;
2. the incompatible behavior being accommodated;
3. whether that component can actually communicate with this production firmware;
4. whether continued compatibility is an explicit requirement.

If the compatibility target no longer exists in the operational system, the compatibility path should not remain merely because removing it feels risky.

Compatibility with historical software is not automatically a production requirement.

Where legacy systems intentionally remain operational, their boundaries should be explicit rather than inferred from old code.

## 11. Remove tests that canonize obsolete behavior

Tests are not architectural authority.

A test proves that code behaves as the test expects.

If the expected behavior has been deliberately superseded, the test is obsolete.

During cleanup:

* remove tests whose sole purpose is preserving removed developmental behavior;
* revise tests that combine legitimate behavior with obsolete assumptions;
* retain tests that protect genuine current requirements;
* add tests proving that removed concepts no longer influence production behavior where useful.

Do not retain a production feature merely to keep an old test green.

The architecture determines the expected behavior.

Tests follow the architecture.

## 12. Remove telemetry for states that no longer exist

Telemetry should describe the current system.

If an architectural concept is removed, review telemetry for fields, event names, counters, warnings, or diagnostic states that continue to expose it.

Dead telemetry is not harmless.

It can cause:

* operator confusion;
* dashboard clutter;
* incorrect debugging conclusions;
* future agents to infer nonexistent architecture.

If a concept no longer exists operationally, its telemetry should normally disappear as well.

Historical analysis remains available through old logs and Git history.

## 13. Do not retain code merely for possible future use

Future requirements should be implemented when they become requirements.

The production sketch should not accumulate dormant mechanisms for speculative future possibilities.

Examples include:

* alternate wheel calibrations that do not exist;
* runtime pitch changes that cannot occur;
* hypothetical transmitter implementations;
* unused compatibility modes;
* abandoned experimental states.

When a future requirement becomes real, Git history and architectural records can inform its implementation.

Until then:

Simple current code is preferable to speculative flexibility.

## 14. Distinguish observation, judgment, and authority

A recurring NAVI design principle is that evidence should remain evidence.

Sensors report observations.

NAVI interprets those observations.

Operational authority comes from the appropriate current source.

Old code sometimes embeds conclusions into low-level mechanisms because earlier architectures had less information.

During cleanup, look for places where:

* a sensor condition has accidentally become operating authority;
* a diagnostic state changes behavior;
* a compatibility condition controls current operation;
* a historical defensive assumption has become a decision rule.

The preferred architecture remains:

OBSERVATION → EVIDENCE → EVALUATION → DECISION → ACTION

Removing vestigial code helps preserve those boundaries.

## 15. Preserve hard protection that has a current physical basis

This cleanup is not permission to remove genuine protection.

Hard protection remains appropriate where it answers a current physical question such as:

Could this event physically have occurred?

Examples include:

* physical reachability;
* genuine measurement continuity;
* actual source freshness;
* electrical protection;
* e-stop;
* low-voltage protection;
* other explicitly accepted current protection mechanisms.

The test for retention is not whether code is restrictive.

The test is whether it corresponds to a legitimate current physical or operational condition.

## 16. Do not silently replace removed code

Removal should simplify the architecture.

Do not respond to removal by automatically inventing a new:

* fallback;
* timeout;
* governor;
* state machine;
* compatibility layer;
* warning state;
* recovery mechanism.

If removing vestigial code exposes a genuine unresolved operational requirement, surface that requirement for review.

Do not assume that every deleted mechanism requires a replacement.

Sometimes the correct replacement is:

nothing.

## 17. Cleanup process

The cleanup should proceed deliberately.

For each candidate remnant:

1. Identify the code.
2. State what it currently does.
3. Identify why it originally existed if known.
4. Identify the legitimate current condition that would exercise it.
5. Determine whether that condition actually exists.
6. Classify the code as:
    * current and necessary;
    * current but misplaced;
    * diagnostic only;
    * historical/developmental;
    * obsolete compatibility;
    * speculative;
    * unknown and requiring operator clarification.
7. Remove only after the classification is understood.
8. Update dependent tests, telemetry, and documentation.
9. Compile and test after each coherent cleanup group.

The objective is not indiscriminate deletion.

The objective is architectural reconciliation.

## 18. Historical evidence must remain available

Removing code from production does not erase the development history.

Preserve important history through:

* Git commits;
* decision records;
* field records;
* archived/reference code where appropriate;
* historical logs;
* documentation explicitly marked historical.

This allows production code to remain simple without losing the reasoning that produced it.

## 19. Production-code test

At the end of the cleanup, a future reviewer should be able to ask of every significant mechanism:

Why does this exist?

and receive an answer based on the current railroad.

Not:

“Because an earlier experiment needed it.”

Not:

“Because someday something might change.”

Not:

“Because removing it seemed dangerous.”

The desired production sketch should be easier to understand precisely because its concepts correspond to real current concepts.

## 20. Governing principle

The cleanup is guided by three related rules:

Respect the intelligence of NAVI.

Simple solutions are better.

and:

Production code should describe the railroad that exists, not every railroad we once considered building.

The goal is not fewer lines for their own sake.

The goal is to remove obsolete conceptual authority so that future development begins from a clean and truthful architecture.
