# Observed vMAX at steady PWM 90 — MM130–160 CW and MM80–60 CCW

Date: 2026-09-18
Question (operator): what is vMAX at PWM 90 for Otto or Toby, in the MM130–160 CW
and MM80–60 CCW stretches, for any specific marker-to-marker interval, confined
to steady-90 running.

Read-only analysis. No firmware changed.

---

## Answer

**Fastest single steady-90 interval anywhere in the two windows: 338.6 mm/s.**

| window | loco | n | median | p95 | p99 | **max** | fastest interval |
|---|---|---:|---:|---:|---:|---:|---|
| CW 130→160  | Otto | 818  | 271.9 | 295.9 | 302.7 | **312.2** | 133→134, 300 mm / 961 ms |
| CW 130→160  | Toby | 1042 | 286.4 | 317.5 | 324.0 | **333.7** | 133→134, 300 mm / 899 ms |
| CCW 80→60   | Otto | 573  | 286.3 | 311.9 | 323.3 | **333.0** | 75→74, 300 mm / 901 ms |
| CCW 80→60   | Toby | 539  | 299.4 | 327.3 | 335.2 | **338.6** | 75→74, 300 mm / 886 ms |

All figures mm/s. 2,972 qualifying intervals in total.

The maximum is not a lone spike: p99 is 302–335 across the four cells, so the
top value sits at the crest of a dense tail, not detached from it.

## What "steady 90" means here

An interval qualified only if PWM read exactly 90 at the arrival marker **and at
the three markers before it** — at least three consecutive intervals held at 90.
Loosening this to "90 at both endpoints only" changes the maxima by 0.0 mm/s in
every cell, so the result is not sensitive to the definition. The stricter form
matters because the raw two-endpoint filter admits intervals still decaying from
a PWM 110 run (e.g. Otto CCW on 2026-08-13 dropped 110→90 at MM77 and the
75→74 interval three markers later was still shedding that speed).

## Where the speed is

Both windows have a broad fast section, not an isolated fast interval. Pooled
median steady-90 speed per interval:

```
CW    130->131  267.6      CCW   80->79  283.1
      131->132  286.7            79->78  285.3
      132->133  292.1            78->77  290.3
      133->134  304.0  <-        77->76  294.7
      134->135  293.7            76->75  297.9
      135->136  302.1            75->74  307.4  <-
      136->137  291.7            74->73  285.7
      137->138  275.6            73->72  284.1
      ...                        ...
      143->144  264.9            65->64  248.9
      144->145  253.7            64->63  244.4
```

133→134 and 75→74 are the crests of smooth gradients spanning six or seven
markers. That is consistent with track grade and inconsistent with a mis-surveyed
single spacing, which would show as a one-interval discontinuity.

Sample counts thin sharply beyond MM147 CW (n drops 108 → 3) and beyond MM73 CCW
(n drops 139 → 8): the locomotives rarely held 90 through the far end of either
window. The maxima above all come from the well-sampled halves.

## Context: speed against steady PWM in the same two windows

| PWM | loco | n | median | p99 | max |
|---:|---|---:|---:|---:|---:|
| 60  | Otto | 198  | 130.5 | 188.9 | 189.5 |
| 60  | Toby | 217  | 150.0 | 198.0 | 198.8 |
| 72  | Otto | 88   | 180.8 | 223.1 | 223.1 |
| 75  | Toby | 223  | 236.0 | 265.0 | 271.2 |
| 90  | Otto | 1391 | 278.3 | 319.8 | 333.0 |
| 90  | Toby | 1581 | 290.1 | 331.5 | 338.6 |
| 99  | Otto | 30   | 319.5 | 338.9 | 338.9 |
| 109 | Otto | 38   | 379.5 | 422.5 | 422.5 |
| 110 | Otto | 356  | 353.4 | 411.0 | 415.5 |
| 110 | Toby | 192  | 357.1 | 395.8 | 397.7 |

This locates the ~417 mm/s figure quoted in the NAVI_SIMPLIFIED prospectus §6.1:
it is a **PWM 110** observation, not a PWM 90 one. At PWM 90 the observed ceiling
is about 80 mm/s lower.

## Data and method

**Sources.** Two sessions, both locomotives:

| file | records | locos |
|---|---:|---|
| `field-records/logs/20260813_otto_1_13_noquorum_watch.log` | 41,546 `mm/marker` | Otto, Toby |
| `field-records/logs/20260820_morning_session.log` | 24,394 `mm/marker` | Otto, Toby |

Firmware: QUORUM 1.13 / `QUORUM_1_16R_IR_TEST_A`. These are not NAVI_ONE
architectures, but the question is about what the locomotive can physically do at
a given duty cycle, which the navigation architecture does not change.

**Timing is locomotive-clock, not broker.** `dt` in `mm/marker` is measured
onboard. Broker receive timestamps were used only to order records and to pair
topics; no interval is derived from them.

**Distance is surveyed map spacing** from `ROUTE_SPACING_MM` in
`firmware/test-programs/NAVI_ONE_X18_RECORDER/RouteMap.h` (171 markers, 52,150 mm,
mean 305.0). Both windows are almost uniformly 300 mm, which is why the operator's
choice of windows is a good one: distance error cannot differentially bias the
comparison between intervals.

**Gating.** Only `timing_gate == ACTIVE` marker records were used, which excludes
`QUARANTINED`, `RAMP`, `LOW_PWM` and `NO_POSITION`.

