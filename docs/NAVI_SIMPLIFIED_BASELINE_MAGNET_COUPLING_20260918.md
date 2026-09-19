# When the baseline changes, does the magnet reading move with it?

Date: 2026-09-18
Scope: one empirical question, answered from the X18 Hall dataset already
used for the ordinary-track baseline, noise, baseline-drift and magnet-field
measurements cited in
[NAVI_BASELINE_TIMING](NAVI_BASELINE_TIMING_20260916_C3B93D0B.md),
[NAVI_BASELINE_DRIFT](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md) and the
[baseline design checklist](NAVI_SIMPLIFIED_BASELINE_DESIGN_CHECKLIST_20260918.md).
Same recording: `xhr_20260916_191904.xhr`, session `C3B93D0B`, Otto, 1 kHz,
0 gaps, 59 minutes. **No new firmware, no new field capture, no design
decision.** Tool: [`tools/xhr_magnet_baseline_coupling.py`](../tools/xhr_magnet_baseline_coupling.py).

## Answer, up front

**Mostly "tide lifts the boat."** Across the one large, well-measured
baseline change in this session (a ~33-count shift), the magnet's absolute
Hall reading `H` moved with the baseline `B` — not by a little, by most of
it: **ΔH/ΔB = 0.85 (North magnets, median) and 1.18 (South magnets,
median)**. "Tide and pier" (ΔH/ΔB ≈ 0, departure moves opposite to baseline)
is not what happened: essentially no magnet (1 of 84 North, 0 of 79 South)
showed that pattern. The relationship is not a perfect, noise-free 1.00
either — there is real magnet-to-magnet spread (roughly 0.3–1.7 for North,
0.9–1.5 for South) — so the honest answer is hypothesis 3 (quantify), but
the quantity it lands on sits close enough to 1 that for the practical
NAVI_SIMPLIFIED question, **`|H − B|` behaves as approximately, not exactly,
independent of the absolute baseline level.**

The full picture needed two different baseline signals in the same ruling
record, because the one X18 actually uses for detection barely moved all
session (§2). §5 is the load-bearing section; §3–4 explain why it, not the
operative baseline, is what had to be used.

---

## 1. Data and filtering

`xhr_20260916_191904.xhr`, session `C3B93D0B`: 1,548 ruling records. Kept
only `ADVANCED` rulings with a known MM identity (`nav_mm_after`) and known
direction (`nav_dir`) — `WRONG_MAGNET`, `CONTRADICTED`, `NOT_A_MAGNET`,
`NO_POSITION` and `STALE_EPOCH` rulings do not carry a trustworthy MM
identity and were excluded, per the recorder's own ruling semantics
(`tools/xhr_format.py`). **1,538 usable observations, 171 unique magnets**
(86 North-polarity, 85 South-polarity — no magnet showed both polarities
across its repeats, confirming polarity is a fixed property of the physical
magnet, not of travel direction).

Per observation:

| quantity | definition | source field |
|---|---|---|
| MM identity | the marker just crossed | `nav_mm_after` |
| direction | CW (`nav_dir=+1`) / CCW (`nav_dir=-1`) | `nav_dir` |
| polarity | N / S | `polarity` |
| `peak` | departure amplitude, always ≥0 by firmware construction | `peak` |
| `D` (signed) | `+peak` if N, `−peak` if S | derived |
| `B` (entry) | X18's **operative** baseline, frozen at passage-open | `entry_baseline` |
| `H` | `entry_baseline + D` — the reconstructed absolute Hall count at the peak sample | derived |
| `B` (shadow) | a second, non-authoritative rolling median in the same record | `shadow_baseline` |
| `\|D\|` | `peak` | `peak` |

`H` is reconstructed once, from `entry_baseline`, because that is the value
the firmware actually subtracted when it measured `peak`
(`HallCapture_::close()`, decision 0065: peak is read from the judged/
oriented recording, never a lone sample). It is not re-derived from
`shadow_baseline` anywhere below — only compared against it.

**Dwell filter.** Five `ADVANCED` rulings have `duration_ms` far outside the
normal range (86–494 ms for 99% of passages) — 4.9–41.2 seconds, meaning the
locomotive parked at or near the magnet and the passage stayed open. Three
of these, all at MM63, show `shadow_baseline` at 2135/2133/2063 while
`entry_baseline` correctly held 1935 throughout — `shadow_baseline` is
explicitly described in the recorder as kept running "for observability"
even when the operative baseline is held static, and it is **not** protected
against walking into a magnet's own field the way `entry_baseline` is.
These five are excluded from every `shadow_baseline` computation below
(kept for `entry_baseline` ones, which were unaffected).

