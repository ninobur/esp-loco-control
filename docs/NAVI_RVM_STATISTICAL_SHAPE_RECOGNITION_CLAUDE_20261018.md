# NAVI_RVM — Statistical magnetic shape recognition (Claude, independent)

2026-10-18 · Claude (Opus 5.5) · Research only

> **Status.** This is a research report. It makes no firmware, map, Pi-service,
> MQTT, dashboard or hardware change, and no device was flashed or operated.
> All sources were read only.
>
> **Scope of the evidence.** This is replay evidence from one evening
> (2026-10-15), one locomotive (Otto) and one Hall sensor. Per AGENTS.md §8 it
> shows what the software does on recorded passes. It does not establish
> railway behaviour.
>
> **Independence from Codex.** I did not read Codex's parallel work. A local
> folder `~/NGR/NGR-Files/navi-rvm-statistical-shape-20261018/` exists. While
> taking the inventory I saw its directory listing, its last three git log
> lines and the first lines of its README (the generic repository README). I
> did not open its code or results.

Package: [`analysis/navi_rvm_shape_20261018/`](../analysis/navi_rvm_shape_20261018/)
(code, annotation, results, figures and the dataset inventory in its README).

---

## 0. Answer

**Can a general statistical shape model recognize magnetic ensembles and
baselines as reliably as geographic-specific models, without relying on
movement estimates or independent measurement gates?**

**On the October 15 recordings, yes.** Model A, the general model, performed as
well as Model B, the MM050–MM070 model, on every measure. Neither model made a
recognition error in that region. **I recommend Model A**, provided §7 is
understood first: this is one evening of good data and does not yet amount to
field reliability.

Results in held-out passes (leave-one-pass-out). Both models used the same
features, class-density method, decision and grouping:

| | Model A, all 171 magnets | Model A, MM050–070 | Model B, MM050–070 |
|---|---|---|---|
| Annotated passages with an extent | 1,081 | 141 | 141 |
| Recognized exactly once | **1,081** | **141** | **141** |
| Missed (passage judged not-ensemble) | 0 | 0 | 0 |
| Lobes counted separately (split) | 0 | 0 | 0 |
| Recognitions on a frozen baseline (false positives) | **0** | 0 | 0 |
| Recognitions on activity the annotation has no identity for | 5 | 0 | 0 |
| Principal polarity = map polarity | 1,092 / 1,092 | 143 / 143 | 142 / 142 |
| Midpoint error, median / p95 / max | 10 / 40 / 100 ms | 10 / 50 / 100 ms | 10 / 40 / 90 ms |
| Smallest LLR among recognitions | 43.9 | 47.0 | 50.7 |

- **Baselines and ensembles do not overlap.** On held-out passes, all 28,697
  baseline windows scored LLR < 0. The highest was −126. All 1,081 whole-ensemble
  windows scored LLR > 0. The lowest was +30.
- **This is a shape result, not an amplitude test under another name.** I
  removed the amplitude feature entirely and kept only the normalised shape.
  The table did not change: 1,081/1,081, 0 missed, 0 split, 0 false positives,
  and the highest baseline window was −87. The flat line and the big bump
  differ in form, not just in size.
- **Model B adds no information on these data.** The 21 regional portraits are
  indistinguishable from the general mean once shape is normalised
  (`figures/region_portraits.png`). The variation within one magnet's repeats
  is as large as the variation between magnets.
- **One general model, two families.** Model A found its own families from the
  ensemble shapes, without seeing magnet type. BIC chose two Gaussian
  families. They separate bar from disk magnets: 414 of 459 bar passages fall
  in family 0, and 614 of 622 disk passages in family 1. The bar family has a
  slightly broader shoulder and a small opposite-sign tail. Each family is still
  a single principal excursion on a flat line. The model does not need to know
  the magnet type; the mixture accommodates both.

**What did not work, or is not shown** (§6, §7):

- **The level reference matters after a long stop.** With a trailing-only
  (causal) level, Model A missed the first **9 magnets after Otto left a 35 s
  stand** at a displaced Hall level (MM130–MM138, 674–683 s). The trailing
  median needed about 10 s to return to the running baseline. The primary,
  retrospective (centred) level did not have this problem.
