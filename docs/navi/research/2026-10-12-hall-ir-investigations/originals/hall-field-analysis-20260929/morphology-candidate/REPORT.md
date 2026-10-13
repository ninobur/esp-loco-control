# Hall Morphology and Candidate-Selection Investigation

**Source:** September 28, 2026 Otto continuous Hall/IR recording  
**Scope:** research, causal replay, and synthetic sensitivity only; no firmware, specification, or magnet-map changes

## Answer

The two ideas have value, but September 28 does not demonstrate an operational improvement over the historical geographic qualification.

- Hall waveform morphology contains substantial information about whether an emitted opening is a normal mapped-magnet passage or a secondary observation. A held-out rise-and-peak model achieved **AUC 0.938** and **86.3% balanced accuracy**. At its training-selected operating thresholds it nevertheless rejected **56/585 provisional genuine openings** and accepted **15/84 secondary openings**. It is unsuitable as a veto.
- The closest-candidate rule was correct in **385/386** causal replays when every emitted opening was treated as independent. Its failure was a same-field reopening 9.652 mm after the genuine opening. Requiring return to baseline fixed that case but suppressed two genuine subsequent magnets whose fields never returned to baseline, reducing the result to **384/386**.
- A post-hoc minimum candidate separation of two through five IR pulses produced **386/386** in this capture. One pulse retained the same-field failure; six or more pulses began suppressing genuine observations. This interval was selected from the evaluation data and is a hypothesis, not a validated rule.
- The historical first-opening simulation, with expected polarity and the existing ±15% geographic qualification, was already correct in **386/386**. Therefore the new procedure shows no measured identification gain on the actual capture and adds a median **4.25 mm** of travel beyond the expected position before finality; p95 was 25.81 mm.
- Morphology repaired the one raw closest-distance failure in a post-hoc near-tie reranking test using opening-rise evidence, but there was only one such actual case. The result does not establish a general combined decision rule.

The supported direction is a research prototype that preserves several candidates, uses geography as the main ranking evidence, and records morphology as a non-authoritative score. Baseline return, waveform rejection, and a calibrated opening map remain unvalidated as operational requirements.

## Evidence classes and labels

The report keeps three evidence classes separate:

1. **Physical observations:** five ADC conversions and their 1 kHz median; recorded opening reference; threshold crossings; transmitter snapshots; and nominal IR displacement.
2. **Inferred identities:** `ADVANCED` is used as a provisional genuine label and `NON_LANDMARK_HALL` as a provisional secondary label. `ADVANCED_WITH_DISCREPANCY` was excluded. These labels were produced by historical geographic and polarity judgment, not morphology, but they are not independent physical proof of magnet identity.
3. **Synthetic cases:** expected-polarity impostors at 270, 295, and 299 mm. They are simulations and did not occur at those positions in the recording.

The morphology evaluation contains **585 genuine-labelled** and **84 secondary-labelled** openings. Five-fold cross-validation held out the entire source magnet identity (`source MM mod 5`), so repeated traces from a magnet never appeared in both training and test partitions. Thresholds were chosen using training folds only. Class balancing was applied during fitting; the reported confusion counts use the original class frequencies.

Candidate replay used the same capture-time-aligned, linearly interpolated IR coordinate for both historical and proposed methods. This was necessary to keep measuring after the historical target opening without adopting its new anchor. It is a diagnostic coordinate, not a claim that September 28 NAVI possessed sub-pulse operational distance. The earlier [IR odometry report](../opening-map/REPORT.md) remains controlling for that limitation.

## Morphology findings

### Measured differences

The strongest population differences were in the shape and maturity of the rise, rather than simple amplitude:

| Feature | Provisional genuine median | Secondary median | Finding |
|---|---:|---:|---|
| Opening departure | 73 counts | 77.5 counts | The common ≥70 trigger makes opening amplitude weak evidence |
| Peak departure | 174 counts | 127.5 counts | Genuine peaks were usually stronger, but both classes had extreme outliers and broad overlap |
| Pre-opening slope | 7.21 counts/mm | 4.28 counts/mm | Genuine rises were usually steeper; secondary distribution had a long high-slope tail |
| Opening-to-peak distance | 13.56 mm | 0.83 mm | Many secondary openings were re-openings partway through an existing field |
| Pre-opening backtrack ratio | 0.111 | 0.369 | Genuine rises were smoother on average |
| Rise backtrack ratio | 0.150 | 0.365 | The distinction persisted through the observed peak |
| Peak-to-quiet-return distance | 19.69 mm | 17.03 mm | Strong overlap; not a useful stand-alone separator |
| Median five-conversion span during the pre-rise | 6 counts | 6 counts | Conversion-level noise did not distinguish the classes |

Polarity reversal and return behavior helped describe individual fields but did not form a universal signature. Only 156 genuine and 22 secondary profiles reached an opposite ≥70 threshold inside the bounded analysis window. For genuine passages that threshold often belonged to a much later field, so it cannot be interpreted automatically as the original magnet's morphology.

