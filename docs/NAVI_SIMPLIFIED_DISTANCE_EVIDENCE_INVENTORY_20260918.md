# Distance-traveled evidence inventory for NAVI_SIMPLIFIED — what we already have

Date: 2026-09-18
Scope: inventory and quantify existing motion evidence relevant to estimating
distance traveled between MM magnet detections. **No algorithm is proposed
here and no firmware changed.** This answers "what do we know," not "what
should NAVI_SIMPLIFIED do with it."

Sources used:
- `NGR-Files/Calibration Data OTTO with two 2 axle passenger cars/` — three
  distinct files (`cal_9950011_20260630_113040`, `_131620`, `_135528`;
  `_135528_full` is byte-identical to `_135528`, confirmed by `diff`, and not
  double-counted). Otto only, loaded with two 2-axle passenger cars, one day.
- [HALL_SELECTIVITY_AND_GATE_TIMING_FROM_CAL_20260630.md](HALL_SELECTIVITY_AND_GATE_TIMING_FROM_CAL_20260630.md) — a prior same-day analysis of the identical three files (2026-09-18), reused here rather than re-derived.
- [NAVI_VMAX_PWM90_20260918.md](NAVI_VMAX_PWM90_20260918.md) — Otto vs. Toby, August MQTT run logs, unloaded.
- [NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md](NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md) — 1,279 station-approach ramps from regular run logs.
- `docs/decisions/0048`, `0066`, `0067`, `0068`, `0079` — ratified/proposed grade and per-locomotive findings that already rest on this same corpus.
- `tools/xhr_ramp_templates.py` and siblings — the existing ramp-template tooling.

---

## 0. What kind of record this is

Every line in the three calibration files is one marker passage:

```
MM120  ... dt=0 dt_expected=2520 timing_mode=SPEED_TRANSITION effective_pwm_avg=32 seg_mm=0 speed_mm_s=0.0 ...
```

`seg_mm` (surveyed spacing to the previous marker) and `dt` (locomotive-clock
milliseconds since the previous marker) are both **directly measured**;
`speed_mm_s = seg_mm / dt × 1000` is **derived**, not measured, and is a
**leg-average**, not an instantaneous value. `timing_mode=MOVING` legs were run
at a held PWM; `timing_mode=SPEED_TRANSITION` legs occurred while
`effective_pwm_avg` was changing — 20 such legs exist across the three files
(1 + 3 + 16, see §4).

The three files are not repeats of one run. They are three separate PWM sweeps
covering non-overlapping ranges of the same day:

| file | PWM range covered | records |
|---|---|---|
| `_113040` | 35–90 | 2,178 |
| `_131620` | 47–240 (steps ≥100) | 1,189 |
| `_135528` | 32–240 | 2,187 |

Direction was inferred here from the sign of consecutive MM numbers
(increasing = one travel direction, decreasing = the other), since the file's
own `dir=` field records agreement with the *expected* direction, not compass
sense. `_135528` ran essentially one direction throughout (a monotonic
sweep); `_113040` and `_131620` ran mostly one direction with small
opposite-direction subsets (9–14 records) — almost certainly short
reversals during setup, not full-loop opposite-direction runs, and are
labeled low-confidence below for that reason.

---

## 1. Empirical PWM → velocity relationship

**Direct measurement, this corpus (Otto + 2 coaches), full-loop medians by PWM
decade** (from HALL_SELECTIVITY, reused, n=5,021 confirmed passages):

| PWM | n | median mm/s | p99 | max |
|---:|---:|---:|---:|---:|
| 40–49 | 486 | 69.5 | 134.2 | 156.0 |
| 60–69 | 554 | 147.2 | 212.8 | 238.9 |
| 80–89 | 420 | 224.6 | 281.7 | 303.3 |
| 90–99 | 277 | 239.8 | 303.6 | 322.6 |
| 100–109 | 296 | 298.9 | 362.8 | 372.2 |
| 110–119 | 126 | 330.6 | 411.5 | 414.4 |
| 120–129 | 273 | 386.1 | 454.5 | 487.8 |
| 200–209 | 278 | 697.7 | 855.1 | 1124.6 |
| 240–249 | 167 | 842.7 | 996.7 | 1123.6 |

Full 40–249 table is in HALL_SELECTIVITY §1; not reproduced in full here.

