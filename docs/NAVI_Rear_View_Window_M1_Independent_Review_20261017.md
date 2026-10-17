# NAVI_Rear_View_Window M1: independent mathematical review

**17 October 2026 · Claude (Opus 5.5) · Review only**

Reviewed: `docs/NAVI_Rear_View_Window_Mathematical_Specification_M1.md` at
`e021228`, on branch `docs/navi-rear-view-window-architecture-20261017`.
Read against the architecture document, M0 and my M0 review
([`NAVI_Rear_View_Window_M0_Independent_Review_20261017.md`](NAVI_Rear_View_Window_M0_Independent_Review_20261017.md)).

> **Status.** Review only. No firmware, map, governing architecture,
> service or device was changed. Labels: **[OPERATOR]** marks a
> requirement recorded as the operator's answer. The architecture document
> calls itself "proposed", and decisions are not ratified until the
> operator approves them. **[MATH]** marks an unvalidated mathematical
> choice, M1's or mine. The October 17 baseline-extent report that M1 cites
> is not in the repository. Its figures are used here only as M1 states
> them.

---

## 0. Answer first

M1 closes most of the M0 findings. The appearance-commit rule also removes
the biggest timing problem: the `d_flat_max` latency, and the resulting
station shift, are no longer on the token path. Five problems remain or are
new.

1. **The overlapping pieces count one bad magnet twice (§2).**
   P_062 = (MM061, BL, MM062) contains the bad MM061. If Q_062 is scored
   on the whole piece, a single anomaly fills two of the five LOSS OF
   PATTERN slots, and "if 062 matches, coherence resumes" is false. The fix
   is a definition, not a tuning: score quality on the **new part**
   (BL + MM_j) only.
2. **Once G and Q are separated, progression is a counter (§3).** G_j asks
   "did a new magnetic region appear after a genuine BL?" No PWM minimum,
   no IR veto and no Q veto apply, so G_j carries **no identity
   information**. Identity is held entirely by the count. A missed or extra
   region therefore makes a permanent one-MM slip. Only Q_j can expose it,
   through C_recent, and only where the landscape is locally distinctive.
   On the all-alternating N/S map in M1 §9.1, a slip is **undetectable by
   Hall at any horizon**. That is a mathematical limit, not a tuning
   result, and it should be stated as one.
3. **Wrong tile orders run during the five-piece horizon (§3.3).** After a
   slip, up to five tiles' orders execute before LOSS OF PATTERN. That is
   about 1.5 m. If a station stop falls inside that horizon, the stop
   executes at the wrong place. This is the safety question M1 §5 names,
   and it can now be quantified per station (F4).
4. **The answer to M1's central question is a change-point detector with a
   known tradeoff (§4).** "What evidence establishes appearance?" is
   classical quickest change detection. Page's CUSUM gives a direct link
   between false-token rate and detection delay: `delay ≈ log(ARL) / KL`.
   That puts M1's three candidates (A–C) on one axis, and the threshold
   comes from a stated false-token rate rather than an arbitrary number.
5. **The PWM-0 movement flag fires at every station stop unless coast is
   modeled (§6).** Normal momentum after a commanded zero produces IR
   pulses and Hall change. Without an expected-coast window, "notify and
   require inspection" triggers on every ordinary stop. The appearance of
   MM_j can also fall inside the coast, and M1 does not say what then
   happens.

---

## 1. Status of the M0 review findings