Peak amplitude was also insufficient. Some secondaries were stronger than ordinary genuine peaks, and a few labelled genuine or secondary events occurred during baseline/field conditions that produced departures above 2,000 counts. No Gaussian field assumption was used.

### Held-out discrimination

| Feature availability | AUC | Balanced accuracy | Genuine rejected | Secondary accepted |
|---|---:|---:|---:|---:|
| Speed only | 0.653 | 61.3% | 84/585 | 53/84 |
| Opening rise, available at opening | 0.902 | 86.0% | 39/585 | 18/84 |
| Opening rise, excluding speed | 0.903 | 86.0% | 38/585 | 18/84 |
| Rise plus associated peak | **0.938** | **86.3%** | 56/585 | **15/84** |
| Full return/polarity morphology | 0.930 | 85.4% | 53/585 | 17/84 |
| Full morphology, excluding speed | 0.928 | 85.1% | 56/585 | 17/84 |

The no-speed results are essentially unchanged, demonstrating that the classifier did not obtain its discrimination merely from different travel speeds. On the 632 observations at or above 80 mm/s, the rise-and-peak model retained AUC 0.927 and 84.6% balanced accuracy. The 37 slow/stationary observations were too few for a firm estimate; genuine recall there was only 56.3% at the common operating point.

More future waveform did not improve the result. Full morphology was worse than rise plus peak, partly because return and later polarity observations were frequently censored or contaminated by the next field. This is also a causality constraint: opening-rise evidence is available when the opening occurs, whereas peak, return, and polarity-change evidence arrive later and may not mature before a geographic decision boundary.

### Repeatability

There were 171 magnet/direction groups with repeated genuine-labelled passages. Under held-out full-morphology classification:

- 127 groups were accepted on every measured passage;
- 44 groups changed classification between passages; and
- no repeated group was rejected on every passage.

Median within-magnet SD was 7.07 counts for peak amplitude, 1.30 counts/mm for pre-opening slope, 2.87 mm for opening-to-peak distance, and 0.036 for rise backtrack ratio. The long upper tails were substantial: opening-to-peak within-magnet SD reached 136 mm in a censored/outlier case.

Morphology is therefore informative in aggregate but not a stable magnet fingerprint across this capture. The observed errors and mixed repeat classifications make a hard morphology rejection unjustified.

## Establishing independent candidates

The data contradicts a simple requirement that every new candidate follow a return to baseline.

At MM108→109, event 425 was the genuine opening at 299.012 mm from the preceding magnet. Event 426 was another same-polarity opening at 308.664 mm, only one pulse later. The Hall signal never returned to the ±25-count quiet band between them. Treating both as independent makes event 426 closer to the 305 mm expectation and causes the raw closest-candidate rule to select it incorrectly. Baseline-return consolidation correctly treats event 426 as the same encounter.

The same consolidation fails elsewhere. Events 254→255 and 256→257 also have no return to the source reference's quiet band, yet events 255 and 257 are the historically confirmed next mapped magnets. The fields overlap while the detector falls below and re-crosses the 70-count opening threshold. Treating baseline return as necessary merges away those genuine openings and produces false selections for MM125→126 and MM126→127.

Across all consecutive labelled observations, 53 genuine-labelled openings had less than 15 mm of measured quiet travel before them. This count includes complex multi-step/recovery areas and reference limitations, but it confirms that a long quiet-distance requirement cannot be imposed from this recording.

A post-hoc displacement sensitivity gave:

| Minimum displacement between candidates | Replay result |
|---:|---:|
| 0 or 1 pulse, 0–9.652 mm | 385 correct, 1 false |
| 2 pulses, 19.304 mm | 386 correct, 0 false |
| 3 pulses, 28.956 mm | 386 correct, 0 false |
| 4–5 pulses, 38.608–48.260 mm | 386 correct, 0 false |
| 6 pulses, 57.912 mm | 385 correct, 1 false |
| 7–10 pulses, 67.564–96.520 mm | 384 correct, 2 false |

This suggests a short spatial consolidation interval may be more practical than waiting for baseline. It is fitted and tested on the same capture and uses unvalidated IR distance, so the September 28 data cannot establish its operational value. Additional runs must intentionally test genuine close spacing, low-speed fields, same-magnet re-openings, and overlapping adjacent fields.

## Candidate-selection replay

### Historical comparator

The historical comparator accepts the first two-sample opening with expected polarity inside the capture-era map's ±15% interval. It selected the genuine target in **386/386** high-confidence adjacent transitions. This does not prove the historical policy generally safe; it establishes that the September 28 replay contains no demonstrated opportunity for the proposed model to improve its result.

### Proposed closest-candidate model

| Candidate interpretation | Correct | False | Actual mixed trials observed before finality |
|---|---:|---:|---:|
| Every emitted opening is independent | 385/386 | 1 | 10 |
| Require quiet baseline return and ≥1 pulse travel | 384/386 | 2 | 9 |
| Post-hoc three-pulse consolidation | 386/386 | 0 | 9 |

