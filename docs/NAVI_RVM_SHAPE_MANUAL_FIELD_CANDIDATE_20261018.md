# NAVI_RVM_SHAPE_MANUAL: field-test candidate for review

2026-10-18 · Claude (Opus 5.5) · Development, integration, compilation and offline testing only

> **Status.** This is a compilable candidate. It is **not field accepted** and
> **has not been flashed**. No locomotive, Pi service, MQTT or dashboard
> configuration was changed. No upload command exists in the build script.
>
> **Evidence base.** The evidence comes from one evening (2026-10-15), one
> locomotive (Otto) and boot 1 only. **Boot 2 was not used**: it remains
> reserved until David authorizes it.
>
> **Previous work.** This builds on
> [`NAVI_RVM_STATISTICAL_SHAPE_RECOGNITION_CLAUDE_20261018.md`](NAVI_RVM_STATISTICAL_SHAPE_RECOGNITION_CLAUDE_20261018.md)
> (research commit `696a7db`).

Sketch: [`firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/NAVI_RVM_SHAPE_MANUAL.ino`](../firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/NAVI_RVM_SHAPE_MANUAL.ino)

---

## 0. Answer first

**The local baseline preserves recognition, with one exception.** That
exception is the same physical situation as the main open risk.

Results on boot 1, Model A, leave-one-pass-out, with 1,081 passages that have
an extent:

| | Research (±10 s centred median) | **Local baseline (this build)** |
|---|---|---|
| Recognized exactly once | 1,081 | **1,065** |
| Missed | 0 | **16**, all in the 17 s after the 285 s stall over MM129 |
| Lobes counted separately | 0 | 0 |
| Baseline called a magnet | 0 | 0 |
| Principal polarity = map | 100 % | 100 % |
| Midpoint error, median / p95 / max | 10 / 40 / 100 ms | 15 / 50 / 170 ms |

**Why the 16 misses happen.** "The whole baseline since the last magnet"
absorbed 285 s of a displaced, stationary Hall level, about 80 ADC above the
running baseline. Otto had stalled at PWM 100 with the sensor on MM129's
field. The reference line therefore stayed far above the true baseline after
he moved on. I did not restore the centred median and I
did not add a compensating gate.

**The firmware is the offline model.** The sketch's own headers, compiled on
the host and fed the raw native recording, reproduce the Python reference
exactly:

- the same 192,123-block tracing;
- the same 1,097 recognitions (identical centres and polarity; LLR within
  0.0004);
- the same navigation events.

**It compiles for Otto's ESP32.** It uses **979,735 B flash (74 %)** and
**116,732 B static RAM (35 %)**. Navigation has no path to the motor. A static
check confirms this (§5.4).

**Main risk: stopping with the Hall sensor over a magnet** (§6.1). In a
labelled synthetic characterization, **any hold of 1 s or more inside an
ensemble means that magnet is not counted.**

- With manual control this is a likely situation, not a hypothetical one.
- The result is a CONTRADICTION when the next magnet's polarity differs.
- When the polarities agree, the count is silently one off until a polarity
  disagreement exposes it.

The real stall at 388–673 s is the one recorded instance. **Handling it needs
your decision.** I have not designed a mechanism.

**Recommendation.** Review the sketch. Before any powered run, decide §6.1.
The options are listed there. The minimum is a procedural rule: do not stop
with the sensor car over a magnet. Then run a bounded track test of exactly
that situation.

---

## 1. What was built

