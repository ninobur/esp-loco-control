# NAVI unified authority: X22 concept integration, reconciliation plan

**Status: plan only. No firmware, test, dashboard, Pi, threshold or configuration
change has been made.** It is for David and Sam to review together. Implementation
waits for David's authorization.

- Build audited: `NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3`, branch
  `claude/navi-ps-r1-20q-standards` at `c1651ef` (the flashed build), variant
  folder `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/`.
- Line numbers refer to that commit. Unless marked `.ino`, they are in
  `ExcursionDetectorX22R.h`.

## 0. The decision this plan implements (operator, 2026-09-26)

> NAVI is the sole navigation authority. Hall and IR provide data. X22-derived
> concepts are tools available to NAVI; X22 is not an independent decision
> layer.

The test applied to every mechanism below is about **consequence, not
information**. X22 may compute and report anything. The question is whether it
uses that information to suppress, delay, alter or reinterpret an observation,
or to refuse a baseline opportunity, *because of an inference about movement,
distance, position, identity or trajectory*, before NAVI has judged it.

Three consequence classes are used:

- **Measurement:** establishes that an electrical Hall observation occurred at
  all. Appropriate in the measurement stage.
- **NAVI evidence:** reported for NAVI to weigh. Appropriate.
- **Navigation consequence inside X22:** suspect. It moves to NAVI, where it
  becomes either the preferred method or an explicit fallback.

For each mechanism the plan asks the operator's three questions:

- **A.** What does it provide?
- **B.** Is it useful when IR distance is unavailable?
- **C.** Does it currently withhold data or exercise authority before NAVI?

## 1. Evidence base

### 1.1 Field records, 2026-09-26 (committed, `field-records/`)

| Record | What it contributes | Contamination / interventions (as recorded) |
|---|---|---|
| run 1 (`verdicts/20260926_toby_20q3_run1.md`) | Hall-only → Epoch handoff; ±15% at 1.03/0.97/1.03. **X22R baseline never re-locked: 4 of 4 `Cadence` rejections at 4.0–4.5 s intervals, while the Epoch IR distance was judging the same MMs** | none reported |
| run 2 (`…run2.md`) | Recovery chain worked three times, including a mid-interval stop. **#22: an extra Hall opening exactly 402 ms after MM028, rejected by IR distance.** 6 locks accepted, 18 refused for `Cadence` | IR car tilted by hand after the stop over MM21 (IR contaminated after about 09:35:37); #30 probably contaminated |
| run 3 (`…run3.md`) | Two-MM identity error across a reversal, uncorrected by recovery. **#61 and #63: extra openings exactly 402 ms after an accepted MM, rejected by IR.** #65: threshold re-detection at 720 ms, rejected by IR | IR car tilted after the final stop only |
| run 4 (`…run4.md`, logs `run4a–d`) | 117/117 Hall events correct, 111 IR-judged within ±15%; AUTO with Patio and Bamboo stops; two obstruction stalls | Hand positioning before declaring; E-stop and clearing by hand; IR car moved by hand; flower pot; power cut, then train moved by hand |
| run 5 (`…run5.md`) | Declaration refused; 926/926 Hall poles consistent over five laps with no position; 922 locks accepted | IR coupling unconfirmed (display only) |

The operator's station-lap A/B comparison (IR coupled from 10:51:31) is **not in
the repository**. The uploaded `…100408_COMPLETE.log` ends at 10:42:40. That
capture is still owed if it is to be cited.

### 1.2 What the 20Q3 logs establish about baseline timing (new analysis)

Of the 1,067 locks accepted in runs 2–5, **1,064 were accepted 710–712 ms
after their opening**. The other three came at 713, 773 and 6,336 ms. The
711 ms is the sum of:

| Stage | Duration |
|---|---|
| acquisition window (N1) | 400 ms |
| CLEAR | 30 samples |
| SETTLE (N8) | 80 ms |
| collection | 200 samples |
| completion tick | 1 ms |

Consequences:

- The collection position is set by the clock, not by the magnet: signal-quiet
  CLEAR completed on its first opportunity in all but three cases.
- Estimating speed from the marker interval, the collection ended at a median
  of about 153 mm past the opening in run 5 (216 mm/s median) and 127 mm in
  run 4c (179 mm/s).
