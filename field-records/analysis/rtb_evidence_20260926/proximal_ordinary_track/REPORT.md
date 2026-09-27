# Proximal ordinary-track Hall behaviour around MM passages — evidence report

**Date:** 2026-09-27  **Status:** analysis only. No firmware change, no RTB
implementation, no design decision. The recommendations are labelled as
recommendations. David decides the architecture.

**Question asked.** Is the best reference for interpreting a magnet the
ordinary-track Hall behaviour *immediately preceding* it? How short and how
proximal can that sample be? Does the evidence support the following cycle
better than an acquired-and-held baseline?

> ordinary track → continuously maintained proximal robust reference →
> departure freezes learning → observe complete event → RTB demonstrated →
> resume learning

**Source.** `field-records/analysis/rtb_evidence_20260926/` (README and
manifest read first). The package is on branch
`claude/navi-ps-r1-20q-standards` at `82600b4`. Only the numerical passage CSVs
are used. The plots and the recorder's baseline and ruling fields are used
only as context. Historical baselines, lap averages and X22's operative/shadow
baselines are **not** treated as ground truth anywhere below.

**Reproduce.**

```bash
export RTB_PKG=field-records/analysis/rtb_evidence_20260926   # dir holding passages/ + manifest.json
cd field-records/analysis/rtb_evidence_20260926/proximal_ordinary_track
for s in 1.0 0.5 1.5; do python3 scripts/clean.py $s > results/intervals_x$s.txt; python3 scripts/prox.py $s > results/prox_x$s.txt; done
python3 scripts/settle.py > results/settle.txt
python3 scripts/figures.py
```

---

## 0. Summary

| # | Finding | Strength |
|---|---|---|
| 1 | Every magnet has **opposite-sign lobes on both sides** of the core. The *leading* lobe begins about **3–3.5 W** before the core. W is the core's half-max width. Toby's leading lobe reaches **−11 to −25 counts**. So the samples immediately before a departure are **not ordinary track**. | Strong for Toby (10/10 events). Moderate for Otto (lobe −2 to −11, partly masked by the recorder comb). |
| 2 | The lobe extent scales with **W, not with milliseconds**, so it is spatial. For Toby's leading settle distance: corr(ms, W) = 0.94, CV 0.13 in W against 0.34 in ms. Magnet spacing is also **10.8–13.9 W** across 1.07–2.60 s, so W behaves as a speed-free ruler. | Strong for Toby. Otto inconclusive (comb-limited). |
| 3 | A reference frozen *at* departure absorbs the leading lobe. A 100-sample window ending at the departure proxy is biased by a median of **−9 counts (worst −22)** for Toby. The bias falls to ≤ 2 counts only when the window ends **≈ 2.5–3 W** before departure (Toby) or **≈ 1.5–2 W** before (Otto). | Strong (all 27 pre-core windows). |
| 4 | Once the lobes are excluded, ordinary track **within an interval is flat at the 1–3-count level**. MAD is 1–3. Medians of 100-sample blocks span 0–6 counts. Theil–Sen slope is ≤ 7 counts/s. The pre- and post-magnet clean medians differ by **≤ 2 counts** in all 24 magnets that have both. | Strong within this package. |
| 5 | **Minimum clean sample length is set by noise structure, not by distance.** For Toby, 20–50 samples give p95 error ≤ 1 count. For Otto, windows shorter than 100 samples reach p95 5–8 counts. The cause is a **100 ms, recorder-batch-locked +7–10-count comb**. Windows of exactly 100 samples are within ≤ 2 counts (max). | Strong. The comb is time-locked and present at PWM 0. |
| 6 | Clean ordinary evidence between consecutive magnets is only about **3–6 W**, roughly 35–45 % of the inter-magnet stretch. The rest is core plus lobes. | Strong. |
| 7 | **At the scale of seconds, proximal is no better than held.** Against the post-magnet level, P100, the whole-interval median and a value held from the segment's first interval are all within a mean of ~1 count and a max of 2–3 counts. | Strong, but this is an **absence of variation** over 4–6.5 s windows. |
| 8 | **Across the session, a held value fails.** Otto's clean ordinary level is 1932–1935 in the early passages and 1901–1903 in the late ones. An early-acquired held value is up to **31 counts** off. The recorder's held baseline (1935) is **34** off. | Strong as an observation. The *cause* (place, time, loop direction, handling) cannot be separated from this package. |

