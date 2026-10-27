# NAVI magnetic waveform recognition — minimal architecture

**Status:** engineering design for operator review. No firmware, map, or
governing specification has been changed. Nothing here is a decision; it is
a proposal, and the next decision belongs to the operator.

**Revision 2 (2026-10-27), revised per the operator's instruction
to take waveform recognition as far as it will go without IR, and
per Claude's competing design.** Four changes: (1) the recognizer
is Claude's single 240 ms scale, pad 0.5 — the 3-scale bank is
withdrawn (§2.4); the causal figures and the fixed 359 ms latency
are adopted from Claude's analysis (§5); (2) **IR is removed
entirely.** The Oct 21 ruling excludes IR from the current stage,
and no reliable IR device exists — the ±15% window, the IR history
buffer, the IR-supported position estimate, and the IR-dependent
provisional advance are all withdrawn (§4, §5.3, §5.4). The design
uses no IR anywhere, and re-admitting it is not an open decision
for this design; (3) the matcher gains a **bounded sequence
re-alignment** (§4.3.2): on recurring polarity disagreements it
matches the rolling observed-polarity window against the map within
±10 MM and re-anchors only when the evidence is unambiguous —
measured on replay, 29 re-anchors, all correct, none wrong; (4) the
acceptance tests validate the single-scale implementation against
its own single-scale reference, and add the sequence-check replay
(§8). Revision 1's provisional advance (§4.3 case B) is withdrawn:
it depended on the IR distance evidence, which is excluded, and it
conflicted with the Oct 21 ruling.

