# NAVI_Rear_View_Window — Mathematical Specification M0
**17 October 2026 | Candidate for independent review | No firmware authorization**

Companion governing document: [NAVI_Rear_View_Window_Architecture.md](NAVI_Rear_View_Window_Architecture.md). This is a falsifiable mathematical proposal, not a claim of successful replay validation. Operator decisions govern; all mathematical choices and numeric cutoffs remain candidates until tested.

## 1. State, observations, and objective

Let N=171 mapped magnets (confirm from the authoritative map), L=loop length, j an **unwrapped** expected MM ordinal, and x(j) its known map coordinate, with x(j+N)=x(j)+L. The declared direction remains fixed during a run. Define s(t) as unwrapped continuous phase along the known landscape, not a new geographic search across all 171 alternatives.

Each immutable observation is O_i=(boot_id,serial_i,t_i,Hall_i,applied_PWM_i,IR_i,quality_i). Preserve source acquisition gaps, original timestamps and IR pulse integrity. The latent continuous Hall landscape is g(s); the versioned learned statistical model is M_v(s). The online recognizer maintains: a trusted *multi-feature* alignment anchor a, a local score frontier D_i(s), one best-supported phase estimate s_hat, one committed tile j_c, one next expected tile j_e, and independent diagnostics for sensor credibility and pattern coherence.

**Objective:** Maintain persistent agreement between the *whole* observed MM–BL–MM landscape and the fixed geographic reference; emit exactly one tile token per supported physical progression; execute existing tile orders. Never require perfect individual Hall lobes, polarity, IR intervals, or PWM odometry.

## 2. The fuzzy brush: causal, continuous, and auditable

A candidate causal low-pass representation is

    y_i = SUM_(l<=i) [K((xi_i-xi_l)/sigma) w_l Hall_l] /
          SUM_(l<=i) [K((xi_i-xi_l)/sigma) w_l]

where K(z)=exp(-z²/2) is one *candidate* kernel; xi is an auxiliary uncertain movement coordinate, **not** the trusted map phase; sigma is a calibrated smoothing scale; and w_l=0 for missing, corrupted, or PWM-zero geographic observations. If movement-coordinate quality is inadequate, compare a causal time-domain smoother and a robust local smoother instead. A recorded gap must not be interpolated into a manufactured physical baseline.

Offline symmetric smoothing may build reference models but is **forbidden** as evidence of live recognition timing. Measure causal filter lag and the corresponding station-order delay on actual recordings. Test 20–50 ms averaging as a research starting point, not a fixed requirement: the October 17 baseline-extent report found useful nominal separation there but loss/merging of short features under stronger smoothing.

Separate slow background b(s) and amplitude A(s) from waveform shape: Hall(s)≈b(s)+A(s)g(s)+noise. Fit b and A robustly using geographically supported context, **not** a flat magnetic lobe mistaken for new ground. Do not update either from PWM-zero observations. Constrain gain normalization and retain absolute ADC swing so that a >50% reduction remains visible as a maintenance anomaly.

## 3. Learn a fuzzy statistical corridor from complete laps

Use existing independently supported successful CW laps initially. Every training record must be a **complete, validated lap** with provenance, direction, map version, E-STOP status and acquisition coverage. E-STOP disqualifies learning, not archival. A new lap cannot validate itself merely by matching the model it would update.

Align each full-lap smoothed trace Y_l(s) monotonically to the fixed physical MM map. A selected first lap can supply an initial alignment scaffold, **not** permanent ground truth. Preserve uncertainty of alignment and exclude unresolved corrupted local observations from model updates without deleting them.

For K eligible recent laps (initial candidate K≈20), use recency weights

    w_l = exp(-lambda * age_l) / SUM_m exp(-lambda * age_m).

At each geographical coordinate s, compute a robust center m_v(s)=weighted_median(Y_l(s)), dispersion d_v(s) (regularized MAD), and soft predictive corridor [Q_(alpha/2)(s),Q_(1-alpha/2)(s)]. This is a *family of expected landscapes*, not one waveform or a pointwise acceptance gate. With ~20 laps, extreme empirical quantiles are unstable; calibrate whole-pattern predictive coverage by held-out complete laps. Estimate separate variation for baseline, amplitude, ensemble shape and alignment displacement. Tune K, lambda, alpha, smoothing and spatial resolution empirically. Do not move the physical map to explain sensor errors.

The Pi archives raw source and computes validated model versions. ESP32 retains a compact local verified model; Wi-Fi/MQTT/Pi outages must not stop navigation.

## 4. Persistent monotone pattern alignment

A candidate constrained dynamic-programming score for eligible moving samples is

    D_i(m) = rho(y_i, M_v(s_m)) +
             MIN_(m'<=m, m-m' in V_i) [D_(i-1)(m') + psi_i(m-m')].

