# NAVI_ONE — magnet-only navigation feasibility

**Date:** 2026-09-19  
**Question:** Can Lowline navigation be made reliable using magnets alone?  
**Status:** Analysis only. No firmware was changed.

## Conclusion

Yes, with an important qualification:

**Magnets alone can support reliable operational navigation if NAVI_ONE is
allowed to represent uncertainty and recover from an observation error.
Magnets alone cannot guarantee continuously correct, fail-safe absolute
position at every instant.**

The field record does not show that Hall sensing is intrinsically too
unreliable. It shows that the existing navigation architecture is brittle:
one Hall observation is immediately converted into one irreversible location
advance. A wrong polarity, merged passage, missed passage, or false passage
therefore creates a hidden position error that eventually appears as a strike.

The strike may occur several magnets after the causal observation because the
route contains runs of equal polarity. Strike location is therefore not a
reliable indication of fault location.

## What the evidence establishes

### Hall passages can be detected reliably

X13 completed a two-hour, both-direction run with 3,284 magnet passages, zero
polarity disagreements, and 76 of 76 station stops. With the full passage
preserved, none of those 3,284 passages would have been rejected by the old
shape rule.

This establishes that a Hall detector can produce a highly reliable stream of
passage observations. It does not establish that every observation can safely
be treated as an infallible location update.

### Most failures were architectural, not failures to sense a magnet

The recurring failure mechanisms were:

1. A single ADC conversion was allowed to determine polarity or distort a
   shape decision.
2. A reference derived from the same signal being classified moved onto a
   magnetic shelf or fringe field.
3. A real passage was refused by a shape, amplitude, duration, or guard rule;
   the single location counter had no way to recover.
4. A fringe field or stopped-on-magnet condition held one passage open and
   swallowed later magnets.
5. A false stationary event or duplicate was permitted to advance position.
6. Opening polarity and whole-window polarity sometimes disagreed, but the
   disagreement was recorded rather than used as evidence of uncertainty.

These mechanisms are serious, but none proves that the route cannot be decoded
from magnets. They prove that one observation must not have irreversible
authority over location.

## The information magnets provide

Each magnet supplies a discrete landmark crossing and an observed polarity.
The ordered polarity pattern supplies route information. It is a sequence
code, but not a uniformly strong one: long same-polarity runs delay detection
of an insertion, deletion, or polarity error.

Consequently, a magnet-only system should answer:

> After this passage and the recent sequence, which route positions are still
> physically and logically possible?

It should not require every passage to answer:

> What is the one exact location now?

The first question permits recovery. The second turns a transient sensor error
into permanent state corruption.

## Required magnet-only architecture

### 1. Physical passage detector

The detector should emit observations, not location decisions. An observation
should retain at least:

- passage opening and closing times;
- opening polarity;
- integrated or whole-passage polarity;
- amplitude and duration;
- reference value and reference provenance;
- confidence and any internal disagreement flags.

Multiple ADC conversions should form each Hall sample. Real passages should
not be refused merely because their waveform differs from an ideal shape.
Only a physically demonstrated same-magnet rebound guard should suppress an
otherwise credible passage.

### 2. Interval-locked reference

The reference should be established only from track known to be clear, then
locked for the interval leading to the next landmark. It must not continuously
learn from a slow approach, station dwell, fringe field, or departure ramp.

A candidate reference needs validity evidence, age, and provenance. Low sample
spread proves only that a level is stable; a stable magnetic shelf is still the
wrong reference.

The unresolved engineering problem is defining "known clear" using magnets
alone. A quiet-time test is insufficient because a long magnetic approach can
also be quiet. A conservative magnet-only implementation must therefore retain
the previous valid reference when a clean replacement cannot be proved and
stop if the resulting ambiguity affects station safety.

### 3. Sequence estimator

Navigation should maintain a bounded set of route hypotheses. For each new
observation it should consider at least:

- the expected next marker;
- an ambiguous-polarity version of that marker;
- one missed marker;
- one duplicate or rebound observation;
- one false passage, where physical evidence permits it.

Subsequent observations should remove inconsistent hypotheses. Distinctive
polarity subsequences or an intentional synchronization procedure can restore
one absolute position.

This need not become an unconstrained probabilistic system. The Lowline route
is finite, the error alternatives can be bounded, and hypotheses can be
discarded quickly when they contradict later observations or motion commands.

### 4. Safety controller

The controller may continue only while every plausible position hypothesis
agrees that the commanded action is safe. If plausible hypotheses disagree
about whether a station or stopping boundary is approaching, the safe result
is a controlled stop and resynchronization.

Position must never advance solely because time passed, PWM was commanded, or
the locomotive was stationary in a field.

## What magnets alone cannot prove

Magnets do not directly measure:

- physical movement between landmarks;
- distance travelled after a landmark;
- whether wheel rotation produced movement;
- exact stopping distance;
- whether the locomotive has completely left a magnetic field.

A magnet-only controller can handle these absences conservatively, but cannot
make them observable. This is the boundary between reliable operation and
continuous fail-safe position truth.

A second physical channel, such as distance or wheel motion, would materially
improve safety. It should be asymmetric: it may veto an advance or command a
stop, but it must not invent, identify, or count a magnet.

## Practical verdict

A magnet-only NAVI_ONE is supportable if "reliable" means:

- normally completes the route and station stops;
- survives one ambiguous, missed, duplicated, or wrongly classified
  observation without silently corrupting position;
- resolves uncertainty from later route evidence where possible;
- otherwise makes a controlled stop instead of asserting a false location.

It is not supportable if "reliable" means:

- always reports one exact position;
- never pauses for ambiguity;
- continues normally after every possible Hall/reference failure;
- provides fail-safe motion or distance knowledge between magnets.

The central design change is therefore not a more elaborate magnet classifier.
It is replacing the irreversible location counter with a recoverable sequence
estimator, while making the detector report physical evidence without silently
turning that evidence into position.

## Repository evidence

- `docs/NAVI_ONE_FIX_TO_STRIKE_CROSS_VARIANT_ANALYSIS_20260919.md`
- `docs/NAVI_SIMPLIFIED_LOCKED_BASELINE_FINDINGS_20260919.md`
- `docs/NAVI_SIMPLIFIED_BASELINE_DESIGN_CHECKLIST_20260918.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_05_IMPULSE_FLIPPED_POLARITY_AT_MM70.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_07_ARTIFACT_IN_THE_TAIL_WRONG_SHAPE_MM169.md`
- `docs/NAVI_ONE_FIELD_FINDING_08_BASELINE_LATCH_SWALLOWS_MARKERS.md`
- `docs/NAVI_ONE_0_9_FIELD_FINDING_13_THE_DEPARTURE_MAGNET_LANDS_INSIDE_THE_DWELL_PASSAGE.md`
- `docs/NAVI_ONE_X18-X21_RAW_AUDIT_20260916.md`
- `firmware/programs/NAVI_FRESH/README.md`