- In runs 4b, 4c and 5 (1,032 locks), accepted locks changed by |Δ| of
  1 count (median), at most 11 counts, from
  one lock to the next.

**This establishes that a region about 90–190 mm past the opening produced
stable locks at Toby's cruise speeds. It does not establish where the safe
region begins or ends, and it says nothing below about 120 mm/s.**

Method: the `diag/hall_decision` time `t` against the preceding `mm/marker`
`opened_ms`, with speed approximated as 300 mm ÷ the preceding opening interval.

### 1.3 Prior raw-waveform evidence (Otto, not Toby)

Sources: `docs/NAVI_BASELINE_TIMING_20260916_C3B93D0B.md`,
`docs/NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md`, `docs/HALL_WAVEFORM_VS_SPEED.md`.
The data is one hour of 1 kHz raw Hall from Otto (9950011) on 2026-09-16.

- **The passage is a spatial object.** Time scales as 1/v. The ≥70 region is
  27.6 mm wide (24.0–31.1), and the opening-to-apex distance is 13.4 mm.
- **The tail is a distance.** Tail as time has CV 0.142; tail as distance has
  CV 0.093, with p50 36 mm, p99 45 mm and maximum 71 mm, and
  corr(tail, speed) = −0.76. It is a return-flux shelf of 15–18 counts, not a
  decay, and it is location-specific (MM90; MM65–78).
- **There is a leading lobe.** The signal is last within ±15 counts 70 ms (p99)
  to 82 ms (max) before the next 70-count departure, at about 257 mm/s.
- **The 3000 ms cadence gate** was added as a **motion requirement**. It
  "removes the 41-count stationary lock and the 1 mm collections".

**None of this was measured on Toby.** Toby's sensor environment differs:
before 20Q3 it entered at 38 counts, where Otto entered at 70.

## 2. Current data flow (as flashed)

```
core 0, hallTask, 1 kHz (.ino:542-589)
  hallRead(): median of 5 ADC reads (.ino:202)
  ExcursionDetectorX22R::sample(now, raw, mayAdapt = actualPwm > 25,
                               stopped = observationHold (PWM 0), pwm)
    prime (2 s)                      -> no output until primed
    serviceBaseline()                -> lock (WaitGuard/CLEAR/SETTLE/CADENCE/MOVING/QUIET)
    acquiring_ (400 ms window)       -> openings SUPPRESSED            [N1]
    dwell_ / oldFieldHold_           -> openings SUPPRESSED            [N2, N3]
    rearm, 2-sample same-sign run    -> Detection (polarity at opening)
  takeDetection -> Judged -> judgedQ (serial assigned)
  window completed -> Judged{windowOnly} -> judgedQ (diagnostic)
  BaselineOutcome -> hallDecisionQ (diagnostic)

core 1, loop()
  judgedQ -> Navigator::judge(NavObservation{openedAtMs, polarity, odometry})
    valid same-epoch MM-referenced IR distance -> +/-15% window   (Navigator.h:105-112)
    otherwise                                  -> 650 ms Hall-only (Navigator.h:97-104)
  serviceTraversal() -> POSITION_ADVANCED_SANS_MM beyond 1.15D
  stationService() -> StationMachine on navMm
```

Every suppression happens on core 0, before a serial number is assigned. NAVI
never learns that those openings existed. The `nav/suppression` topic is
declared (`.ino:337`) but **never published**, so today's field records cannot
count suppressed openings.

## 3. Authority inventory

### 3.1 Measurement concepts (retained, in the measurement stage)

| Concept | Where | A: provides | B: useful without IR | C: current authority |
|---|---|---|---|---|
| Median of 5 conditioning | `.ino:202` | ADC noise rejection | yes | measurement |
| Prime | 1003–1009 | first lock (2 s, standing clear of magnets) | yes | measurement; relies on operator placement |
| Locked electrical baseline value | 450, 700 | the reference for departure | yes | measurement |
| ≥70-count departure | 453 | what counts as an excursion | yes | measurement |
| Two consecutive same-sign samples | 525–528 | transient screen | yes | measurement |
| Opening polarity | 553 | polarity with NAV authority | yes | measurement; window cannot revise it (840) |
| Electrical return / rearm | 458, 511–524 | one excursion yields one opening | yes | measurement: it defines an opening as a transition from inside to outside |
| CLEAR, QUIET (spread ≤32), interrupted-by-over, LEASH | 641–647, 671, 691, 693 | electrical quality of a candidate lock | yes | measurement |
| 400 ms window record, peak, sum, widths, disagreement | 770–905 | waveform evidence | yes, as evidence | diagnostic (`nav/hall_window`) |

