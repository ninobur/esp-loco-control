# 70×2 diagnostic detector: replay on the Oct 27 recordings, compared with the matched filter

**Status:** replay report. No firmware, matcher or map was changed. It reports observations and their interpretation only, and recommends nothing David has not decided.

**Requested by David, 2026-10-27:**

- Replay the existing 70×2 diagnostic detector on the Oct 27 recordings, with MM037, MM113 and MM169 in particular.
- Report detection, timestamp and polarity, plus false and duplicate detections.
- Compare the result with the matched-filter recognizer.
- His clarification: evaluate the actual firmware implementation, with its baseline tracking and suppression, not a simplified threshold test. Separate genuine magnets from background disturbances.

## 1. What was replayed: the detector as compiled in NAVI_WAVEFORM_0_1

**Source.** The detector is the one that ran on the locos for these recordings, in `NAVI_WAVEFORM_0_1` (`impl/navi-waveform-recognizer-manual-20261027`, `5aca12b`). It was ported line by line from three files:

| File | What the firmware does |
|---|---|
| `NaviDiagnosticLevel.h` (baseline) | Keeps a ring of the last 1,000 values passed to `add()`. Every 100 `add()` calls it recomputes the median (`nth_element`, upper median) and holds that level between recomputes |
| `NAVI_WAVEFORM_0_1.ino` `hallTask` | Passes **every native 5 kHz raw read** to `diagLevel.add()`, not the tick median, even though the header comment says "1 s of tick medians." The level is therefore a **trailing median of the last 200 ms**, recomputed every **20 ms** (when serial % 100 == 0). `add()` runs before `onSample()` on the same read |
| `NaviCandidate.h` (onset rule and suppression) | Evaluates every 5th read (stride 5, phase serial % 5 == 0). An **onset** is the second of two consecutive evaluated reads that are each ≥ 70 counts from the level with the same sign, while no excursion is active. **Suppression:** once an onset fires, no further onset is possible until an evaluated read falls back under 70. Nothing else suppresses an onset: there is no PWM gate, no minimum spacing and no width or completion test |

The onsets are counted in `diag_onsets` (`state/loopstat`) and printed to Serial. They do not reach the matcher.

**This is not the `621a029` path.** At `621a029`, `NAVI_POWER_OF_NORMAL_MANUAL_ONLY` used the 70×2 rule against `NaviBaseline`'s frozen median, behind the PWM-90 `windowOpen` timing gate. That path is replayed separately in §8. *(Corrected 2026-10-27: an earlier version of this paragraph called `621a029` "fielded". The logs show it never ran on a loco; see §8.1.)*

**Fidelity check against the firmware's own counter.** The replay's onset total was compared with the last `diag_onsets` value the loco published for the same boot.

| Boot | Run | Replay | Firmware `diag_onsets` |
|---|---|---|---|
| CA662AD9 | Otto run 1 CCW 004-005 | 937 | **937** |
| 563C8E21 | Otto CCW 044-045 | 600 | **600** |
| 7247956A | Otto CW 040-041 | 777 | **777** |
| CC9DF08D | Otto control (Muse) MM121–039 | 344 | **344** |
| EDFAFAEA | Toby CCW 035-036 | 499 | **499** |
| 51104A27 | Toby CW 024-025 | 468 | **468** |
| 3D46D330 | Otto restart b | 151 | 149 |
| 90C05999 | Otto parked | 4 | 5 |

- **The six driven runs match exactly.** Three of those captures each have one serial gap (96–336 reads), and they still match.
- The two non-driven boots differ by 1–2. The likely cause is the capture and the last loopstat ending at different moments; this was not investigated further.
- A first version of the replay recomputed the median on the wrong phase (every 100 reads from the start of the capture, not at serial % 100 == 0). It was off by up to 72 onsets per run. That shows how much the result depends on the exact firmware detail, and why the parity check matters.

**Scripts and data:**

- Script: `research/navi_waveform_design_20261027/diag70x2_20261027/replay_70x2.py`.
- Output: `results.json`.
- Truth: the same as the bank replay (`replay_bank.truth27`): single-240 events plus oracle excursions. Each run's count equals the operator's start-to-end count.

## 2. How onsets were classified

Each onset was assigned to the magnet whose template peak is nearest in time, with d = onset − peak.