**Bottom line.** The data support the *shape* of the proposed concept, with one
correction and one open gap.

- **Supported:** keep the reference fresh rather than hold it for a session
  (finding 8). Observe the complete event, including the trailing lobe, before
  declaring return (findings 1, 4).
- **Correction:** "departure freezes learning" is too late as literally stated.
  By the time a departure is detected, a reference that updates continuously
  has already absorbed the leading lobe (finding 3). The evidence points to
  freezing a reference that ends about **3 W before** departure. This is
  distance-based, not time-based.
- **Open:** this package cannot tell *how proximal* the reference must be
  between the 6 s scale (no measurable change) and the 40 min scale (33 counts).
  The hypothesis that ordinary Hall changes "substantially with
  local/environmental conditions" on a sub-interval scale is **not demonstrated
  here**, and not refuted either. §8 proposes a bounded extraction to decide it.

---

## 1. What the package contains, and conditions found

- 14 passage CSVs: 9 Otto (XHR, session `C3B93D0B`), 5 Toby (QTRACE, session
  `D7651658`). Each spans ±2000 samples around a recorder anchor at about
  1 kHz.
- **Overlapping windows.** `otto_low_pwm_N`/`otto_mid_pwm_S` overlap by 1.5 s,
  and `otto_high_pwm_S`/`otto_high_pwm_N` overlap by 2.5 s. The same magnets
  appear twice. I merged the overlapping windows by `sample_seq` into
  continuous segments. That gives **12 segments and 28 magnet cores** (2
  partial at window edges), with no double counting.
- **`sources/*.xhr` and `*.qtcap` are Git LFS pointers** (133 bytes), not
  data, on this clone. Only the ±2 s windows were available. See §7.
- `nav_mm` is a per-batch **marker index** (MM50–66 early, MM6–8/MM106–107
  late), not millimetres. It carries no within-interval position.
- Direction: all Otto rows are `FWD` and all Toby rows are `CCW`. Direction
  therefore **cannot be analysed** as a variable (§6).
- Toby rows have 30–40 `late`/`gap_before` samples per window (dt up to 4 ms).
  They were kept unchanged. Otto has no gaps.
- **Repository condition (reported, not changed).** `main` does not contain
  this evidence package. `claude/navi-ps-r1-20q-standards` contains it. That
  branch diverged from `main` at `31d877a`; `main` has 3 policy commits it
  lacks, and it has 497 commits `main` lacks. This report was committed to the
  designated task branch (based on `main`) as a new directory only. No merge or
  rebase was done.

---

## 2. Method: separating the magnetic phenomenon without circular classification

The rule was to exclude *structures*, not *values*. No sample was dropped
because it differed from a presumed level.

1. **Cores.** A core is a contiguous run of |5-sample rolling median − passage
   median| > 60 counts, with a peak of at least 100. The weakest core detected
   here peaks at 157; the source metadata's lowest is 129. The passage median
   is used *only* to find these large excursions. Moving that reference by
   ±15 counts moves core boundaries by ≤ 21 ms in moving passages (typically
   5–10 ms), and by 39 ms in the slow PWM-0 passage. Cores peak at 157–299
   counts, so the location of a core does not depend on the baseline chosen.
2. **Spatial unit W.** W is the core width at half its own peak. It is
   reference-insensitive because the half-max level is ~90 counts from any
   plausible line. W ranges from 86 ms (PWM 90) to 286 ms (PWM 48).
3. **Lobe extent was measured, not assumed.**
   - For each core side with at least 500 ms of neighbouring data, I took the
     41-ms rolling median minus a far level F. F is the median of samples at
     least 400 ms from this core and at least 300 ms from the next core, so it
     is chosen by *position* in the gap.
   - The profiles were pooled in 0.25 W bins (figure `f1_lobe_profiles.png`).
   - Exclusion margins were read from where the pooled median profile stays
     within about 1 count of zero: **Otto** 3.0 W before / 4.0 W after the
     half-max edges, **Toby** 3.5 W before / 3.0 W after.
4. **Sensitivity instead of trimming.** The whole analysis was rerun at 0.5×
   and 1.5× those margins (§5.4).
   - At 0.5×, the "ordinary" statistics pick up spurious slopes and 8-count
     block ranges. That shows the narrow margin still contains lobe.
   - At 1.5× they barely change. The margin was therefore chosen where the
     statistics converge.
