# Replay: one waveform at several widths (240 / 600 / 1500 ms) — 2026-10-27

**Status:** replay only, at David's decision of 2026-10-27, about 17:25 PDT. No firmware, matcher or map was changed, and this is not an implementation plan.

**Question (Sam's framing, agreed by David, Sam and Muse):** can one magnetic waveform, stretched in time, recognise every genuine magnet passage in the October 27 recordings without introducing false observations?

**Answer, on replay [R]:**

- **Yes for the October 27 recordings.** Adding a 600 ms copy recognised all three known misses, and adding a 1500 ms copy changed nothing further. Across all eleven Oct 27 recordings, the wider copies added **no false or duplicate observation in any driven run**, including Muse's control file (still 90 / 90).
- **The wider copies did add six stationary observations in one parked Otto session** (`AFADA3EF…`, old build, PWM 0 throughout). The Hall signal there shows wide, clean excursions while Otto was not being driven. David has since confirmed that Otto was being handled while parked (see §8).
- **On the October 25–26 held-out corpus**, the wider copies recognise 4–5 more magnets and add 5 false events. All five added false events are in the two sessions already known for background steps and slow crawl (`278F53E8…`, `F420A05D…`).
- **Raising only the wide copies' thresholds** to |r| ≥ 0.9 brings the corpus false count back to the single-template 8, still recognises 6,002–6,003 magnets, and still recognises all three October 27 misses.

---

## 1. Method

**Bank.**

- **Template:** the existing template, unchanged (`mf.template_mean()`, the 945-curve mean), sampled at widths 240, 600 and 1500 ms.
- **Window and thresholds:** pad 0.5, level-only normalisation, |r| ≥ 0.7 and |A| ≥ 70 for every width.
- **What chooses the width:** the Hall signal alone. No speed, PWM, timing or geography input.

**Combining rule (stated before running).**

- **Candidates:** for each width, every local maximum of |r| that passes that width's thresholds is a candidate.
- **When a candidate is known:** at `ready = t + 0.5·D + L/2`, which is when its own window and its own suppression neighbourhood are complete.
- **Declaring:** candidates from all widths are taken in `ready` order. A candidate is declared unless:
  - an already-declared event lies within the larger of the two widths' suppression radii (±0.5·D), or
  - a stronger candidate (larger |r|, any width) that is already known lies within its radius.
- **One magnet, one observation:** declarations are final, with passage time taken from the declaring width's template peak.
- **Parity check:** with uniform thresholds this is exactly the research causal emulation `exp_continuous.causal_events`. The script asserts identical event times before it runs anything else.

**Truth.**

- **Oct 25–26:** the research reference (`corpus.score`), held-out partitions (Otto Oct 26 and Toby), 6,027 magnets, unchanged.
- **Oct 27 driven runs:**
  - Truth is the single-240 events, which are identical to the on-loco firmware events, plus every template-independent oracle excursion while moving that no event covers.
  - In each of the six driven runs those extra excursions are exactly the three known misses (asserted in the script).
  - Each run's total equals your start-to-end magnet count: 234, 179, 180, 186, 188 and 90.
- **Not scored for recall:** Otto restart b (`3D46D330…`; E-stop, direction change) and the four parked recordings. Their events are listed instead.
- **Stationary:** every event while applied PWM is 0.

**Reproduce:** `research/navi_waveform_design_20261027/bank_20261027/`.

- `replay_bank.py` produces `results.json`.
- `followup_bank.py` produces `followup.json` (late declarations, and fitted width against speed).
- The Oct 27 NSR captures are decoded with `../navi_matched_filter/extract_nsr.py`.

## 2. Baselines reproduced (Oct 25–26 held-out, causal)

| Configuration | Brief's figure | This replay |
|---|---|---|
| 240 only, pad 0.5 | 5,999 / 6,027, 8 false | **5,999 / 6,027, 8 false**, 1 duplicate |
| 240 + 600, pad 1.0 | 6,001, 11 false | **6,001, 11 false** |
| 240 + 600 + 1500, pad 1.0 | 6,001, 11 false | **6,001, 11 false** |
| Six research widths, pad 1.0 | 6,002, 13 false | **6,002, 13 false**, 1 duplicate |

## 3. Results at pad 0.5 (as fielded), uniform thresholds 0.7 / 70

### 3.1 October 27 recordings

| Run | True magnets | 240 | 240 + 600 | 240 + 600 + 1500 |
|---|---|---|---|---|
| Otto run 1, CCW 004-005 | 234 | 232 (MM169, MM113 missed) | **234** | **234** |
| Otto CCW 044-045 | 179 | 178 (MM037 missed) | **179** | **179** |
| Otto CW 040-041 | 180 | 180 | 180 | 180 |
| Toby CCW 035-036 | 186 | 186 | 186 | 186 |
| Toby CW 024-025 | 188 | 188 | 188 | 188 |
| **Control, Otto MM121–039 (Muse; includes PWM 150)** | 90 | 90 | **90** | **90** |
| False / duplicates / polarity errors, all driven runs | — | 0 / 0 / 0 | **0 / 0 / 0** | **0 / 0 / 0** |
| Stationary observations, Otto parked (old build) `AFADA3EF…`, 123 s at PWM 0 | — | 2 | 4 | 6 |
| Stationary observations, the other three parked recordings (96 s) | — | 0 | 0 | 0 |
| Otto restart b (not scored) | — | 35 events | 36 events | 36 events |

- **The extra event in restart b** is at 74.35 s, width 600, r 0.80, A 70, PWM 17. It coincides with a clean oracle excursion of 460 ms at PWM 18. It is probably a genuine slow passage after the E-stop, but there is no truth for that run, so it is not scored.
- **Which width declared:** the 240 ms copy declared every cruise magnet. The three recovered misses were all declared by the 600 ms copy.
  - The 1500 ms copy declared nothing on Oct 27. Where it matches MM037 and MM113 better (r −1.00 and 1.00), its decision comes later, and the 600 ms copy has already declared by then.
  - So **240 + 600 is sufficient for these three**. The 1500 ms copy is a back-up whose value shows only under the threshold changes in §4.

### 3.2 The three known misses, r and A at every width

| Magnet | Passage | 240 ms | 600 ms | 1500 ms | 240 + 600 | 240 + 600 + 1500 |
|---|---|---|---|---|---|---|
| MM037 (S) | 786 ms over 70 counts, PWM ≈ 28, braking | r −0.69, A −30 | r −0.80, A −142 | r −1.00, A −193 | recognised (600) | recognised (600) |
| MM113 (N) | 696 ms, PWM ≈ 34, braking | r 0.70, A 29 | r 0.81, A 125 | r 1.00, A 162 | recognised (600) | recognised (600) |
| MM169 (N) | 400 ms, slowed by plastic on the track | r 0.66, A 73 | r 0.95, A 166 | r 0.88, A 115 | recognised (600) | recognised (600) |

### 3.3 October 25–26 held-out corpus (6,027 magnets)

| Configuration | Recognised | Missed (cruise / slow / stationary) | False | False: cruise / slow / stationary | Duplicates | Stationary observations, whole corpus (4,665 s parked) |
|---|---|---|---|---|---|---|
| 240 | 5,999 | 26 / 1 / 1 | 8 | 8 / 0 / 0 | 1 | 3 |
| 240 + 600 | 6,003 | 22 / 1 / 1 | 13 | 11 / 1 / 1 | 1 | 5 |
| 240 + 600 + 1500 | 6,004 | 21 / 1 / 1 | 13 | 11 / 1 / 1 | 1 | 5 |

"Slow" means PWM below 60. "Cruise" means PWM 60 or above. Only 30 held-out magnets were passed slowly.

**False events, listed individually.** The 8 baseline false events (all 240 ms) appear in every configuration:

- `278F53E8…`: 87.06, 102.87 and 109.81 s;
- `F420A05D…`: 70.25, 75.22, 94.47, 95.67 and 97.15 s.

The wider copies add five:

| Session | t (s) | Width | r | A | PWM | Context |
|---|---|---|---|---|---|---|
| `278F53E8…` | 94.22 | 600 | 0.79 | 247 | 71 | Slow-crawl session with wide flat-topped excursions (manifest limitation) |
| `278F53E8…` | 104.19 | 600 | 0.89 | 421 | 71 | Opposite-polarity pair 0.43 s apart, \|A\| ≈ 420: a background step or burst, not a magnet shape |
| `278F53E8…` | 104.62 | 600 | −0.87 | −416 | 71 | Same pair |
| `F420A05D…` | 34.84 | 600 | 0.81 | 126 | 0 | Stationary. F420 is a known background-step session (manifest: 39–100 s) |
| `F420A05D…` | 41.66 | 600 | 0.72 | 134 | 22 | Slow, same session |

None of the added false events is in an otherwise clean session. They share the sessions, and the kind of disturbance, that already produce the baseline's false events.

### 3.4 One observation per magnet

| | 240 | 240 + 600 | 240 + 600 + 1500 |
|---|---|---|---|
| Extra per-width observations of the same magnet **before** combining (corpus) | 1 | 5,128 | 5,145 |
| Duplicates **after** combining (corpus) | 1 | 1 | 1 |
| Before / after combining, Oct 27 driven runs | 0 / 0 | 948 / 0 | 960 / 0 |

- Each wider copy, run alone, also fires on nearly every cruise magnet. The combining rule reduces that to one observation per magnet.
- The one remaining duplicate is the baseline's own 240 ms duplicate.

### 3.5 Declaration latency after passage (measured, fixed per width)

| Width | Latency | Events declared by this width |
|---|---|---|
| 240 ms | **359 ms** | all cruise magnets |
| 600 ms | **893 ms** | 4 corpus magnets; 3 Oct 27 misses (and the restart-b event) |
| 1500 ms | **2,233 ms** | 1 corpus magnet; none on Oct 27 |

My earlier estimate for the 1500 ms copy (0.8–1.3 s) was wrong. The measured latency is 2.23 s, because the window is 3 s long and the suppression radius is 0.75 s.

**Late declarations after the next magnet's.**

- **Oct 27:** none.
- **Corpus:** one, in each bank configuration. In `278F53E8…` at 118.86 s, a 600 ms event (r 0.70, A 413, PWM 71) was declared after a 240 ms event 0.46 s later. Both lie outside the reference spans, in the slow-crawl session.
- **When it can happen:** whenever two encounters are closer than the difference in latency (0.53 s for 600 ms, 1.87 s for 1500 ms). At cruise, magnets are 1.2–1.6 s apart, so a 1500 ms declaration at cruise would normally arrive after the next magnet's 240 ms declaration.
- **Why it matters later, not here:** a matcher would have to order observations by passage time, not by declaration time. The matcher was not changed here.

## 4. Threshold sensitivity (the 240 ms copy fixed at 0.7 / 70; only the wider copies raised)

| Wide-copy thresholds \|r\| / \|A\| | Corpus recognised, 240 + 600 | Corpus false | Corpus recognised, 240 + 600 + 1500 | Corpus false | Stationary events (held-out sessions) | All 3 Oct 27 misses, 3-width bank | Oct 27 false |
|---|---|---|---|---|---|---|---|
| 0.70 / 70 | 6,003 | 13 | 6,004 | 13 | 5 | yes | 0 |
| 0.75 / 70 | 6,003 | 12 | 6,005 | 12 | 5 | yes | 0 |
| 0.80 / 70 | 6,002 | 11 | 6,005 | 11 | 4 | yes | 0 |
| 0.80 / 130 | 6,001 | 10 | 6,004 | 10 | 3 | yes | 0 |
| 0.85 / 70 | 6,002 | 10 | 6,004 | 10 | 3 | yes | 0 |
| **0.90 / 70** | 6,002 | **8** | 6,003 | **8** | **3** | **yes** | 0 |
| 0.90 / 130 | 6,001 | 8 | 6,002 | 8 | 3 | yes | 0 |

All fifteen grid points are in `results.json`.

- **Higher thresholds suit the wider copies.** Raising the wide copies to |r| ≥ 0.9 removes every false event they add in the corpus. It keeps 3–4 of their extra corpus recoveries, and keeps all three Oct 27 recoveries.
- **With 0.9 / 70 the 1500 ms copy earns its place.** In the three-width bank all three misses survive every grid point: MM169 through the 600 ms copy (r 0.95), MM037 and MM113 through the 1500 ms copy (r 1.00). In a 240 + 600 bank at 0.9, MM037 (600 ms r 0.80) and MM113 (0.81) would be lost again.
- **The stationary events in `AFADA3EF…` were not part of the grid.** All four wide-copy events there have |r| between 0.71 and 0.87, so a wide-copy threshold of 0.9 would remove them. I infer this from their scores; I did not run it.

## 5. Fitted width, as an observation only (not used for recognition)

For each held-out genuine magnet, the best-fitting width is the one with the largest |r| among 240 / 600 / 1500 at that event. Speed is mapped spacing divided by the passage-to-passage interval [R]:

| Best width | Magnets | Speed, median (p5–p95), mm/s |
|---|---|---|
| 240 ms | 5,837 | 238 (171–293) |
| 600 ms | 100 | 135 (97–178) |
| 1500 ms | 1 | 88 |

The best-fitting width tracks traversal speed: the slower the passage, the wider the winning copy. It may be useful telemetry later. It is not part of this decision.

The six-width research bank's *declaring* width does not track speed (Spearman −0.12). That is because the narrowest width that passes declares first, so the declaring width is not a width estimate.

## 6. Secondary check: wide copies on a slowed-down signal

Each wide copy was run on a block-averaged series with the same stretched template:

- 600 ms: decimated ×3, 6 ms, 201 taps.
- 1500 ms: decimated ×6, 12 ms, 249 taps.

The decimated scores were compared with the full-rate scores at every recognised Oct 27 event and at the three misses (1,060 points per width):

| Width | Largest \|Δr\| | Largest \|ΔA\| | Accept/reject flips at 0.7 / 70 |
|---|---|---|---|
| 600 ms at ×3 | 0.0044 | 0.81 counts | 7, all on cruise magnets the 240 ms copy declares, with the 600 ms score sitting at threshold |
| 1500 ms at ×6 | 0.063 | 5.7 counts | 0 |

- **At the three misses** the changes were at most 0.0033 in r and 0.6 in A. No decision changed.
- **The 1500 ms differences are larger,** up to 0.06 in r. They occur on cruise magnets, where the wide copy's score is low anyway.
- **Not established:** whether the decimated form keeps the passage-time accuracy, which is limited to the decimated sample (6 or 12 ms). It would need the same firmware-against-replay parity discipline as today's build before it was relied on.

## 7. Against the brief's success criteria

| Criterion | Result |
|---|---|
| All three known misses recognised | **Met** with 240 + 600 and with 240 + 600 + 1500, at 0.7 / 70. Only the three-width bank still recognises all three when the wide copies' thresholds are raised to 0.9 |
| No material rise in false events in slow or stationary segments | **Oct 27 driven runs: met** (0 → 0). **Corpus:** +1 slow and +1 stationary false at 0.7 / 70, both in the known background-step session F420. They are gone at wide-copy |r| ≥ 0.9. **Oct 27 parked Otto (`AFADA3EF…`):** 2 → 4 → 6 stationary observations at 0.7 / 70. David confirmed Otto was being handled, so these are hand-passed magnets |
| No duplicates after combining | **Met.** Duplicates stay at the baseline 1 (corpus) and 0 (Oct 27) |
| Control file still clean at 90 / 90 | **Met** in every configuration and at every threshold tried |

## 8. For David

- **The parked Otto session `AFADA3EF…`** (15:59, old build, PWM 0 for its full 123 s) contains wide, clean Hall excursions (366–1,168 ms) and steps. **David confirmed on 2026-10-27 that Otto was being handled while parked.** Those observations are therefore magnets passed by hand and handling disturbances, not stationary false events on the running railway.
  - With that settled, the October 27 recordings show no stationary false observation from the wider copies.
  - The wider copies did respond to hand movement, as the 240 ms copy did twice.
- **Choices this replay informs, not decided here:**
  - two widths or three;
  - whether the wide copies get a higher r threshold;
  - whether the decimated form is used.
- **Not touched:**
  - the matcher, which is the hold rule; one miss still leaves it one magnet behind;
  - ordering observations by passage time, which a matcher would need with these latencies;
  - any firmware.