| File | Role |
|---|---|
| `NAVI_RVM_SHAPE_MANUAL.ino` | Manual controller (MANUAL_1_1 structure, Otto EWO_0_6 hardware rules) plus the observation-only RVM task |
| `RvmTracing.h` | 5 kHz native reads → 20 ms tracing (plain mean; gap = fewer than 50 reads or a serial jump) |
| `RvmShape.h` | Model A shape recognizer with the local baseline (mirror of `rvm_local.py`) |
| `RvmShapeModel.h` | Generated Model A parameters, from `model_A.json`, sha256 prefix `330cdc007da3445b` (the `MODEL_SHA256` constant) |
| `RvmNavigator.h` | Declaration, MM–BL–MM progression, polarity check, CONTRADICTION |
| `RouteMap.h` | Route polarity map, unchanged from NAVI_RVM (`ROUTE_POLARITY`). Spacing data is present but unused |
| `RvmOps.h` | Payload parsers copied unchanged from `Ops.h`, plus manual admissibility only |
| `NaviEstop.h` | Ordered E-STOP, unchanged |
| `NaviSyncRecorder.h`, `NaviWordStorage.h` | NSR1 Pi recorder. The IR and EWO-decision record paths are **removed**; all other wire formats are byte-identical |
| `LocoConfig.h`, `LL_LocoConfig_9950011.h` | Otto profile (unchanged), with an `#error` for any other locomotive |

**Base.** The controller layer comes from the NAVI_RVM field candidate
(`d18fe67`, derived from Otto EWO_0_6). It supplies:

- PWM GPIO4, direction GPIO16, Hall GPIO33, 20 kHz/8-bit PWM;
- INA219 on SDA21/SCL22;
- manual ramp at 150 ms/count;
- brake-controlled deceleration from 400 to 15 ms/count;
- STOP and low-voltage stop at 31 ms/count;
- low-voltage latch after five readings 5 s apart below 13.25 V, recovering at
  14 V;
- ordered E-STOP;
- 5 kHz Hall acquisition and the NSR1 UDP recorder.

**Not carried.** None of the following exist in this build:

- AUTO, GO, dispatcher go/stop/release, stations and the stop overlay;
- tile timing, DART, EWO, IR (ESP-NOW, pulse telemetry), startup lap;
- the earlier RVM alignment reference;
- the resource-diagnostic framework.

They are not gated; they are absent.

**Rollback.** No operational firmware was touched. The development branch
starts from research commit `696a7db`, which descends from `origin/main`
`94062cd`. Otto's currently flashed binary is not affected.

## 2. The local baseline, and why it is not circular

The principle came from David, and the implementation follows it. Code:
`RvmShape.h`; reference `scripts/rvm_local.py`.

1. **Startup.** The operator declares `start_interval` while stopped, with the
   sensor clear of a magnet. Recognition starts, and the baseline **BL₀** begins
   at the next 20 ms block. That baseline is the stationary tracing.
2. **Scoring a candidate.** Each candidate window (one ensemble with half its
   width of context on each side) is scored against **the baseline that
   precedes it**: every block from BL's start up to the window's first block.
   - **Level.** A least-squares line through *all* of BL, evaluated at the
     window centre. A gentle curve is followed by the line's slope.
   - **Noise.** The mean absolute 20 ms step over all of BL.
   - **No fixed-duration window** is used anywhere.
3. **Acceptance.** When an ensemble MM₁ is accepted, **BL₁ starts at the end of
   MM₁'s accepted window**. That is where the E class defines the return to
   baseline. Windows after it are re-scored against BL₁.

**No circularity.**

- A candidate's window must start at least two blocks after BL's start. So its
  own samples are never part of its own reference.
- BL₁'s start is fixed only by MM₁. MM₁ was recognized against BL₀, which was
  fixed by the declaration.
- The chain runs strictly backwards to the declared stationary baseline.
- "Completion" (acceptance) is the statistical best-explanation rule, unchanged
  from the research: the held window is accepted once the tracing has run half
  its own width past its end. It does not depend on the baseline that follows.

**Unchanged from the research recognizer:**

- the windows, shape representation, DCT basis and Gaussian-mixture classes;
- the LLR > 0 decision;
- the preference for the whole ensemble over its parts.

The model was re-fitted because the reference changed: the training windows use
the same local reference.

**No threshold detector or gate was added.**