The raw failure is MM108→109, described above. The baseline-return failures are MM125→126 and MM126→127. The three-pulse result matches historical performance but is not held-out evidence and therefore does not demonstrate improvement.

For the raw rule, finality occurred a median **4.25 mm beyond the expected mapped position**, p95 25.81 mm, maximum 44.24 mm. Relative to the genuine opening, median additional travel was zero, p95 43.73 mm, and maximum 88.48 mm. An early genuine opening must wait until the symmetric boundary beyond the expectation, which explains the longer tail.

Selection remains distinct from confirmation. September 28 packets explicitly carried `distanceValidated = 0`; morphology has nonzero error; polarity can agree for both genuine and false observations; and physical reachability depends on the candidate-separation interpretation. A selected candidate from this experiment cannot establish a new authoritative anchor by selection alone.

## Interaction between geography and morphology

The actual raw replay had one encounter in which the best two geographic errors were within one 9.652 mm pulse. A post-hoc rule reranking only such near candidates by held-out opening-rise score changed the MM108→109 choice from event 426 to genuine event 425, producing **386/386**. Full-morphology score also repaired it; rise-plus-peak score did not. No other actual selection changed.

This is an example of incremental benefit, not validation:

- it is one case;
- the one-pulse reranking margin was chosen after inspecting the measurement resolution;
- both event 425 and event 426 fell below the classifier's accept threshold, so a morphology veto would reject the genuine opening as well; and
- full morphology may use samples not available at the causal decision time.

Morphology is best interpreted as a likelihood or confidence contribution when geographic candidates are effectively tied. It should not erase a candidate or override clear geographic evidence from this dataset.

## Synthetic expected-polarity impostors

The following cases are synthetic. The expected and genuine position is 300 mm.

| Impostor | Exact-distance model | September 28 pulse coordinate | Independence implication |
|---:|---|---|---|
| 270 mm | Closest-candidate waits, then selects genuine 300 mm | 270.256 versus 299.212 mm; genuine remains closer | 30 mm separation could pass a two/three-pulse candidate rule if the waveform closes sufficiently |
| 295 mm | Selects genuine 300 mm | Both round to 299.212 mm; distance tie | 5 mm is below one pulse and cannot satisfy the tested independent-candidate spacing |
| 299 mm | Selects genuine 300 mm | Both round to 299.212 mm; distance tie | 1 mm cannot be resolved as a separate candidate with this odometry |

The historical first-qualified comparator selects the impostor in all three idealized cases because each is inside ±15% and has expected polarity. Thus the closest-candidate concept provides a clear simulated improvement for 270 mm when both candidates can actually be observed independently.

At a pure distance tie, held-out morphology scores rank a randomly paired genuine above a secondary with probability equal to their AUC: 0.902 from opening-rise evidence and 0.930 from full morphology. This is an abstract pairwise result. It combines waveforms recorded at different places and times; it does not model two magnetic fields only 1–5 mm apart. The actual recording contains no physical 295/300 or 299/300 pair, so it cannot show that morphology resolves those cases.

## Recommendations

Continue research on a bounded candidate buffer and symmetric closest-distance boundary. It has a clear causal definition and solves the synthetic 270 mm case, but field tests must create close, expected-polarity impostors before claiming benefit.

Record candidate encounter state explicitly: threshold closure, minimum departure after closure, quiet-run duration and displacement, and whether the waveform is still part of the preceding field. Test separation values independently; the apparent 19–48 mm successful band is only an in-sample observation.

Retain morphology as diagnostic supporting evidence. Opening-rise features are attractive because they are causal, perform almost as well as longer profiles, and do not depend on speed for their observed discrimination. Preserve the score and its constituent measurements rather than reducing it to accept/reject.

Do not use morphology as a veto, require baseline return, or adopt a combined weighted rule from this analysis. Do not treat synthetic ties or successful replay as field reliability. A useful validation run needs repeated CW and CCW passages, controlled speeds, at least one other locomotive, independent distance truth, and deliberately placed or simulated field sources that create candidates within one IR pulse of the expected opening.

## Reproducible artifacts

- [Analysis script](../analyze_morphology_candidates.py)
- [Per-opening morphology features](morphology_features.csv)
- [Held-out predictions](held_out_morphology_predictions.csv)
- [Causal candidate replay](candidate_replay.csv)
- [Candidate-separation measurements](candidate_separation.csv)
- [Synthetic cases](synthetic_impostor_cases.csv)
- [Machine-readable summary](summary.json)
- [Feature distributions](plots/morphology_feature_distributions.png)
- [Held-out ROC curves](plots/morphology_heldout_roc.png)
- [Candidate-separation traces](plots/candidate_separation_traces.png)
- [Actual candidate replay example](plots/actual_candidate_replay.png)
- [Synthetic scenario diagram](plots/synthetic_impostor_scenarios.png)
- [Previously reconstructed representative Hall profiles](../plots/representative_field_profiles.png)