| Class | Rule | What it is physically |
|---|---|---|
| **leading** | Same polarity as the magnet, d ≤ +50 ms | The departure from background: **the detection** |
| **duplicate** | A second leading onset on the same magnet | Re-crossing while the field builds (the reading dips under 70 for one 1 ms evaluated read, then returns) |
| **trailing** | Opposite polarity, d > 0 | Caused by the magnet, but the wrong sign. The 200 ms level has already followed the magnet's field, so the return to background reads as a departure of the other sign |
| **false** | More than 0.6 s from any magnet, or any other combination | A **background disturbance** |

## 3. MM037, MM113, MM169

| Magnet | Truth (template peak, pol) | 70×2 result | Matched filter, single 240 (fielded) | Matched-filter bank 240+600 (replay only) |
|---|---|---|---|---|
| **MM169** (run 1, Otto slowed by plastic, PWM 90 commanded) | 70.313 s, N (+) | **Detected.** Onset at **70.145 s**, serial 355,865, **pol + (N)**, level 2057, PWM 90; 168 ms before the peak. **Plus 5 duplicates** (70.150, 70.197, 70.256, 70.263, 70.270 s) | Rejected (r 0.66) | Recognized (600 ms copy) |
| **MM113** (run 1 end, braking at PWM ≈ 34) | 406.885 s, N (+) | **Not detected.** No onset within ±1.5 s. Largest \|raw − level\| at an evaluated read: **57** (406.627 s), under 70 | Rejected (A 29) | Recognized |
| **MM037** (run 4 end, braking at PWM ≈ 28) | 273.445 s, S (−) | **Not detected.** No onset within ±1.5 s. Largest \|raw − level\| was **−103** (273.311 s), but never on two consecutive evaluated reads | Rejected (r −0.69, A −30) | Recognized |

**Why the slow passages are lost.**

- At braking speed the field builds over 700–800 ms.
- The 200 ms median, refreshed every 20 ms, follows the rise, so the deviation from the level stays small.
- At MM037 the field was 280 counts below background (raw minimum 1617 against roughly 1900). The detector still never saw two consecutive reads 70 below its own level.
- The same baseline tracking that makes the detector ignore slow drift makes it ignore slow magnets.

## 4. All driven runs

| Run | Truth magnets | 70×2 detected | Missed | Onsets | Leading | Duplicate | Trailing | False (moving) | False (PWM 0) | Onset before peak (median, range) | Matched filter 240 | Matched filter 240+600 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Otto run 1 CCW | 234 | 233 | MM113 | 937 | 233 | 131 | 557 | 0 | **16** | 58 ms (36–171) | 232 (missed MM169, MM113) | 234 |
| Otto CCW 044 | 179 | 178 | MM037 | 600 | 178 | 50 | 372 | 0 | 0 | 57 (35–157) | 178 (missed MM037) | 179 |
| Otto CW 040 | 180 | 180 | — | 777 | 180 | 93 | 504 | 0 | 0 | 56 (38–129) | 180 | 180 |
| Otto control MM121–039 | 90 | 90 | — | 344 | 90 | 43 | 211 | 0 | 0 | 47 (26–160) | 90 | 90 |
| Toby CCW 035 | 186 | 186 | — | 499 | 186 | 21 | 292 | 0 | 0 | 60 (40–164) | 186 | 186 |
| Toby CW 024 | 188 | 188 | — | 468 | 188 | 21 | 258 | 1* | 0 | 59 (36–169) | 188 | 188 |
| **Total** | **1,057** | **1,055** | 2 | 3,625 | 1,055 | 359 | 2,194 | 1* | 16 | | 1,054, 0 false | **1,057, 0 false** |

\* Toby CW, 288.451 s, polarity −, PWM 90. It is 52 ms after a magnet's peak, with that magnet's polarity, so it falls just outside the leading window. It belongs to that passage, not to background.

Every 70×2 polarity on a leading onset agreed with the truth polarity.

**Onsets per magnet passage.**

- Trailing onsets: Otto most often had 1–3 per passage and up to 10; Toby usually had 1.
- Duplicates: concentrated at slow and decelerating passages (PWM 41–60), with 3–6 per passage, and at PWM 90 they are 0–2.
- Overall, a passage produced **3.4 onsets** on average (3,625 / 1,057).

## 5. Genuine magnets against background disturbances