**Retrospective.** The median decision comes 340 ms after the annotated
ensemble end (p95 1.0 s, max 2.9 s). The Hall sensor is in the first car, so
this is about a landmark already passed, as intended.

## 3. Navigation

Code: `RvmNavigator.h`. The navigator is observation only.

- **Declaration.** `start_interval AAA-BBB` with the session/motor direction:
  - CW: `last = AAA`, `next = BBB`;
  - CCW: `last = BBB`, `next = AAA`.
- **Recognized ensemble whose polarity = `ROUTE_POLARITY[next]`.** For Otto,
  North = ADC above baseline, checked on all 171 magnets. The piece
  `last → BL → next` is complete: `next` becomes the MM, and the following
  piece begins. Each recognized ensemble advances at most once.
- **Opposite polarity.** **CONTRADICTION** is reported with the expected MM,
  the expected and observed polarity, and the full ensemble record. No
  geographic claim is made until the operator declares again. The recognizer
  keeps running, so the Pi record continues.
- **Direction change while declared.** A change of motor direction or session
  direction, made at a standstill, gives **UNSET**: "declare the position
  again". See §6.2; this is a choice for you to review.
- **STOP/GO.** Nothing happens at a stop: no reset, no re-declaration, no PWM
  input. Recognizer, baseline and navigator state simply continue.

## 4. Interfaces

**MQTT subscriptions** (`ngr/loco/9950011/cmd/…`):
- `estop`, `throttle`, `direction`, `brake`, `stop`, `session_direction`,
  `start_interval`, `start_mm`;
- the fleet `ngr/dispatcher/cmd/estop`.

They are **not** subscribed: `auto`, `go`, `dispatcher_release`, dispatcher
`go`/`stop`, `ir_pair`, `ir_coupled`.

**Dashboard v1.11.2.** The dashboard STOP button sends `cmd/throttle 0`. It
needs no change, because:
- `state/nav` uses the existing fields;
- the MM tile renders when `"state"` is `TRACKING`;
- `CONTRADICTION` and `UNSET` render as "--";
- `state/nav_ready` and `state/start_interval` keep their existing meaning.

| Topic | When | Content |
|---|---|---|
| `state/nav` (retained) | 1 Hz and on every event | `{"state":"TRACKING\|CONTRADICTION\|UNSET","mm":last,"target":next,"dir",…,"event":"DECLARED\|MM\|CONTRADICTION\|SUSPENDED\|RECOGNIZED_NO_CLAIM","contradicted_mm",…,"recognized","advanced","contradictions","baseline_start_us","authority":"NAVI_RVM_SHAPE_OBSERVATION_ONLY"}` |
| `diag/rvm_ensemble` | every recognized ensemble | Result, MM, expected MM, expected and observed polarity, midpoint/start/end/decision/baseline-start times in µs, window width, LLR, amplitude, baseline level and noise |
| `mm/marker` | every advance | Same JSON (the dashboard uses it for marker freshness) |
| `state/warning` (retained) | CONTRADICTION, refusals, E-STOP, low voltage | Text |
| `state/loopstat` | 1 Hz | Build, model hash, PWM, safety flags, block/queue drops, recognizer windows, maximum push time, RVM stack, NSR drops, heap |
| `state/throttle`, `state/direction`, `state/estop`, `state/brake`, `state/lowvolt`, `state/session_direction`, `state/start_interval`, `state/start_mm` | as before | as before |
| `state/auto` | 1 Hz | Always "0": the build has no AUTO |
| `telem/voltage`, `telem/current`, `telem/power` | 5 s | INA219 readings |
| `state/bootid` (retained) | on connect | Sketch, class, `field_accepted:0`, `navigation:"OBSERVATION_ONLY"`, model hash |

**Pi recording.** NSR1 UDP goes to 192.168.68.142:47620, as before:
- native 5 kHz Hall (REC 5), legacy Hall batches (REC 1), status (REC 3);
- declarations and suspensions as consumption records (REC 6);
- every command received, command consumed, requested PWM and applied PWM
  (REC 7).

