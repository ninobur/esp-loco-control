# NAVI_RVM_SHAPE_MANUAL: pattern-based STOP/GO resumption

2026-10-18 · Claude (Opus 5.5) · Development, compilation and offline testing only. **Not flashed.**

> **Scope.** This document answers the operator's assignment "Pattern-Based
> STOP/GO Resumption" (starting commit `2f75899`). It supersedes the
> resumption part of
> [`NAVI_RVM_SHAPE_MANUAL_STOPGO_20261018.md`](NAVI_RVM_SHAPE_MANUAL_STOPGO_20261018.md).
>
> **Evidence base.** Everything here comes from Oct 15 boot 1 and Otto.
> Boot 2 is untouched.
>
> **Unchanged.** No operational firmware, Pi service, MQTT or dashboard
> configuration was changed.

> **UPDATE: arrival mirror implemented** (approved by David 2026-10-18).
> §A below is current. §0–§6 record the state before it and remain as history.

## A. Arrival mirror (current candidate)

**Mechanism** (`RvmPause.h`; Python reference `continuation.pause_sequence`):

1. **Hold-back.** From the moment the operator **commands** a stop (target
   PWM < 15), the 20 ms blocks are held back instead of appended, up to the most
   recent **512** (10.2 s). Older ones are released in order.
2. **At STOP.** Applied PWM < 15: the stationary range is observed as before.
3. **At GO.** The held blocks are judged against that range, newest first:
   - blocks inside it, or single excursions, are stationary arrival repetitions
     and are **dropped**;
   - the walk stops at the first block that is outside the range together with
     the block before it on the same side, where the magnetic structure was
     still developing;
   - the rest is appended. Then the resumption rule (§1) applies.
4. **Withdrawn stop, or a declaration.** If the stop command is withdrawn before
   PWM < 15, or the operator declares, everything held is released unchanged,
   into the old context first.

Retraction is never needed: blocks are held before the recognizer sees them, so
no announced recognition is ever withdrawn.

**Why 512 blocks.** Otto's measured arrival plateau is up to 291 blocks
(median 153). A manual ramp at brake 0 from a PWM-35 stop is about 400 blocks.
512 is a buffer capacity, not a recognition criterion. It is a constant for you
to ratify.

**Results**

| Measured stop over a magnet (MM correct, of ~107) | Rising | Peak | Falling |
|---|---|---|---|
| `2f75899` (PWM < 15 only) | 0 | 0 | 8 |
| `fbb5b53` (resumption only) | 0 | 0 | 8 |
| **Current (resumption + arrival mirror), Otto's slow recorded arrivals** | **107** | **106** | **108** |
| Oracle (every truly stationary block removed) | 107 | 103 | 108 |

- **Required cases:** all 864 count the same magnets once, with the correct MM
  sequence and 0 CONTRADICTIONs. Midpoint within 2 blocks. C++ = Python in all
  1,260 runs.
- **Measured restart tail:** 141/144 (unchanged).
- **Unit checks** cover hold-back, withdrawn stop, capacity, arrival trim, and a
  bump with an arrival plateau of 150 or 290 blocks plus a stop and a restart
  tail.
- **Boot 1.**
  - Leave-one-pass-out: **1,063 / 1,081** recognized once, 0 split, 0 on
    baseline, polarity 100 %.
  - Misses: the 16 after the PWM-100 stall, plus MM114 and MM115, traversed
    while stopping at the very end of the recording with no GO and no
    declaration after them.
  - Parity: Python = C++, **1,092 = 1,092**, identical centres, block ids and
    polarity.
  - Navigation: 1,047 correct claims, 0 duplicates. The 31 station stops behave
    as before.
- **Build:** **982,135 B flash (74 %)**, **121,292 B static RAM (37 %)**, plus
  the 8 KB hold-back buffer on the heap at startup. The sketch halts with PWM 0
  if that allocation fails. The authority check passes: only `RvmPause.h` reads
  a block's PWM and commanded target.

**What changes for the operator**

