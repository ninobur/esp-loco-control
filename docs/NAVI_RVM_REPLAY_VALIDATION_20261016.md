# NAVI_RVM — Replay validation on the October 15 recordings

2026-10-16 · Claude (Opus 5.5) · Research only

> **Status.** This report is research evidence, not a recommendation to
> implement. Nothing here changes firmware, maps, station-control
> instructions, Pi services or MQTT. No device was flashed or operated. The
> October 15 sources were read only.
>
> It follows David's answers
> ([`NAVI_RVM_OPERATOR_ANSWERS_20261016.md`](NAVI_RVM_OPERATOR_ANSWERS_20261016.md)),
> which supersede parts of the first proposal
> ([`NAVI_RVM_MAGNETIC_LANDSCAPE_RECOGNITION_RESEARCH_20261016.md`](NAVI_RVM_MAGNETIC_LANDSCAPE_RECOGNITION_RESEARCH_20261016.md)):
>
> - no hard PWM bound and no PWM "leg";
> - no IR annotations in the map;
> - no ±150 mm boundary;
> - ensemble boundaries are to be discovered from data.
>
> **This is replay evidence from one evening and one locomotive (Otto).** Per
> AGENTS.md §8 it shows what the software does on recorded passes. It does not
> establish railway behavior. The architecture is **not validated** for
> implementation. §8 lists what remains.

---

## 0. Answer first

A movement-constrained alignment of the continuous Hall signal against a
recorded reference landscape kept the correct magnet identity in every
evaluated case below, with frozen parameters. It used no detector openings
and no trackside anchors, and it held one trusted history.

- **1,187 NAVI-selected openings** were used **only for scoring**:
  - boot 1: 1,100, of which 758 lie outside the reference laps;
  - boot 2: 87, a holdout never run before the parameters were frozen.
- **Retrospective identity (4 s lag): 0 wrong** among them. The median
  position error was **3.2 mm** (p95 10 mm, max 39 mm) out-of-sample, and
  **2.8 mm** on the holdout.
- **The live (zero-lag) estimate was one magnet ahead once.** That was at
  3564 s, during the reported obstruction episode. The retrospective reading
  corrected it.
- **Continuity through NAVI's two withdrawals without redeclaration:** with
  the 021-022 and 123-124 redeclarations ignored, there were still 0 wrong
  identities. In both cases the aligner's position just before your
  redeclaration was already inside the interval you declared.
- **Magnetic ensembles were discovered from two independent passes**, without
  using the detector or magnet spacing:
  - there are 171 ensembles for 171 magnets;
  - widths are 74–149 mm (median 92 mm);
  - 170 of them are multi-part (main lobe plus reproducible opposite-sign
    flanks).
- **"Normal" was learned from one clean lap.** Only 7 seconds of the whole
  session fall outside it: the 3564 s obstruction resume and the unmodeled
  manual reversal at the end.
- **Physical disruptions were recognized from movement context.** A
  stationarity statistic (PWM expects motion, the aligned path does not
  advance) found four stall/obstruction episodes. The longest was 283 s. The
  clean reference lap had none.

**What is not yet demonstrated:**

- a geographic judgment layer (WATCH/STOP rules) running on these outputs;
- recovery from operator relocation without a declaration;
- cross-day stability;
- Toby;
- an ESP32 timing measurement;
- any real missed magnet.

The development history matters too. The aligner was improved iteratively on
boot 1 before freezing (§2). The out-of-sample boot 1 numbers are therefore
not untouched evidence. Only boot 2 is.

---

## 1. What was built (research code)

Package: `~/NGR/NGR-Files/navi-rvm-replay-20261016/`. The scripts,
`FROZEN.json` and the result JSON are copied into
[`analysis/navi_rvm_replay_20261016/`](../analysis/navi_rvm_replay_20261016/).

