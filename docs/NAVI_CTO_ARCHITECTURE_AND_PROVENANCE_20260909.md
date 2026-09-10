# NAVI_CTO — architecture and provenance report

**Date:** 2026-09-09
**Branch:** `agent/toby-1-13-flash`
**Status: proposal. No firmware written, no image built, no hardware touched.**
This report is deliverable 1–13 of the NAVI_CTO brief. It ends with the
questions that must be answered before Stage 1 can be implemented.

---

## The architectural statement

> **NAVI is the sole navigation authority. CTO consumes NAVI position and
> occupied-track truth but cannot create, correct, score, or replace it. CTO is
> distributed between locomotives over ESP-NOW. The Dispatcher assigns and
> observes service through MQTT but is not the real-time traffic controller.
> CTO may reduce motion authority; it may never manufacture movement
> authority.**

This statement goes at the top of `NAVI_CTO.ino` when that file is created, and
at the top of every design document in this lineage.

---

## 0. What changed while this report was being written

Two findings from the 2026-09-09 Otto dataset are load-bearing and appear in
full in §3. Stated here because they change the shape of the plan:

1. **Otto's phantom population is effectively gone, but not entirely.** At the
   38-count survey aperture that would have exposed it, the 2026-08-20 cluster
   at peaks 40–91 does not reappear. In 4,090 waveforms the entire
   sub-threshold population tops out at **33 counts**. There is exactly **one**
   spurious accepted event in ~3,100 steady-state passages.
2. **`NAVI_APPROACH_MARKER_MS` still cannot be fitted.** The survey ran at a
   held PWM 90. The five approach throttles have 0–2 samples each. This is the
   same gap that blocked 2026-09-04, and the 2026-09-09 session did not close
   it. **Stage 1 needs one more short measurement run, or an operator ruling.**

---

## 1. Exact NAVI base commit and files

**Baseline commit** `7d163fa0de4337d41036f9d4de4b5c192afae24f`
*"firmware: shape is recorded, not refused (decision 0074, proposed) —
NAVI_ONE 1.0X13, recorder 0.3"* — verified present, reachable from this branch.

**Base tree** `firmware/test-programs/NAVI_ONE/`, firmware identity
`NAVI_ONE_1_0X13_FIELDTEST` (`NAVI_ONE.ino:95`).

| file | lines | role in NAVI_CTO |
|---|---:|---|
| `HallCapture.h` | 1065 | acquisition. **Unmodified.** |
| `MagnetRecognizer.h` | 368 | recognition. **Unmodified.** |
| `TwoSided.h` | 510 | stitched-passage judgement. **Unmodified.** |
| `Navigator.h` | 295 | position authority. **Unmodified** except a `const` accessor block (§5). |
| `Stations.h` | 371 | `StationMachine`, per-direction stopping points. **Policy inputs only** (§7). |
| `RouteMap.h` | 200 | route, section cruise. **Unmodified.** |
| `WaveformWindow.h` / `WaveformDump.h` | 198 | diagnostics. **Unmodified.** |
| `Ops.h` | 177 | operator command surface. Extended with CTO commands (§9). |
| `LocoConfig.h` | 70 | profile selector + the three compile guards. **Unmodified — the guards stay.** |
| `NAVI_ONE.ino` | 1386 | main sketch → becomes `NAVI_CTO.ino`. |
| `tests/` | 19 files | inherited whole and must keep passing (§12). |

**Behavioural reference, not lineage:** `firmware/test-programs/NAVI_ONE_STATION_CURVES/`
(`NAVI_ONE_STATION_CURVES_0_3`) — the image that actually flew on 2026-09-04.
Same acquisition, recognition, navigation, station and motor behaviour as X13,
plus unconditional per-passage diagnostic publication
(`publishCurveMeta()` / `publishWaveformSlot()`, `.ino:876-884`). Its diagnostic
publication may be carried into early NAVI_CTO field-test builds. It does not
redefine the production lineage.

**The field result this preserves:** Toby, 2026-09-04, ~2 h both directions —
3,284 accepted magnets, 76 completed station stops, zero false positions, zero
navigation shutdowns, zero transport loss.

---

## 2. Exact CTO donor commits and functions

Donors, all verified present:

| commit | subject | what is taken |
|---|---|---|
| `8d426a6` | QUORUM 1.16R: the CODEX review round — all seven findings accepted | the Bubble foundation |
| `31c4898` | docs: 0035 Accepted — CODEX approves 1.16R for supervised field testing | the acceptance evidence |
| `26df0fe` | QUORUM 1.16Ra: radio instrumentation — measure the transmitter, name the channel | send-completion callbacks, channel watch |
| `55716a5` | QUORUM 1.16Rb: decision 0037 — a pairing dissolves when nose-tail order inverts | order-inversion dissolution |
| `8122ed5` | CE: Circuit Express as a station-service instruction (decision 0038) | mission layer |
| `02aeb3d` | CE round 2: fix both lifecycle defects CODEX found, and add the tests | lifecycle fixes |
| `6742d98` | CE: gate the mission lifecycle on position, not just the gap test | position gating |
| `ca27f1c` | CE: end the mission on re-pairing, not on a gap number (P1, CODEX) | completion rule |
| `86e3539` | CE: show EXPRESS/LOCAL in the CTO row instead of "unpaired" | status telemetry |

Source presently at `firmware/QUORUM/QUORUM.ino` (5,783 lines,
`SKETCH_NAME "QUORUM_1_16R_IR_TEST_A"`).