| | 70×2 (diagnostic) | Matched filter 240 (fielded) |
|---|---|---|
| Onsets/events while moving, not caused by a magnet | **0** in all six driven runs | **0** |
| Onsets/events while stationary (PWM 0), driven runs | **16**: Otto run 1, 2–29 s before the first throttle; all +, level 2076–2121 | **0** |
| Parked boots (replay): Otto 90C0 / restart b 3D46 / old-build AFAD | 4 / 5 / 27 | 0 / 0 / 2 |
| Toby parked boots | 0 | 0 |
| Magnet-caused onsets that are not the detection | **2,553** (359 duplicates + 2,194 trailing of the wrong sign) | 0 (one event per passage, by NMS) |

**Interpretation.**

1. **Moving background on these runs was quiet.** Neither detector reported anything while moving that was not caused by a magnet. On this evidence, the matched filter adds no background rejection while moving that 70×2 lacked.
2. **Stationary background differs.**
   - Parked Otto produced 70×2 onsets that the matched filter ignored: 16 in run 1 before the first throttle, plus 4 and 5 in two parked boots.
   - These are disturbances (Otto was being handled when parked, as David confirmed) that cross 70 counts but do not have a magnet's shape.
   - On the old-build parked capture both detectors fired (27 against 2).
3. **What the waveform recognizer contributes beyond 70×2 on this data:**
   - **One observation per passage, with its polarity, instead of 3.4 onsets** of which about 2 have the wrong sign. The raw 70×2 stream cannot be passed to a next-magnet rule without an interpretation stage. The `621a029` build had one (`NaviEncounter`); the diagnostic does not.
   - **Shape rejection** of stationary handling disturbances.
   - **Slow passages.** The 240+600 bank recognizes MM037 and MM113, which 70×2 misses because its baseline tracks the slow rise. The single-240 template, as fielded, misses all three. 70×2 catches MM169 (400 ms wide), where single-240 does not.
4. **What 70×2 does better:**
   - **Timing.** The onset comes on average 58 ms *before* the template peak. The matched filter declares 359 ms *after* the peak (240 ms copy), and 893 ms after it for the 600 ms copy.
   - That is roughly 0.4–0.95 s earlier. What that is worth for stopping is not established here.

## 6. Not covered

- **The `621a029` 70×2 path.** Replayed afterwards, in §8. *(Corrected 2026-10-27: this item called it "fielded", and said its parity could be checked on its own Oct 25–26 recordings. Neither is true; see §8.1.)*
- The replay level warms up on the first 100 recorded reads, not on boot data. No onset fell in the first 200 ms of any capture.
- The classification windows (+50 ms, 0.6 s) were chosen after looking at the onset-time distribution. They change how onsets are labelled (leading, duplicate, trailing), not the onset counts, the missed passages or the parity.

## 7. David's three questions, and the fourth possibility

These answers cover the Oct 27 driven runs only: 1,057 magnet passages, Otto and Toby, both directions.

### 7.1 Sensitivity: which detector recognizes more genuine magnets?

| Detector | Recognized | Missed |
|---|---|---|
| 70×2 diagnostic | 1,055 | MM113, MM037 (both braking, PWM ≈ 28–34, 700–790 ms wide) |
| Matched filter, single 240 (as fielded) | 1,054 | MM169, MM113, MM037 |
| Matched-filter bank 240+600 (replay only) | **1,057** | none |

- At cruise, all three detectors recognize every magnet.
- **During braking**, both 70×2 and single-240 miss the two end-of-run magnets. 70×2 misses them because its 200 ms level follows the slow rise (§3); single-240 misses them on r or A.
- **For the obstructed passage at MM169** (400 ms wide), 70×2 is better than single-240.
- Only the two-width bank recognizes all three.

### 7.2 Specificity: which detector is better at rejecting disturbances?

| | 70×2 | Matched filter (240 or 240+600) |
|---|---|---|
| Moving, not caused by a magnet | 0 | 0 |
| Stationary (handling), driven runs | 16 | 0 |
| Parked boots, Otto | 4 + 5 | 0 + 0 |
| Wrong-sign trailing onsets caused by the magnet itself | 2,194 | 0 |
| Extra same-sign onsets on one passage | 359 | 0 |

- **The matched filter is clearly more specific.** While moving on these runs, though, there was no background disturbance for either detector to reject.
- The difference lies in two places:
  - **Stationary handling.**
  - **70×2's own response to each magnet:** about 2.4 surplus onsets per passage, two of them of the wrong polarity on average.

### 7.3 Timeliness: how much sooner does 70×2 recognize an encounter?

**Measured.** For every passage both detectors recognized, the matched-filter declaration time was compared with the 70×2 leading onset:

