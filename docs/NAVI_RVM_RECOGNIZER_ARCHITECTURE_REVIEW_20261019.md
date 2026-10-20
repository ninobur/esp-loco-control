# NAVI_RVM recognizer: what it actually does, set against what was intended

Claude (Opus 5.5), 2026-10-19, at David's request before any retraining.
**Read-only review. No retraining, no firmware change, no new rule.**

**Code examined:**
- `firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL/RvmShape.h` (firmware);
- `analysis/navi_rvm_shape_20261018/scripts/rvm_shape.py`, `rvm_local.py`,
  `s02_annotate.py`, `s10_local_experiment.py` (training).

## The intention (David)

NAVI_RVM recognizes the continuous pattern MM₁ → BL → MM₂ in the Hall tracing:
big bumps, with their lobes and shoulders, on a more-or-less flat line read as a
whole. A baseline includes gentle curves. A magnetic ensemble is its principal
excursion, associated lobes, shoulders and return to baseline. The general
composite learns magnetic structure as a class; the polarity map supplies the
expected sequence.

## What the implementation actually does

### 1. How are the beginning and end of an ensemble identified?

**They are not identified directly.** The recognizer is a **multiscale sliding
window classifier**:
- For every new 20 ms block e, it scores every window ending at e, at 19 fixed
  widths W from 8 to 444 blocks (0.16–8.9 s, steps of ×1.25).
- The ensemble is then *declared* to be the **middle half of the winning
  window**, [c − W/4, c + W/4].
- Its start and end are therefore a consequence of which window scored best, on
  a fixed width grid.
- No step finds the departure from the baseline or the return to it.

### 2. Does Model A evaluate the complete tracing of an ensemble?

**Partly.**
- **How a window is turned into features:**
  - resampled linearly to 32 points (a 146-block window is sampled every 4.6
    blocks);
  - the local baseline level is subtracted: a least-squares line through the
    preceding recognized baseline, evaluated at the window centre;
  - the largest excursion is scaled to +1 (this also sets the polarity), and the
    window is reversed in time for CCW;
  - it is summarized by **12 DCT coefficients**, a smooth, low-resolution
    outline, plus one size feature, log(amplitude ÷ baseline noise).
- **What the training made a "magnet" window:** a window centred on an
  annotated encounter and **twice its duration**. The encounter fills the
  middle half and baseline fills a quarter on each side.
- **So:** the principal lobe and shoulders, and the start of the return to
  baseline, are inside the window. Fine secondary lobes are only partly
  represented by 12 smooth coefficients.
- **The amplitude is removed** before the shape is judged; size survives only as
  amplitude relative to the 20 ms step noise of the preceding baseline.

### 3. Does another mechanism select candidate windows?

**No separate detector.** Every window at every width is a candidate, about
1,000 windows a second while moving. Selection is entirely the decision rule in
item 4.

### 4. What exactly causes the recognizer to declare a magnet?

1. **A candidate:** any window whose log-likelihood ratio, log p(E) − log p(N),
   is **greater than 0**. Each class is a Gaussian mixture fitted to the
   training windows. The threshold 0 means equal prior odds.
2. **Holding the best one:** the best candidate is held. A later candidate
   replaces it if its core overlaps the held core and it scores higher.
3. **Acceptance:** the held candidate is accepted once the tracing has run
   **half its width beyond its end** with nothing better overlapping.
4. **Polarity** is the sign of the largest excursion from the baseline line
   inside the window.
5. **After acceptance:** the next baseline starts at the window's end, and
   later windows are re-scored against it.

There is **no minimum score**, no requirement relating the candidate to
anything after it (beyond the half-width wait), and no comparison with the size
of other magnets.

### 5. How could Otto's weak 12 ADC observation qualify?

Otto CCW, 18:19:19, window 965.43–967.05 s, W 146 blocks.

- **Shape:** after scaling to height 1, a smooth single bump has the outline of
  a magnet window. Baseline training windows are jagged noise at that scale.
- **Size:** log(12.4 / 0.71) = 2.86.

| Class | Where the size feature sits |
|---|---|
| Magnet class (E) | 98 % of its weight at 5.4–5.6, spread about 0.2 |
| Non-magnet class (N), baseline components | 0.6–1.8 |
| N, partial-magnet and straddling components | 4.6–5.2 |

  The bump sits in a region **neither class was trained on**.
