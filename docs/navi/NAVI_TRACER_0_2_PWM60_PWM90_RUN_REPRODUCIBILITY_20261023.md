# NAVI_TRACER_0_2 — Relative-speed reproducibility at PWM 60 and PWM 90, individual runs preserved

**Status: review only. No evaluator experiment run. No firmware. No
mechanism proposed or authorized.** Answers the narrowed question: at a
constant commanded PWM of 60 or 90, are segment-speed ratios reproducible
from one run to another in the same direction?

## Correction to the previous deliverable first

While building this analysis I found a real parsing bug in the script
used for the prior calibration-corpus review
(`NAVI_TRACER_0_2_CAL_CORPUS_REVIEW_20261023.md`): eight markers carry a
landmark name in the raw text (`MM063 Grillers`, `MM072 Westpoint`,
etc. — `Southpoint`, `Patio`, `Grillers`, `Westpoint`, `Northpoint`,
`Arches`, `Eastpoint`, `Bamboo`, per `RouteMap.h`'s own `landmarkAt`).
The previous parser's regex didn't tolerate the extra token and silently
dropped every landmark-marker line — including **MM063, which sits
inside the MM060-079 section this whole investigation is about.** Worse,
every run that passed through a landmark marker was artificially split
into separate "runs" at exactly that point, which is why Experiment 4's
analysis never found a single run spanning both reference sections.

Fixed and re-run. Corrected counts: 10,503 raw lines (was 10,014), 9,063
passages survive the same documented filter (was 8,225). The
reference-compatibility numbers shift slightly (PWM≥70 median ratio
0.914, range 0.838–1.118, n=7 — materially the same conclusion as
before: Codex's tight-compatibility claim still doesn't reproduce). The
qualitative MM060-079 directional finding is unchanged. I'm flagging this
plainly rather than quietly patching it — the earlier deliverable's
numbers should be read as superseded by this correction, not relied on
as published.

## Method, exactly as requested

For every one of the 16 cal files, walked records in file order and
built **maximal contiguous runs** where every record satisfies, without
exception:

- `pos == EXACT_LOCKED`, `obs == expected` (trustworthy, confirmed read)
- `timing_mode == MOVING` (not `SPEED_TRANSITION`, not
  `STOPPED_INFERRED` — excludes acceleration/deceleration and stops)
- `effective_pwm_avg` **exactly** 60 or **exactly** 90 — no bands, and a
  run ends the instant the recorded PWM changes to any other value,
  including 59 or 61
- `seg_mm` 250–360 (adjacent single-interval passage)
- MM progression adjacent to the previous record (+1/-1 with wrap) —
  any break in strict adjacency ends the run

No band substitution, no averaging across runs before comparing them,
no relaxed criteria to raise the sample size.

## 1. Qualifying runs and passages

| Loco | PWM | Direction | Runs (≥3 records) | Total records | Run lengths |
|---|---:|---|---:|---:|---|
| 9950011 | 60 | CCW | 2 | 165 | 51, 114 |
| 9950011 | 60 | CW | 2 | 195 | 183, 12 |
| 9950011 | 90 | CCW | 6 | 450 | 26, 149, 28, 149, 90, 8 |
| 9950011 | 90 | CW | **0** | 0 | — |
| 9950012 | 60 | any | **0** | 0 | — |
| 9950012 | 90 | any | **0** | 0 | — |

**Toby (9950012) has zero qualifying runs at exactly PWM 60 or 90 under
these criteria.** Every Toby passage in the corpus sits at some other
constant PWM or inside a transition. **PWM 90 CW has zero qualifying
runs for either locomotive.** Both are real, reportable gaps, not
estimated around.

## 2. Individual runs, segment speeds and section ratios — not combined

Two sections probed: MM035-055 (candidate reference) and MM060-079 (the
section already shown to carry the strongest directional effect). A run
only yields a ratio if it covers *both* sections itself, unbroken.

| Loco | PWM | Dir | File | MM range | n | ref MM035-055 (n, median mm/s) | seg MM060-079 (n, median mm/s) | **ratio** |
|---|---:|---|---|---|---:|---|---|---:|
| 9950011 | 90 | CCW | `cal_..._112659.txt` | 22–170 | 149 | 21, 241.2 | 20, 271.4 | **1.125** |
| 9950011 | 90 | CCW | `cal_..._113040.txt` | 0–170 | 149 | 21, 250.0 | 20, 279.8 | **1.119** |
| 9950011 | 60 | CW | `cal_..._135528.txt` | 0–170 | 183 | 21, 163.9 | 20, 111.3 | **0.679** |
| 9950011 | 90 | CCW | `cal_..._113040.txt` | 58–147 | 90 | 0 | 20, 286.0 | — (no ref coverage) |
| 9950011 | 90 | CCW | `cal_..._113040.txt` | 36–43 | 8 | 8, 250.6 | 0 | — (no seg coverage) |
| 9950011 | 60 | CCW | `cal_..._113040.txt` | 72–122 | 51 | 0 | 8, 180.7 | — (no ref coverage) |
| 9950011 | 60 | CCW | `cal_..._113040.txt` | 0–170 | 114 | 21, 139.2 | 0 | — (no seg coverage) |

Full per-segment speed list for every run (not just the medians above) is
preserved in `experiment_1_code/exp5_runs.json`.

## 3. The central question, answered directly

**At PWM 90, CCW: yes, reproducible, from the only two runs that qualify.**
Ratio 1.125 and 1.119 — a 0.6% difference between two independent
passes recorded about 4 minutes apart (`112659` and `113040`). That's
tight enough to call the effect real and stable across these two runs,
not noise.

**At PWM 60, CW: not yet testable for reproducibility.** Only one run
(0.679) covers both sections unbroken. There is nothing to compare it
against. The direction of the effect (MM060-079 slower than the
reference) is consistent with the broader finding from the previous
review, but "reproducible" can't be claimed from n=1.

**At PWM 60, CCW and PWM 90, CW: the question can't be answered at all
with this corpus.** Zero qualifying runs cover both sections (PWM 60
CCW has two runs that each cover only one section), and PWM 90 CW has no
qualifying runs whatsoever.

## 4. PWM 60 vs. PWM 90 comparison

Only one direction (CW for comparing magnitude isn't possible — the only
PWM 60 ratio is CW, the only reproducible PWM 90 ratios are CCW, so a
direct same-direction PWM 60-vs-90 comparison isn't available from this
corpus either. What can be said: at PWM 90/CCW the section runs ~12%
*faster* than the reference; at PWM 60/CW it runs ~32% *slower*. Opposite
sign is expected (matches the established CW-slow/CCW-fast pattern), but
comparing magnitude across PWM at the *same* direction isn't possible
here — another real gap.

## 5. Which sections are stable, using the one pair that allows a real check

The two reproducible PWM 90/CCW runs are long enough (149 records each,
most of a full lap) to check more than one section. Ratios computed
against each run's own MM020-059 median (controls for the ~4% overall
pace difference between the two runs):

| Section | Run 1 ratio | Run 2 ratio | Spread |
|---|---:|---:|---:|
| MM000-019 | 0.965 | *(no coverage)* | — |
| MM020-059 (baseline) | 1.000 | 1.000 | 0.000 |
| **MM060-079** | 1.183 | 1.171 | **0.012** |
| MM080-109 | 0.985 | 0.989 | 0.004 |
| MM110-129 | 1.030 | 1.009 | 0.021 |
| MM130-149 | 0.915 | 0.909 | 0.006 |
| MM150-170 | 0.905 | 0.941 | 0.036 |

By this (admittedly n=2) check, MM080-109 and MM130-149 are the most
stable sections, MM060-079 is tightly reproducible and large in
magnitude (the most *useful* section, not just the most stable), and
MM150-170 shows the largest run-to-run spread of the sections with
coverage in both runs — worth more data before trusting it as tightly as
the others.

## 6. Gaps, stated plainly

- No PWM 90 CW data at all, either locomotive.
- No PWM 60 CCW run covering both reference sections in one unbroken
  pass.
- No PWM 60 reproducibility check possible (n=1 for the only complete
  run).
- No PWM 60-vs-90 comparison in the same direction.
- Zero Toby (9950012) data at exactly PWM 60 or 90 under these strict
  criteria — the broader historical coverage noted in the prior review
  (50–69, 70–99 bands) does not include an exact-60 or exact-90 run.
- Everything here is Otto (9950011) only, one locomotive, mostly one
  short calibration session (the two usable PWM 90 runs are ~4 minutes
  apart, same day, likely the same consist) — this is evidence that the
  *effect* is reproducible within one session, not yet evidence that it's
  stable across different days, different consists, or different
  equipment wear.

## What this does not do

No PWM band substituted for 60 or 90. No cross-run averaging before
reproducibility was checked. No evaluator experiment. No firmware. No
mechanism proposed — this answers the empirical reproducibility question
only, per the stated purpose.
