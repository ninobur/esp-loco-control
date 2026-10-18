# Dataset eligibility and stationary uncertainty

No source-clock interval has independently confirmed physical stationarity. **Confirmed stationary: 0 intervals, 0 seconds.** The archived operating and optical records provide movement support and stop/uncertainty evidence, not an external physical-motion ground truth. Operator clarification was requested for the documented powered stall around boot B 400–650 s; no confirmation was available for this report. That material is excluded as uncertain, not relabeled confirmed stationary.

The complete source inventory contains 23,184,336 native Hall observations from two boots. Recognition uses the controls' identical 20-ms mean tables. Their combined source-clock grid is 4,658.16 s, including gaps and initial unbracketed recording time. All original observations remain preserved; eligibility creates an index mask, not a rewritten waveform.

| Partition | Eligible continuous time | Excluded clock time | Eligible passages | Contiguous excluded intervals |
|---|---:|---:|---:|---:|
| Training | 1,034.22 s | 1,227.56 s | 89 | 90 |
| Evaluation, identical for A/B | 1,195.70 s | 1,200.68 s | 89 | 90 |

These 180 excluded intervals are data-eligibility intervals, **not 180 physically confirmed stops**. Status changes can subdivide one interval; `excluded_contiguous_intervals.csv` gives the contiguous exclusion census, while `eligibility_intervals.csv` gives the finer evidence/status/partition census. Exclusions include 24.08 s of recording loss and 0.06 s straddling partition boundaries. The remaining 2,404.10 s have uncertain motion evidence.

| Evidence category | Training | Evaluation | Treatment |
|---|---:|---:|---|
| Actual completed-pulse advance in clean, nonheld tracking brackets | 933.60 s | 1,048.84 s | Eligible, direct movement support |
| Internal clean tracking brackets between actual advances, quantized count unchanged | 74.64 s | 110.06 s | Eligible, explicitly inferred interpulse continuity |
| Source bin spans both categories above | 25.98 s | 36.80 s | Eligible, mixed evidence label |
| Flat count, held/E-stop context, motor actual and command zero | 614.36 s | 302.40 s | Excluded, stop-supported but physically unconfirmed |
| No count advance without sufficient continuity support | 593.62 s | 834.66 s | Excluded, uncertain; includes powered stale-count episodes |
| Count advance while held/E-stop | 2.94 s | 12.10 s | Excluded, contradictory/handling/coasting uncertainty |
| No complete snapshot bracket | 0.10 s | 28.74 s | Excluded |
| Source bin spans incompatible bracket statuses | 3.96 s | 5.74 s | Excluded |
| Transport or optical-quality uncertainty | 2.62 s | 2.86 s | Excluded |

`eligibility.py` uses completed-pulse count advances and their quality context: optical tracking reason 6, no health fault, stable cumulative unreliability/inference/gap counters, the recorded 9,652-µm pulse pitch, continuous IR boot and sequence transport, and held/E-stop context. Pitch and count changes are used only to validate movement support, never to estimate navigation displacement or speed. Transport brackets must be positive and at most 250 ms with sequence advances of 1–3. Source bins must be completely covered. Neither zero PWM nor powered PWM independently establishes motion or stationarity.

The interpulse inference addresses optical count quantization. It is bounded by actual count advances within an uninterrupted clean/nonheld tracking run and never bridges stale, held, optical-quality or recording gaps. It cannot rule out a subpulse pause within such a run. Consequently this is an explicit movement-support policy at recording resolution, **not proof of continuous physical motion at every Hall sample**, and perfect removal of every brief stop is unproven. Direct and inferred support are reported separately rather than silently merged into confirmed movement.

No source samples on opposite sides of an exclusion are concatenated. A complete MM touching a passage edge is censored. The source-only morphology evaluation additionally requires its whole outer boundary uncertainty range to lie in one eligible passage. A point opening can remain eligible when its entire physical field is censored; those are different counts. Of the provisional opening hypotheses, 567 training and 618 evaluation openings are eligible at their individual times, covering all 171 locations; this does not prove complete-field eligibility.

The original training/evaluation clock boundaries are unchanged: boot B training before 1,840.213501 s and from 2,745.450837 through 3,167.014139 s; the complementary boot B ranges and all boot A are evaluation. These recordings have been examined in previous research; they are reserved partitions for this frozen fit, not newly collected blind data.

Movement snapshots, quality fields and bracket classifications are committed, with original paths and SHA-256 hashes in `sources/movement_manifest.json`. `inventory.py` and the portable `eligibility.py` replay reconstruct the complete index and exclusion report. The recognition module never imports either program and never sees these movement values.
