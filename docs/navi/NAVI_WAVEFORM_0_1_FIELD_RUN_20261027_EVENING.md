# NAVI_WAVEFORM_0_1, Oct 27 evening: when does the waveform recognizer fail?

**Status:** field record. It reports observations and their interpretation only. It changes no firmware, matcher or map, and the diagnostic comparisons in §6 are not proposals.

## The run

**David's description:** a bigger sample; no hand manipulation except at reset; a variety of speeds, with acceleration, deceleration, stops and restarts.

| Item | Value |
|---|---|
| Loco, build | Otto, NAVI_WAVEFORM_0_1. `state/bootid` recognizer `NAVI_WAVEFORM_MATCHED_FILTER_SINGLE240`; sketch name still `NAVI_POWER_OF_NORMAL_MANUAL_ONLY` |
| Boot | `044A3972942824CE`, 20:45:20 → 21:02:54 |
| Capture | `navi_session_9950011_044A3972942824CE_D594CA94_20261027_204520.nsr` (SHA-256 `1ac94de315b8f01c…`): 1,031.9 s, 0 bad records, 3 serial gaps |
| MQTT log | `9950011_20261027_204520.log` (`07587f722687625a…`) |
| Declarations | Session CCW. `048-049` at 48.2 s; reset `047-048` at 491.7 s; reset `053-054` at 924.1 s. No `direction` commands |
| Speed | Applied PWM 0–200; 10 standstills of ≥ 1 s, each followed by a restart (§4) |
| Battery | `INA219 NOT FOUND — no battery protection this session` warning at boot |

**Scripts** in `research/navi_waveform_design_20261027/field_20261027_evening/`, each with its output:

- `when_it_fails.py` and `results.json`: truth, misses and passage descriptions;
- `bands.py`: recognition by speed, phase and restart;
- `standstills.py`: where each stop left the sensor;
- `bank_check.py`: the diagnostic comparison in §6.

## 1. Firmware = replay

- **The on-loco events equal the Python replay:** 514 events against 514, none on one side only.
- **Passage times agree** within 19 ms (median 0.7 ms).
- **Consequence:** the analysis below, done on the replay, describes what the loco did.

## 2. How the true magnet passages were established

Truth comes from two independent sources, which had to agree:

- **The map.** Within each declared segment, the recognized polarities were aligned with RouteMap.h in the CCW direction from the declared start.
- **The oracle.** A Hall excursion detector that does not use the template. Its excursions with no recognizer event within 0.3 s were allowed into the alignment as possible magnets.

Alignment costs:

| Case | Cost |
|---|---|
| A map magnet with nothing matched to it | 1 |
| A recognizer event that fits no map magnet | 1 |
| An oracle excursion | 0, whether matched or left out |

**Result: total cost 0 in all three segments.**

- Every one of the 514 recognizer events falls on a map magnet with the map's polarity, so there were **no false recognitions**.
- Every magnet the segments span is accounted for. The **13 recognizer misses** are each an oracle excursion of the right polarity at the right place in the sequence.
- In total: **527 magnet passages, 514 recognized (97.5%)**.

**The 17 oracle excursions that fit no magnet** were left out. They are:

- stationary noise at the start of the capture, at 15.6–16.2 s;
- small (60–88 count) excursions while moving;
- two large hardware transients (§5).

## 3. The 13 misses

| MM | Time (s) | PWM | Phase | Width (ms) | Amplitude | 240 ms copy: r, A | Why rejected |
|---|---|---|---|---|---|---|---|
| 003 (S) | 135.63 | 31 | decelerating | 1,088 | 242 | −0.69, −43 | r and A |
| 146 (N) | 196.49 | 33 | decelerating | 866 | 178 | 0.71, 54 | A |
| 070 (S) | 282.98 | **187** | accelerating | **52** | 130 | −0.75, −67 | A |
| 039 (S) | 297.00 | **189** | decelerating | **50** | 169 | −0.69, −86 | r |
| 057 (S) | 427.84 | 134 | steady | 54 | — | −0.53, −411 | r. Passage overlaid by a hardware transient (§5) |
| 112 (N) | 713.26 | 40 | steady | 754 | 186 | 0.67, 54 | r and A |
| 111 (N) | 719.00 | 35 | decelerating | 764 | 193 | 0.68, 55 | r and A |
| 058 (S) | 835.00 | 40 | steady | 816 | 190 | −0.71, −53 | A |
| 056 (N) | 844.57 | 36 | decelerating | 704 | 186 | 0.70, 62 | A |
| 055 (S) | 852.09 | 26 | decelerating | 1,296 | 183 | −0.69, −23 | r and A |
| 054 (S) | 907.72 | 30 | accelerating, **4.5 s after a restart** | 1,076 | 186 | −0.70, −30 | A |
| 048 (N) | 957.60 | 32 | decelerating | 848 | 183 | 0.69, 45 | r and A |
| 035 (N) | 1,020.68 | 21 | decelerating | 1,382 | 182 | — (no candidate) | — |

- **The slow misses' magnets were normal.** Their amplitudes (178–242) are ordinary for these magnets.
- **Their shape was ordinary too.** The same excursion stretched wider correlates at r 0.95–1.00 with the 1.5 s copy of the same template.
- **What failed was width.** A 700–1,400 ms passage gives the 240 ms template only r about 0.7 and a fitted amplitude of 23–62.

## 4. When it fails: by speed, phase and restart

