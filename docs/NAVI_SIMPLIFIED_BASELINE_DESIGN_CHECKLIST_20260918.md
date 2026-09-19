# Baseline design checklist — answered from existing evidence

Date: 2026-09-18
Scope: answers the operator's 14-question baseline checklist for a
magnet-only NAVI_SIMPLIFIED Hall baseline, using evidence already in the
repo. **No new firmware and no new field capture.** Every number below is
either a direct citation of prior analysis or a direct citation of the
current shipped mechanism (`HallCapture.h`); nothing here is a fresh
measurement. Where evidence is thin or missing, that is stated rather than
filled in with a guess.

Two designs appear in the sources and are kept distinct throughout:

- **Shipped (NAVI_ONE)**: `firmware/test-programs/NAVI_ONE/HallCapture.h` —
  a continuously-adapting rolling median, motion-gated on `actualPwm` (decision 0017).
- **Candidate (analysed for NAVI_SIMPLIFIED, not implemented)**: the
  per-interval collect-after-close state machine developed across
  [NAVI_BASELINE_TIMING](NAVI_BASELINE_TIMING_20260916_C3B93D0B.md),
  [NAVI_BASELINE_DRIFT](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md),
  [NAVI_BASELINE_ELIGIBILITY](NAVI_BASELINE_ELIGIBILITY_20260916_C3B93D0B.md),
  [NAVI_BASELINE_PWM_SAMPLING_GATE](NAVI_BASELINE_PWM_SAMPLING_GATE_20260916_C3B93D0B.md),
  [NAVI_BASELINE_STOP_RECOVERY](NAVI_BASELINE_STOP_RECOVERY_20260916_C3B93D0B.md).
  All from one 59-minute recording, Otto, `xhr_20260916_191904.xhr` session
  `C3B93D0B`, 1 kHz, no gaps, 1,547 magnets.

---

## 1. What physically constitutes baseline?

Baseline is the raw 12-bit ADC reading (GPIO 33, Hall sensor) on ordinary
track, away from any MM magnet's field. Measured properties:

| quantity | value | source |
|---|---|---|
| stationary sensor noise (motor off) | **≤15 counts** excursion in 90 s, no floor rejections | WHAT_THE_HALL_SENSOR_SEES §1 |
| stationary, motor turning at PWM 90 | **zero** measurable excursion in 100 s — H-bridge/commutation inject nothing | WHAT_THE_HALL_SENSOR_SEES §1 |
| RMS noise, moving | **4.1–4.88 counts** (magnet-arc jitter is the same 4.88, i.e. sensor noise, not field noise) | WHAT_THE_HALL_SENSOR_SEES §2 |
| line-to-line noise while running, peak-to-peak | **±10 to ±17 counts** | NAVI_BASELINE_TIMING §1 |
| level variation **around the loop** (position, not time) | up to **47 counts** | NAVI_BASELINE_DRIFT §1 |
| level variation **over time at one place**, one hour | **<6 counts** (1931–1936 CW, 1899–1904 CCW) | NAVI_BASELINE_TIMING §0 |
| fastest **clean** measured drift (motion, whole loop averaged) | **+1.30 counts/min**, R²=0.88, 22 min | NAVI_BASELINE_DRIFT §1 |
| typical drift | **<0.17 counts/min** | NAVI_BASELINE_TIMING §0 |
| measured **steps** (always from stops/handling, never mid-run) | −32 counts (79 s stop, handling event), −20 counts (one session) | NAVI_BASELINE_DRIFT §1 |
| morning warm-up drift | **no record found** — every usable history is midday or later | NAVI_BASELINE_DRIFT §1 |

**Formula in current use (both designs):** baseline is a scalar `B`, in raw
ADC counts, estimated as `median(N recent samples)` — 41 samples at 25 ms
cadence (~1 s window) in the shipped continuous median, or 200 samples at
1 ms cadence (200 ms window) in the candidate per-interval design.

## 2. Where physically do we measure it?

**Definition of the usable region, from the magnet's own signal, not a
fixed distance:**

```
|<-- magnet body -->|<- 30 ms close ->|<- 80 ms guard ->|<--- usable --->|<- 120-150 ms clearance -->|next magnet|
```

