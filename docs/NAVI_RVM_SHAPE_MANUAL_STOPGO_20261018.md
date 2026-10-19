# NAVI_RVM_SHAPE_MANUAL: STOP/GO continuity correction

2026-10-18 · Claude (Opus 5.5) · Development, compilation and offline testing only. **Not flashed.**

> **Scope.** This implements the operator's STOP/GO assignment of 2026-10-18 on
> top of
> [`NAVI_RVM_SHAPE_MANUAL_FIELD_CANDIDATE_20261018.md`](NAVI_RVM_SHAPE_MANUAL_FIELD_CANDIDATE_20261018.md),
> and supersedes its §6.1.
>
> **Evidence.** Everything here comes from Oct 15 boot 1 and Otto. Boot 2 is
> untouched.
>
> **Unchanged.** No operational firmware, Pi service, MQTT or dashboard
> configuration was changed.

> **Superseded in part.** Pattern-based resumption after GO is now implemented; see
> [`NAVI_RVM_SHAPE_MANUAL_RESUMPTION_20261018.md`](NAVI_RVM_SHAPE_MANUAL_RESUMPTION_20261018.md).
>
> **REVISION: STOPPED = applied PWM < 15.** David ruled this after the first
> version ("PWM <15 for sure"). The candidate now implements **< 15**.
> §0A gives the current numbers. Sections 0–4 below record the results at the
> first boundary (< 25) and remain as history. The commanded-stop results are
> the same at both boundaries.

## 0A. Current results (STOPPED = PWM < 15)

**Commanded stops are fully preserved.** Every one of the 864 idealized STOP/GO
cases is again invariant:
- 18 real ensembles, CW and CCW;
- stops at every position, from 1 s to 30 min, including repeated stops;
- correct MM sequence throughout;
- C++ equal to Python in all 1,260 runs.

**Boot 1 with the pause.**
- Leave-one-pass-out: 1,064 of 1,081 recognized once, 0 split, 0 on baseline.
- The misses are unchanged: the 16 after the PWM-100 stall, and MM115 at the
  final manual reversal.
- Parity Python = C++ holds: 1,093 recognitions, identical.
- Navigation replay: 31/31 recorded stops behave as before (30 correct; the one
  exception follows a capture gap), with no duplicate counting.

**Otto against the PWM < 15 boundary.** Source: `stopped_threshold_evidence.*`,
35 recorded station stops.

| | At < 25 | **At < 15** |
|---|---|---|
| Arrivals still moving after PWM fell below the boundary | 12 / 35 | **1 / 35** (0.43 s) |
| Stationary time after PWM reached the boundary on departure, median (range) | 1.8 s (0.1–3.8 s) | **3.3 s (0.1–5.3 s)** |

Below 15 he is indeed stopped. The arrival edge is now right.

**On departure, Otto now stands still even longer at PWM ≥ 15** before he moves.
Those stationary blocks are admitted to the waveform.

**Stops with Otto's measured behaviour.** Each case is a 5 s stop; the figure is
invariant cases out of 18 per position.

| Variant | Baseline | Rising | Peak | Falling |
|---|---|---|---|---|
| Departure: still for 3.3 s (median) at PWM ≥ 15 | 18 | 0 | 0 | 0 |
| Departure: still for 5.3 s (max) at PWM ≥ 15 | 17 | 0 | 0 | 1 |
| Arrival: moving 0.43 s below 15 (worst recorded case, 1 of 35 stops) | 17 | 17 | 8 | 0 |
| Both | 17 | 0 | 0 | 0 |

**What this means for the field test.**

- STOP/GO with the sensor car on the baseline works.
- A stop with the sensor car over a magnet will almost always still lose that
  magnet. The cause is the stationary start-up interval above PWM 15. The
  navigator then reports CONTRADICTION at the next magnet, or, if polarities
  agree, miscounts silently.
- Holding still at the start is a property of Otto's motor. No STOPPED
  threshold below his breakaway PWM (27–90) can exclude it.

**Build.** It compiles unchanged in size: **980,207 B flash (74 %)**,
**121,228 B static RAM (36 %)**. The authority check and unit checks pass.

**Practical rule for the first test: stop with the sensor car on baseline.**

---

**Candidate**

- Sketch: `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/NAVI_RVM_SHAPE_MANUAL.ino`
- Branch: `dev/navi-rvm-shape-manual-20261018`
- The commit is given in the final message of the session that pushed this file.
  The branch HEAD is the candidate.

---

## 0. Answer first (first boundary, PWM < 25; history)

**The correction is implemented exactly as ruled, and it meets the acceptance
criterion for commanded stops.** STOPPED is defined as applied PWM < 25.

