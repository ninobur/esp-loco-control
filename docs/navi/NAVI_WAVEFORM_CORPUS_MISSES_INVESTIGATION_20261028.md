# Investigation: Oct 25–26 passages the multiwidth recognizer misses at every width (2026-10-28)

**Status:** investigation only. No firmware or governing specification was changed. The assessment in §6 is a recommendation; the decisions are David's.

**Request (David, 2026-10-28):**

- Investigate individually the Oct 25–26 genuine passages that remain unrecognized at every tested width.
- Give each one's loco, recording, location, PWM and conditions; its waveform against the local baseline; its best r, A and width; the rejection mechanism; and the 70×2 evidence.
- Find common causes, separating ordinary-operation risks from historical disturbances.
- Assess readiness for controlled testing in normal automatic station sequences.

## Method

- **Corpus:** the Oct 25–26 held-out corpus, 6,027 reference magnets (Otto Oct 26, Toby Oct 25–26).
- **Which misses are investigated:** a reference magnet is investigated when all four tested configurations miss it.

  | Configuration | Thresholds |
  |---|---|
  | 240 (as fielded) | 0.7 |
  | 240+600+1500 | wide copies at 0.9 |
  | 150+240+600+1500 | 0.7 |
  | 150+240+600+1500 | 0.9 |

- **Result: 19 such passages.** The "about 22" in the previous report was the per-configuration count of 21–26. 19 are common to all four.
- **Scripts** in `research/navi_waveform_design_20261027/corpus_misses_20261028/`, each with its output:
  - `investigate_misses.py`, writing `misses.json`: per-passage evidence;
  - `same_magnet_other_laps.py`: the same magnet on the loco's other laps;
  - `background_levels.py`: background level over the disturbed sessions.
- **Reference.** The research reference (`truth.py`) places template-independent Hall excursions on RouteMap.h:
  - an **anchor** is a clean excursion placed on the route;
  - an **expected** magnet is interpolated where a chain skipped one.

  It is evidence about which magnet produced an excursion, not physical ground truth.
- **70×2 evidence** comes from two sources:
  - **logged:** the loco's own `diag/navi_encounter` rows from the Oct 25–26 builds, which ran a 70×2 detection path. Their opening time was compared with the reference.
  - **replay:** NAVI_WAVEFORM_0_1's diagnostic 70×2, firmware-exact, run on the capture.

## 1. The 19 passages