### 3.2 Navigation consequences inside X22R (N1–N8)

| # | Mechanism | Where | What it currently does | Inference | Fate under the plan |
|---|---|---|---|---|---|
| N1 | 400 ms acquisition window | 467–482 | suppresses every opening within 400 ms of the last | "no new magnet within 400 ms" (time-based spacing) | suppression authority removed. Openings go to NAVI with a `within_window_of` fact. The window stays as a recording |
| N2 | PWM-zero hold ("dwell") | 447, 486–498; `.ino:1125` | suppresses every opening while applied and commanded PWM are 0 | PWM 0 ⇒ no movement ⇒ no new magnet | suppression removed. Openings go to NAVI with `pwm_zero_hold`. NAVI judges with IR; the fallback rule is in §5.3 |
| N3 | Old-field hold | 945–970 | after a hold that began displaced, suppresses openings until the signal returns to the lock | "sitting in a magnet already identified" (identity) | suppression removed. Fact `field_present_at_stop` / `began_displaced`. NAVI decides; fallback in §5.3 |
| N4 | Cadence ≤3000 ms | 655–658 | refuses lock collection | "running between two mapped markers, not a station" (motion) | removed from the IR path. Decision D6 for the fallback |
| N5 | MOVING (PWM >25) | 665; `.ino:555` | refuses lock collection | motion from PWM | IR path: motion from IR (D5). Fallback: PWM retained as fallback evidence |
| N6 | STOPPED (PWM 0) | 630 | aborts lock collection | motion from PWM | as N5 |
| N7 | Baseline cycle keyed to raw openings | 556–562, 633–639 | every opening, including ones NAVI rejects, restarts the cycle and the cadence clock; a MM advanced without a Hall landmark starts none | "every opening is mapped marker N" (identity) | cycle keyed to NAVI's world view: accepted landmarks and sans-MM advances |
| N8 | SETTLE 80 ms | 649–651 | delays collection 80 ms after CLEAR | **a proxy for clearance from the magnet's tail** (§4) | IR path: NAVI's distance region replaces it. Fallback: retained unchanged |

**Adjacent, directed by NAVI:** `reset()` (408–425) runs on declaration and
direction change (`.ino:190–194, 550`). It drops an undelivered detection,
forces the detector armed and clears the cadence history. NAVI makes that
decision, so it is appropriate in direction. Under the plan the measurement
stage should not force-arm: arming stays a signal fact (§5.1).

### 3.3 Dormant or inherited paths (compiled, no current effect)

| Item | Where | State | If enabled | Proposal |
|---|---|---|---|---|
| `lostMs` lock recovery | 250–262, 613–624; `HallObserver.h` sets `lostMs=0` | disabled | re-primes after 2 s over threshold at PWM ≥70: distance inferred from time and PWM ("≈480 mm") | keep disabled. A bad prime is currently unrecoverable except by reboot. A NAVI-side, distance-based equivalent is future work (D9) |
| `widthFloorMs` | 236, 899 | 0 = disabled | rejects by time-width, which depends on speed | leave disabled; width stays diagnostic |
| `adjustBaseline()` | 402 | no caller | second write path to the lock | leave uncalled |
| `MagnetRecognizer` 500 ms guard | `NAVI_ONE_X22/MagnetRecognizer.h:132, 239` | compiled only for `medianOfThree`; never instantiated | close-to-open time gate | no action; document as unreachable |
| `NAVI_GUARD_MS 500` | shared `LL_LocoConfig_9950012.h:143` | defined, unused by this sketch | — | shared header; leave, document as unused |
| `NGR_PHYSICAL_VMAX_MM_S = 0` | `LocoConfig.h:16`; `.ino:465–486` | `physical_timing: UNCONFIGURED` | time × vmax "too soon" exclusion | stays diagnostic-only and unconfigured |
| `staleMs` 120 s | 249 | reported only | — | keep as diagnostic |
| `Judged.priorGapMs`, `restartUncertain`, `zeroDuringCandidate`; `NavObservation.restartUncertain` | `.ino:69–72`; `Navigator.h:32` | never set (published `gap_ms` is always 0) | — | populate or remove (§7) |
| `nav/suppression` topic | `.ino:337` | declared, never published | — | superseded: nothing will be suppressed (§7) |

