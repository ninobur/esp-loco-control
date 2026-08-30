# 0062 — The shape test abstains on a railed passage, and the data says keep it that way

**Status:** Accepted
**Date:** 2026-08-30

## Context

The 0.1 review's Finding 5 said the shape test — the recognizer's
second-strongest instrument — disappears through three doors, and that the
abstention is asymmetric in the permissive direction: a clean but unfittable
waveform is *refused* (`NoCurve`), while a truncated or clipped one is
*excused*. Its composite worst case: a railed transient longer than 40 ms with
a high peak passes amplitude, abstains shape, passes the guard, and is
accepted — leaving one polarity bit as the entire defence.

Two of the three doors were defects and are closed:

- **`clipped_` was contaminated from outside the passage.** It was set by
  `updateBaseline()` at any moment and cleared only when a passage closed, so a
  supply transient minutes earlier excused the shape test on the next magnet.
  It is now cleared when a passage opens: it means *this passage railed*.
- **Any passage over 512 samples was truncated**, so at crawl speed — a station
  approach, or the 18.7 s dwell of decision 0057 — the shape test was
  systematically absent. The buffer now **decimates** instead: when it fills,
  the whole arc is halved in place and the sample rate halves with it. The
  Gaussian fit derives its own centre and sigma from the data in sample-index
  units, so scaling x by a constant leaves the normalised residual unchanged.
  Gate 4 replays a four-second passage and confirms the fit still reads 0.13.

The third door is the question this record answers.

## The measurement

Should a passage that railed during its own arc be refused rather than excused?
And should there be an upper bound on amplitude ratio, so a large electrical
event cannot pass on amplitude alone?

Both were measured against the 2026-08-28 survey, 351 waveforms:

| | ratio min | ratio max |
|---|---:|---:|
| 195 real primaries | 0.395 | **2.621** |
| 156 non-primaries | 0.076 | 0.274 |

There is a wide gap below — which is where the 0.34 floor sits, measured. There
is **no gap above**. The highest-amplitude record in the whole survey, ratio
2.621, is a real magnet. It is also one of the two primaries that railed.

## Decision

1. **No upper bound on amplitude ratio.** The data does not support one. Any
   ceiling low enough to be useful would refuse a real magnet, and this project
   sets thresholds at the midpoint of measured gaps or not at all.
2. **A clipped passage still abstains from the shape test.** Refusing them
   would have refused 2 of 195 real magnets on the survey — including the one
   at ratio 2.621. Under decision 0059 a missed magnet can hide for up to six
   markers, so refusing real magnets is not the safe direction; it is a
   different failure with a longer tail.
3. **The exposure is stated instead.** With the first two doors closed,
   `clipped` now means the ADC railed *during this passage*, which on this
   sensor is rare and is an instrument event. When it happens the passage is
   examined on amplitude and the rebound guard, and identity then rests on the
   polarity bit — the same one bit every other magnet rests on.

## Why this is recorded rather than fixed

Because the next reviewer will raise it again, and should find the measurement
rather than repeat the argument. The asymmetry is real. It is also the cheaper
of the two errors, on this railway, on this evidence.

## References

- `firmware/test-programs/NAVI_ONE/HallCapture.h`, `MagnetRecognizer.h`
- `firmware/test-programs/NAVI_ONE/tests/gate_ops.cpp` — C2, C3
- `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md` — Finding 5
- decisions 0052, 0057, 0059
