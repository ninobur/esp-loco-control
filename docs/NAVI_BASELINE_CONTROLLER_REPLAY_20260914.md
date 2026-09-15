# Whole-lap baseline controller — replay and candidate sweep, 2026-09-14

**What this is.** Data mining and replay. No firmware is modified, nothing is
committed or pushed, and the rolling one-second median is treated as
measurement only — under X17 it has no navigation authority and nothing here
gives it any.

**Locomotive.** Otto, 9950011, `NAVI_ONE_1_0X17_FIXED_BASELINE_FIELDTEST`,
entry 70, exit 25, floor 82 ms, guard 500 ms, shadow median 41 × 25 ms,
`baseline_adapt_pwm` 24.

Supersedes the replay section of
[`NAVI_BASELINE_LAP_DATASET_20260914.md`](NAVI_BASELINE_LAP_DATASET_20260914.md).
The extraction in that report stands; its lap **anchoring** and its replay
**do not**, and both are corrected here.

---

## 1. Clarifying the previous result

The earlier line was:

> **Warm, N=2:** the cap never binds. 30 laps, tracking error 0.00 at every one.

That statement was true of what it measured and much weaker than it sounded.

**The update equation actually used.** The operative baseline `ref` was a
Python float and was never rounded:

```
want    = estimate_k − ref
applied = want                       if this was the first completed lap
        = clamp(want, ±N)            otherwise
ref     = ref + applied              (float, never rounded)
error   = estimate_k − ref           reported as "tracking error"
```

So:

- **Rounding method:** none. `ref` carried .5 values the firmware could not
  hold. The previous report was silent on this because there was nothing to
  report — a defect, not a choice.
- **Before or after rounding:** the question did not arise; there was no
  rounding. The error was computed after the update.
- **Requested, applied, or residual:** it was the **residual after the update**,
  `estimate_k − ref_after`. Whenever the cap does not bind, `applied == want`
  and that residual is **identically zero by construction**. Reporting 0.00
  thirty times was reporting "the cap did not bind" thirty times in a more
  impressive-looking way.
- It said **nothing** about how far the reference sat from the resting level
  *during* a lap, which is the quantity that decides whether a passage opens
  and closes.

It also gave the first completed lap unclamped authority, which this brief
correctly rejects and which this replay no longer does.

**The same run, done properly** — estimate rounded to an integer, baseline
integral, cap 2, no escape rule:

| lap | estimate | rounded | before | requested | applied | after | residual |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 1950.0 | 1950 | 1949 | +1.0 | +1 | 1950 | 0.0 |
| 6 | 1953.0 | 1953 | 1951 | +2.0 | +2 | 1953 | 0.0 |
| 7 | 1952.0 | 1952 | 1953 | −1.0 | −1 | 1952 | 0.0 |
| 11 | 1951.5 | 1952 | 1951 | +0.5 | +1 | 1952 | **−0.5** |
| 13 | 1952.5 | 1953 | 1952 | +0.5 | +1 | 1953 | **−0.5** |
| 24 | 1953.5 | 1954 | 1953 | +0.5 | +1 | 1954 | **−0.5** |
| 30 | 1956.0 | 1956 | 1955 | +1.0 | +1 | 1956 | 0.0 |

The largest request across all 30 laps is **+2.0**, so a cap of 2 never binds;
the largest residual is **0.5 counts**, and it comes from rounding, not from
the cap. The instantaneous error against the measured resting level over the
same 30 laps runs **−9 to +17 counts**, which is the number that matters and
which the old figure hid completely.

---

## 2. Lap semantics, as required and as implemented

The lap origin is the location supplied by `SET LOCATION` and it holds for the
rest of the boot session.

| event | effect |
|---|---|
| `SET LOCATION` (`state/nav` `DECLARED`) | new origin; **all** controller state reset |
| direction change (`DIRECTION`) | origin unchanged; the lap in progress is **discarded**; counting resumes toward the same origin in the new direction, starting at the next arrival at the origin marker |
| reboot | new session; everything reset |
| rejection, refusal, withdrawal, non-marker activity | no advance, no lap progress |
| complete lap | exactly 171 firmware-accepted advances ending on the origin marker |

**The previous report's anchoring was wrong.** It used the firmware's own `adv`
counter as the anchor. `adv` also resets at a direction change, which silently
re-anchors the lap origin onto wherever the locomotive happened to be at the
reversal. In the warm run that moved the origin from the declared **MM071** to
**MM067** without anything saying so. `adv` is now used only to establish that
an advance was accepted, never as an anchor; `build_advepoch()` keeps the old
cut in `laps_advepoch_compare.csv` purely so the two can be compared.