| Session | Loco | MM | Kind | t (s) | PWM, phase | Excursion amp / width (ms) | Best r, A by width: 150 · 240 · 600 · 1500 | Mechanism | 70×2 logged by loco | 70×2 replay |
|---|---|---|---|---|---|---|---|---|---|---|
| `278F53E8` | Otto | 011 N | anchor | 94.39 | 71 steady | 256 / 680 | 0.53, 38 · — · 0.79, 247 · 0.88, 219 | event 0.18 s early, scored false | N CONFIRMED, −0.55 s | onset −0.54 s |
| `278F53E8` | Otto | 016 S | anchor | 104.46 | 71 steady | 339 / 516 | — · — · 0.89, 421 · — | events of both signs nearby, scored false | S CONFIRMED, −0.46 s | onset −0.11 s |
| `278F53E8` | Otto | 017 N | anchor | 105.36 | 71 steady | 386 / 916 | 0.61, 28 · −0.70, −38 · −0.86, −416 · 0.68, 344 | below threshold (best fit of the wrong sign) | none | onset −0.54 s |
| `278F53E8` | Otto | 018 S | anchor | 109.87 | 71 steady | 385 / 390 | −0.88, −32 · −0.75, −26 · −0.98, −418 · — | 240 event 64 ms early, scored false | none | onset −0.22 s |
| `F420A05D` | Otto | 119 N | anchor | 34.65 | **0** | 174 / 528 | 0.75, 47 · −0.72, −75 · 0.81, 126 · 0.82, 84 | 600 event 0.19 s late, scored false | N NONQUALIFYING, 0.00 s | onset 0.00 s |
| `F420A05D` | Otto | 120 N | anchor | 41.28 | 19 accelerating | 188 / 574 | 0.74, 32 · 0.59, 24 · 0.71, 134 · 0.82, 137 | below threshold | N NONQUALIFYING ×2 | onset −0.01 s |
| `F420A05D` | Otto | 131 N | anchor | 58.67 | 90 | 138 / **44** (an S excursion 0.25 s before it) | −0.94, −151 · −0.99, −154 · 0.87, 100 · — | the strong fit is the preceding S excursion (given to MM130) | S CONFIRMED, −0.31 s | onset −0.16 s |
| `F420A05D` | Otto | 061 N | anchor | 70.08 | 90 | **74** / 840 (an S excursion 0.16 s after) | −0.91, −136 · −0.90, −130 · — · — | below threshold (only the wrong sign fits) | none | none |
| `F420A05D` | Otto | 065 N | anchor | 74.97 | 90 | 106 / 108 (second N excursion 0.25 s after) | −0.92, −120 · −0.93, −117 · 0.74, 97 · — | 150 event at the second excursion, scored false | none | onset 0.00 s |
| `F420A05D` | Otto | 160 N | expected | 95.00 | 90 | **none** | — · 0.69, 31 · — · 0.52, 98 | no signal | none | onset −0.45 s |
| `F420A05D` | Otto | 161 S | expected | 96.56 | 90 | **none** | — · — · — · 0.52, 98 | no signal | none | none |
| `22BBF8FC` | Toby | 065 N | anchor | 1,383.34 | 91 | **69** / 74 | 0.76, 36 · 0.81, 32 · — · — | below threshold (amplitude) | none | onset 0.00 s |
| `22BBF8FC` | Toby | 168 N | anchor | 1,535.94 | 91 | 107 / **40** | — · 0.54, 27 · — · 0.54, 72 | below threshold | none | onset −0.06 s |
| `8CFE3CE1` | Toby | 167 N | anchor | 815.82 | 90 | 192 / 128 | — · — · — · — | **capture gap** (192 reads lost 76 ms before) | **N CONFIRMED, −0.07 s** | onset −0.04 s |
| `8CFE3CE1` | Toby | 056 N | anchor | 895.82 | 90 | 182 / 52 | — · — · — · — | **capture gap** (3 gaps, 300 ms missing) | **N CONFIRMED, −0.07 s** | onset 0.00 s |
| `EC79FDC0` | Toby | 157 N | anchor | 177.18 | 90 | 212 / 170 | — · — · — · — | **capture gap** (338 ms missing) | **N CONFIRMED, −0.07 s** | onset −0.07 s |
| `EC79FDC0` | Toby | 168 N | anchor | 192.47 | 90 | 237 / 164 | — · — · — · — | **capture gap** (222 ms missing) | **N CONFIRMED, −0.07 s** | onset −0.07 s |
| `EC79FDC0` | Toby | 169 N | anchor | 193.96 | 90 | 200 / 48 | — · — · — · — | **capture gap** (232 ms missing) | **N CONFIRMED, −0.07 s** | onset −0.01 s |
| `EC79FDC0` | Toby | 005 N | anchor | 203.90 | 90 | 264 / 74 | — · — · — · — | **capture gap** (192 reads lost at the passage) | **N CONFIRMED, −0.06 s** | onset −0.06 s |

**How to read the table:**

- "—" means no candidate at that width at the research floors (|r| ≥ 0.5, |A| ≥ 20).
- No rejection was caused by duplicate suppression in the combining rule ("suppressed": 0).

## 2. Common causes

### A. Otto, two disturbed recordings on Oct 26: 11 passages

**`F420A05D` (7 misses).**

- All seven lie between **34 and 97 s** of a 2,287 s session.
- In that window the moving background wandered between 1,947 and 2,231. While parked, at 10–30 s, it ranged 1,987–2,185 (`background_levels_output.txt`).
- From about 130 s it settled to 2,034–2,056, within ±3 counts per 10 s.
- **Every later lap of the same seven magnets in that session was recognized:** 10 or 11 of each, amplitudes 134–189, widths 140–250 ms.
- The misses show the disturbance's signatures:
  - an excursion of the wrong sign next to the magnet (MM061, MM131);
  - a doubled excursion (MM065);
  - nothing at all where two magnets should be (MM160, MM161);
  - a magnet passed while stationary or creeping (MM119 at PWM 0, MM120 at PWM 19), with the background moving under it.

