# Independent review — NAVI Four-Station Local, Pass 2

**Date:** 2026-10-09
**Reviewer:** Claude (Claude Code), read-only review requested by David
**Target:** branch `codex/navi-four-station-local-pass2`, commit
`9130cc55701b868f58defd1d3cd9c4175f8aaf16`, parent/baseline
`c7c21163b016e443524eb9fa483f8b706ce76e22`
**Governing documents:** commit `7a31d913ef2218687b488936a998f82655f5d3c6`
(`docs/simplified-ir-loss-fallback`): Document C §§8–35, October 8 decisions,
Decisions 0116, 0120, 0121, 0123, `AGENTS.md`
**Scope limits:** no code was modified, compiled, run, flashed or deployed.
Line numbers refer to the target commit.

## Verdict

**FAIL — not suitable for a controlled Otto field test.**

The architecture is right: legacy station machinery is gone and geography
selects the instruction. Three deterministic controller defects, each confirmed
by reading the code, would appear on the first station visit or the first
stop/restart.

## Confirmed compliance

- `StationMachine`, `EwoStationStopProfile` and `EwoStationStop.h` are not
  included by the sketch. There is no ARMED state, Station-0 Hall gate or
  station timeout. The header and its test remain in the tree as history only.
- `FourStationGeography::evaluate` is pure: `(mm, direction)` → instruction.
  It is direction-aware, wraps correctly through `routeMod`, needs no approach
  history, and is unaffected by missed-magnet progression except through `mm`.
- One actuator path: a single `requestPwm()` call site; `serviceRamp()` is
  unchanged. E-stop and low voltage still take precedence inside `serviceRamp()`.
- IR loss at steady speed holds PWM (`NaviFourStationLocal.h:100`). During a
  glide, PWM reduction continues and is never increased (`:85`). No ticker tape,
  PWM history, prediction or Hall-checkpoint logic exists; §35.8 is left open.
- Outside the controller, the sketch changed only the include, the static
  instance, `reset()` on STOP, and the `serviceAutoCruise()` body. MQTT,
  ESP-NOW, declaration/reversal, Decision 0120, manual control and the recorder
  are untouched. `tick()` is O(1) and non-blocking.
- Exactly four files changed; no credentials or binaries were committed.

## Findings

### BLOCKER 1 — A correct stop at Station +1.5 never dwells or departs

- **Where:** `NaviFourStationLocal.h:35–36`, `:66`, `:85`.
- **Code behaviour:** `mm` is the last marker passed, so offsets 0–1 (between
  marker C and C+2) are all `FinalStop`. Dwell and departure exist only in the
  `StopTile` branch, which requires offset 2 (past marker C+2). A train that
  stops correctly at C+1.5 still reads offset 1, and `FinalStop` then requests
  `max(0, 0−1) = 0` indefinitely. Only an overshoot past C+2 at non-zero PWM
  reaches the dwell code; at PWM 0, Hall is ignored and IR movement triggers
  Decision 0120 instead.
- **Requirement:** Document C §35.1 (stop at Station +1.5, dwell 5 s, depart);
  §22 (accept a physical stop).
- **Consequence:** Otto stops at the first station and stays stopped in AUTO.
- **Smallest correction:** apply stop/dwell/depart execution when physically
  stopped in the final region, or at the IR-measured +1.5 point, rather than on
  MM tile +2. What counts as "at the station" is left by §22 to a field-tested
  definition and needs David's decision.

### BLOCKER 2 — A start from standstill inside a glide region never moves

- **Where:** `:85`.
- **Code behaviour:** both glide regions always request `actualPwm − 1`, which
  is 0 from standstill: GO at Station −9, STOP/GO anywhere from −10 to +2, or a
  glide that reaches PWM 0 early (for example after IR loss).
- **Requirement:** Document C §15 (a stationary locomotive at Station −9
  "accelerates gradually toward 20 pKPH"); §23 and §25 (GO re-evaluates from
  the actual physical state).
- **Consequence:** stuck in AUTO until manual intervention.
- **Smallest correction:** below the region's terminal speed, use the ordinary
  150 ms/count acceleration toward the terminal target. For the final region
  this is the same question as Blocker 1.

### BLOCKER 3 — The acceleration ramp is never handed off; homeostasis can only lower PWM

- **Where:** `:101`, `:106`.
- **Code behaviour:**
  1. Below 95% of target, the code requests `cruisePwm` (90). Above 95% it
     returns −1, leaving `rampTarget` at 90, so the ramp keeps climbing past
     95%, including during IR loss. In the 20-pKPH zone this ramps toward
     cruise PWM.
  2. The 95% test uses one observation, not the five-observation median: any
     single low reading restarts the ramp.
  3. Samples are stored only at ≥95% of target, so the +1 branch at `:106` is
     unreachable. PWM 90 becomes a ceiling and homeostasis only lowers PWM.
- **Requirement:** §35.4 (ramp until 95%, then homeostasis); §17 and §35.2
  (±1 corrections in both directions from the median of five).
- **Consequence:** overshoot and hunting in the station zone and at cruise.
  `RouteMap.h` records NAVI_2 stalling at PWM 100 on the CW Grillers climb and
  the operator's 110 setting there; the removed `cruisePwmAt()` supplied it.
  Pass 2 cannot exceed 90 on that grade.
