# Implementation plan — NAVI waveform recognizer, Option B (simple matcher)

Branch: `impl/navi-waveform-recognizer-manual-20261027`
(branched from `experiment/navi-manual-frozen-baseline-20261026` @ `621a029`,
the current manual-only sketch.)
Design: `docs/navi/NAVI_WAVEFORM_RECOGNIZER_ARCHITECTURE_20261027.md`
(Revision 2 + amendment, commit `8ac4aec` on the design branch), and
Claude's single-scale recognizer design
(`docs/navi/NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md`).
Assignment: David's implementation assignment, 2026-10-27 (Option B).

This plan supersedes the earlier draft plan written for the Revision 2
matcher. The matcher is deliberately simpler now: one target, confirm on
agreement, record on contradiction. No bounded sequence re-alignment, no
rearview mirror, no competing hypotheses, no automatic geographic
correction. Those remain research; the experiment establishes whether
they are needed.

## Baseline and recovery

- **Baseline:** the manual sketch at `621a029` — navigation-only,
  AUTO/GO hard-refused (`AUTO_GO_PERMITTED` is false), no protective
  stop. It is the current manual sketch. It was never flashed and never
  compiled (per David's message of 2026-10-26); the last known-working
  field build is its parent, `a8154b5` (the 2026-10-24 full CW lap,
  every marker confirmed, zero non-qualifying, zero abandoned).
- **Recovery path:** the known-working build is the parent commit. To
  restore it: check out `a8154b5` in the sketch directory and reflash.
  Nothing in this branch touches the parent's history. The handoff
  gives the exact commands.

## Scope

- **Phase 1 — host first (design §10).** The portable recognizer core
  plus a host replay harness. It must reproduce the single-scale
  research scores and the causal figures on recorded Otto and Toby
  data. No locomotive needed.
- **Phase 2 — manual-only firmware.** Integrate into the manual sketch:
  the recognizer (hallTask conditions, rvmTask correlates), the
  Observation record, the simple next-target matcher, and diagnostic
  telemetry. The 70×2 onset detector stays as a diagnostic cross-check
  only; it must not veto or modify the recognizer's decisions.
- **Not in scope.** Any change to the map (`RouteMap.h`), the governing
  specifications, or automatic operation. No flashing — that is David's
  step. No track test by Muse. No IR anywhere. No timing gate, no
  interval-derived scaling, no adaptive scales, no failure-detection
  machinery beyond the single duplicate guard below.

## The recognizer (unchanged from the design)

- Per 1 ms tick, the median of the 5 Hall reads; pairs averaged to a
  500 Hz series (2 ms bins).
- One fixed 240 ms template (the 945-curve mean), pad 0.5, level-only
  normalized correlation.
- Thresholds r ≥ 0.7 and |A| ≥ 70 ADC counts (starting parameters).
- Local maxima of |r| with ±120 ms non-maximum suppression; greedy
  causal declaration (t_passage + 359 ms).
- Recognition is independent of map position, expected polarity, IR,
  and geographic timing.
- Event: passage time, recognition time, polarity (sign of A), score r,
  amplitude A, scale 0.24 s, sequence number.
- Template: `NaviWaveformTemplate.h`, generated from the research data
  (core 120 samples, peak 1.0021 at index 61; `NAVI_TEMPLATE_NORM` =
  5.241398 — the least-squares amplitude is dot(x, Tn) / NORM; omitting
  the division overstates A ~5.24×, caught by the smoke test).
- Core: `NaviMatchedFilter.h`, no Arduino dependencies, shared by the
  host harness and the firmware. Smoke test verified: synthetic
  North/South bumps give r = ±1.000, A = 169.9 / −179.9 (true ±170/180),
  passage times exact, recognition at passage + 359 ms, 2 events, no
  false positives.

## The matcher (Option B — the simplification)

- State: last confirmed marker, next expected marker and its mapped
  polarity, direction. The chronological record of recognized
  encounters (Observation record).
- **Expected polarity:** confirm the target, advance to the next mapped
  magnet.
- **Contradictory polarity:** record the observation and the
  discrepancy. Retain the last confirmed position; leave the expected
  target unchanged. Never reinterpret or erase a credible observation
  because it contradicts the map.
- **Duplicate guard (separate from recognition, provisional).** An
  advance is refused when the previous advance's passage time is less
  than 400 ms earlier. The observation is still recorded and published
  (outcome: duplicate-guard), only the advance is refused. The 400 ms
  value is NOT an established hardware-validated constant: it is
  derived from the fastest observed traverses in the event ledger
  (~650 mm/s over the 280 mm minimum spacing ≈ 430 ms), and the
  separation from genuine fast traverses (earliest observed 466 ms
  peak-to-peak) is narrow. The track test must measure the real
  fast-traverse intervals. If the guard ever fires on a genuine magnet,
  that is visible in telemetry and is evidence the constant is wrong.
- **No re-alignment, no rearview mirror, no competing hypotheses.**
  A desync stays visible until the operator re-declares. That is the
  point of this experiment: to see whether the simple matcher holds
  geographic continuity on its own.
- Telemetry: every observation published with its outcome (confirmed /
  discrepancy / duplicate-guard), the expected and observed polarities,
  and the resulting position — so the exact point of divergence is
  diagnosable.

## Files

- **New, portable core (no Arduino dependencies):**
  - `NaviWaveformTemplate.h` — the template (generated, verified).
  - `NaviMatchedFilter.h` — conditioning, correlation, NMS, causal
    declaration. (Draft reviewed and smoke-tested; amplitude fix applied.)
  - `NaviWaveformMatcher.h` — the simple matcher, Observation record,
    duplicate guard. (To write.)
  - `NaviDiagnosticLevel.h` — a trailing median of tick medians, as the
    level source for the 70×2 diagnostic only. (To write; ~30 lines.)
- **Host harness (phase 1):** a program that feeds recorded sessions
  through the portable core and scores the events against the research
  reference (to write; reuses the research corpus loading).
- **Changed in the manual sketch (phase 2):**
  - The `.ino`: hallTask computes the tick median and sends it to the
    median queue (and keeps the 70×2 diagnostic); rvmTask drains the
    median queue, runs the recognizer, and passes declared events to
    the matcher; the loop publishes the new event rows. The
    Tracing/blockQ path (which fed only the retired baseline) is
    removed. `NaviBaseline`, `NaviTarget`, `NaviEncounter` and the
    timing gate are retired as navigation inputs; the survey table
    files stay in the tree, unconsulted.
  - Telemetry (reconciled with the repo's hard constraint,
    `docs/CLAUDE.md`: existing MQTT topic names and payload formats do
    not change, unless a spec explicitly says otherwise — and the
    assignment explicitly asks for diagnostic telemetry). Same topic
    names (`state/nav`, `diag/navi_encounter`, `mm/marker`). `state/nav`
    keeps its fields and order: `confirmed` counts advances;
    `nonqualifying` counts recorded polarity discrepancies (observations
    that did not confirm the target — the same core meaning as before,
    with timing no longer a cause); `abandoned` is always 0 (no encounter
    packaging in this build); the `timing_*` fields are 0/false (no
    timing gate in this build); the `blocks_*` fields are 0 (no block
    pipeline). `diag/navi_encounter` keeps its existing fields and gains
    the new event fields (sequence, passage/recognition times, score r,
    amplitude, outcome); `width_blocks`/`peak_adc` stay 0, as they
    already were in the frozen-baseline branch. `mm/marker` is still
    published on confirm. No command topic or payload changes; the
    operator drives the train exactly as before.
- **Unchanged:** `RouteMap.h`, the governing specifications, the AUTO/GO
  refusal, manual throttle/direction/stop, MQTT, ESP-NOW, E-stop,
  battery protection, NSR recording, the station/bubble machinery.

## Verification

- **Host harness:** Level 1 (22/22, 0 false) plus validation and
  transfer sessions (Otto and Toby). Score polarity, duplicates,
  misses, false events, and passage-time accuracy against the
  single-scale reference (causal held-out 5,999/6,027, 8 false,
  1 duplicate). A focused implementation check, not a new optimization
  campaign.
- **ESP32 build:** attempt the Arduino toolchain install and a full
  sketch compilation in this environment. If the toolchain is
  unavailable, report that plainly; do not present host compilation as
  ESP32 compilation.
- **Audit:** Sam and Claude audit the firmware against the design and
  this plan — the same step that catches drift in Claude's code applies
  to Muse's.
- **Track test:** David's step, per the assignment §7 (Otto manual,
  PWM 90 first, then Toby and other speeds; MM066–069 stay as repaired).

## What Muse cannot do

- Flash the ESP32 or run it on the track.
- Verify the ESP32 build if the toolchain cannot be installed here.
- Decide the open questions: the 400 ms guard after its measurement,
  the stopping offset after the track test, whether geographic judgment
  is needed at all. Those remain David's, and the experiment is what
  answers the last one.
