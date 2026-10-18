# NAVI_RVM: whole-function MM–BL–MM experiment

This is the separate corrected-assignment research package, based on `8c40d6d`, on branch `research/navi-rvm-whole-ensemble-20261018`. The previous [original shape study](../statistical-shape-20261018/README.md) and [70-ADC control](../statistical-shape-70adc-20261018/REPORT.md) are preserved unchanged.

**Outcome:** A meets the frozen complete-field test in 1/28 reviewed eligible fields, B in 2/28; neither validates a complete MM–BL–MM unit. Both retain every reviewed principal and secondary-lobe landmark together. Physical boundary ownership and general reliability remain unproven.

Start with the [results and limitations](REPORT.md). A whole-function representation is a testable hypothesis, not proof that its latent MM states contain complete physical fields. This package separates source-morphology completeness from provisional geographic associations.

| Review material | Location |
|---|---|
| User assignment | [Preserved prompt](sources/ASSIGNMENT.txt) |
| Frozen model contract | [Protocol](PROTOCOL.md) |
| Actual mathematics, functions and approximations | [Architecture and reconciliation](ARCHITECTURE.md) |
| Stationary exclusions and motion uncertainty | [Dataset report](STATIONARY_REPORT.md) |
| Whole-function recognizer | [functional.py](functional.py) |
| Reference construction and identical ADC replay | [experiment.py](experiment.py) |
| Frozen references and input hashes | [Model A](models/A.json), [Model B](models/B.json), [freeze](models/FREEZE.json) |
| Actual construction IDs for all 171 labels | [Reference audit](results/all171_reference_audit.csv) |
| MM050–MM070 reference counts and IDs | [Geographic references](results/geographic_references.csv) |
| Source-only morphology review | [Criteria](sources/MORPHOLOGY_REVIEW.md), [30 structures and boundary ranges](sources/morphology_adjudication.csv) |
| Complete field, baseline and unit outcomes | [Metrics](results/metrics.json), [field A](results/morphology_fields_A.csv), [field B](results/morphology_fields_B.csv), [baseline A](results/baseline_cores_A.csv), [baseline B](results/baseline_cores_B.csv) |
| All continuous state partitions | [A](results/evaluation_A.csv), [B](results/evaluation_B.csv) |
| Shared overlapping MM–BL–MM object IDs | [A](results/units_A.csv), [B](results/units_B.csv) |
| Every provisional evaluation opening, including unresolved | [Accounting](results/annotation_accounting.csv) |
| Source inventory and excluded intervals | [Inventory](results/source_inventory.csv), [contiguous exclusions](results/excluded_contiguous_intervals.csv), [detailed evidence ranges](results/eligibility_intervals.csv) |
| Reproduction and checks | [Instructions](REPRODUCE.md), [integrity](results/integrity.json), [actual-recording replay](results/replay_validation.json) |

Recognition consumes only ADC observations. There is no 70-count gate, separate lobe rejection, greedy window suppression, movement/arrival qualification, recovery mechanism or operational code change. The experiment is retrospective and does not implement a live navigator.

Model and source-review freezes precede evaluation. No result-dependent threshold, model substitution, or recovery was introduced. See `PACKAGE_MANIFEST.json` for every new artifact's size and SHA-256.