| Piece | What it does |
|---|---|
| `blocks.py` | 10 ms blocks of **every native Hall sample** (mean/min/max/count), PWM, direction, gap flag. Gaps stay empty. Nothing is interpolated. |
| `context.py` | 40 ms steps. IR odometer interpolated between Type-5 snapshots. Map positions are taken from the surveyed edge lengths (MM0–MM170, lap 52,264 mm). Declarations are kept. NAVI selections are exported **for scoring only**. |
| `template.py` | **Reference landscape**: one recorded pass placed on a 1 mm grid of the map coordinate. Placement uses that pass's IR odometer, scaled so the lap equals the surveyed 52,264 mm, and one anchor label at the pass start. Templates: CW lap 1, CW lap 2, CCW lap 1. **The geographic map is not changed.** The template is a recording of the landscape, not a map edit. |
| `speed_model.py` | **Otto's own history**: IR speed conditioned on PWM bin (5 units) and ramp state (rising / steady / falling over 0.5 s), learned from the template pass only. |
| `aligner.py` | Online constrained DTW (Viterbi). One exported incumbent and a fixed-lag retrospective backtrack. Details in §3. |
| `experiments*.py` | Frozen-parameter experiments. `score.py` compares positions with NAVI openings **after** alignment. |

**What the aligner reads:**

- 40 ms Hall step means;
- PWM (commanded; carried through recording gaps);
- the IR odometer;
- the operator's declarations;
- the reference landscape and speed model.

**What it never reads:** NAVI openings, detector decisions, trackside
sensors, or anything from the future of the step being processed. The
fixed-lag view uses up to 4 s of following data, and is labelled
*retrospective*.

---

## 2. Development history (disclosed in full)

Before the freeze, the first versions failed badly. Each fix below was made
on boot 1 in response to a diagnosed physical cause, not to make a count come
out. They are listed because they are findings in their own right.

| # | Symptom on boot 1 | Diagnosed cause | Change |
|---|---|---|---|
| D1 | hundreds of one-magnet slips | Beam pruning discarded the correct path during low-information stretches. A 2 mm grid quantized speed to 50 mm/s steps | 1 mm grid; no beam prune inside the window |
| D2 | slips at every slow station approach | **Otto does not move below about PWM 30–35 on a rising ramp, and decelerates in a band of its own.** A linear PWM→speed model pushed the alignment ahead by 1–2 spacings. With alternating polarity, a **two-magnet slip is Hall-consistent** | Speed model learned per PWM bin and ramp state from the template pass |
| D3 | a one-magnet loss at 287 s | A 1 s recording gap made PWM read −1, which was treated as PWM 0, so the alignment froze while Otto moved at 280 mm/s | Missing PWM is not PWM 0: carry the last recorded command. Hall stays empty |
| D4 | stalls not held; magnets "crossed" cheaply | Noise model too loose. Measured lap-to-lap variability is ~0.5 counts on ordinary track and ~2.5 mm position jitter on flanks | σ(x) = √(1.5² + (2.5 mm·slope)² + (0.02·deviation)²) |
| D5 | the 3555 s obstruction pushed the path ~200 mm ahead | PWM penalty was uncapped while IR's was capped, so PWM effectively vetoed IR | **Both capped at the same level** (answer 6: neither vetoes). Hall decides where they disagree |
| D6 | session start (063-064) aligned one or more magnets ahead | **The Hall record shows Otto started ~100 mm before MM63.** The first excursion is MM63's positive lobe, and NAVI correctly selected MM64 next. A declaration did not mean "already past MM a" | Initial span = the declared interval extended back by one spacing |

Parameters were then frozen (`FROZEN.json`, with file hashes, before boot 2
was run). No parameter changed after the freeze.

---

## 3. The aligner as tested

- **State:** the cost of each 1 mm cell in a window of 2,400 mm, in progress
  coordinates. CCW progress is the negated map coordinate.