5. **What was kept.** Toby's single-sample spikes (9–43 per second, |x −
   med5| > 6), Toby's late samples, and Otto's 100 ms comb were all kept.
   - They are part of the ordinary signal the firmware would see.
   - They are not magnetic structure.
   - Removing them would be value-based classification.
   - Their effect is quantified instead (§4.3).

The result is 22 Otto and 14 Toby clean intervals: 14.0 s and 10.0 s of clean
ordinary track (`figures/f4_segmentation_examples.png`).

---

## 3. The magnetic event is wider than its core

### 3.1 Lobes (per event: `results/lobes.csv`, `results/settle.txt`)

| | leading lobe (counts, opposite sign) | peak at | settled (W) median / p90 / max | trailing lobe | peak at | settled (W) median / p90 / max |
|---|---|---|---|---|---|---|
| **Otto** (n=15/14) | −7 median [−2, −11] | 1.2 W | 2.2 / 2.8 / 4.9 | **−10.5** [−8, −15] | 1.0 W | 2.8 / 3.6 / 4.6 |
| **Toby** (n=10/10) | **−13** [−11, −25] | 0.6 W | 2.6 / 2.9 / 3.2 | −4.5 [−3, −7] | 0.8 W | 2.0 / 2.3 / 2.5 |

"Settled" is the first point past the lobe peak where the one-comb-period
(101-sample) median is within 2 counts of F. That smoothing adds about 0.5 W at
PWM 90, so the values are slightly conservative.

- **The locomotives are asymmetric.** Toby's large lobe is on the *leading*
  side and Otto's is on the *trailing* side. With one direction per locomotive,
  this package cannot say whether the cause is the sensor mount or the
  direction of travel.
- **No polarity difference is visible.** Lobe magnitudes for N and S cores
  overlap on both locomotives (Toby leading: N −11…−25, S −11…−20; Otto
  trailing: N −9…−14, S −8…−16).

### 3.2 Spatial, not temporal (Toby)

| | CV of settle distance in ms | CV in W | corr(settle ms, W) |
|---|---|---|---|
| Toby leading | 0.34 | **0.13** | **0.94** |
| Toby trailing | 0.38 | 0.32 | 0.48 |
| Otto leading | 0.22 | 0.44 | 0.14 |
| Otto trailing | 0.31 | 0.35 | 0.39 |

- **Toby's leading lobe behaves spatially.** It scales with passage width.
- **Otto's settle estimates are dominated by the 100 ms comb and smoothing**
  and do not decide spatial vs temporal. That is **inconclusive**, not evidence
  of temporal behaviour.
- **Consecutive magnet centres are 10.8–13.9 W apart** (median about 12.4 W)
  across a 2.4× range of milliseconds. This independently supports W as a
  spatial ruler.
- **Hedged conversion to distance.** If the physical magnet spacing is known,
  1 W ≈ spacing / 12.4. I did not verify the spacing in mm, so no mm figure is
  asserted here.

### 3.3 Consequence: the clean ordinary track between magnets

- Clean intervals bounded by magnets on both sides are **2.4–6.0 W** long,
  about 350–760 ms at the observed speeds.
- That is roughly **35–45 % of the inter-magnet stretch**, consistent with
  12.4 W spacing minus about 3.5 W of lobe, 3–4 W of trailing lobe, and about
  1 W of core.
- If W is spatial, the clean *distance* per gap is fixed, and the clean
  *sample count* shrinks in proportion to speed.

---

## 4. Ordinary track once the phenomenon is excluded

Full per-interval table: `results/intervals_x1.0.csv`. Summary:

| | Otto (22 intervals) | Toby (14 intervals) |
|---|---|---|
| local medians | 1932–1935 (early passages), 1901–1903 (late) | 1797–1800 |
| MAD | 1–3 | 1 |
| p5–p95 spread | 11–12 with comb; **3–5 without comb** | 4–10 |
| Δ median, last third − first third | −3 … +3 | −2 … +4 |
| Theil–Sen slope | −7.0 … +6.5 counts/s (median 0) | −3.1 … +3.9 counts/s |
| range of 100-sample block medians | 0–4 | 0–6 |
| largest step between adjacent blocks | ≤ 2 | ≤ 3 |
| curvature (max block residual from a line) | ≤ 1.2 | ≤ 3.0 |

