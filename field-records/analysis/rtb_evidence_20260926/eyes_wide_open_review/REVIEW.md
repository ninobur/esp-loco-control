# NAVI_EYES_WIDE_OPEN — adversarial design review with full-session replay

**Date:** 2026-09-27  **Status:** review and diagnostic replay only. No
firmware, threshold, IR, station or dashboard change. Nothing here is a
decision. Findings are classified as **SOUND PRINCIPLE**, **SUPPORTED BY
EVIDENCE**, **NEEDS CHARACTERIZATION** or **COUNTEREXAMPLE / FAILURE**.
Recommendations are labelled as such, and the architecture is David's to
decide.

**Evidence.** Both full raw sessions were retrieved from Git LFS and checked
against the package manifest's SHA-256 values:

- Otto XHR `C3B93D0B`: 3,537,200 contiguous samples, 1,548 recorder rulings.
- Toby QTRACE `D7651658`: 2,790,270 samples, 7,207 decisions.

Decoding used the repository's own `xhr_format` and `extract_passages`
readers from `claude/navi-ps-r1-20q-standards`.

**Method.**

- An offline **oracle** segmentation of each session is used for scoring only.
  It is non-causal: cores are |med5 − 4 s rolling median| > 60 with peak ≥ 100,
  and lobes are excluded by W-scaled margins. It finds 1,545 Otto cores
  against 1,548 recorder rulings, and 1,189 Toby cores.
- Proposal behaviour is tested with a **causal replay** (`scripts/replay.py`).
  It opens on ≥ 70 with 2 same-sign median-of-3 samples against a frozen
  reference. The reference for each encounter is derived from the previously
  *completed* interval.
- Synthetic offsets, injected errors and steps are deterministic software
  tests (AGENTS.md §8). They show what a rule does, not what the railway does.

**Distance.** W is the core's half-max width. Consecutive magnet centres lie
10.8–13.9 W apart (median about 12.4 W). With the 300 mm span stated in the
X22R header, **1 W ≈ 24 mm**. This is derived, not calibrated.

---

## 0. Verdict

| Question | Finding | Class |
|---|---|---|
| Q1 Causal / ESP32-implementable? | Yes. The reference is computed from a completed interval and used only afterwards. Needs a histogram of in-band samples plus a short ring buffer, not the whole interval's history (§4.1). | SOUND PRINCIPLE |
| Q2 Breaks the circularity? | **Only within an encounter.** The circularity *moves* to the interval-to-interval step: deciding which samples of the completed interval are ordinary needs a reference. Anchored to the frozen reference, one bad reference certifies the next. Self-referenced, a magnetic dwell becomes the reference. Neither closes the loop alone (§3.3). | COUNTEREXAMPLE / FAILURE (as stated) |
| Q3 Retrospective segmentation reliable? | Cores: yes. Lobes: yes within about 3–4 W at running speed. **But the reference does not need lobe segmentation.** Encounter recognition does (§3.2, §3.6). | SUPPORTED BY EVIDENCE |
| Q4 Very short ordinary region | Does not occur on this route at the observed speeds. Otto's shortest clean region is 186 samples (1.0 W); p1 is 252 (2.7 W). Toby's short cases are all capture dropouts. A missed MM makes the region longer, not shorter. | SUPPORTED (this route) / NEEDS CHARACTERIZATION (higher speed, other maps) |
| Q5 Adjacent intervals differ | Measured interval-to-interval error: p99 4–6, max 7–10 counts. Tolerance is ≥ 38. Beyond about 60 counts in one step, the anchored design **never recovers**. | SUPPORTED / FAILURE beyond ~60 |
| Q6 Tolerable reference error | **38–48 counts minimum** (p1 40–55) before the first failure. Failure is **not** a miss first: it is a lobe split, a wrong-polarity opening or a false opening on ordinary track. Weak Toby magnets set a miss floor of about 31–40 (§3.1). | SUPPORTED BY EVIDENCE |
| Q7 One encounter read as many | Yes. Slow creep onto a station magnet re-triggers (7 Otto cases, all ZERO_RAMP, PWM 28–40). Lobes split once reference error exceeds ~44 counts. QUORUM's ~38-count opener splits at e = 0 (`toby_difficult_disagree`). A strict RTB fixes creep only by risking permanent non-closure (§3.4). | COUNTEREXAMPLE |
| Q8 Missed MM | Harmless to the next reference: a merged interval gives p99 3–5, max 6–7, even with the missed core unmasked. Harmful only if Hall intervals are defined by NAVI's *map* position rather than by Hall facts (§4.8). | SUPPORTED |
| Q9 Boot | **Unsolved by the proposal.** A reference primed while parked on a magnet, or ≥ 60 counts off, deadlocks an anchored design for the whole session (Otto 824–946 misses; Toby 215–788). | COUNTEREXAMPLE / FAILURE |
| Q10 Reversal | No reversal (running backwards) in either session. Otto's lobe asymmetry did not flip across its CW → CCW handling change. The ~33-count level change coincided with it. | NEEDS CHARACTERIZATION |
| Q11 Gates removed or relocated? | Removed: SETTLE, QUIET, LEASH, CADENCE. **Relocated:** CLEAR → anchored segmentation mask; lost-lock recovery → still needed; prime → still needed; MOVING → still needed in the recovery path; rearm → RTB. | — |
| Q12 Simpler interpretation? | Yes. **The reference is the median of the last completed interval's in-band samples, anchored to the operative reference, with a physical escape when a running interval contains no in-band ordinary track.** Lobe segmentation belongs to encounter recognition and RTB, not to the reference. | Recommendation |