| M0 review finding | M1 | Status |
|---|---|---|
| Commit rule missing (§3.1) | Appearance-commit; the predicate is still open | **Rule-type decided [OPERATOR]; predicate open** |
| `d_flat_max` latency bound (§3.2) | Appearance commit removes it from the token path | **Resolved by design**; it moves into G_j (§4.2) |
| Station shift from completion trigger (§3.3) | Appearance near the opening; timing to be measured | **Largely resolved**; residual = detection delay |
| One-MM slip identifiability (§2.3) | §9.5 asks for identifiability limits | **Open, and sharper** (§3) |
| PWM-0 coast (§3.4) | Investigate, notify, inspect | **Partly addressed; new false-flag problem** (§6) |
| ξ-coordinate brush circular (§2.1) | Time-domain smoothing | **Resolved** |
| `b + A·g` not identifiable (§2.2) | `a_run` model adopted | **Resolved** |
| `gamma_i` feedback (§4.3) | IR-internal integrity only | **Resolved** |
| PWM and IR not independent (§4.2) | Stated | **Resolved** |
| `no_match` undefined (§4.1) | L_j alternative = "no progression yet" | **Resolved for G_j; open for slip detection** (§3.2) |
| n_eff, shrinkage, held-out days (§4.4) | Adopted | **Resolved** |
| Frozen diagnostic reference (§4.5) | Adopted | **Resolved** |
| Lap validity independent of `M_v` (§4.5) | "Do not certify solely by fit"; no criteria given | **Open** (§7) |
| Model resolution and size (§5.2) | Not stated | **Open** (§5) |
| Token audit fields | `commit_time`, `quality`, commit lag | **Resolved** |

---

## 2. Overlapping pieces double-count anomalies

**Statement.** P_j = (MM_{j−1}, BL, MM_j), and P_{j+1} shares MM_j.

**Consequence.** Suppose Q_j is computed over the whole piece (M1 §5,
E_j over "the one expected piece"). Then:

- a bad MM061 degrades both Q_061 and Q_062, so one anomaly uses up two
  of the five-piece LOSS OF PATTERN slots;
- two adjacent bad magnets use three;
- the M1 invariant "if 062 matches, coherence resumes" cannot hold,
  because Q_062 includes MM061;
- the per-piece incoherence increments are **correlated by construction**
  at lag 1. That is the same failure M1 warns about for overlapping
  samples.

**The overlap does contribute something useful:** *context for
recognition*. The trailing shape of MM_{j−1} tells the detector where the
BL starts. It should not contribute to *quality bookkeeping*.

**Recommendation [MATH].** Use the overlap for G_j only. Define

    Q_j  = quality of the new part only:  (BL_{j−1,j}, MM_j)
    run  = consecutive count of bad Q_j  (one slot per physical magnet)

The five-run then counts magnets, and one bad magnet costs one slot.
**Operator decision needed:** whether "five pieces" means five magnets or
five overlapping windows. The two differ by a factor of about two in how
fast the rule fires on a single bad region.

---

## 3. Progression is a counter: identity limits

### 3.1 What G_j can and cannot know

G_j tests "a new region appeared after a genuine BL, consistent with the
expected board position". Evidence available under the recorded rules:

- Hall: that *some* region appeared. Its shape belongs to Q.
- "Consistent with expected position": requires distance since MM_{j−1},
  which comes from PWM and IR. O12 says PWM has no minimum and O13 says IR
  has no veto, so position consistency can only *support*, never refuse.

So G_j fires on **any** magnetic region after any genuine BL. Identity is
`j_c + 1` by count. This is the intended design (O1: one expected place).
Its consequences should be written down:

| Event | Effect on count | Detected by |
|---|---|---|
| Ensemble too weak to register | −1 slip, permanent | Q of following pieces, *if* distinctive |
| Extra region (stray steel, a tool on the track, a split ensemble seen as two) | +1 slip, permanent | same |
| Recording gap over one ensemble | −1 slip | same, plus the gap flag |
| Missed BL (two ensembles merged) | −1 slip | same |

### 3.2 Slip detection needs named alternatives

After a slip, every subsequent piece is scored at the wrong expected
location. If the landscape is distinctive, Q_j drops for every piece and
LOSS OF PATTERN fires after the horizon. If it is not distinctive, nothing
fires. L_j's alternative ("no progression yet") cannot see this. A
dedicated slip statistic can, and it stays diagnostic, which respects O1:

    S_slip(k) = SUM over recent pieces of  [cost(Q_j at j) − cost(Q_j at j+k)],   k = ±1

