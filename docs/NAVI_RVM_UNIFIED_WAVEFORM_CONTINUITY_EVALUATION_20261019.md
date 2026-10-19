# NAVI_RVM: unified waveform continuity for stops and stalls (architectural evaluation)

Claude (Opus 5.5), 2026-10-19, for David's assignment "NAVI_RVM — Unified Waveform
Continuity for Stops and Stalls".

**Research and offline testing only.** No firmware was modified, nothing was
flashed, and no Pi service was changed. The Pi data were copied read-only. Boot 2
of October 15 was not used.

## Summary

- **Recommendation: Outcome A, simple and effective,** within the recorded
  operating envelope. One caveat must be settled by the planned bounded track
  test before implementation: crawling slower than anything recorded.
- **The unified Hall-only mechanism replaces both PWM rules.** It is a 1 s delay
  line that withholds only flat stretches lying off the baseline. Its settings
  were fixed before the replay (k = 50 blocks, band = 24 ADC, off-baseline
  margin m = 16 × noise).
- **On every recording it was as good as or better than the current firmware:**

| Measure | Current firmware | Unified |
|---|---|---|
| Magnets missed on the route (Oct 18 CW counterfactual + Oct 19 runs 1, 3, 4/5, 6, 7, 8) | 23 | 6 (5 are gaps in the Pi recording, 1 is shared by every mechanism) |
| Annotated ensembles recognized once (Oct 15 boot 1) | 1,087 / 1,089 | 1,089 / 1,089, no splits, no extras |
| Correct MM claims | 834 | 866 |
| Wrong claims | 0 | 0 |
| Added reporting delay | up to 209 s | at most 0.99 s |
| Waiting-block backlog | up to 512 | none |
| Recognizer-ring memory | 8 KB | about 0.8 KB |

- **Its one cost is "false stall" withholding.** Slow magnet shoulders that sit
  flat but off the baseline are withheld even while the train moves: 124.5 s in
  total with the pre-chosen settings. No recognition loss was found from it.
- **The statistical shape model itself cannot make the distinction** (it is
  duration-invariant by design). The distinction comes from the tracing over a
  span, plus the recognizer's existing baseline-and-noise context.
- **The baseline context is essential.** The same rule applied everywhere,
  including the baseline (U0), destroys recognition: 0 of 164 recognitions on
  run 8.

---

## 1. The current mechanisms (firmware `3a8953c`, `RvmPause.h`)

| Part | Rule | Inputs |
|---|---|---|
| STOPPED | A block whose highest applied PWM is below 15 is not appended. The stationary range [min, max] is observed. | applied PWM |
| Arrival hold-back | From a **commanded** target below 15, blocks are held (ring of HOLD_MAX 512, oldest released when full). They are released unchanged if the command is withdrawn. | commanded PWM |
| Arrival trim | At GO, held blocks inside the stationary range are dropped newest-first, until two consecutive blocks lie outside it on the same side. | Hall, the stopped range |
| Resumption | After GO, blocks are withheld until the tracing leaves the stationary range for two consecutive blocks on the same side. | Hall, applied PWM (for GO) |
| Startup | From a declaration until the first moving block, the stationary tracing is the declared baseline. | applied PWM |
| Feeding | Released blocks wait in the same ring and are fed one per pass (the bounded-feeding fix). | — |

That is four states, two PWM signals, a 512-block ring, a trim rule, a
resumption rule and a feeding queue.

**Its failure modes in the field:**
- **Commanded-stop lag.** Held blocks are not seen until GO. At a final stop
  they are never seen.
- **Stall blindness.** At PWM 15 or above the waveform is never paused.

## 2. The unified architecture evaluated

**Principle (David):** recognize the development of magnetic structure, preserve
unfinished structure when development ceases, and continue when the pattern
resumes. The cause (STOP, stall, slow running) is irrelevant.

**Mechanism U1** (`analysis/navi_rvm_shape_20261018/unified/u03_mech.py`):
1. Every block passes through a **delay line of k − 1 blocks**.
2. A block is a **stationary repetition** if it lies in some k-block span whose
   range (max − min) is at most `band`, **and** it lies **off the baseline**:
   |hall − baseline level| > m × noise.