- **Stopping speed.** Slow stops over a magnet now work in the offline tests;
  the quick-stop rule in §4 is no longer required. Premature resumption after
  very short stops still applies (§0.5): stay stopped about 6 s or more before
  GO over a magnet.
- **Delayed reports.**
  - While a stop is commanded, recognition is delayed up to 10 s; nothing is
    lost.
  - A magnet traversed while stopping is reported only after the train moves
    again or you declare. That is retrospective, as intended.
- **Not covered.**
  - A train still rolling after PWM falls below 15: rare, 1 of 35 recorded stops
    for 0.43 s. That part of the waveform is paused away, and at the peak or
    falling flank that magnet is lost.
  - A stall at running PWM (not a commanded stop): as before.

**Corrections made during this step (disclosed)**

- **Declaration handling.** The first version discarded held blocks at a
  declaration. That lost magnets traversed during the deceleration before a
  relocation stop (MM020–021, MM122–123 on boot 1). Fixed: they are released.
- **Python boundaries.** The Python reference could see one block past a
  declaration, and released held blocks at the end of data. The firmware does
  neither, so both were aligned to it.
- **Leave-one-pass-out harness.** It now computes the pause over the whole
  record, so a declaration at a fold end releases as on the locomotive.

---

**Candidate**

- Sketch: `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/NAVI_RVM_SHAPE_MANUAL.ino`
- Branch: `dev/navi-rvm-shape-manual-20261018`
- The commit hash is given in the session's final message. The branch HEAD is
  the candidate.

---

## 0. Answer first

**1. Implemented: pattern-based resumption.** After a commanded stop
(PWM < 15), PWM ≥ 15 no longer resumes the recognition waveform.

- **Reference.** While the train is stopped, the Hall blocks are a direct
  observation of the *stationary state* at that stop: its level and its own
  fluctuation. On Otto, PWM < 15 has always been stationary.
- **Resumption.** After GO, blocks that remain within that observed state are
  stationary repetitions and are withheld. The waveform resumes when the Hall
  tracing **leaves the stationary state and stays out on the same side in the
  next block**. It resumes from the first of those blocks and joins the
  preserved ensemble with no plateau.
- **Only magnetic evidence.** No PWM value, amplitude, duration, speed or
  elapsed time decides the resumption. Details are in §1.

**2. Verified offline.** The C++ firmware path equals the Python reference in
every run.

- **Required STOP/GO cases.** All 864 recognize the same magnets, counted
  exactly once, with the correct MM sequence and 0 CONTRADICTIONs. The cases
  are 18 real ensembles (CW and CCW), stops at the baseline, leading lobe,
  rising, peak, falling, trailing lobe and after completion, durations
  1 s – 30 min, and repeated STOP/GO.
- **Midpoint.** It stays within 2 blocks (40 ms).
- **Otto's measured restart** (stationary for 3.3–5.3 s at PWM ≥ 15): 141 of
  144 correct. It was 0 at commit `2f75899`.
- **Boot-1 replay.** Unchanged: 1,064 / 1,081 recognized once, 0 split, 0 false
  magnets. The C++ and Python results are identical (1,093 recognitions).

**3. But on Otto, restart was only half the problem.** Rebuilding stop-over-
magnet cases from Otto's **measured** station-stop tracks (IR odometer and PWM
from 30 recorded stops) showed something else:

- **Otto also stands still on arrival while PWM is still ≥ 15.** He stops at
  PWM 13–35 (median 26), and the ramp then takes seconds to reach 15.
- **That arrival plateau alone breaks the ensemble.** Even a perfect restart
  oracle scores 0/107 at the rising flank and peak and 8/108 at the falling
  flank.

| Measured stop over a magnet: MM correct, of about 107 per position | Rising | Peak | Falling |
|---|---|---|---|
| At `2f75899` (PWM < 15 resumes) | 0 | 0 | 8 |
| **This candidate**, Otto's slow (AUTO-like) arrival as recorded | 0 | 0 | 8 |
| **This candidate**, quick arrival (STOP command, 31 ms/count, about 17 blocks) | **107** | **106** | **108** |
| This candidate, arrival plateau up to 70 blocks (1.4 s) | 107 | 106 | 108 |
| This candidate, arrival plateau 140 blocks (2.8 s) | 67 | 69 | 72 |
| Oracle: every truly stationary block removed (IR; not implementable) | 107 | 103 | 108 |