## 4. N8: provenance of the 80 ms SETTLE

The trace, from the start:

1. **2026-09-16, Otto raw capture** (`NAVI_BASELINE_TIMING…` §2). Measured from
   the close of a magnet's ≥70 span, the signal was within ±15 counts for 30 ms:
   ordinary running p99 69 ms, maximum 157 ms; station maximum 109 ms. The same
   document shows the tail is a **distance** (§4, CV 0.093 as distance against
   0.142 as time).
2. **`NAVI_BASELINE_DRIFT…` §7.** The guard after close moved from 40 ms to
   **80 ms**, because "40 ms leaves shelf in the window". The shelf is the
   magnet's return flux.
3. **NAVI_SIMPLIFIED / NAVI_ONE_SIMPLE** (`SimpleHall.h:94, 107`; README 22–24).
   The 80 ms starts after the 30 ms close confirmation, because "starting the
   timer at the last above-threshold reading put many windows into the magnetic
   tail".
4. **X22** (`3f06b72`, 2026-09-19; `docs/X22_LOCKED_BASELINE_IMPLEMENTATION_20260919.md:52`)
   adopted it as SETTLE, citing that replay finding.
5. **X22R** (`394c64d`) copied it unchanged.

**Finding: the 80 ms is not electrical or ADC settling.** Nothing in its
history refers to converter, filter or analog settling; the median of 5 runs
inside every 1 ms sample. It is a time proxy for spatial clearance of the
magnet's return-flux shelf, fitted on Otto at Otto's running speed (median
about 257 mm/s, where 80 ms ≈ 20 mm).

At Toby's slow running (about 70 mm/s in run 1) the same shelf lasts about 3.7
times as long. On today's evidence, however, SETTLE never decided where a Toby
collection sat: the 400 ms window (N1) already placed every collection far
beyond the tail (§1.2).

**Classification: navigation consequence (physical clearance by time), with a
legitimate purpose.** On the IR path, NAVI's measured-distance region replaces
it. On the Hall-only fallback path it is retained unchanged, as an inherited
fallback, not a new gate.

## 5. Proposed architecture

### 5.1 Where each concept lives

```
MEASUREMENT STAGE (core 0, 1 kHz; the retained X22 concepts, no navigation authority)
  median of 5 -> prime -> locked baseline value (written only at prime and on NAVI's accept)
  departure >= 70, two same-sign samples, opening polarity, electrical rearm
  conditioned-sample ring (raw, t, applied PWM), readable by NAVI
  per-opening FACTS (below); waveform window as a recording
  candidate-lock computation on request (median, spread, CLEAR/QUIET/interrupted)
  NO suppression except "not yet primed" and "not re-armed"
                 |
                 v  every opening, serial-numbered, with facts
NAVI (core 1)
  landmark judgment:   IR +/-15% (preferred) | 650 ms Hall-only (fallback) | PWM/field fallback rules (5.3)
  traversal / sans-MM: unchanged
  baseline policy:     eligibility by IR region (preferred) | inherited X22R timing (fallback)
                       accepts or refuses candidate locks and tells the measurement stage
  stations:            unchanged (navMm)
```

The measurement stage still runs at 1 kHz on core 0, because baseline samples
and openings need deterministic timing. **Authority is what moves, not the
thread.**

**Facts attached to every opening** (all already computable in X22R):
- applied and commanded PWM;
- `pwm_zero_hold` (N2 condition);
- `field_present_at_stop` / `began_displaced` (N3 condition);
- `within_window_of: <serial>` (N1 condition);
- time since the previous opening (raw, not previous accepted);
- lock value and lock age;
- departure at opening;
- `armed_since_return` (always true for a delivered opening; kept for
  reconstruction).

