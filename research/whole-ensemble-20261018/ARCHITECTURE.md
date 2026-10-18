# Implementation and reconciliation

Research hypothesis only; no navigation architecture or deployment decision. The operator's physical definition remains the test, not a consequence of the latent state names.

## Representation

`functional.py:basis` defines a whole supported function

`phi_j(u) = sin(pi u)^2 cos(j pi u), 0 <= u <= 1`.

All coefficients belong to one field. There is no independently detected principal, lobe or shoulder. Their joint distribution is `w_MM ~ N(mu_MM, Sigma_MM)`, with **full**, rather than phase-diagonal, covariance. It preserves raw ADC amplitudes and uniform temporal phase. Value and first derivative vanish at both ends, so a state meets the common background curve continuously. BL has a separate three-coefficient curved-function distribution. MM has twelve coefficients. Polarity and reversal transform the entire vector and covariance together (`modes`, `canonical`). A reversal never independently flips a lobe.

The observation model is `y_i = b_i + phi(u_i) w_state + epsilon_i`. The actual observations are the same **20-ms native-recording means** used in the controls, not all individual 5-kHz samples. Original native observations remain inventoried and preserved. There is no RMS normalization, fixed ADC background band, 70-count gate, excursion list, expected arrival, speed, distance, NAVI state or PWM input in `functional.py`.

The shared `b` is a smooth latent curve with second-difference Gaussian prior. It is estimated over the entire eligible passage (`smooth`), not separately from candidate endpoints. BL is an explicit random curved state with a likelihood, not the complement of accepted MM windows. `epsilon` is Student-t with five degrees of freedom. Positive continuous latent noise precisions discount large residuals; no sample is deleted and no spike acceptance threshold is introduced.

## Integrated likelihood and joint path

For one proposed latent state's support, define `r=y-b`, `W=diag(lambda/sigma^2)`, design `Phi`, prior covariance `Sigma`, prior mean `mu`:

`Lambda = Sigma^-1 + Phi' W Phi`

`q = Sigma^-1 mu + Phi' W r`

`log p(r | state) - log p(r | background noise)`

`= (q' Lambda^-1 q - mu' Sigma^-1 mu - log|Sigma| - log|Lambda|)/2`.

`gains` computes this integrated factor. It marginalizes the discrete whole-function nuisance alternatives and, for B, geographic means with equal weights. The common background-noise term cancels between partitions because every source observation belongs to exactly one state; it is not a positive-score admission test. Posterior coefficient reconstruction uses the highest-weight discrete mode, not a mixture-averaged function.

`coarse_path` compares **full alternating partitions** of the eligible passage. Its two recurrences are:

`D_MM(t) = max_s [D_BL(s) + L_MM(s,t) - log(N)]`

`D_BL(t) = max_s [D_MM(s) + L_BL(s,t)]`.

`log(N)` is the frozen BIC complexity cost for two MM boundary parameters. No state is independently accepted on `L_MM-L_BL > 0`. Adjacent states share their boundary variable. Every duration represented by the 80-ms numerical lattice is considered, including the entire passage. Initial and final states can be either class and are censored at passage edges. There is no greedy overlap selection or later lobe rejection/deduplication.

`refine` jointly optimizes every internal boundary on the 20-ms grid within ±80 ms of the coarse boundary, preserving the coarse topology and using the same objective. There is no second physical admission rule. The support endpoints are therefore latent transition hypotheses chosen by the joint fit. A smooth endpoint constraint does **not** prove that the selected endpoint is the actual outer magnetic-field transition.

`infer` performs three fixed coordinate passes: conditional joint path, coefficient posterior, shared background, then Gamma noise-precision update. It is an approximate hard-path scale-mixture procedure, using a MAP discrete orientation for reconstruction. It is not a globally exact variational posterior or a guaranteed monotone optimizer. The final returned coefficients were inferred before the final background update. All those approximations are part of this frozen test.