This is **not** a competing geographic identity. It does not move the
count. It is evidence that LOSS OF PATTERN is a slip, and it is
reported. It gives the operator a reason with each STOP and gives replay a
measured detection distance. **Operator decision needed:** whether this
diagnostic is acceptable under "no carousel of positions" (arch. O1). It
never drives the controller, and it should be confirmed as permitted.

### 3.3 Self-ambiguity profile (carried from the M0 review §5.4)

    A_j(k) = min_warp cost(new-part_j , new-part_{j+k}) / noise,  k = ±1, ±2

- A_j(±1) is low everywhere, as on the all-alternating map: slips are
  invisible to Hall, the system is a pure counter, and only PWM and IR
  distance can raise suspicion.
- A_j(±1) is high: a slip shows within one or two pieces, well inside
  five.

The expected number of pieces to detect a slip at j is approximately
`h / mean(A_{j..j+n}(±1))` for a detection threshold h. Combined with the
station table, this gives the **worst-case wrong-tile distance per
station**. That is the quantitative answer to M1's safety question, and it
can be computed from reference data alone.

---

## 4. The appearance predicate is quickest change detection

### 4.1 Formulation [MATH]

Before MM_j appears, the trace is BL: `y ~ N(b, σ_BL²)`, with b from
credible BL at PWM>0. After appearance, it follows the leading part of the
expected corridor: `y ~ N(b + a_run·A_ref·g_ref(φ), d²)`. Page's CUSUM over
the new-part template is

    W_i = max(0, W_{i−1} + log[ p(y_i | appeared, φ_i) / p(y_i | BL) ])
    commit TILE(j) when  W_i ≥ h  and  G_BL holds (§4.2)

Lorden's bound links the two costs:

    E[detection delay]  ≈  log(ARL_FA) / KL(appeared ‖ BL)   samples
    ARL_FA ≈ e^h                         (mean samples between false alarms)

- **KL is set by the leading-edge shape.** It is large for a steep, strong
  edge. It is small for the "flat leading lobe displaced from BL" that M1
  §3 describes: there KL ≈ (displacement/σ_BL)²/2 per sample. M1 calls
  that lobe magnetic evidence, and it is, but detecting it takes
  `2·log(ARL)/(Δ/σ_BL)²` samples. Measure Δ/σ_BL on real leading lobes.
  If it is near 1, a flat leading lobe takes roughly tens of samples
  (hundreds of ms) to detect at any reasonable false-alarm rate.
- **h comes from a stated false-token rate.** A false-token rate is an
  operator-understandable quantity ("one early TILE per N laps"), unlike
  an ADC threshold. **Operator decision needed:** the acceptable false-
  versus late-token tradeoff. The two errors are asymmetric: an early
  token executes orders early and irreversibly, while a late token only
  delays them.
- **Baseline uncertainty enters directly.** If b is stale, the
  "displacement" is partly zero drift. The NGR Hall zero moves by about
  6 counts/s at most and does not trend. Its age matters more than its
  value, so b must be refreshed from recent credible BL, and that age
  should be logged with each token.
- **Correlated samples.** The 20–50 ms smoother correlates adjacent
  samples. Use the CUSUM on unsmoothed 10 ms R1 block means, or inflate
  `σ_BL` by the smoother's effective sample factor. Otherwise ARL_FA is
  overestimated, the warning M1 itself gives.

M1's candidates map onto this:

- (A) semi-Markov progression gives the same detector with a duration
  prior on BL;
- (B) correlation gives a matched filter, whose CUSUM is the
  sequential form;
- (C) restricted DP or path convergence gives the same alternative with
  warping, and costs more.

All three can be scored by the same two numbers: delay and ARL_FA.

### 4.2 The BL gate is where `d_flat_max` went