### 4.1 Steps and curves

- No short-scale step of more than 3 counts appears in any clean interval.
- The largest within-interval movements are in two Toby intervals:
  - `toby_low_pwm_S` *pre*: rises about 5 counts over 1.4 s.
  - `toby_low_pwm_S` / `toby_low_pwm_N` *post*: rise 5–6 counts at the window
    end.
- The *post* rises are consistent with the leading lobe of a magnet just
  beyond the window edge (right sign, about 2–3 W extent). They could not be
  confirmed because the next core is outside the package (§7).
- The *pre* rise is **unresolved**: it could be real ordinary-track variation
  or a lobe tail longer than 3.5 W.

### 4.2 Proximal window vs surrounding clean behaviour

For every magnet with a clean interval before it, P_N is the median of the
last N clean samples before the lobe exclusion. It was compared with the full
interval median M_pre and the post-magnet clean median Q_post (`results/prox_x1.0.csv`):

| | vs M_pre, p50 / max | vs Q_post, p50 / max |
|---|---|---|
| Otto P30 | 1 / 6 | 1 / 8 |
| Otto P100 | 1 / 2 | 1 / 3 |
| Toby P30 | 1 / 2 | 1 / 2 |
| Toby P100 | 0 / 2 | 1 / 2 |
| M_pre vs Q_post (both) | — | 1 / 2 |

Mean |error| against Q_post:

| | P30 | P100 | P200 | M_pre | held from segment start |
|---|---|---|---|---|---|
| Otto | 1.87 | 1.13 | 1.07 | **0.93** | 1.07 |
| Toby | 0.94 | 0.94 | 0.88 | 0.89 | **0.78** |

Within 4–6.5 s, proximity gives no measurable advantage.

### 4.3 Otto's 100 ms comb (instrument component, not track)

- **Shape:** a +7 to +10-count plateau lasting 20–41 samples out of every 100,
  always ending at `sample_seq % 100 == 0`. That boundary is the X18
  recorder's batch boundary.
- **Not track position:** it is present at PWM 0 (`otto_difficult_no_position`)
  and is independent of speed.
- **Intermittent:** it is absent in 3 of 20 long intervals (`otto_high_pwm_*`
  early section, `otto_difficult_wrong_magnet` 999–1516 ms).
- **Effect:** it biases an interval median by ≤ +1 count. It biases a window
  shorter than 100 samples by up to about +8, depending on phase.
- **Unknown:** whether production (non-recorder) firmware shows this component.
  It may be recorder self-interference. This needs checking before any window
  length is chosen for Otto.

---

## 5. How short and how proximal?

### 5.1 How proximal: bounded by the leading lobe, in distance

(`figures/f3_freeze_lag.png`) Bias of a 100-sample window ending D·W before the
departure proxy (the core start), relative to the clean pre-interval median, in
core-signed counts:

| D (W) | 0 | 0.5 | 1 | 1.5 | 2 | 2.5 | 3 | 3.5 |
|---|---|---|---|---|---|---|---|---|
| Otto median [min, max] | +7.5 [+2, +40] | −1 [−6, +14] | −2 [−4, +1] | −2 [−3, −1] | −1 [−2, 0] | −1 [−2, 0] | −1 [−2, 0] | 0 [−2, +1] |
| Toby median [min, max] | −9 [−22, +4] | −10.8 [−18, −7] | −7 [−9, −3] | −4.2 [−5, −1] | −2.5 [−3, 0] | −1.5 [−2, 0] | −0.5 [−2, 0] | 0 [−2, 0] |

- At D = 0, Otto's window also catches the rising flank of the core below the
  60-count threshold.
- **The most proximal usable ordinary-track sample ends about 2–3 W before
  departure** (Toby 2.5–3 W, Otto 1.5–2 W).
- Because W scales with speed, a fixed time guard would be wrong. At the
  observed speeds, 3 W is about 260 ms at PWM 90 and about 860 ms at PWM 48.

### 5.2 How short: bounded by noise structure, in samples

(`figures/f2_window_length.png`) Sliding test within every clean interval: the
median of the previous N clean samples against the median of the next 100.