- The magnet occupies **~14% of a marker interval** at cruise (167 ms body
  in a ~1,200 ms interval) — WHAT_THE_HALL_SENSOR_SEES §1.
- **Trailing clearance required**: the signal does not decay smoothly after
  the ≥70-count span closes — it sits on an opposite-sign **shelf of
  15–18 counts** for a further 100–200 ms (worst case MM90: −15 to −17
  counts, 140–300 ms after start). A **guard of 80 ms after span-close**
  is the recommended clearance (see §7 for why 80, not 40) —
  NAVI_BASELINE_TIMING §1, §3; NAVI_BASELINE_DRIFT §7.
- **Leading clearance required**: the approaching magnet has its own
  **leading lobe** — the line is last quiet within ±15 counts **70 ms
  (p99), 82 ms (max)** before the next 70-count departure, and within
  ±10 counts **118 ms (p99), 144 ms (max)** before it. A window run right
  up to the next departure is biased *toward* it, the worst direction.
  NAVI_BASELINE_TIMING §3.
- **Length sampled**: 200 ms (200 samples at 1 kHz).
- **Where the sample sits in the interval**: as early as possible after the
  80 ms guard, not late — pushing it later buys almost nothing (bias curve
  is flat past ~300 ms) and risks the next magnet's leading lobe. The
  narrowest safe band in the whole hour of ordinary running is **405 ms
  wide** for a 200 ms window, so timing is not tight in practice.

**Physical coverage of the 200 ms window, measured, by phase** (this is
where the operator's spatial-vs-temporal question surfaces directly):

| phase | n | median mm covered | min mm | note |
|---|---:|---:|---:|---|
| IDLE (cruise) | 1326 | **51** | 1 | typical open running |
| APPROACH | 78 | 47 | 28 | |
| ZONE | 37 | 28 | 21 | |
| **DEPART** | **13** | **1** | **1** | pulling away from a stop at a few mm/s |

NAVI_BASELINE_DRIFT §5. **A fixed 200 ms window covers 300× more track at
cruise (51 mm) than during a slow departure (1 mm).** This is measured, not
hypothesized.

## 3. How is that physical region sampled?

**Currently proposed/analysed (candidate):**

| parameter | value |
|---|---|
| samples collected | **200** |
| sampling rate | **1 kHz** (1 ms/sample; capture integrity confirmed: mean tick 1000.000 µs, 0.002% of samples >5 ms late) |
| physical spacing between samples | **variable** — 51 mm ÷ 200 ≈ 0.26 mm/sample at cruise; ~1 mm ÷ 200 ≈ 0.005 mm/sample during DEPART |
| estimator | **median** of the 200 raw samples |
| outlier handling | not per-sample — see §6 |

**Your hypothesis (same number of samples over the same physical track
length, regardless of speed) is not what either design currently does, and
the DEPART-phase row above is the direct evidence that the time-fixed
version breaks down exactly where your hypothesis would fix it.** No
distance-triggered (spatially-fixed) baseline window has been built or
tested anywhere in this search. This is a real gap, not a settled
"already tried and rejected" — the closest existing result is that a
**distance-based window was tested for a different purpose** (predicting
*when* to start sampling, §4 below) and lost to the magnet's-own-span
predictor on accuracy, but that comparison did not test sampling a fixed
*physical length* once started. Whether a spatially-fixed window would beat
a time-fixed one for the *estimate itself* — as opposed to for locating the
window — is untested and stated here as an open question, not a rejected one.

**Shipped (NAVI_ONE), for comparison:** a rolling median of the last 41
samples, refreshed every 25 ms (≈1 s window), continuously — not
per-interval, and not gated to a clean region at all except by the motion
gate (§8). This is a materially different design from the candidate and
does not answer the spatial-sampling question either.

## 4. How does the locomotive know it is sampling that region?

**Measured answer: it does not need mapped distance, PWM, or a speed
estimate to locate the window.** Four candidate predictors of the earliest
safe start-offset were tested head-to-head (NAVI_BASELINE_TIMING §4):

| predictor | coefficient of variation | needs speed? | needs route map? |
|---|---:|---|---|
| (a) fixed time after previous magnet's start | 0.397 | no | no |
| (b) fraction of prior marker interval | 0.183 | implicitly | no |
| (c) fixed travel distance | 0.177 | **yes** | **yes** |
| **(d) the magnet's own ≥70-count span** | **0.147 — best** | **no** | **no** |

**(d) wins.** The magnet's own span *is* a speed measurement (a faster
locomotive produces a shorter span), self-measured on the very magnet just
crossed. The rule anchors on `span_end + 80 ms`, a purely self-observed
quantity — no `RouteMap.h`, no PWM read, no calibrated speed table, no
acceleration/deceleration integration is used anywhere in this candidate
design to *locate* the window.

