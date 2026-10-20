# NAVI_RVM_CONTINUITY_MANUAL: delivery for flashing (2026-10-19)

Claude (Opus 5.5) delivers the firmware implementing the approved unified Hall
continuity. **Otto was not flashed**; David flashes it.

## Authorization

David, 2026-10-19: "Implement, compile, test, commit, and push the firmware …
Do not flash Otto. I will do that myself." Basis:

| | |
|---|---|
| Starting firmware | `3a8953c` |
| Research | `d8e2f4d` |
| Decision record | `7a27405` (the watchdog reboot fix is folded into this implementation) |

## What was built

| | |
|---|---|
| Branch | `dev/navi-rvm-shape-manual-20261018` (commit given in the push message) |
| Sketch | `firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL/NAVI_RVM_CONTINUITY_MANUAL.ino` |
| New header | `RvmContinuity.h` (`HallContinuity`): replaces `RvmPause.h` |
| Unchanged, byte-identical to `3a8953c` | `RvmShape.h` (recognizer, local baseline), `RvmShapeModel.h` (model `330cdc007da3445b`), `RvmNavigator.h` (polarity map, progression), `RvmTracing.h`, `RouteMap.h`, `RvmOps.h`, `NaviEstop.h`, `NaviSyncRecorder.h`, `NaviWordStorage.h`, `LocoConfig.h`, `LL_LocoConfig_9950011.h` |
| Previous sketch | `NAVI_RVM_SHAPE_MANUAL/` is left untouched as the rollback |
| Tests | `tests/navi_rvm_continuity/`: `unit.cpp`, `replay.cpp`, `authority_check.py`, `run_checks.sh` |
| Build script | `tools/navi_rvm_continuity/build.sh` |
| Recording verification | `analysis/navi_rvm_shape_20261018/unified/u10_firmware_parity.py`; results in `unified/results/firmware/` |

### The mechanism

These are the evaluated parameters, unchanged:

| Parameter | Value |
|---|---|
| K (delay line and span) | 50 blocks (1 s) |
| BAND | 24 ADC |
| M (off-baseline margin) | 16 × baseline noise |

- **Rule.** A block is withheld when it lies in a 50-block span of range ≤ 24 ADC
  **and** more than 16 × noise from the baseline level. Every other block
  reaches the recognizer, in order.
- **Baseline level and noise** follow the evaluated model exactly:
  - at a declaration: the median and noise (√π/2 · mean |step|, floor 0.5) of
    the declared block and the 49 after it;
  - then a 5 s exponential mean of blocks within 16 × noise.
- **Declarations** take effect when their block leaves the delay line, about 1 s
  after the command. The recognizer's baseline and the navigator start there.
  DECLARED is published then. A direction or session change cancels a
  declaration that has not yet taken effect.
- **Removed:** the applied-PWM STOPPED state, the commanded-PWM hold-back (the
  512-block ring), the arrival trim, the stationary-range resumption and the
  feeding backlog.

### Points that answer the requirements

| Requirement | Done |
|---|---|
| 1. Replace both PWM mechanisms | `RvmPause.h` removed. The authority check confirms the sketch and `RvmContinuity.h` read no block PWM or commanded value. |
| 2. Evaluated parameters | 50 / 24 / 16 (`CONT_K`, `CONT_BAND`, `CONT_M`) |
| 3. Recognizer, local baseline, polarity map, progression preserved | Their headers are byte-identical to `3a8953c`. |
| 4. Unfinished ensembles preserved through stops and stalls | Run 3 (3 stops over magnets), run 8 (MM109, MM023), run 9 (22 stalls), boot 1 (the 212 s obstruction); see below. |
| 5. No 512-block backlog | One block in, at most one block out. `rvmTask` always blocks on the Hall queue (100 ms timeout), so the core-0 idle task, and with it the task watchdog, is no longer starved: the probable cause of the run 5 and run 9 reboots. |
| 6. Manual control, safety, MQTT, Pi recording preserved | The motor, E-STOP, battery, MQTT and NSR code are unchanged. The authority check allows actuator writes only in the operator, E-STOP and battery paths. |
| 7. Observation only | The navigation functions touch no actuator or motor state (authority check). |
| 8. Nothing else added | No recovery, speed estimate, IR, or new gates. |

## Verification

### Host checks (`tests/navi_rvm_continuity/run_checks.sh`)

**Unit checks: all passed.**
- The navigator, tracing and recognizer checks from `3a8953c`.
- 7 continuity checks:
  - a 30 s in-magnet stall with drift is withheld; its rise and fall are kept;
  - baselines before and after are kept;
  - a long flat baseline is never withheld;
  - nothing is withheld before a declaration;
  - a slow 4 s passage keeps its flanks (see limitation 3);
  - gap blocks pass;
  - a live declaration binds to the next block;
  - a stall on the baseline is kept;
  - one block in, at most one out; chronological order.