---

## 1. Verification of the proposal's §11 evidence

| Claim | Result |
|---|---|
| Otto `C3B93D0B` ~3.54 M samples, high quality | **Verified.** 3,537,200 samples, contiguous seq 0…3,537,199. |
| Toby `D7651658` ~2.79 M samples | **Verified**, but not continuous: 77 gaps > 50 ms, 14 > 500 ms (max 2.9 s), about 52.6 s missing. All Toby replay misses fall in one 10 s dropout episode (1933–1943 s). |
| Ordinary variability small vs excursion | **Verified.** Clean-interval MAD p50 1, p95 1–3. Weakest cores 123 (Otto) and 101 (Toby). |
| Otto operative baseline ~1935 while later ordinary ~1902–1906 | **Verified with a correction.** The baseline stays at 1935 for 3.08 M samples to the end of the session. Ordinary after 2280 s is **1899–1903** (p50 1901.5). |
| Leading and trailing opposite-sign lobes | **Verified on 1,545 + 1,189 cores** (§3.2). |
| `toby_difficult_disagree` = N leading lobe treated as S event | **Verified, with a cause the proposal omits.** QUORUM opened that event at **−39 counts**. QUORUM's opening threshold in this session is about **38 counts, not 70** (all 1,191 openings at 20–70, median 39). Under ≥ 70 with a correct reference, Toby's leading lobes do not open until the reference is ≥ 52 counts off. |
| Reference updated until departure is contaminated | **Verified** (passage study: −9 median, −22 worst for Toby). |
| Longer-term ordinary movement, ~33 counts on Otto | **Verified.** It is a step during a stationary handling event at about 2271 s (the X22R header calls it a direction reversal), not a drift while running. |
| David's > 100-count variation | **Not present in either session.** The fastest clean drift in repository records is 1.3 counts/min (`NAVI_BASELINE_DRIFT_20260916`). Whether field changes > 40 counts can occur *within one interval while running* is the most important unmeasured fact for this design. |

---

## 2. What EYES_WIDE_OPEN is, relative to X22R

X22/X22R already locks **one reference per mapped interval** and freezes it
while the next magnet is judged. The cycle is recognise N → clear N →
establish → LOCK → detect N+1. EYES_WIDE_OPEN changes three things:

1. **Which track supplies the reference.** X22 collects from the *current*
   interval, after CLEAR + SETTLE. EYES_WIDE_OPEN uses the *previously
   completed* interval. That adds about one interval of age (~300 mm, ~1.2 s).
   **Measured cost: none.** Error p99 is 4 for both on Otto; max 7 vs 10.5
   (§3.3).
