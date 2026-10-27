# Handoff — NAVI waveform recognizer, manual-only experimental firmware

**To:** Sam and Claude (independent review)
**From:** Muse
**Date:** 2026-10-27
**Assignment:** David's implementation assignment of 2026-10-27 (Option B —
simple next-target matcher; no sequence re-alignment, no rearview mirror,
no automatic geographic correction).

## 1. Branch, commit, firmware variant

- **Branch:** `impl/navi-waveform-recognizer-manual-20261027`
- **Base:** `experiment/navi-manual-frozen-baseline-20261026` @ `621a029`
  (the current manual-only sketch). The last known-working field build is
  its parent, `a8154b5` (2026-10-24 full CW lap: every marker confirmed,
  zero non-qualifying, zero abandoned) — see §5 for the recovery path.
- **Commit:** [TO FILL — the implementation commit hash]
- **Firmware variant:** `NAVI_POWER_OF_NORMAL_MANUAL_ONLY`, experimental,
  manual only. `AUTO_GO_PERMITTED` is `false`; AUTO/GO are hard-refused;
  there is no protective stop. Locomotive is selected at compile time:
  default Otto (`-DNAVI_LOCO=9950011`); Toby with `-DNAVI_LOCO=9950012`.
- **Design reference:** `docs/navi/NAVI_WAVEFORM_RECOGNIZER_ARCHITECTURE_20261027.md`
  (Revision 2 + amendment, commit `8ac4aec` on the design branch) and
  Claude's single-scale recognizer design
  (`docs/navi/NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md`).
  Implementation plan: `docs/navi/NAVI_WAVEFORM_RECOGNIZER_IMPLEMENTATION_PLAN_20261027.md`.

## 2. What was built

A manual-only NAVI experimental sketch that recognizes magnetic encounters
from the continuous Hall signal with the agreed single-scale matched filter
and follows the DNA map with a simple next-target matcher.

**Recognizer** (`NaviMatchedFilter.h`, portable, no Arduino dependencies;
template in `NaviWaveformTemplate.h`, generated from the research data):
per 1 ms tick, the median of the 5 Hall reads; pairs averaged to a 500 Hz
series; one fixed 240 ms template, pad 0.5, level-only normalized
correlation; thresholds r ≥ 0.7 and |A| ≥ 70; local maxima of |r| with
±120 ms non-maximum suppression; greedy causal declaration at passage +
359 ms. Recognition is independent of map position, expected polarity, IR,
and geographic timing. Each event carries passage time, recognition time,
polarity (sign of A), score r, amplitude A, and scale 0.24 s.

**Matcher** (`NaviWaveformMatcher.h`, portable, no Arduino dependencies
except `RouteMap.h`): last confirmed marker, next expected marker and its
mapped polarity, direction. Expected polarity → confirm and advance to the
next mapped magnet. Contradictory polarity → record the observation and the
discrepancy; retain the last confirmed position; leave the expected target
unchanged. A duplicate-advance guard (separate from recognition,
provisional): an advance is refused when the previous advance's passage
time is less than 400 ms earlier; the observation is still recorded
(outcome `DuplicateGuard`). Every event is kept in the Observation record
(last 32, chronological) with its outcome. No re-alignment, no rearview
mirror, no competing hypotheses, no automatic correction.

**Integration** (`NAVI_POWER_OF_NORMAL_MANUAL_ONLY.ino`): hallTask computes
the tick median and sends it to rvmTask on a new queue (medianQ, depth
256); rvmTask feeds the recognizer and passes declared events to the
matcher. The 70×2 onset detector stays in hallTask as a **diagnostic
only**, fed by a trailing 1 s median level (`NaviDiagnosticLevel.h`); its
onsets are counted and logged to Serial and never reach the matcher.
Manual throttle, direction, stop, MQTT, ESP-NOW, E-stop, battery
protection, NSR recording, and the station/bubble machinery are unchanged.
`RouteMap.h` is unchanged. No IR anywhere. No timing gate, no interval
scaling, no adaptive scales.