3. Stationary repetitions are withheld. Every other block passes, in order.
4. There is no state, no PWM, no resumption rule and no trim. When the tracing
   develops again, its blocks are no longer in any flat span and pass. Both ends
   of a plateau are removed by the same test.
5. **Baseline level and noise.**
   - In this prototype they are a causal tracker: the median of the 50 declared
     baseline blocks, followed by a 5 s exponential mean of blocks within
     m × noise of it. The noise uses the recognizer's estimator, √π/2 · mean |Δ|
     (floor 0.5).
   - In firmware this context would come from the recognizer's own local
     baseline line (see §10).
6. On the baseline nothing is ever withheld. An extended baseline, moving or
   stationary, stays a baseline.

**Settings fixed before the replay,** from the earlier evidence
(`docs/NAVI_RVM_HALL_CONTINUITY_QUESTION_20261019.md`):

| Setting | Value | Basis |
|---|---|---|
| k | 50 blocks (1 s) | the span over which stationary and moving-in-magnet separated |
| band | 24 ADC | between the stationary maximum of 14.4 and the moving-core minimum of 46 over 1 s |
| m | 16 | about 8–10 ADC off the line |

The 36-point grid in §6 tests the sensitivity; nothing was retuned after the
results.

**U0 (control)** is the same rule without the off-baseline condition.

## 3. Can the existing statistical model support it?

- **The shape model alone cannot.** Each window is linearly resampled to 32
  points and normalized; it has no notion of how fast a shape develops. A window
  dominated by a plateau is merely a distorted shape, which the model rejects:
  MM109 as run.
- **What makes the distinction:**
  1. the **tracing over a span** (development is change over time; at a magnet
     extremum the field gradient is zero, so no instant test exists);
  2. the **recognizer's existing context**: the local baseline and its noise,
     which say whether the tracing is inside an unfinished structure.

  No new recognition subsystem, PWM, IR, speed or amplitude gate is needed.
- **Evidence that the context is necessary (U0):**

| Recording | U0 recognitions | Reference |
|---|---|---|
| Run 8 | **0** | 164 |
| Run 1 | 37 | 176 |
| Boot 1 | 232 | 1,097 |

  Withholding flat *baseline* collapses the inter-magnet structure the
  recognizer needs.

## 4. Data and method

| Recording | Content | Motion reference |
|---|---|---|
| Oct 15 boot 1 (local derived data) | 1,089 annotated ensembles; 44 PWM < 15 stops; **the prolonged obstruction** (stationary over a magnet at PWM 90 for 212 s at 389 s and 73 s at 600 s, 83 ADC off baseline) | IR odometer pulses |
| Oct 18, boot `326C…` | Declared CCW, travelled CW about 3 laps; 4 stops. Replayed as declared and with the **CW counterfactual declaration** (last 041, next 042) | raw IR |
| Oct 19 run 1, `05D9…` | CW lap, final stop | none |
| Oct 19 run 3, `B056…` | 11 stops, **3 over magnets** (MM044, MM048, MM050), slow section | none |
| Oct 19 runs 4–5, `8F9B…` | Cruise PWM 90, final stop at MM029, GO, stops, the stop over MM034 (powered standstill at GO) | raw IR |
| Oct 19 run 6, `EFD0…` | Cruise, final stop | raw IR |
| Oct 19 run 7, `FA9D…` | Exact lap | raw IR |
| Oct 19 run 8, `B29B…` | Slow lap; **stalls over MM109 (59 s) and MM023 (8 s)**; stalls on baseline | raw IR |

**Sources** (read-only copies):
- Pi NSR bytes 7,891,648,749–8,652,695,604 of `navi_sync_20261015_100403.nsr`,
  sha256 `b22980ba0b9bd397db729259d4dc3d66c41a6bdda39d2f064005142ce37240e1`;
- IR raw logs 20261018 (`39fe8587…`) and 20261019 (`314b3911…`);
- declarations from the Pi MQTT logs (`29edf08f…`).

