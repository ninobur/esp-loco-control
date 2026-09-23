# 0075 — A rising reading is not a false start

**Date:** 2026-09-03, evening
**Status:** PROPOSED. Not authoritative until the operator reviews and approves it.
**Touches:** 0070 (the discard branch was added under it, X6/X7, for a passage
that opens while stationary); 0065 and 0071 (the recording is never altered:
this record is about what gets recorded, not how it is judged).
**Separate from:** 0074. The operator ruled on 2026-09-03 that the acquisition
defect is fixed first, alone, and that whether the Gaussian residual keeps
operational authority is decided afterwards, on clean acquisition.
**Evidence:** `docs/NAVI_ONE_DISCARD_FLANK_FIX_20260903.md`; the six Arches CW
departures of the `NAVI_ONE_STATION_CURVES_0_1` run, verbatim in
`firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/fixtures_arches_departures.h`.
**Builds:** NAVI_ONE 1.0X12 "Whole flank" and NAVI_ONE_STATION_CURVES 0.2, compiled.
**Field:** the condition flew inside recorder 0.3 on 2026-09-04: sixteen Arches CW departures, pre-roll-to-flank steps of −2 to 6 counts (yesterday 35 to 103), residuals 0.072 to 0.115, all accepted, no shutdown (`docs/NAVI_ONE_X13_FIELD_VERDICT_20260904.md`). Still PROPOSED.

---

## The decision proposed

The capture may discard a just-opened passage as a false start only while the
reading itself is below the entry threshold. A reading at or above the
threshold now was not opened by an excursion that has since gone, whatever a
25 ms-old plateau verdict says about the field.

In code, one added term in the discard guard of `HallCapture::sample()`:
`mag < cfg_.entryMargin &&`. Nothing else changes: not the entry or exit
margins, not the plateau window, not the ceiling, not the archaeology, not the
stop-episode rule, not the stations.

## What forced it

The recorder run of 2026-09-03 published every passage, and every Arches CW
departure it captured had lost the beginning of its rising flank: 35, 62, 74,
66, 46 and 103 counts of step between the pre-roll and the first kept sample,
on flanks that rise 2.5 to 3.75 counts a millisecond. The residual tracked the
hole: 0.1009 to 0.1269 accepted, 0.1372 refused, and the refusal stopped the
locomotive at MM110 with MM111 correctly expected and correctly poled.

The operator's reading of that evidence, recorded here because it set the
scope of the fix:

1. The defect is repeatable and lives in the recorder as well as X11.
2. Nearly every equivalent Arches departure loses some beginning of its flank.
3. The stale plateau and the discard timing determine how much is lost.
4. The Gaussian crosses its threshold only when the amputation is severe.
5. Making shape advisory would have prevented this shutdown and masked the
   corrupted acquisition instead of repairing it.

## The mechanism

The discard branch (X6, guarded in X7 after Arches CCW 14:14:40) throws away a
passage that opened while a stop was armed if the field reads flat and the
passage has shown no progress, provided the flat level is below the entry
threshold. The flatness verdict is computed once every 25 ms from sixteen
samples with the extremes trimmed. When a real flank begins, its first rising
sample is the trimmed one, so the verdict stays "flat" for up to two refresh
periods. In that interval the branch fires on every sample, the passage reopens
on the next, and the pre-roll ring, which fills only while no passage is open,
does not move. The record that survives begins mid-flank on stale pre-roll.

Gate 14 reproduces this on the unfixed code: 14 to 44 samples discarded per
flank, steps of 15 to 166 counts, the loss equal to the stale interval times
the slope.

## What was proven, and what was not

Proven in the harness (`tests/gate_flank_discard.cpp`, gate 14, and gates 1
to 13 unchanged):

- Four synthetic flanks from 1 to 5 counts/ms, opening while the verdict is
  stale: zero discards, opened on the crossing, pre-roll joined to flank with
  a step no larger than the slope.
- The six field records fed as published reproduce their field residuals to
  four places; fed with the missing flank restored at the record's own slope
  they read 0.073 to 0.085, all accepted, no discontinuity.
- The branch still discards what it was built for: a one-sample artifact in a
  30-count fringe (Bamboo), an excursion that rose and settled back below the
  threshold. Resting inside a real field at 88 stays open through the dwell,
  the known fault, unchanged.

Not proven: anything on the railway. Steps 5 and 6 of the operator's list,
repeated Arches CW departures with continuous published flanks and zero
discards, are his to run. The restored flanks in the harness are straight
lines; the real ones curve.

## The unintended consequences to name now

- **Records get longer and residuals get lower on every stop-adjacent
  passage.** Comparisons across this build boundary carry the build name, as
  0072 requires. The 0.13 ceiling was tuned on records that may include
  amputated ones; its margin at stations will look wider after this fix
  without anything about the magnets having changed.
- **A passage that opens on a true false start and keeps reading above the
  threshold is no longer discarded.** By construction that is a field, not a
  false start: something is holding the reading above 38 counts. It goes the
  way a stationary in-field passage goes today, which is the bounded known
  fault already named in the code. This fix does not make that fault more
  likely; it stops the branch from misfiring on flanks.
- **If amputation recurs with this condition in place, the cause is not this
  branch.** The operator's instruction is then to stop adding rescue layers
  and reconsider the capture architecture, not to add another guard.

## Attribution

The operator identified the defect's repeatability from his own run, set the
two-decision separation, the scope ("only the discard condition") and the
six-step proof. The single condition, the gate's design, and the choice to
fly the recorder rather than X12 for the field proof are an agent's; none is
his ruling until he says so.