- **Each 40 ms step:**
  - advance by 0–30 mm, which is 0–750 mm/s. This is a computational bound,
    about 2.5× Otto's top speed, and never the binding constraint;
  - add the movement-context cost;
  - add the Hall cost at the new cell.
- **Movement context:**
  - **PWM:** the median and spread of Otto's recorded speed at this PWM bin
    and ramp state. Huber penalty, capped.
  - **IR:** smoothed odometer speed. Huber penalty, capped at the same level.
  - **PWM 0:** freeze (0106). Observations are recorded but not aligned.
- **Hall:** robust (Huber) residual against the reference, scaled by the
  measured σ(x). Gaps contribute nothing.
- **Outputs:**
  - *live incumbent*: the argmin now;
  - *retrospective*: backtrack from the incumbent 4 s later;
  - local cost, path advance, and live jumps.

---

## 4. Results against the eight requirements

| # | Requirement | Evidence | Verdict |
|---|---|---|---|
| 1 | Continuity despite speed variation | 758 out-of-sample + 87 holdout openings, PWM up to 111. Strata: PWM <55 median 1.8 mm; 55–84 2.4; 85–104 4.5; ≥105 5.1. **0 wrong** in every stratum. CW 3.0 mm, CCW 4.0 mm | **Demonstrated in replay** (one evening) |
| 2 | Continuous Hall, not detector openings | The aligner reads only Hall step means. Openings are used only for scoring and for the one anchor label per template pass | **Demonstrated** |
| 3 | Value of multi-lobe ensembles | 170/171 ensembles are multi-part. A main-lobe-only template still gives 0 retrospective errors, but **doubles live instability** (165 vs 81 live jumps >60 mm), and live wrong identities rise 1 → 2 | **Partially.** The flanks stabilize the live reading. Retrospective identity did not need them on this evening |
| 4 | PWM history as mandatory context | The learned PWM-history model was the largest single fix (D2). Ablations: with IR present, removing PWM leaves 0 retrospective errors (live wrong 1 → 3). **Without IR**, PWM vs nothing gives 2 vs 3 retrospective errors and 155 vs 711 live jumps. Uncapped PWM creates an error at the obstruction (3564 s) | **Demonstrated as context.** PWM is decisive when IR is absent and harmful if made absolute, consistent with answer 6 |
| 5 | Inconsistent IR tolerated | Odometer scaled ×0.95/×0.90/×0.80 (synthetic, Hall untouched): **0 wrong** at every scale. Real shortfall edges (CW019→020, CW121→122 and reverse): aligner errors −11 to +9 mm, where NAVI's IR intervals were −30 to −59 mm. With IR removed: 2 wrong | **Demonstrated in replay** |
| 6 | Ordinary discrepancies vs genuine disruptions | Ordinary: speed variation, station stops, recording gaps (50 % of landmarks masked → 0 wrong, median 5 mm on masked), IR shortfall. None changed identity or exceeded learned normal cost. Disruptions: 4 stall/obstruction episodes flagged by unexpected stationarity; the obstruction resume and the manual reversal exceed learned normal cost; the two declarations after relocation disagree with the aligner by 1.5–3 spacings | **Partially.** Signals separate the classes, but **no judgment layer was run**, and recovery from an undeclared relocation fails (§5.3) |
| 7 | One established history, no alternative search | One incumbent is exported, in a local window. No global search was ever run. Live argmin jumps: 81 >60 mm in 93,479 steps, 68 within 2 s of a stop or IR halt. **The retrospective history had 0 identity errors** | **Demonstrated for the committed (retrospective) history.** Live jumps must be treated as discrepancies (WATCH), never as identity changes |
| 8 | No trackside ESP sensors | None used | **Demonstrated** |

---

## 5. Findings on David's specific questions

### 5.1 Ensemble boundaries, discovered (answer 1)

Method:

1. Take two independent CW passes on the same coordinate.
2. Mark points where both deviate from the local ordinary level (rolling
   median over 3 m) by more than *k* × the lap-to-lap noise (0.69 counts),
   with the same sign.