**Authority check: passed** (no violations).

### Python/C++ parity, and the recordings replayed through the firmware path

`replay.cpp` uses exactly the firmware headers on the native 5 kHz Hall
recordings. The Python reference is the evaluated model (`u03_mech.u_unified` +
`rvm_local`).

| Recording | Withheld blocks C++ / Python (differing) | Recognitions C++ / Python (same block and polarity) | Max LLR difference | Navigator events |
|---|---|---|---|---|
| Oct 15 boot 1 | 20,398 / 20,398 (0) | 1,098 / 1,098 (1,098) | 0.0004 | identical |
| Oct 18 (as declared, CCW) | 1,526 / 1,526 (0) | 522 / 522 (522) | 0.0004 | identical |
| Oct 18 (CW counterfactual) | 1,526 / 1,526 (0) | 522 / 522 (522) | 0.0005 | identical |
| Oct 19 run 1 | 518 / 518 (0) | 176 / 176 (176) | 0.0003 | identical |
| run 3 | 5,814 / 5,814 (0) | 86 / 86 (86) | 0.0002 | identical |
| runs 4–5 | 1,799 / 1,799 (0) | 77 / 77 (77) | 0.0003 | identical |
| run 6 | 106 / 106 (0) | 67 / 67 (67) | 0.0004 | identical |
| run 7 | 71 / 71 (0) | 168 / 168 (168) | 0.0003 | identical |
| run 8 | 5,562 / 5,562 (0) | 170 / 170 (170) | 0.0003 | identical |
| run 9 (boots joined) | 4,839 / 4,839 (0) | 169 / 169 (169) | 0.0002 | identical |

The single trailing block in each count is the firmware tracing's last,
unfinished block. **Parity is exact on all ten replays.**

**Two harness issues found and fixed during verification, neither in the
firmware:**
1. The replay first loaded all of boot 1's declarations at once into the
   continuity's 4-slot pending queue, which dropped the later ones. It now hands
   each one over at its block, as on the locomotive.
2. The run-9 Python reference had joined the two boots' already-blocked
   tracings. Boot B's clock sits 0.907 ms off A's 20 ms grid, so it now re-blocks
   the joined reads as the firmware does.

### Outcomes (firmware path)

| Recording | Recognitions | MM claims | Missed on the route | Extras | Lowest LLR |
|---|---|---|---|---|---|
| Boot 1 (annotation truth) | 1,098 | 135 | 1,089 / 1,089 recognized once; 0 missed, 0 split, 0 unmatched | — | 12.75 |
| Oct 18 (CW counterfactual) | 522 | 222 | MM093 | 10 | 33.57 |
| Run 1 | 176 | 176 | 0 | 0 | 34.20 |
| **Run 3** (3 commanded stops over magnets, slow section) | 86 | **86 (MM041 → MM126)** | **0** | 0 | 12.34 |
| Runs 4–5 | 77 | 1 | MM128 (recording gap) | 0 | 32.13 |
| Run 6 | 67 | 67 | 0 | 0 | 26.31 |
| Run 7 | 168 | 80 | MM104–106 (recording gap) | 0 | 33.16 |
| **Run 8** (stalls over MM109, MM023) | **170** | 99 | MM123 (recording gap) only: **MM109–MM113 and MM023 recognized** | 0 | 18.64 |
| **Run 9** (22 stalls, very slow running) | 169 | 1 | MM025 (cut by the reboot), MM049 (all mechanisms) | 0 | 32.04 |

**What the table shows:**
- **No duplicate magnets:** 0 extras in every Oct 19 run, and 0 split or
  unmatched in boot 1.
- **The Oct 18 extras and MM093** occur identically under every mechanism,
  including the uninterrupted waveform. They are recognizer observations in a
  counterfactual replay, not continuity effects.
- **Every miss** is a recording gap, the run-9 reboot cut, or the MM093/MM049
  cases shared by all mechanisms.
- **Claims after a recording gap** are lower than in the field, because the
  replay's navigator CONTRADICTs where Otto, which had the data, did not.

**Commanded STOP/GO, slow running, throttle reversals:**
- Run 3's three stops over magnets (MM044, MM048, MM050) and its slow section
  give 86 consecutive correct claims, MM041 → MM126, with no final-stop loss.
  The old firmware lost MM121–MM126 behind its hold-back.
- Run 9 contains 3 throttle reductions reversed before reaching zero, and 22
  stalls at PWM 15–89. The per-stall outcome is in the evaluation addendum.
- **Reporting delay:** a constant 0.98 s, including during commanded
  decelerations. The old firmware's delay reached 10–209 s.

### ESP32 processing

- **One block in, at most one block out, per 20 ms.** There is no backlog, no
  release burst and no busy loop.
- **Per-block cost of continuity:** one 50-element min/max scan plus a few
  operations. The recognizer's cost per block is unchanged, and it is fed fewer
  blocks during stalls.
