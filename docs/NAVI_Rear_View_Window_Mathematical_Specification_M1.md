# NAVI_Rear_View_Window — Mathematical Specification M1
**17 October 2026 · Review candidate, not authorized firmware**

Companion: [Architecture](NAVI_Rear_View_Window_Architecture.md). Previous: [M0](NAVI_Rear_View_Window_Mathematical_Specification_M0.md). Independent M0 review: Claude branch `claude/navi-rvm-m0-review-20261017`. M1 incorporates the operator's subsequent **linear puzzle-board** clarification. It does not claim validation.

## 1. Fundamental model: one expected piece

The **board** is the fixed ordered geographic MM map plus the validated fuzzy landscape. The **successfully placed pieces** are trusted accumulated pattern history. The next observed piece has **one expected place** on the board, not 171 candidate positions.

Let j be the unwrapped expected tile ordinal (j mod N indexes the cyclic map). The overlapping piece is

    P_j = (MM_(j-1), intervening BL, MM_j).

P_(j+1) overlaps P_j at MM_j. This overlap preserves context; it does **not** mean an anomalous MM_j becomes the next measurement anchor.

**Updated operator commitment rule:** When the *appearance of MM_j* makes the developing MM–BL–MM piece recognizable at its expected board position, issue one irreversible TILE(j) instruction. The new MM need not itself finish and return to BL before this token. This **supersedes M0's later-completion timing**. “Appearance” means evidence of the expected magnetic region within the continuous pattern, **not** a mandatory isolated Hall opening threshold. Its numerical criterion remains to be derived from replay.

A locally imperfect piece normally advances the count if progression is supported; it contributes an anomaly, **not** a new board, geographic origin or next-piece specification. Five **successive collectively incoherent pieces** are the proposed LOSS OF PATTERN horizon (not five ADC or lobe failures). A later good piece ends the consecutive mismatch run, while its preceding anomalies remain in the record.

## 2. State and invariant

Keep (boot/run ID, declared direction, unwrapped committed tile j_c, expected j_e=j_c+1, last trusted **multi-piece** alignment A, causal live waveform summary R, long-horizon coherence C_long, recent piece coherence C_recent, consecutive mismatch count, PWM history, IR-internal health, Hall health, model version). No second independently drifting geographic estimator.

**Invariant:** The board and the trusted history never change because one observation differs. MM050–060 match; MM061 differs but a new magnetic region supports progression → count 061 once, record anomaly, **do not reanchor**. Compare MM062 against the expected continuation from the board and trusted coherent history through MM060, not against a distance measured from bad MM061. If 062 matches, coherence resumes. If 062 also differs, keep evaluating 063. Never reset the pattern to force a local match.

Each controller token is idempotent:

    T_j=(boot_id,run_id,unwrapped_j,source_serial_range,model_version,commit_time,quality).

An already executed token cannot be retracted. Later disagreement is diagnostic; if accumulated evidence makes location indefensible, controlled STOP and operator redeclaration. Do not silently shift geographic count.

## 3. Signal and fuzzy brush

Preserve native Hall samples, PWM, IR source timestamps, acquisition gaps and provenance. The recognizer compares **continuous** Hall traces, not mandatory per-lobe segmentation.

Candidate *causal time-domain* smoothing:

    y_i = SUM_(l<=i) k(t_i-t_l) q_l h_l / SUM_(l<=i) k(t_i-t_l) q_l

where k is a short causal kernel, q_l is acquisition eligibility, and no gap is filled with synthetic samples. Prefer time-domain smoothing to M0's IR/PWM-derived distance-coordinate brush: unreliable odometry must not secretly reshape Hall evidence. Speed tolerance belongs in pattern registration. Test robust smoothing, Gaussian and boxcar against held-out recordings; 20–50 ms is an experimental starting range, not a prescribed filter.

A flat *leading* lobe displaced significantly from the established BL is magnetic evidence even if its slope is small. A flat *trailing* lobe may resemble BL but cannot independently create a new tile; the next overlapping MM–BL–MM pattern must fit the board. Preserve broad deflection, polarity information as optional evidence, absolute swing, and baseline drift. Do not rebaseline during PWM 0.

For normalization use the identifiable candidate

    h_i = b_i + a_run * A_ref(s_i) * g_ref(s_i) + noise_i