| Matched-filter copy | Declaration after the 70×2 onset |
|---|---|
| 240 ms | Median 406–419 ms per run; range 385–530 ms |
| 600 ms (the only copy that recognized MM169 in the bank) | 1,081 ms |

Where the gap comes from:

- The onset leads the template peak by a median of 58 ms.
- The 240 ms copy declares 359 ms after the peak; the 600 ms copy declares 893 ms after it.

**70×2 is about 0.4 s sooner at cruise, and about 1 s sooner on a 600 ms passage.** The distance this represents at PWM 90 was not measured here.

### 7.4 The fourth possibility: 70×2 observes immediately, the matched filter confirms

**Test.** For each 70×2 onset, look for a matched-filter event of the **same polarity** whose template peak falls between 0.15 s before and 0.6 s after the onset. Every leading onset's own peak fell inside that window. Script: `complement_70x2_mf.py`; output in `complement.json`.

| 70×2 onsets | Count | Confirmed, single 240 | Confirmed, bank 240+600 |
|---|---|---|---|
| Leading (the genuine detection) | 1,055 | 1,054 (MM169 not) | **1,055** |
| Duplicates (same sign, same passage) | 359 | 354 | 359, by the **same** event as the leading onset |
| Trailing (wrong sign) | 2,194 | **0** | **0** |
| Stationary handling | 16 | **0** | **0** |
| Other (Toby, 288.451 s, a late same-sign onset on a passage) | 1 | 1 | 1 |
| Matched-filter events with **no** 70×2 onset | — | 0 | 2 (MM037, MM113, the 600 ms copy) |

**What the replay shows:**

1. **Confirmation separates the two kinds of onset cleanly on this data.**
   - Every genuine detection was confirmed, by the bank.
   - No wrong-sign trailing onset and no stationary onset was confirmed.
   - Duplicates were confirmed by the same event as their leading onset, so they collapse into one observation, provided "one event confirms one observation" is a rule.
2. **The confirmation arrives about 0.4 s after the onset at cruise, and about 1.1 s after it at MM169.** In that interval the 70×2 stream has usually already produced the passage's wrong-sign trailing onsets, which arrive 70–90 ms after the peak. An immediate-observation design would therefore treat every onset as provisional until confirmed. Otherwise it reports, for about 0.3 s, a second encounter of the wrong polarity.
3. **The matched filter is not only a confirmer.** At the two braking magnets it recognized passages that 70×2 never saw. In a combined arrangement, the matched filter would also have to be able to originate an observation, not only confirm one.
4. **The two mechanisms are not fully independent.**
   - Both read the same Hall sensor and the same samples.
   - Their independence is in method: a level threshold against a trailing median, versus a shape correlation.
   - A disturbance that both crosses 70 and has a magnet's shape would pass both. None occurred on these runs.

**What this does not establish:**

- **Whether 0.4 s earlier matters on the railway.** It would matter only if something acts on the provisional observation; under the October 21 hold rule nothing does.
- **Behaviour on the `621a029` path** (§6).
- **Behaviour on a run with moving background disturbances.** These runs had none.

**This is analysis, not a proposal.** The arrangement, and whether to pursue it at all, is David's decision.

## 8. The `621a029` 70×2 path (added 2026-10-27, at David's request)

### 8.1 First, a correction: `621a029` never ran on a loco

- **Commit date.** `621a029` ("freeze baseline at timing gate and qualify MM directly at Hall opening") was committed on **Oct 26 at 20:30**.
- **No log shows it.** At `621a029`, `state/nav` publishes `timing_lower_ms` and `timing_calibration_ready`. I searched every MQTT log in `~/Pi_Log_Downloads` from Oct 24–27. Those fields appear **only** in the Oct 27 afternoon NAVI_WAVEFORM_0_1 boots, which inherited the publisher.
- **The Oct 27 morning Toby run (`96348B2C`) was an earlier commit of the same sketch.** Its evidence:
  - `state/nav` has no `timing_lower_ms`;
  - its encounter rows carry `width_blocks` and `peak_adc`, from the return-to-baseline `NaviEncounter`, which `621a029` no longer fills;
  - it confirmed openings at PWM 29–88, which `621a029`'s gate would refuse.
- **Telemetry cannot tell the commits apart.** `state/bootid` names only the sketch, and every commit of this sketch has the same name.
- **This corrects my own records:**
  - the design document's line "Both locos ran it on Oct 25–26";
  - this report's earlier word "fielded".