- **Host replay timings:** worst single recognizer step 0.07–1.9 ms across the
  recordings; continuity storage 800 B.
- The ESP32 field figure for the unchanged recognizer was a worst step of
  0.95–1.73 s in slow sections; the 128-block (2.56 s) Hall queue absorbs that.

### Compilation for Otto

Toolchain: arduino-cli 1.5.1, esp32:esp32 3.3.12, FQBN `esp32:esp32:esp32`,
with private credentials.

| | NAVI_RVM_CONTINUITY_MANUAL | `3a8953c` (same toolchain) |
|---|---|---|
| Program storage | **982,667 B (74 %)** | 982,499 B |
| Global (static) RAM | **121,324 B (37 %)** | 121,292 B |
| Heap allocated at boot for continuity / hold-back | **800 B** | 8,192 B |

The new build frees about 7.4 KB of heap at run time. Free heap had been the
tightest resource: `heap_min` was about 8.7–9.9 KB.

## Remaining field-test limitations

1. **Slower crawling than recorded is untested.** The rule withholds any part of
   a moving passage that changes by 24 ADC or less within 1 s. A slower crawl
   than recorded (PWM about 30) loses more of its top. The slow-crawl track test
   (session sheet `docs/NAVI_RVM_SLOW_CRAWL_TEST_SESSION_SHEET_20261019.md`)
   still applies.
2. **The off-baseline margin (16 × noise, about 8 ADC) has about a factor-of-2
   safety band** on Otto's real stalls (+3.7, +9.8, −18.6 ADC). Stalls on a
   magnet's outer shoulder near that level are partly kept. Watch
   `DEVELOPMENT_STOPPED` events whose `hall − baseline_level` is under about
   15 ADC.
3. **Flat tops and the first and last few blocks of slow passages are
   withheld.** The edge blocks are those within 24 ADC of a flat baseline
   (unit check 4). This compresses slow ensembles. No recognition loss was
   found, but margins fall on slow passages (lowest LLR 12.3).
4. **Declarations take effect about 1 s after the command.** Declare while
   stationary on a baseline, as before.
5. **The probable watchdog cause of the run 5 and run 9 reboots is removed but
   not confirmed.** The build still does not publish `esp_reset_reason()`. Any
   further `online` flap should be reported.
6. **Untested:** Toby, CCW travel, other temperatures. The run-9 MM023
   rest-position discrepancy remains open (it does not affect this firmware).
7. **Not field accepted.** Navigation is observation only.

## Flashing Otto (for David)

1. **Check out and build.** `firmware/config/credentials.h` is your private,
   git-ignored file.

   ```sh
   cd ~/esp-loco-control
   git fetch origin && git checkout dev/navi-rvm-shape-manual-20261018 && git pull
   tools/navi_rvm_continuity/build.sh --credentials-dir firmware/config --build-path /tmp/navi_rvm_continuity_build
   ```

   Expect: `Sketch uses 982667 bytes (74%)`, `Global variables use 121324 bytes (37%)`.

2. **Connect Otto by USB** (CH340) and find the port:

   ```sh
   ls /dev/cu.wchusbserial*
   ```

   It has been `/dev/cu.wchusbserial110` and `/dev/cu.wchusbserial10`.

3. **Upload at 115200 baud.** Faster speeds corrupted reads on this adapter.

   ```sh
   arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 --port /dev/cu.wchusbserial110 --input-dir /tmp/navi_rvm_continuity_build firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL
   ```

   Use your actual port. Expect "Hash of data verified" for each region.

4. **Reboot Otto on battery or track power** (not USB), so the INA219 is found
   and battery protection is active.

5. **Verify over MQTT (read-only):**

   | Topic | Expected |
   |---|---|
   | `ngr/loco/9950011/state/bootid` | `"sketch":"NAVI_RVM_CONTINUITY_MANUAL"`, `"continuity":"HALL_ONLY_K50_B24_M16"`, `"model":"330cdc007da3445b"`, `"auto":"ABSENT"` |
   | `state/loopstat` | `"continuity":"HALL_ONLY_K50_B24_M16"`, `rvm_delay_blocks` about 49, `withheld_blocks`; `block_q_drop 0` |
   | `state/warning` | not "INA219 NOT FOUND" |

6. **Declare** as before (`session_direction`, then `start_interval`). `DECLARED`
   appears about 1 s later.

**Rollback:** rebuild and flash `NAVI_RVM_SHAPE_MANUAL` (commit `3a8953c`)
with `tools/navi_rvm_shape/build.sh` and the same upload command with that
sketch path. The pre-RVM image is
`~/NGR/NGR-Files/otto-flash-backups/otto_before_NAVI_RVM_SHAPE_MANUAL_30271a6.bin`
(see `docs/OTTO_FLASH_NAVI_RVM_SHAPE_MANUAL_20261018.md`).
