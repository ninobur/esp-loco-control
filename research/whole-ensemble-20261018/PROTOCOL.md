# Corrected whole-ensemble experiment: frozen recognition contract

Authority: [the corrected assignment](sources/ASSIGNMENT.txt). Research only. Base `8c40d6dab0b957e6884eb81b2836c5a5fa2062cd`; preserve both prior experiments unchanged. No operational architectural decision or deployment is made. One agent.

## The model being tested

Fit one generative model of an entire eligible continuous ADC passage. Its latent structure is `BL → MM → BL → MM → BL`. An MM state is one **complete random function**, not a set of separately detected or accepted lobes. A BL state has its own random curved-function distribution. All states share a continuous latent background curve. No interval is independently admitted as a magnet by an amplitude or shape-score threshold.

For native 20-ms observation `i`,

`y_i = b_i + Phi_state(u_i) w_state + epsilon_i`.

`b` is a common smooth background curve, with a Gaussian second-difference prior. Its mean level is inferred, not fixed to an ADC band. `epsilon` is independent Student-t measurement noise, represented by latent Gamma precisions. Those precisions are continuous positive weights in the same likelihood, never spike-deletion or magnetic admission gates.

An MM's phase runs uniformly from zero to one across its whole latent support. Its basis is `sin(pi*u)^2 * cos(j*pi*u)`, `j=0..11`. One full covariance matrix over the 12 coefficients learns amplitude, principal/secondary excursions, asymmetry, lobes and shoulders together. Basis value and derivative vanish at both support endpoints: each field departs from and returns continuously to the shared background. There is no local time warping or phase skipping. Whole-function polarity reversal and time reversal are nuisance alternatives, not independent fragments. No RMS or height normalization is performed.

A BL's basis uses the first three of those smooth functions, with its own mean and full covariance learned from BL observations. Together with `b`, it represents gently curved background portions. BL has an actual likelihood; it is not whatever remains unclassified.

The continuous latent sequence is inferred jointly, with boundaries shared by adjacent states. A whole-passage dynamic program compares complete alternating state paths using integrated functional likelihoods. It does not use the previous classifier, its diagonal profile likelihood, its finite window list, or greedy overlap suppression. A statistical complexity penalty `log(N)` per complete MM accounts for its two free boundary parameters; it is not an amplitude, duration, distance, speed or arrival gate. All field durations available on the numerical lattice are considered, up to the eligible passage length. No mapped separation, expected arrival or movement value enters the model.

The shared background and observation precisions are latent variables inferred jointly with the state path by three fixed coordinate updates. Coefficients are analytically integrated conditional on the current precisions; the Student-t scale-mixture inference is variational. This is approximate posterior inference, not a claim of a globally exact nonlinear optimum.

For computation, coarse boundary spacing is four source bins (80 ms). Raw source observations are accumulated, with their weights and squared residuals, into 64 uniform phase quadrature bins. They are not amplitude-normalized or discarded. After the global coarse path is found, all its boundary variables are refined together on the 20-ms source grid within the neighboring coarse cells, using the same model. This is a numerical approximation whose resolution and sensitivity must be reported, not a physical duration qualification.

Adjacent units are the actual inferred `MM_k, BL_k, MM_(k+1)` objects. The following unit reuses the identical `MM_(k+1)` object and its boundaries. Count unique complete MM objects, not unit appearances. Components touching an excluded passage edge are censored; no unit crosses an excluded interval, and no waveform parts are concatenated across it. Report partial/censored observations as unresolved, never as completed magnetic traversals.

This contract makes complete-ensemble grouping the **hypothesis of the generative model**, not a guarantee that inference will be physically correct. Lobe ownership and field boundaries must be checked against visible source morphology and uncertainty. A failure must be reported without adding fragment rejection, recovery or another recognition mechanism.

## Reference construction and A/B comparison

Use the original source recordings and original training/evaluation clock partitions. Training uses only eligible moving observations. Initial noise/background statistics come from training data only. Whole-state fitting and three training updates learn the functional reference distributions; there is no fixed 70-count rule. Existing NAVI openings supply geographic research labels after whole-field inference, not candidate locations or acceptance requirements. Multiple/conflicting labels for one inferred field remain annotation conflicts. Model A's actual coverage of all 171 labels must be audited, not assumed.

Model B uses the same fitting, preprocessing, likelihood, inference and evaluation. Its reference distributions are conditional on MM050–MM070 and their associated BL states, with covariance shrinkage toward the pooled training class for sparse repeats. Its blind focused evaluation marginalizes those geographic reference distributions; it does not implement geographic tracking. Publish every reference ID and sample count, including missing or single-observation locations. Do not call a one-observation location a repeated statistical reference.

Evaluate the whole continuous eligible observations. Do not change hyperparameters for individual passages. Use all remaining source labels in accounting; distinguish missing eligibility, unresolved physical ownership and recognizer errors. Existing source labels alone do not establish a complete-field success. Report all ordinary annotation associations separately from morphology-adjudicated complete-ensemble, lobe and boundary results. No IoU-based association may be renamed physical completeness.

## Movement and exclusions

Dataset preparation is a separate program that cannot be imported by recognition. It checks native IR completed-pulse advances, source optical quality, record continuity, and held/E-stop operating state. Zero PWM is not stationary proof; powered absence of pulse evidence is not moving proof. Clean nonheld tracking runs are trimmed to actual advances at both ends. Internal quantized zero-increment snapshots are explicitly labeled interpulse continuity, rather than treated as independently confirmed motion. Stale, contradictory, held and unbracketed evidence remains uncertain/excluded. Publish the direct-advance and interpulse-supported durations separately and state the physical resolution limit.

No independently confirmed source-clock stationary intervals have yet been supplied. Stop-supported flat-count periods and the documented powered stall are conservatively excluded as uncertain. A pending operator clarification may strengthen their stationary designation; it must not silently turn uncertain motion into moving data. Retain the entire source inventory and report intervals/durations for every exclusion reason. Never join across excluded intervals.

## Evaluation and honesty controls

Publish complete MM and BL states, full-function fits and residuals, shared units/IDs, principal polarity, beginning/end/midpoint errors, baseline boundaries, lobe-ownership checks and duplicate counts. Annotate source morphology independently of evaluation predictions where possible; unreviewed or ambiguous cases remain unresolved. Report observed amplitude and temporal breadth after inference. Do not label duration as calibrated speed.

The previous two experiments remain controls. Different stationary exclusions and recognition definitions prevent claiming directly comparable success percentages. Preserve old files and hashes. Freeze this model before evaluating reserved passages. If the new model does not demonstrate complete structures and their intervening baselines, report failure or inconclusive evidence; do not substitute the old window-match metric.
