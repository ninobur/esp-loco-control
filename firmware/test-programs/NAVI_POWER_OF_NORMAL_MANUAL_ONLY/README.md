# NAVI_POWER_OF_NORMAL_MANUAL_ONLY

EXPERIMENTAL. Clean-sheet NAVI built from
`docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md`, authorized
2026-10-24, corrected the same day against real field runs and the
governing proposals. Not field accepted for AUTO. **AUTO and GO are hard-
refused** — `navi_power::AUTO_GO_PERMITTED` is `false` and `handleCommand()`
checks it directly; it cannot be bypassed from MQTT. Manual throttle,
direction, declare, E-STOP and battery protection work normally, and this
is how the build is meant to be operated right now.

## What is new

`NaviCandidate.h`, `NaviEncounter.h`, `NaviBaseline.h`, `NaviTarget.h`,
`NaviTimingPredictor.h`, `NaviTimingProfiles.h` — built directly from the conference record and the
integrated implementation proposal, not adapted from `RvmShape.h` /
`RvmContinuity.h` / `RvmNavigator.h`, which both independent audits of r5.1
(Claude and Codex, 2026-10-24) found in active conflict with the agreed
design.

- **Onset detection** (`NaviCandidate.h`): 70-count, two-consecutive-sample
  sentinel at the established 1 kHz cadence, comparing against the live
  baseline level. No lock, no reference of its own — a candidate while
  another encounter is unresolved does not stop detection.
- **Baseline** (`NaviBaseline.h`): the long, inter-magnet measurement.
  Collection begins the instant the signal *appears* to re-enter ordinary
  track (one block within margin of the committed level) — not gated on
  `NaviEncounter`'s own confirmation — and runs, PWM-gated, to the next
  genuine opening, where the median of everything collected is promoted.
  Retains the previous level only when literally zero samples were
  collected (e.g. a fully stationary dwell). Verified independent of the
  short measurement's timing, both in host tests and against 198 real
  encounters (entry always preceded confirmation by ~200-300ms, zero
  counterexamples).
- **Encounter** (`NaviEncounter.h`): the short, local measurement — observed
  return to the committed baseline level, no timeout.
- **Target** (`NaviTarget.h`): one expected target; nonqualifying evidence
  leaves it unchanged; no Contradiction/search/recovery state.
- **Timing as qualification evidence** (`NaviTimingPredictor.h`,
  `NaviTimingProfiles.h`): the interval clock runs from the native timestamp
  of the last *confirmed* 70x2 opening to the current candidate's native
  70x2 opening. A nonqualifying observation never resets it. Profiles are
  independently keyed by source marker, direction, and fixed PWM, so a CW
  90-PWM value is never used for CCW or 60-PWM travel. The catalogue is
  intentionally empty until field data establishes normal variation; until
  then every measured interval is visibly `UNPROFILED`, not silently passed
  by a guessed tolerance.

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

- **Minimum physically plausible width** (`NaviEncounter.h`,
  `MIN_WIDTH_BLOCKS`): mechanism present, value intentionally `0`
  (disabled).
- **Timing profiles and acceptance bands**: the field test must establish
  them from clean fixed-PWM CW and CCW runs at 60 and 90. No values are
  invented in firmware.
- **CCW**: declared successfully (suspends and re-declares correctly on a
  session-direction change) but not yet driven/observed in the field —
  untested, not known-broken.

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

`tests/navi_power_of_normal/run_checks.sh` — host-side unit tests for all
five headers plus a replay of the real boot-1 native Hall corpus. No
hardware, no network.
