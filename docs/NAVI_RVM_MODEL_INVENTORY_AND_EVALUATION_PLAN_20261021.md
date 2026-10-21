# NAVI_RVM: revision 5 freeze, model inventory, and whole-tracing evaluation plan

2026-10-21 · Claude (Opus 5.5) · Branch `agent/navi-rvm-gated-peer-20261020`

**Operator instruction (2026-10-21):**

> Freeze revision 5. NO more incremental fixes. … Do not begin by modifying
> firmware. First, identify the original model, the subsequent corrective
> changes, and their dependencies. Preserve revision 5 unchanged. Establish
> whether the October 16 whole-tracing experiment represents a genuinely
> different navigation model or merely another implementation of the existing
> one. Evaluate that model against the recorded evidence using causal replay,
> without changing its behavior to accommodate individual failures. Report the
> results, limitations, and comparison with revision 5 before proposing any
> firmware changes.

**No firmware has been changed by this work.**

---

## 1. Freeze

| | Commit | State |
|---|---|---|
| **Revision 5.1, FROZEN** | `5b7a518` | Flashed to Otto and Toby on 2026-10-20. This is the operational firmware. |
| Revision 6 | `2b2940c` | Built and **not flashed**. It changed only the follower's station hold (`RvmStations.h`). It is set aside and is not part of the comparison. |

Nothing is to be added to revision 5. Revision 5 is the comparison baseline.

## 2. The model family running today, and how it grew

### 2.1 The original model (NAVI_RVM_SHAPE_MANUAL, 2026-10-18)

Four independent parts:

1. **Tracing** (`RvmTracing.h`): 20 ms means of the native 5 kHz Hall samples.
2. **Recognizer** (`RvmShape.h`): a window classifier.
   - It scores every window ending now, at 19 widths (0.16–8.9 s), against two
     Gaussian-mixture shape classes, "magnet" and "anything else".
   - The features are 12 DCT coefficients of the window, normalized to its
     largest excursion, plus log(amplitude / noise).
   - The reference level is a least-squares line through the baseline since the
     last accepted window.
   - The best window with LLR > 0 is held, then accepted half a width later.
   - The model was trained on Otto's 2026-10-15 recording.
3. **Navigator** (`RvmNavigator.h`): counts accepted windows. A window whose
   polarity is the map polarity of the next MM advances the count; otherwise
   it is a CONTRADICTION and the navigator stops claiming.
4. **Stop handling**, which changed several times:
   - PWM < 15 pause, `RvmPause.h` (`2f75899`);
   - pattern-based resumption (`144e5ae`, `fbb5b533`);
   - arrival mirror (`30271a6`);
   - bounded feeding (`3a8953c`);
   - then **unified Hall continuity** (`RvmContinuity.h`, NAVI_RVM_CONTINUITY_MANUAL `8986c5f`). This replaced the PWM rules: a block is withheld if it is flat for 1 s and off the baseline.

### 2.2 Corrective changes since 2026-10-20

| Commit | Change | Why | Second-order effects |
|---|---|---|---|
| `93cb9ee` | 70-count excursion gate on candidate windows | Run 12 false 12-ADC ensemble | none seen |
| `119ad9b` r1 | AUTO, rainbow-carpet stations, bubble, ESP-NOW peers, Toby's continuity off | operator's 4 stations and two trains | Toby stopping over a magnet unprotected → MM064 CONTRADICTION |
| `1bf6454` r2 | dashboard GO topic, pairing, dynamic roles and echo, silence rule | dashboard GO never arrived; running singly | release at dwell end → follower nudged the leader |
| `125a55e` r3 (not flashed) | convoy wait, follower holds only behind a stopped leader, 2 s start | operator rulings | superseded |
| `97dcefb` r4 | follow-the-leader carpet, random starts, **open-ensemble continuity**, Toby continuity back on, follower **creep on the leader's PWM** | pause collision; MM141 | **Arches collision**: continuity froze Otto's position; the creep let Toby run into him |
| `df164d8`, `fc759b8` r5 | **PWM < 15 pause** before continuity; continuity reference only from recognizer ensembles; 25 ADC floor; creep removed; END AUTO | Arches collision | standing declarations never took effect → AUTO refused |
| `5b7a518` r5.1 | standing declaration takes effect at once | AUTO refused | 2026-10-21 CONTRADICTIONs at MM038 and MM049. A continuity reference error of more than 25 ADC withheld cruising baseline |

Each row repaired the previous row's side effect. That is the pattern the
operator named.

### 2.3 Dependencies inside revision 5.1

- **The tracing feeds everything.** PWM pause (< 15) → continuity → recognizer → navigator → station driver, bubble and peer messages.
- **Continuity depends on the recognizer.** Its reference level and noise are copied from the recognizer at every accepted ensemble. The recognizer depends on what continuity let through.
  - **This is a feedback loop.** A wrong reference withholds baseline. The recognizer then forms its next reference from the wrong blocks, and so on (2026-10-21, MM049).