**Inputs:**
- Commanded PWM is the firmware's own value (NSR legacy records) for every
  Oct 18–19 boot. Boot 1 uses the s17 proxy.
- Run 2 and the post-reboot boot `DAFB…` were undeclared and are not replayed.

**Recognizer:** `rvm_local` with `model_A.json`, the Python reference. Its
parity with the firmware C++ path was 164/164 on run 8. Scripts are
`u01`–`u07` in `analysis/navi_rvm_shape_20261018/unified/`; results are in
`.../unified/results/`.

**Scoring:**
- **Route alignment:** an edit alignment of the recognized polarity sequence to
  the route from the declared start magnet, with misses placed at the latest
  consistent position, plus the operator-reported end magnet for the final
  stops.
- **Navigator claims:** the firmware rule.
- **Boot-1 annotation truth.**
- **IR reference** (research only, never an input): a block is MOVING if a
  stripe edge or a swing of at least 60 counts appears within ±0.5 s, and
  STATIONARY if the swing is at most 15 counts.

**Recording artifacts:** three gaps in the Pi recording (reads Otto had but the
UDP recording lost) fall inside magnets and remove those magnets from **every**
replay:
- MM128 (runs 4/5, 103.1 s);
- MM104–MM106 (run 7, 162.0–163.8 s);
- MM123 (run 8, 700.6 s).

On Otto they were recognized in the field. After such a gap, every replay's
navigator CONTRADICTs where the field did not, so claim counts after a gap are
not comparable with the field.

## 5. Results (pre-chosen settings)

R0 is the uninterrupted waveform, F the current firmware, U1 the unified rule.

| Recording | Mechanism | Recognitions | MM claims | Missed on the route | Reporting delay, max | Withheld while IR moving | Stationary off-baseline kept |
|---|---|---|---|---|---|---|---|
| Run 1 | R0 | 176 | 176 | 0 | 0 s | — | — |
| | **F** | 169 | 169 | 0 *(7 fewer: the final deceleration was held and never fed)* | 0 s | — | — |
| | **U1** | **176** | **176** | **0** | 0.99 s | — | — |
| Run 3 (stops over magnets) | R0 | 83 | 3 | **MM044, MM051, MM055** | 0 s | — | — |
| | **F** | 80 | 80 | **MM121–MM126** (final stop held) | 10.2 s | — | — |
| | **U1** | **86** | **86** (MM041 → MM126) | **0** | 0.99 s | — | — |
| Runs 4–5 | R0 / F / U1 | 77 / 77 / 77 | 1 | MM128 (recording gap) | 0 / 0 / 0.99 s | 0 / 8.0 / 7.9 s | 27.5 / 4.8 / **0** s |
| Run 6 | R0 | 67 | 67 | 0 | 0 s | 0 | — |
| | **F** | 62 | 62 | **MM019–MM023** (final stop held) | 10.2 s | 12.3 s | — |
| | **U1** | **67** | **67** | **0** | 0.99 s | 1.8 s | — |
| Run 7 | R0 / F / U1 | 168 each | 80 | MM104–106 (recording gap) | 0 / 0 / 0.99 s | 0 / 1.8 / 1.4 s | — |
| Run 8 (stalls) | R0 / F | 164 / 164 | 85 | **MM109–MM113**, MM123 (gap), **MM023** | 0 s | 0 / 0.6 s | 66.8 / 66.8 s |
| | **U1** | **170** | **99** | MM123 (gap) only | 0.99 s | 37.1 s | **0** |
| Oct 18 (CW counterfactual) | R0 / F / U1 | 523 / 521 / 522 | 222 | MM093 (all) | 0 / **30.6** / 0.99 s | 0 / 18.6 / 23.7 s | — |
| Boot 1 (annotation truth) | R0 | 1,097 | 135 | 1,089 once, 0 missed | 0 s | 0 | **298.4 s** (the obstruction) |
| | **F** | 1,092 | 135 | 1,087 once, **2 missed** | **209.3 s** | 138.7 s | 284.3 s |
| | **U1** | **1,098** | 135 | **1,089 once, 0 missed, 0 split, 0 unmatched** | 0.99 s | 52.6 s | **2.0 s** |

