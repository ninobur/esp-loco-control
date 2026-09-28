# NAVI_EYES_WIDE_OPEN: change record and audit

> **Governing principles:** [`NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`](../../../../../docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md)
> and [`NAVI_DECISION_MODEL.md`](../../../../../docs/NAVI_DECISION_MODEL.md).
> This build applies 0102–0108. It departs from nothing knowingly. Where the
> implementation still conflicts with a governing document, §15 says so.

2026-09-28. Implemented by Claude on task branch `claude/wizardly-turing-2axvng`
at David's instruction (the "NAVI_EYES_WIDE_OPEN build" prompt of 2026-09-28).
**Not flashed. Not field tested. Not field accepted. ESP32 compile not yet
performed** (the cloud container cannot download the Espressif toolchain). Do
not flash Toby or Otto until David and Sam have reviewed it.

| Item | Value |
|---|---|
| Build name (`SKETCH_NAME`, on `state/bootid`) | `NAVI_EYES_WIDE_OPEN` |
| Sketch | `firmware/programs/NAVI_COHERENCE/variants/NAVI_EYES_WIDE_OPEN/NAVI_EYES_WIDE_OPEN.ino` |
| Behavioral base | `NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3_SYNC_R1` (20Q3 + NSR1), branch `claude/navi-ps-r1-20q-standards` at `3d6a1d3` |
| Not based on | NAVI_ONE X22, X22R (both are prior art; X22R's header is not included) |
| Rollback | flash the 20Q3 sketch (`NAVI_COHERENCE_0_6_IR_HEALTH`, unchanged in this commit), or Toby's last verified installed build |
| Target | Toby (9950012) only, as 20Q3 |

## Repository state found (reported, not repaired)

The governing documents named in the prompt (0102–0108, the decision model, the
reconciliation plan, both 2026-09-27/28 field-record analyses) and the 20Q3
sketch exist only on `origin/claude/navi-ps-r1-20q-standards`. `main` does not
contain them. `main` has 14 commits (IR bench tools, IR docs) that that branch
does not contain; the three agent-policy commits exist on both lines with
identical content but different hashes. So neither branch contains the other.
The NAVI work was therefore based on `claude/navi-ps-r1-20q-standards` at
`3d6a1d3`. The designated task branch had held only `main`'s history; it was
re-pointed to that base. Nothing was merged, rebased or deleted on any other
branch.

---

## 1. Reconciliation: governing decision → 20Q3 → conflict → disposition

| Governing principle / decision | 20Q3 implementation | Conflict | Disposition in this build |
|---|---|---|---|
| **0102** NAVI is the sole navigation decision authority; X22 is prior art; no shadow (D7) | X22R on core 0 held the baseline lock cycle, the 400 ms window that suppressed openings (N1), PWM-0 dwell and old-field holds (N2, N3), CADENCE/MOVING/STOPPED gates (N4–N6), a cycle keyed to raw openings (N7), SETTLE (N8), prime, lost-lock. NAVI saw only the openings X22R released | **Yes** | X22R removed from the sketch. The Hall task only acquires. NAVI reads every native sample, measures openings against its own reference, and judges every opening. No shadow comparison |
| **0103** Direct physical measurement before proxy | Baseline collection placed by the clock (711 ms after the opening); 3 s cadence as a motion proxy; PWM > 25 as a motion proxy; 80 ms SETTLE as a distance proxy | **Yes** | The reference collection is placed by IR route distance (100–200 mm). No time or PWM gate exists in the Hall path. The 650 ms Hall-only fallback stays as degraded-mode evidence (0093) |
| **0104** Evidence Accumulation Before Revision; loss of information does not invalidate knowledge | Missed MM → sans-MM advance (kept). Wrong polarity inside the window → accepted (ADVANCED_WITH_DISCREPANCY) | Partial | Missed MM unchanged. With valid distance a wrong-polarity opening is OBSERVED and HELD, not accepted; if the window is traversed, its polarity enters the proximal history. The incumbent Hall reference is retained whenever a new one cannot be measured |
| **0105** Evidence is relational, not votes | IR distance already constrains which MM an opening can be | No | Kept. Distance constrains, polarity is a characteristic of the expected MM, PWM 0 removes authority. No score introduced |
| **0106** Motive PWM 0 is a non-navigation interval | X22R *suppressed* openings at PWM 0 (upstream). IR pulses at PWM 0 counted as route travel in `traverse()` and the ±15% window | **Yes** | Openings at PWM 0 reach NAVI and are retained as `RETAINED_PWM_ZERO_NO_AUTHORITY`. `RouteDisplacement` excludes pulses measured at motive PWM 0 from route displacement (window, traversal and reference collection) |
| **0107** The Hall baseline is spatial; boot reference permitted; RTB open (D4); held-vs-continuous open (D8) | X22R locked a 200-sample collection on a timetable; 2 s prime | **Yes** | NAVI-owned spatial reference: clearance 0–100 mm, collect 100–200 mm, median at 200 mm (operator specification for this build). Stationary 2000-sample boot reference. No RTB recognizer, no tail recognition |
| **0108** Absence of measured change is not sensor failure | IR health: `INADEQUATE_CONTRAST` → instrument not ready → Epoch ends | Partial, **not changed** | In the Hall path, zero route displacement is a stationary span (a valid result): the collection waits and nothing is inferred. IR health classification is unchanged; see §15 |
| **0093** 650 ms Hall-only fallback | NAVI, only without MM-referenced distance | No | Retained unchanged |
| **0073** median of five conversions per reading (status: PROPOSED) | `hallRead()` | No (acquisition) | Retained; documented in §6 |
| Handoff §14: wrong polarity is retained as contradictory evidence | Accepted with discrepancy on both paths | Changed by this prompt | IR-coherent path: held. Hall-only path: unchanged (review item, §15) |

## 2. Build path and name

`firmware/programs/NAVI_COHERENCE/variants/NAVI_EYES_WIDE_OPEN/NAVI_EYES_WIDE_OPEN.ino`,
`SKETCH_NAME "NAVI_EYES_WIDE_OPEN"`. It sits at the same depth as the 20Q3 folder,
so the shared relative includes (`../../../QUORUM/credentials.h`, `NAVI_SIMPLIFIED`
profile, `reference/…/IR_ARCHITECTURE_0_4`, `common/`) are unchanged.

## 3. Files

**New:** `NaviHall.h` (sample ring, opening observer, spatial reference),
`RouteDisplacement.h` (PWM-0 exclusion), `tests/test_eyes_wide_open.cpp`, this
record, `README.md`.

**Copied from 20Q3 and changed:** `NAVI_EYES_WIDE_OPEN.ino` (Hall task, loop,
telemetry), `Navigator.h` (judgment, route displacement, held contradiction),
`IrHealthMonitor.h` (one read-only accessor, `clockOffsetUs()`), `LocoConfig.h`
(comment; `NAVI_HALL_DEPART_COUNTS` moved to `NaviHall.h`), `NaviSyncRecorder.h`
(one comment), `tests/test_20q_standards.cpp`, `tests/test_coherence.cpp`.

**Copied unchanged:** `IrSpeedTelemetry.h`, `MovementEvidence.h`, `Ops.h`,
`ProximalRecovery.h`, `RecoveryControl.h`, `RouteMap.h`, `Stations.h`, and the
tests `test_proximal_recovery`, `test_station_correction`, `test_station_position`,
`test_navi_stop_display`, `test_ir_speed`, `test_ir_health_monitor`,
`test_navi_sync_recorder`, `test_health_json.py`.

**Not copied:** `ExcursionDetectorX22R.h`, `HallObserver.h`,
`tests/test_x22r_equivalence.cpp`, `CHANGES_20Q3.md` (history; it stays in the
20Q3 folder). Nothing outside this folder was changed except the firmware catalog row.

## 4. Hall data path, ADC to NAVI

```
core 0, hallTask, 1 kHz (acquisition only)
  hallRead(): 5 ADC conversions of GPIO33 in ~100 us, median           [§6]
  HallSample{tUs (esp_timer), raw, applied PWM} -> hallRing (1024)
  same reading -> NSR1 recorder (unchanged, observation only)

core 1, loop()
  serviceMovement(): each accepted IR packet -> onIrPoint()
     navigator.observeIr(point, motivePwmZeroSincePrevious)   route account
     hallReference.irPoint(point, localUs, route, hallRing)   reference collection
  serviceHall(): every sample since the last pass, in order
     before the boot reference exists: hallReference.bootSample()
     afterwards: openingObserver.sample(sample, hallReference.value())
         -> opening (70x2, polarity) -> judgeOpening() -> Navigator::judge()
         -> accepted: hallReference.acceptedOpening(serial, NAVI's sync point)
  serviceTraversal(): navigator.traverse(latest IR point)
  stationService() ...
```

NAVI receives every native reading. The only thing that stands between a
reading and NAVI is the median of five conversions inside that reading.

## 5. X22R authority removed

| X22R mechanism | Now |
|---|---|
| Baseline acquisition, CLEAR/SETTLE/collection/QUIET/LEASH, lock | gone; NAVI's `SpatialReference` (§7) |
| Prime (shadow median of 25 ms medians of three, 2 s) | gone; NAVI boot reference = median of the first 2000 native samples (§7) |
| 400 ms acquisition window and its suppression (N1) | gone; every opening reaches NAVI |
| PWM-0 dwell hold (N2), old-field hold (N3) | gone; openings at PWM 0 reach NAVI and carry no authority (0106) |
| CADENCE (N4), MOVING (N5), STOPPED (N6) | gone; the collection is gated only by IR route displacement |
| Cycle keyed to raw openings (N7) | gone; the collection restarts only on a NAVI-accepted opening |
| Lost-lock recovery, width floor, `adjustBaseline`, shadow median, stale flag | gone |
| Window morphology (peak, sum, widths, window polarity) and `nav/hall_window` | gone; no shape has authority anywhere |
| `diag/hall_decision` (baseline outcomes) | replaced by `nav/hall_reference` (NAVI's reference events) |
| `nav/suppression` (declared, never published) | removed: nothing is suppressed |
| `reset()` force-arming on declaration | gone; arming is a signal fact only |

Nothing was renamed and reproduced. What survives from X22 is knowledge the
prompt told us to preserve: the 70-count departure, two-sample same-sign
persistence, polarity fixed at the opening, and "an opening is a transition"
(§13).

## 6. Processing below NAVI

Exactly one step: **one reading = the median of five consecutive ADC
conversions, taken within about 100 µs** (`hallRead()`, decision 0073).

- *What:* rejects a single wrong conversion or sub-100 µs glitch inside one 1 ms
  reading. It never combines two readings; the 1 ms sample stream is otherwise raw.
- *Why necessary:* the 2026-09-02/03 bench test found 4 bad single conversions
  in 3,731 flat-top samples, 0 in 5,113 with the median; the fault appears only
  under a magnet's field, which is where the 70-count test is made. The 70×2
  opening's field support (117/117, 926/926 on 2026-09-26) and the NSR1 spatial
  analysis were both obtained on median-of-five readings. Removing it would
  change the input those results describe; that would be a new experiment, not
  the elimination of an old detector's preference.
- *Why acquisition:* it characterizes one reading of one conversion channel. It
  knows nothing of references, magnets, position or time between readings.
- *Note for review:* 0073 is recorded with status PROPOSED. It has been in every
  field build since X11.

Removed as detector-motivated: X22's 25 ms medians of three, its rolling prime
median and its lock collection.

## 7. The 100–200 mm spatial reference (`NaviHall.h`, `SpatialReference`)

Specification (operator, 2026-09-28): after an accepted 70×2 opening, 0–100 mm
of IR-measured travel is clearance; 100–200 mm is collected; at 200 mm the
median of the collected Hall observations becomes NAVI's reference for the
next MM. These are distances, not timers or percentages, and no tail is recognized.

- **Anchor.** When NAVI accepts an opening and synchronizes it to an IR point
  (the same MM/IR synchronization the ±15% window uses), the reference begins
  at that point. An accepted opening without an IR point starts no collection
  (`NO_IR_POINT_AT_OPENING`); the incumbent reference is kept.
- **Route distance** is `RouteDisplacement::routeUm(anchor, point)`: IR pulses
  since the anchor, minus pulses measured while motive PWM was 0 (0106), times
  the pitch.
- **Spans.** On every accepted IR point the collection walks the native samples
  captured since the previous point (IR capture time mapped to the local clock
  by the existing Hall/IR alignment offset). If route distance did not advance,
  the span is stationary and its samples are skipped (counted as
  `stationary_skipped`). If it advanced, each sample's route distance is
  linearly interpolated inside the span; samples at 100 ≤ d < 200 mm are added.
- **Close.** When a point reaches d ≥ 200 mm, the median of the collected
  samples (exact, 4096-bin histogram; even count = mean of the two middle
  values, rounded down) becomes the reference (`REFERENCE_SET`).
- **End without a new reference** (incumbent kept, reason published): a later
  accepted opening, a declaration or reversal, an Epoch break, a Hall sample
  gap, route distance unavailable, or zero samples collected.
- **Stationary dwell.** A 60 s stop inside the interval advances neither the
  interval nor the population (tested at PWM 0 and at PWM 40, with a +180-count
  standing level shift and hand rotation of the wheel at PWM 0).
- **Boot.** Until the first collection closes, the reference is the stationary
  boot reference: the median of the first 2000 native samples read by NAVI,
  under the operator's clear-of-magnets condition (0107). No opening exists
  before it. `hallReady` (which AUTO requires) means this reference exists.
- **Not done:** no sans-MM advance opens a collection (D8 stays open; the
  specification says "after an accepted opening"). No minimum sample count,
  bins, adaptive bounds or no-IR substitute.

## 8. MM judgment path (`Navigator::judge`)

For each opening, with the IR point nearest its time (≤150 ms, unchanged):

1. No position, or direction conflict → `NO_POSITION` (unchanged).
2. **Motive PWM 0 at the opening** → `RETAINED_PWM_ZERO_NO_AUTHORITY`. Reported
   on `nav/discrepancy` and `mm/marker`; position, synchronization, history untouched.
3. **Valid same-Epoch MM-referenced route distance d**, cumulative mapped
   distance D to the expected MM:
   - expected MMs whose whole window lies behind d are traversed sans MM first (unchanged);
   - d < 0.85D → `NON_LANDMARK_HALL`, reason `TOO_EARLY_BY_DISTANCE`;
   - 0.85D ≤ d ≤ 1.15D and **opening polarity ≠ expected** → `WRONG_POLARITY_HELD`:
     the expected MM is unchanged; the first held polarity for that MM is kept;
   - 0.85D ≤ d ≤ 1.15D and polarity = expected → `ADVANCED` (the expected MM),
     new MM/IR synchronization, reference collection restarts.
4. **No valid distance** (no reference, no IR point, Epoch break): 650 ms from
   the previous accepted opening (0093), then accepted; a wrong polarity here
   is still `ADVANCED_WITH_DISCREPANCY` as in 20Q3 (review item, §15).

Every ruling carries `judgment_reason` on `nav/discrepancy`. Proximal recovery
(0098) is unchanged and still evaluates on accepted MMs.

## 9. Missing MM

Unchanged from 20Q3, plus the held contradiction. When the expected MM's whole
window (1.15D) is traversed with no accepted opening, NAVI advances
`POSITION_ADVANCED_SANS_MM`: position known, history retained, MM/IR
synchronization unchanged (cumulative distance continues from it). The history
records UNKNOWN, or the held wrong polarity if there was one
(`nav/sans_mm.hall_evidence = HELD_WRONG_POLARITY`). The next accepted MM
resolves it. The ten-consecutive AUTO limit (20Q3 ruling 6) remains; it ends
AUTO, it is not LOST. There is no ONE STRIKE path.

## 10. Remaining clock/time mechanisms in the Hall/navigation path

| Mechanism | Where | Class | Reconciliation |
|---|---|---|---|
| 650 ms Hall-only fallback | `Navigator.h` | **A** | Decision 0093; 0103 names it as the degraded-mode timing used only without MM-referenced distance. It is the only A item |
| 1 kHz sampling, `vTaskDelayUntil(…,1)` | `hallTask` | B | sampling |
| Median of five within ~100 µs | `hallRead` | B | §6 |
| Boot reference = first 2000 samples (2 s) | `NaviHall.h` | B | instrument initialization at a stationary point (0107); gates only "no opening before a reference exists" |
| IR point ↔ Hall alignment ≤150 ms | `IrHealthMonitor::at/acceptedMm` | B | measurement correspondence (unchanged) |
| IR link freshness 1 s → Epoch end | `IrHealthMonitor::tick` | B | communications; loss of distance, not of position |
| IR capture → local clock offset | `IrHealthMonitor` | B | clock domain mapping (§15, drift) |
| Frame-start timestamp (stale openings) | `.ino` | B | ordering across a declaration (was `navEpoch`) |
| Sample-ring capacity ~1 s | `.ino` | B | transport; overrun → Hall evidence lost (§12) |
| Hall-derived speed (< 30 s) | `.ino` | C | telemetry only |
| IR speed qualification 3 s / 2 s, stopped display | `IrSpeedTelemetry.h` | C | display only |
| Status/telemetry cadences 1 s/5 s/200 ms; NSR1 status | `.ino` | C | |
| Station dwell 5 s, steps 200 ms/count, watchdog 120 s, approach pacing | `Stations.h` | D | actuation; unchanged |
| Manual/AUTO/brake ramps | `.ino` | D | unchanged |
| Wi-Fi/MQTT timers | `.ino` | D | unchanged |

No refractory, settle, quiet, cadence or window remains.

## 11. Remaining physical proxies

| Proxy | Where | Stands for | Status |
|---|---|---|---|
| 650 ms since the last accepted opening | `Navigator.h` | distance | degraded mode only (0093/0103) |
| Motive PWM 0 → no route movement | `Navigator.h`, `RouteDisplacement.h` | movement | decided rule (0106), not a proxy by the decision's own terms; listed for completeness |
| PWM attribution of IR spans by loop observation, not capture time | `.ino` `irPwmZeroSeen` | PWM at the time the pulses occurred | conservative: a span that touched PWM 0 anywhere is excluded |
| `admitDeclaration`: standing still = PWM 0 | `Ops.h` | stationary | operator-command gating; unchanged |
| Stopped display from commanded/applied PWM 0 + quiet IR | `IrSpeedTelemetry.h` | stopped | display only; unchanged |
| Hall speed = mapped span / elapsed time | `.ino` | speed | telemetry only |
| Station approach pacing (marker/time table with PWM model) | `Stations.h`, profile | stopping distance | actuation, open-loop stop position; unchanged |
| Boot reference sample count | `NaviHall.h` | "enough" stationary samples | instrument initialization |

No PWM, time or sample count stands in for distance anywhere in the reference collection or in the IR-referenced judgment.

## 12. Remaining upstream or hidden decision authority

Paths outside `Navigator::judge()` that can affect navigation:

| Path | Effect | Why it remains |
|---|---|---|
| Opening = transition: no second opening until the signal returns inside 70 counts | an excursion that never returns inside 70 yields one opening | this is what "opening" means; it is a signal fact. It would hide a second magnet only if two fields merged without a return inside 70 counts; not observed on NGR |
| No openings before the boot reference | ~2 s at boot | a departure needs a reference |
| Openings from samples captured before a declaration/reversal are counted `stale_frame`, not judged | drops evidence from the old frame | preserves 20Q3's `navEpoch` rule; the operator's declaration is authoritative |
| Hall sample overrun → `navigator.haltForLoss()` (LOST), declare required | invalidates navigation knowledge | carried from 20Q3's queue-overflow rule; with evidence missing, NAVI cannot claim continuity. Now depends on the loop never stalling ~1 s (§15) |
| `IrHealthMonitor` Epoch end (stale link, health fault, `INADEQUATE_CONTRAST`) | NAVI loses MM-referenced distance (not position) | instrument continuity; see §15 for 0108 |
| `RouteDisplacement` exclusion | PWM-0 pulses cannot move position or the collection | 0106 |
| Stations / withdraw paths | end AUTO | AUTO authority, not position (unchanged) |
| Declaration / redeclaration | sets position | operator authority (0106), unchanged |

No path outside NAVI can accept or reject an MM, advance/retreat/correct
position, alter opening polarity, or manufacture a recovery position.

## 13. Vestigial X22/X22R audit (active code)

| Item | What it does | Influences navigation? | Why it remains |
|---|---|---|---|
| 70-count departure, 2 same-sign samples, polarity at the second | the MM opening | yes, as the observation NAVI judges | prompt §6 and handoff §11: established knowledge |
| Rearm when the signal is inside 70 counts | one opening per excursion | yes (§12) | definition of an opening; not a timer |
| 2 s boot collection | boot reference | only "no opening before it" | X22 prime duration; 0107 permits a boot reference; the operator is already told "2 s, keep clear of magnets" |
| `nav/discrepancy` keys `window`, `window_valid`, `peak_signed` (always `null`) | none | no | payload shape unchanged for log readers, as in 20Q3 |
| `mm/marker` `base_open`, status `baseline`/`base_age_ms` | report NAVI's reference and its age | no | same keys, new source |
| Comments naming X22/X22R | provenance | no | |

No copied or renamed X22 logic remains: no lock cycle, window, hold, cadence,
lost-lock or morphology in any compiled file of this folder
(`test_eyes_wide_open.cpp` checks the source for the X22R API names).

## 14. Compile and test results (cloud, 2026-09-28)

Flags: `-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined` (g++ 13.3).
These prove the software implements the defined rules, not railway behavior (AGENTS.md §8).

| Test | Result |
|---|---|
| `tests/test_eyes_wide_open.cpp` (new) | PASS, 3,949 checks: source (no X22R; acquisition-only Hall task); 70×2 and polarity; wrong polarity held, distance-incoherent rejected, coherent+polarity accepted, PWM 0 retained, all 171 MMs both directions; PWM-0 pulses excluded; held contradiction into history; missing MM continuity; redeclaration; Hall-only unchanged; waveform rig (sketch loop order): normal run with a reference per interval equal to that interval's level, nothing collected before 100 mm, close ≥200 mm, next MM judged on the new reference, 60 s dwell at PWM 0 and 40, missing MM, wrong-polarity MM, declaration abandons, median |
| Mutation check on the new test | 13/13 mutants killed (100↔0 mm, 200↔250 mm, 69 counts, 1 sample, stationary spans collected, no rearm, polarity inverted, PWM-0 pulses counted, PWM-0 authority, wrong polarity accepted, held evidence dropped, declaration ignored, median altered) |
| `tests/test_20q_standards.cpp` | PASS: 2,394 window, 6 Hall-only/epoch, 242 traversal/limit. Changed: detector section removed (X22R); wrong polarity now `WRONG_POLARITY_HELD`; two fixtures made physical (distinct timestamps; no odometer running backwards in an Epoch) |
| `tests/test_coherence.cpp` | PASS: 2,052 offset corrections, 6,840 single-misread holds (unchanged counts). Changed: two IR-coherent wrong-polarity assertions now HOLD |
| `tools/test_nav_decisional_inertia.cpp` (unmodified, against this Navigator) | PASS: 342 scenarios |
| proximal recovery, station correction, station position, stop display, IR speed, IR health monitor + JSON, NSR1 recorder | PASS, unchanged counts |
| `tools/test_position_station_integration.py`, `tools/test_manual_pwm.py` (paths pointed at this sketch, g++ for clang++, in scratch copies) | PASS |
| `tools/test_ir_stationary_{pipeline,revision,adversarial}.cpp` (same substitution) | PASS; adversarial 0 hard failures |
| Whole-sketch host syntax/type check against Arduino/ESP-IDF stubs | PASS; the only warnings are two pre-existing `-Wmisleading-indentation` in unchanged code, identical for 20Q3 |
| **ESP32 compile** | **not performed**; first real compile must be local |

Reproduce (repository root):

```sh
V=firmware/programs/NAVI_COHERENCE/variants/NAVI_EYES_WIDE_OPEN
F="-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined"
for t in test_eyes_wide_open test_20q_standards test_coherence test_proximal_recovery \
         test_station_correction test_station_position test_navi_stop_display test_ir_speed \
         test_ir_health_monitor test_navi_sync_recorder; do
  c++ $F -I $V $V/tests/$t.cpp -o /tmp/$t && /tmp/$t $V; done
c++ $F -I $V tools/test_nav_decisional_inertia.cpp -o /tmp/tdi && /tmp/tdi
```

## 15. Remaining conflicts, risks and review items

1. **Wrong polarity on the Hall-only path (decision needed).** The prompt's
   test "wrong-polarity candidate does not become the expected MM" is applied
   where IR distance is valid. On the Hall-only path (after a declaration and
   after every Epoch break, including station stops) a wrong polarity still
   advances with discrepancy. Reason: without distance there is no traversal,
   so a held opening can never resolve; one genuine MM read with the wrong
   polarity (F05/F06 are documented cases) would leave NAVI one MM behind, and
   proximal recovery (0098), which needs the observed polarity in its history,
   would lose its input. The 0.5/20Q3 "corrected on the 10th point" behavior
   (2,052 test cases) depends on it. Applying the gate there is one line in
   `Navigator::judge()`. David to decide.
2. **Recovery evaluation after a held contradiction.** Proximal recovery still
   evaluates only when an MM is accepted. A position error in which every
   in-window opening has the wrong polarity would accumulate held
   contradictions and sans-MM advances until the ten-MM AUTO limit, without an
   evaluation. Not observed; not changed (AGENTS.md §8).
3. **0108 versus IR health.** `INADEQUATE_CONTRAST` still makes the IR
   instrument "not ready" and ends the Epoch (unchanged; 0108 authorized no
   behavior change). At a stop this ends MM-referenced distance and any
   collection in progress; the first MM afterwards is Hall-only (20Q3 ruling 2).
   The Hall path itself treats zero displacement as a valid result.
4. **The 100/200 mm specification and 0107.** 0107 says the sampling distance
   is "not specified", D4 (RTB/IR-region bounds) is OPEN and "no new …
   locomotive-specific distances are introduced for RTB". This build uses the
   operator's 2026-09-28 specification, which is not RTB recognition and not
   locomotive-specific. It resolves, for this build, the sampling distance and
   acquired-per-interval versus continuous (part of D8's prerequisite). It
   should be recorded as a numbered decision after review (AGENTS.md §7); this
   record does not claim to be that decision.
5. **Real-time dependency moved to the loop.** Openings are now found on the
   loop thread from a ~1 s ring. A loop stall of ~1 s loses samples, and NAVI
   goes LOST (visible, fail-safe). 20Q3 detected on core 0 and depended only on
   a 16-deep queue. Loop timing on the ESP32 is unmeasured.
6. **Clock mapping.** IR capture times are mapped with the minimum observed
   arrival − capture offset. Crystal drift between the two ESP32s is not
   tracked; tens of ppm is a few mm per minute of motion at running speed. The
   same offset already governs Hall/IR alignment.
7. **Anchor precision.** The collection starts at the IR point NAVI
   synchronized (nearest within 150 ms, as the ±15% window). At 220 mm/s one
   100 ms point spacing is ~22 mm.
8. **RAM.** About 16 KB (ring) + 8 KB (histogram) + 4 KB (route history) of
   new static memory; not yet measured on the ESP32. The X22R buffers are gone.
9. **Field risk (0102).** 20Q3's field record was achieved with X22R's gates.
   Openings those gates hid (the ~402 ms second lobes, PWM-0 field changes)
   now reach NAVI and are judged. The replay and host tests say they are
   rejected by distance or by PWM 0; only the railway can say whether an
   uncharacterized phenomenon was being hidden. Watch `nav/discrepancy`
   `judgment_reason` and `nav/hall_reference`.
10. **Historical impostor survivors.** The 2026-09-28 challenge left three
    moving candidates (X21 Grillers departure, two QUORUM rows) that pass every
    non-IR characteristic. This build judges them by IR distance; whether that
    rejects them is a track question.
11. **Telemetry changed.** Removed: `diag/hall_decision`, `nav/hall_window`,
    `nav/suppression`. Added: `nav/hall_reference`; `nav/discrepancy` keys
    `departure`, `pwm`, `expected`, `judgment_reason`; `nav/sans_mm`
    `hall_evidence`/`polarity` may now be `HELD_WRONG_POLARITY`/`N|S`;
    `state/loopstat` `baseline_diag_drop` → `route_excluded_pulses`,
    `hall_drop` now counts sample-loss events. `mm/marker` `gap_ms` now carries
    the time since the previous opening (20Q3 always sent 0). Boot record:
    `window_ms` → `hall_opening_samples`; new `ref_clearance_mm`,
    `ref_collect_end_mm`, `hall_detector:"NONE_UPSTREAM"`; `baseline` =
    `NAVI_SPATIAL_100_200MM_MEDIAN`. No `server/` code reads any of these (checked).

## Status

Built for review. Host tests pass. **ESP32 compile pending locally; not
flashed; not field tested; not field accepted.**