**The conservative default for a direction change is implemented as specified**
— discard the partial, keep the origin. The alternative, counting the
part-lap already travelled toward the fixed origin in the new direction, is not
implemented: it would count some markers twice and some not at all, and the
brief calls the discard conservative, which it is.

### What the rule produces

```
session                origin  laps  discarded      returning to origin
B20260914T103024       MM040     0   1 (54 adv, end of session)        0
B20260914T114813       MM040     0   1 (1 adv)                         0
B20260914T115155       MM040     1   0                                 0
B20260914T120419 #1    MM040     7   1 (100 adv, DIRECTION_CHANGE)     0
B20260914T120419 #2-4  MM036/36/45  0  2                               0
B20260914T125025       MM071    30   2 (143 adv total)                 4
```

**38 complete laps**, every one exactly 171 accepted advances ending on its own
session origin. The warm run's 30 laps run **12:55:50 → 14:31:01** anchored at
**MM071**; the same 30 circuits cut at MM000 run 12:57:44 → 14:32:53, which is
the window reported earlier. The four "returning to origin" advances in the warm
session are MM068→MM071 after the reversal: they belong to no lap and feed no
estimate.

### Log ambiguities that prevent an exact reconstruction

1. **The warm run's origin is a declaration immediately overridden by a
   reversal.** `SET LOCATION` at 12:55:05 gave **MM071 CCW**. Three advances
   later, at 12:55:28, the direction was changed to CW. Under the required rule
   the origin stays MM071 and the three CCW advances are discarded — which is
   what is implemented — but a reader who expects the run to be anchored where
   it was *driving* from will find MM071 surprising. It is in `origins.csv`.
2. **A `DIRECTION` event reports the dead-reckoned position behind the
   locomotive, not the position it is standing at.** At 12:55:28 it reported
   `mm: 67` while the last accepted advance had been MM068. The implementation
   therefore does not trust that field to decide whether the locomotive is
   standing on the origin; it waits for an accepted marker at the origin. In
   this dataset that distinction never changes a lap boundary, but it would if a
   reversal happened exactly at the origin.
3. **Whether a new `SET LOCATION` should also reset the operative baseline is
   genuinely ambiguous.** The brief says it "resets all lap-controller state",
   and the operative baseline is controller state; but a position declaration
   says nothing about the Hall level, and reverting to a startup value measured
   45 minutes earlier throws away everything learned since. This is a switch,
   `--setloc-resets-baseline`, not an assumption. See §8.
4. **No `SET LOCATION` in this dataset is followed by a completed lap under a
   different origin**, so the reset rule is exercised only in the cold run's
   tail, where the locomotive was being driven by hand between re-declarations.
5. **Position is unavailable while the navigator is UNSET or STRUCK.** `mm` is
   then stale rather than moving, so those samples can feed no per-MM estimate
   and are excluded (439 samples). They are counted separately from a stall.

---

## 3. The estimator, and the reason it is per-MM

`center_permm_median`: take the median of the observations within each route
position, then the median across the 171 positions.

STATUS arrives at 1 Hz **in time, not in distance**. A slow stretch contributes
more samples than a fast one, so an average over samples is weighted by dwell
rather than by geography. Per-MM gives every marker one vote, which is also the
shape of evidence a route-wide estimator would use. Coverage is 168–171 of the
171 positions on every complete lap.

**Two exclusions were added since the previous report**, both aimed at the same
failure: the 41-sample median tracking a *magnet* instead of the resting level.

- `position_not_advancing` — no accepted marker for 3 s. A 130 ms passage cannot
  move a 1 s median, but a locomotive that has stopped crawling and is sitting on
  a magnet holds it under the sensor for seconds and the median follows it. PWM
  does not see this: MM094 at 10:36:08 reads **+185 counts at PWM 44**. Not
  passing a marker does. 85 samples.
- `position_unknown` — navigator UNSET or STRUCK. 439 samples.

Together they take the largest `|shadow − fixed|` in the kept population from
**185 counts down to 46**, and they cost the lap estimates almost nothing: of
the warm run's 30 complete laps every one of 5,705 samples is kept, and the two
new gates remove exactly **one** sample from the cold run's 7 complete laps
(which keep 1,356 of 1,371, the other 14 excluded by rules that already existed).
Every lap estimate is unchanged to the count.

