# NAVI_ONE 0.3 — Field finding 05

## A single-sample impulse latched the wrong pole at MM70; the "weak magnet" was full strength

**Date:** 2026-08-31, 10:46:22
**Locomotive:** Toby (9950012), NAVI_ONE 0.3, CCW, AUTO
**Sketch:** `agent/toby-1-13-flash` @ `84b1730`
**Status:** Observed. Not a decision. Nothing here has been ratified.
**Significance:** first natural non-recognition event captured with its waveform,
and the waveform contradicts every scalar the firmware published about it.

---

## What the published scalars said

| time | event | navigator | reading |
|------|-------|-----------|---------|
| 10:46:20.706 | AGREE, ADVANCED | mm 71, tgt 70 | obs N, peak 171 |
| 10:46:21.677 | **NOT_A_MAGNET / TOO_WEAK** | mm 71 (did not advance) | **peak 41, ratio 0.1925** |
| 10:46:22.622 | DISAGREE / WRONG_MAGNET / POLARITY_MISMATCH | STRUCK | — |

Read as scalars this is unremarkable: a marker gave a weak reading, was rejected
as non-primary, the navigator lagged one marker, and the next magnet's polarity
exposed the lag and struck. `ROUTE_POLARITY` confirms the geometry exactly —
MM71 N, **MM70 S**, MM69 N — so the strike is the navigator comparing physical
MM69 (N) against its lagged expectation of MM70 (S).

The obvious conclusion is that MM70 is a weak or badly seated magnet.

**That conclusion is wrong.**

---

## What the waveform said

The `withdraw()` dump put the rejected passage in slot 1, one back from the
strike. Its samples:

```
0 -4 -3 -6 -6 -6 -7 -7 41 -10 -4 -11 -13 -16 -23 -23 -76 -23 -24 -26 ...
... -178 -180 -183 -180 -178 -170 -178 -180 -178 ...
... -32 -25 -23 -23 -19 -17 -17 -12 -10 -10
```

That is a clean, well-formed, full-amplitude bell curve reaching **−183**.

`WaveformWindow` stores *oriented* samples — the capture negates them when it
believes the pole is South, so a correctly-read passage of either pole comes out
positive. Every other slot in this dump confirms that convention, including
slot 3, physical MM72, surveyed S, stored positive:

| slot | marker | reported pol | reported peak | reported ratio | true excursion | true amplitude |
|-----:|--------|-------------:|--------------:|---------------:|----------------|---------------:|
| 0 | MM69 | 1 | 166 | 0.7793 | 1 … 166 | 166 |
| 1 | **MM70** | **1 (N)** | **41** | **0.1925** | **−183 … 41** | **183** |
| 2 | MM71 | 1 | 171 | 0.7917 | 8 … 171 | 171 |
| 3 | MM72 (S) | 0 | 181 | 0.8380 | 8 … 181 | 181 |
| 4 | MM73 | 1 | 177 | 0.8157 | 7 … 177 | 177 |
| 5 | MM74 | 1 | 178 | 0.8203 | 0 … 178 | 178 |

Slot 1 is the only passage in the window stored **negative**. Its true amplitude
is 183 — the largest in the window, and squarely among its neighbours' 166–181.

**MM70 is a perfectly healthy magnet.** It is not weak, not misaligned, and not
degraded. Every scalar the firmware published about that crossing —
`peak`, `ratio`, `polarity`, and the `TOO_WEAK` ruling itself — was an artifact.

---

## The mechanism, confirmed in the code

`HallCapture.h:60-90`. A passage opens on the first sample whose magnitude
exceeds `entryMargin` (38), and the pole is latched from that one sample's sign:

```cpp
if (mag < cfg_.entryMargin) { /* pre-roll */ return false; }
open_ = true;
pol_ = delta >= 0 ? 1 : 0;
```

Peak then tracks only the *oriented* value:

```cpp
const int32_t oriented = pol_ ? delta : -delta;
if (oriented > peak_) peak_ = oriented;
```