- **Decision:** with no training data there, the ratio is decided by the tails
  of the two mixtures, and E's tail (helped by the magnet-like outline) was
  slightly larger: **LLR 3.4 > 0, so accepted**. Real magnets that night scored
  37.9–43.
- **Why small bumps were never non-magnet examples.** The non-magnet training
  windows were drawn at random but **excluded any window whose core overlapped
  an annotated encounter of any kind**, including the 198 short `UNCERTAIN`
  instabilities and the 8 `UNLABELED` activities of Oct 15. Small excursions
  therefore appear in neither class. The baselines of Oct 15 (Otto, one boot)
  were otherwise quiet.
- **Its surroundings were not used.** The real MM101 began 2.3 s after the
  window ended, outside its acceptance wait. No mechanism weighs a candidate
  against what follows it.

## Intended against actual

| Intended | Actual |
|---|---|
| Recognize the continuous MM₁ → BL → MM₂ pattern in the tracing as a whole | Classify isolated windows, magnet or not, each against the line of the baseline before it. The baseline after a candidate is not part of the judgement. |
| An ensemble is its excursion, lobes, shoulders **and return to baseline** | An ensemble is the middle half of the best-scoring fixed-width window. The return to baseline is present only as far as the window's trailing quarter covers it, and only as learned shape. |
| A baseline is recognized as such, including gentle curves | A baseline is **whatever lies between accepted windows**. It is never itself recognized. Its curves matter only through the least-squares line. |
| A big bump on a flat line: big **relative to the line** | Size is relative to 20 ms step noise, not to the line's slow variation. A smooth 12 ADC wander is 17 × the step noise. |
| Learn magnets as a class from all genuine ensembles | Learned from Otto's Oct 15 encounters only (one boot), with extents drawn from an ADC-only annotation. |
| Learn the full variety of baseline | Learned from random quiet windows of one boot. Small excursions were excluded from training. |

## Would broader training alone fix it?

**Broader training is necessary but probably not sufficient.**
- **It would help:** real small bumps, wander, drift, deceleration tails, stall
  drift, Toby's noisier, motor-shifted baseline, and all magnet types and
  speeds would populate the empty region. A 12 ADC bump would then sit inside
  the baseline class, not between the classes.
- **What it cannot change:**
  1. **There is no "unlike anything known" outcome.** LLR > 0 asks only which
     class is less unlikely. Wherever training is thin, and at about 1,000
     windows a second some window always finds such a place, the decision is
     arbitrary. The equal-prior threshold also ignores that magnets occupy a
     small fraction of windows.
  2. **The size reference is the wrong one for the brush.** Step noise is not
     the flatness of the line. Your eye judges a bump against the line's own
     slow variation and against magnets in general; the model judges it
     against 20 ms jitter.
  3. **The pattern is not judged as a pattern.** An ensemble is never required
     to be followed by baseline (MM → BL). That is half of the MM₁ → BL → MM₂
     unit, and it would have counted against the 12 ADC bump, whose "return"
     was simply the same flat line.
  4. **Boundaries come from a window grid,** not from the structure, so the
     ensemble's extent and its baseline context are approximate by
     construction.

## Recommendations for discussion (none implemented)

1. **Broaden the training data as you described.** Use the whole population of
   genuine ensembles (all magnet types, speeds, stops, Otto and Toby) and the
   full variety of real baseline, including the small excursions previously
   excluded. This is needed in any case.
2. **Decide whether the recognizer should judge the pattern MM → BL, not the
   window alone.** That means scoring a candidate together with the baseline
   that follows it, so a structure counts as an ensemble only when the line
   returns to baseline behaviour around it. This is the architectural change
   that most directly matches the original intention.
3. **Reconsider the size reference.** Measure amplitude against the line's own
   slow variation (its wander at the window's scale), not 20 ms step noise.
   This is a feature definition, still statistical and relational to the line,
   not a magnet gate.
4. **Reconsider the decision threshold.** Accept on a calibrated confidence that
   reflects how rare magnets are among the windows tested, instead of LLR > 0.
   Or make "resembles neither class" an explicit outcome (not a magnet).

**Suggested order:** (1) and (2) together, tested offline across all
recordings, with every recording left out of its own training. (3) and (4) only
if the evidence still shows weak false ensembles.
