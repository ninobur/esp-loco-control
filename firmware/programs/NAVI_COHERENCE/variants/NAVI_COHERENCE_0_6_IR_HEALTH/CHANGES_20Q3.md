# NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3 — change record

2026-09-25, updated 2026-09-26 with the post-review operator rulings (§1a).
Implemented by Claude on task branch `claude/navi-ps-r1-20q-standards`
at David's explicit instruction. **Not flashed. Not field tested. ESP32
compile pending local verification on David's laptop.**

## Identity and base

| Item | Value |
|---|---|
| Build name (`SKETCH_NAME`, published on `state/bootid`) | `NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3` |
| Behavioral base | POSITION_STATIONS_R1, commit `57d34d6` (2026-09-23) |
| Task-branch base | `8d9a9ac` = `agent/toby-1-13-flash` `e7a8a8b` + lineage doc. The variant folder and every file it includes are byte-identical to `57d34d6`; the intervening commits are documentation/policy only. |
| Arduino sketch and shortcut | unchanged: `NAVI_COHERENCE_0_6_IR_HEALTH.ino` in this folder |
| Rollback | flash the `57d34d6` source (POSITION_STATIONS_R1), or Toby's last verified installed build per the README |

## What this build is

The implementation of three **previously decided** Twenty Questions NAVI
standards, plus removal of the anachronisms that stood in their way. None of
70 counts, 650 ms or the ±15% semantics is a new Claude design choice.
Sources: `docs/NAVI_COHERENCE_TWENTY_QUESTIONS_HANDOFF_20260923.txt` §§10–13,
decision 0093, decision 0094, `docs/NGR_NAVI_ARCHITECTURAL_LINEAGE_DRAFT_v0_1.md`
§§3, 5, 8, and David's implementation instructions of 2026-09-25.

## 1. Previously adopted decisions now implemented

### Hall opening: ≥70 counts, two consecutive same-sign samples

- **Before:** the sketch passed `HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS`
  = 25 + 13 = **38** (Toby profile) to the X22 detector.
- **Now:** `NAVI_HALL_DEPART_COUNTS 70` (variant `LocoConfig.h`), passed to the
  unchanged X22 detector: absolute departure ≥70 from the locked per-interval
  baseline on two consecutive same-sign 1 kHz samples, detection on the second,
  polarity fixed at detection. X22's locked baseline is untouched.
