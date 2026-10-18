# Operator's follow-up assignment

Repeat the statistical shape-recognition experiment at commit 804f1a2, changing one experimental condition:

Require a magnetic candidate to exhibit a principal excursion of at least 70 ADC counts relative to its local baseline.

Preserve the existing statistical shape models, reference data, evaluation partitions, and scoring methods.

Apply the 70-count requirement before accepting a candidate as a magnetic ensemble. Do not use it to truncate the waveform or exclude associated lobes and shoulders.

The complete ensemble, including its transitions to baseline, must remain available for shape recognition.

Reevaluate Models A and B on the identical continuous recordings.

Report:

* Correctly recognized magnetic ensembles.
* Missed magnetic ensembles.
* False detections within baselines.
* Incorrect splitting of magnetic ensembles.
* Ensemble boundary and midpoint accuracy.

Compare the new results directly with the original experiment.

Do not add any other thresholds, movement estimates, recovery mechanisms, or changes to the recognition architecture.

Preserve the original results and commit the revised experiment separately.

No firmware or operational changes are authorized.

## Why this experiment matters

Codex reported that genuine principal magnetic excursions in the reference were substantially larger than ordinary baseline variations.

The original experiment deliberately removed amplitude information. Your proposed test restores a simple physical distinction that is obvious in the tracings.

If the 70-count requirement eliminates most false baseline detections without introducing missed magnets, we will have demonstrated that amplitude and statistical shape recognition can work together.