---

## 2. X18's operative baseline barely moved all session

`entry_baseline` — the value `HallCapture_::sample()` actually subtracts to
decide OPEN/CLOSED and that every stored passage is measured against — took
**exactly three values across the entire 59-minute, 1,538-observation
session: 1932 → 1934 → 1935**, all within the first 7.6 minutes as the
motion-gated rolling median settled out of its 2-second boot prime. For the
remaining **51 of 59 minutes** — through a direction reversal and a
physically real baseline shift documented below — **it did not move by one
count.**

A within-magnet regression of `H` on `entry_baseline` (the panel/fixed-
effects estimator: demean each magnet's own `B` and `H`, pool the residuals,
regress — this is what "controlling for MM identity" means throughout this
report) gives:

| polarity | slope(H~B_entry) | se | R² | n_obs | n_magnets |
|---|---:|---:|---:|---:|---:|
| N | −1.693 | 0.401 | 0.025 | 773 | 86 |
| S | −1.928 | 0.496 | 0.022 | 760 | 85 |

These are not physically interpretable — neither hypothesis predicts a slope
around −1.7 to −1.9. The reason is visible in the data, not just the
statistics: because `entry_baseline` only ever took 3 values, each magnet's
apparent "slope" is set almost entirely by comparing its single earliest
crossing (during the first-lap ramp, often at a different throttle — e.g.
MM51's first crossing was at PWM 49 against PWM 90 for every later crossing)
against a tight cluster of later crossings all at B=1935. That is a
throttle/ramp-up comparison wearing a baseline comparison's clothes. **This
session cannot test the hypothesis using X18's actual detection baseline —
it simply did not move enough.** `shadow_baseline` did move, so §3 onward
uses it instead, with that substitution stated plainly at every step.

---

## 3. The baseline actually moved — visible only in shadow_baseline

Between the last ordinary CW crossing (minute 37.35) and the first CCW
crossing (minute 38.78), the locomotive stopped (`NOT_A_MAGNET` ruling at
minute 38.13, duration 16.4 s, preceded by a 30 s gap, with a further gap
before the next lock) and resumed moving in the opposite direction. This is
the same handling-event stop documented in
[NAVI_BASELINE_DRIFT §1](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md) as a
**−32 count step during a 79 s stationary period, t≈2273 s** — 2273 s =
37.9 min, matching. `entry_baseline`: unchanged (1935 before, 1935 after).
`shadow_baseline`: **1930–1938 before, 1898–1904 after** — a genuine ~33-
count drop that the operative baseline never registered.

| | time span | n (clean) | shadow_baseline range |
|---|---|---:|---:|
| CW leg | 0.56–37.35 min | 1,353 | 1930–1938 |
| CCW leg | 38.78–55.15 min | 180 | 1898–1904 |

164 of 171 magnets were crossed in both legs. PWM actual is comparable
between legs (CW mean 89.1/median 90, CCW mean 85.5/median 90) — the legs
are not obviously a speed-confound, and the design checklist's own citation
(HALL_SELECTIVITY §3) already found peak amplitude speed-invariant across
0–1,049 mm/s, which weighs against a big speed-driven amplitude shift here.

**What this session cannot separate**: direction and this baseline step are
perfectly collinear — there was exactly one reversal, so every CCW
observation is both "after the step" and "traveling the other way." §4
below is the same-direction check that removes the direction confound, at
the cost of a much smaller, noisier ΔB. §5 is the large, direction-
confounded step, which is nonetheless the best-powered evidence in the
dataset and is treated as such, confound stated rather than hidden.

---

## 4. Same-direction check (small ΔB, no direction confound)

Within-magnet slope of `H` on `shadow_baseline`, computed separately inside
each leg so direction cannot be a confound — at the cost of using only each
leg's own natural range (CW: 1930–1938, 8 counts; CCW: 1898–1904, 6 counts,
both close to the ~4–5 count RMS sensor noise already measured for this
railway):

| polarity | leg | slope(H~B_shadow) | se | 95% CI | R² | n_obs | n_magnets |
|---|---|---:|---:|---|---:|---:|---:|
| N | CW | 0.205 | 0.220 | −0.23 .. 0.64 | 0.001 | 680 | 86 |
| N | CCW | −0.353 | 1.631 | −3.55 .. 2.84 | 0.006 | 18 | 9 |
| S | CW | 0.261 | 0.168 | −0.07 .. 0.59 | 0.004 | 673 | 85 |
| S | CCW | 0.500 | 0.866 | −1.20 .. 2.20 | 0.045 | 16 | 8 |