**Notes on the table:**
- Run 1: there is no rest-position report beyond MM038. U1/R0's 7 extra claims
  (MM039–MM045) are consistent with the route (0 extras) and fall in the final
  coast-down.
- Boot 1: route metrics are not used (several declarations and directions), and
  the claim counts are identical.

**Totals** over the route-scored recordings (Oct 18 CW, Oct 19 runs 1, 3, 4/5,
6, 7, 8), plus boot-1 truth:

| | R0 | F | **U1** |
|---|---|---|---|
| Magnets missed on route | 15 | 23 | **6** (5 recording gaps + MM093, which every mechanism misses) |
| Extra recognitions | 11 | 9 | 10 (all on Oct 18, in every mechanism, §7) |
| Correct MM claims | 769 | 834 | **866** |
| Wrong claims | 0 | 0 | **0** |
| Boot-1 ensembles missed / split / unmatched | 0 / 0 / 0 | 2 / 0 / 0 | **0 / 0 / 0** |
| Lowest LLR | 8.08 | 8.08 | **12.34** |
| Max reporting delay | 0 | 209 s | **0.99 s** |
| Max delay in a commanded deceleration | 0 | 30.6 s | **0.99 s** |
| Stationary off-baseline blocks kept (missed stall time) | 392.7 s | 355.9 s | **2.0 s** |
| Blocks withheld while the IR shows movement ("false stall") | 0 | 180.0 s | **124.5 s** |

### Against the assignment's criteria

| Criterion | U1 result |
|---|---|
| Magnetic recognition | Every complete ensemble is recognized wherever the recording is complete. Boot 1: 1,089/1,089 once. |
| Lobe grouping | No split ensembles (boot-1 truth split 0). No extras attributable to the mechanism. |
| Geographic progression | Each magnet is advanced once. 866 correct claims, 0 wrong, 0 duplicate. |
| Stationary plateau | Stalls over MM109 and MM023, the boot-1 obstruction (212 s + 73 s at PWM 90) and the three commanded stops over magnets in run 3 are all excluded. Only 2.0 s of stationary off-baseline tracing is kept, in total. |
| Slow running | Run 3's slow section and run 8's PWM-30 running are recognized. Lowest LLR 12.34 (F 8.08). |
| Commanded deceleration | Magnets are reported throughout the deceleration with a delay of at most 0.99 s. F's final-stop losses (runs 1, 3, 6) disappear. |
| Unexpected stalls | MM109 and MM023 are preserved and completed; MM109–MM113 and MM023 are recovered. |
| Restart | No duplicates. The same ensemble continues: MM044/048/050 in run 3, MM109 and MM023 in run 8. |
| Baseline continuity | Baseline is never withheld; the baseline stops of boot 1 and run 3 are harmless. |
| Reporting latency | A constant 0.98–0.99 s (the delay line), in place of 0 to 209 s. |
| Processing load | No backlog, no release bursts; about 0.8 KB delay line instead of an 8 KB ring; about 100 comparisons per block. |

## 6. Sensitivity (36 settings: k = 25/50/100, band = 8/16/24/32, m = 8/16/32)

Full table: `unified/results/grid_totals.csv`.

- **Recognition is nearly invariant.** Across all 36 settings: route misses 6–7,
  boot-1 truth missed 0 / split 0 / unmatched 0, MM claims 856–866, wrong claims
  0 in every case.
- **m (the off-baseline margin) governs false-stall withholding.**

| m | Withheld while moving (s) |
|---|---|
| 8 | 82–617 |
| 16 | 5–201 |
| 32 | 3–46 |

  - At m = 32 the stationary off-baseline time kept rises only slightly (to
    0.8–5.6 s), but the lowest LLR falls back to about 8.
  - At m = 8 one miss appears in some settings (claims drop to 856).
- **Short spans with wide bands are unsafe.** k = 25 with band 32, and k = 100
  with band 32 and m = 32, drive the lowest LLR to 0.3–5.3: they start removing
  real development.