REC 2 (IR) and REC 4 (EWO decisions) are no longer produced. The run logger
subscribes to `ngr/#`, so `diag/rvm_ensemble` and `state/nav` are captured.
**None of this was exercised against the live Pi** (§7).

## 5. Offline verification

All of this is boot 1. The scripts are in
`analysis/navi_rvm_shape_20261018/scripts/` and the results in
`analysis/navi_rvm_shape_20261018/results/local/`.

### 5.1 Research reproduced, and the local baseline against it

`s10_local_experiment.py` uses the same eight folds, annotation and scoring as
the research (`s03`); the table in §0 shows the outcome.

- **MM050–MM070.** 130/130 passages with an extent were recognized once (MM
  number filter).
- **Breadth and amplitude.** No miss occurs outside the stall episode. The
  passages span the same 150–1,670 ms and 109–276 ADC as the research.
- **Unmatched recognitions.** 5, the same 5 magnet waveforms as in the research
  (before the first declaration, at 2,680.8 s, and in the 3,564 s obstruction
  episode). None lies on a frozen baseline.

### 5.2 Firmware equals reference

`s14_firmware_parity.py` → `parity.json`:

- the C++ tracing equals the Python tracing (same gap pattern, maximum
  difference 0.00012 ADC);
- 1,097 = 1,097 recognitions with identical centres and polarity, maximum LLR
  difference 0.00037;
- identical navigation events.

### 5.3 Geographic progression, startup, STOP/GO

The recorded declarations were replayed (`s11`, `s12`). The model was trained on
all of boot 1, so recognition here is in-sample. §5.1 is the honest recognition
measure.

- **Startup.**
  - Five of the six recorded declarations were followed by a correct first MM.
  - After the declaration at 104 s (`063-064` CW), the first magnet was North.
    MM064 is South, so the result was **CONTRADICTION**. The magnet after it is
    the one NAVI's old firmware labelled MM064.
  - So the declaration or the old labels were one magnet off. I cannot tell
    which.
- **Progression.** The harness re-declares after each CONTRADICTION from the
  annotation identity, as an operator would. It is not firmware behaviour.
  - It made 1,061 MM claims: 1,047 correct, 5 on magnets the annotation has no
    identity for, and **9 wrong**.
  - **Every wrong claim and every CONTRADICTION follows a specific cause**:
    - a magnet lost in a Pi-capture gap (15 distinct losses);
    - the MM129 stall (674 s);
    - the 3,564 s obstruction episode;
    - the 104 s declaration.
  - The 9 wrong claims are silent off-by-one counts after a gap-lost magnet
    whose neighbour has the same polarity.
- **STOP/GO.** Within tracking stretches there were 31 operator stops, all 5 s
  station dwells at PWM 0.
  - 30 are correct on both sides.
  - The exception (2,993.7 s) had MM016 lost in a capture gap before the stop.
  - **No passage was counted twice.**
  - No recorded stop lay inside an ensemble, so STOP-over-a-magnet is
    unexercised on real data (§6.1).
- **Post-stop baseline shift (the 388–673 s stall).** See §0 and
  `figures/causal_level_misses.png`.
  - Under the local baseline, MM129 is not counted.
  - In navigation, the next magnet then gives CONTRADICTION (expected 129,
    MM130 has the other polarity).
- **Irregular slow passages.** The activity at 184–188 s and 201–203 s, the
  139 s dip and the 3,598 s step produce **no recognitions**. No geographic
  claim is made there. Whether magnets were traversed is unknown.

### 5.4 Software checks

- **Unit checks.** `tests/navi_rvm_shape/unit.cpp`, run with
  `tests/navi_rvm_shape/run_checks.sh`, all pass:
  - navigator advance, CONTRADICTION latch, re-declaration, CCW seam (0→170)
    and suspension;
  - tracing mean, short-block gap, empty block, serial-jump gaps;
  - recognizer idle before declaration;
  - a flat noisy line gives 0 recognitions;
  - a bump gives one recognition with the right polarity, also when three
    times broader.