**Retired** (deleted from the sketch directory; preserved in git history
at `621a029`): `NaviBaseline.h`, `NaviTarget.h`, `NaviEncounter.h`,
`NaviTimingPredictor.h`, `NaviTimingProfiles.h`, `NaviObservation.h`
(the frozen-median baseline, the PWM-90 timing gate, the encounter
packaging, and the 20 ms tracing — all superseded by the recognizer).
The survey table in `NaviTimingProfiles.h` is regenerable from
`docs/OTTO_70X2_ONSET_PWM90_TIMING_TABLE_20261025.{csv,json}`.

**Telemetry** (topic names unchanged; payload formats kept stable per
`docs/CLAUDE.md`, with additive fields only):
- `state/nav`: same fields and order; `confirmed` counts advances;
  `nonqualifying` counts recorded polarity discrepancies (observations
  that did not confirm the target — the same core meaning as before,
  with timing no longer a cause); `abandoned` is always 0 (no encounter
  packaging); `duplicate_guards` is new; the `timing_*` fields are 0/false
  (no timing gate) and the `blocks_*` fields are 0 (no block pipeline).
- `diag/navi_encounter`: same existing fields (`width_blocks`/`peak_adc`
  stay 0, as they already were; `meets_min_width` stays 1, vestigial)
  plus the recognizer's event fields: `seq`, `passage_us`,
  `recognized_us`, `score_r`, `amplitude_adc`, `outcome`
  (`CONFIRMED` / `NONQUALIFYING` / `DUPLICATE_GUARD` / `OBSERVATION`).
  `opening_us` is the passage (template-peak) time; `opening_pwm` is the
  PWM at recognition time. Authority is now `WAVEFORM_RECOGNITION`.
  Published to `mm/marker` on confirm, as before.
- `state/loopstat`: `block_q_drop`/`onset_q_drop`/`blocks_*` replaced by
  `median_q_drop` (median-queue drops), `diag_onsets` (70×2 diagnostic
  onset count), and `duplicate_guards`.
- Boot payload: `recognizer` is now
  `NAVI_WAVEFORM_MATCHED_FILTER_SINGLE240`.

**Host verification** (`tests/navi_waveform_recognizer/`, run with
`sh run_checks.sh`): unit tests for the matcher (confirm / discrepancy /
duplicate guard / suspend / Observation record), the diagnostic level,
the median-of-5, and the recognizer smoke test; a replay harness that
feeds recorded 1 ms tick medians through the portable core; a scorer
against the research reference. Fixtures are in `tests/.../fixtures/`
(Level 1 ticks + reference, and the CSV panels).

## 3. Verification results

### Host verification — completed

- **Unit tests:** all pass (`medianOf5`, `DiagnosticLevel`,
  `WaveformMatcher`, recognizer smoke). The smoke test gives exact
  passage times, recognition at passage + 359 ms, r = ±1.000,
  A = 169.9 / −179.9 for true ±170/180.
- **Level 1 replay (Otto, the 96.8 s NSR file, 20 route-anchored
  reference magnets MM037–MM056):** **20/20 found, 0 missed,
  0 polarity errors, 0 false events in the span.** Passage timing error
  −0.2 to +2.1 ms (mean +0.4 ms). Latency exactly 359 ms for every
  event. The core reproduces the research's single-scale reference
  (the research's own Level 1 result was 22/22, 0 false — 20 magnets
  plus 2 unscored; this core finds the same 22 events).
- **Panels robustness (the plotted-samples CSV, 7 panels):** 10 events.
  Panels at PWM 0 (stationary, 4 panels): **0 events** — no false
  events while stationary. PWM 90 (1 panel): 7 events, r 0.987–0.997,
  spaced ~1.0–1.2 s. PWM 50: 2 events, r 0.76–0.77. PWM 21–45: 1 event,
  r 0.74. The weaker scores at low PWM are the known single-scale
  behaviour (slower passages are wider than the 240 ms template); all
  still pass r ≥ 0.7.