| Applied PWM | Passages | Recognized | Missed |
|---|---|---|---|
| < 30 | 2 | 0 | MM055 (26), MM035 (21) |
| 30–34 | 5 | 1 (MM036 at 32) | MM003, MM146, MM054, MM048 |
| 35–39 | 2 | 0 | MM111, MM056 |
| 40–44 | 15 | 13 | MM112, MM058 (both at 40) |
| **45–59** | 32 | **32** | — |
| **60–90** | 194 | **194** | — |
| **91–119** | 72 | **72** | — |
| 120–149 | 68 | 67 | MM057 (134, transient) |
| **150–174** | 90 | **90** | — |
| 175–200 | 47 | 45 | MM070 (187), MM039 (189) |

**By phase:**

| Phase | Recognized |
|---|---|
| Accelerating | 95 / 97 |
| Steady | 220 / 223 |
| Decelerating | 199 / 207 |

**Restarts.** No standstill left the sensor inside a magnet's field: the Hall level during every stop equals the moving background, within 2 counts in 9 of the 10 stops and 31 in the other. For the first magnet after each restart:

| Restart at | First magnet | PWM | Result |
|---|---|---|---|
| 66.5 s | MM048 | 55 | recognized |
| 225.2 s | MM145 | 42 | recognized |
| 511.6 s | MM047 | 44 | recognized |
| 752.8 s | MM110 | 48 | recognized |
| 907.7 s | MM054 | 30 | **missed** |
| 942.4 s | MM053 | 44 | recognized |
| 986.9 s | MM047 | 49 | recognized |

The one missed restart was a slow creep: three moves at a maximum PWM of 7, 38 and 39 between 876 and 915 s.

**Answer to "when does it fail":**

1. **Slow passages, below about PWM 45.**
   - Every passage at PWM 45–174 was recognized: 455 of 455, leaving out the transient-overlaid MM057.
   - Below PWM 40, 1 of 9 was recognized.
   - At PWM 40–44, 13 of 15 were recognized.
   - The failing passages were 700–1,400 ms wide. This is the same failure as MM037 and MM113 this afternoon, now with ten examples.
2. **Restarting is not a failure by itself.**
   - 6 of 7 first-after-restart magnets were recognized.
   - The one miss was passed at PWM 30, where steady and decelerating passages fail too.
   - Whether a restart is recognized depends on how fast the loco is moving when it reaches the first magnet.
3. **Very fast passages, near PWM 190.**
   - 2 of 47 passages at PWM 175–200 were missed (MM070, MM039). Both were about 50 ms wide, with magnets about 0.46 s apart.
   - The 240 ms template is too wide for them: r 0.69–0.75, A 67–86, at the edge of both thresholds.
   - At PWM 150–174 nothing was missed.
   - This failure was not visible before. No earlier run went above PWM 150 for long.
4. **No false recognitions anywhere.** That includes 10 standstills, two large hardware transients, and the stationary noise at the start.

## 5. Two hardware transients

At both transients the Hall reading went far outside anything a magnet produces on Otto, whose magnets stay inside raw 1777–2112:

| Time | PWM | Raw readings | Duration | Overlapped a passage? |
|---|---|---|---|---|
| 427.770–427.842 s | 134 | 4012 down to 721 | 72 ms | Yes: MM057's, which was lost (§3) |
| 443.056–443.112 s | 0 (standing) | 4095 (rail) down to 1121 | 56 ms | No |

- **Neither produced a recognition.**
- **The 443 s event happened while the loco was standing**, with no command or declaration near it. David reported no hand manipulation except at reset (491.7 s). The cause is not determined.

## 6. Diagnostic only: other template widths on this run

This uses the bank combining rule from the Oct 27 bank replay. It is reported for comparison and decides nothing.

| Widths | Events | Map passages recognized | Missed | Events off the map |
|---|---|---|---|---|
| 240 (as fielded) | 514 | 514 / 527 | 13 | 0 |
| 240+600 | 524 | 524 / 527 | MM070, MM039, MM057 | 0 |
| 150+240 | 518 | 517 / 527 | the 10 slow ones | 0 (1 duplicate on a passage) |
| 150+240+600 | 528 | 527 / 527 | — | 0 (1 duplicate on a passage) |

**What the table shows:**

- **The 600 ms copy recovers all ten slow misses:** r 0.72–0.91, A 99–187.
- **A 150 ms copy scores the two fast ones well:** r −0.92 / −0.89, A −101 / −139.
- **MM057's recovery by the 150 ms copy rests on the transient-overlaid signal** (A −705). It is not evidence about fast passages.
- **The 150 ms copy has not been evaluated** against the Oct 25–26 corpus or for false events beyond this run.

## 7. Navigation, for the record

- **Result:** the hold rule confirmed 342 encounters and judged 172 as nonqualifying.
- **When it went wrong:** the first nonqualifying encounter, at 20:47:55, follows the MM003 miss at 135.6 s. As this afternoon, one miss displaced the hold rule until the next declaration.
- **Not analysed further.** The question here was recognition.

## Open, for David

- **Slow passages below about PWM 45.** The decision to add a wider copy (600 ms) is open from the bank replay. This run adds ten slow examples, all recovered by the 600 copy with no false events.
- **Very fast passages near PWM 190.** This is new. Whether it matters depends on whether AUTO will run above about PWM 175. A narrower copy is the corresponding diagnostic; it has not been evaluated for false events.
- **The transient at 443 s while standing** is hardware evidence with an undetermined cause.
- **Build identity.** `SKETCH_NAME` still reads `NAVI_POWER_OF_NORMAL_MANUAL_ONLY`.