The one place elapsed time *is* used is the **motion/cadence gate** (§8),
which compares the raw interval between successive magnet-open events to a
fixed **3,000 ms** threshold — again a directly-measured quantity, not a
derived one.

**Standard values, all self-observed, no calculation required:**

| constant | value |
|---|---|
| guard after span close | 80 ms |
| samples collected | 200 (≈200 ms at 1 kHz) |
| motion/cadence gate | prior marker interval ≤ 3,000 ms |

Where PWM/speed/distance/acceleration *would* enter, per the companion
[distance evidence inventory](NAVI_SIMPLIFIED_DISTANCE_EVIDENCE_INVENTORY_20260918.md),
is only if a spatially-fixed window (§3) is built later — see §12.

## 5. How are MM transients excluded?

Two separate exclusions, both with numeric bases:

**Previous magnet's trailing field (the shelf).** Opposite-sign shelf of
15–18 counts persists 100–200 ms past span-close (worst case MM90: −15 to
−17 counts through 300 ms). An **80 ms guard** after close, followed by the
200-sample collection with a **spread (max−min) ≤ 32 counts** acceptance
test, is what actually excludes it — the guard alone does not fully clear
the shelf (residual bias is still visible at 200 ms, per the bias-vs-offset
table in NAVI_BASELINE_TIMING §3), so the spread test does the rest of the
work by rejecting any window whose contents still show excess spread.

**Approaching magnet's leading field (the lobe).** Quiet only within
±10 counts up to 118 ms (p99)/144 ms (max) before the next departure.
**Structural exclusion**: the collection is triggered only after the
*previous* magnet closes and completes well before the next one opens — the
narrowest safe band anywhere in ordinary running is 505 ms (100 ms window)
/ 405 ms (200 ms window), so under normal operation the leading lobe is
never reached. No dedicated "stop N ms before the next expected magnet"
logic exists or is needed, because the collection is short relative to the
band.

**Physical clearance in distance, not just time.** The trailing tail is
better described as a **fixed distance than a fixed time**: tail-as-distance
has cv = 0.093 versus tail-as-time cv = 0.142 (corr(tail, speed) = −0.76).
Measured tail length: **median 36 mm, p95 40 mm, p99 45 mm, max 71 mm**
(NAVI_BASELINE_TIMING §4). This is a second, independent piece of evidence
for treating parts of this problem spatially rather than temporally,
consistent with the framing in your question 3.

**Structural protection against a magnet directly under the sensor:** the
detector state machine never starts a collection while `|dev| ≥ 70` —
a locomotive parked on or near a magnet holds the departure OPEN
indefinitely, so no baseline collection can begin there at all, regardless
of timing. This was confirmed at five real station dwells where the
locomotive rested on a magnet at +130 to +230 counts and zero baseline
contamination resulted (NAVI_BASELINE_STOP_RECOVERY §1).

## 6. How is a candidate baseline calculated?

```
median of 200 raw samples, collected consecutively at 1 kHz
starting 80 ms after the previous magnet's ≥70-count span closes.

accept  if max(sample) - min(sample) <= 32 counts  ->  baseline = median
reject  otherwise                                   ->  keep the old baseline
```

