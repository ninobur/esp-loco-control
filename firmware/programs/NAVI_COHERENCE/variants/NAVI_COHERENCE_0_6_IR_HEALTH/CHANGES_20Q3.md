# NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3 — change record

2026-09-25. Implemented by Claude on task branch `claude/navi-ps-r1-20q-standards`
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
- X22's own ~645 ms refractory stays disabled (`hallConfig()` sets 0). There is
  one timing gate.
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

## 2. Anachronistic mechanisms removed

| Removed | Was | Replaced by |
|---|---|---|
| Active 500 ms guard | `MIN_MARKER_MS=500`, both Hall-only paths | `HALL_ONLY_GUARD_MS=650`, Hall-only path only |
| `legacy_guard_500` | `nav/ir_compare` key evaluating `elapsed<500` | nothing; key removed |
| Window-polarity NAV authority | `hallSupports()` accepted the 400 ms window peak sign as supporting evidence | opening polarity at detection is the only Hall polarity with NAV authority (lineage principles 3, 4) |
| ~400 ms delay of NAV judgment | Hall task queued the event only after the acquisition window completed | the opening is queued at detection; the window follows as a separate record on `nav/hall_window` with `nav_authority:"NONE"` |
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
| `NAVI_COHERENCE_0_6_IR_HEALTH.ino` | Build name/subtitle; `hallConfig(NAVI_HALL_DEPART_COUNTS)`; Hall task queues at detection and sends window telemetry separately; loop passes the epoch point to NAVI, calls `serviceTraversal()` before `stationService()`, publishes sans-MM advances, applies the ten-MM AUTO limit via `withdraw()`; `nav/ir_compare` uses epoch distance and drops `legacy_guard_500`/`window_matches`; new topics `nav/hall_window`, `nav/sans_mm` |
| `Navigator.h` | 650 ms; exact ±15% semantics; `traverse()`; sans-MM queue; epoch MM synchronization; opening polarity only; `DistanceBasis`; `SANS_MM_AUTO_LIMIT` |
| `LocoConfig.h` | `NAVI_HALL_DEPART_COUNTS 70` |
| `tests/test_20q_standards.cpp` | new focused tests (below) |
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
| `tests/test_20q_standards.cpp` (new) | PASS: 2,394 window cases (every MM, both directions, wrap); 6 Hall-only/epoch; 242 traversal/limit; 6 detector (real X22) |
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
versus second qualifying sample; opposite-sign pair; X22 refractory disabled
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
  is their first compilation.

## 6. ESP32 compile

**Pending local verification** (David's Arduino environment). Not performed in
the cloud: its network policy blocks the Arduino/Espressif toolchain downloads,
and David directed not to change that.

## 7. Deliberately unchanged

PROXIMAL_R1 recovery; station behavior (targets, offsets, ramps, dwell,
POSITION_STATIONS_R1 logic); X22 detector code and its locked baseline; IR TX/RX
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
- `nav/ir_compare`: `legacy_guard_500` and `window_matches` removed; `branch`
  is always 0; `anchor_mm` is the synchronized MM; distances are epoch travel;
  `ir_quality` is the distance basis.

## 9. Known limitations and review items

1. **X22's 400 ms acquisition window still blanks new detections.** While it is
   open the detector declares nothing new and only counts suppressions. This is
   inherited X22 behavior, now separate from NAV judgment. A real next magnet
   inside 400 ms needs more than about 700 mm/s; Toby cruises near 240 mm/s.
   Not changed; recorded so it is not mistaken for NAVI's timing gate.
2. **IR TX loses contrast at stops** (final-run record: every stop ended its
   epoch). After each station stop, the first MM is Hall-only (650 ms) and no
   traversal occurs until re-synchronization. This is expected under the Epoch
   model, not a defect of this build.
3. **A within-epoch IR count jump would traverse windows.** For example the
   synthetic 10 m case: NAVI advances sans MM, and AUTO ends at ten. No physical
   rate guard was added (AGENTS.md §8; the lineage rejects a universal Vmax).
   Watch `nav/sans_mm` on track.
4. **Sans-MM advances move `navMm` without Hall.** Station logic (unchanged)
   acts on NAVI's position, including inside a station region.
5. Hall-interval speed (`telem/speed`) does not span a sans-MM advance: the
   next estimate restarts from the next accepted Hall.
6. The traversal ordering guard (detection numbered before timestamped) and the
   loop integration are verified by review only until local compile and track.

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