**Repeated-`mm` records handled.** 327 records repeat the previous `mm` within
the same run. They are NOT one phenomenon — see the correction below. Each one
was excluded from the interval arithmetic rather than counted as a fresh arrival.
This is not cosmetic: uncorrected, the apparent maximum was **801.6 mm/s** (Otto,
MM71→70, 295 mm / 368 ms, peak 46 counts) — an artefact entirely, and exactly the
kind of number that would have justified a wrong Vmax.

**Robustness.** Only **10 of 2,972** windowed steady-90 intervals touch a
repeated-`mm` record at all. Dropping all ten outright, with no repair, leaves
every maximum in the table above unchanged to 0.1 mm/s, and the medians within
0.3 mm/s. The headline does not depend on how these records are treated.

### QA gates

1. **Clock provenance** — `dt` onboard; broker timestamps used only for ordering.
2. **Field semantics from source** — spacing read from `RouteMap.h`; `ACTIVE`
   semantics read from the gate values actually present in the corpus.
3. **Population stated** — two sessions, QUORUM lineage, named above.
4. **Independent recomputation** — the same intervals were computed a second way,
   from the `mm/speed` topic's own onboard `dist`/`dt`/`mmps` fields, and paired
   to `mm/marker` by locomotive and marker. 1,180 intervals paired; `dt` agreed
   **exactly in 1,179 of 1,180**, worst disagreement 36 ms. `mm/speed` covers
   fewer records (Toby has none in the 2026-08-20 session), which is why the
   marker-derived figures above are the headline.
5. **Single-source dominance** — no cell draws more than 66% from one session.

## What this is, and is not

This is the **highest speed observed** at steady PWM 90 in two favourable
stretches of the Lowline. It is a **lower bound on the physical ceiling**, not
the ceiling.

Prospectus §6.1 is explicit that `Vmax` must not be defined as the fastest speed
ever seen in telemetry, and this number does not satisfy that requirement. Known
reasons the true PWM-90 ceiling is higher than 338.6 mm/s:

- **Pack voltage.** These runs sit at 15.6–16.7 V. A fuller pack delivers more
  motor volts at the same duty cycle. Speed at fixed PWM is not fixed.
- **Grade.** The windows are favourable but not necessarily the most favourable
  point on the railway; the search was confined as instructed.
- **Load and consist.** Single locomotives; a lighter or freer-rolling condition
  would run faster.
- **Coverage.** Steady-90 running thins out over the far half of both windows.

For the hard-reachability layer, the defensible path remains the one §6.1 already
states: derive the bound from motor no-load speed, gearing and wheel
circumference, and confirm that the derived figure comfortably exceeds the
338.6 mm/s measured here.

## Reproduction

Extraction and analysis scripts are in the session scratchpad; the two log files
named above are the only inputs. Method: parse `ngr/loco/+/mm/marker`, keep
`ACTIVE`, collapse repeated `mm`, require PWM 90 across four consecutive markers,
divide `ROUTE_SPACING_MM` by locomotive-clock `dt`.


---

# CORRECTION — 2026-09-18, same day

The paragraph above originally described all 327 repeated-`mm` records as "the
same physical magnet opening twice." That was wrong, and it was asserted from one
examined case (the peak-46 artefact) generalised to the whole population without
checking. Classifying all 327:

| n | what it is |
|---:|---|
| 147 | two full-strength detections, same polarity |
| 127 | two full-strength detections, **opposite polarity** |
| 40 | weak first (<70 counts), full-strength second |
| 30 | full-strength first, weak second — a genuine re-opening |
| 5 | boot / declaration discontinuity |

Only **30** are re-openings of the same magnet in the sense originally claimed.
**274 are two full-strength detections reported at the same `mm`** — median peak
176 counts, median separation 1,518 ms. At the speeds involved that separation is
roughly one marker spacing, and in 127 cases the two polarities disagree. Those
are not one magnet read twice. They are a second, real magnet on which the
navigator **did not advance** — position silently lagging, which is the failure
Otto's own profile already documents:

> "A rejected event never reaches telemetry, so the next accepted marker still
> reads mm+1 and the step stays 1 while the position silently lags."
> — `firmware/QUORUM/LL_LocoConfig_9950011.h`

The design consequence differs from what the original text implied. An amplitude
threshold does not address the bulk of this population:

- QUORUM's **only** amplitude gate is entry = `HALL_DEADBAND_COUNTS` +
  `HALL_ENTRY_MARGIN_COUNTS`. `HALL_MIN_PEAK_DELTA` (35) appears solely in the
  boot telemetry string and gates nothing — the profile says so explicitly.
- Both locomotives ran `entry_margin` **13** through both sessions analysed here,
  i.e. an entry threshold of **38 counts**. That is why a 46-count event was
  admitted. Otto's profile has since moved to `entry_margin` 45 → **70 counts**
  (2026-08-20 evening, after 65 → 90 counts proved too aggressive and cost four
  markers around mm 100–125). **Toby's profile is still 13 → 38 counts.**
- A 70-count entry would have rejected **40 of 327** (12%) of these records, and
  would have cost **82 of 54,502** real arrivals (0.15%).

So the ≥70-count detector of NAVI_SIMPLIFIED §5.1 does dispose of the specific
record that produced the 801.6 mm/s artefact, and of about an eighth of the
population. It does not touch the other 274. Those are full-strength, correctly
detected magnets that the navigator declined to count — a §11 judgment question,
not a §5.1 detection one, and the distinction matters because an amplitude
threshold raised far enough to catch them would start missing real markers long
before it caught them.

Not analysed here: why the navigator declined. The records cluster (MM101 hosts
55 of 327, MM35 hosts 24), which suggests specific locations rather than a
general rate, but that is a separate question from vMAX and was not pursued.

The vMAX figures in this report are unaffected — see **Robustness** above.
