# NAVI_RVM: causal comparison of revision 5.1 and the 2026-10-16 whole-tracing aligner

2026-10-21 · Claude (Opus 5.5) · Branch `agent/navi-rvm-gated-peer-20261020`

**Operator instruction:** "Run the causal evaluation of the recovered frozen
aligner and revision 5.1 on the full collection, unchanged, and report before
proposing anything."

**No firmware or model was changed.** This report proposes nothing. The plan
and the provenance check are in
[`NAVI_RVM_MODEL_INVENTORY_AND_EVALUATION_PLAN_20261021.md`](NAVI_RVM_MODEL_INVENTORY_AND_EVALUATION_PLAN_20261021.md).

## 0. Answer first

1. **Neither model is acceptable as it stands. They fail in opposite ways.**
2. **Revision 5.1 is accurate when it claims.** Of 2,473 MM claims checked
   against the raw tracing, **8 contradict it** (99.7% supported). But it
   **stops**: 22 CONTRADICTIONs across the collection, and on several
   recordings it stops early. After a CONTRADICTION it claims nothing.
3. **The 2026-10-16 aligner, unchanged, never stops.** It keeps a position
   throughout. Where conditions match its reference it is as accurate as 5.1
   **and covers more**:
   - Otto, CW, 10-15 to 10-18 clean runs: 99–100% supported, **without IR** on
     10-18;
   - on runs where 5.1 stopped after 1 claim (10-18 `8F9B71ED`), it made 68
     supported claims and none contradicted.

   Elsewhere it degrades badly, and it does not know it has:

   | Recording | Contradicted claims |
   |---|---|
   | Stalls and stops: 10-18 run 8 | 31 |
   | Stalls and stops: run 9 | 81 |
   | Stalls and stops: runs 10–11 | 48 |
   | **Toby, all runs** (Otto's landscape does not fit Toby's sensor) | 49 on run 12 |
   | Otto on 10-20 | 110 |
   | Otto on 10-21 | 16 of 22 |

   Over the whole collection, live: 2,696 supported, **510 contradicted**,
   674 unsupported.
4. **Why it degrades is visible in its design (§3).** It matches **absolute**
   Hall levels against **one** recorded lap of **Otto** from 10-15. It has no
   model of a shifted baseline (Otto's +100 on 10-21, Toby's different sensor),
   no notion of the motor-off Hall shift, and no confidence it can report.
5. **What the comparison establishes.** The whole-tracing approach is
   **viable in principle**: it holds identity through long runs with no event
   thresholds when the reference fits. The **2026-10-16 implementation is not
   viable unchanged.** Its reference and its level-matching are tied to one
   locomotive, one day and one sensor state.

## 1. Method

- **Models.**
  - **Revision 5.1:** the firmware navigation path at `5b7a518` (tracing,
    PWM pause, continuity, recognizer, navigator), compiled from that commit's
    headers through `replay_open.cpp`.
  - **The aligner:** the recovered frozen `aligner.py` (sha256 `f3d5230e…`)
    with the frozen templates `CW_L1` and `CCW_L1` and their speed models, run
    through `aligner.run(…, declarations='all')` as written.
- **Causal outputs only.** For 5.1: its committed MM claims; the firmware
  decides about 1 s after each ensemble. For the aligner: the **live**
  position (the best path end at each 40 ms step), and the **1 s fixed-lag**
  position, causal with a 1 s delay comparable to 5.1's. An aligner MM claim is
  the first step at which its position passes that MM's map coordinate, moving
  forward from the declared MM.
- **Inputs, identical for both:**
  - the native Hall, from the Pi capture `navi_sync_20261015_100403.nsr`;
  - applied PWM;
  - operator declarations: the first DECLARED event of each declaration in
    the run logs, placed on the locomotive clock with each boot's measured
    Pi offset;
  - IR only on 10-15. The 10-18 to 10-21 recordings have none.
- **Shared, model-independent check of every MM claim.**
  - A claim of MM k is **supported** if the raw 20 ms tracing within ±0.5 s
    of the claimed passage has its largest excursion at least 70 counts from
    the median of ±1.5 s, with the map polarity of k.
  - **Contradicted:** that excursion has the opposite polarity.
  - **Unsupported:** there is no such excursion (no magnet seen there).
- **Scoring defect found and fixed during the run.** The first scoring pass
  measured the aligner's progress from its first estimate instead of from the
  declared MM's map coordinate, which shifted every claim by up to half a
  spacing. It was corrected before the results below. The models were not
  touched.

## 2. Results, every navigated recording

Each model column gives supported / contradicted / unsupported claims. The
percentage is supported ÷ (supported + contradicted).