For sufficient-statistic computation, `stats` stores weighted counts and weighted residual sums. It does not explicitly accumulate the squared-residual prefix described in the protocol: that term cancels identically between complete partitions. Raw ADC observations remain available for noise updates and residual diagnostics; their squared-residual evidence is not dropped selectively. A 64-bin uniform-phase quadrature approximates the basis over each latent support (`stats`). Fractional source-bin mass is apportioned by linear prefix interpolation. This is a numerical approximation, especially on short states. No phase samples are removed because they lie below an excursion threshold.

## Learning and reference provenance

`experiment.py:initial` uses eligible training observations only. Noise scale is MAD of successive 20-ms ADC differences divided by sqrt(2). Initial MM amplitude scale is RMS of passage-median-centered training ADC. Initial MM mean has only its first coefficient nonzero; diagonal variance decays as `(j+1)^-4`. Initial BL coefficient scale is three times the learned measurement-noise scale, with the same decay. The shared-background second-difference scale is the MAD of second differences of consecutive 64-source-bin block medians divided by 64 squared. These are frozen initialization/prior choices, not amplitude admission gates. Their substantive influence is an experimental limitation.

`main(train)` performs exactly three training updates, each using the same three-pass recognizer. `moments` combines whole-vector coefficient means and posterior covariances, with K diagonal pseudo-observations for covariance shrinkage. A fourth replay records provenance at final frozen parameters without another update. Unsupervised training includes every inferred closed state of its respective class. **That can contaminate the learned MM class with wrongly partitioned source material.** A whole-function parameterization alone does not prevent this failure.

NAVI seeds are associated only after inference (`objects_for`); they supply geographic hypotheses, never MM candidate locations or admission. `all171_reference_audit.csv` reports how many closed inferred states in the **third parameter-construction update** have an unambiguous single geographic seed at each location, and separately the final replay count. Its IDs therefore identify the actual A parameter-construction cohort, rather than silently substituting the fourth replay. This is reference provenance, not proof those states contain complete magnetic ensembles.

`conditional` restricts B's reference evidence to unambiguous MM050–MM070 training states and their actual adjacent BL states. Each conditional mean is shrunk with one focused-pool pseudo-observation. The full covariance is completely pooled **within the focused cohort**, shared across its geographic distributions; individual covariances are not independently estimated. The shared covariance is a strong sparse-data modeling assumption, disclosed rather than called 21 separately estimated covariance portraits. A's pool uses all training inferred MM/BL states. Noise, background prior, recognizer, source eligibility, and evaluation are identical for A and B. B is replayed without geographic input over all eligible evaluation spans; only post-hoc reporting selects the focused geography. B's `geographic_reference` metadata records its actual 65 MM and 73 unique BL reference states; its inherited `training_counts` field records the preceding A fitting update, not 795 B references.

## Overlapping units and limits

`assess.py:units` serializes existing `MM_k, BL_k, MM_(k+1)` objects. The following unit reuses exactly the same central MM ID. Counting uses unique MM objects, not unit appearances. This guarantees bookkeeping consistency only. If inference assigns two MM states to one physical field, it has already failed grouping: that is reported, never repaired.

A latent alternating sequence represents an MM–BL–MM relationship and allows surrounding states to influence boundary selection. Its likelihood still factorizes into whole-state functions conditional on the shared background; it does **not** learn a higher-order joint covariance across two neighboring magnets and their baseline, nor encode surveyed geographic progression. No such stronger recognition claim follows from the serialized unit objects.

Compared with the controls, the new hypothesis replaces independently normalized candidate-window classification and greedy overlap suppression with raw-amplitude full-function distributions, explicit curved BL states, a common background, and a joint continuous partition. The physical test remains independent source morphology: complete leading feature, principal, secondary feature, recovery, boundaries and one count. If this model partitions those into multiple MM states, calling its states “whole functions” does not make it a successful complete-ensemble recognizer.