### 5.2 How NAVI gets the data previously hidden

| Hidden today by | Under the plan |
|---|---|
| N1 window | delivered with `within_window_of` |
| N2 PWM-zero hold | delivered with `pwm_zero_hold` |
| N3 old-field hold | delivered with `field_present_at_stop` |
| N7 raw-opening keying | NAVI's rejection is fed back: the baseline cycle follows NAVI, not raw openings |
| Baseline refusal reasons | still published on `diag/hall_decision`, now with NAVI's eligibility basis |

Queue load: more openings reach `judgedQ` (depth 16). Openings are still
bounded by electrical rearm, a return below 70 counts between any two. The
`hallEventDrops → haltForLoss()` rule stays. Sizing will be checked in host
tests (§8).

### 5.3 Landmark judgment

**Valid MM-referenced IR distance** (unchanged): the Epoch ±15% cumulative
window decides.
- An opening inside the window or at PWM 0 that is too early by distance is
  `NON_LANDMARK`, exactly as #22, #61, #63 and #65 were.
- An opening at PWM 0 whose IR distance fits the window is accepted: the train
  moved, whether it coasted or was pushed. See risk R4 and decision D3.

**No valid IR distance** (fallback):
- The **650 ms rule is unchanged**: open-to-open from the previous accepted
  landmark (`Navigator.h:102`). Because 650 > 400, every opening N1 used to
  hide is also refused by 650 when IR is unavailable, so Hall-only behavior at
  speed is unchanged.
- **PWM-zero fallback (proposed; decision D2):** without IR, an opening with
  `pwm_zero_hold` is recorded and not accepted as landmark progression. This
  preserves today's outcome. It becomes NAVI's explicit fallback rule instead
  of an upstream suppression, and IR evidence overrides it when present.
- **Field-present-at-stop fallback (proposed; decision D2):** without IR, an
  opening flagged `field_present_at_stop` is not accepted. This preserves the
  N3 outcome under NAVI's authority.

**No new time gate is created.** The two fallback rules use PWM and a signal
fact, both explicitly allowed as fallback evidence (principle 6), and both
reproduce existing outcomes.

### 5.4 Baseline establishment

**Trigger (N7 fix):** NAVI opens a baseline opportunity for the interval after
each **accepted** landmark, and after each sans-MM advance if D8 allows it. A
rejected opening does not reset the opportunity. It only interrupts a
collection in progress if its samples go over 70 counts, which is a measurement
fact.

**With valid IR** (preferred):
- NAVI grants an eligible region in same-epoch IR distance from the MM/IR
  synchronization: from `clear_after` past the accepted MM to `clear_before`
  short of the expected next MM, using mapped span geometry.
- The measurement stage collects when the current IR point lies inside the
  region **and** the signal gates pass (CLEAR 30 samples, collection 200
  samples, spread ≤32, not over).
- The candidate is returned to NAVI, which accepts it. The measurement stage
  then writes the lock (one write site, as now).
- No cadence and no PWM gate. Motion is IR pulses advancing during the
  collection; whether a stationary collection in a clear region is allowed is
  decision D5.

**The distances `clear_after` and `clear_before` are not authorized constants
and are not invented here:**
- Otto evidence: tail maximum 71 mm; leading lobe about 18–21 mm (p99/max) at
  257 mm/s.
- Toby evidence: 90–190 mm past the opening worked at cruise (§1.2).
- Toby's actual extents require the track experiment T1 (§9).
- Until T1, the choice is decision D4. Two options:
  - (a) adopt the field-proven Toby region as interim values;
  - (b) run the IR path in shadow and keep the fallback in control.

**Without IR** (fallback): today's X22R cycle **unchanged in timing and
gates**, but keyed to NAVI's accepted landmarks (N7):
- the window-end wait, CLEAR, SETTLE 80 ms, MOVING (PWM >25), STOPPED,
  CADENCE ≤3000 ms and QUIET;
- this is the timing behind the 1,067 locks accepted today (§1.2);
- whether CADENCE stays in the fallback is decision D6. It is the gate that
  refused slow-running refreshes, but without IR it is the only motion
  evidence besides PWM.