| Dataset | Loco | Notes | Seconds | r5.1 | r5.1 % | Aligner live | live % | Aligner 1 s lag | lag % |
|---|---|---|---|---|---|---|---|---|---|
| O_1015_boot1 | Otto | aligner template source (in-sample); IR present | 3842 | 135 / 0 / 0, C 6 | 100% | 936 / 14 / 163 | 99% | 932 / 9 / 171 | 99% |
| O_1015_boot2 | Otto | 10-16 holdout; IR present | 812 | 87 / 0 / 0, C 0 | 100% | 84 / 0 / 3 | 100% | 84 / 0 / 3 | 100% |
| O_1018_326C3574 | Otto | declared CCW, travelled CW (force CW as in the 10-19 harness) | 1663 | 222 / 0 / 0, C 1 | 100% | 55 / 51 / 8 | 52% | 41 / 20 / 53 | 67% |
| O_1018_05D95832 | Otto |  | 294 | 176 / 0 / 0, C 0 | 100% | 168 / 3 / 5 | 98% | 169 / 2 / 5 | 99% |
| O_1018_B05694E2 | Otto | run 3: stops over magnets | 685 | 83 / 3 / 0, C 0 | 97% | 63 / 3 / 20 | 95% | 56 / 3 / 26 | 95% |
| O_1018_8F9B71ED | Otto |  | 465 | 1 / 0 / 0, C 1 | 100% | 68 / 0 / 10 | 100% | 69 / 0 / 9 | 100% |
| O_1018_EFD0225A | Otto |  | 422 | 67 / 0 / 0, C 0 | 100% | 65 / 0 / 1 | 100% | 65 / 0 / 1 | 100% |
| O_1018_FA9D779D | Otto |  | 255 | 80 / 0 / 0, C 1 | 100% | 164 / 1 / 5 | 99% | 166 / 0 / 5 | 100% |
| O_1018_B29BDF0E | Otto | run 8: stalls | 920 | 98 / 1 / 0, C 1 | 99% | 89 / 31 / 92 | 74% | 87 / 8 / 117 | 92% |
| O_1019_RUN9 | Otto | run 9 stitched; 22 stalls | 884 | 1 / 0 / 0, C 1 | 100% | 101 / 81 / 57 | 55% | 117 / 41 / 81 | 74% |
| O_1019_D004D9A3 | Otto | runs 10-11: deliberate end stalls, slow crawl | 1243 | 0 / 0 / 0, C 1 | - | 51 / 48 / 53 | 52% | 39 / 6 / 107 | 87% |
| O_1019_3B1BE1D9 | Otto | run 12 CCW; MM101 false ensemble in field | 1854 | 4 / 0 / 0, C 1 | 100% | 502 / 64 / 180 | 89% | 466 / 80 / 200 | 85% |
| T_1019_6A31DD99 | Toby | run 12 Toby CW | 708 | 173 / 0 / 0, C 1 | 100% | 67 / 49 / 4 | 58% | 75 / 33 / 12 | 69% |
| O_1020_DD20D550 | Otto | MM141 CONTRADICTION in field | 3153 | 667 / 1 / 0, C 3 | 100% | 227 / 110 / 46 | 67% | 202 / 49 / 132 | 80% |
| O_1020_5CE2BC5F | Otto | Arches collision 17:21 | 1154 | 64 / 0 / 0, C 1 | 100% | 44 / 34 / 22 | 56% | 57 / 8 / 35 | 88% |
| O_1021_671EDD80 | Otto | MM038 and MM049 CONTRADICTIONs in field | 1541 | 168 / 1 / 0, C 2 | 99% | 5 / 16 / 1 | 24% | 1 / 10 / 11 | 9% |
| T_1020_E504D063 | Toby | MM064 CONTRADICTION in field | 3101 | 105 / 1 / 0, C 1 | 99% | 4 / 4 / 1 | 50% | 0 / 2 / 7 | 0% |
| T_1020_199278F9 | Toby | Arches collision (follower) | 1050 | 89 / 1 / 0, C 0 | 99% | 0 / 0 / 1 | - | 1 / 0 / 0 | 100% |
| T_1021_33A0700E | Toby |  | 1871 | 245 / 0 / 0, C 1 | 100% | 3 / 1 / 2 | 75% | 0 / 0 / 5 | - |

**Totals:**

| Model | Supported | Contradicted | Unsupported | Other |
|---|---|---|---|---|
| r5.1 | 2,465 | 8 | 0 | 22 CONTRADICTIONs |
| Aligner, live | 2,696 | 510 | 674 | — |
| Aligner, 1 s lag | 2,627 | 271 | 980 | — |

**Not navigated:** eight boots were never declared (bench and idle boots), and
two declared boots never moved (10-20 r1, the STOP_NOW episode).

### Reading the table honestly