**A DERIVED linear fit was already attempted on this exact corpus and
rejected** (decision 0066): `speed ≈ 3.990 × (PWM − 25.1)` mm/s, fit to 4,617
samples. It was rejected for firmware use because the same corpus shows **31%
loop-position-dependent variation at a fixed throttle** — larger than the
effect the fit exists to capture. Treat this line as a rough first-order
description of the corpus, not a usable model; it is recorded here as prior
art directly answering "has this been tried," not as a recommendation.

**Cross-check from a different corpus (August MQTT run logs, unloaded, both
locomotives)**, NAVI_VMAX_PWM90: Otto median 278.3 mm/s and Toby 290.1 mm/s
at PWM 90, confined to two favorable track windows — comparable order of
magnitude to the June figure (239.8 loop-wide median at PWM 90–99) but not
the same measurement (different windows, different day, no coaches). The two
corpora should not be merged into one PWM curve without accounting for
consist load, which was not held constant between them (see §3).

## 2. Evidence at PWM 60, 90, 110, 120 specifically

Directly measured from this session's own re-parse of the three files
(median and outlier-trimmed mean; outliers = points outside 0.4–2.5× the
group median, flagged as instrument transients per
[transients-are-not-electrical](../field-records) precedent, not distance):

| PWM | file | direction (inferred) | n | median mm/s | trimmed mean | outliers excluded |
|---:|---|---|---:|---:|---:|---:|
| **60** | `_113040` | decreasing-MM | 176 | 133.7 | 135.8 | 1 |
| **60** | `_135528` | increasing-MM | 194 | 153.7 | 152.5 | 0 |
| **90** | `_113040` | decreasing-MM | 302 | 239.1 | 240.1 | 2 |
| **110** | `_131620` | decreasing-MM | 129 | 328.7 | 326.7 | 0 |
| **110** | `_131620` | increasing-MM (low-n, likely a short reversal) | 14 | 292.1 | 290.9 | 0 |
| **120** | `_131620` | decreasing-MM | 120 | 365.9 | 362.4 | 0 |
| **120** | `_135528` | increasing-MM | 169 | 398.2 | 393.8 | 1 |

No interpolation was performed for these four values — every row above is a
direct in-corpus measurement at exactly that `effective_pwm_avg`. Other
plateaus with ≥8 samples exist at PWM 35, 40, 45, 50, 55, 65, 70, 75, 84,
100, 130, 140, 150, 160, 180, 200, 217, 220, 221, 240 (full table available
on request; omitted here because the task asked specifically about 60/90/
110/120 plus "other values for which useful measurements exist" — the above
list is that inventory).

**PWM 60 and PWM 120 each show a ~13–9% difference between the two
full-loop directional sweeps taken hours apart on the same day.** This is
addressed in §3 below rather than averaged away here.

## 3. Direction and location effects on velocity at fixed PWM

**These effects are large enough to matter**, and this is not new — three
ratified/proposed decisions already rest on this same June-30 corpus:

- **Decision 0048** (a different corpus, Toby, PWM 90 fixed): CW vs. CCW
  marker-crossing duration differs by up to **73 ms (≈60%)** at specific
  markers, in a spatially contiguous, sign-consistent pattern (26 markers in
  a row, one sign; another 12-marker block, the opposite sign) — the
  signature of grade, not noise.
- **Decision 0066** (this June-30 corpus): the Grillers climb, CW only, MM65
  onward, runs measurably slower than the surrounding cruise; the rejected
  linear PWM fit's 31% loop-position residual is dominated by this section.
- **Decision 0067** (this June-30 corpus): the curve into Patio, CCW only,
  bottoms out at **0.72× of the local cruise index** at MM29 — "about twice
  the grade of Grillers" — and never recovers to 1.00 for ten markers.

**This session's own re-parse confirms the same effect at the exact PWM
values requested:**

| PWM | direction A median | direction B median | Δ |
|---:|---:|---:|---:|
| 40 (`_113040`) | 59.0 (dec) | 53.6 (inc, n=14) | dec ~10% faster |
| 55 (`_113040`) | 117.4 (dec) | 102.2 (inc, n=9) | dec ~15% faster |
| 60 (two files) | 133.7 (dec, `_113040`) | 153.7 (inc, `_135528`) | inc ~15% faster |
| 110 (`_131620`) | 328.7 (dec) | 292.1 (inc, n=14) | dec ~12% faster |
| 120 (two files) | 365.9 (dec, `_131620`) | 398.2 (inc, `_135528`) | inc ~9% faster |
| 130 (`_131620`) | 419.2 (dec) | 395.6 (inc, n=9) | dec ~6% faster |
| 180 (`_131620` vs `_135528`) | 609.8–611.6 (both) | 627.6 (inc) | ~3%, effect shrinking |
| 200 (`_131620` vs `_135528`) | 688.9–703.6 | 697.7 | ~2%, effect gone |