- **Sample count: 200.** This was tuned empirically and is the single most
  load-bearing constant in the design (NAVI_BASELINE_TIMING §5):

  | N | locks | rejected | p99 \|err\| | **worst \|err\|** | locks > 20 counts off |
  |---:|---:|---:|---:|---:|---:|
  | 20 | 1554 | 25 | 36.2 | 86.0 | 63 |
  | 40 | 1549 | 12 | 26.0 | 67.0 | 27 |
  | 100 | 1491 | 59 | 14.0 | 51.6 | 3 |
  | **200** | **1455** | **95** | **11.5** | **12.0** | **0** |
  | 250 | 1449 | 101 | 10.0 | 11.0 | 0 |

  200 is not merely a better estimator than 100 — a window contaminated
  enough to produce a 50-count error at N=100 spans more than the 32-count
  spread limit when extended to 200, so it is caught instead of accepted.
  N=250 buys nothing more.

- **Outlier/transient treatment: whole-window rejection, not per-sample
  filtering.** There is no per-sample outlier exclusion inside the 200-sample
  set (e.g. trimming, robust regression) anywhere in the evidence. The
  single spread test (max − min ≤ 32) either accepts the whole window as-is
  (median of the untouched 200 samples) or discards the whole window and
  retains the previous baseline. This is a deliberate simplicity choice, not
  an oversight — see §7 for its measured performance.

## 7. How is validity determined? Numerical limits and failure handling

| acceptance test | limit | basis |
|---|---|---|
| spread (max − min) of the 200-sample window | **≤ 32 counts** | with an 80 ms guard, 32 costs zero locks and catches step contamination that 40 does not (NAVI_BASELINE_DRIFT §7) |
| motion / cadence requirement | prior marker interval **≤ 3,000 ms** | removes a measured 41-count stationary-lock hazard (NAVI_BASELINE_DRIFT §6); the gate value is flat 2,500–6,000 ms, so 3,000 is not a fine-tuned edge |
| relationship to the **previous accepted baseline** | **none found** — no delta-vs-previous-baseline gate exists in any design examined | acceptance is purely a property of internal spread within the new candidate window, not a comparison to history |
| structural contamination protection | `\|dev\| ≥ 70` never allows a collection to start | five real station dwells at +130 to +230 counts produced zero contaminated collections |
| **on failure** | **reject, keep the old baseline** — never partial-accept, never blend | uniform across every version examined |

**Measured acceptance rate (recommended constants: guard 80 ms, spread 32,
motion gate 3,000 ms):** **1,512 locks / 38 rejected** out of 1,550
candidate intervals — **97.5%** — with **max |error| 13 counts** against
the held reference line (NAVI_BASELINE_DRIFT §8). Worst individual locks
are all attributable to the trailing shelf itself (the physics of the
magnet), not to a design or motion failure.

**Validation is explicitly distinguished from later use** per your
question: the spread test is the only gate the *candidate* baseline itself
must pass; once accepted, it is simply substituted as the new operative `B`
with no further check (see §9, §10).

## 8. How often is baseline measured?

**Attempted every qualifying marker interval** — not periodic-by-clock and
not purely on-demand. Concretely: every time a magnet closes, a fresh
200-sample collection is attempted (subject to the §7 gates); this is as
frequent as physically possible, once per magnet, in both directions.

| condition | behavior |
|---|---|
| through an ordinary interval | attempted, ~97.5% accepted (§7) |
| through a **station** (dwell, or resting on a magnet) | **frozen** — the ≥70-count span never closes while parked on/near a magnet, so no collection starts; five real dwells at +130 to +230 counts confirm this structurally, not by luck (NAVI_BASELINE_STOP_RECOVERY §1) |
| through a **stop not on a magnet** | frozen by the motion/cadence gate (prior interval > 3,000 ms) |
| **restart / boot** | primed once from a **2 s median** of raw samples while stationary (`primeMs = 2000`, exempt from the motion gate because there is no first reference otherwise) — `HallCapture.h` |
| **direction change** | **untested** — the analysed session ran forward 100% of the time, zero reversals. Clearing the cadence qualification on a direction change was assessed as "cheap and principled... free on this recording (it never fires)" but carries no measured evidence either way (NAVI_BASELINE_ELIGIBILITY §5) |
| **extended period with no new valid baseline** | median carried-baseline age **1,197 ms (p50) / 4,662 ms (max)** in the recommended-rule replay, cost nothing because real drift was <6 counts/36 min in that recording. **Failure threshold is quantified**: staleness of **60 counts** is where a magnet first starts to be missed; **70 counts** is a hard cliff (the frozen baseline itself reads as a departure on clean track, the detector chatters, and it can never re-close to start a fresh collection) (NAVI_BASELINE_STOP_RECOVERY §2). At the measured maximum real drift rate (1.19 counts/min) reaching 60 counts needs **~50 minutes standing still**; the longest stop in evidence was 6.8 minutes reaching 37 counts (a handling event, not drift). |