**What this means for the first field test.**

- **Stop quickly, and the candidate handles stop over a magnet** as well as
  perfect knowledge. Stop quickly means the STOP command, or the brake slider at
  about 220 or more before throttle 0.
- **Quick stops don't make Otto coast.** At 6 genuine fast stops in boot 1, he
  rolled 0 mm after PWM fell below 15 in five, and 19 mm in one.
- **A slow stop over a magnet still loses that magnet.** That means dashboard
  throttle 0 with the brake slider low (400 ms/count).

**4. Not implemented, for your decision (§5).** The same stationary reference
applied backwards would trim the arrival repetitions; call it the arrival
mirror.

- **Offline it reaches oracle level** with Otto's slow recorded arrivals:
  rising 107, peak 106, falling 108.
- **It is a new mechanism, so I have not built it.** It must *retract* blocks
  the recognizer has already consumed. AGENTS.md §2 requires your approval for
  it.

**5. Remaining risk of the implemented rule: short pauses.** With a short pause
the stationary range is narrow, and Otto's real stationary fluctuation can step
outside it twice on the same side, resuming too early.

| PWM < 15 pause before GO | Premature resumption (stationary 3.3 / 5.3 / 8 s after GO) |
|---|---|
| 1 s | 26 % / 37 % / 43 % |
| 2.7 s | 8 % / 10 % / 14 % |
| 6 s | 1.2 % / 2.6 % / 3.2 % |

These rates are measured on Otto's real stationary Hall from 30 recorded stops
(`false_resumption.csv`).

**Build.** It compiles for Otto: **981,587 B flash (74 %)** and **121,276 B
static RAM (37 %)**. The recognizer takes 68,192 B and the pause state 48 B.
Navigation has no motor authority: the static check passes, and only
`RvmPause.h` reads a block's PWM.

---

## 1. The mechanism, and why it is magnetic evidence

`RvmPause.h` is the firmware; `continuation._rule` (need = 2) is the Python
reference.

**At STOP** (applied PWM < 15, your ruling):

- the waveform stops growing;
- the preceding baseline, the unfinished ensemble, the held candidate, the
  expected MM and the polarity are untouched, because nothing reaches the
  recognizer;
- raw Hall is still acquired and recorded.

Each STOPPED block also updates the **stationary reference**: the lowest and
highest Hall block seen while stopped at this stop.

**After GO** (PWM ≥ 15), each block is compared with that reference:

| Block | Treatment |
|---|---|
| Inside the observed range | A stationary repetition: withheld |
| Outside, but the next block is inside or on the other side | A fluctuation: withheld |
| Outside, and the next block outside **on the same side** | The magnetic structure has moved on: both are appended, and every later block too |

- **The join.** The waveform continues from the last pre-stop block to the
  first resumed block. Whatever the Hall did while standing is not in it.
- **Re-stop before resumption.** It is the same stop; the reference keeps
  accumulating.
- **Declaration.** It ends a stop. The stationary declared tracing is the
  startup baseline (unchanged rule).

**The question you asked: fluctuation versus continuation.**

- **The statistical shape model cannot answer it on its own.** It models whole
  ensembles and baselines, and has no model of a train standing still.
- **The new calculation compares the tracing with the stationary state observed
  at this very stop:**
  - the reference is measured, not chosen: Otto's own level and fluctuation
    while certainly standing still;
  - the departure must persist on one side, which is what continued motion
    through a field produces and noise does not.
- **The one design choice is "two consecutive blocks on the same side".** It is
  an operating point between two kinds of error. On 738 measured-stop cases:
  - *one* block resumed prematurely in 247 cases;
  - *two* blocks resumed prematurely in 0, and late by more than 5 blocks in 55.
    The late cases are slow motion near the extremum, where the tracing is flat
    anyway, so a few dropped blocks barely change the shape.

  **I flag this choice for your ratification.**

