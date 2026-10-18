# Frozen 70-ADC repeat protocol

Authorized by David's follow-up requesting one changed experimental condition. Base: `804f1a2d9306d7c2d19c9ee1be69be4a5ca2fc30`. This instruction explicitly authorizes the 70-count amplitude requirement for this offline experiment, superseding the original assignment's amplitude-gate prohibition only to that extent. No firmware or operating-system work is authorized or performed.

The original package `../statistical-shape-20261018/` is immutable. Load its saved models without refitting, exact Hall tables, annotations, loss masks and `results/scan_spans.json`. Use its `features`, `scores` and `match_events` functions directly. A is evaluated on all original full spans and focused spans. B uses the same label-free mixture of 21 geographic models on the original focused spans only. No oracle geography is introduced into the continuous scanner.

## The single changed condition

For a candidate `[lo,hi)`, reconstruct the same unnormalized 64 phase-bin means used by the original area-integrating feature extractor. Let `b` equal the mean of the first six and last six phase-bin means, exactly the endpoint-level baseline estimator already used by the original polarity calculation.

Measure principal excursion on the existing 20-ms ADC tracing, before amplitude normalization:

`E = max(abs(max(y[lo:hi]) - b), abs(min(y[lo:hi]) - b))`.

Either principal polarity qualifies. Require `E >= 70.0` ADC counts, inclusively. The peak is measured before the 64-bin averaging can attenuate a narrow excursion. The local baseline is candidate-specific and has no geographic, timing, PWM, movement or global ADC-level input. It remains an endpoint estimate, not independently measured field-free ground.

Original acceptance into overlap suppression: finite positive statistical shape score. Revised acceptance: the same finite positive score **and** `E >= 70.0`. Apply the requirement before the original greedy overlap suppression; rejected low-excursion windows cannot suppress qualifying windows. Do not change ranking, tie order, candidate scales, stride, missing-data handling, scoring or overlap suppression.

The amplitude check never resizes a candidate, finds threshold-crossing boundaries, removes a shoulder/lobe, or changes any value supplied to `features()`. Every original candidate window is scored in full. The previous experiment's physical completeness limitations remain: finite windows and provisional reference envelopes are not verified physical field boundaries. No new grouping, envelope-extension or recovery mechanism is added.

## Paired evaluation and controls

Generate candidate features and scores once, then select original and 70-count conditions from those identical scored windows. Reproduce original events and summary rows against the published artifacts, not merely approximate totals. Log every positive-shape candidate's baseline, peak, excursion, score, amplitude eligibility and selection status under both conditions.

Preserve all original 623 full-scan MM annotations and all 76 focused MM annotations, including the original 45 and four unresolved cases. Use unchanged one-to-one IoU >=0.3 matching and original definitions of baseline-contained detections, possible lobe splits, unassigned detections and errors. Report newly lost/gained annotation matches individually; distinguish usable missed ensembles from unresolved annotations. Do not assert independent physical geography or exact lobe attribution.

Compare beginning, end and midpoint errors for all matched events and for the same annotations matched under both conditions. Also report baseline-time occupancy, candidate rejection counts and original unassigned detections. Match rates alone must not conceal splitting or overly broad detections.

Validate: original artifact hashes unchanged; saved model hash identical; original-event replay exactly reproduced; acceptance at exactly 70 and rejection below 70; either polarity; whole-window feature identity; no time/local amplitude cropping; all denominators unchanged. Use the same fixed threshold across every recording and both models. No tuning after results are read. If the change causes misses or leaves grouping failures, report them without adding conditions or recovery.