- **Synthetic suite.** It contains 864 idealized STOP/GO cases on 18 real
  recorded ensembles (12 CW, 6 CCW; bar and disk magnets).
  - The stops were inserted on the baseline, the leading lobe, the rising flank,
    the peak, the falling flank, the trailing lobe, and after completion.
  - They lasted 1, 3, 5, 10, 30, 300 and 1,800 s, singly and repeated within one
    ensemble.
  - In every case the result is identical to the uninterrupted passage: the same
    recognitions, the same polarity, the same centres, the same MM sequence, no
    duplicate and no miss.
- **Firmware parity.** The C++ firmware headers give results identical to the
  Python reference in all 1,260 suite runs and on the whole of boot 1.

**But Otto's measured behaviour does not fit the PWM < 25 boundary at
departure.** At all 35 recorded station stops, Otto stayed still for **1.8 s
(median; range 0.1–3.8 s) after PWM rose past 25**. He first moved at PWM 27–90
(median 37).

- Those stationary blocks are admitted to the waveform.
- When the stop is **inside an ensemble**, the magnet is then lost and the next
  magnet is a CONTRADICTION. This happened in **105 of 108** measured-departure
  cases at the rising flank, peak and falling flank.
- Stops on the baseline are unaffected (36/36).

**At arrival the opposite happens.** Otto was still moving below PWM 25 at 12
of the 35 stops. That loses part of a magnet stopped on: of 54 cases at the
rising flank, peak and falling flank with 0.5 s of motion after PWM < 25, **31
were not invariant**.

**What this means for the first field test.**

- STOP/GO on the baseline works.
- STOP/GO with the sensor car over a magnet will usually still fail on Otto,
  even though the commanded-stop logic is correct.
- The cause is the PWM boundary, not the waveform preservation.
- **Your decision is needed on the STOPPED boundary (§4).** I did not change
  the threshold.

**Other results**

- **Build.** It compiles for Otto: **980,207 B flash (74 %)** and
  **121,228 B static RAM (36 %)**.
- **No motor authority.** Navigation has none, as the static check shows. PWM
  is read in exactly one place, the pause decision.
- **Boot-1 replay with the pause.** Recognition is unchanged apart from these
  cases:
  - three magnet waveforms while the train sat at PWM 0 during the obstruction
    episode (it was evidently moved by hand);
  - MM115 at the final manual reversal (arrival creep);
  - the 285 s **stall at PWM 100**, which the pause cannot see because it was
    not a commanded stop. That remains open.

---

## 1. What changed

| File | Change |
|---|---|
| `RvmPause.h` (new) | `WaveformPause`: STOPPED = the block's highest applied PWM < 25. A STOPPED block is not appended to the recognition waveform. From a declaration until the first moving block, the stationary tracing *is* the declared starting baseline and is appended |
| `RvmTracing.h` | Each 20 ms block carries the highest applied PWM among its reads (`Block.pwm`), for the pause only |
| `RvmShape.h` | Each waveform block keeps its tracing block id, so reported times are true times when blocks were skipped. Recognition logic is unchanged |
| `NAVI_RVM_SHAPE_MANUAL.ino` | Blocks carry `actualPwm`; the RVM task runs `waveformPause.admit(b.pwm)` before `push`; a declaration calls `waveformPause.declared()` |
| Reporting | `state/nav` adds `waveform_paused`; `state/loopstat` adds `paused_blocks`; `diag/rvm_ensemble` times are the true block times and `window_ms` became `waveform_window_ms` (it excludes paused time) |

**Not changed:**

- the recognizer (windows, features, model, LLR, best-explanation grouping);
- the local baseline;
- the navigator, the polarity map and CONTRADICTION;
- every motor, ramp, brake, direction, E-STOP and low-voltage path;
- MQTT command handling;
- the Pi recorder (raw Hall is recorded throughout stops);
- the model, `RvmShapeModel.h`, byte for byte.

**How the requirement maps onto the code.** While STOPPED, nothing reaches the
recognizer. As a result:

- the preceding baseline's sums are untouched;
- the unfinished ensemble's samples stay in the ring;
- the held candidate (if any) stays held;
- the acceptance clock, measured in waveform blocks, does not advance;
- the navigator's expected MM and polarity are untouched.

On resume, the next moving block follows the last pre-stop block as if no time
had passed. Stationary values never become baseline or plateau. The dashboard
keeps showing the last established MM; the MM advances only when the completed
ensemble is accepted.

**Startup.** The rule from the earlier assignment still applies. The stationary
tracing after a declaration, taken with the sensor declared clear of a magnet,
is BL₀, so it is admitted until the first moving block. After that, stops pause.

## 2. Verification

### 2.1 Required STOP/GO cases

Script: `s18_stopgo_suite.py`; results in
`results/local_pause/stopgo_suite*.{csv,json}`.