**A tested recovery for the 70-count cliff** (candidate, not implemented):
a stale-baseline timer (10 s with no accepted lock) armed by **two fresh
"excursions"** — the signal breaking out of a 40-count quiet band, with
quiet-run length ≤2,500 ms and trailing-3 s peak-to-peak ≥100 counts —
recovers **17 of 17** injected lockouts and re-primes onto a parked magnet
**zero** times, because a locomotive at rest produces no further excursions
however recently it was moving (parked quiet-run p5 = 3,832 ms vs. running
max = 2,337 ms) — NAVI_BASELINE_STOP_RECOVERY §3–5. This mechanism has not
been field-validated against a real 60+ count stale event, because none has
been recorded.

## 9. What is the new candidate baseline compared with?

**For validation (§7):** only its own internal spread — `max − min` of the
200 samples just collected. **Not** compared against the previous accepted
baseline, a per-marker expected range, or any history, in any design
examined.

**Closest thing to a history-aware check** is the stop-recovery mechanism's
"fresh excursion" pattern (§8), which implicitly relies on recent signal
history (quiet-run duration distribution) rather than a direct baseline
value comparison — it decides *whether conditions permit re-priming*, not
whether the resulting median is itself plausible against expectation.

**No per-marker expected-baseline table was found.** This differs sharply
from `strengthPct[]`/`durationMs90[]`, which are explicitly per-marker,
per-direction expectation tables (decision 0048). Baseline is treated
throughout as one scalar tracked over time and (implicitly) location, never
as a per-marker expected value with its own tolerance band. Whether it
should be is not addressed anywhere in the searched evidence — worth
flagging as an open design question, not a settled "no."

**For later use in detection (§10):** the accepted baseline simply becomes
the new operative `B` for every subsequent sample until the next accepted
lock. There is no separate "is this baseline still good enough to detect
with" check beyond the staleness/cliff behavior of §8.

## 10. How does baseline relate to the next magnet measurement?

**D = H − B**, sign retained; detection is on `|D|`, exactly as posed.

- **Where this belongs:** in the capture/detection layer, computed on
  every 1 ms sample — in the shipped design this is
  `HallCapture::sample()`, line `const int32_t delta = (int32_t)raw - baseline_;`
  followed by `mag = |delta|`. The candidate design's `dev = raw - baseline`
  is the identical calculation. **This is where D belongs in both designs
  already** — it is not something to relocate, only to confirm.
- **Thresholds, both designs:** entry ≥70 counts sustained 5 samples (open),
  exit <70 held 30 ms (candidate) or exit <25 held 8 ms (shipped, with
  hysteresis specifically to avoid one magnet producing several passages).
- **Speed-dependent effects on the interpretation of D:**
  - **Magnitude/polarity: no correction needed.** Peak amplitude does not
    decay with speed across the measured range (peak p50 236–255 counts
    from 0–1,049 mm/s, HALL_SELECTIVITY §3) — `|D|` at peak means the same
    thing at any speed measured.
  - **Apparent width is speed-dependent and already barred from use.**
    `hallms × v ≈ a·v + b` (a=19.1 ms detector overhead, b=34.4 mm true
    magnet width) has 21% residual and rises from 35.3 mm at low speed to
    48.5 mm at 800–900 mm/s (HALL_SELECTIVITY §3) — this is a property of
    *when* D crosses threshold, not of D's magnitude, and decision 0074/0065
    already keep morphology (including width) out of navigation authority.
  - **Net effect on D's interpretation: none required.** The only
    speed-dependent quantity that matters for baseline purposes is *when*
    a fresh sample may be taken (§2, §5), not how `D = H − B` itself should
    be read once B is valid.

