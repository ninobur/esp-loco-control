# Frozen research protocol

Operator authorization: `sources/ASSIGNMENT.txt`. Research code and offline evaluation only. Base commit: `6aea978ae45e7164d176dfdda6d38ff41e8f272b`. This package applies direct observation before proxy and preserves contradictions. No departure from those principles is proposed. No operational architectural decision is made.

Written before model evaluation. One agent. No reuse of either prohibited recognizer, likelihood implementation, or alignment kernel. Only recording-format documentation and prior evidence/provenance are used.

## Partition

Boot `15549e9385faa4d5`: training before 1840.213501 s and 2745.450837–3167.014139 s; evaluation 1840.213501–2745.450837 s and after 3167.014139 s. Boot `4d8083de79a0e299`: supplementary evaluation, never training. Intervals crossing a partition boundary remain unresolved. All are previously examined development recordings; none is a fresh blind holdout. CW second complete lap is a named evaluation stratum. All reference geography is an annotation hypothesis.

## Dataset construction convention

Use original native ADC, nonoverlapping 20-ms sample-weighted means on the boot clock. Retain counts, native row ranges, serial ranges and gaps. Do not fill gaps. Do not read PWM or IR into recognition.

Existing MM opening annotations seed *research annotation cells*, from the midpoint with the previous opening to the midpoint with the next opening in the same directional passage. A break in the recorded MM sequence creates unresolved cells rather than inferred magnets. Within a cell, subtract the straight line between medians of the first/last tenth; the 0.1%–99.9% cumulative squared residual-energy span is a reproducible **provisional ensemble envelope**. Extend by one 20-ms bin. This is amplitude invariant and includes both polarities, lobes and shoulders by their contribution to the complete contour, rather than Hall threshold crossings. It is not verified physical field extent. Noise/drift can widen it and weak shoulders can fall outside it. The intervening envelope complements are provisional BL labels. Preserve every annotation and unresolved case. No earlier detector's acceptance decision or threshold boundary defines this dataset.

This convention uses labels to prepare examples, not to locate recognition windows. It cannot establish independent discovery sensitivity. Its dependence on neighboring openings must be disclosed with all interval-classification results. Include boundary sensitivity using energy tails 0.5% and 1%, without changing the primary detector.

## Common statistical model

Uniformly resample the **entire** candidate interval to 64 phase bins using area integration; no point skipping, dynamic programming or local warping. Subtract mean; divide by RMS; canonicalize principal polarity using excursion from endpoint level. Train with time reversal augmentation. Preserve polarity separately. All lobes inside the interval remain in the representation. Numerically constant profiles are unresolved, never divided by a noise floor or assigned by amplitude. No amplitude, elapsed-duration, movement or arrival acceptance gate.

Estimate a Gaussian functional profile: 64 means and 64 diagonal variances; shrink variances 50% toward pooled class variance, with fixed numerical ridge 0.05 normalized units squared. Equal MM/BL priors. Class decision is the difference of complete-profile Gaussian log densities. A tie is unresolved. All preprocessing, variance estimation and decision rules are identical for A/B. Model A pools all MM labels and BLs. Model B fits each MM050–MM070 and its following BL; no geographic model outside that range. B's primary interval comparison uses the supplied annotation as expected MM (an oracle advantage, disclosed); also report a label-free union over its 21 references. No individual historical waveform is required to match exactly.

## Continuous experiment

Same profiles and score. Scan candidate windows with lengths `8,12,18,27,40,60,90,135,202,303,454,681,1022` 20-ms bins (0.16–20.44 s), every 5 bins. These are computational scales, not qualification gates. Windows touching loss are unresolved. Select positive-margin intervals in descending margin, suppressing overlapping intervals. No minimum gap, spacing, sequence, movement or predicted arrival rule. Uncovered time is provisional BL. This simple grouping may split lobes or merge neighboring ensembles; measure those failures rather than adding a recovery mechanism.

For a fair local scan, run A and label-free B on exactly the same focused passage spans. A is additionally scanned on the entire evaluation clock intervals, including unannotated activity and stationary exposure. Unassigned detections there have unknown physical identity, not a confirmed false-positive label. B never sees other geography. Oracle B is not a continuous tracking result.

## Measures and limitations

Retain all MM/BL rows, including unavailable references and loss-censored profiles. Report resolved confusion counts, unresolved counts and correctness with unresolved included in denominator. Profile separation: ROC AUC and score quantiles. Report individual geographic and recording strata. Count continuous events against annotation envelopes by maximum intersection-over-union one-to-one matching (IoU >=0.3 reporting convention); unmatched detections overlapping an already matched envelope are possible lobe splits, other unmatched detections are unassigned false positives. Distinguish detections wholly within a BL. Report start/end/midpoint error conditional on matches, misses in denominator, and polarity against envelope polarity (not independent magnetic metrology).

Report observed envelope RMS and width strata after recognition; do not estimate speed. Test mathematical invariance by multiplying/offsetting full profiles and uniformly changing time coordinates. Synthetic checks are arithmetic tests, not evidence about operating speeds or signal-to-noise changes. Compare disk/bar metadata only as an exploratory association; do not equate bar with independently verified ballast construction. No parameter retuning after evaluation. If grouping or class separation fails, report the failure and stop algorithm development.