2. **How samples are selected.** X22 uses forward gates (CLEAR, SETTLE,
   MOVING, CADENCE, QUIET). EYES_WIDE_OPEN uses retrospective segmentation
   with both boundaries visible.
3. **What RTB means.** X22 uses electrical rearm (30 in-band samples).
   EYES_WIDE_OPEN wants return to ordinary behaviour.

So EYES_WIDE_OPEN is a refinement of X22's lock architecture, not a
replacement for it. That matters for Q11: it inherits X22's hard cases
(magnet dwell, lost lock, boot prime), and the evidence below shows it needs
answers to each of them.

---

## 3. Empirical results

### 3.1 Q6: how much reference error the next encounter tolerates

`scripts/tolerance.py` and `tolerance_types.py` cover every non-stop magnet:
Otto 1,502, Toby 1,126. For each, the reference is set to the true
ordinary level plus e, for e = −160…+160. The detector starts disarmed at the
previous core's peak. "Toward" means the reference is displaced toward the
core's polarity, which shrinks the apparent departure.

**Minimum |e| at which the first failure of any type occurs** (min / p1):

| | Otto N | Otto S | Toby N | Toby S |
|---|---|---|---|---|
| reference toward core | 48 / 54 | 44 / 48 | 42 / 43 | **38** / 40 |
| reference away from core | 42 / 46 | 48 / 52 | 52 / 55 | 42 / 55 |

**Onset of each failure type** (min / p1 across magnets):

| failure | Otto | Toby |
|---|---|---|
| split on previous trailing lobe | 44–50 / 48–56 | 52–58 / 58–62 |
| leading-lobe opening, wrong polarity | 50–60 / 56–60 | 52 / 52 |
| wrong polarity at the core | 54–56 / 56–60 | 44–48 / 46–48 |
| false opening on ordinary track (noise, comb) | 56–62 / 58–64 | 44–58 / 58–61 |
| **miss** | 56–60 / 58–64 | **40–44** / 42–76 |

- **The failure mode is splitting and wrong polarity, not missing.**
  Splitting and wrong polarity are worse for navigation than a miss.
- **A structure-aware recogniser is still capped.** Even with lobes and
  polarity handled perfectly, it remains limited by false openings on ordinary
  track and by misses, about **40–45 counts**. The weakest Toby magnets
  (101–110 counts) put the miss floor at about 31–40.
- **So "10, 20, 30 counts may be adequate" is supported. "Perhaps more" is
  bounded near 40** on this railway with a 70-count opener.
- **The measured error sits far inside that tolerance.** Interval-to-interval
  reference error was at most 7–10 counts (§3.3), leaving 28+ counts of
  headroom. A smooth drift would need about 40 counts per interval, roughly
  2,000 counts/min, to use it up. The fastest measured drift is 1.3/min.
  Only **steps** matter.

### 3.2 Lobes at session scale

Median lobe depth relative to the neighbouring clean level (counts, opposite
sign to the core):

| | leading N / S | trailing N / S |
|---|---|---|
| Otto before handling | −4 / −9 | −10 / −11 |
| Otto after | −5 / −9 | −11 / −7 |

This matches the passage study: Otto's trailing lobe is larger. The asymmetry
did **not** flip across the CW → CCW handling change.

Lobes stay ≤ ~25 counts. A median over an interval of about 11 W, where lobes
occupy about 3 W, is shifted by only 1–2 counts.

### 3.3 Reference accuracy, and the circularity moving between intervals

Error is the reference minus the ordinary level just before the next core,
over all intervals (`results/reference_comparison.txt`):

| reference | Otto p50 / p99 / max | Toby p50 / p99 / max |
|---|---|---|
| EYES_WIDE_OPEN: previous completed interval, lobe-segmented | 1 / 4 / **238** | 1 / 6 / 9.5 |
| same, **plain in-band median, lobes included** | 1 / 5 / 238 | 1 / 7 / 30.5 |
| X22-like: start of current interval | 1 / 4 / 10.5 | 1 / 7 / 10.5 |
| held from session start | 2 / 34 / 38 | 1 / 5 / 11.5 |
| recorder operative baseline | 1 / 36 / 40 | 1 / 4.2 / 11.5 |