**What it does not use:**
- PWM beyond your STOP boundary;
- speed, distance or elapsed time;
- any fixed amplitude.

The window of stationary observations is the actual stop. The decision is
relative to that stop's own fluctuation.

**On the baseline** a moving and a standing tracing look the same. So after a
stop on the baseline, blocks are withheld until the next magnet's rise leaves
the range.
- **Effect:** the waveform skips some baseline. The pre-stop baseline remains,
  and the reference line is unchanged.
- **Tested:** the required suite (baseline and after-completion stops) and the
  boot-1 replay (31 station stops on baseline).
- **Result:** no miss and no duplicate.

## 2. Verification

### 2.1 Required STOP/GO cases (`s18_stopgo_suite.py`)

The stops use Otto's **real recorded stationary Hall**. Earlier suites used
Gaussian noise, and their noise estimate was inflated by a magnet (12 ADC
instead of 1.0); that is fixed (§6).

| Case | Cases | Same count | MM correct | CONTRADICTIONs |
|---|---|---|---|---|
| Uninterrupted | 18 | 18 | 18 | 0 |
| Stop at baseline, leading lobe, rising, peak, falling, trailing lobe, after completion; 1, 3, 5, 10, 30, 300 s | 756 | 756 | 756 | 0 |
| Long stop 30 min, peak and baseline | 36 | 36 | 36 | 0 |
| Repeated STOP/GO (3 × 2 s, 3 × 10 s, 5 × 1 s) in one ensemble | 54 | 54 | 54 | 0 |
| **All required cases** | **864** | **864** | **864** | **0** |

- **Midpoint.** In 61 of the 864 the centre moved by 2 blocks (40 ms). The
  block at the stop point has the same value as the stationary state, so it is
  withheld.
- **Following baseline and subsequent magnets.** Every following magnet in each
  stretch, 5 to 8 of them, was recognized with the correct MM.
- **Hall zero shift of 2, 5 or 10 ADC after a 30 s stop.** MM sequence correct
  in 108/108.
- **C++ equal to Python** in all 1,260 runs.

**Measured departure tail** (stationary for 3.3 s median and 5.3 s maximum at
PWM ≥ 15 after GO): 141 of 144 correct. The 3 failures are all at 5.3 s:
premature resumptions on real noise.

### 2.2 Otto's measured stops (`s19_realstop_suite.py`, `realstop.py`)

**How the cases are built.** Recorded magnet stretches are replayed along the
IR-measured position track of each real station stop, in real millimetres:
deceleration, the stop, then acceleration. Stationary blocks carry Otto's real
stationary fluctuation from that stop. IR serves only to build these cases and
the oracle.

The cases are 18 targets × 6 measured tracks × 7 stop positions (738 per
variant). The table in §0 shows the positions inside the ensemble.

**Stops on the baseline or after completion** fail at the same rate in every
variant, oracle included: 78/93 and 93/108. 26 of the 30 failures come from one
track (the 2,648 s stop). It begins with Otto pulling away from the previous
station stop, 24 s earlier, which lands on a magnet with a slow arrival. It is
the same arrival-plateau mechanism, and it is reported, not excluded.

### 2.3 Boot 1 (`s17_stopgo_replay.py`)

- **Leave-one-pass-out, Model A.**
  - 1,064 / 1,081 recognized once, 0 split, 0 on baseline, polarity 100 %.
  - The misses are unchanged: the 16 after the PWM-100 stall, and MM115 at the
    final manual reversal.
- **Parity.** Python = C++: 1,093 recognitions with identical waveform centres,
  block ids and polarity, LLR within 0.0003. The navigation events are
  identical (135 MM, 6 CONTRADICTIONs).
- **Navigation harness.** The 31 recorded operator stops give 30 correct on
  both sides; the exception follows a capture gap. 0 duplicate counts. Every
  break still has a known cause.

### 2.4 Software checks and build

