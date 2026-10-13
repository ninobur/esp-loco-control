# Hall Opening Map and IR Odometry Investigation

**Source:** September 28, 2026 Otto continuous Hall/IR capture and its five MQTT logs  
**Scope:** research only; no firmware, specification, or magnet-map changes

## Finding

The September 28 evidence does **not yet justify replacing the physical spacing map with a calibrated Hall-opening map for navigation identification**.

It does show a real, potentially useful signal: after capture-time alignment and interpolation, a leave-one-passage-out opening map reduced held-out prediction RMSE from **9.88 mm to 6.78 mm** and MAE from **6.67 mm to 5.08 mm**. This is evidence that some directed MM intervals have repeatable opening-position offsets.

That gain disappears in the measurement NAVI actually recorded. Using the integer pulse distances from `nav/ir_compare`, the physical map produced **12.82 mm RMSE / 9.79 mm MAE**, while a leave-one-passage-out opening map produced **13.24 mm / 9.80 mm**. Calibration therefore made integer-pulse prediction slightly worse in this capture.

Identification also did not improve. The physical and calibrated maps each selected the genuine opening in all **356/356** held-out trials. Only eight trials contained an intervening qualifying false Hall observation, and all were easy: the smallest winning margin over the next candidate was **48.26 mm**. The dataset therefore demonstrates neither a calibration benefit nor a calibration penalty for difficult candidate selection.

The practical interpretation is that opening-specific offsets appear to exist, but September 28's approximately 100 ms IR snapshots, nearest-snapshot event association, and 9.652 mm pulse quantization obscure much of the gain at runtime.

## Data and method

The analysis reused the previously reconstructed 1 kHz Hall openings and did not repeat the waveform investigation. An interval was admitted only when:

- both endpoints were `ADVANCED`, excluding `ADVANCED_WITH_DISCREPANCY`;
- the transition was exactly one mapped MM step;
- the destination named the source MM as its accepted reference;
- the IR interval was `MM_REFERENCED_EPOCH`; and
- `nav/ir_compare` supplied a one-step integer pulse distance.

This yielded **386** high-confidence interval observations in **175** directed MM groups. There were **145** groups with at least two observations. Direction coverage was **377 CW observations in 166 groups** and only **9 CCW observations in 9 groups**. No CCW group repeated, so all 356 leave-one-passage-out trials are CW.

For every repeated directed interval, each passage was held out in turn and the mean of the other passages was used as its calibrated prediction. Candidate selection then followed the stated rule: process threshold-qualified openings in increasing distance, retain the candidate closest to the expectation, and finalize once a later candidate cannot be closer. `NON_LANDMARK_HALL` openings between accepted anchors were treated as false candidates. The capture-era physical map remained the reference map.

Two distance representations were kept separate:

1. **NAVI measurement:** the exact integer pulse delta reported by `nav/ir_compare` multiplied by 9.652 mm.
2. **Offline diagnostic:** cumulative IR distance linearly interpolated at the Hall opening using the transmitter's `capturedUs` aligned to the NAVI clock. This is smoother, but it assumes uniform movement between snapshots and was not available to the September 28 navigator.

## IR odometry

### Nominal pitch and scale