The cases are 18 real ensembles: CW MM083, 117, 133, 162, 032, 066, 111, 127,
163, 019, 070, 112, and 6 CCW. Each one is:
- embedded in its recorded stretch (one magnet before, six after);
- preceded by a 5 s declared stationary baseline;
- run through Python **and** the firmware headers.

A stop is the held Hall value plus the stretch's own baseline noise, at PWM 0.
"Invariant" means the same recognitions, polarity and centres (±1 block, after
mapping back to the recording) as the uninterrupted run. "MM ok" means the
navigator produced the full expected MM sequence with no CONTRADICTION.

| Required test | Cases | Invariant | MM ok |
|---|---|---|---|
| Normal uninterrupted passage | 18 | 18 | 18 |
| Stop on baseline (1–300 s) | 108 | 108 | 108 |
| Stop during leading lobe | 108 | 108 | 108 |
| Stop during principal excursion: rising / peak / falling | 324 | 324 | 324 |
| Stop during trailing lobe | 108 | 108 | 108 |
| Stop after ensemble completion | 108 | 108 | 108 |
| Long stop (1,800 s), peak and baseline | 36 | 36 | 36 |
| Repeated STOP/GO in one ensemble (3 × 2 s, 3 × 10 s, 5 × 1 s) | 54 | 54 | 54 |
| Subsequent magnets after any of the above | (inside each case) | all | all |
| **C++ equal to Python** | 1,260 runs | **1,260** | |

**Hall level shifted after a 30 s stop.** This is a characterization: the
offset persists for the rest of the stretch.
- At 2, 5 and 10 ADC, all 54 baseline cases are invariant.
- At the peak, 52 of 54 are invariant: two CCW cases at 10 ADC moved a centre
  by 2 blocks (40 ms). All 54 had the correct MM sequence.

**Firmware unit checks (`unit.cpp`).** The same bump with 1 s and 30 s stops
inserted at seven positions, and with repeated stops, gives exactly the same
recognition: identical centre and LLR. All checks pass.

### 2.2 With Otto's measured stopping and starting

These are the same cases with what the recordings show Otto doing (`s16`, 35
station stops):

| Variant (5 s stop) | Baseline | Rising | Peak | Falling |
|---|---|---|---|---|
| Departure: still stationary for 1.8 s at PWM ≥ 25 | 18/18 | 0/18 | 0/18 | 2/18 |
| Departure: still stationary for 3.8 s at PWM ≥ 25 | 18/18 | 0/18 | 0/18 | 1/18 |
| Arrival: still moving for 0.5 s at PWM < 25 | 17/18 | 16/18 | 7/18 | 0/18 (8 MM ok) |
| Both (1.8 s + 0.5 s) | 16/18 | 0/18 | 0/18 | 0/18 |

The failures inside an ensemble are CONTRADICTIONs at the next magnet: the held
magnet is not counted.

### 2.3 Boot 1 with the pause (`s17_stopgo_replay.py`)

- **Firmware parity.**
  - With the pause: Python = C++, 1,093 = 1,093 recognitions with identical
    waveform centres, block ids and polarity, and LLR within 0.0003.
  - Without the pause: the earlier 1,097 = 1,097 (`s14`) still holds.
- **Leave-one-pass-out, Model A.**
  - 1,064 of 1,081 recognized once, 0 split, 0 on baseline, polarity 100 %.
  - Misses: the 16 after the PWM-100 stall (unchanged) and MM115 at 3,798 s.
  - MM115: the end-of-session manual reversal at low PWM, i.e. arrival creep.
- **Recorded STOP/GO.** 31 operator stops (5 s dwells) fall inside tracking
  stretches.
  - 30 are correct on both sides.
  - The one exception has a capture-gap loss before the stop, as before.
  - No duplicate counting.
- **Navigation breaks.** They are the same as without the pause. Each one
  follows a Pi-capture gap, the stall, the 104 s declaration or the manual
  reversal. The obstruction-episode recognitions are gone, because the train was
  at PWM 0.

### 2.4 Build and authority

| | |
|---|---|
| Program storage | **980,207 / 1,310,720 B (74 %)** |
| Static RAM | **121,228 / 327,680 B (36 %)**; recognizer 68,192 B, pause 2 B |
| Warnings | none (arduino-cli, esp32:esp32 3.3.12, FQBN `esp32:esp32:esp32`) |
| Authority check | passes |

The authority check confirms:
- the recognizer, navigator and model headers contain no PWM, actuator, safety
  or network symbol;
- `RvmTracing.h` and `RvmPause.h` contain no actuator or safety symbol;
- in the sketch, a block's PWM is read only in `waveformPause.admit(b.pwm)`;
- actuator writes exist only in the operator, E-STOP, low-voltage and startup
  paths.