**Donor functions and state, by line, all adapted rather than copied wholesale:**

| donor | lines | disposition |
|---|---|---|
| `CtoPeerPacket`, `Cto3RoleEcho`, magics/versions | 556–584 | **copied byte-identically. Frozen. §8.** |
| `CtoTrafficPhase`, `CtoRole` enums | 586–587 | copied, values frozen |
| spacing/pairing tunables (`CTO_*_MARKERS`, intervals, stale) | 589–620 | carried as **provisional** with provenance (§6) |
| `CtoPeer` registry + freshness | 623–640 | adapted |
| radio instrumentation counters (`ctoTxDone/Failed`, channel watch) | 647–698 | adapted whole (Stage 2) |
| `ctoRadioInit()`, recv callback → `ctoRxQueue`, send-completion cb | 4980–5000 | adapted whole |
| `ctoAcceptPeer()`, `ctoAcceptEcho()` | 5041–5090 | adapted; validation preserved |
| `ctoLimitPwm()`, `ctoDesiredPwm` continuous re-application | 2860–2880, 1186 | **the pattern is adopted (§6) — this is the donor's best idea** |
| `ctoService()`, `ctoDissolve()`, `ctoHandleClear()`, `ctoRefusalReason()`, `ctoDwellMs()` | 1186–1199 | adapted |
| `ceBegin()`, `ceEnd()`, `ceMissionName()`, `ceCruisePwm()`, CE state | 1125–1199 | adapted (Stage 5) |
| `tests/test_cto_roles.py`, `test_ce_mission.py`, `test_channel_watch.py` | — | **inherited as donor evidence** |

**Not taken from the donor:** `serviceStations()`, and everything in §4.

---

## 3. Otto and Toby model derivation plan

### 3.1 Datasets, and the cutoff

**Toby** — `field-records/logs/20260828_survey/`, 2,092 waveform records over
twelve laps, collector `QUORUM_1_13X`, entry threshold 38. Toby's block is
already fitted, already flown, and **is not re-fitted**. The evidence chain
(records → `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md` → the constants
in `LL_LocoConfig_9950012.h` → commit `03d2945`) is preserved unbroken. Nothing
in this work re-fits Toby to make him resemble Otto.

**Otto** — `field-records/logs/20260909_survey/` only. Same collector
`QUORUM_1_13X`, **same entry threshold 38**, same PWM 90 manual drive. Same
instrument, same aperture, two locomotives — which is the only way the two
blocks stay comparable, and was the explicit reason for opening Otto's gate for
the survey (`OTTO_ON_NAVI_ONE_STEP1_REPORT_20260904.md` §10).

**The hardware-consistency cutoff is absolute.** Nothing before the
**21:04:17 boot on 2026-09-09** enters training, fitting, validation, threshold
selection, reported performance, or examples. The three earlier CW boots, the
stall and the NO_QUORUM stretch were collected on the faulty INA219 state and
are excluded — they are not in this directory at all.

**Verified against the dataset's own README, independently:**

| | CW | CCW |
|---|---:|---:|
| `mm/wave` | 1,481 ✓ | 2,609 ✓ |
| `mm/marker` | 1,084 ✓ | 2,083 ✓ |
| admitted `rej:0` | 1,083 ✓ | 2,081 ✓ |
| floor-rejected `rej:1` | 2 ✓ | 1 ✓ |
| sub-threshold `rej:2` | 396 ✓ | 527 ✓ |
| `tr:1` | 24 ✓ | 24 ✓ |
| `clip:1` | 1 ✓ | 5 ✓ |

Combined 4,090. Every published figure reproduces. **Quality rules honoured:**
`tr:1` may inform timing, amplitude, duration and jitter but never shape;
`clip:1` is never an ordinary clean shape example; the first CCW record
(`mm:46`, `dur:33762`, `pwm:0`) is the direction-switch dwell and is excluded
from every duration analysis; `RAMP` and `LOW_PWM` records are startup, not
steady-state cadence.

### 3.2 What is fittable from this data, and what is not

The nine symbols Otto's NAVI profile lacks, against the 2026-09-09 dataset:

| # | symbol | fittable now? | evidence |
|---|---|---|---|
| 1 | `NAVI_RECOGNIZER_MEASURED_ON` | **yes** — becomes a true attestation | this work |
| 2 | `NAVI_GUARD_MS` | **yes** | §3.4 |
| 3 | `NAVI_AMPLITUDE_FLOOR` | **yes** | §3.5 |
| 4 | `NAVI_BOOTSTRAP_GAIN` | **yes** | §3.5 |
| 5 | `NAVI_RESIDUAL_CEILING` | **n/a — diagnostic reference** | decision 0074 made the residual unable to refuse. Otto takes Toby's `0.13f` **labelled as a diagnostic reference, not a threshold**, so his shape diagnostics are directly comparable with Toby's. This is the one value where copying is correct. |
| 6 | `NAVI_AUTO_CRUISE_PWM` | policy — `90` is the surveyed regime | operator |
| 7 | `NAVI_APPROACH_MARKER_MS` | **NO — see §3.6** | blocking |
| 8 | `NAVI_BASELINE_ADAPT_PWM` | **NO — see §3.7** | blocking |
| 9 | `IR_FITTED` | operator declaration, never probed | operator |

CW and CCW are evaluated separately throughout, and stay separate in the
reported result even where the selected value is common. No direction-specific
constant is invented unless the data demonstrates one is needed; no material
directional difference is averaged away to keep the configuration simple.