Underpowered on its own — every CI is wide enough to admit both hypotheses
— but every point estimate is positive and none is anywhere near the −1.7
seen with `entry_baseline`. This is consistent with, not independent
confirmation of, §5; it does not contradict it.

---

## 5. The large step: within-magnet, by polarity

For each magnet crossed in both legs: mean `shadow_baseline` and mean `H`
in each leg, then `ΔB`, `ΔH`, and `ΔD = (H−B_shadow)` in each leg (computing
`D` from the leg means is identical to averaging per-observation `D`, by
linearity of the mean).

### North (84 magnets in both directions)

| | mean | sd |
|---|---:|---:|
| ΔB (shadow) | −33.13 | 2.19 |
| ΔH | −28.03 | 7.95 |
| ΔD (shadow) | **+5.10** | 7.99 |

Per-magnet ratio ΔH/ΔB: **mean 0.848, median 0.835, sd 0.241, range
0.273–1.743**. 64% of magnets land in [0.7, 1.3] ("boat-like"); **1 of 84**
lands in [−0.3, 0.3] ("pier-like"); none is negative or above 2.

### South (79 magnets in both directions)

| | mean | sd |
|---|---:|---:|
| ΔB (shadow) | −32.63 | 1.19 |
| ΔH | −38.51 | 4.85 |
| ΔD (shadow) | **−5.88** | 4.83 |

Per-magnet ratio ΔH/ΔB: **mean 1.181, median 1.181, sd 0.149, range
0.870–1.546**. 77% land in [0.7, 1.3]; **0 of 79** land in [−0.3, 0.3];
minimum ratio across all 79 magnets is 0.87 — every single South magnet
moved with the baseline.

**Across-magnet OLS is weak by construction, not by a weak effect**: fitting
`ΔH` on `ΔB` across magnets gives slope 0.434 (N, r=0.119) / 0.556 (S,
r=0.136) — low because `ΔB` itself barely varies *between* magnets (sd
1.2–2.2 counts around a −33 count mean common to nearly all of them). This
is a matched-pairs situation (~163 magnets each given almost the same ΔB),
not a regression with spread-out ΔB to exploit — the per-magnet ratio
distribution above, and the mean-ΔH-over-mean-ΔB ratio it agrees with, is
the meaningful summary, not the across-magnet regression slope. (Internal
check: fitting `ΔD` on `ΔB` gives slope − 1 of the `ΔH` fit exactly in both
polarities, as the algebra `D=H−B` requires — 0.434−1=−0.566 and
0.556−1=−0.444, matching computed output to three decimals.)

### Representative magnets, full record

```
MM20 (S):  B_shadow 1936 x8 (CW) -> 1902 (CCW)               ratio 0.996
           H         1744,1744,1754,1745,1744,1745,1744,1747 (CW) -> 1712 (CCW)
           D_shadow  -192,-192,-182,-191,-192,-191,-192,-189 (CW) -> -190 (CCW)
           almost exactly constant D_shadow across a 34-count baseline drop --
           this session's median South ratio, and it lands almost exactly on 1:1

MM8  (S):  B_shadow ~1935-1936 (CW) -> 1903 (CCW)             ratio 1.546 (max, S)
           H         ~1722-1726 (CW) -> 1674 (CCW)
           D_shadow  ~-210..-214 (CW) -> -229 (CCW)
           overshoots past 1:1 -- H drops MORE than the baseline did

MM167 (N): B_shadow ~1934-1936 (CW) -> 1901 (CCW)             ratio 0.273 (min, N)
           H         ~2102-2113 (CW) -> 2096 (CCW)
           D_shadow  +167..+177 (CW) -> +195 (CCW)
           this session's weakest-transmission magnet -- H barely moves at
           all, and D_shadow moves the "pier" direction (up, not down). This
           is the one North magnet (of 84) whose ratio falls inside the
           [-0.3, 0.3] "pier-like" band in §5 -- the exception, not the
           pattern (1 of 163 magnets total, both polarities combined)
```

For scale: departure magnitude `|D|` (the `peak` field itself) across all
1,533 clean observations is **176.1 ± 22.1 counts (N, median 177, range
95–283)** and **182.5 ± 23.5 counts (S, median 182, range 122–271)** — the
5–6 count mean `|ΔD|` from the baseline step is small against both the
absolute departure size and its own natural observation-to-observation
spread.