## 3. Remaining limitations for the first manual field test

1. **Stopping over a magnet will usually still lose that magnet.** The cause is
   Otto's PWM breakaway and roll-out (§0, §2.2, §4). If not changed, the
   practical rule for the first test is: **stop with the sensor car on
   baseline**.
2. **A stall at running PWM is not a commanded stop.** The pause cannot see it.
   - A stall over a magnet, as at 388–673 s, still loses that magnet and
     corrupts the baseline until the navigator contradicts.
3. **The train moved by hand at PWM 0 is invisible**, by design of the pause.
   Re-declare after handling.
4. **Manual direction change still suspends navigation.** Re-declare. The
   assignment assumes forward-only running.
5. **Silent off-by-one.** A missed magnet whose successor has the same polarity
   gives wrong MM claims until the next polarity disagreement. The architecture
   has no recovery.
6. **Narrow evidence base.** One evening, one locomotive. Not tested: weak
   magnets, other days, Toby.
7. **Not tested on hardware.** No ESP32 timing, heap or live MQTT/Pi delivery
   test. `state/loopstat` reports the maximum push time, queue drops, paused
   blocks, stack and heap for the first powered run.
8. **Empty 20 ms blocks.** A block with no Hall reads at all carries PWM 0 and
   is treated as STOPPED. This does not occur on the ESP32 in normal operation;
   it would only matter if the Hall task were starved.

## 4. Decision needed: the STOPPED boundary on Otto

The measurements come from `results/local/stopped_threshold_evidence.csv`.

**Measured PWM boundaries**

| | Distribution |
|---|---|
| Last motion when stopping (35 stops) | 13–35, median 26 |
| First motion when starting | 27–90, median 37 (25 % ≤ 34, 75 % ≤ 41) |

**Effect of a single threshold X.** "Lost" means real waveform excluded;
"stationary" means stationary samples admitted.

| X | Departures still stationary at PWM ≥ X | Arrivals still moving at PWM < X |
|---|---|---|
| 25 (ruled) | 35 / 35 | 12 / 35 |
| 30 | 34 | 25 |
| 35 | 19 | 32 |
| 40 | 9 | 35 |
| 45 | 2 | 35 |

No single threshold separates both edges, because Otto's starting and stopping
PWMs overlap.

Whether a different boundary is acceptable is your architectural call. Any
choice is a statement about Otto's motor, not about magnets. The candidate
implements PWM < 25 exactly, and nothing else.

## 5. Build and flashing instructions (for the operator)

Run from a checkout of `dev/navi-rvm-shape-manual-20261018`.

1. **Identify Otto's current binary for rollback.** I could not determine
   exactly which sketch and commit Otto runs now; the last recorded field build
   was the EWO_0_6 line. Keep that build path, or an exported `.bin`, so you can
   reflash it.
2. **Close every serial monitor or capture on Otto's USB port** before
   uploading.
3. **Compile with your real credentials.** The directory must contain your
   private `credentials.h`:

   ```bash
   tools/navi_rvm_shape/build.sh --credentials-dir ~/NGR/private-config --build-path /tmp/rvm_otto
   ```

   Expect "Sketch uses 980… bytes (74%)".
4. **Upload.** Replace the port with Otto's:

   ```bash
   arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/cu.usbserial-XXXX --input-dir /tmp/rvm_otto
   ```

5. **Check after boot.**
   - The serial console shows
     `[BOOT] NAVI_RVM_SHAPE_MANUAL MANUAL_OBSERVATION_ONLY_NOT_FIELD_ACCEPTED loco=9950011 model=330cdc007da3445b`.
   - `ngr/loco/9950011/state/bootid` carries `"sketch":"NAVI_RVM_SHAPE_MANUAL"`
     and `"navigation":"OBSERVATION_ONLY"`.
   - The dashboard MM shows "--" until declared.
   - Throttle, brake, STOP and E-STOP work on the stand before any running.
6. **Declare.** Stop with the sensor car over baseline, then set the session
   direction and the start interval:
   - `ngr/loco/9950011/cmd/session_direction`: `CW` or `CCW`;
   - `ngr/loco/9950011/cmd/start_interval`, for example `063-064`.

   `state/nav` should read `"state":"TRACKING"` with `"mm":63` (CW).
7. **Watch these topics.** The Pi run logger (`ngr/#`) records them, and the
   native NSR stream still goes to 192.168.68.142:47620:
   - `diag/rvm_ensemble` for each recognition;
   - `state/nav` for the MM and any CONTRADICTION;
   - `state/loopstat` for `rvm_push_max_us`, `block_q_drop`, `paused_blocks`
     and heap.
8. **Rollback.** Reflash the build from step 1.