- **No motor authority.** `tests/navi_rvm_shape/authority_check.py` passes:
  - The recognition and navigation headers contain no actuator, PWM, E-STOP,
    MQTT or Arduino symbol in code.
  - In the `.ino`, `ledcWrite` is only in `writePwm`.
  - `writePwm` is only called from `serviceRamp`, `setup` and `haltStartup`.
  - `requestPwm` is only called from `handleCommand` (operator) and
    `serviceBattery`.
  - None of the 11 navigation/acquisition functions assigns motor, ramp or
    safety state or calls an actuator.

### 5.5 ESP32 build

Built with `tools/navi_rvm_shape/build.sh`, which compiles only:
- arduino-cli with esp32:esp32 3.3.12, FQBN `esp32:esp32:esp32`;
- PubSubClient 2.8, Adafruit INA219 1.2.3, BusIO 1.17.4;
- inert placeholder credentials.

| | |
|---|---|
| Program storage | **979,735 / 1,310,720 B (74 %)** |
| Static RAM | **116,732 / 327,680 B (35 %)**, of which the recognizer is 63,704 B (ELF symbol `recognizer`) |
| Warnings | none |
| Host timing (Apple silicon) | 22.8 µs per 20 ms block on average. The worst is 24.7 ms, during re-scoring after an acceptance |

**ESP32 timing is not measured.** My estimate is roughly 1–2 ms of
recognizer work per 20 ms block, and re-score bursts of a few hundred ms.
That work runs in its own task (core 0, priority 1, below the Hall task),
behind a 2.5 s block queue. `state/loopstat` reports the maximum push time,
queue drops and stack headroom, so the first powered run measures them.

## 6. Decisions needed from David (none acted on)

### 6.1 Stopping or stalling with the Hall sensor over a magnet

**Evidence.**
- **Real:** the 388–673 s stall. MM129 was not counted, the reference was
  poisoned for about 17 s, and navigation gave CONTRADICTION.
- **Synthetic characterization** (`s15_hold_characterization.py`). Real
  ensembles had the tracing frozen mid-ensemble:

| Hold | Held magnet counted, of 4 (rising flank / peak) | Next 5 magnets recognized, of 20 (rising flank / peak) |
|---|---|---|
| 0 s | 4 / 4 | 20 / 20 |
| 1 s | **0 / 0** | 20 / 20 |
| 5 s | 0 / 0 | 20 / 17 |
| 15 s | 0 / 0 | 20 / 15 |
| 30 s | 0 / 0 | 19 / 10 |

The ensemble class was learned from moving traversals. A level held inside an
ensemble is not a shape it explains. On a ~300 mm magnet spacing with ~92 mm
ensembles, roughly 30 % of arbitrary stops would leave the sensor over a field.

**Options.** I recommend nothing beyond option (a) without your ruling.

- **(a) Procedure plus evidence.** For the first field test, do not stop with
  the sensor car over a magnet. Add a bounded track test that stops over a
  known magnet for 1, 5 and 30 s and then departs, so real data replaces the
  synthetic hold.
- **(b) Hall-only stationarity.** Treat a flat stretch at a level displaced
  from BL as time in which nothing moved, and exclude it from the tracing the
  shape model sees. This needs a definition of "flat" and "displaced", i.e. a
  rule you would have to approve.
- **(c) Operator STOP state.** While the motor is commanded and applied at 0,
  exclude the blocks. This uses actuator state to shape the tracing, which you
  may regard as PWM use, and it would not cover the stall, where PWM was 100.

### 6.2 Direction change while declared

I made the conservative choice: UNSET, re-declare. The alternative is to swap
last and next and continue, which assumes the sensor is in baseline at the
reversal.

### 6.3 Other choices made in the implementation