Full accounting over 18,005 STATUS samples: kept 7,669 (42.6%); `not_moving`
8,739; `no_shadow_field` (X16 sessions) 814; `position_unknown` 439;
`pwm_at_or_below_adapt_floor` 213; `position_not_advancing` 85;
`shadow_window_refilling` 46.

---

## 4. The controller model

State: one integer `B`, plus a consecutive-qualifying counter and its sign.

```
at boot              B = startup baseline, counters cleared
at SET LOCATION      counters cleared; B per --setloc-resets-baseline
on a completed lap   only if the lap is VALID:
    est    = per-MM median of that lap's kept observations   (may be fractional)
    delta  = est − B
    qualifies = |delta| >= threshold                          (escape enabled)
    run    = run + 1 if qualifies and sign(delta) == run_sign
           = 1       if qualifies and the sign flipped
           = 0       otherwise
    if escape enabled and run >= persistence:
        target = est                       (--target newest)
               = median(last `persistence` qualifying estimates)   (--target median)
        applied = round(clamp(target − B, ± exceptional_cap))
        run = 0
    else:
        applied = round(clamp(delta, ± normal_cap))
    B = B + applied                                        (integer throughout)
on an incomplete or invalid lap    no adjustment of any kind
```

A lap is **valid** only if it is complete, contains no stop and no navigation
withdrawal, and covers at least 150 of the 171 positions. Incomplete laps are
kept in the dataset and marked; they never produce an adjustment.

**The first completed lap gets no special authority.** The cold run is the
reason: its first completed lap centres on 1905.0, the startup value exactly,
and the warming arrives over the six laps after it. First-lap privilege would
have bought nothing there and would hand full authority to whichever lap
happened to be first.

### Rounding — three options, `estimate` is the default

