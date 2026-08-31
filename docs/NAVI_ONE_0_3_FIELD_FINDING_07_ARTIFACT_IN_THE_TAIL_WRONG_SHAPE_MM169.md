# NAVI_ONE — Field finding 07

## A tail artifact inflated the peak and broke the shape fit at MM169

**Date:** 2026-08-31, 15:16:44
**Locomotive:** Toby (9950012), **NAVI_ONE 0.4**, CW, AUTO
**Status:** Observed. Not a decision. Nothing here has been ratified.
**Significance:** first naturally occurring `WRONG_SHAPE` rejection captured with
its waveform. It is the same single-sample artifact class as findings 05 and 06,
landing in a different place and therefore producing a different symptom.

---

## Sequence

| time | event | physical | reading |
|------|-------|----------|---------|
| 15:16:42.352 | AGREE, ADVANCED → mm 168 | MM168 (N) | obs N, peak 225 |
| 15:16:43.126 | **NOT_A_MAGNET / WRONG_SHAPE** | **MM169** | peak 313, ratio 1.6828, resid 0.1307 |
| 15:16:44.419 | DISAGREE / WRONG_MAGNET / POLARITY_MISMATCH | MM170 (S) | obs S vs lagged expectation of MM169 (N) |

One passage of lag. The strike came immediately, so the rejected passage sat at
slot 1 of the dump — comfortably inside the six-passage window.

This happened on **0.4**. The polarity change is not implicated and would not
have prevented it: nothing in decision 0064 touches `peakCounts` or the shape
fit, and 0.3 would have produced the identical rejection.

---

## The waveform

The passage body is an ordinary, well-formed bell rising to **185** and decaying
normally. At sample 165 of 175 — 94% through the passage, in the tail:

```
+38 +38 +38 +28 +24 +24 +20 +17 | +313 | +21 +19 +15 +15 +12 +8 +8
```

One sample, **294 counts off the mean of its neighbours**. A field cannot do
that in two milliseconds. It is the same artifact as the +41 at MM70 and the
-43 at MM119, an order of magnitude larger.

### Why one sample in the tail failed the shape test

Two separate mechanisms, both downstream of `peakCounts` being a raw running
maximum (`HallCapture.h`, `if (d > maxPos_) maxPos_ = d`):

1. **Amplitude was misreported.** `peakCounts` became 313 rather than the body's
   185, so `amplitudeRatio` read **1.6828** against a neighbourhood of 0.99–1.28.
   The passage was recorded as far stronger than it was.
2. **The shape test was fed a false peak.** `fitResidual`
   (`MagnetRecognizer.h:167-222`) takes its arc threshold as 20% of the peak —
   here 62.6 rather than 37 — and then extends the window through every positive
   sample. The fit therefore spans the whole bell while carrying a 313-count
   spike ten samples from the end, where the fitted Gaussian is essentially
   zero. That single squared error took the normalised residual to **0.1307**.

The ceiling is 0.13. It failed by **0.0007**.

---

## What this does and does not establish

**Establishes:** a single-sample artifact can produce a `WRONG_SHAPE` rejection,
and the resulting silent lag, without anything being wrong with the magnet. The
symptom depends on where the artifact lands:

| lands at | consequence |
|---|---|
| the entry crossing | pole latched wrong, peak = artifact -> `TOO_WEAK` (findings 05, 06 — fixed in 0.4) |
| the body or tail | peak inflated, shape fit corrupted -> `WRONG_SHAPE` (this finding — not addressed) |

**Does not establish** that this is what happened at MM110 (finding 02, residual
0.1422) or MM146 (finding 03, 0.1811). At MM110 the peak was 179 and the ratio
0.856 — both unremarkable — so if an artifact was involved there it did *not*
become the peak, and the arithmetic of a mid-arc artifact that leaves the peak
untouched is different. This is now a strong candidate explanation for those two,
and no more than that.

**Does not identify the source.** Still unknown, exactly as in findings 05 and 06.
Today's observed artifact magnitudes now span roughly 40 to 294 counts.

---

## An observation, not a proposal

The operator's ruling behind decision 0064 was that one isolated sample must not
determine the polarity of an entire passage. `peakCounts` is a raw single-sample
maximum, so one isolated sample determines the passage's *amplitude* — and,
through the 20%-of-peak threshold, the shape test's own arc as well.

The same principle appears to apply to a second field. Nothing is proposed here,
nothing is decided, and no threshold has been touched. Recording it so the
question is asked deliberately rather than discovered again.

## References

- `docs/NAVI_ONE_0_3_FIELD_FINDING_05_IMPULSE_FLIPPED_POLARITY_AT_MM70.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_06_SECOND_ENTRY_IMPULSE_MM119.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_02_SHAPE_REJECTION_LAG_STOP.md` (MM110)
- `docs/NAVI_ONE_0_3_FIELD_FINDING_03_SECOND_SHAPE_REJECTION_MM146.md`
- `docs/decisions/0064-polarity-is-the-sign-of-the-summed-passage.md`
- `~/ngr-telemetry/waveforms/waveform_20260831T151644_465_slot*.csv`