So the sequence at MM70 was:

1. A single-sample positive impulse of **+41** arrived as the South field began
   to develop — **three counts** over the `entryMargin` of 38.
2. `pol_` latched to 1 (North) from that one sample.
3. The real South bell followed. Oriented as North it is negative, so it never
   raised `peak_`. `peak_` stayed at the impulse's own 41.
4. `peak 41` against the gain baseline gave `ratio 0.1925`, below the amplitude
   floor (~0.34), so the recognizer ruled `TOO_WEAK` → `NotAMagnet`.
5. `NotAMagnet` does not advance the navigator and does not warn. One-marker lag.
6. MM69's N met the lagged expectation of MM70's S. Strike, stop, dump.

The defect is that **the pole is decided by a single unconfirmed sample**, taken
at the one moment in a passage when the signal is weakest and an impulse is
therefore most likely to dominate it. There is no de-glitch, no confirmation
window, and no majority.

### The channel demonstrably carries impulses of this size

Single-sample deviation from the mean of the two neighbouring samples, largest
three per slot:

| slot | largest impulses (deviation in counts) |
|-----:|----------------------------------------|
| 0 | 60, 31, 29 |
| 1 | 53, 50, 34 |
| 2 | 10, 9, 6 |
| 3 | 10, 9, 7 |
| 4 | 46, 24, 23 |
| 5 | 7, 6, 6 |

Impulses of 46–60 counts are present in three of the six passages in this single
window. This is not a rare event; MM70 was simply the one where an impulse
landed at the passage boundary, where it decides the pole, rather than in the
body, where it does not.

---

## Relation to findings 02 and 03 — NOT closed by this

It is tempting to extend this to the unexplained `WRONG_SHAPE` residual
excursions. The arithmetic does not currently support it.

Slot 4 carries a 46-count impulse *inside* the fitted arc and returned residual
0.0613 — normal. That is consistent: one outlier of magnitude `d` over an arc of
`n` samples at amplitude `A` adds about `d/(A·√n)` in quadrature, which for
46/(177·10) is ≈0.026, taking a ~0.055 baseline to ≈0.061. Observed: 0.0613.

Applying the same model to MM110 (finding 02: residual 0.0868 → 0.1422 at
amplitude ~179) requires an added term of ≈0.118, i.e. a single impulse of very
roughly 200 counts — larger than the signal itself, and implausible.

So MM110 needs either many impulses or a broad distortion, not one spike. **The
cause of the findings 02/03 residual excursions remains unknown.** This finding
is a different failure mode reached through the same silent-lag path.

---

## What this validates

The instrument did exactly what decision 0063 built it to do, on its first
natural event. The scalars said "weak magnet at MM70" and would have sent the
operator to the track with a spanner. The waveform said "full-strength magnet,
wrong pole latched off a 3-count threshold overshoot" — a firmware defect at the
other end of the railway from where the scalars pointed.

No scalar in the published set could have distinguished these. `peak`, `ratio`,
`gap` and `gain` were all internally consistent with a weak magnet.

---

## No change proposed

Candidate remedies exist — confirming the pole over several samples, taking the
pole from the passage body rather than the entry crossing, or median-filtering
the input — and each has consequences for latency and for the recognizer's
existing calibration. **None is proposed here and none has been decided.** This
record exists so the defect is known and argued before anything is changed.

Recognition thresholds remain untouched.

## References

- `firmware/test-programs/NAVI_ONE/HallCapture.h:33,60-90` — `entryMargin`, the
  pole latch, and the oriented-peak update
- `firmware/test-programs/NAVI_ONE/RouteMap.h` — MM69 N, MM70 S, MM71 N
- `docs/NAVI_ONE_0_3_FIELD_FINDING_04_WAVEFORM_CAPTURE_VERIFIED.md` — the
  capture path this depends on
- `~/ngr-telemetry/waveforms/waveform_20260831T104622_662_slot*.csv`
