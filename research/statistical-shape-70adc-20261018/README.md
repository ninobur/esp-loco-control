# Statistical shape recognition with a 70-ADC principal-excursion requirement

This is the separately committed repeat of `804f1a2`, with exactly one changed acceptance condition. **The 70-count requirement removes every baseline-contained false detection and every reported split proxy, without losing any previously matched ensemble.** Boundary accuracy remains limited. See [the report](REPORT.md), [the frozen protocol](PROTOCOL.md), and [the paired waveform plot](figures/continuous_comparison.png).

The [original package](../statistical-shape-20261018/README.md) is unchanged. Saved models are loaded without refitting. Every original candidate is scored in full using the original code. The added condition only determines eligibility for the unchanged overlap suppression.

From this directory, using Python and the [original pinned requirements](../statistical-shape-20261018/requirements.txt):

```sh
python repeat.py
python compare.py
python checks.py
```

`repeat.py` verifies all original artifact hashes, reproduces the original continuous experiment, evaluates the 70-count condition, and writes only this directory's `results/`. `compare.py` produces paired tables and plots without altering recognition. `checks.py` verifies the inclusive threshold, both polarities, eligibility before suppression, complete-window preservation, original byte-identical replay and unchanged denominators.

`python verify_portable.py` runs these steps in a clean temporary copy of both packages and compares every regenerated result and figure byte for byte. No native archive, network service, new training or prior revised outputs are used.

| Artifact | Contents |
|---|---|
| [REPORT.md](REPORT.md) | Direct original-versus-70 comparison and limitations |
| [PROTOCOL.md](PROTOCOL.md) | Pre-evaluation definition of the single changed condition |
| [aggregate.csv](results/aggregate.csv) | Both conditions, full A and focused A/B |
| [continuous_events.csv](results/continuous_events.csv) | Every accepted interval and missed annotation, with excursion and baseline |
| `results/candidate_audit.csv.gz` | Every positive-shape proposal, exact bounds, unchanged score and both selection flags |
| [annotation_comparison.csv](results/annotation_comparison.csv) | Every MM annotation paired across conditions, including unresolved cases |
| [boundaries.csv](results/boundaries.csv) | Beginning/end/midpoint errors, all matches and common annotations |
| [envelope_excursions.csv](results/envelope_excursions.csv) | Diagnostic amplitude measurements; reference labels/models remain unchanged |
| `results/original_*_replay.csv` | Byte-identical copies reconstructed by the control run |
| [checks.json](results/checks.json) | Tests of the changed acceptance condition and preserved evidence |
| [portable_reproduction.json](results/portable_reproduction.json) | Clean-copy reproduction evidence |

Candidate audit bounds are half-open 20-ms bin indices, `[lo,hi)`, on the identified boot's original tracing. `baseline_adc` uses the original endpoint-level estimator; `excursion_adc` is the greatest absolute departure of an original 20-ms observation from that baseline. `passes_70` is inclusive. `selected_original` and `selected_70adc` also reflect unchanged shape-score ranking and overlap suppression; an eligible candidate need not be selected.

`B_union` is the original label-free, equal-weight geographic-model mixture within MM050–MM070. No B model is applied outside its coverage. Unresolved annotations remain in all denominators. `inside_BL` means a detection wholly inside an existing provisional baseline; it is different from baseline time covered by a window that also contains a magnet. `possible_lobe_split` retains the original annotation-overlap proxy and is not an independent physical lobe determination.

No firmware, locomotive configuration, Pi service, dashboard or operational behavior changes are included.