## 11. Does baseline itself vary with locomotive speed?

**No dedicated same-location, multi-speed, same-session sweep was found.**
What exists:

- **Motor/drive electronics inject nothing measurable.** Stationary with
  the motor turning at PWM 90 for 100 s: **zero** excursions
  (WHAT_THE_HALL_SENSOR_SEES §1) — rules out a speed-correlated electrical
  offset from the H-bridge or commutation.
- **Ordinary-track noise band is stable across the (implied) range of
  speeds in a full-loop lap**: ±10–17 counts peak-to-peak throughout a
  240 s, 171/171-marker lap that included the full range of PWM the
  locomotive ran that day (WHAT_THE_HALL_SENSOR_SEES §1, NAVI_BASELINE_TIMING
  §1).
- **What is confirmed to change with speed** is not the baseline level but
  (a) the physical *sampling region* size at fixed sample-time (§2, §3), and
  (b) the magnet's own apparent width/tail length in time, though the tail
  is closer to constant when expressed as a distance (§5).
- **What was observed to change the level** was **location** (up to 47
  counts around the loop) and **discrete handling/stop events** (−32, −20
  counts) — never a smooth function of speed in any record found.

**Conclusion: no evidence found supports a speed-dependent baseline level
on ordinary track.** This is an absence-of-effect within what has been
measured (one stationary-vs-driven test, one full lap), not a controlled
sweep at several held speeds at one fixed point — that experiment has not
been run and would be needed to fully close this question.

## 12. How is speed estimated when baseline acquisition needs it?