### 3.3 The phantom question, answered

This was the single largest open risk for Otto on NAVI. The 2026-08-20 record
identifies a spurious population at peaks 40–91, duration 40–58 ms, which cost
a NO_QUORUM and drove Otto's entry threshold from 38 to 70. The 2026-09-09
survey ran at 38 precisely so that population would be visible if it still
existed.

**Sub-threshold population (`rej:2`), the band the phantoms lived in:**

| | CW (n=396) | CCW (n=527) |
|---|---:|---:|
| peak min / median / p95 / **max** | 15 / 17 / 25 / **33** | 15 / 17 / 24 / **32** |
| records with peak in 38–70 | **0** | **0** |
| records with peak ≥ 70 | **0** | **0** |

The entire sub-threshold population is baseline noise. **The 40–91 cluster does
not reappear.** The most likely explanation is the electrical one: new ESP32,
external antenna, and the INA219 replacement — consistent with the dataset
README's own caution about what the power fault may have touched.

**But it is not zero.** One spurious accepted event exists in the whole survey:

```
21:17:23.166  mm 68  obs S  peak 150  dur 172  dt 1298   <- genuine
21:17:23.309  mm 68  obs N  peak  67  dur  41  dt  250   <- spurious
```

mm 68 CW is a South magnet reading 150–165 counts on all six laps. This extra
**North**, 67-count, 41 ms event 250 ms behind it is the classic signature:
weak, short, opposite pole, adjacent in time. One in ~3,100 steady-state
passages.

**Why this matters more under NAVI than it did under QUORUM:** decision 0076,
*navigation recovers from one wrong observation*, is **proposed and not built**.
Under the current Navigator, one accepted phantom advances `navMm`, the next
genuine magnet arrives with the wrong pole against a stale expectation, and that
strikes. A strike is a shutdown — one of the outcomes the final success
criterion forbids.

**Three gates could refuse it, and they should be tested in this order:**

| gate | at Otto's numbers | refuses this event? |
|---|---|---|
| `NAVI_GUARD_MS` 200 (close-to-open) | genuine passage `dur` 172, next event 250 ms later → close-to-open ≈ **78 ms** | **yes, comfortably** |
| entry threshold 70 (margin 45) | peak 67 < 70 | yes |
| `NAVI_AMPLITUDE_FLOOR` 0.34 × gain ≈ 0.34 × 173 ≈ **59** | peak 67 > 59 | **no** |

The guard time is the gate that catches it, and it catches it with a 2.5×
margin. **This is exactly the defence NAVI has and QUORUM's quorum did not.**

The 78 ms figure is arithmetic on the published log, not a replay. **It must be
confirmed by replaying this record through `MagnetRecognizer` before it is
relied on** — that is a Stage 1 test, listed in §12.

**Consequence for Otto's entry threshold:** if the replay confirms the guard
refuses it, entry 38 becomes defensible and Otto recovers the sensitivity the
70-count gate costs him. If it does not, entry stays at 70, which refuses this
event on amplitude and — per §3.5 — costs no genuine steady-state marker in this
survey. **Either answer is measured. Neither is assumed, and the decision waits
on the replay.**

### 3.4 `NAVI_GUARD_MS` — proposed 200, margin measured

Shortest `ACTIVE`-gated genuine marker gaps, phantom and startup records excluded
per the README conventions:

| | CW | CCW |
|---|---:|---:|
| shortest genuine gap | **710 ms** | **917 ms** |
| median gap | 1,147 ms | 1,164 ms |

Toby runs 200. Otto's nearest genuine population sits **3.5× above it CW, 4.6×
CCW**. Expected false-reject from the guard: zero in 3,164 admitted passages.
Expected false-accept: the one mm 68 phantom is refused. **Selected value 200,
common to both locomotives — and here the data shows no directional difference
worth encoding.**

(Two CW gaps sit below 710 — the 250 ms phantom, and a 372 ms event at mm 100,
peak 101, dur 82. The mm 100 event has the same too-close signature and is a
second phantom candidate. It is flagged as a Stage 1 fit item, not resolved
here; a guard of 200 does **not** refuse it.)

### 3.5 Amplitude — `NAVI_BOOTSTRAP_GAIN` and `NAVI_AMPLITUDE_FLOOR`

Accepted-peak distribution, steady state:

| | CW (n=1,083) | CCW (n=2,081) |
|---|---:|---:|
| min | 67 (the phantom) | 41 (post-switch ramp, `drop:30`) |
| p1 / p5 | 126 / 144 | 127 / 142 |
| **median** | **173** | **174** |
| p95 / max | 204 / 266 | 204 / 424 (dwell record) |
| accepted below 104 | 4 | 3 |

Otto's median of 173/174 confirms the 2026-08-20 post-realignment mean of 176.4
and is **stable across direction** — a real result, and the reason no
direction-specific amplitude constant is proposed.

- **`NAVI_BOOTSTRAP_GAIN` proposed 175** (Toby 190), from Otto's own combined
  median. It is only the denominator of the amplitude ratio until eight peaks
  are in hand.
- **`NAVI_AMPLITUDE_FLOOR` proposed 0.34** — Toby's ratio, but the *ratio* is
  the portable quantity and Otto's own gap justifies it independently:
  0.34 × 173 ≈ 59 counts, against a genuine steady-state p5 of 144 and a
  sub-threshold maximum of 33. The gap it sits in is 33→144, and 59 is well
  inside it.