**The pattern is consistent across every independent comparison in this
corpus**: the direction/location effect is largest (9–15%) at low-to-mid PWM
and shrinks toward negligible (2–3%) by PWM 180–200. This matches decision
0066/0067's physical account — grade is a roughly constant additive force,
so it is a larger fraction of available torque (and therefore of speed) at
low commanded power than at high. **A single class-wide PWM-to-speed value
would misrepresent low-PWM distance by 10–15% depending on which direction
or section it is applied to**, which is the same conclusion the operator and
prior decisions already reached from partly independent evidence.

Caveat on confidence: the 40/55/110/130 rows above compare a large
same-direction sample against a **9–14 sample** opposite-direction subset
that is very likely a short reversal during setup, not a full-loop run —
treat those five rows as suggestive, not as strong as the 60/120 rows, which
compare two full-loop sweeps.

**Otto vs. Toby — not directly testable in this corpus** (Toby does not
appear in any of the four calibration files: it is Otto-only). The only
Otto-vs-Toby comparison found is NAVI_VMAX_PWM90 (August MQTT logs, no
coaches): Toby faster than Otto by **~15% at PWM 60** (150.0 vs. 130.5
median), narrowing to **~1% at PWM 110** (357.1 vs. 353.4). This is the same
"gap shrinks with PWM" shape as the direction effect above, but it cannot be
cleanly separated from consist: the June calibration is loaded (two
coaches), the August cross-loco comparison is unloaded. **Two variables are
confounded — locomotive identity and consist load — and no evidence found in
this search holds one constant while varying the other.** Given this,
**combining Otto and Toby into one motion model is not justified without
either a per-locomotive correction or evidence that isolates load from
locomotive identity**, which is also decision 0079's existing conclusion
("Otto's recognizer constants are measured on Otto") reached independently
for a different measurement.

## 4. Intervals containing PWM changes between known MM positions

20 `timing_mode=SPEED_TRANSITION` records exist across the three files —
every one where `effective_pwm_avg` differed from the previous record while
still inside a `MOVING`-adjacent leg. Full extraction:

**`_135528`** (16 records, the richest — this file contains most of the
day's ramp-up/ramp-down events):

| MM | dt (ms) | pwm | seg_mm | speed (leg-avg mm/s) | hallms |
|---|---:|---:|---:|---:|---:|
| 120 | 0 | 32 | 0 | 0.0 | 502 |
| 121 | 2951 | 45 | 300 | 101.7 | 223 |
| 122 | 1863 | 61 | 300 | 161.0 | 172 |
| 123 | 1540 | 72 | 300 | 194.8 | 160 |
| 124 | 1318 | 82 | 300 | 227.6 | 145 |
| 145 | 2305 | 56 | 305 | 132.3 | 290 |
| 146 | 3989 | 40 | 300 | 75.2 | 481 |
| 065 | 28272 | 43 | 315 | 11.1 | 726 |
| 067 | 5765 | 43 | 300 | 52.0 | 859 |
| 143 | 3225 | 43 | 300 | 93.0 | 292 |
| 144 | 2190 | 60 | 300 | 137.0 | 191 |
| 145 | 1761 | 73 | 305 | 173.2 | 174 |
| 146 | 1402 | 83 | 300 | 214.0 | 157 |
| 084 | 1292 | 88 | 300 | 232.2 | 188 |
| 085 | 1579 | 73 | 300 | 190.0 | 219 |
| 086 | 2212 | 60 | 330 | 149.2 | 273 |
| 100 | 2581 | 44 | 300 | 116.2 | 325 |

**`_131620`** (3 records, a decel sequence): MM105 (pwm 82, 241.2 mm/s) →
MM104 (pwm 68, 193.4 mm/s) → MM103 (pwm 47, 108.1 mm/s), each over a
measured 300 mm leg.

**`_113040`** (1 record): MM034, pwm 78, 300 mm leg, 239.6 mm/s.

**What this evidence is and is not.** Each row is the surveyed distance and
locomotive-clock time for a leg during which PWM was ramping. The `speed`
column is that leg's **average**, not the speed at either endpoint — the
MM120–124 sequence (0 → 101.7 → 161.0 → 194.8 → 227.6 mm/s while PWM climbed
32→45→61→72→82) is a staircase of leg-averages, not four instantaneous
samples. There is no sub-leg timing anywhere in this corpus: the finest time
resolution is one sample per ~300 mm magnet crossing.

## 5. Station approach and departure specifically

The June-30 corpus contributes almost nothing here directly — it is a PWM
sweep, not a station-stop test, and contains only the 20 transition records
above, none of which is a full ramp-to-zero or a departure-from-zero. The
authoritative evidence for this question is a different, much larger corpus,
already analyzed: **[NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md](NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md)**, 1,279 usable
`ZERO_RAMP → DWELL_BEGIN` ramps from the regular MQTT run logs (19 dates,
both locomotives, all 4 stations × 2 directions after exclusions). Key
findings reused here rather than re-derived:

- **`DWELL_BEGIN` is a command boundary** (PWM ramp reached zero), not the
  physical stop instant — confirmed to track `entry PWM × 200 ms` within 0.4 s
  in 99.2% of ramps.
- Distance to the **last marker crossed before the ramp** is MEASURED
  (surveyed spacing ÷ logged dt). Distance from that marker **to physical
  rest is INFERRED**, bounded above by the next surveyed spacing (which was
  demonstrably not reached) — never directly measured by any evidence found.
- Entry speed explains a median r² of only 0.37 of the variance in total
  ramp-down travel; the sign is consistent (faster entry → more travel) in
  every one of 16 configurations, but residual scatter (~106 mm RMS, a third
  of a marker spacing) remains even after fitting it.
- A 200 ms window anywhere in the *measured* part of a ramp covers
  **11–30 mm** of track (percentile floors, by station/direction/locomotive)
  — the finest-grained sub-marker travel bound found in any searched source.
- Two stations (Grillers, Patio) already carry ratified/proposed
  direction-specific throttle profiles for approach and departure (decisions
  0066–0068), derived partly from this same June-30 calibration's grade
  index.

## 6. Can existing telemetry reconstruct continuous v(t)?

**No, not below marker-crossing resolution, in any source examined.**

- The finest time-and-distance record anywhere in this search is one
  `(dt, seg_mm)` pair per magnet crossing — a leg-average speed, roughly
  every 300 mm and, depending on PWM, every 0.3–3 s.
- During `SPEED_TRANSITION` legs (§4), that single leg-average is known to
  mask a within-leg change in true velocity, since PWM itself was changing
  across the leg — a leg's reported speed is not usable as the instantaneous
  velocity at either its start or its end.
- Beyond the last marker crossed before a stop, or before the first marker
  reached after a departure, **no distance sample exists at all** (§5); only
  a bound (current leg measured, next leg's spacing as an upper limit).
- The 200 ms baseline-window travel floors (§5) are the only sub-marker
  quantity found, and they are themselves derived percentile bounds from the
  ramp corpus, not direct instantaneous-velocity samples.

**Conclusion: a true continuous v(t) cannot be reconstructed from any
telemetry field found in this repository or in the supplied calibration
files.** What exists is a piecewise-constant staircase of leg-average
speeds at marker resolution, plus (for station ramps specifically) a
command-timed PWM ramp shape whose actual kinematic effect on distance is
bounded, not measured, on its last leg.

## 7. Prior experiments, calculations, scripts, or analyses found

| artifact | what it covers | status |
|---|---|---|
| `docs/HALL_SELECTIVITY_AND_GATE_TIMING_FROM_CAL_20260630.md` | Same three calibration files; PWM→speed table by decade, Vmax correction, Hall-amplitude discriminator work | Reused directly in §1 |
| `docs/NAVI_VMAX_PWM90_20260918.md` | Otto vs. Toby at fixed PWM, two field-log sessions, no coaches | Reused directly in §1, §3 |
| `docs/NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md` + `analysis/ramp_templates/ramp_templates_runlogs.json` | 1,279 station-approach/departure ramps, per station/direction/locomotive | Reused directly in §5; supersedes §5 of the doc below |
| `docs/NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md` | Earlier, smaller ramp-template attempt from one raw Hall capture | Partially withdrawn by the doc above (its "insufficient CCW data" table); window audit and Hall-uniformity result still stand |
| `tools/xhr_ramp_templates.py`, `xhr_baseline_timing.py`, `xhr_baseline_drift.py`, `xhr_baseline_eligibility.py` | Working code that builds ramp-down travel templates from `.xhr` captures, explicit "observed vs. estimated" bookkeeping | Existing tooling, not re-run here |
| `docs/decisions/0048` | CW/CCW duration asymmetry at fixed PWM (different corpus, Toby) | Ratified finding reused in §3 |
| `docs/decisions/0066` | Grillers climb, CW throttle profile; rejects a linear PWM→speed fit on this exact June-30 corpus as false precision | Reused in §1, §3; the rejected fit is recorded here as prior art, not resurrected |
| `docs/decisions/0067` | Patio curve, CCW throttle profile, quantified grade index | Reused in §3 |
| `docs/decisions/0068` | Station stop machine, approach pacing table (marker-time based, not distance-integral based) | Reused in §5 |
| `docs/decisions/0079` | Per-locomotive constants required, evidence from Otto only used for Otto | Cited in §3 as existing precedent for not pooling locomotives |

No other PWM-calibration, acceleration/deceleration, or stopping-distance
scripts were found beyond these. No prior attempt at a true ∫v(t)dt
reconstruction (as opposed to leg-average or bounded-interval accounting)
was found anywhere in `docs/`, `tools/`, or `firmware/`.

## 8. Is `distance = ∫v(t)dt` computable from what we have?

**Split by regime:**

- **Within one marker-to-marker leg, at held PWM:** distance is not derived
  from an integral at all — it is the surveyed spacing, a direct
  measurement. `dt` and `speed_mm_s` are derived from it, not the reverse.
  No integration question arises here.
- **Across several legs at held PWM (e.g., a cruise section):** total
  distance is the sum of surveyed leg spacings for every leg actually
  crossed — again a direct measurement (sum of known constants), not an
  integral of a fitted or interpolated velocity curve.
- **Across a PWM ramp, while legs are still being crossed** (§4): the same
  sum-of-surveyed-legs method still gives an exact total distance for the
  legs crossed, but the *velocity* attributed to each leg is a leg-average
  that provably does not equal instantaneous velocity at either endpoint
  (the 32→45→61→72→82 PWM staircase produces a speed staircase, not a smooth
  curve) — so this cannot be extended to *sub-leg* distance-at-time-t without
  an assumed velocity-time shape, and no such shape is fit or validated
  anywhere in the searched evidence.
- **Beyond the last leg crossed before a stop, or before the first leg
  reached after a departure** (§5, §6): there is no measured velocity
  sample at all. Existing practice (NAVI_RAMP_TRAVEL_FROM_RUNLOGS) bounds
  this distance between the last measured leg and the next surveyed
  spacing; it does not, and with current telemetry cannot, integrate a
  velocity curve to produce a point estimate.

**Conclusion: the data support exact distance accounting wherever a marker
is actually crossed, and only a bounded range (not an integral) for the
final leg into or out of a stop.** Any `∫v(t)dt` claim finer than
leg-resolution, or covering the uncrossed final leg, would require an
assumed velocity-time shape that no evidence examined here establishes or
tests — consistent with the prompt's caution against assuming instantaneous
velocity changes at PWM transitions, which the SPEED_TRANSITION records in
§4 confirm does not hold.

---

## Summary table: measurement vs. derived vs. assumption

| quantity | status |
|---|---|
| `seg_mm` (leg spacing) | **MEASURED** (survey) |
| `dt` (leg time) | **MEASURED** (locomotive clock) |
| `speed_mm_s` per leg | **DERIVED** (seg_mm/dt), and only a leg-average |
| PWM→speed decade table (§1) | **DERIVED** (aggregation of measured legs) |
| Linear PWM fit `3.990×(PWM−25.1)` | **DERIVED**, previously rejected for use |
| Direction/location effect sizes (§3) | **DERIVED** from measured legs, cross-checked across independent files/decisions |
| Otto vs. Toby gap | **DERIVED**, but confounded with consist load — not isolated by any evidence found |
| Station approach entry speed | **MEASURED** (last pre-ramp leg) |
| Distance from last marker to physical rest | **BOUNDED**, not measured (upper bound = next surveyed spacing) |
| Sub-leg or sub-ramp v(t) | **NOT AVAILABLE** — no source found supports it; any use would be an **ASSUMPTION** |

*Read-only analysis. No firmware changed. No NAVI_SIMPLIFIED design proposed.*
