# NAVI_TRACER_0_2 — Experiment 2: observation-window duration, persistence removed

**Status: offline research prototype only. Not firmware. No navigation
decision. No thresholds or mechanisms authorized as final.** Per Sam and
David's request: isolate the effect of observation-window duration alone,
with the persistence (consecutive-agreement) filter from Experiment 1
removed, ΔBIC threshold unchanged at 10.

## Bug fix carried into this pass

The detection-latency measurement from Experiment 1 was wrong, not just
unreliable — found and fixed before running this experiment. It scanned a
range that extended *past* the evaluator's detected onset and kept
overwriting its answer with the *most recent* qualifying quiet run. For a
short excursion, that meant it was finding the quiet period *after* the
excursion recovered (which sits close to the same background level) and
reporting that as the "onset" — producing the suspicious, uniform ~-980ms
results reported last time. Fixed version: never looks at or past the
detected point; establishes background from a short window strictly
before a bounded search range, then finds the first 2-consecutive-block
departure (same persistence convention already used for the 70-ADC
criterion elsewhere in the project). Code and exact diff are in
`experiment_1_code/evaluator_experiment.py` (same file, used for both
experiments) — see the `independent_true_onset` docstring there.

## Window durations actually used

Requested: 150/300/450/600/900 ms. At 20ms blocks, 150ms and 450ms aren't
exact multiples; also, the ΔBIC implementation itself refuses windows
under 9 samples (returns -∞, never fires) — 150ms (7.5 blocks) is below
that floor entirely. Substitutions, stated plainly rather than silently
rounded:

| Requested | Used | Blocks |
|---:|---:|---:|
| 150ms | **180ms** (closest feasible — 9 blocks is ΔBIC's own minimum) | 9 |
| 300ms | 300ms | 15 |
| 450ms | **440ms** (450 isn't a multiple of 20ms; split the difference) | 22 |
| 600ms | 600ms | 30 |
| 900ms | 900ms | 45 |

## Result 1 — PWM~90 (22 real magnets, known from telemetry)

| Window | Total detected | "Real" (\|peak departure\| ≥ 100 ADC) | Extra/spurious | Gap between real detections (ms) |
|---:|---:|---:|---:|---|
| 180ms | 76 | 17 | 59 | 1180–4300 |
| 300ms | 38 | 20 | 18 | 1180–3100 |
| 440ms | 48 | 16 | 32 | 1180–4800 |
| 600ms | 55 | **3** | 52 | 2540–26680 |
| 900ms | 80 | **1** | 79 | — |

No window in this set recovers all 22 cleanly. 300ms comes closest (20 of
22 at real amplitude). Past 440ms the evaluator doesn't just fragment —
it effectively stops closing: see `exp2_pwm90_900ms.png`, where the
CURVE-state shading is nearly continuous across the whole 25-second
stretch. This isn't adjacent magnets cleanly merging into single
double-peaked detections — it's the evaluator losing the ability to
return to LINE between consecutive real magnets at all, because the
window is now wide enough (900ms) to be comparable to or larger than the
actual 1.1–1.5s gap between magnets at this speed, so it keeps re-finding
curve evidence before ever clearing the trailing window. Compare
`exp2_pwm90_300ms.png` (clean, each magnet separately bracketed) against
`exp2_pwm90_900ms.png` (near-continuous red) directly.

Background medians, where measurable, stay tight regardless of window —
1954–1957 ADC, std 0.12–0.88 — so when the evaluator *does* close
correctly, the median itself is fine. The problem is entirely about
closing, not about the measurement once closed.

## Result 2 — MM044 fragmentation, PWM~35-60

| Window | Pieces overlapping MM044's real span |
|---:|---:|
| 180ms | 13 |
| 300ms | 4 |
| 440ms | 2 (messy — one piece has no valid peak departure) |
| **600ms** | **1 — clean, peak departure 168.9 ADC** |
| **900ms** | **1 — clean, peak departure 168.9 ADC** |

This is the mirror image of Result 1. `exp2_mm044_600ms.png` shows MM046
and MM044 each correctly bracketed as a single clean excursion, nothing
left over, backgrounds correctly measured on both sides (median 1904 for
each).

## What this answers, directly

Sam's three framed questions:

1. **Does a longer window reduce fragmentation?** Yes, demonstrably, for
   MM044 — 600–900ms gives a clean single detection where 180–440ms
   fragments it into 2–13 pieces.
2. **Does the same window work at both speeds?** No. There is no overlap
   in the range tested: the window that fixes MM044 (600–900ms) is
   exactly the range that breaks PWM-90 (3 and 1 real detections out of
   22, respectively). The window that works best for PWM-90 (300ms)
   still leaves MM044 split into 4 pieces.
3. **Is local line-vs-curve classification fundamentally insufficient?**
   Not established as fundamental — what's established is that *a single
   fixed window size* is insufficient across this operating range. That's
   a narrower, more precise claim. Whether the method works with
   something window-duration-dependent on speed, or something else
   entirely, isn't answered by this experiment and wasn't in its scope.

## What this experiment did not do

- No adaptive windows, no new smoothing, no persistence — exactly as
  requested.
- No firmware.
- No attempt to pick a "winning" window — the data doesn't support one;
  reporting that honestly rather than picking the least-bad compromise.
- Did not investigate *why* the crossover happens between 440ms and
  600ms specifically, or whether it tracks PWM/speed in a predictable
  way — that would need more recordings across intermediate speeds.

## Files

- `exp2_pwm90_300ms.png`, `exp2_pwm90_900ms.png` — PWM-90 at the best and
  worst tested windows.
- `exp2_mm044_300ms.png`, `exp2_mm044_600ms.png` — MM044/MM046 region at
  the worst and best tested windows for that case.
- `experiment_1_code/evaluator_experiment.py` — same file as Experiment
  1, with the latency-measurement fix; `persistence` parameter set to 1
  (no smoothing) for every run in this experiment.

Bringing this back for joint review before proposing anything further.