- **Smallest correction:** at the first observation ≥95%, request `actualPwm`
  to freeze the ramp; thereafter homeostasis alone governs, including +1, and a
  low reading does not restart the ramp.

### MAJOR 1 — The glide cannot hold PWM; its rate limits are unapproved

- **Where:** `:85`, `:92–95`, `:120`.
- **Code behaviour:** PWM always falls by one count; "too slow" only stretches
  the step, to at most 400 ms. The 50–400 ms limits, ±25 ms adjustment, 150 ms
  starting step and 1-pKPH tolerance floor appear in no decision. The 150 ms
  start is not the established manual braking, which follows the brake setting
  (15–400 ms).
- **Requirement:** §35.3 ("flatten **or suspend** further PWM reduction"); §13
  (settle on the target if reached early); §35.4 (initial pacing is manual
  braking).
- **Consequence:** reaching 20 pKPH early in the approach keeps PWM falling and
  may stall short of −5 (physical outcome not verified; missing hold confirmed).
- **Smallest correction:** allow a hold while slow; David to approve or set the
  numeric limits.

### MAJOR 2 — Glide distance is measured from where the glide began

- **Where:** `:114`, `:118–119`.
- **Code behaviour:** fixed `5×span(C)` or `1.5×span(C)` from the glide's entry
  point, not remaining distance to the geographic endpoint; one interval's span
  stands in for all five approach intervals (surveyed 280–360 mm).
- **Requirement:** §9 and §35.3 (remaining geographic distance).
- **Consequence:** mid-region entry shifts the endpoint. In the best case the
  +1.5 representation is off by the difference between `1.5×span(C)` and
  `span(C) + ½×span(C+1)`, a few tens of mm.
- **Smallest correction:** anchor to the Hall-confirmed IR coordinate of the
  region's start marker and sum actual mapped spans.

### MAJOR 3 — Execution state survives everything except STOP

- **Where:** `.ino:604`, `:611`, `:568`, and the e-stop, low-voltage and
  `withdraw()` paths; only `stop` calls `reset()`.
- **Example:** an e-stop during dwell, then GO 10 s later, departs immediately
  because the stale `dwellSinceUs_` already exceeds 5 s.
- **Requirement:** §25; October 8 Decision 2 (dwell starts when control returns).
- **Smallest correction:** `fourStationLocal.reset()` in the GO handler.

### MAJOR 4 — "Fresh observation" means a radio packet, not a spoke

- **Where:** `:56`, `:102–104`.
- **Code behaviour:** freshness is a new Type-5 sequence number (the code's own
  comment at `:59` says a packet is not a movement pulse). `irSpeedMmS()` is a
  0.9–1.5 s sliding window, so five packets (about 0.5 s at an inferred
  ~100 ms cadence) are largely overlapping, and the five-packet moratorium ends
  before the window holds post-correction motion.
- **Requirement:** the review brief's "five-spoke moratorium"; Document C's
  "5 IR beats".
- **Needs David's decision:** whether a beat is a spoke, a packet or a fresh
  independent speed window.

### MINOR

1. Three-consecutive counter is not consecutive: fast and slow do not reset
   each other (`:93`).
2. Glide start speed is invented (45 or 20) when IR is invalid (`:115`).
3. The `remaining == 0` → 0 request at `:89` is overwritten next tick by
   `actualPwm−1`.
4. Station-zone samples persist into the median after departure (`:111` does
   not run within one station); currently the only way +1 can fire.
5. Dwell requires a fresh IR packet after `reset()` (`:68`); the first packet
   is also counted as movement.
6. Boundaries are bare literals (`:31–36`), not named parameters (§35.1).
7. `README.md:6,71` still says StationMachine is retained (pre-existing).
8. Tests cover four exact offsets, one homeostasis decrement and one dwell on
   tile +2. Untested: stopping in the final region, standstill start in a glide
   region, the 95% handoff, any +1 correction, STOP/GO and e-stop resets, IR
   loss and recovery at steady speed, offsets −6, −1 and +1.
9. Build figures (1,018,851 bytes / 77% flash; 58,188 bytes RAM) rest on
   CODEX's report; no artifacts were available and no build was run.

**Hypothesis, not confirmed:** if the fastest glide step (50 ms/count) reaches
PWM 0 while Otto still rolls, Decision 0120 withdraws AUTO at the platform.

## Summary answers

| Question | Answer |
|---|---|
| Remaining legacy station authority | None |
| One coherent PWM authority | One actuator path, but not one coherent authority (Blocker 3) |
| Manual and MQTT independence | Preserved; command latency under AUTO still needs a bench check |
| Suitable for a controlled Otto test | No |

## Minimum corrections before field testing

1. Blocker 1, with David's definition of a station stop.
2. Blocker 2.
3. Blocker 3.
4. Major 3 (reset on GO).
5. Major 1 hold, with approved numeric limits.
6. Majors 2 and 4: decide or explicitly defer.
7. Tests for each of the above.

Follow-up: the glide-specific corrections are reviewed in
[`NAVI_FOUR_STATION_LOCAL_PASS2_GLIDE_FIX_REVIEW_20261009.md`](NAVI_FOUR_STATION_LOCAL_PASS2_GLIDE_FIX_REVIEW_20261009.md).