- **Without stops, EYES_WIDE_OPEN is within 7 (Otto) and 9.5 (Toby).**
- **Every large error is a station dwell.** The locomotive stands *on* a
  magnet for 30–80 s (MM64 +200, MM161 +160, a stop at −237). A time-weighted
  median of that interval is the magnet.
- **This is where the circularity reappears.** Causal replay of 1,550 Otto
  encounters:

| segmentation of the completed interval | Otto miss / extra / wrong-pol | Toby | worst Otto ref error |
|---|---|---|---|
| **anchored** (magnetic = > 60 from the frozen reference) | 0 / 7 / 0 | 3 / 3 / 2 | 32 (handling step) |
| **self-referenced** (magnetic = > 60 from the interval's own median) | 9 / 13 / 4, including 5 misses and a wrong polarity within 8 s after one dwell | 3 / 3 / 2 | **239** |

- **Anchored segmentation keeps a magnetic dwell out of the reference.** It
  does this by trusting the previous reference, and that is the circularity
  again, one level up (§3.5).
- **Self-referenced segmentation breaks the circularity** but converts every
  dwell into a wrong reference, and the errors cascade into the next several
  magnets.
- **Toby never dwells on a magnet**, so the two are identical there. All of
  Toby's errors are in capture dropouts.

### 3.4 RTB: the proposal's definition, tested against the frozen reference

"Returned to ordinary behaviour" needs *some* level to return to. The proposal
excludes "crossed a stored ADC value", but ordinary level cannot be recognised
by flatness alone. A magnetic dwell or fringe is also flat, per the X22 header:
Grillers at +200, and a 41-count shelf for 36.7 s. The replay therefore closed
encounters only after 100 samples within ±B of the frozen reference
(`results/rtb_band.txt`):

| RTB band | Otto miss / extra | worst encounter open-time | with a −45 step | Toby |
|---|---|---|---|---|
| ±70 electrical (30 samples) | 0 / 7 | 78 s (a dwell) | 0 / 7 | 3 / 3 |
| ±40 | 0 / 5 | 78 s | **27** / 5 | 3 / 1 |
| ±25 | **26** / 5 | 97 s | 65 / 5 | 3 / 0 |
| ±15 | **191** / 4 | **1,266 s** | 332 / 4 | 3 / 0 |

- **Tightening RTB toward "truly ordinary" removes only 2–3 creep and lobe
  extras.** If the ordinary level has moved by more than the band while an
  encounter is open, the encounter **never closes**.
- **That happens without any synthetic step.** Otto's real handling step
  (−34) at ±15 kept one encounter open for 21 minutes.
- **This is the same dilemma the X22 header records, moved into RTB:** "there
  is no time constant that separates a long approach into a magnet from a new
  resting level." EYES_WIDE_OPEN has not removed it.

### 3.5 Error propagation and recovery (Q13: one bad reference)

**Injected single bad reference, anchored design**
(`results/injection_anchored.txt`):

| injected error | Otto (at openings 300 and 900) | Toby |
|---|---|---|
| ±30, ±45 | recovers within 0–2 magnets | recovers within 0–2 |
| ±60 | **never recovers**: 457–1,055 consecutive bad magnets, up to +4,460 extra openings | recovers after 3–83 magnets |
| ±80, ±120 | **never recovers**: +249…+781 misses | never recovers: +47…+590 misses |

- **Mechanism.** Once the error exceeds about 60, every ordinary sample is
  "magnetic" to the anchored mask. No ordinary region is ever found, and the
  bad reference is kept for the rest of the session.
- **The same cliff hits boot and steps.** A boot reference primed on a magnet
  (±100–200 counts) behaves identically. A synthetic +60 step while running
  gives 282 extras and 57 wrong polarities; ±120 gives 90–276 misses.

**A physical escape restores recovery.** This was a probe, not a proposal
(`results/anchored_hybrid_self_scenarios.txt`). The rule: if the anchored
segmentation of a completed interval finds no ordinary track, re-derive the
reference from that interval itself, using only samples at cruise (PWM ≥ 70).
With it, every injection, boot-on-magnet and ±60/±120 step case was within
+4 misses and +2 extras of baseline on Otto, and within +5 misses and +2
extras on Toby. Station dwells stayed protected.

- **This is X22R's lost-lock recovery** (`lostMs` 2 s, `lostMinPwm` 70),
  relocated.
- **Its justification is spatial.** At cruise, a field cannot persist over
  more than a magnet's footprint.
- **It uses PWM as a stand-in for distance.** PWM does not prove motion:
  `otto_difficult_no_position` shows a full magnet passage at PWM 0.
  MM-referenced IR distance would be the physical evidence.

### 3.6 Q8: a missed MM

The test merges intervals k−1 and k as if magnet k had been missed (Otto 499
merges, Toby 380). The next reference error is p99 3–5 and max 6–7, whether
or not the missed core is masked (`results/missed_mm_merged_interval.txt`).
The median resists one unrecognised core, which makes up about 10 % of the
samples.

---

## 4. Answers to the review questions

**4.1 Q1: causal and implementable.** **SOUND PRINCIPLE.**

- The reference for encounter k+1 is computed after encounter k's core has
  completed (~0.1–0.3 s after its opening) and is used ≥ 7.8 W later.
  Nothing uses information that was not yet available.
- **Memory.** Storing the interval's history is not needed and would not fit
  a 30–80 s stop (80 k samples). A 141-bin histogram of in-band samples
  (±70 about the frozen reference, 564 bytes) gives the median exactly.
- **Lobe exclusion**, if kept, needs only a delay line: about 3 W of samples
  (≤ ~0.9 s at PWM 48) held back from the histogram until the next opening
  shows whether they were a leading lobe.
- The trailing exclusion is known at the start.

**4.2 Q2: circularity.** Freezing the reference while an encounter is judged
is **SOUND PRINCIPLE**: the current phenomenon cannot rewrite its own
reference. The proposal's broader claim is a **COUNTEREXAMPLE / FAILURE**.
The segmentation of the completed interval must still decide which samples
are magnetic, and that needs either:

- **the previous reference.** One bad reference then certifies the next, and
  errors above ~60 are permanent (§3.5);
- **the interval's own statistics.** A magnetic dwell then becomes the
  reference (§3.3).

The loop closes only with an **independent physical fact**. The best
candidate is motion or distance: a field that persists while the locomotive
travels further than any magnet's footprint is not a magnet.

**4.3 Q3: retrospective segmentation.**

- **Cores:** unambiguous (peaks 101–299 against MAD 1).
- **Lobes:** measurable. At running speed, lobes are gone by about 3 W before
  and 3–4 W after the core.
- **Ordinary region:** it exists in every Otto running interval (≥ 1.0 W,
  p1 2.7 W).
- **The distinction is not needed for the reference.** The plain in-band
  median is within 1–2 counts of the lobe-segmented one.
- **It is needed for encounter recognition and RTB.** This is where lobes
  cause splits and wrong polarity (§3.1). **SUPPORTED BY EVIDENCE.**
- **Recommendation:** keep "understanding the waveform" in the recogniser,
  and keep the reference estimator crude and robust.

**4.4 Q4: short ordinary region.** Not observed at the observed speeds.

- If W is spatial, the clean *distance* per span is fixed, about 65–110 mm.
  The clean *sample count* then falls with speed: at 3× Otto's cruise, about
  100 samples.
- For the X18 Otto recorder, 100 samples is the minimum that averages out its
  100 ms batch comb.
- A policy is still needed for "too little clean track". Keeping the previous
  reference is safe only if that reference is itself recoverable (§3.5).
- **NEEDS CHARACTERIZATION** for faster running, closer magnet pairs and
  other maps.

**4.5 Q5: adjacent intervals differ substantially.**

- Up to about **38–45 counts**, the next encounter is still recognised
  correctly, and the next interval re-establishes the level.
- **Beyond about 60 in one step, the anchored design fails permanently.**
- The one real event, Otto's −34 handling step, used **~80 %** of the margin.
- **SUPPORTED** up to ~40. **COUNTEREXAMPLE** beyond ~60 without an escape.

**4.6 Q6.** See §3.1. The tolerance is ~40 counts, and it is bounded by
splits, wrong polarity and false openings before misses.

**4.7 Q7: one encounter read as many.** **COUNTEREXAMPLE.**

- **Station creep.** In the replay, all 7 Otto extras are slow approaches
  onto station magnets: t = 1134.5 (×2), 1541.6 (×2), 1755.9, 1951.0 and
  2424.7 s, at PWM 28–40. This independently reproduces the four ZERO_RAMP
  re-triggers in `NAVI_BASELINE_TIMING_20260916`.
- **Lobes** split once reference error exceeds ~44 counts.
- **A low opening threshold** (QUORUM, ~38) splits even at zero error.
- **Other candidates, not observed here:** stopping on a magnet and then
  reversing off it, and oscillation near threshold during a stall.
- Structure-aware RTB would merge creep re-triggers only if RTB is strict,
  and strict RTB deadlocks (§3.4).

**4.8 Q8: missed MM / `POSITION_ADVANCED_SANS_MM`.**

- **For the reference:** harmless (§3.6).
- **Architectural hazard:** if "completed interval" is defined by NAVI's map
  position (an MM advanced without Hall), a navigation decision feeds back
  into the Hall reference.
- **Recommendation:** define Hall intervals by Hall facts (encounter to
  encounter), independent of map advancement. The map can label them.

**4.9 Q9: boot.** **COUNTEREXAMPLE.** The proposal has no first interval.

- A prime over the first 2 s, which is X22's `primeMs`, worked in both
  sessions because both locomotives booted off-magnet.
- Priming on a magnet deadlocks an anchored design permanently (§3.5).
- **A stored or fixed baseline performs better here.** A value from the
  previous run is closer than a prime taken on a magnet.
- Any design needs a recovery path that does not depend on the boot value.

**4.10 Q10: reversal.** **NEEDS CHARACTERIZATION.** Neither session contains
running backwards. From the evidence:

1. **Otto's large-trailing asymmetry persisted across its CW → CCW change.**
   The asymmetry is therefore probably sensor- or mount-specific, not a
   property of travel direction. But Otto still drove FWD, presumably turned
   by hand, so this is not a reversal test.
2. **The level changed −33 counts at that change.** Place, direction and
   handling are not separable in this record.
3. **At a reversal, the "previous completed interval" is the same span in the
   other direction.** It is bounded by the same magnet twice, often with a
   stop in it. If a real reversal shifts level by ~33, that uses most of the
   ~40-count margin.

A bounded track test is needed: run forward, then backward, over the same
spans, with no handling.

**4.11 Q11: gates.**

- **Removed by EYES_WIDE_OPEN, and the evidence supports removing them:**
  - SETTLE (timer): replaced by retrospective lobe handling, which the
    reference does not even need.
  - QUIET (spread ≤ 32): the robust median makes it unnecessary.
  - LEASH: already unreachable.
  - CADENCE: the anchored replay without it handled all 22 Otto stops within
    32 counts.
- **Relocated, not removed:**
  - **CLEAR** → the anchored segmentation mask. Same function (protects
    against magnetic dwells), same cliff.
  - **Lost-lock recovery** → still required (§3.5).
  - **Prime** → still required (Q9).
  - **MOVING** → still required in the recovery path, where it is a distance
    stand-in.
  - **Electrical rearm** → RTB. The proposal's stricter RTB carries a deadlock
    risk (§3.4).
- **Net result:** fewer procedural gates in the normal path. The hard cases
  keep exactly the mechanisms X22 has, restated as physical conditions.

**4.12 Q12: simpler interpretation.** Recommendation, not decision:

> The reference for the next encounter is the **median of the in-band samples
> of the last completed Hall interval**, where in-band means within the
> opening threshold of the operative reference. If a completed interval
> travelled farther than any magnet's footprint without any in-band ordinary
> track, the operative reference, not the signal, is wrong. The reference is
> then re-derived from that interval's own running samples.

This uses two physical facts (magnets are short in space; ordinary track
dominates distance) and no timers. It reproduces EYES_WIDE_OPEN's measured
accuracy without lobe segmentation, and it recovered from every injected,
boot and step failure in the replay. What remains open is the source of
"travelled farther", which is IR (§5), and the RTB definition (§3.4).

---

## 5. Implications for IR (flagged, not resolved)

- The only closure found for the circularity is **motion or distance
  evidence**, used for *reference validity* and not for agreement with Hall.
  That is a different IR role from the ±15 % gate.
- In this replay, "cruise PWM" stood in for it. PWM ≠ motion
  (`otto_difficult_no_position`).
- The 7 creep re-triggers at stations are what the refractory and the 650 ms
  Hall-only fallback absorbed. Removing time guards needs a recogniser that
  merges creep, and that interacts with RTB.

---

## 6. Adversarial checklist

| Probe | Result | Class |
|---|---|---|
| Circular reasoning | Moves from within an encounter to between intervals (§4.2) | FAILURE (as stated) |
| Future information | None. The reference is used ≥ 7.8 W after its data completes. | SOUND |
| Hidden timers or distance proxies | Rearm count (30 or 100 samples); W-scaled margins (derived from W, a measured distance); 100-sample minimum (instrument comb); cruise PWM in any recovery path (distance proxy) | Needs explicit ownership |
| Recovery after one bad reference | ≤ 45: 0–2 magnets. ≥ 60 (Otto) / ≥ 80 (Toby): never, without an escape | FAILURE |
| Error propagation N → N+1 | Anchored: yes, above ~60. Self: one dwell error cascades into 5 misses and a wrong polarity within 8 s | FAILURE (each alone) |
| Lobe contamination of the reference | 1–2 counts; negligible | SUPPORTED |
| Transients admitted as ordinary | Otto comb biases the median +1; Toby spikes 9–43/s, median-immune | SUPPORTED |
| Short intervals | None on this route; min 1.0 W | NEEDS CHARACTERIZATION (speed, map) |
| Stops / handling | Dwell on magnet (30–80 s): anchored OK, self fails. Handling step −34 while stopped: anchored OK at 80 % of margin | COUNTEREXAMPLE (self) |
| Reversals | No data | NEEDS CHARACTERIZATION |
| Environmental steps | ≤ 45 fine; ≥ 60 anchored deadlock. Field rate of change unknown | NEEDS CHARACTERIZATION |
| Old or fixed baseline better | Boot on a magnet. Toby's whole session (held = EYES_WIDE_OPEN) | COUNTEREXAMPLE |
| QUORUM low threshold | 38-count opener splits leading lobes at zero error | COUNTEREXAMPLE (to the ≥ 70 framing) |

---

## 7. Recommended bounded track tests (not authorised; for David)

1. **Rate of ordinary change.** Hall recorder running through sun/shade and
   temperature transitions, reporting the per-interval clean median. This
   decides whether steps > 40 per interval exist while running.
2. **Reversal.** Forward, then backward, over the same 10 spans, no handling:
   level offset, lobe sides, and encounters at the reversal point.
3. **Boot on a magnet.** Power up parked on a station magnet: prime value and
   recovery.
4. **Stop-on-magnet and creep at stations,** with IR distance recorded: does a
   distance criterion separate dwell from new ordinary level?

## 8. Limitations

- One session per locomotive, both evening and stable. No morning, sun/shade,
  reversal or boot-on-magnet evidence.
- Toby's capture has about 52.6 s of dropouts.
- The oracle's lobe margins and 60-count core rule come from the passage
  study. The replay's opener approximates X22 (median-of-3, 2 same-sign
  samples, 30-sample rearm). It is not the firmware.
- Injected errors and steps are synthetic tests of rule behaviour, not
  railway observations.
- W → mm uses the X22R header's 300 mm span. Not independently calibrated.

## Reproduce

Needs the LFS sources and `tools/xhr_format.py`, `tools/xhr_decode.py` and
`scripts/extract_passages.py` from `claude/navi-ps-r1-20q-standards` next to
the scripts:

`decode_full.py` → `oracle.py` → `intervals.py` → `refcmp.py`,
`tolerance.py`, `tolerance_types.py`, `replay.py`/`explain.py`,
`adversarial.py`, `rtbband.py`, `hybrid.py <loco>`, `missed.py`.

Outputs are in `results/`.