These are for ratification. They are not filed as `docs/decisions` records,
because the numbering differs by branch; tell me the number to use.

- **Level fit.** The reference line is fitted by least squares through the
  whole baseline. One unrecognized ensemble inside BL biases it, which is part
  of the §6.1 mechanism.
- **Where BL₁ starts.** It starts at the accepted window's end, not the core
  end.
- **Commands kept.**
  - `cmd/stop` is kept as an independent operator STOP at 31 ms/count.
  - The fleet E-STOP subscription is kept.
  - MANUAL_1_1 had removed `cmd/stop`; your assignment asks for an independent
    STOP.
- **Recorder fork.** The Pi recorder was forked to drop the IR/EWO record
  paths rather than carry IR detector code.
- **Model training.** The deployable model is trained on all of boot 1. The
  leave-one-pass-out figures are the performance estimate.

## 7. Known limitations and unresolved issues

- **Narrow evidence base.** One evening, one locomotive, one sensor; boot 2 is
  unused. Not tested: Toby, weak or failing magnets (the weakest here was
  109 ADC), Hall-zero drift across days, rain or temperature.
- **Not exercised on hardware.** No ESP32 timing, heap endurance, Wi-Fi
  coexistence or live MQTT/Pi delivery. The dashboard compatibility is by
  reading v1.11.2's handler, not a live test.
- **Pi-capture gaps fall on magnets** (19 of 21 gap-lost openings). They broke
  every navigation replay segment. On the locomotive the 20 ms tracing is
  local, so the recognizer does not see those gaps. The recorder's 24-batch
  native ring (about 230 ms) is unchanged, and the Pi copy can still have them.
- **Silent off-by-one.** A missed magnet whose successor has the same polarity
  gives wrong MM claims until the next polarity disagreement. The architecture
  intends this; it has no recovery.
- **Long baselines.** The baseline line fit accumulates in double precision
  from BL start. Numerically safe for about 1 hour of uninterrupted baseline;
  multi-hour stationary baselines are untested.
- **Weak magnets.** The amplitude feature is relative to baseline noise. A much
  weaker magnet than any on Oct 15 (below about 25 % of the amplitude) is
  unlikely to be recognized (research §7).

## 8. Architecture-to-code reconciliation

| Requirement | Where | Status |
|---|---|---|
| General composite Model A, two families accommodated | `RvmShapeModel.h` (E: 6 components, N: 8), `RvmShape.h` | Done |
| Complete ensemble with lobes and transitions; isolated spikes discounted | E class = whole ensemble ± half width; best-explanation hold | Done; 0 splits, 0 baseline recognitions |
| Midpoint and principal polarity reported | `diag/rvm_ensemble` | Done |
| Local baseline replaces ±10 s median; no fixed window | `Recognizer::score` / `p_` sums from BL start | Done; §0 failure case reported |
| Startup baseline from stationary declared tracing | `startBaseline()` on declaration | Done |
| STOP/GO preserves history; no double count | No reset path at stops | Done on 31 recorded stops; **stop-over-magnet fails** (§6.1) |
| MM–BL–MM, map supplies polarity and sequence | `RvmNavigator.h` + `ROUTE_POLARITY` | Done |
| CONTRADICTION: report, preserve, no claims until re-declare | `NavState::Contradiction`, warning, NSR1 recorder continues | Done |
| No PWM, IR, speed, distance or arrival in recognition | Recognizer inputs are Hall block + route direction | Done (static check) |
| Navigation has no motor authority | §5.4 | Done |
| No AUTO, stations, CTO, dispatcher, recovery; no dormant DART/EWO/QUORUM/old RVM | §1 | Done |
| Manual controls, ramps, brake, STOP, E-STOP, battery preserved | `.ino` controller section | Ported unchanged in rule; not bench-tested |
| Hall 20 ms tracing, MQTT, dashboard MM, Pi recording | §4 | Implemented; not live-tested |
| Reserved boot 2 untouched | n/a | Untouched |