with one slow run-wide gain a_run, fixed learned local amplitude A_ref, and baseline b_i estimated only from credible geographically supported BL. Do **not** fit an unconstrained per-magnet gain that would erase a >50% amplitude loss.

## 4. Statistical fuzzy reference

Use **complete validated laps only**, beginning with existing successful CW data. E-STOP disqualifies a lap from learning, not archival. Preserve original recordings and model provenance. Align full-lap waveforms monotonically to the known map; do not certify a lap solely because it fits the model it would update.

Initial candidate K≈20 recent validated laps, recency weights

    w_l = exp(-lambda * age_l) / SUM_m exp(-lambda * age_m).

Build a robust expected center m(s) and fuzzy dispersion corridor. With correlated laps, effective count is

    n_eff = (SUM w_l)^2 / SUM(w_l^2).

Prefer a candidate shrunk dispersion

    d_shrunk(s)=[n_eff*d_local(s)+n0*d_pool(s)]/(n_eff+n0),
    corridor(s)=m(s) ± k*d_shrunk(s).

Calibrate k, K, lambda and spatial resolution on **held-out days**, not the same laps used for learning. The corridor is **soft**, not a pointwise veto. Small validated variation is absorbed. Keep a separate, non-navigational long-term diagnostic reference to expose gradual degradation; it never overrides the adaptive navigation board.

## 5. Pattern scoring and the missing numerical predicate

At any instant the recognizer tests the **one expected piece P_j** against its expected board location and preceding trusted context. Local phase/time-warp variables are numerical nuisance parameters, **not competing geographic identities**.

Candidate robust whole-piece cost:

    E_j(t)=MIN_(admissible monotone warps phi)
           SUM_i rho( y_i - m(phi(t_i)) )/[d(phi(t_i))+epsilon].

The fit must preserve ordering, allow different speed and stall, limit unrealistic warping and cap outlier influence. A whole-piece score alone does not decide whether another physical MM has appeared.

**Separate two judgments:**
1. **Progression evidence G_j:** Did a new physical magnetic region appear after the intervening BL, consistent with the expected ongoing board position?
2. **Piece quality Q_j:** How closely does that developing piece resemble the learned statistical corridor?

An imperfect Q_j is **not** an automatic veto on progression. A convincing G_j produces one token even if Q_j is atypical; record the discrepancy without reanchoring.

Candidate causal likelihood ratio for progression:

    L_j(t)=log P(live trace | expected new region has appeared, map, history)
           -log P(live trace | still in preceding region or BL, map, history).

The alternative is **no progression yet**, not another geographic tile. Compare (A) a local semi-Markov progression model, (B) robust expected-piece correlation, and (C) restricted monotone DP/path convergence confined to previous-versus-next region. **No algorithm or threshold has been approved.** Derive token criterion from held-out true/false advancements and station timing. Overlapping samples are correlated; do not multiply their likelihoods as independent evidence.

A completed piece can be **counted but anomalous**; only multi-piece coherent evidence promotes a trusted anchor. A missing physical ensemble cannot be replaced with manufactured Hall samples. Retrospective inferred progression must be marked as such; authority to issue tile instructions when the ensemble is absent remains a safety-critical validation question.

## 6. Motion, independence and nonprogression

PWM applied over time supplies the **normal expectation** of progression, informed by grade, ramps and historical locomotive response. PWM does not prove displacement; stalls at positive PWM occur, and there is no guaranteed positive minimum speed. Do not set a historical maximum as a physical downhill limit.

IR is supporting evidence, **never a veto** on coherent Hall/map progression. Candidate capped cost:

    cost_IR(delta)=gamma_i * MIN(Huber(delta-IRdelta_i), cap_IR).

gamma_i must be determined from **IR-internal integrity and source timing**, not agreement with Hall. Otherwise IR cannot independently disagree. PWM and IR both concern drivetrain motion and are not independent proof of movement on rails when wheels slip. **Quick to condemn, quick to forgive:** bad Hall or IR data cannot prohibit consideration of the next observation. No sticky strikes.

**LOSS OF PROGRESSION:** commanded motion, accumulated evidence shows physical nonprogression → E-STOP, inspection, operator redeclaration. A stationary Hall trace in BL alone cannot establish a stall; Hall sensor failure can imitate stillness. **LOSS OF PATTERN:** five proposed collectively incoherent expected pieces, or earlier genuinely indefensible location when independent safety requires it → controlled STOP and redeclaration. Test the five-piece number against actual failure distance and station approaches.