3. Group those points into runs.

The gaps between runs then fall into two families, with almost nothing in
between:

| gap between reproducible structures | k = 6 count |
|---|---|
| 0–25 mm | 222 |
| 25–150 mm | 6 |
| 150–300 mm | 166 |

The valley between 25 and 150 mm is the **empirical ensemble boundary**.
Merging across gaps under 25–100 mm gives the same result:

- **171 ensembles**, after joining MM0, which the coordinate wrap splits;
- each contains exactly one NAVI-selected opening;
- widths 74 / 92 / 149 mm (5 / 50 / 95 %);
- **170 multi-part**, up to 3 parts each.

The reproducible opposite-sign flanks found in the first report's MM1/81/87/90
are therefore **the norm, not the exception**. The 4 "two-excursion" magnets
were most likely just the ones whose flanks crossed a fixed baseline band (not checked individually).

### 5.2 Search width, measured (answer 1, separate question)

- Live vs retrospective position at openings: median 1 mm, p99 5 mm, max
  291 mm (one case: the 3564 s obstruction).
- Window sensitivity:
  - **800 mm and 1,200 mm:** tracking identical to 2,400 mm everywhere
    *except* the session-start case (D6), which failed in both.
  - **500 mm:** fails broadly (115 wrong).

So ordinary tracking needs a window of about ±400 mm around the incumbent.
Initialization from a declaration that does not match the physical start
needs more, and is **fragile** (§8).

### 5.3 Continuity and relocation

- Honoring only the declarations that follow PWM-0 handling or reversal
  (063-064, 071-072, 121-122, 143-144) still gives **0 wrong**. RVM did not
  need the two post-withdrawal redeclarations.
- With *only the first* declaration, continuity **breaks at 276 s**. Otto was
  handled back across MM72 at PWM 0 and re-passed it. The aligner cannot
  move backwards, so it explained the repeated MM72 as a magnet about 900 mm
  ahead and stayed wrong thereafter.
  - This relocation is a genuine disruption. Under 0106 it requires a
    declaration.
  - The replay shows it is *visible*: a +918 mm live jump with elevated cost.
    It does **not** show that a judgment layer would turn that into STOP or
    WATCH. That layer was not built.
- Declarations versus the aligner's last moving position:
  - **071-072:** inside the declared span (8 mm before MM72).
  - **021-022:** inside the declared span.
  - **123-124:** inside the declared span.
  - **121-122** (after the CCW reversal): about 600 mm away.
  - **143-144** (after the 3555 s obstruction): about 700–985 mm away.

  The last two are consistent with Otto being moved back. Your recollection
  would settle it. After 143-144 the Hall record shows MM143 re-passed at
  3688 s.

### 5.4 Atypical, learned empirically (answer 5)

- The 1 s sums of local alignment cost on CW lap 2 (393 s) define normal:
  median 6.6, p99 22, max 68.
- Across the session, **7 seconds exceed the lap-2 maximum**: 3564 s (the
  obstruction resume) and 3788–3796 s (the manual reversal, not modeled).
- No station stop, recording gap, IR shortfall or speed change reached that
  level.
- Stalls are a *movement-context* discrepancy, not a pattern one. While
  stalled, the Hall signal still matches the reference at Otto's position. A
  second empirical statistic therefore flags stalls: "PWM expects >50 mm/s,
  aligned path advances <10 mm in 1 s". Lap 2 had **0** such seconds. The
  session had four episodes:

| Episode (s) | Length | Context |
|---|---|---|
| 139–163 | 25 s | PWM 110 just after MM72, Hall flat (before the PWM-0 handling) |
| 389–671 | 283 s | the Grillers stall (PWM 90, Hall flat at ~1973 on a magnet) |
| 3057–3080 | 24 s | PWM 50 between CCW MM161 and MM160; the aligner held and identified MM160 afterwards |
| 3557–3561 | 5 s | the 3555 s obstruction (wind/bag reported that evening; correspondence not established) |