**For the recommended design, it is not needed** — §4 already established
that the winning window-placement rule (the magnet's own span) requires
neither a speed estimate nor `RouteMap.h`. This directly satisfies "we
should not build more sophistication than the baseline problem requires."

**If a spatially-fixed sampling window is pursued** (§3's open question),
speed *would* become necessary to convert a target physical length into a
sample count or duration. In that event, the existing evidence base to draw
on (not built here, cited from the companion
[distance evidence inventory](NAVI_SIMPLIFIED_DISTANCE_EVIDENCE_INVENTORY_20260918.md)):

- **Constant-PWM speed is directly measured** at a dense set of PWM values
  (including exactly 60, 90, 110, 120) from the 2026-06-30 Otto calibration,
  with direction- and location-dependent variation of 9–15% at low-mid PWM.
- **Acceleration/deceleration integration is not currently supported by any
  telemetry found.** The finest resolution anywhere is one leg-average speed
  per ~300 mm marker crossing; sub-marker v(t) cannot be reconstructed, and
  leg-averages during a PWM ramp are shown to mask real within-leg velocity
  change (companion report §4, §6, §8). A spatially-fixed baseline window
  acquired *during* a PWM transition would therefore need either (a) to be
  restricted to held-PWM, non-transitioning intervals, or (b) to accept the
  same leg-average approximation error already documented for distance
  accounting generally.
- Recommendation carried over from that report: build only what the
  baseline problem requires, and prefer the self-timed span-based approach
  of §4 unless the spatial-coverage problem at DEPART (§2) is judged to need
  fixing specifically.

## 13. How is the accepted baseline communicated and owned?

From the shipped implementation, `HallCapture.h` (the pattern is identical
in the analysed candidate designs, which describe the same single-scalar
ownership without naming a class):

| value | role | owner |
|---|---|---|
| `baseline_` | **LIVE** reference — decides only whether a passage is OPEN or CLOSED; permitted to migrate under an open passage (gated by `openMigrateMs`) so a stale offset cannot hold a false passage open forever | `HallCapture` |
| `entryBaseline_` | **FROZEN** at the instant a passage opens; every stored sample — and therefore polarity, peak, amplitude — is measured against this, never against the moving live reference | `HallCapture` |
| `shadowBaseline_` | the same rolling median, kept running even when `fixedAfterPrime` holds `baseline_` static, for observability (field-test policy only) | `HallCapture` |

**Accessors**: `baseline()`, `shadowBaseline()`, `entryBaseline()`,
`ready()` (primed flag). **What accompanies the value**: a primed/not-primed
boolean and (via the `Passage` struct handed off at close) the
`entryBaseline` used for that specific passage, plus timestamps
(`openedAtMs`, `closedAtMs`). **No explicit uncertainty/confidence value is
attached to the accepted baseline in any design examined** — acceptance is
binary (passed the spread test or did not); there is no propagated error
bar. This is a genuine gap against your question's framing ("value,
validity, timestamp/event identity, perhaps uncertainty") — value, validity
(implicitly, via `primed_`) and event identity (via the `Passage`'s
timestamps) all exist; uncertainty does not.

**How the detector receives it:** the detector *is* the same object —
`baseline_` is read in the same `sample()` call that computes `dev = raw -
baseline_`. There is no separate transport or message between "baseline
owner" and "Hall detector" in the shipped design; they are the same class
by construction. Whether NAVI_SIMPLIFIED should keep that coupling or split
it into separate components is a design choice this evidence does not
settle either way — it documents what exists, not what NAVI_SIMPLIFIED must do.

## 14. What program component compares Hall readings with baseline?

**The same component that owns the baseline: `HallCapture`.** Its own file
header states the boundary explicitly and this matches the checklist's
target model exactly:

> "It produces Passages. It makes NO judgement about whether one is a
> magnet: that is the recognizer's job, and this layer must not pre-empt
> it."

Concretely, in the shipped design:

1. **`HallCapture`** — owns `baseline_`/`entryBaseline_`, computes
   `dev = raw − baseline_` every sample, applies the entry/exit thresholds,
   and emits a `Passage` (raw samples, polarity, peak, timestamps) or a
   `FloorRejection` (an acquisition transient, deliberately kept out of the
   `Passage` path so it cannot reach the recognizer or NAVI by accident).
2. **`MagnetRecognizer`** — receives only completed `Passage` objects, never
   raw Hall values and never the baseline itself. It judges whether a
   passage is a genuine magnet (morphology, shape, per-locomotive
   constants — decision 0079).
3. **NAVI** — receives only the recognizer's output (a sensor event: which
   marker, which polarity, when). It never sees raw Hall counts or the
   baseline value.

**No component was found that quietly reinterprets baseline for another
purpose.** The pole, in particular, is explicitly *not* decided from the
entry sample or from `RouteMap.h` — "the map may not be consulted, or a
mis-latched passage would simply be told what it ought to have been and the
instrument would stop being an instrument" (`HallCapture.h`, `close()`).
This is the same discipline your question asks for, already enforced in the
shipped code; the open question is only whether the *candidate*
NAVI_SIMPLIFIED per-interval design, when implemented, preserves the same
three-way split or collapses baseline-ownership and detection into a
simpler single state machine (its own descriptions suggest the latter — one
`baseline` variable, no separate class named) — which would still satisfy
the ownership boundary as long as the recognizer and NAVI still receive
only a `Passage`-equivalent event, never the raw value or the baseline.

---

## Where evidence is thin, stated plainly

- **Morning warm-up drift**: no record found anywhere that combines morning
  hours with motion.
- **Direction changes**: the entire 59-minute analysed session ran forward;
  zero reversals to learn from.
- **CCW at the same confidence as CW**: 191 CCW intervals against 1,355 CW
  in the primary session.
- **Other locomotives, other Hall mounts**: everything in §1–§12 is one
  locomotive (Otto), one Hall sensor, one evening (plus the separate
  2026-08-28 Toby survey cited in §1/§5/§6/§10, which is a different
  session and not merged into the numeric tables above).
- **A real 60+ count stationary staleness event**: never recorded; the
  60-/70-count failure thresholds and the stop-recovery mechanism's 17-of-17
  result are from *injected* offsets on real timing, not an observed failure.
- **Baseline uncertainty/confidence propagation**: not implemented or
  proposed anywhere examined (§13).
- **A spatially-fixed (constant-track-length) sampling window**: not built
  or tested anywhere (§3) — the DEPART-phase 1 mm finding is the strongest
  existing argument for trying it, not a report that it has been tried.

*Read-only synthesis of existing analysis and shipped code. No firmware
changed, no new field data collected, no NAVI_SIMPLIFIED design decided.*
