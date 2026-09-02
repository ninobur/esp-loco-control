# Two-sided interpretation: calibrated on 312 real magnets

**Date:** 2026-09-02
**Locomotive:** Toby (9950012)
**Status:** MEASUREMENT, host-side. No firmware judgement changed. No threshold moved.
**Tools:** `tools/two_sided/` — `extract_passages.py`, `calibrate.cpp`,
`separate.cpp`, `twospeed.cpp`, `consistency.cpp`. All fits are
`TwoSided.h`'s log-parabola, the code the firmware would run.

---

## The operator's brief

> Preserve the existing Gaussian's mathematical meaning and change only how an
> interrupted traversal is represented. Ordinary passage: one Gaussian with one
> temporal width. Interrupted passage: one physical Gaussian field observed in
> two movement intervals, potentially with different temporal widths.

> The ESP should be able to look at the split curve with the same
> sophistication as you or I. Touch the elephant's tail and realize that it
> belongs with the trunk that it encountered before the stop.

The measurements below say what the halves of a real magnet do, which
formulation of "belongs with" is well-conditioned, and whether it separates the
real interrupted crossings of 2026-09-02 from the four adversarial non-magnets.

## The data

Every accepted, uninterrupted magnet passage in the field records — 312 of them:
187 labelled primary magnets from the 2026-08-28 survey at cruise speed, and 125
window-dump slots the recognizer ruled MAGNET across the 08-31, 09-01 and 09-02
telemetry mirrors, at station-approach speeds. Oriented, baseline-relative
counts exactly as recorded; nothing smoothed or reconstructed. All 312 pass the
unchanged whole-passage fit (median 0.071, max 0.111).

## 1. What the halves of a real magnet do (`calibrate.cpp`)

Each passage split at its apex, each half fitted freely.

| | median | p90 | p95 | p99 | max |
|---|---|---|---|---|---|
| amplitude disagreement between halves | **2.1%** | 4.6% | 5.3% | **8.1%** | 22.2% |
| arrival-half residual | 0.010 | 0.015 | 0.018 | 0.023 | 0.026 |
| departure-half residual | 0.016 | 0.022 | 0.025 | 0.032 | 0.033 |
| whole-passage residual (current test) | 0.071 | 0.083 | 0.097 | 0.107 | 0.111 |
| width ratio arrival/departure | 1.10 | — | 1.41 (p95) | — | — |

Two findings beyond the number asked for:

- **Each half is a five-times better Gaussian than the whole.** The current
  recognizer's "normal" residual of ~0.07 is mostly the asymmetry between the
  two halves — even uninterrupted crossings differ in width by 10% median, 41%
  at p95. Different temporal widths are needed for ordinary passages, not only
  interrupted ones.
- The single 22% outlier (peak 144, arrival σ 129 vs departure 55) is a
  station approach decelerating *within* the arrival half. Speed must be roughly
  constant within a half; it is the model's real limitation and it will occur at
  stations.

## 2. The symmetric formulation does not work (`twospeed.cpp`)

Cut each passage at 45% of peak on the rising flank — where Toby stalls — and
replay the departure at 1×, 2×, 3×, 4× the arrival speed. Fit both halves freely
and compare amplitudes.

| departure speed | amplitude disagreement, median | p90 | width ratio |
|---|---|---|---|
| 1× | 45% | 98% | 0.97 |
| 2× | 45% | 98% | 1.94 |
| 4× | 45% | 98% | 3.90 |

The width tracks the speed exactly. The amplitude fails **at 1× as badly as at
4×**: it is not the speed, it is the split. A flank cut at 45% of peak carries
almost no curvature, so a free fit of its amplitude is ill-conditioned. **A tail
on its own cannot say how big the elephant is.** (`separate.cpp` shows the same
on the real Arches records: arrival halves extrapolating to 109, 534, 135
against departures of 135, 133, 173.)

## 3. The formulation that works: the trunk sets the size, the tail must fit it (`consistency.cpp`)

1. The half that **contains the apex** is fitted freely — amplitude A, centre,
   width. That is the trunk. It is well-conditioned because the curvature that
   pins A is in it.
2. The other half is fitted with **A fixed to the trunk's**; only its width and
   centre are free. Its residual against *that* Gaussian is the test.