- **Honest statement of what the floor does on Otto:** at entry 70 it is
  **inert** — nothing that clears entry can fail it until gain exceeds 206, and
  Otto's gain is 173. At entry 38 it becomes live and refuses the 41/43/48-count
  ramp records. It does **not** refuse the mm 68 phantom at 67. The floor is not
  Otto's phantom defence under either aperture; the guard time is.

**Holdout method (Stage 1):** the six CW laps and twelve CCW laps are the natural
folds. Fit on laps 1..n−1, evaluate on lap n, rotate. Report false-accept and
false-reject per fold, per direction, separately. A constant that only works on
the pooled data does not ship. No threshold is moved to make training data pass.

### 3.6 `NAVI_APPROACH_MARKER_MS` — **blocking, and the survey did not close it**

`Stations.h:182` reads five values, the marker time at each approach step from
−10 to −6. Toby's were measured by holding PWM 87, 81, 75, 69 and 63 for a few
markers each and reading `gap_ms` — the numbers *are* the marker times, no
modelling.

Sample counts in the 2026-09-09 survey at exactly those throttles:

| PWM | 87 | 81 | 75 | 69 | 63 |
|---|---:|---:|---:|---:|---:|
| CW | 0 | 0 | 2 | 0 | 0 |
| CCW | 1 | 1 | 0 | 1 | 0 |

Everything else is PWM 90 (1,053 CW / 2,055 CCW). The handful of sub-90 samples
are ramp-up transients carrying `RAMP` or `NO_PREV` timing gates — they are
acceleration, not held throttle, and cannot measure a marker time.

**This is the same gap that blocked 2026-09-04.** It cannot be closed by
analysis, by scaling Toby's numbers, or by fitting a speed model to this data.
Two honest routes, and the choice is the operator's:

- **(a) The approach ladder.** One short manual run — hold 87, 81, 75, 69, 63 for
  a few markers each, both directions, on the survey build already proven on
  Otto. Perhaps fifteen minutes. Produces the five numbers directly and by
  Toby's own method, which keeps the two locomotives comparable.
- **(b) Defer.** Give Otto provisional values and accept that his first station
  approaches are paced wrong. **Not recommended:** a mis-paced ramp means the
  throttle is still moving when the next marker arrives, which is a station
  behaviour fault in a build whose acceptance test is station behaviour.

### 3.7 `NAVI_BASELINE_ADAPT_PWM` — **blocking, and the method is not recoverable**

Toby's 25 comes from *"speed_mm_s = 3.990 × (PWM − 25.1)"*, cited by decision
0066 as fitted to 4,617 samples. The 2026-09-04 report could not reproduce that
fit from either locomotive's calibration files, and found the documented
coefficients land on **Otto's** June data, not Toby's. No script in the repository
reproduces it.

That open issue is **recorded, not fixed, and Toby's frozen field-accepted build
is not touched on the strength of it.** But it means Otto's floor cannot be
derived "the same way", because the way is not written down.

Two routes:

- **(a) Creep test.** Raise PWM one count at a time from 15 until Otto just
  moves. Two minutes on a bench or a straight. Direct measurement of the thing
  the symbol names.
- **(b) Operator ruling to err high.** Freezing the baseline too often is a much
  milder fault than letting a median walk onto a magnet the locomotive is parked
  over (finding 10). A conservative value pending a creep test is defensible if
  it is recorded as provisional.

**Recommendation: (a) for both §3.6 and §3.7, in the same session.** Together
they are one short run and they unblock Stage 1 completely.

### 3.8 What the derivation report will contain

For every fitted value, without exception: source dataset; inclusion and
exclusion rules; sample count; CW result; CCW result; holdout method; selected
value; margin to the nearest observed failure population; expected false-accept
and false-reject behaviour. The §3.4/§3.5 tables above are the template.

---

## 4. QUORUM navigation code explicitly excluded

None of the following enters NAVI_CTO, in any form, adapted or otherwise:

1. `NavState { NAV_UNSET, NAV_NORMAL, NAV_EVALUATING, NAV_NO_QUORUM }` and every
   transition between them.
2. The evidence ring and every consumer of it.
3. Candidate scoring and any alternative position hypothesis.
4. Quorum offset adoption (`offset +N`) — the mechanism that walked Otto four
   markers on 2026-08-20.
5. Quorum recovery, re-acquisition and suffix rescue.
6. Quarantine, its three criteria, its ambiguous PHANTOM default, `Q_FLOOR_MS`
   and `Q_FLOOR_MS_OVERRIDE`.
7. `dt_conserve_ratio` as a navigation input (it may remain a published
   diagnostic).
8. The adaptive baseline governor and the measured-speed governor.
9. `serviceStations()` and the whole QUORUM station implementation.
10. `HALL_DOMINANCE_PERCENT`, `HALL_MIN_PEAK_DELTA` as gates (both are already
    dead; they survive only in boot telemetry).

**No hybrid navigator.** There is no build in which NAVI and QUORUM navigation
state coexist. The three compile guards in `LocoConfig.h` stay exactly as they
are.

Otto's QUORUM-era symbols `IR_TEST_A_ENABLED`, `IR_SENSOR_MAC_BYTES` and
`Q_FLOOR_MS_OVERRIDE` are dead in every NAVI tree and **do not come along**. His
radio arrives deliberately at Stage 2 or not at all.

---

## 5. The NAVI → CTO adapter contract

One header, `NaviTruth.h`, with **read-only** access to a `const NavStatus&` and
a small number of `const` accessors. It is the *only* surface CTO sees.