- **Magnets inside recording gaps cannot be seen.** 21 NAVI openings have no
  ADC-only extent. 19 of them touch gaps in the 20 ms tracing, where the
  waveform is missing rather than ambiguous. Model A recognized 11 of the 29
  passages that are gap-touched or have no extent.
- **Irregular slow movement is not recognized as a magnet.** This includes
  creeping, stop–start movement and handling (184–188 s, 201–203 s), and a
  stationary NAVI opening with no waveform at all (MM129, 393 s). Whether a
  magnet was physically traversed there is unknown.
- **Field reliability is not established.** Not shown: another day, Toby,
  weak or failing magnets (the weakest passage here was 109 ADC), or an ESP32
  run.

---

## 1. Data inventory and provenance

The full inventory is in the package README. In summary:

| Source | Use | Provenance |
|---|---|---|
| `hall-ground-20261015/derived/native_15549e9385faa4d5.bin` | **The only recognizer input.** Native ADC, about 5 kHz, Otto, boot 1, 2026-10-15 evening, 3,843 s | Byte-identical slice of the Pi native NSR source (see that package's PROTOCOL.txt); sha256 `a3cc9c65…` |
| `hall-ground-20261015/derived/encounters_adc_only.csv`, `ground_intervals.csv` | Annotation **extents** and frozen **baselines** | ADC-only Hall-ground protocol, frozen before openings or geography were read; it is threshold-based |
| `navi-rvm-replay-20261016/derived/selections_scoring_only.csv` | Annotation **identity** (MM number, direction, polarity) | NAVI's recorded openings. These are **not** independent geographic truth |
| `…/map_positions.csv` | Map polarity per MM | Built from the mode of NAVI's opening polarity, so it is **not** independent of NAVI |
| `navi-ewo-0-3-ir-validity/field-records/20260828_magnet_types.json` | Bar/disk label, used **only** to describe the families after the fact | 2026-08-28 survey |

- **The 20 ms tracing** consists of non-overlapping 20 ms means of every
  native read. There is no smoothing. A block that touches a serial jump is a
  gap (1,000 of 192,124 blocks).
- **Boot 1 contents.** CW from 128 to 2,680 s (about 4.3 laps). CCW from 2,727
  to 3,773 s (about 2.1 laps). There were NAVI redeclarations at 263, 975 and
  1,403 s, and an obstruction episode from about 3,564 s.
- **Coverage.**
  - All 171 magnets have 5–9 annotated passages: 739 CW and 350 CCW.
  - MM050–MM070 have 5–7 passages each, in 7 region passes. Six are complete
    (four CW, two CCW) and one is partial (MM064–070 at 127 s).
- **Not used: boot 2 (`4d8083de…`, 87 openings).** It was run once as a holdout
  on 2026-10-16 and once on a broken build on 2026-10-18. David reserved it
  as a holdout, so I left it untouched. It can serve as a one-shot test of the
  frozen recognizer if he says so.
- **Not used: other dates.** The October 13 records (`otto-hall-repeatability-20261013`)
  contain openings and counts, not a continuous native Hall tracing. I found no
  other continuous native Hall recording.

**Annotation.** This was frozen before any model was fitted. It is
`scripts/s02_annotate.py`.

- Each NAVI opening is attached to the ADC-only supported encounter that
  contains it, or to the nearest one within 300 ms: 1,087 openings.
- Two unlabeled encounters lying between openings k−1 and k+1 are labeled k
  (SEQUENCE).
- 21 openings have no encounter, because the frozen ADC-only rule censors an
  encounter at a serial gap. They are kept as **NO_EXTENT**: identity known,
  extent unknown. They are scored with ±300 ms tolerance and excluded from
  boundary statistics. **This category was added after the first scoring run**,
  when the plots showed these were real magnets in gaps. It changes scoring
  only, not the recognizer.
- 8 supported encounters remain without identity (**UNLABELED**). 198
  `uncertain_instability` encounters are kept as **UNCERTAIN**.

**Prior exposure.** Boot 1 has been examined extensively in earlier NAVI work,
by me and by Codex. This includes the 10-16 replay, Hall-ground and the RVM
manual matcher. Before setting the recognizer I looked at plots of about 30
magnets. The evaluation is held out by **pass**, not by **evening**.

## 2. The recognizer (one method for both models)

Code: `scripts/rvm_shape.py`. All settings were chosen once, before the first
scoring run, and were not changed afterwards.

1. **Windows.** A window is centred on every 20 ms sample. Window widths are
   19 sizes from 0.16 s to 8.9 s in steps of ×1.25. This is a search range
   and nothing is rejected for its width. Breadth is handled by trying every
   width, not by estimating speed.
2. **Shape.** The window is resampled **linearly** to 32 points, and the
   context level is subtracted.
   - The context level is the median of the tracing over ±10 s.
   - The principal excursion is set to +1 (polarity normalised).
   - Windows are reversed in time on the declared CCW direction, so both
     directions are read in map orientation.
   - Linear resampling cannot warp. The order and proportions of hill, lobe and
     shoulder are preserved. A shoulder cannot be stretched into a principal
     lobe, so the rubber-map defect cannot occur.
3. **Features.** The features are 12 fixed DCT coefficients of the shape, plus
   log(amplitude / local noise). The second feature is a relation to the line's
   own roughness ("big bump on a flat line"). It is a modelled distribution,
   not a threshold, and the shape-only ablation removes it.
4. **Classes.**
   - **E** (ensemble) is a window holding one whole ensemble with half its
     duration of context on each side. The context covers the lobes, shoulders
     and transitions.
   - **N** (not an ensemble) covers baseline windows, and windows that cut
     through an ensemble, hold only a lobe, or straddle MM–BL–MM.
   - Each class is a Gaussian mixture fitted by EM. The number of components is
     chosen by BIC.
5. **Decision.** LLR = log p(f|E) − log p(f|N), with equal priors. A window is
   an ensemble if LLR > 0.
6. **Grouping (best explanation).** The highest-LLR window is accepted first.
   A window is rejected if its core overlaps an accepted core. The core is the
   middle half of the window, which is the ensemble estimate. Because the E
   class is a *whole* ensemble with its lobes, the window covering the whole
   ensemble outscores lobe-only windows. Lobes are absorbed, not counted.
7. **Outputs per recognition.** Each recognition reports its time span, its
   midpoint and its principal polarity. The map supplies the identity. This
   report does not build that association, and there is no navigator.

The recognizer has no PWM, IR, speed, distance, arrival time, duration gate,
amplitude gate or threshold crossing. It is retrospective: a recognition is
available about a quarter of a window after the ensemble ends. The centred
level adds 10 s of hindsight (see §6).

**Model A** is trained on every ensemble in the training passes (all 171
magnets). Its N class is drawn from the whole railway.

**Model B** is trained on MM050–MM070 only. Its E class is a mixture of 21
per-magnet Gaussians, one per magnet, built from that magnet's repeats. Its N
class is drawn from the MM050–070 region only. B is evaluated only inside the
region passes.

The comparison isolates the reference model. Both models use the same windows,
features, regularisation, decision and grouping. The one structural difference
is inherent in "geographic-specific": B's E components are defined by magnet
identity, while A's are learned (BIC).

**Folds.** Boot 1 is cut into 8 passes, in the middle of the baseline after each
MM145 and at the CCW declaration. Every pass is scored by models built without
it.

## 3. Results

### 3.1 Recognition

See the table in §0. Further results from `results/summary.json`:

- **Amplitude.** Annotated passages range from 109 to 276 ADC above or below
  the level. Every amplitude quartile scored 100 %.
- **Breadth.** Passages last 150–1,670 ms, an 11× range of traversal speed.
  Every duration quartile scored 100 %, including slow passages and
  decelerations (`figures/longest_ensembles.png`).
- **Direction.** CW scored 731/731 and CCW 350/350.
- **Magnet type.** Bar magnets scored 459/459 and disk magnets 622/622.
- **Boundaries.** The recognized core starts a median of +20 ms after the
  annotated start (p05 −10, p95 +80). It ends a median of −20 ms before the
  annotated end (p05 −80, p95 +10). The core is slightly narrower than the
  ADC-only extent.
- **Midpoint.** The error is |10| ms median, 40 ms p95 and 100 ms max. These
  are recording times, not distances.
- **All 5 Model A recognitions without an annotated identity** lie on
  UNLABELED activity, and each is a magnet waveform
  (`figures/unmatched_detections_A.png`). They include a magnet before the
  first declaration (125.6 s), one at 2,680.8 s, and three during the 3,564 s
  obstruction episode, when NAVI made no openings. None lies on a frozen
  baseline.
- **Earlier unmatched recognitions resolved by the annotation change.** The
  first scoring run also matched MM147 (1,447.7 s), MM076 (2,849.2 s) and
  MM057 (2,909.6 s). Those were the gap-censored openings above, so they now
  count as NO_EXTENT recognitions.

### 3.2 Statistical separation (held-out windows, `figures/separation.png`)

| Window kind | n | LLR (Model A) | Fraction > 0 |
|---|---|---|---|
| Baseline | 28,697 | max −126, median −1,399 | 0 |
| Partial or straddling (lobe only, half a magnet, MM–BL–MM) | 16,518 | max +8.7*, p99 −164 | 1 window* |
| Whole ensemble | 1,081 | min +30, median +52 | 100 % |

In the region, Model B's margins were similar to Model A's. B's baseline max
was −1,034 against −482 for A, and B's ensemble min was +36 against +40 for A.
B pushes the baseline further away but does not move the ensemble boundary.

- **\*The one positive "partial" window is a labelling artefact.** It is a
  0.96 s window over the whole MM147 ensemble at 1,447.7 s. MM147 has no
  annotated extent (NO_EXTENT), so the sampler could not tell that this window
  held a whole ensemble. **No window that truly cuts through or straddles an
  ensemble scored positive.** Near-aligned windows over a real ensemble do
  score positive. Grouping keeps the single best-scoring one, which is why
  there were no splits.
- **The LLR magnitudes are over-confident.** Tight Gaussian covariances
  produce numbers like −1,399. The meaningful result is that the distributions
  do not overlap, not the size of the numbers.

### 3.3 Families (`figures/families.png`, `results/families.json`)

Model A was fitted with K = 1…10 components on all 1,081 ensemble vectors, and
BIC chose **K = 2**.

- **Family 0** holds 414 bar and 8 disk passages.
- **Family 1** holds 45 bar and 614 disk passages.
- **Consistency.** On average 97 % of a magnet's repeats fall in the same family.

**Observation.** In the 20 ms tracing the "associated lobes" are small, about
3–5 % of the principal excursion for disk magnets and a little more for bar
magnets. On Otto, on this evening, the ensemble is overwhelmingly one big hill
or valley. What makes it recognizable is that one smooth, symmetric excursion
rises from a flat line and returns to it.

### 3.4 Model B versus Model A

On every measure the two models tie (§0, §3.2).

- **The extra information is not useful here.** It would be each magnet's
  individual portrait. After normalisation, those portraits differ from the
  general mean by less than a magnet's own repeat-to-repeat variation
  (`figures/region_portraits.png`).
- **The real difference between magnets is bar versus disk.** Model A captures
  it on its own through its mixture.
- **B's risk is also visible.** Each magnet has 5–7 repeats, so B's per-magnet
  Gaussians are tight. B worked here because the repeats are consistent. It
  would reject a passage that differs from its few recordings, which is the
  "portrait" requirement the assignment warns against. A's tolerance comes
  from pooling across all magnets.

## 4. Representative plots

All figures are in `analysis/navi_rvm_shape_20261018/figures/`:

| File | Shows |
|---|---|
| `overview_cw_ccw.png` | 14 s of held-out CW and CCW tracing, with annotations and Model A recognitions |
| `families.png` | The two families with their means |
| `region_portraits.png` | Every MM050–070 observation against the general mean |
| `separation.png` | LLR histograms, primary and shape-only |
| `longest_ensembles.png` | Slow passages that were recognized |
| `unmatched_detections_A.png` | The 5 recognitions without an annotated identity |
| `no_extent_openings.png` | Gap-touched openings |
| `unlabeled_not_recognized.png` | Irregular activity that was not recognized |
| `causal_level_misses.png` | The 9 misses after the 35 s stand |

## 5. Compliance with the prohibitions

| # | Prohibition | Status |
|---|---|---|
| 1–2 | Prior Claude tracker; Codex alignment kernel | Not used, read or modified. New code with linear resampling, no alignment |
| 3–4 | PWM, IR, speed, distance, predicted arrival | None in the recognizer. IR was not used at all. Direction (a declaration) only orients CCW windows |
| 5–6 | Amplitude, width or duration gates; threshold crossings | None. Width is searched, not gated. Amplitude is a modelled feature, and removing it changes nothing |
| 7 | Baselines by duration or mapped distance | Baseline is a learned class density |
| 8 | Individual portraits required | Model A requires none. Model B is the comparison that does, as specified |
| 9–11 | Recovery, compensation, state machines, motor, stops, AUTO, CTO | None |
| 12 | Firmware, Pi, dashboard, hardware | Untouched |
| 13 | Per-recording retuning | One setting for all folds, fixed before scoring |
| 14 | Dormant or speculative code | None. `RVM_VARIANT` selects the two disclosed ablations |

**One honesty point.** The **training labels** (E-window placement) and the
**evaluation extents** come from the threshold-based ADC-only Hall-ground
encounters. The recognizer never sees them, but it learned "a window centred on
what Hall-ground called activity". The boundary statistics are therefore
relative to that definition, not to a physical field edge.

## 6. Findings that need David's judgement (not acted on)

1. **Level reference: retrospective or causal.** The centred ±10 s level is
   what made the post-stand passage work.
   - A causal version failed for about 10 s after leaving a displaced-level
     stand. That is physical evidence on NGR, at 640–673 s.
   - I have not designed an alternative. Any change of level reference is a
     design decision.
   - The assignment says recognition may be retrospective, but 10 s of
     hindsight is a real cost for a navigator.
2. **Native capture gaps fall on magnets.**
   - 19 of the 21 openings without an extent have gap blocks within ±0.5 s.
     In the plots, the gap usually covers the magnet itself.
   - MM147 at 1,447.7 s has no gap block, yet the ADC-only rule still produced
     no encounter. The cause was not determined. Model A recognized it.
   - MM129 is the stationary case in item 3.
   - The cause of the gaps is unknown (capture or transport). If the same loss
   occurs in an on-board 20 ms tracing, the waveform is simply absent.
3. **No-waveform openings and irregular movement.**
   - NAVI opened MM129 at 393 s while the Hall level was flat and displaced at
     about 1,970, with no excursion.
   - The activity at 184–188 s and 201–203 s is irregular and slow.
   - Shape recognition does not call any of these magnets.
   - Whether a magnet was traversed is a field question. A bounded test would
     answer it: stop on a known magnet, creep off it, and record.

## 7. Limitations and unresolved questions

- **One evening, one loco, one sensor.** All laps are within about 1 hour. Not
  tested: Hall-zero drift across days, Toby, temperature, rain, a different
  mount.
- **Weak magnets are untested.** The weakest passage was 109 ADC.
  - Scaling the amplitude feature to 0.5× still recognized 100 % of ensembles,
    0.25× recognized 91 %, and 0.1× recognized 0 %. This scales only the
    amplitude feature; real weak passages also carry relatively more noise in
    their shape.
  - A failing magnet such as MM128 (2026-08-28), or a weak one such as MM152,
    would need recorded passages.
  - The shape-only ablation does not depend on amplitude, but its behaviour on
    a weak, noisy bump is unmeasured.
- **Annotation identity comes from NAVI.** The map polarity is derived from
  NAVI. "Polarity correct 100 %" means agreement with NAVI's own recorded
  polarity convention, not with an independent survey.
- **The evaluation is within one evening.** Folds are held out by pass.
  Parameters were fixed before scoring, but the data had been viewed. Boot 2
  is unused and remains available as a one-shot holdout.
- **Not done.** The geographic association of recognitions to MM numbers
  through the MM–BL–MM sequence was not built. The assignment asks for the
  recognition mechanism first. Also not done: ESP32 cost or latency.
  - Order of magnitude: 19 scales × 50 windows/s, each a 32-point resample and
    about 14 Gaussian evaluations in 13 dimensions, which is roughly 1–2 Mflop/s.
  - This is unmeasured.
- **Model B had 5–7 repeats per magnet.** With more repeats its portraits
  would tighten further. That strengthens the point about rigidity but could
  change the tie.

## 8. Suggested next evidence (for David to decide)

1. Run the frozen recognizer once on boot 2. This needs David's word.
2. Run it on any other continuous native Hall capture: another day, or Toby.
3. A bounded track test of three cases:
   - departing from a stop on a known magnet;
   - a creep or stop–start over a magnet;
   - a known weak magnet.

   Record the native stream gap-free.
4. Only then consider the MM–BL–MM association layer.

No recovery mechanism is proposed.