- **Consequence.** There is no firmware output for `621a029` to check this replay against. The replay implements the code as written.

### 8.2 What was replayed

- **Script:** `replay_621a029.py`, with its output and `replay_621a029.json`.
- **Ported from `621a029`:** `NaviObservation.h` (20 ms blocks), `NaviBaseline.h`, `NaviCandidate.h`, `NaviTarget.h` (`windowOpen`, `onOnset`), `NaviTimingProfiles.h` (lower factors and survey bases), `RouteMap.h`, and the `rvmTask` / `hallTask` ordering.
- **Source check:** the copies used are byte-identical to `621a029`.
- **Declarations** come from each capture's command records:
  - `session_direction`, then `start_interval`;
  - a declaration is refused without a session direction, as `admitStartMarker` does. That applies to the Muse control run, whose interval came first.
- **Two approximations** (stated in the script):
  - the median buffer's order after `nth_element`;
  - gate changes take effect at 20 ms block boundaries.
- Neither approximation affects the result below. The result is decided by a PWM comparison that is wrong by 30–50 PWM units, not by a few milliseconds or counts.

**`NaviEncounter` is compiled into `621a029` but not used.** Judgement happens at the 70×2 onset (`judgeOnset` → `NaviTarget::onOnset`). The only suppression is the gate:

- 70×2 runs only while the gate is open;
- the detector is reset on every read while the gate is shut;
- a confirmation shuts the gate, and drops onsets still in the queue.

### 8.3 Result: one magnet per run, then the gate never reopens

| Run | Declared | Gate open | Judged onsets | Result | Magnets passed |
|---|---|---|---|---|---|
| Otto run 1 CCW | 004-005 | 41.95–58.69 s | 1 | MM004 confirmed (correct, pol S), onset 58.684 s at **PWM 59** | 234 |
| Otto CCW 044 | 044-045 | 21.60–36.94 s | 1 | MM044 confirmed (correct, N), 36.937 s at **PWM 53** | 179 |
| Otto CW 040 | 040-041 | 11.91–31.83 s | 1 (2 more onsets dropped from the queue) | MM041 confirmed (correct, S), 31.816 s at **PWM 42** | 180 |
| Otto control MM121–039 | refused (interval before session direction) | never | 0 | — | 90 |
| Toby CCW 035 | 035-036 | 78.15–98.25 s | 1 | MM035 confirmed (correct, N), 98.242 s at **PWM 57** | 186 |
| Toby CW 024 | 024-025 | 71.53–83.09 s | 1 | MM025 confirmed (correct, N), 83.072 s at **PWM 41** | 188 |

**Why the gate stays shut.**

1. The first expected magnet after a declaration is judged with no timing anchor, so it needs polarity only. In every run it was passed while Otto or Toby was still accelerating, at PWM 41–59, and it was confirmed correctly.
2. That onset becomes the anchor, with its PWM.
3. `NaviTarget::windowOpen()` returns false whenever `confirmedOpenPwm_ != 90`. The gate therefore never opens again, and nothing in the path re-declares.
4. **This is decided by the code, not by the signal.** It does not depend on the replay's approximations.
5. Under these driving conditions, `621a029` would have recognized 5 of 1,057 magnets on the Oct 27 runs.

**MM037, MM113, MM169:** none was judged; the gate was shut at each of them. This replay therefore cannot say how `621a029`'s frozen-median 70×2 rule itself would have handled those passages.

### 8.4 What this adds to the comparison

- **Sensitivity.** The as-coded `621a029` 70×2 path is limited by its gate, not by its detector. On these runs it is far below the diagnostic 70×2 (1,055) and both matched-filter configurations (1,054 and 1,057).
- **Specificity.** While its gate was open it produced no false or nonqualifying judgements: 5 judged, 5 correct. It can only be judged over 83.6 s of open gate before a first magnet, almost all of it with the loco stationary or accelerating.
- **Timeliness.** Its onset times are those of the 70×2 rule: 155–233 ms before the template peak on the five passages. These were slow, accelerating passages, which is why the lead is longer than the diagnostic's cruise median of 58 ms.
- **The frozen-median 70×2 rule is untested here.** That rule is the part of `621a029` that might have been worth comparing with the diagnostic's 200 ms trailing median. Testing it would need either:
  - a replay on a run whose first magnet is passed at PWM 90; or
  - a replay with the PWM-90 anchor condition set aside.
- The second option is a change to the path, not a replay of it. Both are David's decision.