- **Decision:** Twenty Questions handoff §11; lineage §5 ("decided experimental
  value, not yet implemented").
- The shared `NAVI_SIMPLIFIED/LL_LocoConfig_9950012.h` is deliberately not
  edited; other sketches include it.
- X22 uses the same `departCounts` for its baseline-lock quiet test, so that
  tolerance also becomes 70. This is X22's own design (Otto already runs 70).

### Hall-only fallback: 650 ms

- **Before:** `MIN_MARKER_MS = 500`, applied on the no-anchor path and whenever
  the legacy IR interval was unusable.
- **Now:** `HALL_ONLY_GUARD_MS = 650`, open-to-open from the previous accepted
  detection, applied **only** when no valid MM-referenced IR distance exists.
  Valid same-epoch distance is never vetoed by time.
- **Decision:** 0093 (accepted provisional), handoff §13.
- X22's ~645 ms detector refractory is **superseded** by this rule and has been
  removed from the candidate's detector (X22R, below). There is one timing rule.
- Inherited and unchanged: after a declaration or reversal, the 650 ms is
  measured from the declaration/reversal time, exactly as the 500 ms was.

### Exact ±15% expected-MM window

- **Before:** a Hall event scanned up to ten cumulative windows and accepted the
  first containing it. A miss existed only when a *later* Hall event landed in
  a farther window (`MISSED_AND_ADVANCED`); an event between windows was refused
  without recording any miss.
- **Now**, with a valid MM-referenced IR distance d, for cumulative mapped
  distance D from the last MM/IR synchronization to the expected MM:
  - d < 0.85D: too early. The Hall event is rejected; the expected MM is unchanged.
  - 0.85D ≤ d ≤ 1.15D (inclusive): the opening is eligible for the expected MM
    and proceeds through the opening-polarity/NAVI rules.
  - d > 1.15D with no accepted observation: NAVI judges the expected MM
    physically traversed. It is recorded as MISSED/UNKNOWN in recovery history
    (no Hall or polarity evidence manufactured), `navMm` advances to it and the
    target advances. Ruling: **`POSITION_ADVANCED_SANS_MM`**.
- This is evaluated continuously in the loop (`Navigator::traverse()`), and at
  each Hall event's own measured distance before classifying that event. A
  later Hall event no longer creates the miss.
- Distance stays cumulative from the last actual MM/IR synchronization until
  another accepted MM establishes a new one. Comparisons are integer
  micrometres, so the 0.85D and 1.15D boundaries are exact.
- **Decision:** handoff §10; lineage §5 ("partially implemented").
- `navMm` is NAVI's judgment of the current MM, not the last Hall-observed MM.

### Ten consecutive advances sans MM → End AUTO Operations

- `SANS_MM_AUTO_LIMIT = 10` (operator ruling 2026-09-25, provisional). At the
  limit, while AUTO is running, the established `withdraw()` path ends AUTO:
  controlled stop, AUTO de-enrolled, sticky warning; Manual remains available.
- It is **not** LOST: `navMm`, route/recovery history and the IR reference are
  retained. Only an accepted Hall landmark resets the count; a re-GO before
  that ends immediately with the same message. A declaration also clears it.
- Position judgment itself continues past ten (for example in Manual): the
  limit governs AUTO landmark authority only, per the ruling.

## 1a. Operator rulings after review (2026-09-26)

These are David's decisions on the review items; they govern this candidate.

1. **645 ms refractory: superseded and removed.** Hall detector detects → NAVI
   judges. The single timing rule with authority is 650 ms detection-to-detection,
   applied by NAVI only when valid MM-referenced IR distance is unavailable. Valid
   MM-referenced IR distance is never vetoed by a timing refractory. The candidate
   now uses **X22R** (`ExcursionDetectorX22R.h`): X22 with the obsolete refractory
   machinery removed. This is not a detector redesign; the locked per-interval
   baseline and detection architecture are unchanged, and equivalence with X22 at
   refractory=0 is tested. The historical X22 header and older builds are untouched.
2. **IR Epoch break at station stops: accepted for this field candidate.** This is
   the **currently observed IR TX behavior**, seen by David on the dashboard. It is
   not an architectural requirement that stops end Epochs. Expected recovery:
   Epoch ends → movement resumes in a new Epoch → no MM synchronization yet → the
   first accepted MM uses the 650 ms Hall-only fallback → that accepted strike plus
   its valid aligned IR point establishes the new MM/IR synchronization →
   subsequent MMs return immediately to the ±15% mapped-distance method. No
   probation or trust earning.
3. **Synchronization after a sans-MM advance.** `navMm` is NAVI's judgment of the
   current MM. The MM/IR synchronization point is the last accepted physical MM
   Hall observation with a valid aligned IR point in the current Epoch. A
   sans-MM advance never creates one. Example: MM029 accepted and synchronized;
   MM030 traversed sans MM → the MM031 window is MM029→030 + MM030→031 from the
   MM029 IR count; if MM031 is also missed, MM032 uses MM029→030→031→032. Only
   another accepted MM with a valid aligned IR point resynchronizes and resets
   cumulative distance. (Verified against the committed code on 2026-09-26.)
4. **Accepted Hall MM without an aligned IR point.** NAVI advances `navMm`; no IR
   count is manufactured and no synchronization is extrapolated. The previous
   synchronization loses authority for later windows, and NAVI continues
   temporarily on the 650 ms Hall-only fallback. The next accepted MM with a valid
   aligned IR point resynchronizes immediately. The 650 ms rule is normally a
   short bridge, often one interval, not a mode NAVI stays in.
5. **Hypothetical within-Epoch IR count jump: no new safeguard.** There is no
   installed-hardware evidence that this is an NGR problem. No Vmax/rate guard
   was added; telemetry and the ten-MM AUTO limit suffice. Observed track
   behavior, if any, will be addressed then.
6. **Ten consecutive advances sans MM → normal End AUTO Operations** (preserved).
   A provisional AUTO landmark-authority limit, not LOST; `navMm`, history and
   valid IR are preserved; Manual remains available; an accepted Hall landmark
   resets the count. It is not a statement that IR-only navigation is impossible:
   future TRACKSIDE_LOCATOR absolute fixes may allow continued or restored
   IR-supported navigation without onboard Hall confirmation.
7. **Station logic follows `navMm`** (preserved). When valid referenced IR
   establishes that an expected MM was traversed, station logic responds to the
   updated position. Hall observation is evidence for NAVI's judgment; it is not
   required for every legitimate position advance.
8. **Hall-derived speed: telemetry correctness, not fusion.** Hall and IR speed
   stay separately visible. Hall speed has no NAV or control authority. The last
   actual Hall measurement is preserved until a new legitimate one replaces it
   and is never replaced by an invented 0; its age is published. General NAVI
   speed fusion is deferred.
9. **Opening polarity at detection** is the Hall polarity with NAV authority
   (preserved). The later window is diagnostic only.
10. **500 ms guard: superseded and banished** from the active implementation.
    Historical documents may record it as superseded history.
11. This record and the README reflect these rulings.

## 2. Anachronistic mechanisms removed

| Removed | Was | Replaced by |
|---|---|---|
| Active 500 ms guard | `MIN_MARKER_MS=500`, both Hall-only paths | `HALL_ONLY_GUARD_MS=650`, Hall-only path only |
| `legacy_guard_500` | `nav/ir_compare` key evaluating `elapsed<500` | nothing; key removed |
| Window-polarity NAV authority | `hallSupports()` accepted the 400 ms window peak sign as supporting evidence | opening polarity at detection is the only Hall polarity with NAV authority (lineage principles 3, 4) |
| ~400 ms delay of NAV judgment | Hall task queued the event only after the acquisition window completed | the opening is queued at detection; the window follows as a separate record on `nav/hall_window` with `nav_authority:"NONE"` |
| X22 ~645 ms detector refractory | `DetectorConfig::refractoryMs` (set to 0 by `hallConfig()`), the refractory/rearm-pending state, its suppression branch, `Detection::guardUntilMs`, and the baseline cycle's wait on the guard | nothing: removed in X22R. NAVI's 650 ms Hall-only fallback is the one timing rule. Boot telemetry reports `hall_only_guard_ms` instead of `guard_ms` |
| Invented Hall speed = 0 | `withdraw()`, `LOCATION_UNRESOLVED` and declarations set Hall speed to 0 (`withdraw()` also published `telem/speed` = "0") | the last actual Hall-derived measurement is kept, with `est_age_ms`; spanning protections unchanged |
| TRACKING-at-both-endpoints validity in the NAV decision path | `MovementSource::between()` required TRACKING at both ends and unchanged counters | the adopted Epoch model: the accepted strike's `IrOdometryPoint` is the MM synchronization; same-epoch odometry gives distance (decision 0094; lineage principles 11, 12) |

`MISSED_AND_ADVANCED` is no longer produced. A wrong opening polarity inside
the window is a real contradictory observation (`ADVANCED_WITH_DISCREPANCY`)
handled by the unchanged decisional-inertia/proximal-recovery architecture.
The window can no longer rescue it.

An Epoch break makes MM-referenced distance unavailable until a new
synchronization. The break is published as `EPOCH_BREAK`, never bridged. When
a later accepted MM has a current-epoch IR point, synchronization is immediate:
no probation.

## 3. Source files changed

| File | Change |
|---|---|
| `NAVI_COHERENCE_0_6_IR_HEALTH.ino` | Build name/subtitle; `hallConfig(NAVI_HALL_DEPART_COUNTS)`; Hall task queues at detection and sends window telemetry separately; loop passes the epoch point to NAVI, calls `serviceTraversal()` before `stationService()`, publishes sans-MM advances, applies the ten-MM AUTO limit via `withdraw()`; uses X22R; boot telemetry `hall_only_guard_ms`; Hall speed no longer zeroed by withdrawal, `LOCATION_UNRESOLVED` or declaration, with `est_age_ms` in the status line; `nav/ir_compare` uses epoch distance and drops `legacy_guard_500`/`window_matches`; new topics `nav/hall_window`, `nav/sans_mm` |
| `ExcursionDetectorX22R.h` (new) | X22R: the X22 detector with the obsolete refractory removed (namespace `ngr_hall`) |
| `HallObserver.h` | uses X22R; no refractory configured |
| `Navigator.h` | 650 ms; exact ±15% semantics; `traverse()`; sans-MM queue; epoch MM synchronization; opening polarity only; `DistanceBasis`; `SANS_MM_AUTO_LIMIT` |
| `LocoConfig.h` | `NAVI_HALL_DEPART_COUNTS 70` |
| `tests/test_20q_standards.cpp` | new focused tests (below) |
| `tests/test_x22r_equivalence.cpp` | new: X22R versus historical X22 at refractory=0 |
| `tests/test_coherence.cpp`, `tests/test_proximal_recovery.cpp`, `../../../../../tools/test_nav_decisional_inertia.cpp` | IR inputs converted from legacy `MotionPoint` to epoch `IrOdometryPoint`; assertions encoding the replaced rules updated (listed below) |
| `README.md`, `CHANGES_20Q3.md` | documentation |

### Existing test assertions changed, and why

- `test_coherence.cpp`, MM58 missed: `MISSED_AND_ADVANCED` → `ADVANCED` plus a
  separate sans-MM record for MM58 (the miss is the traversal, not the event).
- `test_coherence.cpp`, missed-plus-wrong-pole: now `ADVANCED_WITH_DISCREPANCY` /
  `POLARITY_DISCREPANCY` on MM58 after a sans-MM MM57. The final opening's
  discrepancy is still retained.
- `test_coherence.cpp`, event at 1.17D: still `NON_LANDMARK`, now also asserting
  that MM102 was traversed (`navMm==102`).
- `test_coherence.cpp`, multi-step history: `MISSED_AND_ADVANCED` → `ADVANCED`;
  history count and UNKNOWN entry assertions unchanged.
- `test_proximal_recovery.cpp`, "single outlying distance" (10 m in 1 s): was
  refused at the old ten-marker horizon with position retained. Under the
  approved rule it traverses about 30 expected windows sans MM; NAVI stays
  TRACKING (not LOST, no correction). See Known limitations.
- `test_nav_decisional_inertia.cpp`, missed landmark: `MISSED_AND_ADVANCED` →
  `ADVANCED` plus exactly one sans-MM record.

## 4. Provisional values and policies

- 70 counts: decided experimental value (field-supported; weakest real magnets ~70–72).
- 650 ms: accepted provisional (0093).
- ±15%: current provisional window (operator ruling 2026-09-22).
- Ten consecutive MMs sans landmark → End AUTO: provisional AUTO
  landmark-authority limit (2026-09-25), about one rolling-history horizon. It
  is not a claim that IR-supported navigation cannot operate between absolute
  fixes (for example future TRACKSIDE_LOCATOR fixes).

## 5. Deterministic test results (cloud, 2026-09-25)

These prove the software implements the defined rules. They are **not**
behavioral evidence (AGENTS.md §8).

Flags: `-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined` (g++ 13).

| Test | Result |
|---|---|
| `tests/test_20q_standards.cpp` (new) | PASS: 2,394 window cases (every MM, both directions, wrap); 6 Hall-only/epoch; 242 traversal/limit; 6 detector (X22R) |
| `tests/test_x22r_equivalence.cpp` (new) | PASS: X22R identical to X22 at refractory=0 over 72 seeded traces, 14,893,908 samples: 9,624 detections (all fields, opening polarity included), 8,772 window records (including sample arrays), 7,269 baseline outcomes (3,217 locks), 1,599 dwells, 29 lost-lock segments, reset and baseline-adjust calls; every public state getter compared each sample |
| `tests/test_coherence.cpp` | PASS: 2,052 offset corrections, 6,840 single-misread holds (unchanged counts) |
| `tests/test_proximal_recovery.cpp` | PASS: 421 ties, 65 one-vote improvements (unchanged counts) |
| `tests/test_station_correction.cpp` | PASS: 32 |
| `tests/test_station_position.cpp` | PASS: 128 entries, 136 resumes, 342 route positions, 24 stops |
| `tests/test_ir_health_monitor.cpp` + `test_health_json.py` | PASS: 13 payloads, max 638 B |
| `tests/test_ir_speed.cpp`, `tests/test_navi_stop_display.cpp` | PASS |
| `tools/test_nav_decisional_inertia.cpp` | PASS: 342 scenarios |
| `tools/test_ir_stationary_{pipeline,revision,adversarial}.cpp` | PASS (adversarial: 0 hard failures) |
| `tools/test_position_station_integration.py` | PASS (run with g++; see note) |
| `tools/test_manual_pwm.py` | PASS: 311 cases (run with g++; see note) |

Boundary coverage in `test_20q_standards.cpp`: 69 versus 70 counts; first
versus second qualifying sample; opposite-sign pair; no detector refractory
(a new opening about 460 ms after the previous one is declared); 649 versus
650 ms with no reference, with no IR point, and across an Epoch break; valid IR
accepted 300 ms after the previous strike (no timing veto); just before 0.85D,
exactly 0.85D, inside, exactly 1.15D, just beyond 1.15D; early extra Hall then
the real MM; wrong opening polarity; windows traversed with no Hall through the
ten-MM limit; reset by an accepted landmark; new synchronization restarting
cumulative distance; an ended epoch never reopening; wraparound; both directions.

A mutation check confirmed that each implemented boundary is caught when
perturbed: 849/851‰, the inclusive/exclusive edges, 649 and 500 ms, limit 9,
missing count reset, missing cumulative reset, 38 counts, and refractory 645.
The equivalence test was likewise shown to fail for X22 with its 645 ms
refractory re-enabled, an inverted opening polarity, and a baseline cycle that
does not wait for the acquisition window. (A `>=0` versus `>0` polarity mutant
survives because a detection always has |departure| ≥ 70; it is equivalent.)
X22's zero-deadline wrap (`nowMs + 0 == 0` maps to 1) is excluded by starting
traces at t = 1; it is a clock-wrap artefact of the removed code, not retained
behavior.

Notes:
- The two Python harnesses call `clang++ -fsanitize=address`. The cloud
  container lacks clang's ASan runtime, so the unmodified baseline `57d34d6`
  fails there identically at link time. Both were run with `g++` substituted in
  scratch copies; the repository tools are unchanged.
- `tools/audit_navi_legacy_contracts.py` (the 09-23 audit probe) compiles
  against the pre-20Q3 navigator API (`MotionPoint`, `.movement`,
  `MissedAndAdvanced`), which this build deliberately removes. It is a
  historical audit artifact and was left unchanged; it does not build against
  this candidate.
- The sketch-level changes (Hall task queuing, `serviceTraversal()`,
  `publishSansMm()`, the AUTO-limit withdrawal, the new telemetry) cannot be
  host-compiled here. They were reviewed line by line; the local ESP32 compile
  is their first compilation. `tools/test_position_station_integration.py`
  stubs `withdraw()`, so the Hall-speed change inside the real `withdraw()` is
  likewise review-only until the local compile and track test.

## 6. ESP32 compile

**Pending local verification** (David's Arduino environment). Not performed in
the cloud: its network policy blocks the Arduino/Espressif toolchain downloads,
and David directed not to change that.

## 7. Deliberately unchanged

PROXIMAL_R1 recovery; station behavior (targets, offsets, ramps, dwell,
POSITION_STATIONS_R1 logic); the historical X22 detector header (older builds
still use it; the candidate uses X22R, whose locked baseline is unchanged); IR TX/RX
firmware; stopped-speed interpretation; manual control; E-stop and low-voltage
paths; the shared NAVI_SIMPLIFIED profile; the measured-speed station ZIP (not
incorporated); `MovementSource` (still used for `diag/ir_link` and
`nav/discrepancy` link fields, no longer by NAV decisions).

## 8. Telemetry changes (for Pi/dashboard and log readers)

No `server/` code parses any of these fields (checked).

- `nav` (retained): new event `POSITION_ADVANCED_SANS_MM`; `MISSED_AND_ADVANCED`
  is no longer produced.
- New `nav/sans_mm`: `mm`, `dir`, `reference_mm`, `travel_mm`, `window_high_mm`,
  `consecutive`, `auto_limit`, `hall_evidence:"NONE"`, `polarity:"UNKNOWN"`, `drops`.
- New `nav/hall_window`: `event_serial`, `opening`, `window`, `window_valid`,
  `peak_signed`, `agrees_with_opening`, `nav_authority:"NONE"`, `window_drops`.
- `nav/discrepancy`: `window`, `window_valid`, `peak_signed` are `null` (not
  known at judgment); `ir_interval` now carries the distance basis
  (`MM_REFERENCED_EPOCH`, `NO_MM_REFERENCE`, `EPOCH_BREAK`, `NO_IR_POINT`).
- `nav/hypotheses` and the status alert: `ir_interval` carries the same basis names.
- Boot record: `guard_ms` (the X22 refractory setting) is replaced by
  `hall_only_guard_ms`: 650; `baseline` reads `X22R_LOCKED_NO_PWM_RECOVERY`.
- Status alert: new `est_age_ms` (age of the Hall-derived `est_mm_s`, `null`
  before the first measurement). `telem/speed` keeps its format but is no longer
  forced to "0" by withdrawal, `LOCATION_UNRESOLVED` or declaration; it carries
  the last actual Hall-derived measurement. IR/NAVI speed remains on `telem/ir`.
- `nav/ir_compare`: `legacy_guard_500` and `window_matches` removed; `branch`
  is always 0; `anchor_mm` is the synchronized MM; distances are epoch travel;
  `ir_quality` is the distance basis.

## 9. Known limitations and review items

1. **X22R's 400 ms acquisition window still blanks new detections.** While it is
   open the detector declares nothing new and only counts suppressions. This is
   retained X22 behavior (not the removed refractory), separate from NAV
   judgment. A real next magnet inside 400 ms needs more than about 700 mm/s;
   Toby cruises near 240 mm/s.
2. **Station-stop Epoch loss is currently observed IR TX behavior** (ruling 2),
   not an architectural requirement. After such a stop the first MM is accepted
   Hall-only (650 ms), which also resynchronizes when it has a valid aligned IR
   point; the ±15% method resumes immediately after.
3. **No safeguard against a within-Epoch IR count jump** (ruling 5). Such a jump
   would traverse windows sans MM and, at ten, end AUTO. Watch `nav/sans_mm`.
4. **Sans-MM advances move `navMm` without Hall** (ruling 7). Station logic
   (unchanged) acts on NAVI's position, including inside a station region.
5. **Hall-derived speed** never spans a sans-MM advance, declaration, correction
   or withdrawal: the next estimate starts from the next accepted Hall. Until
   then the last actual value is shown with its growing `est_age_ms` (ruling 8).
6. The traversal ordering guard, the loop integration and the edited `withdraw()`
   are verified by review only until the local compile and track test.

## Reproduce (repository root)

```sh
V=firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH
F="-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined"
c++ $F -I $V $V/tests/test_20q_standards.cpp -o /tmp/t20q && /tmp/t20q $V
for t in test_coherence test_proximal_recovery test_station_correction test_station_position test_navi_stop_display test_ir_speed; do
  c++ $F -I $V $V/tests/$t.cpp -o /tmp/$t && /tmp/$t; done
c++ $F -I $V tools/test_nav_decisional_inertia.cpp -o /tmp/tdi && /tmp/tdi
python3 tools/test_position_station_integration.py
```

## 10. Status

Built for review; host tests pass; **ESP32 compile pending locally; not
flashed; not field tested; not field accepted.** Next evidence is a track test.
