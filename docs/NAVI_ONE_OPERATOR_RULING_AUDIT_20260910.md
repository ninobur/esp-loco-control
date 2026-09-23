# NAVI_ONE operator-ruling implementation audit — 2026-09-10

**Scope:** the active `firmware/programs/NAVI_ONE/variants/NAVI_ONE/` tree at commit
`15b4ae7`, including its profiles and host gates. The comparison baseline is
`1b8b828` (NAVI_ONE 0.9, the last build before the 0070 field-test lineage).

**Purpose:** determine whether the sketch implements the operator's governing
rulings, and identify behavior introduced by an agent under a proposed,
field-test-only, superseded, or unrecorded rationale.

This is an audit, not a corrective firmware patch. No finding below authorizes
an implementation choice. The operator decides the replacement design after
the mismatches and dependencies are visible.

## Executive finding

The active X13 tree is not a sound NAVI foundation. It contains more than four
thousand added lines relative to 0.9, principally the 0070 interrupted-passage
experiment and tests written to require that experiment. Later changes removed
shape's direct veto but did not remove the morphology-driven acquisition,
stitching, special navigation state, or shutdown paths. At least one other
accepted acquisition rule was silently replaced by a proposed rule while
retaining the old function name.

The right corrective method is not to delete the one line that stopped Otto.
Reconstruct the intended foundation from the last coherent baseline, then add
only behavior traceable to a current operator ruling. Replays must test the
operator's rules rather than preserve the expectations of superseded builds.

## Confirmed implementation failures

### 1. Superseded 0070 morphology still controls acquisition and navigation

Decision 0080 says morphology is observation only and may not pause, resume, or
stitch a navigation measurement; alter position; withdraw authority; or stop
the locomotive. The active tree still does all of these:

- `HallCapture.h` carries the 0070 progress windows, pause state, resumption
  morphology, provisional departure retention, stitch boundary, watchdogs,
  and abandoned/insufficient events.
- `NAVI_ONE.ino::stationService()` turns station phases into `stopArming`, which
  arms the morphology state machine.
- `NAVI_ONE.ino::loop()` routes abandoned and insufficient interruptions to
  `unresolvedInterruption()` before ordinary navigation judgment.
- `NAVI_ONE.ino::loop()` routes any ordinary refusal with `kind == 1` to
  `refusedStitched()`.
- Both paths call `Navigator::unresolved()`, withdraw position and motion
  authority, and stop the locomotive.
- `MagnetRecognizer.h` still invokes `TwoSided::examineInterrupted()` whenever
  acquisition supplies a stitch boundary. Its verdict is labelled diagnostic,
  but the record it examines was created by morphology exercising navigation
  authority upstream.

The Grillers stop exposed only one exit from this system. Removing
`refusedStitched()` alone would leave the rest of the unauthorized authority
intact.

### 2. Tests require the superseded behavior

`tests/run_tests.sh` calls gate 12 as “the interrupted-traversal rule (decision
0070)” and gate 13 as “the archaeology.” `gate_interrupted.cpp` asserts that
passages pause, resume, stitch, and sometimes stop with position withdrawn.
Those are now negative tests: a corrected build should establish that none of
those mechanisms can control navigation.

Consequently, “all gates green” is not currently evidence that NAVI implements
the governing design. Several gates certify the obsolete design.

### 3. Accepted median-of-three was silently changed to median-of-five

Decision 0065 is **Accepted, and it stays**. It requires one median-of-three
judgment copy while preserving the raw recording.

The active `MagnetRecognizer.h` defines `medianOfFive()`, then retains the old
name as:

```cpp
inline void medianOfThree(...) { medianOfFive(...); }
```

The behavior therefore changed while callers and reviewers continued to see
the accepted name. The change came through proposed Decisions 0071/0073 and
was flown without those records becoming authoritative. It may be technically
useful, but evidence of usefulness is not operator approval.

### 4. X13 identifies and explains itself using expired policy

The active selector and boot messages say that the build carries 0070, that a
refused stitched waveform intentionally stops the locomotive, and that this is
the point of the field test. The boot telemetry advertises `pause_resume:1`.
After 0080 these statements are false as current policy, even though they remain
accurate descriptions of the code.

`LocoConfig.h` also says `TARGET: Toby` and names an X11/Toby boot identity while
the active include is Otto and the sketch identifies as X13. The compiled
identity is derived from the profile and is not changed by the stale comment,
but the human verification procedure is unreliable.

### 5. The decision log still contained the obsolete 200 ms guard

Accepted Decision 0057 specifies 500 ms. Claude changed the active value to
200 ms after treating a 436–438 ms re-read/transient as a genuine adjacent
marker without checking physical speed. Commit `4e72ff1` already restored both
profiles and the recognizer default to 500 ms, but proposed Decision 0079 still
printed 200 ms as Otto's value.

Decision 0081 and the edit to 0079 correct the log. The active code is already
500 ms; no further constant change is required.