```c++
struct NaviTruth {
  bool     positionKnown;      // Navigator::positionKnown()
  uint8_t  navMm;              // marker position; meaningless if !positionKnown
  int8_t   navDir;             // +1 CW, -1 CCW, 0 unknown
  Trust    trust;              // Declared | Proven | Contradicted
  bool     autoRunning;        // AUTO enlisted and not withdrawn
  uint8_t  actualPwm;          // what the motor is being given
  uint8_t  naviIntentPwm;      // NAVI's own uncapped throttle intent (§6)
  StPhase  stationPhase;       // Idle | Approach | Zone | Ramp | Dwell | Depart
  uint8_t  frontBoundMm;       // occupied track, producer-applied (§5.1)
  uint8_t  rearBoundMm;
  bool     boundsValid;        // false whenever !positionKnown
};
```

**Enforced by construction, and by test (§12):**

- The adapter hands out values, never pointers or references into Navigator
  state.
- `Navigator` gains no non-`const` method reachable from any CTO translation
  unit. The CTO layer includes `NaviTruth.h`; it does not include `Navigator.h`.
- `boundsValid` is `false`, and `navMm` must not be read, whenever
  `positionKnown` is false. There is no "last known position" for CTO to coast
  on. A locomotive that does not know where it is publishes that fact, and
  §10 governs what its peers do about it.

### 5.1 Occupied-track truth

A locomotive does not occupy a mathematical Hall point. Each publishes front and
rear bounds derived from `navMm`, `navDir`, and its own consist extents, which
live in the locomotive profile and are a property of the **consist** — change
them when cars are added or removed:

```
CONSIST_EXTENT_FRONT_MARKERS 2      // both locomotives, today
CONSIST_EXTENT_REAR_MARKERS  4
```

Bounds are **producer-applied**: consumers receive occupied track, never an
unexpanded sensor point. Applied modulo the route length, so they wrap
correctly — tested explicitly (§12).

Any uncertainty the NAVI model genuinely requires is added here and **must be
justified in writing before it is added**. None is proposed today: NAVI's
position is either known or withdrawn, with no in-between state to widen for.

---

## 6. The CTO → NAVI motion-policy contract

CTO returns a small, closed set of values. It calls nothing in the navigation
path.

```c++
struct CtoPolicy {
  uint8_t     maxPwm;        // ceiling. 255 = no traffic constraint
  bool        trafficHold;   // this peer, this geometry
  bool        fleetHold;     // an expected peer is stale or position-invalid
  uint16_t    dwellMsOverride;   // 0 = NAVI's own dwell
  bool        skipThisStation;   // mission policy, §7
  uint8_t     cruiseOverride;    // 0 = section cruise; EXPRESS/LOCAL service speed
  const char* reason;        // operator-visible, always populated when constrained
};
```

### 6.1 Where the cap is enforced — the one design decision that matters most

NAVI's `requestPwm(target, up, down)` sets `rampTarget`, and `serviceRamp()`
walks `actualPwm` toward it. `requestPwm()` is called **only on events**: a
station order change (`.ino:1367`) or a section-cruise boundary (`.ino:1349-1352`).
Between events nothing re-evaluates the throttle.

**A cap applied only at `requestPwm()` call sites would therefore not be enforced
during steady cruise** — precisely the failure the brief names. Toby cruises
between stations for minutes at a time with no `requestPwm()` call at all.

QUORUM already solved this, and its solution is the donor's best idea
(`QUORUM.ino:2860-2880`): keep the **uncapped** intent in `ctoDesiredPwm`, apply
the cap at request time, **and re-apply `cap(desired)` every pass of
`ctoService()`**, so a cap appearing mid-cruise takes effect within one loop and
lifting it restores the intended speed.

NAVI_CTO adopts that shape, in NAVI's own terms:

1. `naviIntentPwm` holds NAVI's uncapped operational intent. Every existing
   `requestPwm()` call site writes it. **This is the only change to the
   navigation path, and it is a write of a new variable — it changes no
   existing behaviour.**
2. `serviceRamp()` gains one clamp, alongside the E-stop and low-voltage clamps
   that already live there and already work exactly this way:
   `effectiveTarget = min(naviIntentPwm, ctoPolicy.maxPwm)`, evaluated **every
   tick**.
3. **The clamp never mutates `naviIntentPwm`.** Release restores NAVI's
   previously valid intent and nothing else. (Note for the implementer: NAVI's
   low-voltage path *does* mutate `rampTarget` to zero, destroying intent. CTO
   must not copy that pattern, or release becomes unrecoverable.)
4. A CTO hold is `maxPwm = 0`. It stops the locomotive. It does not clear
   `autoRunning`, so release resumes service — **unless** NAVI itself withdrew
   authority, in which case `autoRunning` is already false and no CTO release
   can put power back in (§10).
5. Deceleration rate for every CTO-caused reduction is **one profile**, as
   QUORUM round 3 established — otherwise the spacing ladder's measured stopping
   distance does not hold. In NAVI's terms that is the station machine's own
   down-step rate. Restores are not braking and keep their own rates.

### 6.2 What CTO may never do

Never: advance or decrement position; change a polarity decision; accept or
reject a passage; score candidates; hold an alternative hypothesis; repair NAVI;
override a recognition result; change Hall thresholds at runtime; fabricate
position from peer data; treat a peer's position as evidence about the local
locomotive; or restart a locomotive whose NAVI authority has been withdrawn.

**A peer packet is traffic information. It is never local navigation evidence.**