### 5.5 Preserved semantics

- **Epoch and MM reference:** the MM/IR synchronization is written only in
  `Navigator::acceptThrough()` at an accepted strike with a current-epoch IR
  point. An ended epoch never reopens. Traversal, the ±15% window, the sans-MM
  ruling and the ten-MM AUTO limit are all unchanged.
- **70 counts, two samples, opening polarity:** unchanged, moved as a unit into
  the measurement stage. The X22R equivalence tests pin them (§8).
- **650 ms:** unchanged in location, value and precondition (`travelUmAt`
  fails). Verified in the appendix: it is the only intentionally retained
  time-based landmark gate. N1 was a second, undocumented one, and this plan
  removes its authority.

## 6. Reversal and recovery

The unified architecture does **not** directly resolve run 3's mechanism: the
repeated-landmark expectation after reversing from a stop over a magnet, plus
PROXIMAL_R1's `TRAVEL_UNAVAILABLE_HOLD`. No X22 suppression caused it:
- the first detected magnet (MM022) was a real opening;
- NAVI mislabelled it through the reversal rule, with no IR reference after the
  reversal.

It stays a separate item for a **controlled retest** and is not changed by this
work.

## 7. Telemetry

**Survives unchanged:** `mm/marker`, `state/nav`, `nav/discrepancy`,
`nav/ir_compare`, `nav/hypotheses`, `diag/recovery`, `nav/hall_window`,
`nav/sans_mm`, `diag/hall_decision`, `diag/ir_health`, `telem/ir`, the status
line and `state/station`.

**Needed so NAVI's reasoning can be reconstructed:**
1. **Opening facts** (§5.1) on `nav/discrepancy`, per opening.
2. **Fallback basis on every ruling:** `IR_WINDOW`, `HALL_ONLY_650`,
   `PWM_ZERO_FALLBACK` or `FIELD_AT_STOP_FALLBACK`.
3. **Baseline decisions** carrying:
   - the basis (`IR_REGION` or `FALLBACK_TIMING`);
   - the region (IR µm from the synchronization, start and end);
   - the collection's IR travel;
   - the accepting ruling's serial.
4. **Shadow comparison for the first field build (recommended):** per opening
   and per baseline decision, the X22R verdict that *would* have applied
   (`x22r_would_suppress: N1|N2|N3`, `x22r_would_refuse: CADENCE|MOVING|…`).
   This makes the first track run a direct A/B against today's behavior.
5. `gap_ms` and `restartUncertain`: populate or remove; they are published but
   never set.

## 8. Host tests (deterministic)

- **Measurement-stage equivalence.** For every trace in
  `test_x22r_equivalence.cpp` (72) and `test_20q_standards.cpp` (6 detector
  cases), openings, times and polarities are identical wherever X22R did not
  suppress. Where it did, the opening is delivered with the correct fact.
- **N1.** A double-lobe trace with a second departure at 402 ms, and one inside
  400 ms, reaches NAVI:
  - with IR: rejected by distance;
  - without IR: rejected by 650 ms;
  - `lastAcceptedMs_` and the MM/IR reference are unchanged.
- **N2 and N3.**
  - An opening at PWM 0, and one after a displaced stop, is delivered.
  - Without IR: not accepted (same outcome as X22R).
  - With IR travel that fits the window: accepted.
  - With IR showing no travel: rejected.
- **N7.** A rejected opening does not restart the baseline opportunity or reset
  any cadence state. A sans-MM advance behaves as D8 decides.
- **Baseline, IR path.** Synthetic IR points drive the region. No collection
  outside it; accept and refuse under the signal gates; none across an epoch
  break.
- **Baseline, fallback.** It reproduces X22R's accept/refuse decisions and
  times (including the 711 ms) exactly, for traces with accepted landmarks.
- **Unchanged suites must pass unmodified:** `test_20q_standards.cpp` (2,394
  window, 242 traversal/limit and 6 Hall-only/epoch cases), `test_coherence`,
  `test_proximal_recovery`, the station tests, `test_ir_*` and
  `test_nav_decisional_inertia`.