All **33,264** Type-5 snapshots over **3,377.126 s** carried `pitchUm = 9652`, and every `nominalUm` exactly equaled `completedPulses × 9652`. The nominal value is documented as a 96.52 mm measured rolling circumference divided by ten spokes, based on three ten-revolution measurements ([development audit](../../navi-ewo-0-3-stop-integration/docs/IR_NAVI_DEVELOPMENT_AUDIT_20260919.md#L27)).

This verifies configuration and arithmetic, not physical accuracy. Every September 28 packet carried calibration ID 0 and `distanceValidated = 0`. The capture contains no independent tape, encoder, or surveyed endpoint measurement.

One high-confidence MM000-to-MM000 span covered an inferred two circuits: **103.534 m nominal IR versus 104.300 m of route-map distance, −0.734%**. That is a route-account comparison, not independent ground truth. A separate September 20 repaired-sensor run measured two map-referenced circuits as 51.918 m and 51.937 m against 52.150 m, a two-pulse repeat difference but offsets of −0.445% and −0.408% ([September 20 analysis](../../navi-ewo-0-3-stop-integration/docs/IR_NOON_RUN_ANALYSIS_20260920.md#L76)). Those results support repeatability of that setup, while still not certifying absolute distance.

### Quantization, synchronization, and interpolation

| Contribution or observation | September 28 result | Meaning |
|---|---:|---|
| One pulse | 9.652 mm | Smallest integer distance increment |
| Ideal two-endpoint phase SD | 3.94 mm | Quantization alone under independent uniform endpoint phase |
| Type-5 cadence | median 100 ms; maximum 501 ms | No individual pulse timestamps were recorded |
| Absolute Hall-to-selected-IR alignment | median 100.47 ms; p95 143.60 ms; maximum 149.47 ms | Selected snapshots reached the firmware's 150 ms admission edge |
| NAVI integer distance minus capture-aligned interpolation | SD 9.93 mm; p05 −18.29; p95 +16.12; range −27.02 to +29.24 mm | Snapshot association plus pulse phase dominates ideal quantization |
| Arrival-time interpolation minus capture-time interpolation | SD 3.34 mm; range −9.90 to +10.80 mm | Packet arrival time is not a safe physical timestamp |

The transmitter snapshots accumulated pulses at about 100 ms cadence; they did not preserve individual edge times. Linear interpolation reduces visible stair steps, but speed changes inside a snapshot interval are unknowable. The within-interval correlation between speed and integer-pulse prediction error was **r = 0.407** after removing each interval's mean. It fell to **r = 0.195** with capture-time interpolation and to **r = 0.173** after opening-map calibration, but did not disappear.

### Integrity and detectability

The stream had no sequence reversal or duplication, pulse-counter regression, recorder ring drop, saturation increment, raw sample-gap increment, or inferred pulse addition/removal. It did have **435 Type-5 sequence gaps totaling 484 missing snapshot positions**. Because later packets carry cumulative counts, these gaps do not themselves lose traveled pulses, but they reduce timing resolution. They are observable from sequence and cadence.

NAVI can reliably detect malformed arithmetic, counter reversal, boot/epoch changes, stale input, and Hall/IR association beyond its 150 ms gate. It cannot establish from this recording that every physical spoke produced exactly one completed pulse. Nor can it detect rolling-circumference changes, wheel slip, or a silent doubled/missed optical count. An older, different seven-spoke configuration demonstrated that detector merging could undercount 1% to 39% while its published health indicators appeared normal ([August 26 record](../../navi-ewo-0-3-stop-integration/docs/IR_DEV_REC/2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md#L102)). That earlier failure does not quantify the repaired September 28 hardware, but it proves the failure class is not covered by endpoint counters alone.

Thus NAVI's **precision** can be measured from repeated counts; its **physical accuracy** was not demonstrated by this capture, and some important inaccurate measurements are not self-identifying.

## Opening-interval repeatability

Across the 145 repeated directed groups:

| Statistic across interval groups | NAVI integer pulses | Offline capture-time interpolation |
|---|---:|---:|
| Median within-group SD | 6.82 mm | 3.16 mm |
| p95 within-group SD | 20.47 mm | 10.37 mm |
| Maximum within-group SD | 22.29 mm | 15.23 mm |
| Median within-group range | 9.652 mm | 5.00 mm |
| p95 within-group range | 28.956 mm | 19.92 mm |
| Maximum within-group range | 48.260 mm | 28.29 mm |

Representative intervals show both systematic offsets and measurement artifacts:

| Directed interval | Passes | Physical map | NAVI mean / range | Offline mean / range | Interpretation |
|---|---:|---:|---:|---:|---|
| MM081→082 | 3 | 300 mm | 299.21 / 19.30 mm | 301.15 / **0.96 mm** | Most consistent ≥3-pass offline example; integer endpoints still span two pulses |
| MM059→060 | 3 | 300 mm | 318.52 / 19.30 mm | 312.48 / **3.53 mm** | Repeatable positive opening offset candidate |
| MM058→059 | 3 | 320 mm | 302.43 / 19.30 mm | 305.55 / **3.96 mm** | Repeatable negative opening offset candidate |
| MM023→024 | 2 | 300 mm | 284.73 / **28.96 mm** | 297.18 / **0.19 mm** | Large integer spread is almost entirely snapshot/phase behavior |
| MM115→116 | 3 | 300 mm | 295.99 / **28.96 mm** | 293.45 / **28.29 mm** | Least consistent ≥3-pass offline interval; variation remains after interpolation |

MM115→116 measured 29, 31, and 32 pulses: **279.908, 299.212, and 308.864 mm**. The corresponding offline estimates were **276.04, 299.98, and 304.33 mm**, at mean speeds of 149, 242, and 254 mm/s. No Type-5 sequence position or continuity/optical counter changed inside those three intervals. The low-speed passage is about 24–28 mm shorter than the faster passages. The evidence cannot distinguish a real threshold-position shift from wheel behavior or unreported pulse loss; it does show that interpolation and integer quantization are not the whole explanation.

## Held-out calibrated-map results

| Held-out metric, 356 CW passages | Physical spacing map | Opening map | Change |
|---|---:|---:|---:|
| NAVI integer-pulse RMSE | **12.82 mm** | 13.24 mm | 3.3% worse |
| NAVI integer-pulse MAE | **9.79 mm** | 9.80 mm | no practical change |
| Offline capture-aligned RMSE | 9.88 mm | **6.78 mm** | 31.4% better |
| Offline capture-aligned MAE | 6.67 mm | **5.08 mm** | 23.9% better |
| Genuine openings outside corresponding ±15% diagnostic | 0 | 0 | no difference |

The offline result supports the hypothesis that directed Hall openings carry repeatable geographic offsets. The NAVI result contradicts the stronger claim that those offsets were already useful with September 28's runtime odometry. Learning integer counts from other passages also learns snapshot phase and speed effects, which do not repeat reliably enough.

The CCW sample is inadequate for calibration: nine single observations and no repeated directed interval. Comparing those single offline CCW intervals with the reverse CW group means gives differences from −13.53 to +4.65 mm, median −3.46 mm. This is suggestive but confounds direction, passage, and measurement error. Only Otto was represented. Any experimental map should therefore remain keyed by **locomotive and direction** until cross-locomotive and repeated reverse-direction tests show that pooling is valid.

## Navigation-identification replay

| Result | Physical map | Integer opening map | Offline-derived opening map used as expectation |
|---|---:|---:|---:|
| Correct genuine selections | 356/356 | 356/356 | 356/356 |
| False selections | 0 | 0 | 0 |
| Ties / ambiguous winners | 0 / 0 | 0 / 0 | 0 / 0 |
| Finalized on a false candidate before the genuine opening | 0 | 0 | 0 |
| Mixed-candidate trials | 8 | 8 | 8 |
| Correct in mixed trials | 8/8 | 8/8 | 8/8 |

The most populated example was MM114→115: false candidates occurred at 96.520, 154.432, and 212.344 mm; the genuine opening occurred at 260.604 mm; expectations were 300.0 mm from the physical map, 289.6 mm from integer calibration, and 297.5 mm from offline calibration. All three expectations select the genuine event. The winner's margin over the next-closest candidate was 48.26 mm.

This is useful confirmation that closest-candidate selection rejects the recorded secondary observations, but it is not evidence that calibration improves rejection. There were too few mixed encounters, and none put a false opening close enough to the physical-map expectation to create a different outcome.

## What additional testing must establish

A practical calibration decision needs independent runs that were not part of the calibration capture, repeated CW and CCW laps, at least two locomotives, and deliberate speed strata. IR should record individual pulse completion timestamps or provide an event-synchronous count/interpolation method, together with an independent distance reference for a subset of intervals. Tests should include magnets known to produce candidates within roughly 50 mm of the genuine opening expectation; otherwise identification benefit remains unmeasurable.

The existing physical map should remain the comparison reference. A Hall-opening map is promising as an additional, direction- and locomotive-specific model once its benefit survives those tests, but September 28 establishes only the offline geographic signal, not a deployable improvement.

## Artifacts

- [Calibrated interval table](calibrated_opening_intervals.csv)
- [Held-out candidate trials](held_out_candidate_trials.csv)
- [MM115→116 diagnostic](mm115_116_diagnostic.csv)
- [Machine-readable summary](ir_calibration_summary.json)
- [Reproducible analysis](../analyze_ir_odometry_calibration.py)
- [Timing and quantization plot](plots/ir_timing_and_quantization.png)
- [Held-out prediction plot](plots/physical_vs_calibrated_prediction.png)
- [Candidate-selection example](plots/candidate_selection_example.png)
- [MM115→116 plot](plots/mm115_116_diagnostic.png)
