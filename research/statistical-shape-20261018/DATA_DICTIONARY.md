# Dataset and result dictionary

Boot clocks are separate. Never concatenate them as a continuous recording. All native time fields are microseconds; all `lo`/`hi` bounds use half-open intervals `[lo,hi)` in the indicated boot's 20-ms table.

## Portable observations

`data/hall20_BOOT.csv.gz`:

| Field | Meaning |
|---|---|
| `us` | Nominal center of the bin on the original boot clock |
| `adc` | Arithmetic mean of original ADC samples in the bin; absent means no observed samples |
| `count` | Number of native observations contributing to the mean |
| `first_row`, `end_row` | Half-open native-array row range; -1 for empty bins |
| `first_serial`, `last_serial` | Native sample serial endpoints; -1 for empty bins |
| `valid` | Observed bin without a serial discontinuity or >5-ms native acquisition discontinuity touching it |

The grid starts at boot time zero to keep `row * 20000 + 10000 == us`. Initial empty bins remain masked. Empty bins are not ADC zero. Recognition never accepts a window touching an invalid bin. Temporary zero substitution inside prefix-sum arithmetic is always masked before any decision; amplitude diagnostics on unresolved examples are omitted.

`data/native_frame_index.csv.gz` maps each native frame's `row_start` and `n` to `offset` in the preserved NSR slice, plus its first/last sample times. For an original-frame offset, add `3331022388 - 24` to a preserved-slice offset at or beyond 24. A frame can contribute to two adjacent 20-ms bins.

## Annotations

`annotation_seeds.csv` contains original NAVI selected-label metadata, not new physical ground truth. `prior_status` is retained for disclosure; it does not accept/reject examples. `annotations_tail_0.001.csv` is primary. The other two files vary the *dataset boundary convention*, not the detector parameters.

Each row has:

- `id`, `boot`, `kind` (`MM` or `BL`), `mm`, `direction`, `split`;
- original `opening_us` and `opening_serial`, and the previous research's `prior_status`;
- `lo`, `hi`, source `start_us`, `end_us`, native row and serial endpoints;
- `reason`: empty means usable under the annotation convention; otherwise the example is unresolved, including loss, missing/consecutive geography, passage edge or partition boundary;
- `annotation`: energy-envelope or intervening-complement convention;
- `annotation_polarity`: full-envelope principal sign relative to its median, used only as a descriptive comparison. It is not an independently measured pole.

The `mm` on a BL row identifies its preceding ensemble in the direction of travel. BL rows therefore belong to the same conditional geographic model as that MM. Some valid BL envelopes exceed the continuous scanner's longest computational scale; they are represented by multiple scan windows. No elapsed baseline-duration rule determines their class.

## Models and decisions

`models.json` stores the actual 64-component means and diagonal variances, including the fixed ridge and reference IDs. `reference_intervals` counts original examples; reversal augmentation doubles feature rows, not independent sample size.

`interval_decisions.csv` contains all primary evaluation examples. `score` is `(log p(MM) - log p(BL)) / 64`; `>0` means MM, `<0` BL, nonfinite/tie unresolved. `B_oracle` selects the geographic reference using the supplied annotation. `B_union` uses an equal-weight mixture of all 21 conditional references, separately within each class. Model A uses one reference per class through the exact same scoring function. `rms_adc`, `width_ms` and `profile_polarity` are post-extraction descriptions, not acceptance gates.

## Continuous evaluation

`scan_spans.json` is the exact evaluation exposure. `full_*` spans cover all evaluation clock intervals. `focus_*` spans cover the same annotation-selected MM050–MM070 passages for A and B. Actual waveforms alone enter `scan()`; the annotation span IDs do not locate individual candidate windows.

The scanner chooses positive-margin windows in descending score and suppresses any overlap with an already selected window. No inter-magnet timer, expected sequence, amplitude or width acceptance rule follows. This is an offline localization experiment, not a navigation system.

`continuous_events.csv` statuses:

| Status | Interpretation |
|---|---|
| `matched` | One-to-one match with a usable provisional MM envelope, IoU >=0.3 |
| `missed` | Usable annotation envelope without a match |
| `inside_BL` | Unmatched detection entirely within a usable provisional BL envelope |
| `possible_lobe_split` | Unmatched detection overlapping an already matched MM envelope |
| `unassigned` | Other unmatched detection; includes unannotated exposure and ambiguous boundaries |

`possible_lobe_split` is a grouping-error proxy. It is not physical proof that two lobes came from one magnet. `unassigned` does not prove a false positive. Acquisition-censored annotation envelopes are counted in `MM_annotation_unresolved` in the continuous summary, rather than scored as known missed physical events.

Boundary errors are detection minus reference boundary, in milliseconds. Report absolute errors conditional on matches, alongside the full unresolved/missed denominators. `BL_bins_as_MM` counts baseline-bin occupancy by selected windows; `inside_BL` counts detections fully contained in BL. They measure different things.

In `variation.json`, stratum -1 means unresolved/unmeasurable; strata 0–3 use fixed training-MM quartiles. The baseline sensitivity tables do not retune models. The bootstrap in `analysis.json` resamples paired decisions by MM, keeping repeated observations together. It cannot remove annotation bias or shared recording conditions.