- **Queue and ordering.** A burst of in-window openings cannot overflow
  `judgedQ`. The traversal ordering guard (`nextEventSerial ==
  handledEventSerial`) still holds.
- **Replay.** Today's logs carry NAVI-level events and IR points, not raw Hall.
  A NAVI-level replay (openings + IR points → rulings) must reproduce 117/117
  (run 4) and the other runs' rulings. A raw-Hall replay needs the Otto `.xhr`
  capture from the Pi, and a Toby capture from T1.

## 9. Questions that need track testing

| # | Question | Why a host test cannot answer it |
|---|---|---|
| T1 | Toby's tail and leading-lobe extents in mm (at cruise, slow running, station approach, the Grillers grade), recorded as raw 1 kHz Hall together with IR | the constants are physical and Toby-specific; the Otto figures do not transfer |
| T2 | Baseline stability under the IR region at slow running (the run 1 condition) | today it never re-locked there |
| T3 | Stops, including IR loss at the stop: do the PWM-zero and field-at-stop fallbacks reproduce today's outcomes? | IR contrast-loss behavior is physical |
| T4 | Coasting after PWM reaches 0 with valid IR | a new capability; the physical coast distance matters |
| T5 | Hand movement, and IR-car-only movement, under the new rules | contamination behavior (R4) |
| T6 | Reversal retest (separate, §6) | — |
| T7 | The same four-station AUTO lap as today, with the X22R shadow comparison | regression against field-proven behavior |

T1 needs an instrument: a raw Hall recording build, or streamed raw telemetry.
The X18 recorder and `tools/xhr_*.py` exist for Otto. Building one for Toby is
itself an implementation step that needs authorization.

## 10. Risks to field-proven 20Q3 behavior

| # | Risk | Mitigation |
|---|---|---|
| R1 | Openings N1/N2/N3 used to hide reach NAVI and are misjudged, especially at stops with IR lost | fallback rules reproduce today's outcomes (§5.3); host tests; shadow telemetry |
| R2 | Lock values change because collection position changes | fallback timing unchanged; IR path in shadow until T1 (D4b) |
| R3 | Core-0/core-1 coupling: the region grant and acceptance round trip, and ring-buffer reads | one write site, sequence-numbered handoff; tested for ordering |
| R4 | At PWM 0 with valid IR, NAVI now tracks hand movement. **IR-car-only movement (run 4, 10:00:37) could satisfy IR distance while Toby is stationary** | today it was harmless because the epoch ended, but more weight on IR at PWM 0 makes it matter more; decision D3 |
| R5 | More `judgedQ` traffic | bounded by rearm; tested |
| R6 | Scope creep into reversal and recovery | excluded (§6) |

## 11. Decisions required before implementation

- **D1** Adopt the architecture in §5.1: measurement stage with no suppression
  except prime and rearm; NAVI owns landmark and baseline authority.
- **D2** Make the NAVI fallback rules for PWM-zero and field-at-stop openings
  without IR (§5.3) preserve today's outcomes.
- **D3** At PWM 0 **with** valid IR travel, may NAVI accept a landmark (coast,
  hand push)? How should IR-car-only motion (R4) be treated?
- **D4** Before T1: (a) interim IR-region bounds from the field-proven Toby
  region, or (b) the IR region in shadow with the fallback in control.
  Recommended: **(b)**.
- **D5** On the IR path, may a lock be collected while stationary in a clear
  region, or must IR show motion during the collection?
- **D6** Does CADENCE ≤3000 ms remain in the Hall-only fallback?
- **D7** Is the shadow X22R comparison telemetry (§7.4) wanted for the first
  build?
- **D8** Does a sans-MM advance open a baseline opportunity?
- **D9** A NAVI-side, distance-based lost-lock test (bad prime) as future work,
  or leave `lostMs` disabled?
- **D10** Authorize the T1 instrument (raw Hall + IR recording on Toby).

---

## Appendix: whole-sketch time inventory

This covers every timer and elapsed-time threshold in the compiled set:
- the sketch folder's `.ino` and 13 headers;
- `NAVI_ONE_X22/MagnetRecognizer.h`;
- `IR_ARCHITECTURE_0_4/{IrOdometryEpoch,MmDistanceReference,IrInstrument}.h`;
- `common/IrMovement{Wire,Contract,Detector}.h`;
- `NAVI_SIMPLIFIED/LL_LocoConfig_9950012.h`.