- **The pre-chosen setting (k = 50, band = 24, m = 16) sits inside the safe
  region.**

## 7. Remaining ambiguities

1. **Crawling slower than recorded.** No recording shows Otto moving through a
   magnet slower than PWM about 30 with ground truth. Below some speed a
   developing shoulder or peak will stay within `band` for k blocks and be
   withheld. That is harmless if it only compresses a slow passage, but it is
   untested. **The bounded track test (crawl trial T1, creep trial T4) is the
   check.**
2. **False-stall withholding is real.** 124.5 s in total, mostly flat magnet
   shoulders 8–15 ADC off the line at PWM 30–90. No recognition loss was
   measured, but the waveform the recognizer sees is compressed there.
3. **The baseline context in firmware.**
   - The prototype uses a causal level tracker.
   - Firmware should use the recognizer's own local baseline line.
   - The two may differ slightly at curved baselines.
   - It must be re-validated by C++ replay after implementation.
4. **Declaration on a baseline is assumed** (as now).
5. **The MM109 Hall drift** (15 ADC with the wheel still) stays within `band`
   and was handled, but its cause is unexplained.
6. **Oct 18 CW counterfactual.** 9–11 extra recognitions in two slow approaches
   (2,047–2,058 s and 2,090–2,098 s) and the MM093 miss occur **under every
   mechanism, including R0**. They are a recognizer observation in this
   counterfactual replay, not a continuity effect. Not investigated.
7. **Toby, CCW travel and other temperatures:** untested.
8. **Recording gaps** limit the replays (§4), not the mechanisms.

## 8. Simplicity

| | Current F | Unified U1 |
|---|---|---|
| Inputs | applied PWM, commanded PWM, Hall | Hall (+ the recognizer's baseline and noise) |
| States | Startup / Running / Stopped / Awaiting | none |
| Rules | STOPPED threshold, hold-back, release on withdrawal, arrival trim, two-block resumption, startup | one: withhold flat off-baseline spans |
| Buffers | 512-block ring + feeding queue | 50-block delay line |
| Constants | 15, 512, 2 | k, band, m |
| Failure modes seen in the field | final-stop lag (up to 209 s, magnets never fed), stall blindness | none in these recordings (§7.1 untested) |

U1 is simpler: one rule replaces six, and it has no PWM dependency. It has three
constants against F's three, all derived from Otto's data.

## 9. Recommendation: Outcome A

- The existing recognizer and its baseline context, with a 1 s delay line,
  distinguish developing from stationary magnetic structure in every recorded
  condition:
  - slow running;
  - commanded stops on the baseline and over magnets;
  - final stops;
  - stalls at PWM 29–90, including the 212 s obstruction.
- It replaces both PWM rules, removes the 10–209 s reporting lags, the 512-block
  backlog and about 7 KB of heap, and loses nothing measured.
- **Condition:** run the bounded slow-crawl track test before flashing. Its
  result decides whether the very-low-speed limit (§7.1) lies inside the AUTO
  envelope.

## 10. The smallest justified implementation change (not done; for review)

1. **`RvmPause.h`:** replace the WaveformPause state machine, hold-back, trim,
   resumption and feeding queue with a k-block delay line. Each newest block
   completes the k-span decision for the oldest block, using a running min/max
   over the last k blocks. The oldest block is emitted unless it lies in a flat
   span off the baseline.
2. **`RvmShape.h`:** add a read-only accessor for the recognizer's current local
   baseline level and noise at the newest block, so that "off baseline" is the
   recognizer's own judgement (no new estimator).
3. **`.ino`:**
   - feed the delay line;
   - drop the PWM and commanded inputs from the continuity path (they stay in the
     telemetry);
   - redefine the `diag/rvm_stopgo` events as "development stopped / resumed";
   - free the 8 KB ring.
4. **Tests:**
   - C++ replay parity with this Python model on boot 1 and runs 1–8;
   - a unit test for flat-span detection at plateau starts and ends.
5. **Settings to be ratified by David:** k = 50, band = 24, m = 16.

**Rollback:** firmware `3a8953c` (current) and backup `NAVI_EWO_0_6`.