**`278F53E8` (4 misses).** This is a 128 s session at PWM 71.

- While stationary the background was steady at 1,971 ±3.
- Once moving, 5–95% of the background spread from **1,595 to 2,403**, beyond anything Otto's magnets produce (raw about 1,777–2,112).
- The four "passages" are 390–916 ms wide with amplitudes **256–386**. The same magnets give 181–204 on Otto's other laps, at PWM 56–90.
- The same session holds 3 of the corpus's 8 false detections.

**What this group is:**

- These are sensor-path disturbances on Otto, not a recognizer limitation. The same magnets are recognized on every undisturbed lap.
- **Correction.** In `NAVI_WAVEFORM_ONE_SHAPE_ALL_WIDTHS_20261028.md` I described these two sessions as "later found to be wiring". That is not established:
  - the wiring adjustment that cleared Otto's Hall background steps was made on **Oct 25 at 20:07**;
  - both sessions were recorded on **Oct 26**, at 11:46 and 13:01, after it;
  - Otto's Oct 27 evening run also contained two rail-hitting transients (`NAVI_WAVEFORM_0_1_FIELD_RUN_20261027_EVENING.md` §5).

  So Otto's sensor-path disturbances are **not shown to be resolved**. Their cause on Oct 26 is undetermined.

### B. Toby, recording losses: 6 passages

**What happened:**

- In `8CFE3CE1` and `EC79FDC0` the capture lost reads (serial gaps of 48–720 reads) inside the passages' template windows.
- With part of the window missing, no width produces a score at those passages.
- The magnets themselves were normal: amplitude 182–264. Toby recognized them on every other lap (20 of 21 for MM157, MM167 and MM168; 34 of 35 for MM056).

**The loco itself saw them.** Toby's own logged 70×2 encounter **confirmed each of the six, with the correct polarity**, 60–71 ms before the reference peak.

**What this group is:**

- These are **recording artifacts**. On the loco, the recognizer reads every sample, not the capture.
- They say nothing about the recognizer.
- They do mean an offline replay of a capture with losses will understate the recognizer.

### C. Toby, a reference-labelling artifact: 1 passage

**`22BBF8FC`, MM168 at 1,535.94 s.**

- The reference labels two excursions MM168:
  - the genuine passage at 1,535.17 s (amplitude 238, 240 ms), which was **recognized**;
  - a **40 ms** spike of 107 counts, 0.76 s later.
- Nearby there are other 6–18 ms spikes, at 1,535.88, 1,537.68 and 1,538.75 s.
- At PWM 91 a passage of Toby is 160–240 ms wide. The spike is not a magnet passage, and the recognizer was right to ignore it.

### D. Toby, the MM065–067 stretch on Oct 26: 1 passage

**`22BBF8FC`, MM065 at 1,383.34 s (Oct 26, 10:25).**

- Its amplitude was **69**, against 181–188 on Toby's eight other anchored laps of MM065.
- In the two later Toby sessions that day, `8CFE3CE1` (12:26) and `E7C33809` (12:52), MM065, MM066 and MM067 produced **no excursion at all** on 10 of 11 passes.
- David moved the magnets in this stretch (MM066/067/069) on the morning of Oct 27.
- On Toby's Oct 27 morning run, MM065 was back to 181 on both passes.
- **What this group is:** a physical magnet condition that has since been corrected. The recognizer rejected it on amplitude (A 32), which matches a genuinely weak signal.

## 3. Ordinary operation against historical conditions