- **5.1's small claim counts on some runs are replay artifacts, not field
  behaviour.** Examples: 10-19 run 12 (`3B1BE1D9`) 4 claims, against 623 in the
  field; runs 10–11; run 9. Those recordings lost native samples on the Pi
  relay. The firmware read the ADC locally and did not. **Both models saw the
  same gaps**, so the comparison is fair, but it understates 5.1 there. The
  aligner treats gaps as "no evidence" and carries on. 5.1 stops at its first
  resulting CONTRADICTION.
- **"Unsupported" is not "wrong".** It counts claims with no ≥70 excursion
  near them: a weak magnet, a gap, or a position that is off. It is large for
  the aligner on stall runs and on Toby, and larger still with the 1 s lag.
- **The raw-tracing check is itself a crude truth.** It sees polarity and size,
  not identity. A one-MM slip between two magnets of the same polarity passes
  it. Operator truth points (redeclarations) exist in only a few runs, and
  several followed the locomotive being moved by hand. They are in
  `results/scores.json` but not used for conclusions.
- **10-15 boot 1 is in-sample for the aligner** (its templates are laps of that
  boot). Boot 2 is the only 10-15 holdout: 84 / 0 / 3 for the aligner, and
  87 / 0 / 0 for 5.1.

## 3. Why each fails, from the evidence and the code

**Revision 5.1 stops instead of degrading.**

- Every failure is a local decision going the wrong way: a window accepted or
  missed, or baseline withheld or kept. The navigator then has no way back:
  CONTRADICTION ends the run.
- Its failures today and yesterday all trace to **Hall levels moving under
  it**: drift, motor-on against motor-off, a stale reference.

**The aligner degrades instead of stopping.**

- **It compares absolute ADC values with one recorded lap** (local cost
  `(hall − template) / sigma`, sigma about 1.5 counts on baseline).
  - When Otto's Hall reads 100 higher (10-21), or the sensor is Toby's, every
    position fits badly. The best path then wanders, which shows as
    contradicted and unsupported claims.
  - It has no term for a baseline offset or a gain.
- **Its movement prior is Otto's 10-15 speed-by-PWM table.** On stall runs the
  prior expects motion that is not happening, and the Hall evidence (flat)
  cannot resist it. Positions advance through stalls, which accounts for the
  contradicted claims on runs 8, 9 and 10–11. Holding still when PWM = 0 does
  not cover a stall with the motor on.
- **It reports no confidence.** It cannot say "I am lost". It just carries on.
  The October 16 report's success was on the day its templates came from.

## 4. What this does not establish

- **Field behaviour.** All results are replays. Pi relay gaps make them
  pessimistic for 5.1 on several runs.
- **Physical identity.** No independent survey of position exists. The check
  is polarity and size only.
- **CCW.** Only one CCW run (10-19 run 12), and that recording has relay gaps.
- **ESP32 feasibility of the aligner** (a 2,400-cell dynamic programme every
  40 ms) has not been measured.
- **Any variant of the aligner.** By instruction, none was run.

## 5. Decisions for the operator (no proposal made)

1. **Whether to continue with revision 5.1 or develop a whole-tracing model.**
   This evidence says the approach can work where its reference fits, and that
   the 10-16 implementation's reference does not generalize across
   locomotives, days or sensor states.
2. **If whole-tracing:** the operator decides the behaviour requirements first
   (how a model must handle level offsets, other locomotives, stalls with the
   motor on, and losing confidence). Every candidate is then run against this
   same collection and scoring (`analysis/navi_rvm_eval_20261021/`), with the
   results compared in full before any firmware work.

**Reproduce:** `evaluate.py prepare | r51 | align <dataset> | score`. The inputs
are listed in `collection.json`, built from the Pi capture and the run logs.

---

## Addendum: revision 5.1's baseline after a declaration conflicts with operating procedure (operator, 2026-10-21)

**Operating procedure (David):** after a stop he ends AUTO, places the
locomotive at **mid-interval, on baseline**, and declares there, **standing**.
The standing reading at that spot is the intended baseline.

**What revision 5.1 does instead:**

- r5's motor-off pause ignores every reading below PWM 15, so the standing
  mid-interval readings are discarded.
- The recognizer's first baseline after the declaration is taken **from the
  first seconds of motion**.
- Pulling away from mid-interval, those readings come from the approach to the
  next magnet, possibly inside its field.

**Field example, 2026-10-21:**

1. Declared at MM047 at 09:05:37.
2. Moving from 09:06:51.
3. MM048 was judged against a baseline of about 2024. Otto's cruising baseline
   was 1980–1995.
4. The continuity rule then withheld cruising as "off baseline" (more than
   25 off), which caused the MM049 CONTRADICTION.

David: "This method does not align physically with the way I am operating the
railroad."

**Origin:** a unilateral change in r5. It was made because both locomotives'
Hall reads 40–52 counts differently with the motor off than moving.

**No fix proposed.** How a standing declaration sets the baseline, given that
motor-off shift, is a behaviour requirement for the operator to define.
