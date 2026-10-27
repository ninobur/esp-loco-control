# NAVI_WAVEFORM_0_1

Sketch entry point: `NAVI_WAVEFORM_0_1.ino` in this folder.

EXPERIMENTAL. Clean-sheet NAVI built from
`docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md`, authorized
2026-10-24, corrected the same day against real field runs and the
governing proposals. Not field accepted for AUTO. **AUTO and GO are hard-
refused** — `navi_power::AUTO_GO_PERMITTED` is `false` and `handleCommand()`
checks it directly; it cannot be bypassed from MQTT. Manual throttle,
direction, declare, E-STOP and battery protection work normally, and this
is how the build is meant to be operated right now.

**2026-10-27 — waveform recognizer.** The 70×2 onset / frozen-baseline /
PWM-90 timing machinery described below as "What is new" has been retired
and replaced by a single-scale matched-filter waveform recognizer with a
simple next-target DNA matcher (see the new "What is new" section, and
`docs/navi/NAVI_WAVEFORM_RECOGNIZER_HANDOFF_20261027.md`). The locomotive
is selected at compile time with `-DNAVI_LOCO=9950011` (Otto, default) or
`-DNAVI_LOCO=9950012` (Toby).

## What is new

`NaviMatchedFilter.h`, `NaviWaveformTemplate.h`, `NaviWaveformMatcher.h`,
`NaviDiagnosticLevel.h` — built from the matched-filter research
(`research/navi_matched_filter/REPORT.md`) and the architecture
(`docs/navi/NAVI_WAVEFORM_RECOGNIZER_ARCHITECTURE_20261027.md`), following
Claude's single-scale recognizer design
(`docs/navi/NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md`).

- **Waveform recognition** (`NaviMatchedFilter.h`, `NaviWaveformTemplate.h`):
  per 1 ms tick, the median of the 5 Hall reads; pairs averaged to a 500 Hz
  series; one fixed 240 ms magnetic waveform template, pad 0.5, level-only
  normalized correlation; acceptance r ≥ 0.7 and |A| ≥ 70 ADC counts; local
  maxima of |r| with ±120 ms non-maximum suppression; greedy causal
  declaration at passage + 359 ms. Recognition is independent of map
  position, expected polarity, IR, and geographic timing. Each event
  carries passage time, recognition time, polarity, score r, and amplitude.
- **Simple matcher** (`NaviWaveformMatcher.h`): last confirmed marker, next
  expected marker and its mapped polarity. Expected polarity → confirm and
  advance. Contradictory polarity → record the observation and the
  discrepancy; retain the last confirmed position; leave the expected
  target unchanged. No re-alignment, no rearview mirror, no competing
  hypotheses, no automatic geographic correction. A provisional
  duplicate-advance guard (400 ms, separate from recognition) refuses an
  advance when the previous advance's passage was less than 400 ms
  earlier; the observation is still recorded.
- **Diagnostic 70×2 onset detector** (`NaviCandidate.h`, unchanged): still
  runs in hallTask, fed by a trailing 1 s median level
  (`NaviDiagnosticLevel.h`), counted and logged to Serial only. It never
  reaches the matcher and never vetoes the recognizer.
- **Retired** (deleted; preserved in git history at `621a029`):
  `NaviBaseline.h`, `NaviTarget.h`, `NaviEncounter.h`,
  `NaviTimingPredictor.h`, `NaviTimingProfiles.h`, `NaviObservation.h` —
  the frozen-median baseline, the PWM-90 timing gate, the encounter
  packaging, and the 20 ms tracing. The survey table is regenerable from
  `docs/OTTO_70X2_ONSET_PWM90_TIMING_TABLE_20261025.{csv,json}`.

## What is reused unchanged, from r5.1 commit `5b7a518b`

`RouteMap.h`, `RvmStations.h`, `RvmOps.h`, `NaviEstop.h`, `NaviWordStorage.h`,
`NaviSyncRecorder.h`, `LocoConfig.h`, both `LL_LocoConfig_*.h`.

## What was removed

- **The arrival-boundary protective stop** (formerly `NaviArrivalBoundary.h`,
  Proposition 6B) — removed 2026-10-24 on David's instruction: "missing a
  magnet is not our problem; correctly recognizing it is. This is planning
  to fail." There is no automatic-stop mechanism of any kind in this build
  now. `AUTO_GO_PERMITTED` stays `false` independent of this removal — it
  was never tied to that mechanism's internals, and this change does not
  authorize AUTO.

## What is NOT done

- **PWM-60 and lower speeds**: the single 240 ms template is built from
  PWM-90 passages. Slower passages are wider and score lower (r 0.74–0.77
  at PWM 21–50 in the panels replay; the research's 6-scale detector used
  0.38 s for the PWM-64 MM56). The first track test is at PWM 90 for this
  reason. Whether the single scale is enough at lower speeds is a
  track-test question, not a dry-lab one.
- **CCW**: declared successfully (suspends and re-declares correctly on a
  session-direction change) but not yet driven/observed in the field —
  untested, not known-broken.
- **Merged-signal condition**: no raw Hall trace of the 2026-10-25 12:06
  MM108–120 failure exists; the recognizer has not been replayed over it.
  The track test should watch MM108 and the MM107–113 same-polarity
  stretch.

## Field status, 2026-10-27

Not yet flashed or run. Host verification (`tests/navi_waveform_recognizer/run_checks.sh`):
Level 1 replay (Otto, 96.8 s NSR) finds 20/20 reference magnets, 0 missed,
0 polarity errors, 0 false events; the panels replay produces no false
events while stationary. Full-sketch ESP32 compile succeeds for both Otto
and Toby (arduino-cli, ESP32 core 3.3.12, zero warnings; 985,931 bytes,
75% flash, 67,508 bytes RAM). No `-DESP32` flag is needed. See
`docs/navi/NAVI_WAVEFORM_RECOGNIZER_HANDOFF_20261027.md` for the full
compile, flashing, and recovery instructions.

## Field status, 2026-10-24

Flashed and run on Otto. A full, uninterrupted CW lap (177 markers,
121→170, wrap through 0, to 126) confirmed every marker with zero
nonqualifying and zero abandoned. Two earlier, interrupted CW runs showed
two markers (52→53, 57→58) stalling for tens of seconds under reduced
throttle through a curve — resolved by operator throttle compensation on
the clean run, not a code defect. Full-sketch arduino-cli compile succeeds
for the ESP32 target (confirmed 2026-10-24; a previously-flagged
Adafruit_BusIO/core incompatibility did not reproduce).

## Testing

`tests/navi_waveform_recognizer/run_checks.sh` — host-side unit tests for
the recognizer and matcher, plus a replay of the Level 1 native Hall
recording (Otto, 96.8 s) scored against the research reference, and a
robustness replay of the plotted-samples panels. No hardware or network.
The old host tests in `tests/navi_power_of_normal/` do not compile against
the retired APIs and are superseded by the new harness.
