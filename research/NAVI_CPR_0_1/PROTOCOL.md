# NAVI_CPR_0_1 frozen development protocol

Authorization: sources/ASSIGNMENT.txt. Starting repository commit:
9e7b9208df1e9e06f75f208cd8a39fcec6b62cc7. One agent; offline Python only.
All numerical settings below are research hypotheses, not governing decisions.

## Sources and separation

Use Otto October 15 boot 15549e9385faa4d5, 100–1800 seconds, for construction.
This is inside the previously used construction partition (ending 1840.213501 s).
Use the first 60% of October 19 CW boot D004D9A3888E8394 for calibration;
its remaining 40% is a descriptive within-run evaluation. The entire October 19
CCW boot 3B1BE1D923CB81FD and Toby boot 6A31DD995673D123 are evaluation only.
These are previously examined recordings, not blind tests. Do not access reserved
boot 4d8083de79a0e299. Historical event annotations are comparison hypotheses,
not independent proof of physical magnets, polarity, or geographic correctness.
The known MM101 case is a prespecified case study, not a fresh test.

NSR CRC validation, native serial ordering and loss remain visible. Native
samples are aggregated into nonoverlapping 20 ms arithmetic means, available
only at the bin end. Bins with <50 reads or a touching serial discontinuity are
missing; never interpolate gaps. Recognition reads Hall and timestamps only.
Native reads also provide an independent exact two-consecutive-sample 70-count
witness, using the baseline available BEFORE that block (held during the block).
Detect on the second native sample; candidate opening does not declare a magnet.
The historical governing sample filter/baseline might differ; no equivalence is
claimed. CPR final recognition additionally requires a witness belonging to the
completed event. This is a research coexistence proposal, not firmware approval.

## Components and alternatives

Baseline: two-second bounded history; initial one-second warmup, median level;
compare robust local-linear extrapolation (iterative Huber weighted fit) with
clipped exponential tracking (2 s time constant). Refitting every five bins;
slow residual noise updated only on admitted background. Freeze level, slope
and noise during provisional development, extrapolate frozen slope. Candidate
start needs three bins outside max(4 counts, 4 noise); retain 300 ms prehistory.
No first-peak, reversal, shoulder or sign-crossing completion rule.

Completion: consecutive bins inside max(3 counts, 3 noise), with range no more
than twice that band, for 0.12, 0.24 or 0.48 seconds. Compare all six baseline/
return combinations on training/calibration only; choose by segmentation F1
against historical time intervals (IoU >=0.3), penalizing splits/merges through
one-to-one matching. Ties prefer 0.24 s then local-linear. No retuning on test.
Estimated end is first return-bin start; declaration time is last return-bin end.
30-second/1516-bin event storage cap: emit incomplete, wait for baseline return,
never manufacture completion or recognition. Gap and EOF also censor candidates.

## Whole-event recognition

Only completed objects reach recognizers. Preserve measured polarity, amplitude
in ADC counts, elapsed duration, SNR, asymmetry, opposite-polarity area and whole
curve. Area-integrate all bins to 64 phases; canonicalize principal sign, augment
training with time reversal without reading direction or geography. Compare:
(1) eight empirical template families with amplitude/duration distance;
(2) eight PCA coordinates, reconstruction error, amplitude, duration, SNR,
asymmetry and opposite-area ratio, with 50% diagonal covariance shrinkage;
(3) a moment-fitted Gaussian shape with separately retained amplitude/duration.
The Gaussian is a comparison, never an assumption about all genuine fields.
One-class distance thresholds are the 99th percentile of positive calibration
objects; no nonmagnet likelihood or arbitrary-window competition. Record all
scores including rejected weak completed objects. No geographic inputs.

Measure segmentation independently of recognition, one-to-one interval overlap,
split/merge proxies, boundary errors, completion latency, unmatched recognitions
per recorded hour (NOT a verified false-positive rate), known background false
recognitions where annotated ground exists, runtime and storage. All unavailable
physical-truth metrics must be explicitly unavailable. Synthetic cases establish
invariants and expose limitations, not field reliability. Keep negative results;
no post-test algorithm rescue or alternative architecture.