| N | 5 | 10 | 20 | 30 | 50 | 75 | **100** | 150 | 200 | 300 | 600 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Otto p95 / max | 8 / 11 | 7.5 / 10.5 | 7 / 10.5 | 7 / 11 | 6 / 8.5 | 5 / 8 | **1 / 2** | 2 / 6 | 1 / 2 | 1 / 2 | 1 / 2 |
| Toby p95 / max | 2 / 10 | 1.5 / 9.5 | 1 / 7.5 | 1 / 4 | **1 / 3** | 1 / 4 | 2 / 4 | 2 / 5 | 2 / 5 | 2 / 6 | 3.3 / 8 |

- **Toby:** about 20–50 clean samples already represent the adjacent
  ordinary-track level to within 1 count (p95). That is 0.2–0.6 W at these
  speeds.
- **Toby, longer windows:** error grows slightly (p95 3.3, max 8 at N = 600).
  This is the only sign in the package that *nearer* is better than *longer*,
  and it is small.
- **Otto:** the answer is "one complete comb period". Windows that are integer
  multiples of 100 samples are best, because the comb is a *time* phenomenon.
- **Available clean evidence:** a gap holds 2.4–6.0 W, or 350–760 samples at
  these speeds. That comfortably exceeds both minimums. If W is spatial, then
  at speeds much higher than PWM 110 the sample count per gap falls
  proportionally. That regime is not in the package.

### 5.3 Answer to the specific question

- **Proximal edge:** a reliable representation of the ordinary track just
  before a magnet can end **no later than about 3 W** (spatial) before
  departure.
- **Length:** it needs **tens of samples on Toby and 100 samples on the X18
  Otto recorder** (a noise and instrument constraint).
- **Measured error:** a reference built this way represents the post-magnet
  ordinary level to **≤ 2–3 counts** in every magnet measured.
- **What the package does not show:** that this reference is better than one
  taken a few seconds earlier.

### 5.4 Sensitivity to the exclusion margins

| margin | Otto clean ms | Toby clean ms | M_pre vs Q_post max | Toby P20 vs M_pre p50/max | Otto 100-block range max |
|---|---|---|---|---|---|
| 0.5× | 21 723 | 14 245 | 3 / 3 | **4.5 / 7** | 8 |
| 1.0× | 14 016 | 10 005 | 2 / 2 | 0.8 / 2 | 4 |
| 1.5× | 6 518 | 6 045 | 3 / 3 | 1 / 2 | 4 |

Halving the margins lets the lobes leak into "ordinary track". Enlarging them
changes nothing material.

---

## 6. Differences by locomotive, polarity, PWM/speed and direction

- **Locomotive.**
  - Levels differ: about 1934/1902 (Otto) against about 1799 (Toby).
  - Noise differs: Otto has the batch comb; Toby has single-sample spikes and
    late samples.
  - Lobe asymmetry is opposite (§3.1).
  - Any RTB or reference rule would need per-locomotive geometry, or lobe
    margins that cover both.
- **Polarity.**
  - Lobes are opposite-sign to the core on both sides for both N and S, with
    overlapping magnitudes.
  - The ordinary level does not depend on which polarity comes next.
  - One consequence: a reference contaminated by the leading lobe
    *exaggerates* the apparent departure (the lobe is opposite to the core),
    and it sets the return target on the wrong side of the line.
- **PWM/speed.**
  - W scales with speed (86–286 ms). Lobe extent tracks W.
  - The ordinary level shows no PWM dependence above about 2 counts. Otto late
    passages read 1902–1903 at PWM 0 and 1901–1902 at PWM 90. Early passages
    read 1932–1935 across PWM 39–110.
  - This is confounded with position and time.
  - `otto_difficult_no_position` shows a complete 638 ms magnet passage with
    **PWM 0 for the whole window**. PWM therefore does not indicate motion.
- **Direction.** Not analysable here: one direction per locomotive in the CSVs.

---

## 7. Counterexamples and limitations

**Counterexamples.**

1. **The leading lobe was ruled an event.** In `toby_difficult_disagree`, the
   source `DISAGREE` event (S, peak 39, 53 ms) coincides with the −25-count
   *leading lobe* of the N core at 2070 ms. The pre-magnet fringe is large
   enough to be detected as a magnet in its own right.