## 7. PWM zero, stops and unexpected movement

At PWM=0 preserve all data but do not automatically advance geographic phase, declare new BL, create a new MM or rebaseline. A powered hour-long pause inside an ensemble must RESUME with identical geographic memory and no duplicate token.

**Unexpected evidence of movement during PWM=0 requires investigation at the time.** Flag and preserve synchronized Hall/IR/PWM observations, notify the operator, and require inspection/redeclaration before automatic resumption if physical displacement is plausible. The October 17 research found 35 recorded IR pulses (~338 *nominal wheel* mm) during 13 seconds at PWM zero. This does **not** prove 338 mm track displacement; wheel rotation, handling, slip and sensor faults remain possible.

## 8. Station behavior, maintenance and model activation

Recognizing the *appearance of the next MM* differs from M0's later completion rule. Quantify causal token timing against existing Hall-opening triggers. Recognition latency is a **measurement, not the optimization goal**. Otto's body has operated approximately one MM ahead of the Hall sensor; this does not automatically cancel additional delay. Preserve current station landmarks initially; allow independent future configuration of every station in CW and CCW. No in-run reversal behavior is included.

Major local changes (missing ensemble or >50% absolute swing reduction are provisional examples) recurring at the same location for **five consecutive rounds** → controlled STOP, notify, require inspection, optional adjustment, operator restart, evaluate next complete round. This is separate from five successive bad *pieces* in one run.

Pi validates complete laps and prepares model versions. ESP32 stores a local model and navigates independently of network. Stage and integrity-check updates, activate only at a **committed piece boundary** without phase or tile jump, and retain rollback. Do not introduce a second undefined live old/new consistency veto.

## 9. Required replay and independent review

Before coding, test:
1. Correct one-place board progression on held-out complete laps, including an all-alternating N/S conceptual map.
2. MM050–060 coherent → 061 weak/displaced → 062 normal; then 061–063 atypical; no reanchor or duplicate tokens.
3. Multi-lobe, flat leading and trailing lobes; one physical magnet must not produce two pieces.
4. Genuine BL versus flat impostor, using October 17 evidence (reviewed within-ensemble cores 9–52 nominal mm versus between-ensemble 103–234 nominal mm, **uncertainty overlaps**); no hard cutoff inferred.
5. Missing entire ensemble, recording gaps and one-MM slip; measure **identifiability limits**, not just success rate.
6. IR undercount, overcount, missing beats and immediate return to credibility.
7. Positive-PWM stalls and PWM-zero 13-second pulse episode, plus 60-minute stopped-on-magnet RESUME.
8. Five consecutive incoherent pieces, false STOPs and distance to failure recognition.
9. Station token timing versus existing opening-based instructions.
10. Complete-lap-only learning, slow drift, model swap without phase jump, ESP32 RAM/CPU/queue impact.

Report every emitted token with expected MM, evidence interval, progression score, piece quality, trusted anchor, discrepancy streak, applied PWM, IR integrity, model version and commit lag. Require an independent reviewer to challenge the causal commitment predicate, unidentifiable missing-MM cases and safety near station stops. **No firmware changes are authorized by M1.**

## 10. Alternatives preserved

- **M0 global banded DTW / multi-position frontier:** deferred because ordinary operation has one map-indexed next piece; internal *local* warping may still be useful.
- **Path-convergence or posterior-commit across geographic identities:** deferred; operator chose overlapping puzzle-piece commitment, but its *numerical causal evidence threshold* remains open.
- **Rigid per-lobe/baseline threshold:** rejected as brittle and inconsistent with whole-pattern recognition.
- **Per-magnet reanchoring:** rejected because a bad MM061 would corrupt MM062.
- **IR-based spatial smoothing:** deferred because it lets flawed IR reshape the primary Hall trace.
- **Permanent golden navigation waveform or unlimited unweighted history:** rejected as stale or resistant to change; frozen *diagnostic* history remains an option.
- **Stop on first mismatch / missing magnet:** rejected because the accumulated map-backed pattern supports continued progression.
- **Learning from partial laps:** rejected to prevent self-certifying feedback.
- **Station recalibration now:** deferred until basic navigation works and timing is measured.