---

## 6. Testing the three hypotheses

| hypothesis | predicts | observed |
|---|---|---|
| 1. Tide lifts the boat | ΔH/ΔB ≈ 1, ΔD ≈ 0 | **N: 0.85, ΔD=+5.1. S: 1.18, ΔD=−5.9. Close, not exact.** |
| 2. Tide and pier | ΔH/ΔB ≈ 0, ΔD ≈ −ΔB (≈ +33/+32) | **Not observed. 0/79 S magnets and 1/84 N magnets fall near ratio 0. Observed ΔD is 6–18% of ΔB in magnitude, not ~100%.** |
| 3. Neither (quantify) | — | Closest to (1); real, quantified departure from a perfect 1:1 — North under-transmits somewhat (0.85, wider spread sd 0.24), South slightly over-transmits (1.18, tighter, sd 0.15) |

Hypothesis 2 is rejected with reasonable confidence — it would require `ΔD`
to track `−ΔB` (i.e. ≈ +33 for N, +32 for S); observed `ΔD` is +5.1 and
−5.9, an order of magnitude smaller and, for South, the *opposite sign* from
what hypothesis 2 predicts. Hypothesis 1 is the closest fit; the honest
report is hypothesis 3, landing close to hypothesis 1's prediction rather
than splitting the difference between 1 and 2.

---

## 7. Limitations

- **Single reversal.** Direction and the ~33-count baseline step are
  perfectly collinear in this session — there is exactly one of each. §4's
  same-direction check removes the confound but has too little natural `ΔB`
  (6–8 counts, near the sensor noise floor) to be independently conclusive.
  PWM parity between legs and the prior speed-invariance finding both argue
  against a pure direction/speed artifact, but this session alone cannot
  rule one out with certainty.
- **`entry_baseline` never moved enough to test directly.** The headline
  result rests on `shadow_baseline`, a non-authoritative, non-detection
  channel. It is the same sensor and the same rolling-median formula, just
  without the motion/passage gating that keeps `entry_baseline` from
  drifting under an open passage — and, as §1 shows, also without protection
  from magnet-field contamination during a dwell, which is why the dwell
  filter exists and why any un-caught contamination remains a residual risk.
- **One locomotive, one evening.** Otto, 2026-09-16, one session. No second
  large, independent baseline excursion exists in this capture to
  cross-validate the ~33-count step finding against.
- **The North/South asymmetry (0.85 vs 1.18) is observed, not explained.**
  This report does not investigate why the two polarities differ; it is
  reported as a measured fact, worth knowing before treating "the" magnet-
  baseline slope as a single number.
- **Per-magnet ratios use a 1-observation CCW leg for most magnets** (median
  1 CCW crossing per magnet; a handful have 2). Each ratio is a real
  measurement, not a fit, but it is a single sample of that magnet's
  post-step behavior, not an average over repeats.

---

## 8. The practical NAVI_SIMPLIFIED question

*Does the evidence support using `|H − B|` as a quantity substantially
independent of the absolute baseline level, or does magnet departure itself
systematically vary with baseline level?*

Magnet departure is **not** independent of baseline level in the strong
sense (a genuine, measurable ~5–6 count mean shift accompanied a 33-count
baseline change, and individual magnets ranged as far as a ~25-count shift
in `D`) — but it is **substantially more stable than the baseline change
itself**, because `H` moved with `B` rather than staying fixed. A 33-count
baseline shift produced roughly a 5–6 count (North) to 6 count (South)
change in departure on average — 15–18% leakage, not 100%. Individual
magnets varied more (North's per-magnet ratio spans 0.27–1.74; South's
spans 0.87–1.55), so "approximately independent" is a statement about the
typical/pooled behavior, not a guarantee for every magnet. This is an
empirical description of what the X18 Hall data show; it is not a claim
about what tolerance, guard, or acceptance rule NAVI_SIMPLIFIED should use.

---

## 9. Reproduction

```
python3 tools/xhr_magnet_baseline_coupling.py <path-to-xhr_20260916_191904.xhr> --session C3B93D0B --report
```

The raw capture is not stored in this repository (consistent with other
`*_C3B93D0B` analyses here, which cite the session rather than commit the
`.xhr`/derived CSVs). Session id, loco, sample/ruling counts and zero-gap
status are independently verifiable with `tools/xhr_decode.py <capture>
--report`.