`credentials.h` is git-ignored and absent. Classes: **A** legitimately
temporal; **B** physical fallback when IR is unavailable; **C** time surrogate
while IR is available; **D** diagnostic only.

| Item | Where | Threshold | Class | Note |
|---|---|---|---|---|
| Hall-only landmark fallback | `Navigator.h:102` | 650 ms open-to-open from last accepted (or declaration/direction change) | **B** | only when `travelUmAt` fails; adopted (decision 0093) |
| X22R acquisition window lockout | 467–482 | 400 ms | **C** | N1; undocumented second landmark-time gate |
| X22R SETTLE | 649–651 | 80 ms | **C** (fallback use B) | N8 |
| X22R CADENCE | 655–658 | 3000 ms | **C** | N4 |
| X22R lost-lock | 613–624 | 2000 ms | C (disabled) | `lostMs=0` |
| X22R width floor | 899 | 0 ms | C (disabled) | |
| X22R prime | 1005 | 2000 ms | A | instrument initialization |
| X22R shadow median, stale flag, pre-roll | 990, 249, 229 | 25 ms / 120 s / 512 ms | D | |
| Hall task period | `.ino:587` | 1 ms | A | sampling |
| IR link stale → epoch end | `IrHealthMonitor.h:12, 26` | 1 s | A | communication timeout |
| IR point ↔ Hall alignment | `IrHealthMonitor.h:12, 80, 93` | 150 ms | A | measurement correspondence (run 4 #151: `NO_IR_POINT` → fallback); nearest sample, not interpolated |
| MovementSource stale / alignment | `MovementEvidence.h:281, 290` | 1 s / 150 ms | D | `ir_reason` telemetry |
| Epoch time-order checks | `IrOdometryEpoch.h:92` etc. | ordering | A | integrity |
| IR speed qualification | `IrSpeedTelemetry.h:65, 91` | 3 s / 2 s | D | display only (`OBSERVE_ONLY`); the stall finding would use it |
| Hall speed estimate | `.ino:1154` | < 30 s | D | telemetry only |
| `physical_timing` (time × vmax) | `.ino:465–486` | vmax 0 | D (unconfigured) | |
| Station dwell | `Stations.h:151` | 5000 ms | A | (comment says 30 s; stale) |
| Station stop/depart steps | `Stations.h:152, 160` | 200 ms/count | A | actuation; the resulting stop position is open-loop |
| Station phase watchdog | `Stations.h:161` | 120 s | A | |
| Approach pacing | `Stations.h:182, 409–417`; `LL_LocoConfig:167` | marker-time table | A | actuation pacing with an embedded PWM→time model; no positional authority |
| Grade/curve step | `RouteMap.h:176` | 280 ms/count | A | |
| Manual/AUTO/brake ramps | `.ino:98, 107–112, 231–232, 281` | 150/62/31/15–400 ms | A | |
| INA, status, IR telemetry, IR health scheduling | `.ino:837, 885, 923, 869–871` | 5 s / 1 s / 1 s / 1 s–200 ms | A | |
| WiFi re-associate, MQTT connect | `.ino:634, 645–646` | 15 s / 3 s / 2 s | A | |
| IR optical detector (IR car firmware; compiled here, not executed) | `IrMovementDetector.h:17, 26, 55, 81` | 1.5 ms / 50 ms / 2.5 s | out of scope | runs on the IR transmitter; its 2.5 s stale rule shapes epochs at very low speed |

**Verification of the 650 ms statement.** 650 ms is the only *intentionally
retained* time-based landmark gate, and it applies only when valid
MM-referenced IR distance is unavailable (`Navigator.h:97–104`). The X22R 400 ms
window (N1) is a second, de facto time gate on landmark observations, contrary
to its own header ("cannot delay, reverse or reject"). This plan removes that
gate's authority.

Non-time proxies of the same kind (PWM for motion) exist outside X22R and are
noted, not changed:
- `observationHold` (`.ino:1125`);
- `admitDeclaration` "standing still" as PWM 0 (`Ops.h:152–156`).