- **The navigator depends only on accepted-window polarity.** One miss or one false window becomes a CONTRADICTION. There is no recovery.
- **The station driver and the bubble depend on the committed MM.** The bubble also depends on the peer's committed MM. A frozen or wrong count makes the two trains' roles invert (Arches collision).
- **The motor shifts the Hall reading.** Every level-based rule (continuity, the recognizer's baseline line, the excursion gate) is exposed to Hall levels that depend on the motor state, standing against moving (40–52 ADC), and to drift (10–130 ADC observed).

## 3. Is the 2026-10-16 experiment a different model?

**Yes. It is a different navigation model, not another implementation of the
same one.**

**Source:** `analysis/navi_rvm_replay_20261016/scripts/aligner.py`, identical to
the working package (sha256 `bc7e2f9b…`).

| | Revision 5.1 | 2026-10-16 aligner |
|---|---|---|
| Question asked | "is this window a magnet?", then count | "where on the railway does the whole tracing so far fit?" |
| State | an MM count | a continuous position in mm, every 40 ms |
| Context | the window and the baseline just before it | **the entire tracing since the declaration**, against a recorded **landscape of the whole track** on a 1 mm grid |
| Magnets | detected events with thresholds | not detected. They are part of the landscape being matched. Polarity is implicit in the landscape |
| Decision | per magnet, irreversible, CONTRADICTION ends it | **one best path** (dynamic programming, Viterbi). Later evidence can revise the past (fixed-lag backtrack). The live estimate is the current best end |
| Movement | not used (except the PWM pause) | soft expectation of speed from PWM history and, if present, IR speed. A Huber penalty, no veto |
| Stops | the continuity rule and the PWM pause | **while PWM = 0 the position holds** (ruling 0106). A stall is a path that does not advance |
| Thresholds per event | gate 70, LLR > 0, half-width wait, flat 1 s, 25 ADC | none per event. Costs accumulate over the whole path |

### Known properties that bear on the evaluation, stated before running it

1. **It matches absolute Hall levels to the template.** The local cost is
   `(hall − template) / sigma`, with sigma about 1.5 counts on baseline and a
   Huber cap. The templates are **Otto's**, from 2026-10-15 (CW lap 1 and
   CCW lap 1). Toby's different levels, and Otto's shift of about +100 on
   2026-10-21, were never in its templates.
2. **Movement priors come from Otto's 2026-10-15 template passes**, as a
   speed-by-PWM table. Toby and any later conditions are outside that.
3. **IR.** When IR speed is absent it contributes nothing (code path
   `np.isfinite(vir)`). The 2026-10-18 to 10-21 recordings have no IR. The model
   runs on Hall and PWM there.
4. **Provenance limitation.** The file differs from the hash in `FROZEN.json`.
   Its modification time is 23:45:15 local on 10-16, against the freeze at
   06:44:36 UTC on 10-17, so it was edited about 40 s after the freeze. Boot 2 was
   run after that. The frozen version is not recoverable. **The 10-16 report's
   "untouched holdout" is therefore not strictly established.** This
   evaluation uses `bc7e2f9b…` as "the 2026-10-16 model".

## 4. Evaluation plan (to be run unchanged on both models)

**Reference collection: every recorded run with native Hall in the Pi capture.**

- Otto: 2026-10-15 boot 1 (the template source, so in-sample for the aligner),
  boot 2, the 10-18 and 10-19 runs (including stall run 9, slow crawl run 11,
  run 12 CCW), and the 10-20 and 10-21 runs (stations, the Arches collision,
  MM141, MM038, MM049).
- Toby: run 12 and the 10-20 and 10-21 runs.

**Causal replay.**

- The aligner: its live estimate (best path end at each step), and a fixed lag
  of 1 s (revision 5.1 reports about 0.98 s after each ensemble).
- Revision 5.1: its C++ firmware path (`replay_open.cpp`, built against the
  frozen headers).
- Operator declarations are applied as recorded.

**Shared acceptance measures, independent of both models.** The existing
replay's "route" score aligns revision 5's own recognitions to the map, so it
cannot judge a model that has none.

1. **Operator truth points.** At every redeclaration, and at operator-reported
   positions (for example "actual 046–047" on 10-21), measure each model's
   position against the declared one, in MM.
2. **Raw-tracing check of every MM claim.**
   - A claim of MM k at time t is **supported** if the raw 20 ms tracing within
     ±1 s of t has its largest excursion of at least 70 counts from the local
     median, with the map polarity of k.
   - It is **contradicted** if that excursion has the opposite polarity.
   - It is **unsupported** if there is no such excursion.
3. **Continuity.** MM claims before the first failure: a CONTRADICTION for
   revision 5.1, or the first contradicted claim for the aligner. Also lost
   tracking per run.
4. **Behaviour at the field failures:** Arches 10-20 17:21, MM141 10-20, MM038
   and MM049 10-21, Toby MM064 10-20.

**No parameters will be tuned.** Both models run as they are. Failures are
reported, not fixed.