The operative baseline is an integer; the lap estimate is not (even counts of
observations per position give .5 values, and 3 of the warm run's 30 laps do).

| `--rounding` | arithmetic | +0.5 request |
|---|---|---|
| `estimate` (default) | round the estimate to an integer first, then take an integer difference and clamp it | moves by 1 |
| `correction_nearest` | clamp the fractional difference, then round half **away from zero** | moves by 1 |
| `correction_trunc` | clamp, then truncate toward zero | **moves by 0** |

`correction_trunc` creates a dead zone: every request under one full count is
discarded, so the warm run's three +0.5 laps would never move the baseline and
the error would accumulate until it reached a whole count. Rounding is
half-away-from-zero, not Python's banker's rounding, which on a dataset this
full of exact .5 values would otherwise alternate silently.

Residual error from rounding alone is bounded by 0.5 counts and is observed at
exactly that on 3 of 30 warm laps.

---

## 5. Polarity, and why absolute error is not a safety criterion

Otto's raw level **rises** through a North magnet and **falls** through a South
one. `peak` is published against `entryBaseline_`, which under X17 is the frozen
startup baseline — not the resting level. With `E = resting level − operative
baseline`, the true amplitude `A` is recovered first and the passage is then
re-tested against the candidate:

```
N:  peak = A + E   →  A = peak − E        effective peak under a candidate = A + E'
S:  peak = A − E   →  A = peak + E                                          = A − E'
```

A reference **below** the resting level (E > 0) starves South magnets and
inflates North ones. It is not symmetric, and the two failure modes are opposite
polarities.

### Measured, not assumed

Duration of accepted passages against signed `E`, over 6,807 accepted passages
with a known local resting level:

| E band | South n | median | p10 | min | North n | median | p90 | max |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| +0..+4 | 1707 | 127 | 115 | **102** | 1905 | 131 | 146 | 1333 |
| +5..+9 | 1021 | 125 | 114 | 101 | 846 | 133 | 148 | 205 |
| +10..+14 | 137 | 121 | 110 | 105 | 113 | 145 | 159 | 173 |
| +15..+19 | 114 | 116 | 104 | 96 | 118 | 151 | 169 | 185 |
| +20..+24 | 124 | 109 | 98 | 87 | 143 | 163 | 183 | 202 |
| +25..+29 | 166 | 108 | 95 | **84** | 147 | 166 | 185 | 245 |
| +30..+34 | 42 | 102 | 94 | **85** | 37 | 183 | 240 | **435** |
| +40..+49 | 6 | 152 | 93 | 93 | 3 | 1415 | 1784 | **2542** |

Read down the two halves:

- **South passages shorten** as E rises — median 127 → 102 ms, minimum 84 ms
  against an **82 ms floor**. Both floor-rejected passages in this dataset with
  a known resting level are South, at E = +31 and E = +21.
- **North passages stretch** — median 131 → 183 ms, and past E ≈ +40 they reach
  1,784 ms and 2,542 ms. Those are the latches.

**The nominal 25-count exit margin is not where closing fails.** Raw noise
carries the close well past it: at E = +25..+34 passages still close, with
median durations near normal. The measured latch band begins around **|E| = 40**.
The replay therefore reports the nominal `closing_margin = 25 − |E|` *and* three
measured bands: `south_floor_risk_passages` (South at E ≥ +25),
`north_stretch_risk_passages` (North at E ≥ +30), `passages_in_latch_band`
(|E| ≥ 40).

### Opening is not the binding constraint

The weakest accepted passage in the whole dataset has a true amplitude of **111
counts** against a 70-count entry threshold, so it would take **41 counts** of
error to swallow it outright. No candidate in the sweep produces a single
would-not-open passage. The danger is not a magnet that fails to open; it is a
South magnet that opens weakly, produces a short passage, and is then discarded
by the 82 ms floor — which is exactly the MM140 stop:

> **12:29:48** — a South magnet of true amplitude **130 counts**, read against a
> reference 31 counts adrift, published a peak of 99 and a duration of **79 ms**,
> three milliseconds under the floor. A healthy magnet thrown away by a stale
> reference. Under `cap2/noescape` the reference at that instant would have been
> 1917 instead of 1905, E = +19, effective peak 111 rather than 99; under
> `cap2/P1/T10/X8` it would have been 1927, E = +9, effective peak 121. The band
> table puts both comfortably clear of 82 ms. **Duration is not modelled**, so
> that is an inference from the measured relationship, not a prediction.

---

## 6. Candidate sweep

Grid: normal cap {**0**, 1, 2, 3, 4} × {no-escape control, persistence {1,2,3} ×
threshold {8,10,12,15,20} × exceptional cap {4,6,8,10,∞} × target
{newest, median}} = **755 candidates**. Cap 0 is the control that reproduces X17
as it ships: the baseline never moves.

Safety metrics are taken **inside complete laps**, so observations and passages
describe the same stretch of running.

```
candidate                  | COLD boot 12:04, startup 1905, 7 laps       | WARM 12:50, 30 laps
                           |  lerr exc  mv  s>10  s>15  s>20  Sfl Nst ltch|  mv rev exc lerr
cap0/noescape  (= X17 now) |  26.0   0   0  1053   777   576  158   2   0 |   0   0   0  7.0
cap1/noescape              |  20.0   0   6  1047   685   336    6   0   0 |   9   2   0  1.0
cap2/noescape              |  15.0   0  12  1034   474     7    0   0   0 |  11   2   0  0.5
cap3/noescape              |  10.0   0  18   833    89     0    0   0   0 |  11   2   0  0.5
cap4/noescape              |   7.0   0  24   383    15     0    0   0   0 |  11   2   0  0.5
cap1/P2/T20/X8/newest      |  20.0   1  13  1047   685   336    5   0   0 |   9   2   0  1.0
cap2/P2/T12/X6/median      |  11.0   2  20   801    85     3    0   0   0 |  11   2   0  0.5
cap2/P2/T10/X8/newest      |   9.0   2  24   513     9     0    0   0   0 |  11   2   0  0.5
cap3/P2/T10/X8/newest      |   8.0   1  23   344     0     0    0   0   0 |  11   2   0  0.5
cap2/P1/T10/X8/newest      |   5.0   2  24   235     0     0    0   0   0 |  11   2   0  0.5

lerr  max |residual| at a lap boundary      s>N   seconds with |instantaneous error| > N
exc   times the escape rule fired           Sfl   South passages at E >= +25
mv    total |counts| the baseline moved     Nst   North passages at E >= +30
rev   times the correction changed sign     ltch  passages at |E| >= 40
```

### Four results that do not depend on which candidate you pick

1. **The escape rule never fires in the warm run — for any of the 755
   candidates.** Every warm lap-to-lap request is ≤ 2 counts, so no threshold of
   8 or more ever qualifies. Whatever the escape rule costs, it costs nothing in
   stable warm operation.
2. **Warm-run behaviour does not distinguish caps 2, 3 and 4.** All three move
   the baseline 11 counts in total with 2 sign reversals and leave a maximum
   residual of 0.5. Only cap 1 differs, and only by moving 9 instead of 11.
3. **Every candidate that shows a South-floor-risk or latch passage has a normal
   cap of 0 or 1.** 112 of 755 candidates, 91 of them cap 0 and 21 cap 1. Every
   one of the 151 cap-2 variants, and every cap-3 and cap-4 variant, is clear of
   the measured risk bands on both runs.
4. **No candidate ever overshoots by more than half a count.** The most negative
   residual anywhere in the sweep is −0.5, and it is rounding. In this dataset
   the resting level only ever rises while the controller is active, so the
   escape rule is never tested against a falling or oscillating level.

`--target newest` and `--target median` are identical by definition at
persistence 1, and separate only where there is more than one qualifying
estimate to choose between and enough authority to act on the difference: they
differ on **45 of 375 paired candidates**, every one of them at persistence 2 or
3, every difference in the cold run, and 260 of the 311 differing metric values
at an unlimited exceptional cap. `median` is marginally the more conservative.

---

## 7. The four questions

### 1. Which candidates are clearly unsafe

**Cap 0 and cap 1**, on this dataset, by the measured bands:

- **cap 0** — X17 as it ships. The cold run reaches **+26 counts** of residual
  and **+31** instantaneously, spends **576 s above |20|** and 217 s above |25|,
  and puts **158 South passages** into the band where measured durations reach
  84 ms against an 82 ms floor. This is not a prediction: it is the run that
  ended with MM140 discarded and the locomotive stopped.
- **cap 1** — still leaves **6 South passages** in that band and 336 s above
  |20|, and no escape rule rescues it unless the threshold is low enough to fire
  (T20 with P2 or P3 does not). One count per lap cannot follow an 11-count lap.

No candidate is unsafe on the warm run. No candidate swallows a passage outright.

### 2. Which candidates move the baseline unnecessarily in stable warm operation

**None of them, measurably.** Over 30 warm laps the total movement is 9 counts
(cap 1) or 11 counts (caps 2–4) with 2 sign reversals, against a resting level
that genuinely rose 7 counts. The escape rule never fires. There is no candidate
in this grid whose warm behaviour is objectionable, and no pair of candidates the
warm run can tell apart.

The one lever that does create unnecessary movement is the rounding mode, in the
opposite direction: `correction_trunc` would discard every sub-count request and
let error accumulate to a whole count before acting.

### 3. The smallest-authority rule that handles the cold transient

**A normal cap of 2 is the smallest that clears every measured risk band on both
runs with no escape rule at all.** It leaves 474 s above |15| and 7 s above |20|,
and a worst residual of 15 counts — uncomfortable, but with zero South passages
in the floor-risk band and zero latch-band passages.

Adding a persistent-error escape at **persistence 2, threshold 10, exceptional
cap 8** cuts the worst residual from 15 to 9 and the time above |15| from 474 s
to 9 s, fires twice in the whole cold run and never in the warm one. Persistence
2 is what makes it defensible: it will not act on one lap's estimate.

`cap2/P2/T10/X8`, target `newest` or `median`, rounding `estimate` is the
smallest-authority candidate in this grid that handles the cold transient without
any evident regression. `cap2/P1/T10/X8` is better still on the cold run
(residual 5, zero seconds above |15|) but hands a single lap estimate an 8-count
correction, which this dataset cannot justify — see below.

### 4. Is the dataset enough to choose production parameters

**No. It is enough to choose the next field-test build, and not more.**

- The whole cold transient is **six adjustable laps from one boot**. Every
  conclusion about the escape rule rests on six numbers: +11, +13, +13, +16,
  +17, +16.
- The resting level **only ever rises** while the controller is active. Nothing
  here tests the escape rule against a falling level, a level that reverses, or a
  single corrupted lap estimate. The most negative residual in 755 candidates is
  −0.5 counts, which means the overshoot behaviour is **untested, not safe**.
- Persistence 1 cannot be distinguished from persistence 2 on safety grounds
  here, because nothing ever tried to fool it. The argument for persistence 2 is
  a priori, not empirical.
- The estimator's robustness is demonstrated at one scale only: **lap 5** of the
  warm run (13:05:36–13:08:49, lap 4 under the MM000 cut) carries an excursion
  reaching **+19 counts above the startup baseline and +17 above its own lap
  centre**, sustained across MM002–MM010 from 13:07:31 and decaying to a plateau
  by MM066 — SD 5.27 and span 26 against 1.3–1.7 and 6–10 for every other lap.
  Its per-MM median still lands on **1951.0**: identical to the lap before it,
  two counts below the lap after. An excursion over 100 markers would move it,
  and there is no such excursion in the dataset.
- Warm operation distinguishes no candidate from any other.

What a next field test would have to produce: **a cold boot followed by a cooling
phase** — start cold outdoors, let it warm 40 minutes, then move into shade and
keep circulating — so that the level rises and then falls under one controller,
with completed laps throughout. That is the only experiment in reach that tests
the escape rule in the direction this dataset never goes.

---

## 8. The `SET LOCATION` baseline-reset ambiguity

Inside complete laps the switch changes nothing in this dataset: no completed lap
follows a re-declaration. It changes the cold run's tail, where the locomotive
was driven by hand between re-declarations:

| | max instantaneous error | seconds at \|E\| ≥ 40 | passages in the latch band |
|---|---:|---:|---:|
| `--setloc-resets-baseline yes` (literal reading) | **+46** | 24 | 9 |
| `--setloc-resets-baseline no` | +34 | 0 | 0 |

Reverting the baseline to a value measured 45 minutes earlier creates a 46-count
error instantly. Keeping it does not. Both files are in the artifacts; the choice
is a ruling, not a measurement, and is not made here.

---

## 9. Artifacts and reproduction

```bash
# extraction (SET LOCATION lap rule + the two comparison cuts)
python3 tools/baseline_laps/extract_baseline_laps.py --out tools/baseline_laps/out --compare

# checks
python3 tools/baseline_laps/validate_baseline_laps.py --out tools/baseline_laps/out
python3 tools/baseline_laps/validate_replay.py        --out tools/baseline_laps/out

# the sweep -- 755 candidates, safety measured inside complete laps
python3 tools/baseline_laps/replay_baseline_controller.py \
    --out tools/baseline_laps/out --normal-cap 0,1,2,3,4 --inlap-only \
    --setloc-resets-baseline yes \
    --detail "cap0/noescape,cap1/noescape,cap2/noescape,cap3/noescape,cap4/noescape,cap2/P1/T10/X8/newest,cap2/P2/T10/X8/newest,cap2/P2/T12/X6/median,cap3/P2/T10/X8/newest,cap1/P2/T20/X8/newest"

# the SET LOCATION reset comparison, over all running time
python3 tools/baseline_laps/replay_baseline_controller.py --out tools/baseline_laps/out \
    --normal-cap 0,1,2,3,4 --setloc-resets-baseline yes --prefix allscope_setlocreset_
python3 tools/baseline_laps/replay_baseline_controller.py --out tools/baseline_laps/out \
    --normal-cap 0,1,2,3,4 --setloc-resets-baseline no  --prefix allscope_setlockeep_
```

`--logs` defaults to `~/ngr-telemetry/pi/NGR/telemetry/runs`, `--loco` to
9950011, `--date` to 20260914 (`all` accepted).

| file | rows | what |
|---|---:|---|
| `out/sessions.csv` | 47 | one row per boot |
| `out/origins.csv` | 8 | one row per `SET LOCATION` origin run: direction changes, laps complete, advances discarded, advances spent returning to the origin |
| `out/laps.csv` | 45 | **one row per lap**, `SET LOCATION` anchored |
| `out/markers.csv` | 7,285 | every passage, with the local resting level, the recovered amplitude and the field error |
| `out/observations.csv` | 18,005 | one row per STATUS sample, `usable` + `exclude_reason` |
| `out/replay_candidates.csv` | 2,265 | **candidate-level summary**, 755 candidates × 3 sessions with complete laps |
| `out/replay_laps.csv` | 430 | lap-by-lap replay for the ten-candidate shortlist |
| `out/replay_safety_events.csv` | 307 | every concerning passage for the shortlist |
| `out/replay_sessions_no_data.csv` | 44 | boots with no observation above the tractive floor |
| `out/allscope_setloc{reset,keep}_replay_candidates.csv` | 3,775 each | the §8 comparison |
| `out/laps_mm000_compare.csv` | 36 | MM000-wrap cut — reconciliation only |
| `out/laps_advepoch_compare.csv` | 45 | firmware `adv`-epoch cut — comparison only, **not** a valid anchoring |