3. **Ordering:** the tail's centre must lie beyond its own segment, on the
   trunk's side — it never reached the apex. Two centres inside their own
   segments is two lobes.

Same 312 passages, same 45% cut, departure at 1×, 2×, 4×:

| | median | p95 | p99 | max | over 0.13 |
|---|---|---|---|---|---|
| **tail residual against the trunk's amplitude** | **0.008** | 0.013 | 0.018 | **0.018** | **0 of 311** |
| trunk residual (free) | 0.020 | 0.029 | | | |

Identical at every speed ratio; width ratio 0.94 / 1.88 / 3.77. One passage in
312 refused as "two apexes" (a noise bump on a flank — to inspect).

Against a 0.13 ceiling the real tails sit at 0.018 max: **seven times inside**.
No threshold was moved to get this; the existing ceiling is reused unchanged.

### The real interrupted crossings of 2026-09-02, at their joins

| record | current recognizer | two-sided |
|---|---|---|
| A1967_CW (slip, refused 0.1967) | REFUSED | **ACCEPT** — tail 0.017, width ×3.8 |
| A1818_CW (slip, refused 0.1818) | REFUSED | **ACCEPT** — tail 0.015, width ×2.7 |
| A1810_CW (dwell in field, refused 0.1810) | REFUSED | refused — tail apex misplaced (see open items) |
| arrival-only records (12:13, 14:14, 15:03) | REFUSED | refused — no departure to fit |
| A1223_CW (accepted 0.1223) | ACCEPTED | no waveform was ever published |

Two of the three joined records — both of the morning's refusals — are accepted
by a test whose real-magnet distribution tops out at 0.018.

### The four adversarial non-magnets

| artifact | two-sided |
|---|---|
| electrical step, slow leading edge | REFUSED — **no apex in either half** |
| DC ramp to 80 and back | REFUSED — no apex in either half |
| shoulder, two overlapping lobes | REFUSED — no apex in either half |
| double lobe, stopped in the notch | REFUSED — **two apexes** |

**Every artifact is refused on structure, not on a tolerance.** The electrical
step — which three retention attempts today let through at 0.12–0.13 — never
gets as far as a residual: neither of its halves has an apex to fit a trunk to.
The double lobe agrees on amplitude to 0.0% and is refused because it has two.
That is the difference between a looser recognizer and one that understands the
event.

## What this does not settle

- **It needs the departure recorded.** The arrival-only records are refused
  correctly and unrecoverably. Retention and this judgement remain one change.
- **A1810_CW** is refused on the ordering rule: its apex sits at the join and the
  fixed-A tail places its centre a few samples inside. The rule needs a tolerance
  in samples, set from data, before it is right.
- **The trunk's own residual is measured but not yet a rule.** It should be —
  the trunk must itself be a Gaussian within the ceiling (real passages: p95
  0.029). This is what would refuse a two-lobed trunk on principle rather than by
  accident.
- **One false refusal in 312** on the two-apex rule. To inspect.
- **The within-half deceleration limit** (the 22% outlier) is real and will show
  at stations. The tail fit tolerates it far better than the free fit did, but it
  is not measured here.
- Host-side only. No firmware has this judgement; the fixture set and the
  segment boundary are in place for it.

## The database the operator asked for

> One thing that we should be doing is building a database of magnet curves
> with every run, storing it on the 128 GB card in the Pi.

Agreed, and this calibration is the argument: 312 passages took a day's
forensics to assemble, and the single most informative record of the day — the
one stitched arc that *passed* — was never transmitted at all, because the build
publishes a waveform only on refusal or on withdrawal. Three things are needed:

1. **The locomotive publishes every judged passage**, accepted or refused.
   ~40 bytes header + 2 bytes a sample, one to two MQTT messages each; at
   Toby's marker rate that is a few kilobytes a minute.
2. **A Pi-side decoder** that turns `diag/waveform` chunks into one record per
   passage — day, build, marker, direction, verdict, residual, decimation,
   `stitchAt`, samples — in per-run files or SQLite on the card, indexed.
3. **These tools run against the whole history** rather than a hand-assembled
   text file.

This is the operator's Pi and his call; nothing has been built.
