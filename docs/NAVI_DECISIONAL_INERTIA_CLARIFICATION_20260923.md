# NAVI Decisional Inertia: Operator Clarification and Code Audit

Status: governing operator design clarification; implementation only partial.
Recorded 2026-09-23 following the IR stop-retention investigation.

## Governing Rule

Where Toby has been constrains where Toby can be now. NAVI maintains a preferred
physically coherent trajectory, not a fresh whole-route identity guess at each
Hall signal. Established knowledge has greater standing than an unproven
alternative. Correction remains possible when accumulated, independent evidence
supports a physically coherent replacement. Decisional inertia is not permanence.

Unavailable evidence is not contradictory evidence. An IR epoch break removes
the right to subtract counts across that gap; it does not delete NAVI's route
history, direction, previous location or other evidence. Local uncertainty stays
local. Pattern similarity cannot create physical reachability.

IR is an instrument, not the locomotive-state decider. NAVI, the motor/controller,
Hall observations and IR are one operating system. Applied drive/braking state,
elapsed time and recent motion inform interpretation without converting commanded
PWM into measured distance. No isolated observation must decide everything.

The operator prioritizes simplicity, real installed-hardware evidence and
preponderance of evidence. Do not add control complexity solely because a
synthetic sensor-only scenario can be constructed. Keep test assumptions and
physical plausibility explicit.

## Current Code: What Already Works

Audited `Navigator.h` and `ProximalRecovery.h` in NAVI_COHERENCE_0_6_IR_HEALTH:

- IR loss alone does not unset navigation. Hall interpretation continues from
  the expected successor using the existing timing fallback.
- An isolated wrong pole can advance at the expected landmark with a recorded
  polarity discrepancy. It does not cause a global position search.
- An early extra Hall signal is refused without deleting position/history.
- Supported missed passages create UNKNOWN history entries, not invented poles.
- Established recovery retains the incumbent if reference travel is unavailable;
  a new IR epoch does not independently relocate the train.
- An out-of-distance-window event retains the incumbent. This currently labels
  the Hall event NON_LANDMARK_HALL; that label should not be mistaken for proof
  that Hall rather than IR supplied the erroneous evidence.

New `tools/test_nav_decisional_inertia.cpp` exercises all 171 starting markers
in both directions (342 cases). It establishes 12 coherent landmarks, breaks
IR continuity, admits local Hall continuation including an isolated wrong pole,
rejects early extra Hall noise, reacquires IR, and handles a supported missed
landmark. Position, history and UNKNOWN entries survive; no relocation/reset.
Compiled with C++17, Wall/Wextra/Werror, ASan/UBSan; all cases pass.

This is a baseline of existing behavior, not certification of the complete
system, motor-control loop, station behavior or all ambiguous observations.

## Remaining Gaps: No Silent Policy Change

1. `ProximalRecovery` uses a ten-position rolling score. Multiple discrepancies
   permit a unique one-vote improvement; the code expressly has no extra margin.
   It distinguishes settled/startup but does not retain a graduated strength
   for a longer coherent trajectory. Stronger-established-history semantics
   are therefore not fully implemented.
2. Startup alternatives within +/-10 remain possible when no travel pair can
   be assessed. +/-10 is an outer search limit, not evidence of reachability.
   Established recovery instead holds when travel is unavailable.
3. Established recovery's physical gate is heavily dependent on continuous IR
   travel. It does not combine elapsed time, motor/braking context and recent
   speed to retain useful physical constraints across an IR gap. Missing IR
   must neither erase history nor imply unrestricted motion.
4. `PositionView` exposes one preferred position; it does not represent the
   explicit bounded uncertainty set described by the operator. Adding a
   global list of 171 equal hypotheses would be the wrong remedy.

Decision 0098 and the Twenty Questions previously requested a unique-better
feasible correction without an extra score margin. The latest clarification
requires evidentiary asymmetry tied to established knowledge. Do not silently
replace the earlier rule with an arbitrary new vote count or an absolute veto
against correction. A focused follow-up must specify what independent evidence
permits revision while preserving already-established history.

## Scope of This Change

Recorded the operator's supplied text separately and added the behavioral
baseline. No production code, MQTT shape, motor control or installed firmware
changed. No flash. Keep the separate R3 optical proposal under review; a sensor
quality change is not a mandate to reset route knowledge.

Next implementation work should address the concrete recovery-policy gaps
above, with clear decision reasons visible to the operator. Do not claim this
clarification is fully implemented simply because the baseline tests pass.

Reproduce:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH tools/test_nav_decisional_inertia.cpp -o /tmp/test_nav_decisional_inertia
/tmp/test_nav_decisional_inertia
```

Source: `NAVI_DECISIONAL_INERTIA_OPERATOR_TEXT_20260923.txt` (operator-supplied
attachment, preserved verbatim). This record does not supersede independent
E-stop or electrical protections.