### 6.3 The spacing numbers are provisional, and the measurement that settles them

Carried from 1.16R as field-tested provisional values, with provenance:

| constant | value | provenance |
|---|---:|---|
| `CTO_CLEAR_GAP_MARKERS` | 6 | decision 0033 invariant |
| `CTO_STOP_GAP_MARKERS` | 9 | operator 2026-08-14, 12→9. CODEX noted: a *smaller experimental margin*, not a stronger guarantee |
| `CTO_SLOW_GAP_MARKERS` | 18 | 1.16R ladder |
| `CTO_PAIR_RANGE_MARKERS` | 12 | 1.16R latch range |
| `CTO_ORDER_MARGIN_MARKERS` / `CONFIRM_N` | 12 / 3 | decision 0037 |
| `CTO_FOLLOWER_DWELL_MS` | 5000 | operator 2026-08-13 |

**These are not adopted as constants of the railway.** The measurement that
confirms or replaces them for NAVI_CTO, which CODEX already asked for and which
was never taken: **at Stage 4, record the minimum bound-gap actually achieved on
every approach to a hold, both directions, across at least twenty approaches.**
Assumed stopping distance is not evidence. If the achieved minimum crowds the
6-marker invariant, 9 goes back up.

---

## 7. Station-policy integration

**NAVI owns station behaviour.** `StationMachine`, per-direction stopping points
(decision 0068), approach, controlled zero ramp, dwell, departure, grade and
curve speed (decisions 0066, 0067), and the passage pause/resume/stitch around a
controlled stop (decision 0070) are unchanged. QUORUM's `serviceStations()` is
not copied.

CTO enters through `stationService()` (`.ino:1269`) as **policy inputs only**:

| input | effect | who decides |
|---|---|---|
| `cruiseOverride` | replaces `AUTO_CRUISE_PWM` in `cruisePwmAt()` | CE service speed |
| `skipThisStation` | station is passed, not served | CE rotating skip |
| `dwellMsOverride` | dwell duration | confirmed-follower dwell |
| `maxPwm` | ceiling, enforced in `serviceRamp()` | traffic |

**A CTO-imposed controlled stop uses NAVI's existing mechanism, and there is no
second implementation.** It must arm `stopArming` exactly as the station machine
does (`.ino:1279-1287`) — `Decelerating` on the way down, `Departing` on the way
up — so that decision 0070's two sentinels see the stop and the passage
measurement pauses and stitches correctly.

**This is the sharpest failure mode in the whole design.** A CTO stop that does
not arm the sentinels produces a passage spanning a stop, which decision 0070
exists to forbid, and which on 2026-09-02 at Bamboo cost three markers of
position. It is tested directly (§12) and it is the reason Stage 3 comes before
Stage 4.

---

## 8. ESP-NOW wire compatibility statement

`CtoPeerPacket` (CTO2_VERSION 3, magic 0xC4) and `Cto3RoleEcho` (0xC5, v1) are
**copied field-for-field and byte-for-byte from `QUORUM.ino:563-584`.** Field
order, types, packing and `sizeof` are unchanged. `docs/CLAUDE.md` freezes them;
the dispatcher and the Grillers Repeater decode them today.

**Three fields carry QUORUM semantics that NAVI must map onto, not redefine.**
This is the whole of the compatibility risk and it is not casual:

| field | wire meaning (frozen) | NAVI source | mapping |
|---|---|---|---|
| `stationPhase` | `ST_IDLE, ST_APPROACH, ST_FINAL, ST_RAMP, ST_DWELL, ST_DEPART` = 0..5 | `StPhase::Idle, Approach, Zone, Ramp, Dwell, Depart` = 0..5 | ordinals coincide 1:1, with `Zone` ↔ `ST_FINAL`. **Coincidence, not design — asserted by static_assert and by test, never assumed.** |
| `truthSource` | 0 none/lost, 1 declared/evaluating, 2 confirmed | `Trust` + `positionKnown` | `Proven` → 2; `Declared` → 1; `Contradicted`, `Unset`, `Struck`, or `!positionKnown` → **0**. |
| `mustHoldEligible` | always 0 in 1.14 | — | stays 0 |

Fields NAVI_CTO cannot honestly fill are **zeroed, never guessed** —
`speedX10`/`speedValid` stay 0 until IR is aboard, exactly as the decision 0021
lineage requires.

`hallMm` carries the raw NAVI marker; `frontBoundaryMm`/`rearBoundaryMm` carry
the producer-applied occupied bounds (§5.1).

**No wire change is proposed.** If one becomes genuinely unavoidable, work stops
and the compatibility problem is presented before anything is implemented.

---

## 9. Dispatcher authority statement

**The Dispatcher is supervisory.** It may enlist and release trains; issue
BEGIN, STOP, E-STOP, direction, declaration and mission commands; request
Circuit Express; and display locomotive, service, traffic and fault state.

**The Dispatcher may not** stream throttle to maintain separation; compute the
Bubble centrally; assign continuously changing leader/follower control; decide
each hold and release; be the sole source of collision protection; be required
for peer safety; infer position; replace ESP-NOW coordination; or command a train
forward because a dashboard looks clear.

Every locomotive independently evaluates received peer truth and constrains its
own motion. **ESP-NOW is the safety and coordination path; MQTT reports and
supervises.** Losing Wi-Fi, MQTT or the Dispatcher **grants no new movement
authority** — local and peer safety stay exactly as conservative as they were.

