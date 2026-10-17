# NAVI_Rear_View_Window M0: independent mathematical review

**17 October 2026 · Claude (Opus 5.5) · Review only**

Reviewed: [`NAVI_Rear_View_Window_Mathematical_Specification_M0.md`](NAVI_Rear_View_Window_Mathematical_Specification_M0.md)
at `f9347fb`, read with its companion
[`NAVI_Rear_View_Window_Architecture.md`](NAVI_Rear_View_Window_Architecture.md).
Grounding facts come from
[`NAVI_RVM_REPLAY_VALIDATION_20261016.md`](NAVI_RVM_REPLAY_VALIDATION_20261016.md),
which is on `main`.

> **Status.** This review changes no firmware, map, governing architecture,
> service or device. Its alternative equations are candidates, just as M0's
> are. Every item in §1 is labelled either **[OPERATOR]** (a requirement
> recorded in the architecture document) or **[MATH]** (an unvalidated
> mathematical choice). The architecture document calls itself "proposed".
> Per the operator's 2026-08-30 ruling, nothing in it has force until he
> ratifies it, so **[OPERATOR]** here means "recorded as the operator's
> answer". It does not mean "ratified".

---

## 0. Answer first

M0 is honest about what it does not know, and most of its constraints are
sound. Five problems are structural. More tuning will not fix them. They
need either a mathematical decision or an operator decision.

1. **Tokens are irreversible, but the estimator is not (§3.1).** A DP or
   DTW frontier revises its best path when later data arrives. The
   controller cannot take back a token it has already executed. M0 never
   defines the rule that turns a revisable frontier into a final token. That
   **commit rule is the real recognizer**, and it is unspecified.
2. **Recognizing that the baseline (BL) has returned has a lower bound on
   latency, and the filter is not what sets it (§3.2).** A causal
   recognizer cannot tell "BL has returned" from "a flat lobe inside the
   same ensemble" until it has travelled past the longest flat lobe that can
   occur inside an ensemble, `d_flat_max`. The 20–50 ms filter adds only
   about 0.8σ. The flat-lobe research (architecture item 12) therefore
   already answers M0's "most important unresolved question", given speed:
   token delay ≥ `d_flat_max / v`.
3. **Firing on completion moves the trigger point, and that conflicts with
   "preserve station landmarks" (§3.3).** Existing landmarks fire on the
   Hall opening. A completion token fires about one ensemble width
   (74–149 mm, median 92 mm) plus `d_flat_max` later. Keeping the stop
   routines unchanged means stopping later than today unless the landmarks
   move.
4. **Recorded rules can make a one-MM slip unidentifiable (§2.3).** IR has
   no veto, PWM has no minimum distance, PWM-0 has no authority, and a
   missing ensemble is not a STOP. Together these allow two hypotheses to
   explain an absent ensemble equally well: "ensemble j is missing" and "the
   train moved one spacing less". Only the distinctiveness of the landscape
   itself can separate them. That distinctiveness has to be measured
   (proposed self-ambiguity profile, §5.4). It cannot be assumed.
5. **IR's weight depends on agreeing with Hall (§4.3).** `gamma_i` depends
   on "agreement with independently established broad progression", which
   in practice is the Hall alignment. IR therefore counts most when it
   agrees with Hall and least when it disagrees. Those are the cases where
   Hall is weak, missing or wrong, which is when IR is needed. This is a
   feedback loop, not independent support.

A sixth item is evidence, not mathematics: the **PWM-0 coast** of about
338 mm (§3.4). If that coast is real displacement, the PWM-0 rule itself
produces exactly the one-MM slip in item 4. One cheap check on existing
data decides it (F7).

---

## 1. Requirement and choice register

### 1.1 Operator requirements as recorded in the architecture document [OPERATOR]