| Cause | Passages | Could it affect ordinary operation now? |
|---|---|---|
| A. Otto sensor-path disturbance (Oct 26) | 11 | **Possibly.** Otto's disturbances are not shown to be fixed: Oct 26 sessions after the Oct 25 wiring tweak, and two transients on Oct 27 evening. This is a locomotive hardware question, not a recognizer one |
| B. Capture losses | 6 | No for the loco's recognition. Yes for offline evaluation: captures need to be checked for gaps |
| C. Reference artifact | 1 | No. Not a passage |
| D. MM065–067 magnets on Oct 26 | 1 | No. Corrected on Oct 27 |
| A systematic weakness of the multiwidth recognizer | **0** | None of the 19 is a normal passage that the recognizer failed to score |

**The recognizer weaknesses that do exist are not in this corpus.** They are in the Oct 27 recordings:

- **slow passages below about PWM 45**, which the 600 and 1,500 ms copies recover;
- **very fast passages near PWM 190**, which a 150 ms copy recovers at a false-detection cost.

## 4. The 70×2 as independent evidence

**Where the loco's logged 70×2 is useful (group B).** For all six recording-loss passages, the loco's own 70×2 encounter confirms that a passage of the right polarity occurred, 60–71 ms before the reference peak. That evidence comes from the loco's complete sample stream, not from the capture.

**Where it is not useful (group A).**

- In the disturbed windows the logged 70×2 recorded encounters that do not fit the reference:
  - MM131 was logged as an **S** encounter, the preceding wrong-sign excursion;
  - MM011 and MM016 were logged about 0.5 s early.
- The replayed 70×2 produced a same-polarity onset at 16 of the 19 positions. In disturbed recordings, however, it also fires on the disturbance itself, as it does on the Oct 27 evening transients.
- **There, an onset is not evidence that a magnet was present.**

**Groups C and D.** The replayed 70×2 fired on the 40 ms spike (C) and on the weak MM065 (D). In C it supports the wrong conclusion. In D it adds nothing the amplitude did not already show.

## 5. What remains unexplained

- **The cause of Otto's Oct 26 disturbances**, and whether they still occur.
- **Why the Oct 25 wiring adjustment did not prevent them.**

## 6. Assessment: readiness for controlled testing in normal automatic station sequences

**Recognition itself:**

- The corpus misses give no reason to doubt the multiwidth recognizer on a healthy loco.
- In the Oct 27 runs the 240+600+1500 bank, with the wide copies at 0.9, recognized:
  - every slow passage, at PWM 21–45, the station-approach range;
  - every passage after a restart;
  - with no false detection in any driven run.

  On Otto's evening run that is 524 of 527 (the 2 fast passages and MM057 missed); in the afternoon runs, 1,057 of 1,057.

**My assessment (a recommendation, not a decision):** the multiwidth recognizer is ready for **controlled, supervised** testing in station sequences **as an observation source**. Five issues should be settled before its declarations are given stopping authority, and the first is a reason to begin on Toby:

1. **Otto's sensor path (group A).**
   - Disturbances recurred after the Oct 25 wiring tweak and produce both misses and false detections at every width.
   - Testing on Otto is premature unless the resting background is checked first (about 1,936 and steady) and disturbances are watched for during the test.
   - Toby showed no comparable disturbance in this corpus. Its only disturbances were brief spikes and the recording losses.
2. **Declaration latency of the wide copies.** The 600 ms copy declares about 0.9 s after the passage and the 1,500 ms copy about 2.2 s after (bank replay §3.5). At station approach speed the loco travels on in that time. Any station logic that stops relative to a magnet must allow for it. How the existing station logic would use a late declaration has not been examined.
3. **The hold rule's recovery.** One miss or one false detection still displaces the October 21 hold rule for the rest of the segment (seen on both Oct 27 Otto runs). The experimental map-aware matcher (design §4.5) is not in any build. A station test will show this the first time anything is missed.
4. **Stopping on a magnet, then restarting from inside its field.** This has not happened in any recording (`NAVI_WAVEFORM_0_1_FIELD_RUN_20261027_EVENING.md` §4). It is exactly the station case. It should be part of the controlled test, as an observation.
5. **Passages below about PWM 20, and the final metres of the braking ramp.** Only one passage below PWM 25 exists in our recordings (MM035 at 21, recovered by the 1,500 ms copy).

**Not blocking for station sequences:** the 150 ms copy and the very fast passages. Station sequences do not run near PWM 190.
