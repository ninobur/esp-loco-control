# One reference waveform, adjustable time scale: does it recognize every passage without false detections? (2026-10-28)

**Status:** replay analysis. Diagnostic configurations only. Nothing here is a proposal or an approved recognizer; no firmware was changed.

**David's question:** can one reference magnetic waveform, with an adjustable time scale, reliably recognize all the genuine magnet passages in our normal operating recordings without introducing false detections?

## Method

- **The reference waveform** is the existing mean template.
- **Time scale** is set by a bank of copies of it: 150, 240, 600 and 1,500 ms.
- **Normalization:** level-only, pad 0.5. This is the fielded setting.
- **Combining:** the causal rule from the bank replay (`NAVI_WAVEFORM_BANK_REPLAY_20261027.md` §1).
- **Thresholds:** the 240 ms copy stays at |r| ≥ 0.7, |A| ≥ 70. The other copies were run at either 0.7 or 0.9.

**Scripts** in `research/navi_waveform_design_20261027/field_20261027_evening/`, each with its `_output.txt`:

- `bank_thresholds_corpus.py`
- `bank_thresholds_afternoon.py`
- `bank_thresholds_evening.py`

| Recordings | Passages | Truth |
|---|---|---|
| Oct 25–26 held-out corpus (Otto Oct 26, Toby) | 6,027 | research reference (`corpus.score`) |
| Oct 27 afternoon, 6 driven runs | 1,057 | single-240 events plus oracle excursions, matching the operator's counts |
| Oct 27 evening, Otto | 527 | map alignment plus oracle (`NAVI_WAVEFORM_0_1_FIELD_RUN_20261027_EVENING.md` §2); MM057 at its corrected time |

## Results

| Widths (other copies' \|r\|) | Corpus: recognized / false | Afternoon: recognized / false | Evening: recognized / false |
|---|---|---|---|
| 240 only (as fielded) | 5,999 / 8 | 1,054 / 0 | 514 / 0 |
| 240+600 (0.7) | 6,001 / 11 (bank replay) | **1,057 / 0** | 524 / 0 |
| 240+600+1500 (0.9) | 6,003 / 8 | **1,057 / 0** | 524 / 0 |
| 150+240+600+1500 (0.7) | 6,008 / **15** | **1,057 / 0** | **527** / 1 (the hardware transient) |
| 150+240+600+1500 (0.9) | 6,004 / 8 | **1,057 / 0** | 526 / **0** (MM039 missed, r 0.89) |

On parked boots, the afternoon gave 3–6 stationary observations across configurations. All of them are in the parked Otto session that David confirmed was being handled.

## Answer

**Not yet, on the evidence we have.** No configuration recognized every passage with zero false detections in all three sets.

### 1. Width is solved for speed

- **Slow passages (700–1,400 ms):** a 600 or 1,500 ms copy recognizes every slow passage in the Oct 27 recordings. That is all 3 in the afternoon and all 10 in the evening, with no false detection in any Oct 27 driven run.
- **Very fast passages (about 50 ms):** a 150 ms copy recognizes both.
- The waveform's shape holds across the whole speed range. Stretched to fit, slow passages correlate at r 0.95–1.00.

### 2. The narrow copy raises false detections

- At 0.7, the 150 ms copy adds 7 false detections in the corpus (8 → 15). It also fired on the evening run's hardware transient.
- At 0.9, the corpus is back to the baseline 8 and the transient is rejected, but the evening run loses MM039 (r 0.89).
- **At these thresholds, no setting gives both the fast passages and zero added false detections.** One more number in the score might: the transient's |A| was 705, against at most 285 for any genuine passage. That has not been tested.

### 3. Two residuals no width addresses

- **8 false detections in the corpus**, present in every configuration including the fielded one. All are in two Otto Oct 26 sessions, `278F53E8…` and `F420A05D…`, that were already known for Hall background steps (later found to be wiring). They are disturbances with a magnet-like shape at r 0.71–1.00. Time scaling does not separate them.
- **21–26 corpus misses**, mostly at cruise. They remain in every configuration, so they are not a width problem. This analysis does not determine their cause.

### 4. The recordings we can call normal operation, both Oct 27 sets

- **Afternoon:** every width combination that includes 600 recognized all 1,057 with none false.
- **Evening:** the four-width bank at 0.9 recognized 526 of 527 with none false.

The open points are therefore:

- **very fast passages near PWM 190**, which only this one run contains;
- **electrical disturbances**, which the corpus shows exist and which a shape test alone does not fully reject.

## Not established

- The 150 ms copy's false-detection behaviour beyond these recordings.
- Any amplitude ceiling or raw-range check (not tested).
- The corpus misses' cause.
- Behaviour on Toby at high speed: no Toby recording goes above PWM 90 for long.