- **Unit checks** (`unit.cpp`), all pass:
  - the pause states;
  - a single excursion does not resume;
  - an excursion followed by one on the other side does not resume;
  - two on the same side resume with both blocks appended;
  - a re-stop keeps the reference;
  - a declaration restarts;
  - a bump with 1 s and 30 s stops plus 3.3 s and 5.3 s stationary after GO is
    recognized once within 3 blocks of the uninterrupted centre.
- **Authority check.** Passes:
  - the recognizer, navigator and model contain no PWM, actuator, safety or
    network symbol;
  - `RvmPause.h` reads PWM once and touches nothing else;
  - the sketch never reads a block's PWM;
  - actuator writes exist only in the operator, E-STOP, low-voltage and
    startup paths.

| | |
|---|---|
| Program storage | 981,587 / 1,310,720 B (74 %) |
| Static RAM | 121,276 / 327,680 B (37 %) |
| Warnings | none (esp32:esp32 3.3.12, FQBN `esp32:esp32:esp32`) |

## 3. Diagnostics added (observation only)

`ngr/loco/9950011/diag/rvm_stopgo` is published on every transition and
captured by the Pi run logger (`ngr/#`):

- **STOP** (`STOPPED_WAVEFORM_PAUSED`):
  - last and expected MM;
  - whether a candidate ensemble is held (the unfinished ensemble);
  - the baseline start.
- **Resumption** (`PATTERN_RESUMED`):
  - the block time of the first resumed block;
  - the stationary range observed at the stop and its size in blocks;
  - the total number of blocks withheld after GO.

Unchanged: `diag/rvm_ensemble` carries the completed ensemble, and a
CONTRADICTION is reported in `state/nav` and `state/warning`. `state/nav`
continues to show the last established MM while an ensemble is unfinished.

## 4. Operating procedure for the first manual field test

1. **Stop quickly when the sensor car may be over a magnet.** Either:
   - publish `ngr/loco/9950011/cmd/stop`, which ramps down at 31 ms/count; or
   - set the brake slider to about 220–255 **before** throttle 0.

   With the brake slider at 0, throttle 0 ramps at 400 ms/count and that magnet
   will usually be lost.
2. **Before GO over a magnet, stay at PWM 0 for about 6 s or more.** This keeps
   premature resumption around 1–3 % (§0.5).
3. **Stops on the baseline need no special care.**
4. Everything else is unchanged from the STOPGO report §5: build, flash, boot
   checks, declaration, rollback.

## 5. Decisions for David

1. **The arrival side.** Approve or decline the mirror rule: at STOP, retract
   the most recent appended blocks that lie in the stationary range observed
   during the pause.
   - It needs the recognizer to undo recent pushes, possibly across an
     acceptance. That is a new mechanism, not built.
   - Offline it reaches oracle level for Otto's slow recorded arrivals.
   - Without it, rely on §4.1.
2. **The resumption operating point.** Ratify or change "two consecutive blocks
   on the same side" (§1).
3. **Short pauses.** Accept the measured premature-resumption rates, or tell me
   how you want short stops treated.

## 6. Corrections made during this work (disclosed)

- **Navigation harness ordering.** The harnesses ordered recognitions by their
  nominal decision index. A slowly traversed magnet with a wide window can have
  a later index than the next magnet, although the firmware accepts them in
  order.
  - Fixed: they now use acceptance order (`s11`, `s17`, `s19`).
  - Boot-1 numbers did not change.
- **Synthetic noise.** The idealized suite estimated its synthetic noise from a
  stretch containing a magnet (12 ADC instead of 1.0).
  - Fixed: robust estimate, and Otto's real stationary Hall for every stop.
  - The STOPGO report's commanded-stop invariance results are unaffected,
    because stationary blocks were skipped there.
- **Measured tracks.** The odometer's 9.65 mm pulses first produced staircase
  tracks.
  - Fixed: the tracks are interpolated between pulse instants, and stationary
    only inside the stop gap.
- **Python reference.** It now restarts at a declaration exactly as the C++
  does.