Existing MQTT topics and payloads do not change (`docs/CLAUDE.md`). CTO adds
`cmd/cto` (clear/off/on) and CTO status telemetry, both already in the donor and
already understood by the console.

**Tested, not asserted (§12):** a test that streams `cmd/throttle` at the
dispatcher's maximum rate must not defeat a traffic hold.

---

## 10. Failure behaviour

| failure | local behaviour | what peers do | what must never happen |
|---|---|---|---|
| **Local NAVI position lost** (Contradicted / Struck / withdrawn) | NAVI has already stopped the locomotive and cleared `autoRunning`. CTO publishes `truthSource 0`, `boundsValid false`. | An expected peer reporting `truthSource 0` is an **unresolved obstruction** → fleet stop. | **No CTO path may restart it.** Radio recovery, re-pairing, mission end, cap release — none puts power back. Only the operator. |
| **Expected peer goes silent** (> `CTO_PEER_STALE_MS` 3000) | fleet stop, reason names the peer and its last sequence and age. | — | **Silence is never clear track.** A peer that vanishes is an obstruction at its last known bounds. |
| **Peer position invalid** | fleet stop. | — | its stale `hallMm` is never used |
| **MQTT / Wi-Fi lost** | ESP-NOW peer protection **fully intact**; local NAVI intact; telemetry buffers. | unchanged | no new movement authority; no "dispatcher gone, run free" |
| **ESP-NOW channel divergence** | the station channel follows the AP; a roamed locomotive keeps perfect MQTT and is invisible to peers. `ctoChannel`/`ctoChannelChanges` published; divergence from an expected peer's channel → treat as peer loss → fleet stop. | same | **a frame on the wrong channel reports SUCCESS.** `esp_now_send()` returning `ESP_OK` is never evidence of reception. |
| **Role conflict** (both claim the same role) | safe hold until reciprocal opposite-role echo. | same | no tie-break by ID or by luck |
| **Physical order inverts on the loop** | pairing dissolves; roles re-derived from geometry. Inversion must be **clear** (12-marker margin) and **confirmed** (3 consecutive passes) — decision 0037. | same | a leader braking for a locomotive behind it (observed 2026-08-16) |
| **Peer reboot** (sequence resets) | detected by `alert.uptime_ms` going backwards and by sequence discontinuity; registry entry reset, pairing dissolved, roles re-derived. | same | a reset sequence read as massive frame loss, or as a fresh peer at a stale position |

---

## 11. Stage-by-stage implementation plan

Every stage is one build, one behavioural change class, its own field session,
its own verdict document, and its own rollback identity (§13). **Navigation,
station behaviour, CTO, mission behaviour and radio transport are never changed
in the same field build.**

### Stage 1 — locomotive-specific NAVI models. **No CTO. No radio.**
Otto and Toby NAVI builds, separate measured profiles, correct identity at boot,
reproducible derivation report, CW and CCW reported independently, no excluded
Otto data, no architecture change.
**Blocked on §3.6 and §3.7** — the approach ladder and the tractive floor.
**Field acceptance:** Otto completes ≥ 1 h in each direction, ≥ 20 station stops,
with **zero false positions, zero navigation shutdowns, zero transport loss** —
the same bar Toby cleared on 2026-09-04. Toby's build must be **behaviourally
identical** to his accepted image; his own replay suite proves it.
*Compiling is not acceptance.*

### Stage 2 — ESP-NOW observation only
Frozen wire + 1.16Ra radio instrumentation. **No cap, no roles, no fleet stop, no
mission authority.** The receive path changes no motor or navigation state — it
increments counters and fills the registry, nothing else.
**Field acceptance:** full-route link test both directions, both locomotives;
sequence delivery independently measurable by the locomotives, Recorder,
Dispatcher and Repeater; **solo NAVI behaviour byte-identical to Stage 1 by
replay.**

### Stage 3 — universal traffic protection
Occupied bounds, `maxPwm`, holds. Traffic may only **reduce** authority. MANUAL
authority stays separate except for E-STOP. The cap is enforced continuously
(§6.1). Release restores only previously valid NAVI intent. A NAVI-withdrawn
locomotive stays stopped.
**Field acceptance:** ≥ 20 supervised approaches to a hold, both directions,
E-stops in hand; every hold and release traceable; **minimum achieved bound gap
recorded** (§6.3).

### Stage 4 — Bubble
Role formation, reciprocal echo confirmation, follower behaviour, role conflict
hold, fleet stop, order-inversion dissolution, re-pairing. Supervised, E-stops
immediately available.

### Stage 5 — Circuit Express
Only after ordinary Bubble operation is demonstrated. EXPRESS/LOCAL assignment,
service speeds, rotating every-third-station skip, separation/closing lifecycle,
completion **on re-pairing** — never on a gap number.
**Circuit Express computes no spacing of its own.** When EXPRESS closes on
LOCAL, the Stage 3 traffic layer slows or holds it. No spacing logic is
duplicated inside the mission layer.

---

## 12. Tests and field acceptance criteria

**Before any flash is proposed:** compile both locomotives from clean, explicit
`--build-path` directories; confirm embedded locomotive identity and firmware
identity **from that build path or the boot banner** — never by `strings` over a
stale `build/` folder, which has reported the wrong locomotive before
(`LocoConfig.h`, 2026-08-19).

**Inherited, must keep passing unchanged:** all 19 NAVI tests, including
`replay_lap`, `replay_survey`, `replay_polarity_survey`, `replay_station_run`,
`gate_interrupted`, `gate_two_sided`, `gate_station*`, `gate_polarity`,
`gate_baseline_latch`, `contract`.