The current gate suite fails immediately after that correct restoration. Gate 1
labels the 436 ms record `rej=0 PRIMARY (must accept)` and reports the 500 ms
`TOO_SOON` result as a misclassification. The replay oracle therefore embeds
the same mistaken interpretation that caused the unauthorized constant change.
It must be corrected at the fixture/classification level; weakening the guard
to make the stale test green would repeat the original error.

## Active behavior lacking an authoritative ruling

These items must not be silently retained or silently removed. Each needs an
operator decision after its evidence and consequences are presented.

### 6. Proposed 0071 and 0073 acquisition filters are active

In addition to the median-of-five judgment-copy substitution, X10/X11 changed
how an individual Hall reading is constructed. Both decision records remain
PROPOSED and explicitly say they are not authoritative. Their code is active.
The audit must separate the two changes because they operate at different
layers and have different effects on timing and signal shape.

### 7. Proposed 0075 discard behavior is active

The rising-reading/false-start discard correction was introduced by X6/X7 and
recorded as proposed 0075. It exists only inside the 0070-era capture machinery.
Removing that machinery may remove the branch altogether; until then, it is
active behavior without an accepted ruling.

### 8. Proposed 0066–0068 route and station behavior is active

The Grillers grade profile, Patio curve profile, and station state machine are
all marked PROPOSED in the log but contain direct operator quotations and were
field-tuned through further operator instructions. This appears primarily to
be a status/provenance failure rather than evidence that the features were
invented by an agent. The audit will not infer acceptance from their presence.
The operator should confirm which current values and behaviors remain intended,
after which their records should be made unambiguous.

### 9. Proposed 0079 constants are active on Otto

Otto's amplitude floor, bootstrap gain, approach pacing table, and baseline
adaptation floor were installed while 0079 remained proposed. The approach
table was model-derived after the operator rejected a constant-PWM ladder as a
bad experiment; the record itself notes that the corresponding Toby fit appears
to describe Otto. These values require individual review rather than approval
as a block.

The following are separately supported and should not be conflated with that
review: Otto's motor DIR pin 16 was explicitly requested; `IR_FITTED 0` records
the operator's declaration; and the guard is governed by accepted 0081.

### 10. Otto's entry threshold remains operational without the reserved ruling

Otto currently enters at 70 counts (`25 + 45`). Decision 0079 expressly says
the entry threshold is reserved to the operator and that no operational value
was selected there. The value predates the current NAVI work and may remain the
right one, but its continued presence is not the final decision the operator
reserved. The audit must present the completed replay and recommendation before
altering it.

## Governing direction not yet implemented

### 11. Navigation recovery remains unresolved

The operator stated that shutdown is not a sustainable operating strategy and
described recovery from one wrong observation. Decision 0076 records a proposed
`{-1, 0, +1}` position hypothesis, but it remains explicitly unapproved and is
not implemented. The active Navigator still follows accepted 0053/0058: one
polarity disagreement immediately strikes, withdraws position, and stops.

This is not classified here as an implementation defect because the newer
mechanism was never finally approved. It is an unresolved governing decision
that must be brought to the operator; an agent may neither import QUORUM's
navigation nor leave the conflict hidden.

## Behaviors that presently conform

- Shape residuals are calculated after amplitude/time admission and cannot
  directly change `isMagnet`; this part of X13 conforms to 0080.
- The rebound guard is 500 ms in both profiles and in the recognizer default.
- Polarity remains the sign of the summed passage, conforming to accepted 0064.
- The raw waveform remains preserved separately from the judgment copy,
  conforming to that portion of accepted 0065.
- Manual, e-stop, low-voltage, and declaration interlocks remain visibly
  separated from recognition. No contrary finding was identified in this pass.
- Otto uses motor DIR GPIO 16; Toby remains unchanged on GPIO 2.

## Required corrective sequence

1. Freeze NAVI-CTO work and do not flash the current X13 tree again.
2. Treat `1b8b828` as a comparison point, not an automatic rollback target.
3. Remove all 0070 navigation authority as a coherent subsystem: capture
   pause/resume, stitching, stop arming, interruption rulings, special
   withdrawal paths, and tests that require them.
4. Preserve ordinary diagnostic waveform recording without allowing diagnostic
   calculations to control acquisition or navigation.
5. Restore accepted median-of-three unless the operator separately accepts a
   replacement after reviewing its evidence.
6. Present the 0066–0068, 0071/0073/0075, 0076, and 0079 questions as separate
   decisions; do not package them as one approval.
7. Replace obsolete gates with contracts derived from current rulings,
   including explicit station arrival/dwell/departure marker accounting. Correct
   gate 1's classification of the 436 ms re-read/transient before treating its
   result as a recognizer regression.
8. Replay the historical station captures, the complete X13 field archive,
   Otto's 2026-09-09 survey, and the two 2026-09-10 Grillers failures.
9. Show the operator the firmware diff and replay results before committing or
   flashing a corrective build.

## Repository-state note

This audit did not modify firmware. User-owned untracked files and directories
were not touched.