### ESP32 compilation — completed, succeeds for both locomotives

- **Toolchain:** arduino-cli 1.3.1; ESP32 core **3.3.12**; xtensa toolchain
  `esp-x32` 2601; `esp32-libs` 3.3.12; `esptool_py` 5.3.1. All four
  downloads were checksum-verified against the board-manager index.
  The VM's egress proxy blocks arduino-cli's own downloads (the Go HTTP
  client gets a TLS handshake error for `downloads.arduino.cc` and
  `espressif.github.io`, while `curl` works), so the core, tools, and
  libraries were fetched with `curl` and assembled by hand into
  `~/.arduino15` — the same files and versions the board manager would
  have installed. arduino-cli's built-in `ctags` (5.8-arduino11) was
  fetched the same way. No core version is pinned in the repo; 3.3.12
  is the latest.
- **Libraries:** PubSubClient 2.8, Adafruit INA219 1.2.3, Adafruit BusIO
  1.17.4, Adafruit BusIO dependencies (Adafruit Sensor), all from
  GitHub master. `credentials.h` for the compile test is a copy of the
  inert `credentials.example.h` (git-ignored, not committed).
- **Otto** (`-DNAVI_LOCO=9950011`): compiles. Sketch uses **1,027,211
  bytes (78%)** of program storage (max 1,310,720); global variables
  **67,612 bytes (20%)** of dynamic memory (max 327,680), leaving
  260,068 bytes. `.bin`, `.elf`, and merged `.bin` produced.
  **Zero warnings** in the whole build log.
- **Toby** (`-DNAVI_LOCO=9950012`): compiles, identical size
  (1,027,211 bytes, 78% flash, 67,612 bytes RAM).
- **One build note (environment, not a sketch defect):** with core
  3.3.12 and Adafruit_BusIO 1.17.4 the build needs **`-DESP32`** added
  to the build flags, because core 3.3.12 no longer defines the plain
  `ESP32` macro that BusIO tests for (`#elif defined(ESP32)` in
  `Adafruit_SPIDevice.h`). Without it the build fails inside
  Adafruit_BusIO (`'BitOrder' does not name a type`) — the sketch code
  itself is not at fault. The 2026-10-24 build did not need this flag
  (a previously-flagged BusIO/core incompatibility "did not reproduce"
  then), so the difference is the core/library versions, not the
  sketch. The flag is a standard platform macro that many libraries
  expect; defining it is benign. The flashing instructions in §5
  include it.

### Not verified here (unavailable checks)

- **Full-corpus replay (validation + transfer, including Toby):** the
  other 38 NSR captures are not on this machine (only the Level 1 file
  is). The harness accepts any `.ticks.bin` produced by
  `make_tick_medians.py`, so the full replay is a matter of supplying
  the files — it was not done here and the single-scale figures for
  validation/transfer are the research's, not a re-measurement of this
  code. **This is the largest unverified item.**