**Inherited donor tests:** `test_cto_roles.py`, `test_ce_mission.py`,
`test_channel_watch.py`.

**New:**

*Adapter boundary* — CTO cannot modify Navigator state (no non-`const` path
reachable, enforced at compile time and asserted); `navMm` unreadable when
`!positionKnown`.

*Wire* — `static_assert` on both struct sizes; byte-for-byte comparison against
frozen v3 fixtures; the `StPhase`↔`StationPhase` ordinal mapping asserted
explicitly; `truthSource` mapping across all `Trust` values; unfillable fields
zero.

*Transport* — sequence wrap; duplicates; out-of-order frames; stale peers; queue
overflow (`ctoRxDropped` increments, nothing else changes); channel change; peer
reboot with sequence reset.

*Roles* — formation; conflict → hold; order inversion with the 12-marker margin
and 3-pass confirmation; dissolution; re-pairing.

*Geometry* — spacing across route wrap; occupied bounds both directions;
follower-front to leader-rear separation, never sensor-point to sensor-point.

*Safety invariants* — peer silence produces a hold, never clear track; MQTT loss
does not disable peer protection; a dispatcher streaming `cmd/throttle` at
maximum rate cannot manufacture continuous movement authority; a NAVI-withdrawn
locomotive cannot be restarted by any CTO path.

*Stations* — approach, ramp, dwell, departure, interrupted-passage stitching;
**a CTO-imposed controlled stop arms `stopArming` and stitches correctly** (§7);
CE rotation and mission completion tested independently on both locomotives.

*Equivalence* — **solo NAVI behaviour is unchanged while no eligible peer is
present**, proved by replaying the 2026-09-04 session and comparing event-for-event.

*Stage 1 specific* — replay the 21:17:23 mm 68 record through `MagnetRecognizer`
and confirm the guard-time refusal that §3.3 predicts; resolve the mm 100
candidate the same way.

---

## 13. Rollback identity for every proposed field build

| build | identity | rollback to |
|---|---|---|
| Otto Stage 1 | `NAVI_CTO_0_1_FIELDTEST` — 9950011 | Otto's current **QUORUM 1.12C** |
| Toby Stage 1 | `NAVI_CTO_0_1_FIELDTEST` — 9950012 | **`NAVI_ONE_STATION_CURVES_0_3`**, the 2026-09-04 image |
| Stage 2 | `NAVI_CTO_0_2_FIELDTEST` | that locomotive's Stage 1 image |
| Stage 3 | `NAVI_CTO_0_3_FIELDTEST` | Stage 2 |
| Stage 4 | `NAVI_CTO_0_4_FIELDTEST` | Stage 3 |
| Stage 5 | `NAVI_CTO_0_5_FIELDTEST` | Stage 4 |

Every build carries the `_FIELDTEST` suffix and prints its risk at boot, as the
NAVI lineage already does. No build is renamed to production without provenance
and acceptance evidence. **The locomotives are on QUORUM 1.16R today; nothing
here is flashed without explicit authorization.**

---

## 14. What I need from the operator before Stage 1

1. **The approach ladder and the creep test (§3.6, §3.7).** One short session:
   hold 87/81/75/69/63 for a few markers each in both directions, then raise PWM
   one count from 15 until Otto just moves. This is the only thing standing
   between the 2026-09-09 dataset and a complete Otto block. Run it, or rule
   that Otto flies Stage 1 with provisional approach pacing and a conservative
   floor.
2. **`IR_FITTED` for Otto — 0 or 1?** Declaration, never probed. `0` means pin 34
   is never touched, which also keeps floating-input crosstalk off the Hall line.
3. **Otto's entry threshold for the navigation build.** My recommendation is to
   let the §12 replay decide: if the guard refuses the mm 68 phantom, go to 38
   and recover the sensitivity; if not, stay at 70. Do you want that decision
   made by the replay, or do you want to make it?
4. **Decision records.** This work implies at least three — the CTO authority
   boundary, the wire-mapping of NAVI states onto the frozen v3 fields, and
   Otto's measured block. Numbering continues from **0076** (0077 next).
   I have not written any. Say the word and I will draft them as *Proposed*.

Note also, unchanged since 2026-09-04 and still not touched: `docs/decisions/`
contains two files numbered **0075**. Flagged, not resolved.

---

## Evidence

- NAVI base: commit `7d163fa`, verified reachable; `firmware/test-programs/NAVI_ONE/`.
- Behavioural reference: `firmware/test-programs/NAVI_ONE_STATION_CURVES/`, `SKETCH_NAME "NAVI_ONE_STATION_CURVES_0_3"`.
- Donor commits: all nine verified by `git log`.
- Wire structs: `firmware/QUORUM/QUORUM.ino:556-587`; cap pattern `:2860-2880`.
- NAVI throttle path: `NAVI_ONE.ino:447` `writePwm`, `:453` `requestPwm`, `:458` `serviceRamp`, `:1269` `stationService`, `:1349` section cruise.
- NAVI enums: `Navigator.h:74,110`; `Stations.h:184`.
- Otto dataset: `field-records/logs/20260909_survey/`, every README figure independently reproduced (§3.1).
- Otto distributions, phantom, gaps, PWM coverage: §3.3–§3.6, computed from the two `.log.gz` files.
- Prior state: `docs/OTTO_ON_NAVI_ONE_STEP1_REPORT_20260904.md`, `docs/OTTO_SURVEY_BUILD_20260905.md`.