Here s_m is a discrete geographic phase; rho is a robust *soft* waveform-to-statistical-corridor cost; psi is a contextual movement penalty; V_i is an empirically calibrated feasible **monotone** increment set. A fixed ±150 mm search because nominal magnets are 300 mm apart is not justified. Neither are mandatory per-lobe segmentation or hard ADC thresholds. This recurrence is a **candidate** constrained DTW/probabilistic alignment; compare it with robust multiscale correlation and left-to-right probabilistic alternatives in the same held-out replay.

The numerical frontier can contain nearby *phase hypotheses*, but controller output has **one committed geographic history**, never a carousel of alternative positions. A long coherent journey must remain represented in a persistent anchor and cumulative alignment state; the algorithm must not restart from the last magnet or a fixed five-magnet window.

### 4.1 PWM movement prior

Model phase increments conditional on *applied* PWM, elapsed moving time, local grade, ramps, battery and historical speed response. A candidate mixture is

    P(delta_s | PWM,context) =
        pi_stall P_stall(delta_s) +
        (1-pi_stall) P_move(delta_s | PWM,context).

Normal progression receives strong prior support, while delta_s=0 remains possible at positive PWM. PWM establishes **no guaranteed positive minimum movement**, and a historical top speed is not a physically justified maximum on a downslope. Do not set pi_stall or motion spread without representative runs. PWM remains mandatory predictive/protective context, not a geographic veto.

### 4.2 IR support without veto

Candidate bounded penalty:

    psi_IR(delta_s) = gamma_i * MIN[Huber(delta_s - IRdelta_i), cap_IR].

gamma_i depends on the **current** IR interval's pulse integrity, source timing and agreement with independently established broad progression. The penalty is capped and cannot by itself reject a coherent magnetic landscape. Do not treat IR as an always-undermeasuring lower bound. **Quick to condemn, quick to forgive:** an earlier bad IR interval cannot disqualify a fresh good one; the same fresh-evidence rule applies to Hall.

### 4.3 Outlier isolation: no inherited anchor error

Let a be the last trusted alignment anchor established by **multiple coherent magnetic regions**. Subsequent phase remains anchor-relative:

    s_hat_i = s_a + estimated_progress_from_whole_trace(a..i).

MM050–060 coherent; MM061 atypical: count MM061 once if the *continuing* pattern supports physical progression, but mark its apparent position/amplitude/IR distance **not trusted as a new anchor**. Compare MM062 to the expected continuation from the trusted multi-feature alignment, not to the bad MM061 measurement. If MM062 matches, the outlier is isolated; if it does not, look for MM063. The anomaly never resets the map or pattern.

A robust local cost alone does **not** guarantee no drift: the implementation must explicitly limit an outlier's effect on the phase frontier and prohibit anchor promotion without renewed multi-feature coherence. Verify with deliberate false-anchor, shifted-lobe and duplicated-lobe replay.

## 5. Coherence, persistence, and irreversible control

Carry two independent statistics: (i) **long-horizon** geographic coherence from the entire established journey, retained across individual anomalies; and (ii) **recent whole-pattern integrity** that can detect sustained failure even after 500 earlier good landmarks. One candidate evidence accumulator is

    S_i = clip(S_(i-1) + log[P(y_i|aligned)/P(y_i|no_match)], S_min,S_max).

Adjacent smoothed samples are correlated: **do not** multiply overlapping-window likelihoods as independent evidence. Calibrate with nonoverlapping increments or held-out full-sequence likelihoods. A simple global average alone is inadequate because 500 good features could conceal five recent bad ones.

**Five successive expected magnetic features of collectively incoherent landscape** is a *proposed persistence horizon*, not five independent per-MM pass/fail tests. Determine its actual false-stop rate and worst-case delayed-stop distance in replay. It is entirely separate from the **five consecutive rounds at one location** required for a major persistent maintenance discrepancy.

### 5.1 Tile token

On sufficient causal evidence that the expected MM ensemble has completed and returned to the recognizable BL in the continuing pattern, emit exactly one token

    T=(boot_id,run_id,unwrapped_j,source_serial_range,model_version,reason).

The controller executes that tile's existing orders once, idempotently. A multi-lobe field or replayed message must not create a second token. An atypical *recognized* ensemble may advance the count without becoming a trusted measurement anchor.

**Unresolved causality:** The strongest MM–BL–MM confirmation can arrive only as the *next* ensemble develops, while the operator requires tile instructions at completion of the preceding ensemble's return to baseline. Report (a) earliest causal evidence for MM–BL completion, (b) token time, (c) subsequent corroboration time, (d) station-order delay and (e) actual stopping impact. Do **not** silently insert provisional early advancement. If late confirmation endangers a station stop, obtain a new design decision or later station calibration; retain existing landmarks for initial testing.

An *apparently missing* ensemble is not a synthetic Hall observation. When later pattern establishes the next location, the count may be inferred with explicit provenance; **precise authority to issue station orders during such inference is unresolved and safety-critical**. Do not grant unlimited PWM-only tile advancement.

## 6. Stop, failure and recovery semantics