**Amendment (2026-10-27, per Sam's assessment of Revision 2).**
Sam agrees with the three major decisions (no IR; Claude's
single-scale recognizer; recognition separate from geographic
interpretation) and recommends Decision 2 be approved **for
implementation and manual-only experimental evaluation, but not yet
for authoritative automatic navigation** — approving the experiment
is not approving automatic geographic correction for operational
use. Two changes follow from his assessment: (1) the MM066–069
live test is rewritten, because the three magnets have already been
returned to their mapped polarities — the repaired railroad is not
to be disturbed to test fault detection before normal navigation
has been proven (§8.2.9); (2) the experiment must preserve **both**
geographic interpretations in telemetry — simple target matching
and bounded sequence re-alignment — so live running can establish
whether the latter improves reliability under normal conditions
(§4.3.2, §8.2.7). A third point is recorded as a limitation: a
rearview mirror of ten observed polarities is not necessarily ten
consecutive physical magnets (§4.3.1, §7.12). Decision 2 itself
remains the operator's to make.

**Basis:** `research/navi-matched-filter-20261026` @ `a52eb6a`
(`research/navi_matched_filter/REPORT.md` and the replay harness);
the 945-curve characterization on `research/navi-wolf-pack-20261021` @
`843a975`; Muse's matched-filter evaluation (2026-10-26); the current
firmware on the same research branch, `firmware/programs/NAVI_ONE/
variants/NAVI_ONE_X20` (Hall acquisition, ExcursionDetector,
MagnetRecognizer, Navigator, RouteMap.h), and the manual test sketch
`NAVI_WAVEFORM_0_1` (the 70×2
detector and the timing-gate implementation). **Revision 1
additionally rests on** `docs/decisions/0090` and `0092` (the
operator's NAVI/IR decision model, 2026-09-22) and
`firmware/programs/NAVI_COHERENCE/variants/
NAVI_COHERENCE_0_5_AUTO_ENABLED/Navigator.h` (the
`AdvancedWithDiscrepancy` ruling and the ±15% cumulative window) —
both now superseded on the IR question by the Oct 21 ruling below.
**Revision 2 additionally rests on** Claude's competing design
(`docs/navi/NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md`, branch
`docs/navi-waveform-recognizer-design-20261027` @ `7ec6430`, and its
`research/navi_waveform_design_20261027/design_analysis.py`, which
produced the single-scale causal figures and the matcher-variant
replay); the operator's ruling `docs/
NAVI_NON_IR_ACQUISITION_PWM90_20261021.md` (commit `348514e`,
2026-10-21: IR excluded entirely from the current stage, the ±15%
tolerance vestigial, a polarity-contradicting departure disposed of
with TARGET unchanged); and Muse's bounded sequence re-alignment
prototype (`~/workspace/navi-log-analysis/
sequence_realign_prototype2.py`, replayed over the research event
ledger, `results/event_ledger.csv`).

**What this document is:** the simplest practical architecture that
recognizes magnetic encounters from continuous Hall measurements and
matches them to the known geographic map. It is not a general-purpose
anomaly detector, a global geographic search, or a recovery system.

**Evidence labels used throughout.** Every claim is labelled:
- **Measured** — a number from recorded field data or the replay harness.
- **Simulated** — a number from synthetic traces built from recorded
  waveforms (algorithm behaviour only, not field reliability).
- **Estimated** — a number computed from the design, not measured on
  hardware (notably all ESP32 CPU/memory figures).
- **Proposed** — a design choice, justified by the evidence cited.

---

## 1. Architecture overview — responsibilities and boundaries

The railroad is a deterministic physical environment: a known track,
magnets at known locations with known polarities, a locomotive that in
automatic operation moves forward through the established sequence of
mile markers. NAVI's task is narrow:

> Recognize the arrival of the next magnet and identify its position
> in the map.

The architecture is a pipeline of four distinguishable operations.
Each answers one question, and no operation answers another's:

```
Continuous Hall observations
  → Magnetic waveform recognition   "What magnetic event was observed?"
  → Geographic map matching         "Does it correspond to the next expected magnet?"
  → Confirmed position
```

**Boundaries (the governing rule of this design).**

1. **The recognizer does not know the map.** It does not know where
   the locomotive is, which marker is expected, or which way it is
   travelling. It reports magnetic events with their evidence.
   Identity — *which* magnet — belongs to the matcher.
2. **The recognizer does not discard an event for disagreeing with
   geography.** A credible magnetic encounter is preserved as an
   observation even if its polarity contradicts the expected magnet.
   This is the MM066/067/069 lesson (§1.1).
3. **The matcher does not reinterpret observations.** It does not
   turn a contradictory observation into an agreement to preserve
   its expected sequence. It may hold the confirmed position and
   re-anchor by the sequence check (§4.3), or decline to advance on
   the observation, but it may not rewrite it: a polarity
   disagreement is recorded as a discrepancy, not as a
   confirmation.
4. **The map is a model of the railroad, not an authority over what
   the sensor measures.** When the two disagree, the disagreement is
   the finding, and it must be visible.

This separation already exists in the current firmware and is kept:
`MagnetRecognizer` is position-free ("Is this waveform a magnet
passage, and is it a NEW one?"), `Navigator` owns `navMm` and answers
"Is it the one the map says comes next?", and the two communicate only
through a queue of judged events. The design replaces the *detection
mechanism* inside the recognizer; it does not rearrange the
responsibilities.

### 1.1 The MM066/067/069 case, as the governing example

- **Measured (Oct 25):** MM066, MM067, MM069 matched `RouteMap.h`
  (66N, 67S, 69N), confirmed in the 2026-10-25 12:04 log.
- **Measured (Oct 26):** both Otto and Toby repeatedly observed clean
  magnetic waveforms with the opposite polarity (66S, 67N, 69S),
  34 + 33 recognized passes, correlation 0.98–1.00. MM068 unchanged.
- **Cause (operator report):** magnets were removed and replaced
  while levelling ballast to address Toby slipping; polarity was not
  rechecked after replacement. Three went back flipped, one correct.

The recognizer observed the magnets correctly while the geographic
interpretation met a contradiction. The architecture must therefore be
capable of representing, as a first-class state:

> "I encountered the expected magnet location, but its observed
> polarity disagrees with the map."

That requires keeping observation and interpretation separate. It does
not require a diagnostic system.

**The fundamental question — answered, and the mechanism revised.**
If NAVI recognizes a magnetic waveform at exactly the expected
geographic location but the polarity is wrong, has NAVI lost its
location, or has it discovered that the map and the railroad
disagree? The answer is the latter — and the principle is the
established decision model in `docs/decisions/0092` (the operator's
decisions, adopted verbatim 2026-09-22, Sam's consolidation):

- **Geographic confidence and map agreement are separate
  quantities.** The identification of a magnet rests on the trusted
  prior position and the waveform. The polarity is one piece of
  evidence, and it is about the *map*, not the *location*. A
  wrong-polarity magnet is evidence, and its polarity is retained
  (0092 point 12).
- **Decisional inertia.** One inaccurate observation does not create
  ambiguity and does not overturn NAVI's world view. NAVI remains
  where it has good reason to believe it is, unless a preponderance
  of evidence supports another explanation (0092 point 12).
- **Repeated contradictions cause investigation, not panic.** If
  subsequent observations also disagree, NAVI begins looking for
  alternative explanations using route history and the map — but
  only among physically possible positions, bounded to ±10 MM around
  the believed position, with possible positions considered before
  pattern matching (0092 points 13–14).
- **The history is retained.** Missed magnets are UNKNOWN; actual
  observed polarities remain evidence even when they disagree with
  expectation; a correction does not erase the observations (0092
  points 15, 18).

**What changed in revision 2 is the mechanism, not the principle.**
Revision 1 implemented the principle as a provisional advance
supported by the IR distance (the "distance matches" in the
paragraph above). That mechanism is withdrawn: the Oct 21 ruling
(`NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`) excludes IR entirely
from the current stage and declares the ±15% tolerance vestigial,
and no reliable IR device exists — the operator's words are that we
have not been able to make one. Without a distance measurement there
is no "the distance matches" to support a provisional advance, and
the ruling's own answer to a polarity-contradicting departure is to
dispose of it, log it, and leave TARGET unchanged. The revised
mechanism is in §4.3: hold on the contradiction (the ruling's
default), and recover by the **bounded sequence re-alignment**
(§4.3.2) — the investigation 0092 points 13–14 describe, using only
the observed polarities and the map, which needs no distance
measurement at all. The fix for MM066/067/069 is still physical
(flip the magnets back), and the system's job is still to make that
visible — which is also the operator's stated measure of success:
the firmware should work well enough that when it does not work
reliably, it means the world has changed.

### 1.2 What is deliberately not in this architecture

- No global geographic search. In automatic operation NAVI evaluates
  the next expected magnet only, and any investigation of
  alternatives is bounded to physically possible positions within
  ±10 MM of the believed position (0092 point 14) — never a
  whole-route search. The existing ten-magnet polarity sequence word
  remains as evidence, compared against the incumbent and proximal
  alternatives; a single disagreement does not withdraw the position
  (§4.3).
- **No IR anywhere.** The design uses no infrared distance, no IR
  validity check, no IR history buffer, and no IR-supported position
  estimate (§4, §5.3, §5.4). IR is excluded by the Oct 21 ruling and
  by the absence of a reliable device; the design is complete
  without it. Re-admitting IR is a separate hardware question, not
  an open decision for this design.
- No automatic failure recovery in the sense of a search-and-correct
  framework — **with one bounded exception, proposed explicitly in
  §4.3.2.** The sequence re-alignment re-anchors the position from
  the observed polarity history when the evidence is unambiguous.
  It changes the "no automatic recovery" stance of the manual
  sketch's governing decisions, and it needs the operator's explicit
  approval (§9). If the operator does not approve it, the matcher
  is Claude's hold (dispose, log, TARGET unchanged) and nothing else
  in the design changes.
- No timing gate for separating encounters (§4, §5). Separations on
  the route are ≥ ~9× the waveform width, and the recognizer resolves
  them without a gate (**measured**).
- No adaptive speed-based scaling, no matching pursuit, no joint
  two-template fitting, no local-slope removal (§2, §8). Each was
  tested and did not earn its complexity.

---

## 2. Signal-processing pipeline — from Hall samples to recognized events

The pipeline is the research harness (`mf.py`, frozen configuration)
transcribed into firmware operations, with one simplification: a
**single 240 ms scale** instead of the 6-scale bank (revision 2,
§2.4 — Claude's analysis shows the bank buys nothing measurable),
and the template stored as a 128-point shape from which the single
template is built once at boot.

### 2.1 Input sampling and filtering

**Sampling (measured, from the firmware and the captures).** The Hall
sensor is read by `hallTask` at 1 kHz (`vTaskDelayUntil(&wake,1)`,
core 0, priority 3). The native stream arrives in bursts of 5 samples
~46 µs apart, then ~800 µs gaps — the "5 kHz" rate is an average, and
a 1 ms bin can contain a single sample. Flat-baseline noise is
1.5–3.0 counts (robust std). Single-sample spikes of 69–97 counts
occur about once per 35 s of recording (**measured**: four observed
across the CSV and NSR datasets).

**Step 1 — despike: median of the 5 reads in each 1 ms tick.**
The median is stateless and per tick, so the 5-read burst is exactly
the median window. A single-sample spike cannot survive it
(**measured**: the stress test shows spikes of 150–400 counts at 2/s
and 20/s change nothing). This is the firmware-realistic form of the
despike: the research's sliding median-of-5 over the native stream
and this per-tick median give **identical results on replay**
(**measured**, Claude's `design_analysis.py` §2: 5,999/6,027, 8
false, 1 duplicate either way). A binned median must not be used
instead: a spike inside a sparsely populated 1 ms bin passes a
binned median at full value (that is how the +97-count spike entered
the 1 kHz series in the earlier characterization). Note the recorded
burst sizes vary (1–7+ samples) because NSR transport losses split
bursts; the firmware itself always takes exactly 5 reads per tick,
so the variation is a recording artefact, not a firmware behaviour.

**Step 2 — bin: average pairs of tick medians to a 500 Hz series.**
Every second tick, `y = (m_prev + m)/2`, timestamped at the
midpoint. Binning averages the samples, further suppresses noise,
and cuts the downstream compute. Empty bins are gaps, not zeros.
Gaps up to 20 ms are bridged linearly; a gap longer than 20 ms
splits the trace, and no recognition window may straddle it — windows
touching such a gap produce no event (they are invalid, not
interpolated). **Measured:** Toby's misses include 9 magnets whose
window touched a UDP capture dropout; on the locomotive those samples
would exist, so this is a recording artifact, not a recognizer
failure — but the rule must be the same on the locomotive for any
genuine acquisition gap.

**Effect on shape and latency.** The tick median adds at most 1 ms
of delay; the pair average adds 1 ms. The magnet waveform is at
least 40× wider than either, so the shape is unaffected
(**measured**: identical recognition). The narrowest magnet (FWHM
~78 ms at PWM 90) spans ~39 bins at 500 Hz.

### 2.2 Waveform normalization — level-only, no baseline return

**Method (measured).** Inside every window, the observed samples are
regressed on [1, T]: a local level and the template. Two quantities
come out: the score r (the Pearson correlation after removing the
local level) and the amplitude A (the least-squares template
coefficient, in ADC counts). Both are computed only from that window.
Nothing is carried from the previous magnet: no frozen level, no
return-to-baseline test, no expected polarity.

**What it handles (measured, stress test on held-out Otto data,
2,008 magnets, frozen 6-scale configuration — the single-scale
stress regression is §8.1.5):**

| Disturbance | Result |
|---|---|
| Constant offset ±100 counts | Exactly invariant (2,000/2,008, unchanged) |
| Slow drift, sine ±100 counts, 60 s / 10 s period | 2,000 / 1,998 recognized |
| Gaussian noise σ 5 / 10 / 20 | 2,000 recognized at all levels |
| Single-sample spikes 150–400 counts, 2/s and 20/s | 2,000 recognized |
| Fast drift, ramp 20 counts/s | 1,965 recognized (was 2,000) |
| Fast drift, ramp 100 counts/s | 1,815 recognized, 53 false |
| Random steps ±100 / ±200 counts every ~5 s | 1,981 / 1,911 recognized, 39 / 48 false |
| Sine drift ±100 counts, 3 s period (210 counts/s) | 1,968 recognized, 476 false |

**What it does not handle.** Fast drift (≥20 counts/s) and background
steps ≥100 counts degrade recognition, and a drift whose curvature
resembles a bump *is* a bump to a shape detector. This is a real
limitation, not a tuning problem (§7).

**Why level-only, not level + slope.** Removing a local slope as well
("line") should help with drift. In practice it creates two
opposite-polarity sidelobe events about ±175 ms either side of every
magnet (r 0.63–0.86, |A| ≈ 80), because the slope term absorbs half
a bump at the window edge. On development data that is about 2,400
sidelobe events at r ≥ 0.6. The level-only window does not produce
them. **Do not remove a local slope.**

**No baseline-return requirement.** The recognizer does not depend on
the signal returning to baseline between magnets. **Measured:** the
signal failed to return within ±20 counts between magnets in 2–5% of
intervals (dev 4.8%, validation 2.1%, Toby 5.1%), and the recognizer
still found the following magnet in 99.3% of cases when 20–50 counts
off, and 92% when more than 50 counts off. This is the property that
addresses the 2026-10-25 merged-signal failure mode — but see §7:
the actual merged-signal recording has not been replayed, so this is
supporting evidence, not a proof.

### 2.3 Template matching

**Template (measured).** The mean of the 945 oriented,
amplitude-normalized curves in `aligned_curves.csv` (Oct 15 Otto),
peak-normalized. Leave-one-out median correlation 0.991; 100% of the
945 curves score above 0.7; the template transfers across boots
(hold-out median error 0.0040) and across locomotives (Toby, 99.70%
with the Otto template unchanged). The template is a near-Gaussian
bump (σ ≈ 40 ms at PWM-90 speed) with a systematic asymmetry (rise
~10–19% longer than the fall); the measured template carries the
asymmetry, a symmetric Gaussian does not, so the measured template is
used.

**Correlation.** The window is the template of duration 0.24 s
padded with 0.5 × D of flat ground on each side (window length 241
samples, 0.48 s). Inside the window, r is the Pearson correlation
between the observed samples and the template after removing the
local level, and A is the least-squares coefficient of the template
in the same window (the peak excursion in counts that best explains
the window). The level is fitted in the same window, so A needs no
frozen baseline. Polarity is the sign of A (+ = ADC rises = North on
both locomotives).

**From correlation peaks to events.** Events are local maxima of |r|
over time that pass both thresholds, with non-maximum suppression
(NMS) removing weaker maxima within ±120 ms (0.5 × D) of a stronger
one. The reported time is the template-peak position (§5). NMS is
what consolidates multiple correlation peaks belonging to one physical
magnet: a single magnet produces one maximum (the template sliding
over it peaks once); neighbouring maxima within half a scale are
suppressed. **Measured:** duplicates are 1 in 6,027 held-out
magnets with the proposed configuration (causal and retrospective
alike).

**Thresholds (research-derived defaults, not constants).** r ≥ 0.7 and
|A| ≥ 70 counts. The sweep on development data (1,719 magnets) shows
any amplitude floor in 50–110 behaves the same (at 70: 1,715
recognized, 7 false; at 110: 1,714 recognized, 5 false; at 130, 31
real Otto magnets are lost). The floor of 70 sits inside the usable
range; it is not special. Recognized magnet amplitudes are Otto 1st
percentile 117, median 169, minimum 76; Toby 1st percentile 142,
median 212, minimum 119. The floor matters in parked noise (41
events without it, 7 with it) and does not matter while moving (the
remaining false events have |A| 102–390, the same range as magnets).

**Relationship to the current firmware's amplitude test.** The current
`MagnetRecognizer` uses a *relative* floor (0.34 × the trailing median
accepted peak), deliberately, because a fixed floor of 140 would
refuse MM012 thirty times a lap (decision 0052) and amplitude is
speed-independent. The absolute floor of 70 is consistent with that
evidence: the minimum observed magnet amplitude is 76 counts, and the
relative floor's typical value (≈0.34 × 200 ≈ 68) is numerically the
same as 70. The two agree in practice at normal amplitudes. They
differ if the global amplitude level shifts (e.g. a sensor-height
change scales all amplitudes): a fixed floor would then refuse
everything or admit noise, while a relative floor survives. That
trade-off is an open decision (§9), not a second gate — the design
recommends the absolute floor as the primary gate, because it is
directly measurable in the window and needs no history.

**No geographic timing gate.** Do not use a timing gate as a
substitute for waveform recognition. The current production
`Navigator::judge()` already contains no timing check (polarity and
the ten-magnet sequence word only); the timing gate exists only in
the manual test sketch (`NaviTarget`/`NaviTimingPredictor`). The
design keeps it that way. Timing is used only as diagnostic
consistency evidence in the matcher (§4), never as a condition for
recognizing a waveform.

### 2.4 Waveform scaling — one fixed scale, not a bank

**Options compared (measured, causal emulation, held-out Otto Oct 26
+ Toby, 6,027 reference magnets; Claude's `design_analysis.py` §1,
reusing the research harness unchanged):**

| Configuration | Causal recognized | Causal false / dup | Decision latency |
|---|---|---|---|
| **Single 240 ms, pad 0.5 (proposed)** | **5,999 (99.54%)** | **8 / 1** | **359 ms, fixed** |
| Single 240 ms, pad 1.0 | 5,996 | 7 / 0 | 479 ms |
| 2-scale bank (0.24, 0.6), pad 1.0 | 6,001 | 11 / 0 | 479 ms |
| 3-scale bank (0.24, 0.6, 1.5), pad 1.0 | 6,001 | 11 / 0 | 479 ms |
| 6-scale bank (frozen research config) | 6,002 | 13 / 1 | 298 ms (narrowest scale decides first) |

Misses by PWM band show no band where a bank helps: at PWM 60–84
the single scale misses 8/237 against the 6-scale bank's 6/237; at
PWM 85–95, 18/5,598 against 17/5,598; below PWM 60 both miss 2/30.
The single scale gives up 3 magnets against the 6-scale bank and 2
against the 3-scale bank, and has *fewer* false events than either.

**Recommendation (proposed, revised): a single fixed 240 ms scale,
pad 0.5.** This reverses revision 1's 3-scale bank recommendation.
The bank costs 5–24× the memory and CPU (§6) for +2–3 magnets in
6,027 and more false events. The single scale is also the simplest
defensible choice: one template, one window, a fixed 359 ms decision
delay (§5) instead of a scale-dependent latency. The 3-scale bank is
withdrawn, not kept as a fallback — the evidence does not support
keeping it.

**Why not interval-derived scaling.** It fails badly as implemented:
one missed or extra event, a stop, or a restart corrupts the
"previous interval", the scale goes wrong, and further misses follow.
A scale that depends on the recognizer's own output is fragile in
exactly the conditions (startup, stops, misses) that matter. This
corrects an earlier recommendation (Muse, 2026-10-26) to take the
scale from the last measured interval; that recommendation is
withdrawn. A fixed scale needs no speed estimate and works through
acceleration and deceleration (the Level 1 ramp passages need
D = 0.38–0.95, and the single 240 ms template still recognizes them
at 99.54% held-out). PWM is not used as a speed proxy: it does not
determine speed (**measured**: PWM 50 and PWM 43 give the same speed;
the Level 1 ramp needs D = 0.38 at PWM 76).

**Why one scale is enough.** The matched filter is tolerant of
width: a single fixed 240 ms template already matches 99.9% of the
945 curves above r = 0.7 across a 0.6–7× duration range
(**measured**), because normalized correlation is mostly a
rise-then-fall sign-pattern match. The slow end is the known cost:
only 30 held-out magnets sit below PWM 60, so slow passes are
thinly represented, and the wide flat-topped excursions in the slow
crawl session (`278F53…`) are recognized by no scale tried (§7).
That is a limitation of the evidence, not of the single scale — the
bank does no better there.

### 2.5 Where the recognizer runs

**Proposed (revised).** The design follows Claude's task split,
verified against the manual sketch: **hallTask conditions, rvmTask
correlates.** hallTask (core 0, priority 3, 1 kHz) takes the median
of the 5 Hall reads in each 1 ms tick and averages pairs to the
500 Hz series — the conditioning in §2.1. rvmTask wakes on each
20 ms block (the 70×2 detector's current block boundary) and
correlates the newly available samples against the single template.
There is no separate DSP task and no new queue: the existing
onset/diagnostic queues carry the recognized events, and the loop
thread consumes them and calls the matcher, exactly as it does
today. The recognizer owns no object the loop thread touches (the
existing `Navigator` rule). A declaration raises a reset request,
which the Hall task honours by resetting the recognizer's ring
buffer — the same flag mechanism (`recognizerResetRequest`) already
in place. The recognizer holds no state across a reset except the
template (built once at boot) and its event sequence counter.

**What happens to the 70×2 onset detector.** It is retired as the
detection path: its failure modes (merged signals, single-sample
spikes, baseline dependence) are the reason for this change. Its
window/dump machinery (`WaveformWindow`, `publishWaveformSlot`) is
kept for diagnostics during validation, so that recognized events can
still be inspected against their raw windows; whether it remains
after validation is an open decision (§9). The close-to-open guard
(operator ruling 2026-09-10) is *not* retired: it moves from the
recognizer to the matcher, as a physical-minimum advancement rule
(§4.5), because its duplicate-protection role is partly subsumed by
NMS and partly still needed for re-reads — but its value is
re-derived in consistent passage timestamps (§2.5.1), because the
measured maximum speed has moved since the ruling was made.

One correction to Claude's design document, noted here because the
implementation depends on it: the recognition side does **not**
receive "only 20 ms block means". The 70×2 detector in hallTask
sees every native sample (`.ino:134–135`, `:310`:
`candidate.onSample(uint16_t(raw), …)`); only rvmTask's
encounter/target logic sees blocks and onsets. The new recognizer's
conditioning runs in hallTask on the native samples, as above.

### 2.5.1 A note on re-reads and the physical-minimum rule

The guard exists because of a measured population: rebounds 1–64 ms
(n=152) and two known Otto double-reads at 209 ms and 290 ms
close-to-open. NMS (0.5 × D) suppresses maxima within ~120 ms of a
stronger one at D = 0.24 s, but a double-read 209 ms after the first
is outside that radius, and if it is a genuine second bump in the Hall
signal the filter will score it as a magnet. A rule is therefore
still needed — but as a *matcher* rule (do not advance twice within
the physical minimum), not as a recognizer gate (do not suppress the
observation). And the number in it has to be re-derived, because the
evidence it rested on has moved.

**Re-examination (revision 1).** The 500 ms close-to-open guard
(operator ruling 2026-09-10) was justified by the August 2026 survey:
the genuine population began at 836 ms close-to-open, and the 436 ms
event was judged a re-read because it required 473 mm/s, beyond the
stated fleet maximum of 400 mm/s (Toby's fastest in that survey was
319 mm/s). That 400 mm/s figure is the August survey's maximum —
cruise PWM 90, highest throttle 120 — not the locomotive's actual
maximum achievable speed.

**Measured (Oct 25/26 corpus, from the event ledger):** sustained
consecutive-magnet traverses at **600–650 mm/s** in session D1EC
(PWM ~180–186), with peak-to-peak intervals of 466–514 ms over
mapped spacings of 295–320 mm — a cluster of many pairs, consistent
speeds, not a one-off. The single fastest pair (708 mm/s, MM059→MM060
in 424 ms, session F420, PWM 90) sits inside a disturbed stretch
(F420 39–102 s) where the reference is known to be unreliable, and is
**not** relied on. The reliable maximum is therefore ~650 mm/s, and
the physical floor for a peak-to-peak traverse of the shortest mapped
interval (280 mm) is 280 / 650 ≈ **430 ms**.

**Consequence.** The current 500 ms close-to-open guard would refuse
genuine fast traverses: the D1EC pairs at 466–514 ms peak-to-peak
have close-to-open gaps of roughly 350–450 ms (the passage is
~100 ms wide at that speed), below 500 ms. The 500 ms value cannot
be carried over unchanged.

**Restated rule (proposed).** The rule compares consistent passage
timestamps — `t_passage` to `t_passage` (peak-to-peak), the same
timestamp the matcher uses for intervals and speed (§5). **No second
advance within 400 ms of the previous advance's `t_passage`.** 400 ms
sits just below the physical floor (~430 ms at the measured maximum
speed), so it refuses only intervals that are physically impossible.
The separation is narrow — the genuine fast population starts at
466 ms peak-to-peak, and the double-reads' peak-to-peak equivalents
have not been measured directly — so the exact threshold is a
validation item (§8): measure the double-reads' and the fast
traverses' peak-to-peak intervals from the corpus, and confirm the
maximum speed with a controlled high-speed run. Until then, 400 ms is
the proposed value, not a ruling.

---

## 3. Event data contract — what the recognizer emits

The recognizer emits one record per recognized magnetic encounter.
It never assigns a mile marker number. That is the matcher's
responsibility. It never discards an event because the observed
polarity disagrees with the expected magnet.

### 3.1 Fields

| Field | Type | Meaning |
|---|---|---|
| `seq` | uint32 | Event sequence number, increments per recognized event, resets on declaration. Diagnostic correlation, not identity. |
| `t_passage_us` | uint64 | **Estimated physical passage time** — the template-peak position (§5). The timestamp the matcher uses for distance and interval calculations. |
| `t_recognized_us` | uint64 | **Recognition time** — when the decision was made (window complete + NMS neighbourhood known). Always ≥ `t_passage_us`. |
| `polarity` | int8 (+1/−1) | Observed polarity. +1 = ADC rises = North on both locomotives (sign of A). |
| `r` | float | Correlation score (signed; sign agrees with `polarity`). The shape evidence. |
| `amplitude_adc` | float | Fitted template amplitude A, in ADC counts. The strength evidence. |
| `scale_s` | float | The template scale D that produced the event — **always 0.24 s** in this design (single scale, §2.4). Kept in the contract so a future scale change does not change the record format. |
| `pwm_at_passage` | uint8 | Applied PWM at `t_passage_us` (last sample in the bin). Context, not a gate. |
| `direction` | int8 | Recorded direction at `t_passage_us`. Context, not a gate. |

**Essential fields** (the matcher needs them): `seq`, `t_passage_us`,
`t_recognized_us`, `polarity`, `r`, `amplitude_adc`, `scale_s`.

**Context fields** (diagnostic, carried for the record):
`pwm_at_passage`, `direction`.

**Diagnostic-only** (not in the event; available through the
existing waveform dump, keyed by `seq`): the raw window samples and
the fitted level. The event record stays small so the queue
(depth 16) can hold several events without pressure; the full window
is fetched only when the operator asks for it.

### 3.2 When a candidate becomes a recognized encounter

All of the following, and nothing else:

1. The window's |r| ≥ 0.7 **and** |A| ≥ 70 counts (research
   defaults, §2.3).
2. The window is a local maximum of |r| over time (the single scale
   has no scale dimension to maximize over).
3. No stronger event exists within ±120 ms (0.5 × D at D = 0.24 s,
   non-maximum suppression).
4. No recognition window touched an acquisition gap longer than
   20 ms (invalid windows produce no event).

There is **no** additional condition. In particular: no polarity
check (the recognizer does not know the expected polarity), no
timing check against the last event, no PWM check (a parked event is
still an event — see §3.3), no baseline-return check.

### 3.3 Parked and PWM-0 events

The recognizer reports qualifying events regardless of PWM, with
`pwm_at_passage` recorded. **Measured:** parked data (4,274 s at
PWM 0) produced 7 events, all during start-up disturbances of
70–130 counts in two sessions (D1EC, F420). The matcher will not
advance on a PWM-0 event (§4), preserving the current X21 ruling
that no new navigation event may be created while the locomotive is
stopped — but the observation itself is published, because a
start-up disturbance is evidence about the sensor and the session,
not a nothing.

### 3.4 What the recognizer does not emit

- No mile marker, no "expected" flag, no ruling. Those are matcher
  outputs.
- No "not a magnet" verdict on a candidate that fails the thresholds.
  Sub-threshold windows are simply not events; they leave no record
  except in the waveform dump when the operator requests it. (The
  current firmware publishes a `NotAMagnet` ruling for recognizer
  refusals; under this design the refusal is the *absence* of an
  event, and the matcher never sees it. The distinction is kept in
  telemetry: the recognizer counts sub-threshold local maxima as a
  diagnostic counter, so a session that produces many near-threshold
  candidates is visible.)

---

## 4. Geographic matching interface — one target, a wider field of awareness

The matcher consumes recognized events in time order and owns the
position. Its design principle, from Sam's review of the MM066 case
and the operator's discussion with him, is: **NAVI pursues one
target, but interprets each encounter in the context of the
surrounding map. A contradiction should challenge its
interpretation, not erase its awareness.** NAVI remains focused on
the next expected magnet, but it remembers recent observations and
considers how they fit into the surrounding geographic sequence. That
does not mean searching all 171 magnets whenever something
disagrees — it means maintaining a small, geographically bounded
context around the last trusted position.

The matcher therefore keeps two things apart that earlier designs
conflated:

- **The confirmed position** — the last magnet confirmed by polarity
  agreement (or declared by the operator). This is what NAVI
  *knows*.
- **The search target** — the magnet NAVI is currently looking for.
  Normally the marker after the confirmed position. After a
  contradiction it advances past the contradicted magnet, because
  the locomotive has physically moved on — but advancing the search
  is not confirming the position. Those are separate decisions.

And it keeps two responsibilities apart:

- **Navigation:** maintain the most credible geographic
  interpretation of where the locomotive is.
- **Infrastructure integrity:** identify and report evidence that
  the physical railroad differs from its established map.

NAVI never silently changes the map, and it never pretends a
contradictory observation never happened. It can eventually conclude
that MM066 was probably encountered with the wrong polarity, report
that discrepancy, and maintain geographic awareness — without
changing the DNA map.

The matcher knows: the last confirmed mile marker, the search
target and its polarity, the mapped polarity sequence around the
believed position (`ROUTE_POLARITY`), the direction of travel, and
the **rearview mirror** — the rolling history of the last 10
observed polarities, in order, including disagreements. It uses no
IR distance, no timing gate, and no speed estimate. It ordinarily
evaluates the next expected magnet only; any investigation of
alternatives is bounded to alignments within ±10 MM of the believed
position (§4.3.2).

### 4.1 The matcher's inputs per event

For each recognized event, the matcher has: the event's
`t_passage_us`, `t_recognized_us`, `polarity`, `r`,
`amplitude_adc`, `scale_s`; the current confirmed position
`navMm`, `navDir`, the search target and its polarity; the mapped
polarity sequence; the rearview mirror (the last 10 observed
polarities); and the time since the last advance. It does **not**
have a distance measurement: there is no IR in this design (§1.2),
and PWM/elapsed-time is approximate movement information with no
authority to reject the target, declare it lost, or invoke recovery
(Oct 21 ruling).

### 4.2 Case A — expected encounter

The waveform is credible and its polarity agrees with the search
target. **Action:** advance exactly one marker (`navMm` moves to
the search target, which becomes the confirmed position), record the
advance with the event's `seq`, `t_passage_us`, `r` and
`amplitude_adc`, push the observed polarity onto the rearview
mirror, and record the evidence class `Confirmed`. The search target
moves to the next marker.

**No distance check.** Revision 1's ±15% IR window is withdrawn
with IR itself: the Oct 21 ruling excludes IR entirely from the
current stage and declares the ±15% tolerance vestigial, and no
reliable IR device exists. Polarity agreement against the expected
magnet, from a trusted position, is the confirmation. The spacing
table (`ROUTE_SPACING_MM`) is still used — by the survey timing map
(§5) and by the derivation of the physical-minimum rule (§2.5.1) —
but it no longer gates a distance check, because there is no
distance measurement to check against. Its reconciliation at
MM11–13 (§7.8) is still worth doing, but it no longer blocks the
matcher.

### 4.3 Case B — credible encounter with contradictory polarity

The waveform is credible but its polarity disagrees with the search
target. This is the MM066 case. The answer has changed twice, and
this revision states the current one plainly: **the matcher holds
the confirmed position, records the observation, advances the
search, and reconsiders.** It does not confirm the contradicted
magnet, and it does not discard the observation. That is rudimentary
judgment as a process, not a collection of rules:

1. **Expectation.** "I am approaching MM066. I expect a
   north-polarity magnet."
2. **Observation.** "I have encountered a credible south-polarity
   waveform."
3. **Provisional judgment.** "This does not match my target. I will
   not confirm MM066, but I will remember what I observed." The
   event is published as a first-class record with its full evidence
   (`seq`, `t_passage_us`, observed polarity, expected polarity,
   `r`, `amplitude_adc`, `scale_s`). It is not discarded, not
   relabelled as "not a magnet", and its polarity is retained in the
   rearview mirror — a detected wrong-polarity magnet is evidence
   (0092 point 12).
4. **Continued observation.** The locomotive has physically moved
   onward, so the search target advances to the next magnet (MM067)
   — but the confirmed position does not move, and nothing is
   confirmed on a single polarity match while the matcher is in this
   displaced state. Advancing the search and confirming the position
   are separate decisions: the first says "look for the next magnet
   now", the second says "I know where I am". A missing observation,
   by itself, does not establish that the locomotive has passed a
   magnet — without reliable distance evidence, NAVI needs
   subsequent observations to support that interpretation.
5. **Reconsideration.** Using the recent observations (the rearview
   mirror) and the surrounding map, what geographic interpretation
   best explains what has been seen? That is the bounded sequence
   check (§4.3.2). When it fires, the position re-anchors and the
   matcher returns to the normal state. Until then the position
   stays honestly unconfirmed: decisive when the evidence is clear,
   provisional when it is ambiguous, and willing to revise an
   earlier interpretation when subsequent observations provide
   better evidence.

The important difference from the Oct 21 hold rule is that
rejecting a candidate is not the end of the reasoning process. The
observation remains available, and NAVI can reconsider it in light
of what happens next. The important difference from revision 1's
provisional advance is that this design does not advance the
*position* on a contradiction: revision 1's advance was supported
by the IR distance, and without IR there is no distance evidence to
support it — the Oct 21 ruling's own answer (dispose of the
candidate, log it, TARGET unchanged) is the default, and the
sequence check is the mechanism that goes beyond it.

**When the prior position is NOT trusted** (declared but not yet
proven by the rolling history), the same rule applies — hold,
record, advance the search, reconsider. There is no weaker case in
which a contradiction confirms anything, because confirmation in
this design rests on polarity agreement or on the sequence check,
never on a single contradiction.

**What is rejected.** Advancing on the event while "correcting" the
map; searching the whole map for a place the observed polarity
fits — both would hide the fact that the world changed. And a
complicated multi-hypothesis recovery system (carrying several
candidate positions with probabilities and arbitrating between
them): the operator and Sam both reject it for this stage. The
sequence check below is the simplest mechanism that does the job,
and the track test (§8.2.9) is what decides whether anything more
is needed.

#### 4.3.1 The rearview mirror

The rearview mirror is the rolling history of the last 10 observed
polarities, in order, including disagreements. It is compared
against the map at the incumbent position and at proximal
alternatives; a single disagreement does not withdraw the position.
Missed magnets are UNKNOWN — they contribute neither agreement nor
disagreement, and a gap in the mirror is a gap, not evidence. The
history is retained across a correction; a correction changes NAVI's
interpretation of the observations, not the observations
themselves (0092 point 18). Trust is earned by agreement: after a
re-anchor the position is provisional until the rolling history
(approximately 10 MM / 3,000 mm of travel, 0092 point 15)
re-establishes it by agreement. The mirror is the whole of the
matcher's memory — 10 bytes — and it is what makes the sequence
check possible.

**One caveat, recorded plainly (Sam's assessment).** A rearview
mirror of ten observed polarities is not necessarily ten
consecutive physical magnets. A missed encounter leaves a gap —
the mirror skips a magnet — and a false encounter inserts an extra
polarity — the mirror gains one. Either shifts the alignment
between the mirror and the map. The budget of 2 mismatches in the
sequence check absorbs a single such event inside the window, but
two or more, or a shift that makes a wrong alignment look better
than the true one, are not absorbed. The replay results are
encouraging, but this is precisely why live validation matters
(§8.2.7): the mirror's behaviour under normal running, with real
misses and false events, is what the experiment must establish.

#### 4.3.2 The bounded sequence check — the reconsideration, measured

**The check.** After each event while displaced, compare the
rearview mirror (the last W = 10 observed polarities) against the
map at every alignment within ±10 MM of the search target — the
alignment's last element being the map position the most recent
observed event would occupy. Count mismatches per alignment.
**Re-anchor only if the best alignment has ≤ 2 mismatches AND
beats the second-best by ≥ 3.** On re-anchor, the confirmed
position becomes the map position of the most recent observed event
under the best alignment, the search target moves to the next
marker, and the matcher returns to the normal state. The
discrepancy that caused the displacement is published with the
re-anchor: which markers were observed with the wrong polarity, and
the evidence for the new interpretation.

**Why these parameters.** The margin is the safety property, and
the figures below are for the proposed matcher (advance the search),
not the hold variant — the two behave differently under the same
margin. At margin 2 the check fires on alignments that are wrong:
**49 wrong re-anchors of 85** (validation + transfer), and 98
incorrect advances — margin 2 is unsafe. At margin 3 it never fires
wrongly: **27 re-anchors across validation and transfer (29
including dev), all correct, 0 wrong.** At margin 4 it still fires,
all correct (25 across validation and transfer), but **later**: on
the F420 laps it holds 446 events instead of 156 and the longest
displacement grows from 16 to 43 magnets. Margin 3 is therefore the
chosen value — the strictest margin that fires promptly and
correctly. (For comparison, the hold variant at margin 4 never
fires at all — 0 re-anchors — which is why the margin figures must
not be read across variants.) The check waits for an
alternation-breaking magnet — the double-N at MM073/074 in the
F420 case — to disambiguate, and then it fires exactly. The budget
of 2 absorbs a spurious event or a missed magnet inside the
window.

**Measured (replay over the research event ledger, frozen-config
events; prototype `sequence_realign_prototype3.py`, W = 10, budget
≤ 2, margin ≥ 3, trigger on recurring disagreement):**

| Matcher | Validation (2,008 refs) | Transfer (4,019 refs) |
|---|---|---|
| Hold, no recovery (Claude's P8) | 847 correct / 598 incorrect / 553 held | 2,534 / 732 / 708 |
| Hold + sequence check | 1,782 / 152 / 64 | 3,550 / 226 / 198 |
| **Advance the search + sequence check (proposed)** | **1,808 / 4 / 186** | **3,599 / 1 / 374** |

Re-anchors: validation 11, transfer 16, dev 2 — **all correct, 0
wrong** in every partition. The advance-the-search variant is the
improvement: during a displacement nothing is confirmed on a single
polarity match, so the polarity-coincidence wrong advances that the
hold variant makes (152 validation, 226 transfer) almost vanish (4
and 1). The price is more held events (186 vs 64 validation, 374 vs
198 transfer) — the position stays honestly unconfirmed longer —
and slightly fewer correct advances on dev (1,652 vs 1,674), where
displacements are rare and the hold variant's coincidental
confirmations happen to be right more often than not.

**The MM066 session (F420, 1,685 refs, long CW run).** Hold: 569
correct / 578 incorrect / 539 held — wrong more often than right.
Hold + sequence check: 1,501 / 133 / 52, 10 re-anchors all
correct. **Advance the search + sequence check: 1,530 correct / 0
incorrect / 156 held, 11 re-anchors all correct, one per lap.**
Each lap's displacement lasts ~14–16 magnets: the check waits
until the evidence is unambiguous, then re-anchors exactly (in the
F420 laps, at true MM083 — best alignment 0 mismatches,
second-best 3). During the displacement the matcher holds and
still records every observation; it does not prevent the initial
wrong-polarity observation, it repairs the position from it.

**Limits, measured.** Displacement episodes: 33 across all
partitions; typical length ~14 events (the MM066-type case);
longest 36 events, in Toby's back-and-forth session (direction
reversals — the matcher assumes forward motion in the declared
direction, as NAVI does — where it re-anchors correctly to MM034
on every reversal). Four runs ended while displaced (2 dev, 2
validation); the position then stays at the last confirmed magnet
and the next run re-declares, which is the honest outcome. The
prototype uses the frozen-config (6-scale) event ledger, not the
single-scale events — the event sets differ by a handful of events
— so the sequence-check figures must be re-measured against the
single-scale event stream when the firmware recognizer exists
(§8.1.4). The dev baseline in the prototype differs from Claude's
(1,656/31 vs 1,586/68), probably run-boundary differences in
sessions with stops; the relative comparison on identical runs is
valid, but the absolute dev baseline is not Claude's.

**Telemetry for the experiment (Sam's assessment).** The mechanism
has not yet demonstrated that it is necessary in normal operation:
the replay evidence shows it repairs displacements, but not that
displacements needing repair occur often enough in normal running
to justify it. The experiment must therefore preserve **both**
geographic interpretations in telemetry — the simple target
matching (hold on contradiction, §4.3 as Claude's P8) and the
bounded sequence re-alignment — with each event carrying which
interpretation the matcher acted on, if either. Live running then
establishes whether the re-alignment improves reliability under
normal conditions, by direct comparison on the same runs (§8.2.7),
rather than by argument. Approving the experiment does not
prejudge that question: if the re-alignment never fires, or fires
without improving the outcome, the simpler matcher stands.

### 4.4 Case C — credible encounter earlier or later than predicted

The waveform is credible but its arrival time differs from the
movement estimate.

**Action:** the recognizer has already reported it (there is no
timing gate to suppress it). The matcher evaluates it on polarity
against the search target, exactly as in cases A and B. Timing is
context only: PWM/elapsed-time is approximate movement information
with no authority to reject the target, declare it lost, or invoke
recovery (Oct 21 ruling). A credible event that arrives early or
late is therefore treated like any other credible event — if its
polarity matches the search target, advance (case A); if it
contradicts it, hold, record, advance the search and reconsider
(case B). There is no "expected arrival time" check in the matcher,
and no timing-derived recovery.

The timing-gate results in the research (§10 of the report) show
why: gates suppress hundreds of genuine encounters while rejecting
0–6 spurious ones, because there were only 7–8 spurious events to
reject. The timing map remains a diagnostic (the survey table is
still the reference for how long a traverse *should* take), not a
gate. A missed magnet is UNKNOWN — it contributes neither agreement
nor disagreement to the rearview mirror — and the expectation
advances only through the sequence check (§4.3.2), never by timing
alone: without a distance measurement, a missing observation does
not establish that the locomotive has passed a magnet.

### 4.5 Case D — multiple apparent encounters

The recognizer reports more than one candidate event in a short
interval. The matcher consumes events in time order and, for each,
applies the rules above. Two additional rules bound what it may do:

1. **At most one advance per event, in time order.** Each advance
   is decided by the case rules above (A–C) against the *new* search
   target (the marker after the one just advanced to). A second
   event that also matches is evaluated against the next marker, not
   the one already passed. In the displaced state there is no
   advance at all until the sequence check fires (§4.3.2).
2. **Physical-minimum advancement rule (proposed, re-derived in
   revision 1).** No second advance within **400 ms of the previous
   advance's `t_passage_us`** — peak-to-peak, the same timestamp the
   matcher uses for intervals and speed. The derivation is in
   §2.5.1: the shortest mapped interval is 280 mm and the measured
   maximum speed is ~650 mm/s, so the physical floor for a
   peak-to-peak traverse is ~430 ms, and 400 ms sits just below it.
   A second event inside 400 ms is therefore either a
   duplicate/rebound of the first magnet or a second magnet that
   cannot physically be the next one. The observation is preserved
   and published; only the advancement is withheld, with the reason
   recorded ("too soon after the previous advance"). The 500 ms
   close-to-open value from the 2026-09-10 ruling is not carried
   over: it would refuse genuine fast traverses (§2.5.1).

Note what this rule does **not** do: it does not suppress the event
in the recognizer, it does not delete the observation, and it does
not reinterpret the event as the next magnet. It is a matcher rule
about advancement, and it is the only reachability rule in the
design.

### 4.6 What the matcher never does

- It never searches the whole map for a place an observation fits.
- It never advances the confirmed position on a polarity
  disagreement. The only way a contradicted position is repaired is
  the bounded sequence check (§4.3.2), which re-anchors on the
  preponderance of the observed sequence, not on a single event —
  and then only when the evidence is unambiguous (margin ≥ 3).
- It never confirms a magnet in the displaced state on a single
  polarity match. Advancing the search and confirming the position
  are separate decisions (§4.3).
- It never advances more than one marker per event.
- It never auto-corrects the map.
- It never uses the recognizer's correlation score or amplitude as a
  substitute for polarity and sequence agreement. A strong score does
  not make a wrong-polarity event the right magnet; a weak score does
  not make a right-polarity event wrong. The score and amplitude are
  recorded with the event for the operator's inspection.

---

## 5. Timing analysis — recognition latency and position accuracy

The recognition delay is a fixed 359 ms after the waveform peak for
the single-scale configuration. That number needs careful handling:
delay is not the same as error.

### 5.1 Three times, kept distinct

| Time | Definition | When it is known |
|---|---|---|
| `t_passage` | Estimated time the locomotive crossed the magnetic reference position (the template-peak position). | Retrospectively, once the event is recognized. |
| `t_recognized` | When sufficient samples have arrived to identify the waveform (window complete + NMS neighbourhood known). | At the decision. |
| `t_reported` | When the recognition event becomes available to the matcher (queue delivery on the loop thread). | A few ms after `t_recognized` (queue + loop cadence). |

**Measured (causal emulation, single 240 ms scale, pad 0.5;
Claude's `design_analysis.py` §1, held-out Otto Oct 26 + Toby,
6,027 reference magnets):** the delay from the waveform peak to the
causal decision is a **fixed 359 ms** — window half 240 ms + NMS
hold 120 ms − 1 ms — in every PWM band. Causal recognition is
5,999 / 6,027 (99.54%), 8 false, 1 duplicate — identical to the
retrospective figures for the single scale, because the causal
constraint costs nothing at a fixed scale. (For comparison: the
6-scale bank's causal figures are 6,002 / 6,027, 13 false, 1 dup,
median latency 298 ms, p95 298 ms, worst 1,193 ms for a slow passage
at D = 0.95–1.5 s; the 3-scale bank's are 6,001 / 6,027, 11 false,
0 dup. The single scale gives up 3 magnets against the 6-scale
bank and has fewer false events.)

**Travel during the delay (measured).** At PWM 85–95 the locomotive
travels a median of **84 mm** during the 359 ms (p95 103 mm, max
261 mm); above PWM 95, median 111 mm, max 233 mm. The delay is
fixed in time, so the distance it represents grows with speed —
the stopping-point question (§5.3) is therefore a distance question
that depends on speed, and it is answered by a track test, not by
this document.

**Where the 359 ms goes.** The window completes 240 ms after the
peak (the template half-length of 120 ms plus the 0.5 × D = 120 ms
padding), and NMS needs a further 0.5 × D = 120 ms of neighbourhood
before a maximum can be confirmed: 240 ms + 120 ms − 1 ms = 359 ms.
Reducing the padding to 0 would cut the latency, at the cost of
more false events; the design keeps pad 0.5 and accepts the
latency. Whether 359 ms matters operationally is a navigation
question, not a recognition question (see §5.3), and it is an open
decision (§9).

### 5.2 Passage-time accuracy

**The passage time is the template-peak position, not the decision
time.** The template's peak is the fitted position of the magnet's
centre in the signal; the Hall peak occurs when the sensor is over
the magnet centre, so the fitted peak is the best estimate of the
crossing time.

**Measured (continuous replay, §7 of the report):** the event time
against the oracle extremum (the raw signal's peak) is **−2 to 0 ms
median, 6 ms at the 95th percentile**, worst 118 ms (dev), 218 ms
(validation), 36 ms (Toby). The synthetic overlap experiment (§4)
gives a consistent figure of 4 ms median, 20 ms at the 95th
percentile. Lap-to-lap interval spread is the same whether measured
by template peak (median SD 92.8 ms), oracle extremum (92.2 ms) or
threshold onset (93.0 ms) — lap-to-lap speed variation dominates, and
the choice of centre does not matter. At 280 mm/s, a 6 ms timing
error is ~2 mm of position error; even the worst observed 218 ms
(a slow passage) is ~60 mm, well under one magnet spacing.

**Limit (measured, stated plainly).** The 4 ms / 20 ms figures are
against the *signal* peak, not against a surveyed ground truth. The
absolute accuracy of `t_passage` against the magnet's surveyed
position has not been measured. Validation (§8) must compare
`t_passage` against the surveyed position using the survey timing
map across many laps, and by the standstill test (a magnet centred
under the sensor at rest must report the same `t_passage` as the
surveyed position). There is no IR distance to compare against —
that validation path is closed with IR.

**Early recognition is not viable as implemented.** Prefix templates
ending at peak + 0 / 0.15 / 0.30 × D give latencies of 120 / 156 /
192 ms, but produce 810–1,513 false and ~1,900–2,060 duplicate
events on validation. Do not use prefix recognition.

### 5.3 What the latency does and does not affect

- **Speed estimates.** The current firmware computes speed as surveyed
  distance / onset-to-onset time (`detectedAtMs`). Under this design
  the interval is `t_passage`-to-`t_passage` between successive
  confirmed magnets. The accuracy is unchanged in kind (the interval
  error is the sum of two ~4 ms errors against a ~1,000 ms interval)
  and the estimate is available 359 ms after the second passage,
  which is when the second event is recognized. The speed *display*
  is therefore ~359 ms stale; the speed *value* is not degraded.
- **Magnet-to-magnet travel intervals.** Same as speed: the interval
  is computed from `t_passage` values, so the interval itself is
  accurate; it is only known 359 ms after the later passage.
- **Position reporting.** The confirmed position is exact at
  `t_passage` and stale thereafter. Between recognitions, the
  position is the last confirmed marker plus the elapsed time since
  its `t_passage` and the expected interval to the next marker — a
  time-based estimate, clearly labelled as such (§5.4). It is not a
  confirmed position, and there is no distance measurement to
  support it: the IR-supported estimate is gone with IR.
- **Control decisions.** Any decision that needs "where is the
  locomotive *now*" (stopping at a station, the section-cruise
  throttle, the lead/follower choreography) works with a position
  that is up to 359 ms plus one inter-magnet interval old in *time*,
  but whose *last confirmed point* is exact. At PWM 90 the
  locomotive travels ~84 mm in 359 ms — about a quarter of a magnet
  spacing. Whether that matters for stopping accuracy is a
  navigation and stopping question to be tested on track (§8), not
  something this design assumes away. The design does not claim that
  timestamp correction eliminates operational latency: the locomotive
  has moved on by the time the event is known, and any controller
  must work with the time-based estimate between magnets, not
  pretend the recognition is instantaneous. The stopping-point
  offset — how far past the magnet the locomotive travels before the
  event is known — is an open decision (§9), to be set from the
  track test.

### 5.4 The position between magnets

**Proposed.** The authoritative position is the last confirmed marker
and its `t_passage`. The display position is that marker plus a
time-based estimate: the elapsed time since `t_passage` against the
expected interval to the next marker (from the survey timing map),
labelled as an estimate. The current firmware already publishes a
`dead_reckoned_mm` field for the dashboard and notes the name is
wrong because NAVI dead-reckons nothing; under this design the
field is the confirmed marker plus the time-based estimate, and it
is labelled as such — an estimate, not a measurement. There is no
IR distance to add, and the design does not invent one.

---

## 6. ESP32 feasibility assessment — CPU, memory, scheduling

All figures in this section are **estimates** unless labelled
measured. None has been measured on hardware for the matched filter.
The existing firmware's own measurements (heap, stack high-water
marks) are **measured** and are the baseline the estimates are
checked against.

### 6.1 What the recognizer needs

**Template storage (estimated).** The 128-point normalized shape is
stored in flash (256 bytes as int16). The single template is built
once at boot by resampling the shape: D = 0.24 s with 0.5 padding on
both sides → 241 samples. As int16 that is **~0.5 KB**; as float,
~1 KB. Int16 is sufficient: the template is a normalized shape
(values in [−1, 1] scaled to int16), and the correlation is a ratio,
so quantization error is negligible against the 1.5–3.0-count
baseline noise. (For comparison, the 3-scale bank is 3,510 samples —
~7 KB int16 / ~14 KB float — and the 6-scale bank 5,730 samples,
~11.5 KB / ~23 KB.)

**Sample buffering (estimated).** The ring buffer must hold the
window: 241 samples at 500 Hz (0.48 s of data). As int16, **~0.5
KB**; as float, ~1 KB. The buffer holds the 2 ms-binned,
median-filtered samples. **Total recognizer memory (estimated):
~3.5 KB** (Claude's measured figure for the single-scale
implementation: ~1 KB template + ~2 KB ring buffer + working state),
against Otto's **measured** free heap of 59–74 KB in the manual
test sketch (Oct 25–26, worst low-water mark 36.8 KB, published by
the firmware's own health telemetry) — and 27–35 KB in the October
production build. That leaves ~33 KB of headroom even at the worst
low-water mark. The single scale fits with room to spare; memory is
no longer the reason to choose between configurations.

**Working memory (estimated).** The level and energy terms of the
sliding regression are maintained with running sums, O(1) per
sample. The template dot product is O(L) per output sample — that
is the MAC count below. No per-window allocation: all buffers are
allocated once at boot.

**Processing frequency (estimated).** The recognizer correlates in
rvmTask, which wakes on each 20 ms block and processes the 10 newly
available 500 Hz samples (§2.5).

### 6.2 CPU (estimated)

| Configuration | MACs per output sample | MAC/s at 500 Hz | CPU at ~4 cycles/MAC, 240 MHz core |
|---|---|---|---|
| **Single 240 ms scale (proposed)** | **360** | **0.18 M** | **<1%** |
| 3-scale bank (0.24/0.6/1.5 s) | 3,510 | 1.76 M | ~3% |
| 6-scale bank (frozen research config) | 5,730 | 2.9 M | ~5% |

These are the report's §11 estimates, transcribed. The single scale
at <1% of one core is negligible inside rvmTask. The sequence check
adds almost nothing: 21 alignments × 10 comparisons per event,
a few hundred integer operations, in the loop thread. No cost
reduction is needed, and none is proposed pre-emptively.

### 6.3 Interaction with existing real-time tasks

- `hallTask` (core 0, priority 3, 8 KB stack, 1 kHz): conditioning
  only — the per-tick median of 5 and the pair average to 500 Hz.
  Negligible CPU; the Hall acquisition itself (analogRead, the
  median, the binning) is unchanged.
- `rvmTask`: correlates the newly available samples against the
  single template on each 20 ms block wake. ~0.18 M MAC/s, <1% of
  one core. It must not block: the correlation for a block is
  computed from the ring buffer and the precomputed template, with
  no allocation and no waiting.
- `networkTask` (core 1, priority 1, 8 KB stack): unaffected — it is
  on the other core, and the recognizer does not touch the network.
- The loop thread (Arduino `loop`, core 1): consumes recognized
  events from the queue, exactly as it consumes `judgedQ` today. The
  matcher is a small state machine (polarity, the rearview mirror,
  the sequence check, the physical-minimum rule); its per-event cost
  is negligible. No change to the loop cadence is expected, but the
  loop-thread responsiveness must be measured (§8) because the queue
  depth (16) is the back-pressure limit: if the loop thread ever
  falls behind by more than 16 events, events are dropped — and a
  dropped event must be counted and reported, not silently lost (the
  current firmware already counts queue drops).
- **Wireless and control responsiveness (estimated):** no impact is
  expected. The recognizer is on core 0; WiFi/MQTT are on core 1; the
  motor control (ramp, throttle) is driven from the loop thread,
  which the recognizer does not block. This must be confirmed by
  measurement (§8), not asserted.

### 6.4 Lowest-cost configuration that meets the requirements

The single 240 ms scale is the recommended configuration, not a
fallback: ~3.5 KB of memory, 360 MACs per sample, 99.54% recall
causal on held-out Otto + Toby (**measured**, Claude's
`design_analysis.py`). The 3-scale bank is withdrawn (§2.4): it
costs ~12 KB and ~3% CPU for +2 magnets in 6,027 and more false
events. The known cost of the single scale is the slow end (D >
0.24 s) — see §7. Memory is no longer a decision: the single scale
fits with ~33 KB of headroom even at the worst measured low-water
mark (36.8 KB).

---

## 7. Known limitations — grounded in recorded evidence

1. **The merged-signal condition is not shown solved.** No raw Hall
   recording of the 2026-10-25 12:06 MM108–120 failure exists — only
   the packaged-encounter log. The research searched every Oct 25/26
   capture and found no physically overlapping magnet waveforms, and
   the recognizer does not depend on baseline return (99.3% recall
   when 20–50 counts off baseline, 92% when >50 counts off), but
   that is supporting evidence, not a replay of the failure. The
   condition remains the single most important untested point.
2. **Background steps and bursts are the main false-event source.**
   **Measured:** Otto's false events are almost all background steps
   and noise bursts while moving or starting (a step up and back down
   within 0.3–1 s looks like a bump; wide flat-topped ~400-count
   excursions during a slow crawl). Their |A| is 102–390, the same
   range as magnets, so **no amplitude floor removes them**. What
   they are physically is unknown. The first question is physical
   (what are they?), and a bounded track test with instrumentation is
   the right next step — not a new rule (§8). The matcher can decline
   to advance on them (wrong polarity, or too soon after the previous
   advance), but it cannot tell them apart from magnets on shape
   alone.
3. **Fast baseline drift costs recognition.** **Measured:** ramps of
   100 counts/s drop recognition to 1,815/2,008 with 53 false
   events; sine drift at 210 counts/s gives 476 false events. The
   level-only window absorbs slow drift, not fast drift. The
   level+slope window recovers more under fast ramps (1,930 vs 1,815)
   but produces 1,055 false events there. Neither is a solution; the
   drift rate on the actual railway is the question, and it is not
   yet characterized.
4. **Slow crawl wider than the single 240 ms scale is thinly
   tested.** Only 30 held-out magnets sit below PWM 60, and the
   wide flat-topped excursions in the slow crawl session (`278F53…`)
   are not recognized as magnets by any scale tried — the bank does
   no better than the single scale there, so this is a limitation of
   the evidence, not of the single-scale choice. If the locomotive
   crawls slower than the scale covers, the recognizer will miss
   magnets. The operating envelope (minimum speed at which
   recognition is required) is an open decision (§9).
5. **Same-polarity pairs closer than ~250 ms cannot be separated.**
   **Simulated:** same-polarity pairs at ≤200 ms merge into a
   midpoint event or vanish, by every method tried (including
   matching pursuit, which helps only opposite-polarity pairs). This
   is a property of the waveform, not of the recognizer. It does not
   occur at physical route spacing (≥ ~9× FWHM, i.e. ≥750 ms at
   PWM-90 speed), so it is not a field limitation — but it is the
   reason the physical-minimum advancement rule (§4.5) exists as a
   matcher rule, and why no closer spacing should ever be relied on.
   The separation between the double-read population and the genuine
   fast population is narrow (§2.5.1), and the exact threshold is a
   validation item.
6. **Causal recognition equals retrospective for the single scale.**
   **Measured:** 5,999 / 6,027 causal and retrospective alike — the
   fixed scale costs nothing under the causal constraint. (For
   comparison, the 6-scale bank is 99.35% causal vs 99.60%
   retrospective on validation; the difference there is events
   decided late or not at all under the causal constraint.)
7. **The reference is route-anchored oracle evidence, not physical
   ground truth.** In disturbed stretches (D1EC 64–76 s and 176–192 s,
   F420 39–102 s) some reported false events and misses are reference
   errors, not recognizer errors. The headline percentages carry that
   uncertainty.
8. **The spacing tables disagree.** `ROUTE_SPACING_MM` in
   `NAVI_ONE_X20/RouteMap.h` and in the manual test sketch's
   `RouteMap.h` differ at MM11 (290 vs 335 mm), MM12 (300 vs 324 mm)
   and MM13 (315 vs 360 mm). The polarity tables are identical. The
   matcher no longer relies on spacing for a distance check (§4.2),
   so this no longer blocks the matcher — but the survey timing map
   and the physical-minimum derivation do use spacing, so the tables
   should still be reconciled (§9).
9. **The sequence check assumes forward motion in the declared
   direction.** **Measured:** in Toby's back-and-forth session the
   matcher displaces and re-anchors correctly on every reversal
   (seven episodes of 36 events, all re-anchored to MM034), but it
   is displaced for the whole reversal. Direction reversals are
   outside the design's operating assumption, as they are outside
   NAVI's; the behaviour is honest (unconfirmed, not wrong) but the
   position is unavailable during a reversal.
10. **The sequence-check replay figures use the frozen-config event
    ledger, not the single-scale events.** The event sets differ by
    a handful of events, so the sequence-check figures (§4.3.2)
    must be re-measured against the single-scale event stream when
    the firmware recognizer exists (§8.1.4). The dev baseline in
    the prototype also differs from Claude's (1,656/31 vs 1,586/68),
    probably run-boundary differences in sessions with stops; the
    relative comparison on identical runs is valid, but the absolute
    dev baseline is not Claude's.
11. **The ESP32 figures are estimates.** CPU and memory (§6) have not
    been measured on hardware. The design's feasibility claim is
    provisional until §8 measures it.
12. **The rearview mirror is observations, not magnets.** Ten
    observed polarities are not necessarily ten consecutive physical
    magnets: a missed encounter leaves a gap and a false encounter
    inserts an extra polarity, and either shifts the alignment
    (§4.3.1). The budget of 2 mismatches absorbs a single such
    event inside the window. The mirror's behaviour under normal
    running, with real misses and false events, is untested — it is
    what the live experiment (§8.2.7) must establish, by comparing
    both geographic interpretations on the same runs.

---

## 8. Validation plan — replay and hardware tests

Separate offline replay performance from live firmware performance.
A replay pass does not establish live behaviour.

### 8.1 Offline replay (host)

1. **Reproduce the single-scale reference (revised).** The firmware
   recognizer code (the same source, compiled for the host) must
   reproduce the **single 240 ms, pad 0.5** scores on the 39-capture
   corpus (Claude's `design_analysis.py` §1): causal held-out Otto
   Oct 26 + Toby 5,999/6,027 (8 false, 1 duplicate); dev 1,710/1,719
   (4 false, 1 duplicate). Level 1 (the NSR file, 22 genuine
   magnets) has no published single-scale figure — Claude's analysis
   covers dev and held-out only — so the firmware build must report
   its Level 1 score, with the frozen config's 22/22 (0 false) as
   the comparison. The revision 1
   draft required reproducing the 3-scale scores; that was wrong —
   the implementation must be validated against its own
   configuration, and the configuration is now the single scale. The
   3-scale results (causal held-out 6,001/6,027, 11 false, 0 dup)
   and the 6-scale results (6,002/6,027, 13 false, 1 dup) are
   retained as comparisons, showing what the extra scales buy (+2
   and +3 magnets against more false events). Any difference between
   the host build and the research harness must be explained, not
   adjusted away.
2. **Verify event timestamps.** `t_passage` against the oracle
   extremum: median within ±5 ms, p95 ≤ 10 ms (the research's
   continuous-replay figures are −2 ms median, 6 ms p95). Compare
   `t_passage` against the surveyed magnet position using the survey
   timing map across many laps, and by the standstill test (a magnet
   centred under the sensor at rest must report the same
   `t_passage` as the surveyed position), to measure the absolute
   accuracy (§5.2). There is no IR distance to compare against.
3. **Verify the causal emulation.** The causal figures for the
   single scale already exist (Claude's `design_analysis.py` §1:
   5,999/6,027, 8 false, 1 duplicate, fixed 359 ms latency) and are
   identical to the retrospective figures. The firmware build must
   reproduce *those* figures under the causal constraint (no future
   samples beyond the decision time).
4. **Re-measure the sequence check against the single-scale event
   stream (new).** The sequence-check replay figures (§4.3.2) use
   the frozen-config (6-scale) event ledger. Once the firmware
   recognizer produces the single-scale event stream, re-run the
   prototype (`sequence_realign_prototype3.py`) over it and confirm
   the same behaviour: re-anchors all correct, none wrong, incorrect
   advances in single figures on validation and transfer. Until
   then the sequence-check acceptance is "to be re-measured", not
   assumed from the 6-scale ledger.
5. **Regression on the stress conditions.** Re-run the baseline
   stress suite (offsets, drift, ramps, steps, noise, spikes) against
   the firmware recognizer; the numbers must match the research
   within the reference's uncertainty.

### 8.2 Hardware tests (on the locomotive)

6. **Memory and CPU.** Measure `ESP.getFreeHeap()` /
   `ESP.getMinFreeHeap()` and the task stack high-water marks with
   the recognizer loaded, at rest and during a run, on Otto and on
   Toby. Acceptance: ≥10 KB heap headroom after the recognizer is
   loaded, and no stack high-water mark below 1 KB. Measure the
   loop-thread cadence (queue depth over a run) to confirm no event
   is dropped.
7. **Ordinary operation, Otto and Toby.** Manual-mode runs at PWM 30,
   60, 90 and 120, CW and CCW, several laps each. Record the NSR
   stream and the recognized events. Acceptance: recall ≥99.5% of
   route magnets on replay of the NSR stream, with the same false
   rate as the research (≤8 per ~2,000 magnets). The live recognizer
   and the replay must agree event-for-event on the same recording.
   **Both geographic interpretations are preserved in telemetry**
   (§4.3.2): the simple target matching and the bounded sequence
   re-alignment, with each event carrying which interpretation the
   matcher acted on. The comparison is the point of the experiment:
   on the same runs, how often does the re-alignment fire, how
   often does it change the outcome, and does it improve
   reliability under normal conditions? If it never fires, or fires
   without improving the outcome, the simpler matcher stands.
8. **Baseline disturbances.** Runs that include the known disturbed
   conditions (start-up, the D1EC/F420 stretches if reproducible,
   the slow crawl) to confirm the recognizer's behaviour matches the
   replay.
9. **The MM066–069 stretch after the repair (revised).** The three
   magnets have already been returned to their mapped polarities
   (the operator's track work; Sam's assessment 2026-10-27). The
   test is therefore the normal one: run the stretch and confirm
   normal advancement resumes through MM066–069 — the recognizer
   reports the mapped polarities, the matcher advances on agreement,
   no displacement occurs, and no discrepancy is published. **The
   repaired railroad is not disturbed to test fault detection.**
   A deliberate fault-detection test (re-flipping the magnets to
   their wrong polarities and confirming the matcher holds, records
   and re-anchors) is deferred until normal navigation has been
   proven, and needs a separate decision then. The fault-detection
   behaviour is already evidenced in replay: the F420 run is the
   flipped state, and the sequence check re-anchors correctly on
   every lap of it (§4.3.2). What the live test must still establish
   is the normal case — and whether the re-alignment ever fires, or
   needs to fire, under normal conditions (§8.2.7).
10. **Timing estimates do not erase genuine events.** Confirm on a
   live run that no genuine waveform event is suppressed by any
   timing-derived rule: every recognized event appears in the
   observation record, whether or not the matcher advanced on it.
   (There is no timing gate in this design; the test is that none
   has crept in.)
11. **The merged-signal condition.** If a raw trace containing the
    merged-signal condition is ever recorded (signal not returning to
    baseline across magnet spacings), replay the recognizer over it.
    This remains the one untested point (§7.1) and is the highest
    value validation available.
12. **The physical-minimum threshold and the maximum speed
    (new).** Measure the double-reads' and the fast traverses'
    peak-to-peak intervals directly from the corpus, and confirm the
    maximum speed with a controlled high-speed run (the D1EC session
    is the evidence, but a deliberate max-speed measurement would
    nail it). The 400 ms peak-to-peak threshold (§4.5) is confirmed
    only if it separates the two populations with margin on both
    sides.

### 8.3 Acceptance criteria

- Replay: the **single-scale** scores reproduced (§8.1.1); timestamp
  accuracy within the bounds in §8.1.2; the causal single-scale
  figures reproduced (§8.1.3); the sequence check re-measured
  against the single-scale event stream (§8.1.4).
- Live: recall ≥99.5% on ordinary runs (§8.2.7); normal advancement
  through the repaired MM066–069 stretch with no displacement
  (§8.2.9); both geographic interpretations preserved in telemetry
  and compared on the same runs (§8.2.7); no event erased by a
  timing rule (§8.2.10); heap headroom ≥10 KB and no dropped events
  (§8.2.6).
- Criteria that must be established through hardware testing (not
  from replay): CPU and memory (§6), live-run recall, the MM066–069
  test, the operational significance of the 359 ms latency for
  stopping, the slow-crawl envelope (§7.4), and the physical-minimum
  threshold and maximum speed (§8.2.12).
- No performance guarantee beyond the evidence is claimed. The
  acceptance criteria are the research's measured figures, restated
  as thresholds, not promises.

---

## 9. Open decisions — only those genuinely requiring operator or engineering approval

1. **Amplitude floor: absolute 70 counts, or the current relative
   floor (0.34 × trailing median accepted peak)?** The design
   recommends the absolute floor (directly measurable in the window,
   no history, consistent with the MM012 evidence: minimum observed
   magnet 76 counts). The relative floor survives a global amplitude
   shift (e.g. a sensor-height change) that a fixed floor does not.
   The operator decides; the choice is a constant, not an
   architecture.
2. **The bounded sequence re-alignment (new).** The matcher in
   §4.3.2 re-anchors the confirmed position from the observed
   polarity sequence when the evidence is unambiguous. That changes
   the "no automatic recovery" stance of the manual sketch's
   governing decisions: the matcher now repairs its own position,
   bounded to ±10 MM and only on a margin of ≥ 3. The alternative is
   Claude's pure hold (dispose, log, TARGET unchanged) with no
   recovery at all — the position then stays wrong until the
   operator re-declares. The replay evidence favours the check
   (validation incorrect advances 598 → 4, transfer 732 → 1, all
   29 re-anchors correct), but the change of stance needs the
   operator's explicit approval. **Sam's recommendation (2026-10-27):
   approve for implementation and manual-only experimental
   evaluation, but not yet for authoritative automatic navigation.**
   Approving the experiment is not approving automatic geographic
   correction for operational use. The experiment must preserve both
   geographic interpretations in telemetry (§4.3.2, §8.2.7), and
   live running must establish whether the re-alignment improves
   reliability under normal conditions — it has not yet demonstrated
   that it is necessary there. If the operator does not approve it
   even for the experiment, the matcher is the hold, and nothing
   else in the design changes.
3. **Motion while the position is displaced (revised).** In manual
   mode this is moot — the operator drives. For any future automatic
   operation: while the matcher is displaced (the confirmed position
   is honestly unconfirmed, §4.3), should the locomotive continue
   moving, or hold until the sequence check re-anchors? The design
   makes no recommendation for automatic operation, because there is
   no automatic operation in this stage. The decision is recorded
   here so it is not taken implicitly later.
4. **The physical-minimum threshold (revised).** Proposed: 400 ms
   peak-to-peak (§4.5), derived from the measured maximum speed
   (~650 mm/s). The separation between the double-read population
   and the genuine fast population is narrow, and the exact threshold
   awaits the measurement in §8.2.12. The operator confirms the
   threshold after that measurement.
5. **The 70×2 onset detector: remove, or keep as a diagnostic
   cross-check during validation?** The design retires it as the
   detection path and keeps its window/dump machinery for
   diagnostics during validation. Whether it remains afterwards is
   the operator's call.
6. **Operating envelope: the minimum speed at which recognition is
   required.** The single scale is thinly tested below PWM 60 (§7.4).
   The operator states the slowest speed the recognizer must handle,
   and the scale is extended only if the evidence requires it.
7. **Implementation baseline: which production variant?** This design
   was reviewed against `NAVI_ONE_X20` on the research branch. The
   operator confirms which lineage and variant is the implementation
   baseline, and the design is re-checked against it before coding.
   (The NAVI_COHERENCE lineage is no longer the source of the
   matcher rule — the sequence check in §4.3.2 is new, and the
   provisional advance drawn from COHERENCE is withdrawn.)
8. **Spacing-table reconciliation.** `ROUTE_SPACING_MM` differs
   between `NAVI_ONE_X20` and the manual test sketch at MM11–13
   (§7.8). The operator (or the survey record) states which values
   are correct. This no longer blocks the matcher (there is no
   distance check), but the survey timing map and the
   physical-minimum derivation use spacing, so it is still worth
   settling.
9. **The 359 ms recognition latency and the stopping-point offset.**
   Whether the fixed 359 ms delay matters for stopping accuracy and
   the lead/follower choreography is a navigation question to be
   answered by a track test (§8.2), not by this document. The
   stopping-point offset — how far past the magnet the locomotive
   travels before the event is known (median 84 mm at PWM 90) — is
   set from that test. If the latency matters, the options are a
   smaller padding (more false events) or a time-based position
   estimate between magnets — both to be decided after the test, not
   now. There is no IR-supported estimate to fall back on.

**No longer open.** **IR is excluded entirely** — by the Oct 21
ruling (`NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`) and by the
absence of a reliable IR device. The ±15% tolerance is vestigial
with it. Re-admitting IR is a separate hardware question, not a
decision this document asks for. **The scale is settled:** the
single 240 ms scale, pad 0.5 (§2.4) — the evidence does not support
keeping the bank. **The matcher rule is proposed, not open-ended:**
hold, record, advance the search, and re-anchor by the bounded
sequence check (§4.3) — open decision 2 is whether to approve it,
not what it is.

---

## 10. Implementation recommendation — the minimal coherent design

**The design, in one paragraph.** Replace the `ExcursionDetector`
onset path and the `MagnetRecognizer` admission tests with a
matched-filter recognizer (per-tick median of 5 → pair-average to
500 Hz → single 240 ms template of the 945-curve mean, level-only
normalized window, r ≥ 0.7 and |A| ≥ 70, local maxima with ±120 ms
non-maximum suppression), conditioned in `hallTask` and correlated
in `rvmTask`, emitting recognized events (passage time, recognition
time, polarity, score, amplitude, scale) to the existing queue for
the loop thread. The matcher pursues one target but interprets each
encounter in the context of the surrounding map: it advances on
polarity agreement, and on a polarity contradiction it holds the
confirmed position, records the observation, advances the search,
and re-anchors by the bounded sequence check (rearview mirror of
the last 10 observed polarities, alignments within ±10 MM, budget
≤ 2 mismatches, margin ≥ 3) — never confirming on a single
contradiction, never auto-correcting the map. It applies the 400 ms
peak-to-peak physical-minimum advancement rule. There is no IR
anywhere in the design. No timing gate, no interval-derived scaling,
no matching pursuit, no joint fitting, no local-slope removal, no
global search, no multi-hypothesis recovery.

**Phasing (proposed).**

1. **Host replay harness first.** Compile the firmware recognizer
   source for the host and run it over the 39-capture corpus; it must
   reproduce the single-scale scores (§8.1.1) and the causal
   single-scale figures (§8.1.3), and the sequence check must be
   re-measured against the single-scale event stream (§8.1.4). This
   is a deterministic software test and can be done without touching
   a locomotive.
2. **Manual-only firmware integration.** Integrate the recognizer
   into a manual test sketch (the
   `NAVI_POWER_OF_NORMAL_MANUAL_ONLY` pattern: navigation-only,
   AUTO/GO refused), with the existing 70×2 detector retained as a
   diagnostic cross-check during the transition. Track-test it at
   PWM 30/60/90/120, CW and CCW, with NSR recording.
3. **The MM066–069 test** (§8.2.9) as the acceptance test for the
   repaired stretch — normal advancement, no displacement — with
   the deliberate fault-detection test deferred until normal
   navigation is proven.
4. **Review before any automatic operation.** No automatic running
   on the new recognizer until the operator has reviewed the track
   results. The next decision belongs to the operator after reviewing
   the proposed design and then the test evidence.

**What this design does not do.** It does not write a sketch. It does
not change the map. It does not change the governing specifications.
It does not claim the merged-signal problem is solved, the ESP32
figures are measured, or the latency is operationally insignificant.
Each of those is stated as an open question with a test attached.

---

## Appendix — source review log

**Research reviewed.**

- `research/navi_matched_filter/REPORT.md` (all sections, including
  the §10 geographic models and §11 cost estimates).
- `research/navi_matched_filter/mf.py` (recognizer core, 245 lines),
  `config.py` (frozen configuration), `truth.py` (route-anchored
  reference, 213 lines), `corpus.py` (partitions and scoring,
  112 lines), `exp_continuous.py`, `exp_overlap.py`,
  `exp_stress.py`, `exp_sweep.py`, `README.md`,
  `CORPUS_MANIFEST.md`, `results/continuous.json`,
  `results/event_ledger.csv` (8,032 events), `results/sweep_dev.json`,
  `results/stress_validation.json`.
- The 945-curve characterization (`research/navi_wolf_pack/
  independent_width_study/curve_shapes/REPORT.md`,
  `aligned_curves.csv`) and Muse's matched-filter evaluation
  (2026-10-26).

**Firmware reviewed (read-only, on the research branch).**

- `firmware/programs/NAVI_ONE/variants/NAVI_ONE_X20/`: the `.ino`
  (hallTask 1 kHz, core 0, priority 3, 8 KB stack; networkTask core 1;
  the loop thread draining `judgedQ`; the IR observer on GPIO 34 at
  100 Hz; heap/stack health telemetry), `MagnetRecognizer.h`
  (position-free recognizer; relative amplitude floor 0.34; 500 ms
  time guard, operator ruling 2026-09-10; morphology not in the
  production path), `Navigator.h` (`judge()`: polarity rule,
  strike latch on `WrongMagnet`, ten-magnet sequence word that can
  withdraw but never correct a position), `ExcursionDetector.h`,
  `RouteMap.h` (polarity table identical to the manual sketch's;
  spacing table differs at MM11–13).
- `NAVI_WAVEFORM_0_1/` (the
  70×2 detector, `NaviTarget`/`NaviTimingPredictor` timing gate,
  `NaviTimingProfiles.h`).
- **Revision 1 additions.** `docs/decisions/0090` (operator rulings
  2026-09-22: sequence authority, IR window ±15% widened from ±10%
  after 4/78 real magnets were refused, AUTO enabled) and `0092`
  (the NAVI/IR decision model, Sam's consolidation adopted verbatim
  by the operator: decisional inertia, proximal alternatives bounded
  to ±10 MM, possible positions first, corrections provisional,
  evidence retained). `firmware/programs/NAVI_COHERENCE/variants/
  NAVI_COHERENCE_0_5_AUTO_ENABLED/Navigator.h` (the
  `AdvancedWithDiscrepancy` ruling, `PolarityDiscrepancy` evidence
  class, the ±15% cumulative window over up to 10 steps,
  `MIN_MARKER_MS=500` as a fallback gate, the `Uncertain` state) and
  the 0.6 `IR_HEALTH` notes ("the existing 15% nominal screen is not
  a certified distance error bound"). The fastest
  consecutive-magnet traverses were computed from
  `results/event_ledger.csv` against the NAVI_ONE_X20 spacing table
  (D1EC cluster at 600–650 mm/s; the F420 708 mm/s pair is inside a
  disturbed stretch and is not relied on). (Note: the ±15% window
  and the provisional advance drawn from these sources are withdrawn
  in revision 2 with IR itself; the decisional-inertia and proximal
  investigation principles in 0092 points 12–14 still stand and are
  what §4.3 rests on.)
- **Amendment additions (Sam's assessment of Revision 2,
  2026-10-27, relayed by the operator).** Sam agrees with the three
  major decisions (no IR; Claude's single-scale recognizer;
  recognition separate from geographic interpretation) and
  recommends Decision 2 be approved for implementation and
  manual-only experimental evaluation, not yet for authoritative
  automatic navigation. He notes the three magnets have already
  been returned to their mapped polarities, so the live test must
  not disturb the repaired railroad to test fault detection; and
  that a rearview mirror of ten observed polarities is not
  necessarily ten consecutive physical magnets. The dual-telemetry
  requirement (preserve both geographic interpretations) is his
  recommendation for the experiment.
- **Revision 2 additions.** Claude's competing design
  (`docs/navi/NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md`, branch
  `docs/navi-waveform-recognizer-design-20261027` @ `7ec6430`, and
  `research/navi_waveform_design_20261027/design_analysis.py` with
  its output — the single-scale causal figures, the matcher-variant
  replay, the firmware-parity conditioning check) and Muse's
  evaluation of it (`~/workspace/navi-log-analysis/
  claude-waveform-recognizer-design-evaluation-20261027.md`). The
  operator's ruling `docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`
  (commit `348514e`, 2026-10-21: IR excluded entirely from the
  current stage, the ±15% tolerance vestigial, a
  polarity-contradicting departure disposed of with TARGET
  unchanged; PWM/time has no authority to reject the target,
  declare it lost, or invoke recovery). The operator's instruction
  (2026-10-27) to take waveform recognition as far as it will go
  without IR, and his discussion with Sam on focused attention vs
  broader awareness — the principle "NAVI pursues one target, but
  interprets each encounter in the context of the surrounding map",
  the distinction between advancing the search and confirming the
  position, and rudimentary judgment as a process (decisive when
  clear, provisional when ambiguous, willing to revise). Muse's
  bounded sequence re-alignment prototype
  (`~/workspace/navi-log-analysis/sequence_realign_prototype3.py`,
  replayed over the research event ledger).

**Facts established by the review that the design relies on.**

- The current production `Navigator::judge()` (NAVI_ONE_X20)
  contains no timing check; the timing gate exists only in the manual
  test sketch. The NAVI_COHERENCE lineage answers the polarity
  question differently from NAVI_ONE (it advances with the
  discrepancy recorded, `AdvancedWithDiscrepancy`) — but that
  answer depended on the IR distance, which is now excluded, so
  this design does not follow it. The revised answer is the
  Oct 21 ruling's hold plus the bounded sequence check (§4.3.2).
- The current `MagnetRecognizer` is already position-free and the
  current `Navigator` already owns identity — the separation this
  design prescribes is the existing structure, not a new one.
- IR is excluded entirely from the current stage (Oct 21 ruling),
  and no reliable IR device exists. The ±15% tolerance is vestigial
  with it. There is no IR observer, no IR distance, no IR history
  buffer and no IR-supported position estimate anywhere in this
  design.
- The 500 ms guard is an operator ruling (2026-09-10) whose physical
  justification rested on the August survey's 400 mm/s maximum; the
  October corpus shows sustained running at 600–650 mm/s, so the
  value is re-derived in revision 1 (§2.5.1).
- The firmware publishes its own heap and stack measurements; Otto's
  free heap was 27–35 KB in October in the production build and
  59–74 KB (worst low-water 36.8 KB) in the manual test sketch in
  late October.
- The single 240 ms scale, pad 0.5, matches the 6-scale bank almost
  exactly on causal held-out replay (5,999 vs 6,002 of 6,027, fewer
  false events) with a fixed 359 ms decision delay; the per-tick
  median-of-5 conditioning reproduces the research conditioning
  exactly. The bounded sequence check re-anchors correctly on every
  replay episode (29 re-anchors, 0 wrong) and reduces incorrect
  advances from 598 to 4 (validation) and 732 to 1 (transfer)
  against the pure hold.

**Not reviewed.** The `NAVI_ONE_X22` variant (it exists on the branch
with the same file set; whether it is the current production build is
open decision 7). The Raspberry Pi telemetry service and the NSR
decoder are used as the recording path and were not re-reviewed here.
