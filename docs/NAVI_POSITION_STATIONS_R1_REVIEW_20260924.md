# POSITION_STATIONS_R1 — Independent Review (Claude)

Date: 2026-09-24. Reviewed: `57d34d6` (Stations.h, .ino stationService/GO/
correction path, tests). Context: audit `5cff759`. No flash, no hardware,
no code changed by this review.

## Verdict

Sound for its declared scope. Two items for the operator before flash:
one latch (pre-existing, but it is exactly what a paused-AUTO build will hit)
and one policy question about late acquisition past the stop point.

## Reproduced

- `test_station_position` (ASan/UBSan, -Werror): pass.
- `test_station_correction`: pass (32).
- `tools/test_position_station_integration.py`: pass.
- ESP32 compile not re-run.

## Confirmed correct

- GO no longer writes cruise90; `stationService()` runs before `serviceRamp()`
  in `loop()`, so no transient generic-cruise step exists.
- Region acquisition −10..+5 from current MM, any phase entry; matches the
  MISSED boundary (> +5), so there is no gap or overlap. Regions of the four
  stations do not overlap in either direction.
- Pause: `setRunning()` runs before the `autoRunning` early return and before
  the consensus copy, so the shifted watchdog survives into `stationMachine`.
  Dwell keeps wall-clock time as documented.
- Per-loop `requestPwm()` is idempotent (does not touch `lastStepMs`);
  highest AUTO target is 110 < ceiling 120, so no per-loop cap printf.
- Events only on transitions; steady orders carry `event=nullptr`.
- Completed-visit memory: set at stopAt+3, held to +5, cleared on leaving
  the region. Next lap serves the station again.

## Finding 1 — failed visit survives withdrawal and blocks re-GO (latch)

`stationConsensus()` returns `agrees=false` on MISSED/PHASE_TIMEOUT and
`stationService()` withdraws without adopting `decision.next`. The machine
keeps the old phase. Nothing except a declaration/direction change
(`carryResetRequest`) resets it — not `auto 0`, not `dispatcher_release`,
not manual driving.

Probe (`stationConsensus` as firmware uses it, Patio CCW):

| Step | Result |
| --- | --- |
| Approach at MM22, STOP, train ends at MM9 (+6) | GO → withdraw |
| Re-enroll, GO at MM9 again | withdraw again |
| Re-enroll, GO at MM40 (upstream of −10) | runs (reevaluated) |

Same for PHASE_TIMEOUT: active time already >120 s stays consumed after the
pause shift, so every re-GO times out again.

Pre-existing in structure, not introduced here. But it is the "recovery
bundles decisions / clearing requires redeclaring a known position" item
from the audit, and this build's premise (pause is not failure) makes it
the most likely way to strand Toby. Candidate fixes, for operator choice:
reset the station machine when consensus fails, or on disenroll.

## Finding 2 — late acquisition past the stop point stops past the platform (policy)

With no memory of a completed visit, any position from the stop offset to
+5 produces ZERO_RAMP → 5 s DWELL → DEPART. Cases:

- GO while parked at Patio CCW MM11 (+4): 5 s dwell, then departs.
- Declaration clears `completedIdx_` (`reset()`), so redeclaring at
  MM12 right after a completed Patio visit serves Patio a second time.

Documented in R1 and 0101 as "existing +5 recovery region", so not a
defect. It is an operator question: is "past the stop point" a stop
requirement at that MM, or already missed? Decision 0101 records the
principle, not this boundary.

## Field observations to watch (not defects)

- Restart from zero inside the approach uses approach pacing as the up-rate
  (e.g. MM24 CCW: target 78 at ~256 ms/count, ~10 s before Toby moves).
  Zone restart 0→60 at 200 ms/count ≈ 12 s. Consistent with "slow ramp" rulings.
- `SEQUENCE_CORRECTED.armed_after` is always 0 (declared).

## Recommendation

Ask the operator on Finding 1 (fix now or accept as an open issue) and
Finding 2 (policy). Neither blocks the field gate in R1 as written;
Finding 1 only needs a redeclaration to clear if hit.