| # | Requirement | M0 section |
|---|---|---|
| O1 | One expected progression after declaration; no 171-way search; one committed geographic history | §1, §4 |
| O2 | Map plus accumulated pattern are primary; no single sensor vetoes | §1, §4.1–4.2 |
| O3 | The MM–BL–MM ensemble is the unit; one advancement per ensemble, never one per lobe | §5.1 |
| O4 | An outlier MM is counted but not used as the origin for the next MM; no reset | §4.3 |
| O5 | The tile transitions on ensemble completion plus BL return, not on first opening | §5.1 |
| O6 | No provisional early advancement invented to fix timing | §5.1 |
| O7 | Existing station landmarks and stop routines are preserved; no closed-loop speed control; per-station CW/CCW adjustable later | §5.1 |
| O8 | Continuous Hall; no mandatory per-lobe segmentation or polarity DNA | §2, §4 |
| O9 | Persistent alignment, not a fixed 5- or 10-event window | §4 |
| O10 | A statistical corridor from validated complete laps; "about 20" laps, rolling and recency weighted (the number is a research candidate) | §3 |
| O11 | Whole-pattern similarity; the corridor is not a hard veto | §3, §4 |
| O12 | PWM is mandatory context, not proof of displacement, with no fixed ceiling | §4.1 |
| O13 | IR supports and never vetoes; IR errors are never written into the map | §4.2 |
| O14 | "Quick to condemn, quick to forgive"; no inherited sensor guilt | §4.2 |
| O15 | A missing observation or gap is not a STOP; no synthesized Hall | §5.1 |
| O16 | LOSS OF PATTERN → controlled STOP plus redeclaration | §6 |
| O17 | LOSS OF PROGRESSION → E-STOP plus inspection plus redeclaration | §6 |
| O18 | PWM-0 samples have no authority to advance, rebaseline or create an MM; state is frozen across a pause | §6 |
| O19 | Learn only from complete validated laps; an E-STOP disqualifies the lap; raw data always kept | §3, §7 |
| O20 | The same major discrepancy at one location for five consecutive rounds → STOP plus inspection | §7 |
| O21 | The ESP32 carries the model; Pi outages must not stop navigation | §3, §7 |
| O22 | Live model activation at a completed boundary, with no extra consistency veto | §7 |
| O23 | "About five successive features collectively incoherent" as the **proposed** incoherence horizon (the operator's proposal; its number awaits validation) | §5 |

### 1.2 Unvalidated mathematical choices [MATH]

| # | Choice | M0 section | Review finding |
|---|---|---|---|
| C1 | Causal Gaussian kernel over the auxiliary coordinate ξ | §2 | Circular when ξ comes from IR or PWM (§2.1) |
| C2 | σ ≈ 20–50 ms | §2 | Not the binding latency (§3.2) |
| C3 | `Hall ≈ b(s) + A(s)g(s) + noise` | §2 | Not identifiable as written; a local A absorbs the >50% anomaly (§2.2) |
| C4 | Weighted median, regularized MAD, empirical quantiles | §3 | Effective N is too small for empirical quantiles (§4.4) |
| C5 | Exponential recency weights, λ, K≈20 | §3 | Absorbs slow degradation (§4.5) |
| C6 | Constrained-DTW recurrence `D_i(m)` | §4 | No commit rule (§3.1); BL cells carry no phase information (§2.3) |
| C7 | Stall/move mixture for the PWM prior | §4.1 | Reasonable; π_stall is not identifiable during BL (§2.3) |
| C8 | Capped Huber IR penalty with `gamma_i` | §4.2 | Feedback through `gamma_i` (§4.3) |
| C9 | `s_hat = s_a + progress(a..i)` anchor estimator | §4.3 | A second estimator beside the DP; how the two combine is undefined (§3.1) |
| C10 | Clipped log-LR accumulator S | §5 | `no_match` is undefined; correlation (§4.1) |
| C11 | "Five features" as a number | §5 | Calibrate per feature, not per sample (§4.1) |
| C12 | >50% swing threshold | §7 | Conflicts with local gain normalization (§2.2) |
| C13 | Spatial resolution of `s_m` | §4 | Not stated; it decides whether the model fits ESP32 RAM (§5.2) |
| C14 | Token tuple contents | §5.1 | Fine; add `commit_lag` and `frontier_margin` |

Notation collision: `w_l` means observation-quality weights in §2 and
recency weights in §3. Rename one of them, for example `q_l` in §2.

---

## 2. Identifiability

### 2.1 The brush coordinate ξ

The kernel is evaluated in ξ, the "uncertain movement coordinate", so the
smoothed value `y_i` depends on odometry that M0 says is not trusted. If ξ
comes from IR, then IR noise reshapes the trace that Hall alignment is
judged on. IR then has authority without a veto, which O13 forbids in
spirit. If ξ is time, the brush blurs more at low speed and less at high
speed, so the corridor learned at one speed mismatches another. That is
validation item 1's "differing speeds".

**Recommendation [MATH].** Smooth in time with a short fixed σ. That
removes single-conversion transients and has no odometry dependence. Then
handle speed variation explicitly through the alignment's warping, which
exists to do that job. The two mechanisms should not overlap.

### 2.2 `b + A·g` has no unique decomposition

For any c≠0 and offset k, `(b, A, g)` and `(b − k·A/c, A/c, c·g + k)`
produce the same Hall trace, so the triple is not identifiable without
constraints. The more serious problem is what a locally varying `A(s)`
does: it can rescale an ensemble that has lost half its swing back into
the corridor. That hides exactly the >50% maintenance anomaly that §7 is
meant to catch. M0 does say "constrain gain normalization", but it gives no
constraint.

**Alternative [MATH]:**

    Hall_i = b_i + a_run · A_ref(s) · g_ref(s) + e_i

- `g_ref` is normalized per ensemble: zero mean over the ensemble and unit
  RMS.
- `A_ref(s)` is the learned **local** amplitude, which is part of the
  model and never estimated online.
- `a_run` is **one scalar per run or lap**: global sensor gain, temperature
  and air gap. It is estimated slowly and only from coherent stretches.
- `b_i` is the baseline. It is estimated only in cells the model labels BL
  and only at PWM>0, which is already the rule in decision 0017.

A local swing ratio `r_j = observed_amp_j / (a_run · A_ref(j))` is then a
diagnostic that normalization cannot erase. The >50% test reads
`r_j < 0.5` directly.

### 2.3 Hall carries no phase information over the baseline

Ensembles are 74–149 mm wide, against a nominal 300 mm spacing. On that
basis **about 60–75% of the track is BL**, where `Hall ≈ b`. In BL cells:

- `rho(y_i, M(s_m))` is the same for every `m` in the BL, so the DP is
  driven only by `psi`, the motion prior.
- "Stalled in BL" and "moving through BL" cannot be told apart from Hall.
  π_stall and `P_move` can only be separated by PWM and IR in those cells.
  O17 handles this correctly by requiring corroborating evidence. The
  mathematics should state it outright: **phase is identified only at
  ensembles, and BL travel is pure dead reckoning from PWM and IR.**

**The one-MM slip.** Suppose ensemble j produces no usable signal. The
data then fit two paths with equal Hall cost:

- path P: j missing, normal speed;
- path Q: the train moved one spacing less, and the next ensemble seen is
  j rather than j+1.

The two paths differ only in

1. the PWM and IR distance over about 300 mm, and
2. how well ensemble j's shape matches the observed bump compared with
   ensemble j+1's shape.

O12 rules out a PWM minimum and O13 rules out an IR veto, so (1) is
bounded support at most. **Identifiability then rests on (2): local
distinctiveness of the landscape.** "Persistence" cannot add
distinctiveness that the landscape does not have. If ensembles j and j+1
look alike, a persistent alignment slips together with its own history.

Measure this before choosing any algorithm (§5.4).

---

## 3. Causality and irreversibility

### 3.1 Commit rule: the missing equation

DTW, HMM and DP frontiers all improve when later data arrives. The 10-16
replay showed it: the live (zero-lag) estimate was one magnet ahead once,
and only the 4 s retrospective reading corrected it. M0 also requires one
committed history and an executed, idempotent token. Something has to map
the revisable frontier to final tokens. Three candidates [MATH]:

- **Fixed-lag commit.** Commit ensemble j at the first i where every
  frontier path with cost within Δ of the best has passed the end of
  ensemble j. This is the classical "path merge" or traceback-convergence
  point in Viterbi decoding. It is causal, needs no tuned threshold beyond
  Δ, and is auditable through the merge point and the margin.
- **Posterior commit (HMM/HSMM).** Commit when `P(passed end of j | y_1:i) ≥ 1 − ε`.
- **Margin commit.** Commit when best-path cost beats the best competing
  path ending in ensemble j−1 or j+1 by more than Δ.

Each one is a decision rule and has a threshold, whichever rule is picked.
O6 rules out an *invented provisional* advancement. It does not rule out
defining when the single advancement commits, and M0 cannot run without
that definition. This needs an explicit operator decision on which form of
rule is acceptable. It is **not** a mathematical detail.

The §4.3 anchor estimator `s_a + progress` is a second estimator beside the
DP frontier. Pick one. Under fixed-lag commit, the natural "trusted anchor"
is the last committed merge point, so no separate anchor is needed.

**What happens on contradiction is undefined.** A token for j is
committed, and later evidence says the train was still in j−1. Tokens
cannot be retracted, so the only correct responses are to absorb the
contradiction (the next commit is j+2 and nothing is re-issued) or to
declare LOSS OF PATTERN. M0 should say which. The validation needs a
"retrospective disagreement with committed token" counter.

### 3.2 Lower bound on completion latency

A causal decision that "ensemble j has ended and the BL has returned" must
rule out "still inside j, crossing a flat lobe". It cannot do that until
the flat stretch is longer than `d_flat_max`, the longest flat region
inside a single ensemble. Therefore:

    token_distance_after_last_feature  >=  d_flat_max + 0.8·sigma·v

The second term is the mean lag of a half-Gaussian, `σ·√(2/π) ≈ 0.80σ`.
At σ = 50 ms and v = 300 mm/s it is about 12 mm, which is small. **The
smoother is not the bottleneck. `d_flat_max` is.** The architecture
already plans to measure it (item 12). M0's §9 question, "early enough?",
therefore reduces to a number already queued for measurement:
`d_flat_max` compared with the slack each station routine has.

The bound loosens only if the recognizer uses where the ensemble ends in
the *model*, that is, the learned extent of ensemble j. In that case,
being at the learned end of j is evidence by itself. But "at the learned
end of j" is a phase estimate made over flat signal, so it is dead
reckoning again (§2.3). This tradeoff has to be measured, not argued.

### 3.3 Conflict with O7

[`NAVI_MAGNETIC_OPENING_VS_PHYSICAL_LANDMARK_20261014.md`](navi/NAVI_MAGNETIC_OPENING_VS_PHYSICAL_LANDMARK_20261014.md)
on this branch already separates the Hall-opening coordinate from the
physical landmark. Existing station routines are calibrated to fire on the
opening. A completion token fires about `(ensemble_width − opening_offset)
+ d_flat_max` later: plausibly 60–200 mm with the measured widths.

O5 (fire on completion) and O7 (keep the stop routines unchanged)
together therefore predict that **every station stop moves by that
distance**. This is not a validation question. It is an arithmetic
consequence, and the operator should see it before field testing. Options
for the operator (not chosen here):

- (a) accept the shift and recalibrate per station and direction later,
  which the architecture already allows;
- (b) define station landmarks on the preceding tile's token;
- (c) let station routines alone use a different event.

### 3.4 PWM-0 coast

O18 gives PWM-0 samples no authority. The 10-17 finding of 35 IR pulses
(about 338 nominal mm) during a 13 s PWM-0 episode is already more than
one magnet spacing. Every station stop also ends with momentum after the
commanded zero. If that coast is real displacement and it crosses an
ensemble end, then:

- the ensemble's completion falls inside PWM-0, so it is not counted;
- on RESUME, "continue the previously developing ensemble" continues the
  **wrong** ensemble;
- the result is the one-MM slip of §2.3, created by a rule rather than by
  noise.

The spec rightly does not call this proven. It is still the most direct
route by which a recorded requirement can produce a geographic error, and
it can be tested on existing data (F7).

---

## 4. Independence assumptions and feedback loops

### 4.1 Evidence accumulator S

- **`P(y | no_match)` is undefined.** The alternative hypothesis decides
  what S detects. A generic background model detects "not magnetic". A
  model shifted by ±1 ensemble detects a slip. These are different
  detectors. **Alternative [MATH]:** keep one CUSUM per named alternative:
  `H_slip+1`, `H_slip−1`, `H_stall` (frozen phase) and `H_dead`
  (a constant Hall). Then report *which* failure is accumulating. That maps
  directly onto LOSS OF PATTERN versus LOSS OF PROGRESSION and the
  Hall-failure case in O17.
- **Unit of evidence.** Increment S **once per committed ensemble**, using
  that ensemble's whole-segment likelihood ratio, never per sample. That
  fixes the correlation problem M0 names. It also makes "five features"
  literally five increments, so the false-stop rate can be estimated from
  per-ensemble LR statistics on held-out laps.
- **Features are not independent either.** A slow gain drift raises every
  feature's cost together, so five correlated mild mismatches are not five
  pieces of evidence. With §2.2's `a_run` estimated separately, the
  residuals are much closer to independent. Test this with the lag-1
  autocorrelation of per-ensemble LR on clean laps.

### 4.2 PWM and IR are not independent of each other

Both measure the drivetrain. Wheel slip during a stall produces IR pulses
with no displacement, and the Grillers stall at positive PWM. In that case
PWM says "moving", IR says "moving", and the train is stationary. Two
agreeing movement sensors are therefore **one** piece of evidence about
the drivetrain, not two about the track. Only Hall measures track
position. LOSS OF PROGRESSION should be stated as "Hall stationary *and*
(a drivetrain sensor says moving)". Wheel slip versus Hall-sensor failure
then remains unidentifiable without a third channel. Say so rather than
calibrate around it.

### 4.3 `gamma_i` loop (IR trusted only when it agrees)

`gamma_i` depends on agreement with "independently established broad
progression". That progression comes from Hall. IR is then weighted up
when it agrees with Hall and down when it disagrees, so it can never
correct Hall. In a Hall-loss stretch, the very case IR is for, `gamma_i`
has nothing to compare against.

**Alternative [MATH]:** make `gamma_i` depend **only on IR's own
integrity signals**: pulse-width consistency, missing-pulse detection from
inter-pulse timing, and source timestamps. Those are internal to IR and
give "quick to forgive" for free, because each interval is judged on its
own. Agreement with Hall becomes a *diagnostic output*, never an input to
IR's weight.

### 4.4 The training set is small and self-correlated

- **Effective sample size** with recency weights:
  `n_eff = (Σw)² / Σw²`. For K=20 and a moderate λ, `n_eff` is roughly 8–12.
  An empirical 5/95% quantile from about 10 samples is close to the
  min/max of the sample, so the corridor's edges become noise.
  **Alternative [MATH]:** a location-scale corridor
  `m(s) ± k·d(s)` with `d(s)` shrunk toward an ensemble-pooled
  dispersion, `d_shrunk = (n_eff·d_local + n0·d_pool)/(n_eff + n0)`, and
  `k` calibrated once on held-out *complete* laps.
- **Laps from one evening or one day are not independent.** Hold out
  whole days, not laps (F5).
- **Alignment blur.** Laps are aligned to the map with an alignment that
  depends on the scaffold. Misalignment widens the corridor most at steep
  flanks. Steep flanks are where phase information lives. A sloppy
  alignment therefore cuts identifiability exactly where it matters.
  Report corridor width at flanks separately from BL and peaks.

### 4.5 Self-certification and slow drift

The learning loop is: the recognizer uses model `M_v` to validate a lap,
and that lap updates `M_{v+1}`. M0 says a lap "cannot validate itself
merely by matching the model it would update". It does not say what else
validates it. Three loops exist:

1. **Selection bias.** Only laps the recognizer accepts enter training.
   Over time the corridor narrows toward what the recognizer already
   likes.
2. **Boiling frog.** A 2%-per-lap amplitude decline, after recency
   weighting, never triggers the >50% or five-round rule, because each lap
   is within the corridor of the last 20.
3. **b and A estimation.** Baseline and gain are fitted "using
   geographically supported context". That context comes from an alignment
   that used `b` and `A`.

**Alternatives [MATH]:**

- **Lap validity criteria that do not use `M_v`.** The lap closes with
  exactly N committed tokens; unwrapped phase returns to start within
  tolerance; there was no E-STOP; and IR-integrated distance over the lap
  is within a band of L. IR may be biased, but bias is constant over a lap,
  so a ratio test is fair.
- **A frozen commissioning reference `M_0`**, kept permanently and never
  used for navigation, beside the rolling model. Report
  `drift_j = r_j(M_current) / r_j(M_0)` per ensemble. A gradual decline
  shows up against `M_0` even when it hides inside the rolling corridor.
  This is a diagnostic, not a veto, so it does not breach O22.
- **Cap the rate of model change.** Limit how far the corridor center may
  move per accepted lap, in units of `d(s)`.
- **b-only-in-BL, at PWM>0, slow EMA.** `a_run` is fitted only over
  committed ensembles with low LR residual.

---

## 5. Computational bounds

### 5.1 DP cost

The 10-16 research estimated about 60 band cells × 4 predecessors at
100 Hz, which is under 0.2% of one core. That holds for the band DP. M0
adds:

- **Traceback storage for the commit rule.** Backpointers for B cells
  over the commit lag T_c at 100 Hz take `B · T_c · 100` bytes.
  At B=60 and T_c=4 s that is 24 KB. It is bounded, but it has to be
  budgeted.
- **Per-feature CUSUMs:** negligible.
- The DP is Markov. "Persistence over the whole journey" lives entirely in
  `D_{i−1}` plus the committed history. **No growing state is needed or
  allowed.** State that explicitly, so that nobody implements persistence
  as a growing buffer.

### 5.2 Model size: the binding resource

L ≈ 171 × 300 mm ≈ 51 m. The model stores per cell a center, a dispersion
and, if empirical, two quantiles:

| Resolution | Cells | 4 × int16 per cell | CW + CCW |
|---|---|---|---|
| 1 mm | 51 k | 410 KB | 820 KB |
| 2 mm | 26 k | 205 KB | 410 KB |
| 5 mm | 10 k | 82 KB | 164 KB |

A classic ESP32 has 520 KB of SRAM, shared with Wi-Fi. A 1 mm model does
not fit in RAM. It fits in flash with a memory-mapped read of the band
only, which needs about 60 cells at a time. The replay's median position
error was about 3 mm, so 2–5 mm resolution is likely enough. Storing only
center plus pooled dispersion (§4.4) halves the size. **Choose C13
explicitly.** A live update also needs double buffering (O22, rollback),
which doubles the flash requirement.

### 5.3 Worst-case paths

Bound the band: if the motion prior has no upper speed (O12), V_i is
unbounded in principle. Pick a cap well above any recorded downhill speed,
label it **[MATH]**, and log any frontier that hits it. A frontier at the
cap is evidence of failure, not a reason to widen the cap.

### 5.4 Self-ambiguity profile: compute it first

For each ensemble j and shift k ∈ {−2, −1, +1, +2}:

    A_j(k) = min over warps of cost(M_ref[segment j..j+W], M_ref[segment j+k..j+k+W]) / noise_scale

A_j(±1) near the noise floor marks a place where a missed ensemble can
produce an undetectable slip. The calculation is offline, takes about an
hour on the Pi, and uses only reference data. It answers M0 §9's
"one-MM slip" question per location, *before* any algorithm is chosen.
It also tells the operator where the railway is magnetically repetitive.

---

## 6. Alternative formulations [MATH]

### 6.1 Left-to-right hidden semi-Markov model (main alternative)

States: `E_j` (inside ensemble j) and `B_j` (BL after j), in strict
left-to-right order, with self-transitions only for stall. Each state has
an explicit **spatial duration distribution**, learned from laps, and
time-to-space comes from the PWM/IR motion model.

- The forward algorithm gives the causal `P(B_j | y_1:i)`. **That is the
  MM–BL completion probability M0 asks for, by construction.**
- Fixed-lag smoothing gives `P(B_j | y_1:i+τ)`. The difference between the
  two is exactly **the retrospective advantage of MM–BL–MM, quantified per
  ensemble.** This makes §9's most important question directly
  measurable.
- Flat-lobe impostors are handled by the duration model of `E_j`. A flat
  stretch shorter than the learned `E_j` extent stays in `E_j`. The
  `d_flat_max` bound of §3.2 is visible in the duration tails.
- A missing ensemble is an explicit low-probability "silent `E_j`"
  emission, kept separate from the skip transition `E_j → E_{j+2}`, which
  stays forbidden (O1).
- Cost: N·2 states, banded to a few around the frontier. That is about the
  same as the DTW band.

It satisfies O1, O3, O5, O8 and O9 as well as the DTW does, and it makes
the commit rule (§3.1) a posterior rather than a heuristic.

### 6.2 Normalized cross-correlation in space (baseline comparator)

Resample Hall to space using the committed phase, then
`NCC(window W, shift δ)` against `M_ref`. It is simpler and offers no
warping inside the window. Use it as the floor that DTW and HSMM must
beat on held-out days (M0 §9, first question).

### 6.3 Robust cost with bounded influence

M0 asks for a guarantee that an outlier's effect is limited (§4.3).
Use a **redescending** ρ, such as Tukey or a truncated quadratic, so that
one ensemble's cost contribution is capped at ρ_max. Then, by
construction, any path differs from another by at most ρ_max over one
outlier ensemble. A single anomaly cannot move the argmin across more than
one ensemble unless the rest of the landscape is ambiguous there, which is
§5.4 again. This turns "the implementation must explicitly limit" into a
checkable property.

---

## 7. Falsification experiments

Each experiment states the prediction that would **refute** the
claim. All use recorded data only, unless marked TRACK. A TRACK test needs
separate operator authorization and is not requested here.

| ID | Claim tested | Method | Refuted if |
|---|---|---|---|
| F1 | Completion latency is bounded by `d_flat_max` (§3.2) | Measure `d_flat_max` (arch. item 12). Replay a causal recognizer and log token distance after the last lobe per ensemble | Token distance < `d_flat_max` with no wrong tokens (the bound is wrong, or the model extent is doing the work), or token distance ≫ `d_flat_max` (a different bottleneck) |
| F2 | Firing on completion shifts station stops (§3.3) | Replay all station approaches: opening time against completion-token time; convert with recorded speed to mm | Shift < 20 mm at every station (conflict immaterial) |
| F3 | A one-MM slip is detectable where `A_j(±1)` is high (§5.4) | Labeled injection: blank one ensemble at each j (171 runs per lap); count slips | Slips occur where `A_j(±1)` is high, or none occur where it is low (the profile does not predict) |
| F4 | Commit rule never retracts (§3.1) | Replay with fixed-lag commit; count retrospective disagreements with committed tokens | Any disagreement on clean laps; report rate on disrupted ones |
| F5 | The corridor generalizes across days (§4.4) | Train on day A, test on day B; leave-one-day-out | Per-ensemble LR distribution shifts beyond held-out within-day spread |
| F6 | IR adds information (§4.3) | Replay with γ=0 against IR-integrity-only γ against M0's γ | Identical token sequences and margins: IR contributes nothing in this data. M0 γ beating integrity-only γ only where Hall is good confirms the loop |
| F7 | PWM-0 coast is real displacement (§3.4) | On the 13 s PWM-0 episode and every recorded station stop: does Hall show ensemble-shaped change while PWM=0? Align it to the reference | Hall flat throughout every PWM-0 interval with IR pulses (the pulses are handling or slip, not travel) |
| F8 | Recency weighting hides slow decline (§4.5) | Labeled injection: scale one ensemble by 0.98ⁿ for lap n over 40 laps | Five-round or >50% rule fires before r_j(M_0) < 0.6 (no hiding) |
| F9 | Per-ensemble LR increments are near-independent (§4.1) | Lag-1 autocorrelation of per-ensemble LR on clean laps, with and without `a_run` | \|ρ₁\| > 0.3 after `a_run` removal |
| F10 | `b + A·g` normalization preserves the anomaly (§2.2) | Labeled injection: halve one ensemble; check r_j | r_j > 0.6 after normalization |
| F11 | DTW beats simpler alternatives (M0 §9) | Same held-out days: NCC (§6.2), DTW (M0 §4) and HSMM (§6.1) | NCC within the confidence interval of DTW on identity, lag and false-stop rate. Then choose the simpler method |
| F12 | ESP32 budget (§5) | Bench (no locomotive): stream a recorded R1 strip into an ESP32 running the band DP plus commit; measure µs/block, peak heap and flash reads | Over 5% of a core, or the model does not fit at the chosen resolution |
| F13 TRACK | Stall in BL is detected only via drivetrain sensors (§2.3, §4.2) | Only if separately authorized: a deliberate short hold at positive PWM in a BL stretch | Hall-only stationarity fires during the BL hold (that would mean Hall *has* information over BL, so §2.3 is wrong) |

F3, F8 and F10 are **labeled injections**, per M0 §8. F1, F2, F5, F7
and F9 use only original records.

---

## 8. Smaller points

- **M0 §1:** "N=171 (confirm)". The 10-16 replay discovered 171 ensembles
  for 171 magnets, so cite that run as the confirmation source.
- **M0 §4:** "eligible moving samples". The eligibility definition should
  be one written rule, the same one used for `w_l = 0` in §2, so the brush
  and the DP cannot disagree about which samples count.
- **M0 §5.1:** add `commit_lag_ms` and `frontier_margin` to token `T`.
  Both are needed for F1 and F4 and for the field audit.
- **M0 §6:** "Hall-stationary alone may mean Hall sensor failure". Add
  `H_dead` (constant Hall at the ADC rail or at the frozen baseline value,
  with zero sample variance) as a named alternative. Otto's Hall never
  rails (raw 1777–2112), so a reading near 0 or 4095 is hardware.
- **M0 §7:** "activate atomically at a completed MM–BL–MM boundary". Under
  any commit rule, activate only at a **committed** boundary, and carry
  the frontier `D` across by re-scoring its band under the new model
  before swapping. That is a no-phase-jump test the code can assert
  without a new consistency veto.

---

## 9. What this review does not establish

- It ran no replay. Every numeric estimate here is arithmetic from
  published widths, rates and memory sizes, and the falsification table is
  there to test them.
- The §3.3 stop-shift magnitude depends on the opening-to-landmark offsets
  in the station tables. Those were not re-derived here.
- Per AGENTS.md §8, none of this establishes railway behavior. The
  recommendations are ordered so that offline work (§5.4, F1, F2, F7)
  answers the largest open questions before any firmware is proposed.