2. **A proximal window drifts into the lobe.** In `otto_difficult_wrong_magnet`
   (bounded interval before the N core), the slope is −7 counts/s and P100 is
   1899, against M_pre 1901 and Q_post 1902. The Otto 3 W leading margin was
   marginal here. This proximal window was 3 counts worse than the interval
   median.
3. **Short windows land on the comb.** In `otto_high_pwm_N` (N → N) and
   `otto_difficult_not_a_magnet` (first S), P30 is 1939 and 1909, against
   interval medians of 1933 and 1903. A short window ending on the plateau is
   6 counts high.
4. **Ordinary level moves within an interval.** In `toby_low_pwm_S` *pre*, the
   level rises about 5 counts over 1.4 s. It is unresolved whether this is
   ordinary track or a long lobe tail. Here the proximal window matches the
   post level better than the early part of the interval does.
5. **The level shifts across the session.** Otto's ordinary level is about 33
   counts lower in the late passages. Held references fail by 31–34 counts.
   - The prior repository analysis `docs/NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md`
     records a −32-count step during a 79 s stationary period at t ≈ 2273 s,
     and CW/CCW blocks at 1931–36 and 1899–1904.
   - That is *context* only. The CSVs alone cannot separate place, time, loop
     direction and a handling event.

**Limitations.**

- **Window length.** Each window is ±2 s, and only 12 continuous segments
  exist (≤ 6.5 s each). Variation between about 6 s and about 40 min is
  **unobserved**. This is the scale that decides "proximal vs periodically
  refreshed".
- **Session coverage.** One Otto session and one Toby session, both in
  evening or stable conditions. Morning and thermal behaviour are untested.
- **Missing source binaries.** The source binaries are LFS pointers in this
  clone. Windows could not be extended, and the magnets just outside the
  windows could not be seen.
- **Small counts.** n = 17 Otto and 10 Toby pre-magnet windows; 15 and 9 with
  a post interval.
- **Margins are data-derived.** They come from this same small sample. The
  0.5×/1.5× sensitivity bounds their effect but does not validate them on
  other track.
- **The comb's origin is inferred** from its phase-lock with recorder batches.
  It is not confirmed on production firmware.
- **All claims here are about the recorded signal**, not about physical
  railway behaviour beyond it (AGENTS.md §8).

---

## 8. Concept comparison

| | Proximal continuously-learned reference | Acquired-and-held baseline |
|---|---|---|
| Within seconds (this package) | ≤ 2–3 counts, but only if the reference excludes about 3 W before departure | ≤ 2 counts. Indistinguishable. |
| Across the session (Otto early → late) | tracks 1934 → 1902 | wrong by 31–34 counts |
| Leading-lobe exposure | high if "freeze at departure" is literal (−9 median, −22 worst) | none, if acquired in a clean interval |
| Needs | lobe-aware lag (spatial, about 3 W), ≥ 100 samples on X18 Otto, clean gap ≥ that | a re-acquisition trigger for level changes the package shows can reach 33 counts |

**Evidence-supported statements.**

- A reference that is refreshed beats one held for a session.
- The refresh must come from ordinary track separated from the magnet by its
  lobes on both sides.
- "RTB demonstrated" must include the trailing lobe, about 3–4 W for Otto and
  about 2–2.5 W for Toby, before the level can be compared.

**Not decided by this evidence.** Whether per-interval proximity (seconds)
matters more than refreshing every few intervals. The data show no difference.

**Recommendation (not a decision).** A bounded next step would settle the open
scale question:

1. Fetch the LFS sources.
2. Apply this same segmentation to the whole `C3B93D0B` session: 3537 s,
   about 1550 magnets.
3. Produce the series of clean-interval medians against time and against MM.
   This yields variation at every scale from one interval to the full hour,
   and separates place (same MM on successive laps) from time.

It needs the LFS objects and moderate compute. I have not done it without your
approval.

---

## Files

- `scripts/seg.py` — core detection and W. `segs.py` — merges overlapping
  windows. `settle.py` — lobe profiles. `clean.py` — exclusion and interval
  statistics. `prox.py` — proximal, sliding and freeze-lag tests.
  `figures.py` — figures.
- `results/` — every table above as CSV/TXT, for margins 0.5×, 1.0× and 1.5×.
- `figures/` — f1 lobe profiles, f2 window length, f3 freeze lag,
  f4 segmentation examples.