G_j requires "after the intervening BL". The gate `G_BL` must accept a
genuine BL and reject a within-ensemble flat core. M1 quotes the October 17
figures as 9–52 nominal mm for within-ensemble cores and 103–234 for
between-ensemble BLs. **The nominal ranges are separated by about 51 mm**,
and M1 says the uncertainty overlaps. So:

- The gate threshold must lie between the longest core and the shortest
  BL. Its margin is 51 nominal mm, *measured in IR-nominal distance*,
  which O13 says can be wrong. In time, the margin depends on speed.
- With appearance-commit, the BL is complete **before** MM_j appears, so
  the gate adds **no latency** when the BL is ≥ 103 mm. It adds risk only
  when it wrongly passes a 52 mm core. That produces an **extra region**,
  a +1 slip (§3.1).
- An alternative that does not need a distance gate [MATH]: require the
  CUSUM for "appeared" to beat the CUSUM for "still in the trailing part
  of MM_{j−1}". That is M1's L_j with the expected trailing shape as the
  alternative, rather than flat BL. The model already holds that trailing
  shape.

---

## 5. Computation

- **Whole-piece E_j over monotone warps** has to be **incremental**: one
  DTW column per new sample, O(cells in piece) per sample. Recomputing the
  whole piece per sample is O(n²) per piece. At about 200 samples and
  about 100 cells, that would be about 2 M cell-updates per piece versus
  about 20 k incremental. State this in the spec, so that the cost bound
  can be tested.
- **The CUSUM in §4 is O(1) per sample.** The whole-piece E_j is then only
  needed for Q_j, after commit, at piece rate rather than sample rate.
- **Model size is still unstated.** It is about 82 KB per direction at
  5 mm and about 410 KB at 1 mm, against 520 KB SRAM (M0 review §5.2).
  Only the new-part template near j_e needs to be resident. The rest can
  stay in flash.

---

## 6. PWM-0 movement and station coast

- **Expected coast.** Momentum after a commanded zero is normal and
  happens at every station stop. Partition PWM-0 into
  `coast window` (from the zero command until IR has been quiet for T_q)
  and `dwell`. Flag "unexpected movement" only for movement evidence in
  **dwell**, or for coast exceeding the per-station historical coast
  distribution. Otherwise O18's investigation rule fires on every
  ordinary stop. **Operator decision needed:** whether the investigation
  requirement covers coast.
- **Appearance during coast.** Station stops sit close to magnets. If MM_j
  appears during coast, O18 gives it no authority, so there is no token.
  On RESUME the CUSUM starts partway into MM_j's leading edge, with the
  "before" segment missing. Three options, for the operator:
  - (a) hold Hall evidence from the coast as *pending* and allow the token
    on RESUME if the continuing pattern confirms it;
  - (b) treat the coast as unauthorized and require MM_j to be recognized
    from RESUME data alone. The leading edge is gone, so this risks a
    missed region and a −1 slip;
  - (c) inspect.

  F6 measures how often this occurs.
- **The 13 s, 35-pulse episode.** M1 correctly does not treat it as proof
  of displacement. The M0 review's F7 still applies: did Hall show
  ensemble-shaped change during that PWM-0 interval?

---

## 7. Learning loop: one gap remains

M1 says a lap must not be certified solely because it fits. It does not
say what does certify it. Candidate criteria that do not use `M_v` [MATH]:

- the lap closes with exactly N committed tokens;
- IR-integrated lap distance is within a band of L. IR bias is constant
  within a lap, so the ratio test is fair;
- no E-STOP and no LOSS OF PATTERN;
- no unresolved PWM-0 movement flag;
- **`S_slip(±1)` stayed below threshold throughout.** A lap containing an
  undetected slip would otherwise teach the model that j looks like j+1,
  which erodes the distinctiveness that slip detection depends on.

---

## 8. Requirement register

### [OPERATOR] (new or changed in M1)