None of these changed the committed identity, apart from the relocation cases
above.

---

## 6. ESP32 requirements (estimates, not measured)

Configuration: W = 800 cells at 1 mm, a 30-cell advance range, 25 steps/s.

- **Compute:** about 25 k compare-add operations plus 800 Hall residuals per
  step, roughly 1 M operations/s, which is **a few percent of one 240 MHz
  core**.
- **RAM:**

| Item | Size |
|---|---|
| Cost vector | 3.2 KB |
| Fixed-lag backpointers, 4 s at 1 B/cell | 80 KB |
| Fixed-lag backpointers, 2 s or packed | 20–40 KB |

- **Flash:**

| Item | Size |
|---|---|
| Two direction templates, int16 | 209 KB |
| σ(x), uint8 | +105 KB |
| Speed model | <1 KB |

- **Unmeasured:** heap headroom in the actual NAVI build and coexistence
  with Hall sampling. A bench measurement needs your authorization to flash a
  device.

---

## 7. Changes from the first proposal

- PWM is no longer a hard bound or a "demonstration leg". It is Otto's
  recorded speed for this PWM history, as a capped soft cost, equal in weight
  to IR (answer 6).
- IR is never written into the map. Its shortfalls simply cost little
  (answer 4).
- Ensemble width and search width were measured, not assumed (answer 1).
- "Atypical" is relative to one clean lap's distribution (answer 5).
- Recording gaps never cause STOP and nothing is synthesized. Masking 50 % of
  landmarks still gave 0 wrong identities (answer 3).

---

## 8. Not demonstrated / limits

1. **One evening, one locomotive.** The templates and speed model come from
   laps of the same evening as the test passes. **Cross-day stability is
   untested.** Toby is untested.
2. **Development on boot 1** (§2). Only boot 2 (87 openings, CW only) is a
   clean holdout.
3. **Scoring relies on NAVI-selected identities.** They are conditional
   labels, supported by lap closure and repeatability, but not physically
   proven.
4. **No geographic judgment layer was run.** WATCH, resolution and STOP were
   not replayed. Live jumps (81) exist and must not be read as identity
   changes.
5. **Undeclared relocation is not recovered.** That is acceptable under 0106,
   but the detection-to-STOP path is unbuilt.
6. **Start initialization is fragile** when a declaration does not match the
   physical start (D6). It failed with 800 mm and 1,200 mm windows.
7. **The manual reversal** (3778.7 s, `cmd/direction 0`) is not modeled and is
   excluded from scoring.
8. **No real missed magnet** exists in the record. The rule for one remains
   unexercised.
9. **CCW out-of-sample** covers only the running after the reference CCW lap
   (187 openings).

---

## 9. Proposed next research steps (not started)

- **Cross-day replay:** apply the October 15 templates and speed model
  unchanged to another evening's native recording (e.g. October 13). This is
  the most important missing evidence.
- **Judgment-layer replay:** explicit rules over these outputs (live jump,
  learned atypical cost, unexpected stationarity, declaration disagreement)
  → ADVANCE / WATCH / HOLD / STOP. Score it against the known episodes.
- **ESP32 bench timing,** only with your authorization.
- **Toby.**

## 10. Questions for David

1. **Declarations.** Does `start_interval a-b` assert that the Hall sensor is
   past magnet *a*'s field, or only that *b* is the next magnet to be
   confirmed? The October 15 start (063-064, sensor about 100 mm before MM63)
   suggests the latter.
2. **121-122 at 2732.8 s and 143-144 at 3668.5 s:** was Otto moved back by
   hand? The aligner places him 1.5–3 spacings further along.
3. **Live jumps.** Should a live jump that the retrospective path does not
   confirm be recorded as WATCH (the current interpretation)?