- **ESP32 hardware behaviour:** heap, task timing, the median queue
  under load, and the recognizer running on the chip can only be shown
  by flashing and running. Not done (flashing is David's step).
- **Track test:** not done. The first test is Otto, manual, PWM 90
  (assignment §7).

## 4. Known limitations, deviations, and unresolved questions

1. **Gap handling (a genuine implementation correction, found by the
   replay).** The draft core treated any tick arriving > 2 ms after the
   previous tick as a gap and invalidated 240 ms of correlation windows
   around it. The tick clock jitters by up to ~4.5 ms in the recorded
   data (FreeRTOS scheduling), so that fired 344 times in 96.8 s and
   missed 13 of 20 magnets. Fixed: a gap is now a tick jump > 100 ms
   (20× the largest observed jitter), and on a real gap the core resets
   to a fresh start instead of invalidating a sliding window. Short gaps
   need no handling (the correlation is over consecutive fed bins; the
   passage time comes from the bin's own timestamp). After the fix:
   20/20. **Reviewers: please check this reasoning** — it is the one
   place where the implementation diverges from the design's gap
   discussion, and it was driven by the recorded data, not by the
   design.
2. **Amplitude scaling (found by the smoke test).** The draft computed
   A as dot(x, Tn), overstating it ~5.24×. The least-squares amplitude
   is A = dot(x, Tn) / ||T − mean(T)|| (norm 5.241398, now
   `NAVI_TEMPLATE_NORM` in the template header). Fixed and verified
   (A = 169.9 / −179.9 for true ±170/180).
3. **Scorer boundary (found by the replay).** The first version of the
   scorer excluded events outside the scored span before matching, so
   the last magnet (MM56, recognized 2 ms past the span's edge) was
   reported as missed. Fixed: matching is done first, the span only
   decides whether an unmatched event is false or unscored.
4. **Duplicate guard (400 ms) is provisional, not hardware-validated.**
   Derived from the fastest observed traverses in the event ledger
   (~650 mm/s over the 280 mm minimum spacing, ~430 ms peak-to-peak),
   with the earliest observed genuine traverse at 466 ms. The margin is
   narrow. The track test must measure the real fast-traverse intervals;
   if the guard ever fires on a genuine magnet, that is visible in
   telemetry (outcome `DUPLICATE_GUARD`) and is evidence the constant
   is wrong.
5. **Single-scale limitation (by design, not a bug).** Slower passages
   (PWM below 90) are wider than the 240 ms template and score lower
   (r 0.74–0.77 at PWM 21–50 in the panels replay; the research's
   6-scale detector used 0.38 s for the PWM-64 MM56). The first test is
   at PWM 90 for this reason. Whether the single scale is enough at
   lower speeds is a track-test question.
6. **Merged-signal condition is untested.** No raw Hall trace of the
   2026-10-25 12:06 MM108–120 failure exists; the recognizer has not
   been replayed over it. The track test should watch MM108 and the
   MM107–113 same-polarity stretch.
7. **The 70×2 diagnostic level is a new, simple trailing median**
   (1 s window, recomputed 10×/s). It is diagnostic only and never
   gates the recognizer, but its behaviour has not been compared
   against the old baseline's level on a run.
8. **Session length:** event times are uint64_t (no 71-minute wrap),
   but the matcher's Observation ring holds only the last 32 events.
9. **The old host tests** (`tests/navi_power_of_normal/unit.cpp`,
   `replay.cpp`) did not compile against the retired APIs (pre-existing,
   H2 from the 2026-10-26 review) and have been replaced by the new
   harness in `tests/navi_waveform_recognizer/`. The old test directory
   is left untouched.
10. **The old host tests** (`tests/navi_power_of_normal/unit.cpp`,
   `replay.cpp`) did not compile against the retired APIs (pre-existing,
   H2 from the 2026-10-26 review) and have been replaced by the new
   harness in `tests/navi_waveform_recognizer/`. The old test directory
   is left untouched. The sketch README has been updated to describe
   the waveform recognizer (its "What is new", "What is NOT done",
   "Testing", and field-status sections); the 2026-10-24 field-status
   note is preserved as history.

## 5. Flashing, starting a manual test, collecting telemetry, restoring

**Prerequisites:** Arduino IDE (or arduino-cli) with the ESP32 core 3.x,
libraries `PubSubClient`, `Adafruit_INA219` (and its dependencies
`Adafruit_BusIO`, `Adafruit_Unified_Sensor`), and a `credentials.h` in
the sketch directory (copy `credentials.example.h` and fill in the Wi-Fi
credentials — do not commit it).

**Flash Otto (first test):**
```
cd firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY
# default build is Otto (NAVI_LOCO=9950011); no edit needed
# Arduino IDE: select the ESP32 board, the USB port, Upload.
# arduino-cli: arduino-cli compile --fqbn esp32:esp32:esp32 \
#   --build-property build.extra_flags=-DNAVI_LOCO=9950011 . \
#   && arduino-cli upload -p <port> --fqbn esp32:esp32:esp32 .
```
For Toby, build with `-DNAVI_LOCO=9950012`.

**Starting a manual test (Otto, PWM 90 first):**
1. Power Otto on the track, stationary, and confirm it connects
   (MQTT `state/bootid` shows
   `recognizer":"NAVI_WAVEFORM_MATCHED_FILTER_SINGLE240"`).
2. From the operator console, set the session direction (CW) and declare
   the start marker (e.g. `start_mm` 44, as in the 2026-10-25 run) — the
   same declare flow as before. `state/nav` should show `TRACKING`,
   `mm` 44, `target` 45.
3. Drive manually to PWM 90 and run the loop. Do not enable AUTO or GO
   (they are refused by the firmware).
4. Watch `diag/navi_encounter`: one row per recognized magnet, with
   `result` `CONFIRMED` and the correct `mm` advancing in order. A
   `NONQUALIFYING` row (polarity disagreement) or `DUPLICATE_GUARD`
   row is evidence to note, not a fault — record where it happened.
5. MM066–069 stay as repaired (mapped polarities). Do not reverse them
   for this test.

**Collecting telemetry:** the Pi logger records the MQTT topics as
before (`state/nav`, `diag/navi_encounter`, `mm/marker`,
`state/loopstat`), and the NSR recorder streams the native Hall data to
the Pi (`/home/david/NGR/telemetry/runs/`). After the run, the log files
and the NSR capture are the evidence — send them to Muse for the
marker-by-marker analysis, as with the 2026-10-25 run.

**Restoring the prior working build:** the known-working firmware is the
parent commit `a8154b5` (the 2026-10-24 manual sketch). To restore it:
```
cd firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY
git checkout a8154b5 -- .        # restores the sketch to the known-working state
# then rebuild and reflash as above (Otto default)
git checkout impl/navi-waveform-recognizer-manual-20261027 -- .   # to come back
```
(The `-- .` form restores only this sketch directory; it does not move
the branch. The full history, including the frozen-baseline branch at
`621a029`, is preserved.)

## 6. What Sam and Claude should examine

- **`NaviMatchedFilter.h`** — the core. In particular:
  - the gap handling (pushTickMedian, kGapUs = 100 ms, reset on a real
    gap) and the reasoning in the comment — this is the correction the
    replay forced, and it is the most consequential judgment call in
    the implementation;
  - the causal declaration (decideReadyCandidates, rules (a) and (b))
    against the research's `causal_events` semantics — is the
    "stronger knowable candidate" rule implemented as the research
    defines it (earlier candidates only, or all knowable ones)?;
  - the amplitude (correlateCentre, A = dot / NAVI_TEMPLATE_NORM).
- **`NaviWaveformMatcher.h`** — the simple matcher, the Observation
  record, and the 400 ms duplicate guard (its justification and whether
  400 ms is the right provisional value).
- **`NAVI_POWER_OF_NORMAL_MANUAL_ONLY.ino`** — the integration:
  hallTask (tick median, diagnostic 70×2), rvmTask (medianQ drain,
  recognizer, matcher), the retired machinery's removal (is anything
  still referenced that should not be?), and the telemetry changes
  (state/nav, diag/navi_encounter, loopstat) against the repo's
  constraint that topic names and payload formats do not change.
- **The host verification** (`tests/navi_waveform_recognizer/`):
  re-run `sh run_checks.sh` and confirm 20/20 on Level 1; check the
  scorer's span handling and the panels robustness results.
- **The gap between this verification and the research's full-corpus
  figures:** this code has been replayed on Level 1 (Otto) and the
  panels only. The validation/transfer re-measurement (§8.1.4 of the
  design, against the single-scale stream) still needs the NSR files.
  If you have the 38 captures, please run the harness over them.
- **The README** (sketch README) has been updated to describe the
  waveform recognizer; check that its "What is new" section matches
  the code, and that nothing in it still describes the retired
  machinery.