**PWM=0 ordinary STOP:** record samples for diagnosis but give them zero authority to advance geographic phase, rebaseline, complete an ensemble or create a magnet. Freeze the current tile, trusted anchor, developing ensemble and landscape history for any powered pause, including 60 minutes over a magnet. RESUME continues the same encounter. Require a genuine intervening BL before a new ensemble can be counted. Physical coasting/handling at PWM 0 is an explicit test caveat: October 17 research identified 35 IR pulse advances (~338 nominal wheel mm) during a 13-second PWM-zero episode, **not proof of signed track displacement**.

**LOSS OF PROGRESSION:** commanded positive PWM, magnetic progression ceases and corroborating evidence supports physical stall/obstruction. Request **E-STOP**, preserve evidence, require inspection and geographic redeclaration. Hall-stationary alone may mean Hall sensor failure; IR-silent alone may mean IR failure.

**LOSS OF PATTERN:** continuing evidence cannot establish a defensible tile after sustained whole-pattern incoherence. Controlled STOP and operator redeclaration. One bad magnet, missed Hall record or bad IR interval does not qualify. Independent safety protection remains authoritative. No separate autonomous geographic recovery mechanism is in scope.

## 7. Maintenance and live model update

Absorb small validated differences in amplitude, timing, width and apparent spacing. Preserve absolute-swing and geographic-location history to detect real deterioration. A major local change (e.g., absent ensemble or >50% swing reduction, provisional) repeated at the same location over **five consecutive rounds** causes controlled STOP and operator inspection; adjustment is optional if no repair is needed. Operator RESTART acknowledges inspection, not automatic learning approval. Evaluate the next complete lap before model update.

Model update: Pi builds and validates a new complete-lap reference; transfer versioned bytes with map/direction compatibility and checksum; ESP32 stages without disturbing navigation; activate atomically at a completed MM–BL–MM boundary, preserving unwrapped position, current tile, source history and anchor. Retain previous version for rollback. No additional vaguely defined live old/new similarity gate; **no-phase-jump testing is mandatory**.

## 8. Validation matrix and measurable pass/fail outputs

Replay held-out original Hall/IR/PWM records with their real source clocks and acquisition gaps. No fabricated labels or synthetic samples presented as source data. Synthetic corruption must be explicitly labeled as an injection.

1. **Whole-pattern recognition:** successful laps at differing speeds and smoothing scales; report tile accuracy, causal lag, statistical coverage and phase error against independently supported geography.
2. **Outlier isolation:** MM050–060 coherent, MM061 displaced/weak, MM062 normal; correct count, no MM061 anchor promotion, no phase drift. Repeat with two consecutive outliers.
3. **Lobes and baseline impostors:** no double count from same physical magnet; measure leading flat-lobe loss and trailing plateau confusion; verify 20–50 ms filter is not assumed universally optimal.
4. **Missing MM and gaps:** preserve provenance, avoid synthetic observations, recover subsequent coherent pattern; quantify station control risk and latest safe intervention.
5. **IR:** undercounts, overcounts, lost pulses and immediate restoration; no IR veto and no sticky sensor guilt.
6. **PWM:** Grillers stall at positive PWM; grade-accelerated travel; Hall-sensor silence versus real stall; distinguish failure modes.
7. **STOP on magnet:** 60-minute PWM-zero dwell and resume, with no rebaseline, duplicate MM, or position reset.
8. **Persistent failure:** five collectively incoherent features; report false-stop and detection delay; separate from five-round maintenance case.
9. **Learning:** no mid-lap update, E-STOP exclusion, bounded influence, recency adaptation without concealing gradual deterioration.
10. **ESP32 and network:** CPU, peak RAM, flash size, queue drops, end-to-end token latency, disconnected Pi operation, atomic live update and rollback.

**Required metrics:** per-tile event sequence and timing, duplicates, missed/false advancements, station stopping displacement, false protective stops per lap, worst-case delay to genuine failure detection, reference contamination, and computational/resource costs. Report counterexamples and confidence intervals. If no independent geographic ground truth exists, report the limitation rather than scoring prior detector labels as truth.

## 9. Independent-review questions and alternatives retained

- Does constrained DTW actually outperform robust correlation or a left-to-right probabilistic model on whole-waveform held-out data? Avoid unconstrained warping.
- Can the proposed robust alignment avoid a **one-MM slip** when the polarity sequence is repetitive and one physical ensemble disappears?
- Can a compact causal smoother recognize MM–BL completion early enough for existing station orders? Do not assume retrospective evidence is available immediately.
- How should a weak leading lobe be distinguished from baseline without normalizing away the deflection?
- Can model training distinguish a genuine gradual change from repeated sensor degradation without self-certification?
- Are PWM-zero physical movement and operator handling distinguishable from ordinary stationary dwell under current hardware?
- What quantitative criterion declares a tile no longer defensible when Hall fails but IR/PWM continue?
- How can reference activation preserve the phase frontier without requiring a new ad hoc consistency veto?

**Review instruction:** Challenge mathematical identifiability, causality, independence assumptions, computational bounds and feedback loops. Propose alternative equations and falsification experiments. Mark every operator requirement separately from unvalidated mathematical choices. Do not modify firmware or governing architecture as part of review.