| # | Requirement |
|---|---|
| O24 | One expected piece; no search across the map |
| O25 | Overlapping pieces P_j = (MM_{j−1}, BL, MM_j) |
| O26 | TILE(j) commits on the *appearance* of MM_j within a recognizable piece, superseding completion-commit |
| O27 | Progression (G) and quality (Q) are separate; poor Q does not veto progression |
| O28 | An executed token is never retracted; disagreement is diagnostic or leads to STOP |
| O29 | Unexpected PWM-0 movement: investigate, notify, inspect before automatic resumption |
| O30 | The trusted anchor needs multi-piece coherence; an anomalous piece never anchors |
| O31 | Otto's body runs about one MM ahead of the Hall sensor (operational fact, as stated) |

### [MATH] (M1 choices)

Causal time-domain kernel; `a_run · A_ref · g_ref` model; exponential
recency weights with K≈20; `d_shrunk` corridor with k; the robust warped
E_j; the L_j log-ratio; the Huber IR cost with `gamma_i` from integrity;
"five pieces" as a number; >50% swing; spatial resolution (unstated).

### [MATH] (proposed here)

New-part Q_j (§2); `S_slip(±1)` diagnostic (§3.2); self-ambiguity
`A_j(k)` (§3.3); CUSUM appearance predicate with ARL-calibrated h (§4.1);
trailing-shape alternative for the BL gate (§4.2); coast/dwell partition
(§6); lap validity criteria (§7).

---

## 9. Falsification experiments

Recorded data only, except the TRACK row, which needs separate operator
authorization and is not requested here.

| ID | Claim | Method | Refuted if |
|---|---|---|---|
| F1 | Whole-piece Q double-counts (§2) | Replay a labeled injection degrading MM061 only; compare whole-piece and new-part Q_061, Q_062 | Whole-piece Q_062 stays inside the corridor |
| F2 | The CUSUM delay/ARL tradeoff predicts real delay (§4.1) | Estimate KL per leading edge from reference laps; predict delay at h; replay held-out days | Measured median delay is outside ±50% of the prediction on most ensembles |
| F3 | Flat leading lobes are slow to detect (§4.1) | Measure Δ/σ_BL on leading lobes M1 calls flat | Δ/σ_BL > 3 for all of them (then they are not slow) |
| F4 | Wrong-tile distance before LOSS OF PATTERN (§3.3) | Labeled injection: delete one ensemble at each j; record pieces until the five-run or the S_slip threshold; convert to mm; list the stations inside | Zero stations fall inside the horizon on every injection (no safety issue) |
| F5 | The BL gate's 51 mm margin holds across speed and IR error (§4.2) | Replay all laps; count cores that pass the gate and BLs that fail it | No core passes and no BL fails across all held-out days |
| F6 | Appearances fall inside station coast (§6) | For each recorded station stop: is a Hall leading edge present between the PWM-0 command and IR-quiet? | Never (the open question is moot in current operation) |
| F7 | The coast/dwell partition prevents false PWM-0 flags (§6) | Apply the flag rule to every recorded stop, with and without the partition | No false flags without the partition (the partition is unnecessary) |
| F8 | Slip invisibility on periodic maps (§3.1) | On the conceptual all-alternating map (M1 §9.1), inject a deletion | Q-based coherence detects it (that would refute §3.1) |
| F9 | Incremental E_j fits the budget (§5) | Bench ESP32 with a recorded strip (no locomotive) | Over 5% of a core, or the template does not fit in RAM |
| F10 TRACK | Real missed region | Only if separately authorized: run over a deliberately weakened or shielded ensemble | — (gives the first non-injected slip case) |

F1, F4 and F8 are labeled injections. F2, F3 and F5–F7 use original
records only.

---

## 10. Limits of this review

- No replay was run. The numbers in §3–§5 are arithmetic from the stated
  widths, ranges and rates.
- The October 17 baseline-extent report is not on this branch. Its figures
  are taken as M1 states them.
- Per AGENTS.md §8, none of this establishes railway behavior. The highest
  value offline work is F1, F2, F4 and F6. Together they answer the
  double-count, the central predicate, the station-safety horizon and the
  coast question without hardware.
