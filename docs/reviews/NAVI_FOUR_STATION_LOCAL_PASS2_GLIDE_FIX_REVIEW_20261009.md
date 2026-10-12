# Independent review — NAVI Pass 2 glide-control corrections

**Date:** 2026-10-09
**Reviewer:** Claude (Claude Code), read-only review requested by David
**Target:** branch `codex/navi-four-station-local-pass2-glide-fix`, commit
`22b05a22d30d0ab76f5feb654a3e6f57a005545c`, compared with parent
`9130cc55701b868f58defd1d3cd9c4175f8aaf16` (stationless baseline `c7c21163`)
**Governing documents:** commit `7a31d913` as listed in the
[Pass 2 review](NAVI_FOUR_STATION_LOCAL_PASS2_REVIEW_20261009.md)
**Scope:** the three authorized corrections only — (A) flatten deceleration
when slower than the trajectory, (B) a genuine PWM plateau, (C) remaining
distance from NAVI's geographic position. No code was modified, compiled, run,
flashed or deployed.

## Verdict

**PASS WITH RESERVATIONS.**

All three corrections do what they set out to do, and the navigation core's own
behaviour is unchanged. Two in-scope problems need a fix or an explicit decision
(Majors 1 and 2). Field testing remains blocked by the known Pass 2 defects.

## Scope check

Exactly four files changed. In `NaviIntegratedCore.h` every added line writes
one of three new private fields (`geographicMarker_`, `geographicMarkerIrUm_`,
`geographicMarkerValid_`) beside an existing assignment, or belongs to the new
read-only `geographicPosition()`. No existing variable, condition or event
changed. The `.ino` change only fills controller inputs.

## Correction A — flattening

| Check | Result |
|---|---|
| Compared with trajectory at current geographic position | Yes (`NaviFourStationLocal.h:115–117`): constant-deceleration path from actual entry speed |
| ±5% tolerance | Yes; the 1-pKPH floor (`:118`) remains unapproved |
| Three consecutive observations | Yes (`:119–123`); fast and slow counters now reset each other |
| Plateau persists while slow | Yes |
| Convergence releases | Yes, on one in-band observation (Minor 2) |
| PWM never increases | Confirmed: requests are `actualPwm−1`, `0` or `actualPwm` |
| Holding PWM vs holding speed | Correctly distinguished: only PWM is held |

## Correction B — genuine plateau

Confirmed by reading the control flow (not by tests). While the plateau is set,
`:129` re-requests `in.actualPwm` every tick; `requestPwm()` sets `rampTarget`
to it; `serviceRamp()` returns at `actualPwm == rampTarget` (`.ino:243`).
Repeated ticks reissue the same value. Release returns to `actualPwm−1`, a
decrease. The `serviceRamp()` at the top of the loop (`.ino:1060`) can take one
last step from the previous tick's request before the plateau is first
requested; that step was already part of the established ramp. E-stop, low
voltage and manual precedence are unchanged.

**Evidence level:** the new test shows `tick()` *returns* `actualPwm` after
three slow observations and `actualPwm−1` after convergence. It holds
`actualPwm` constant and never exercises `requestPwm()` or `serviceRamp()`,
which have no host harness. Cancellation of a queued lower target rests on code
reading.

## Correction C — geographic remaining distance

- Endpoints: approach ends at `C−5·dir` (`:36`); the final glide ends at marker
  `C+dir` plus half the next interval in the direction of travel (`:39–41`),
  i.e. Station +1.5 represented between magnets with no further Hall needed.
  CCW indexing checked by hand (C=63: `SPACING[62]` + ½`SPACING[61]`).
- `mappedDistance()` sums surveyed intervals per direction; `routeMod` /
  `nextMarker` handle MM170→MM000 (tested 170→1).
- Origin is the confirmed Hall landmark's IR value (`Core:455–460`), NAVI's
  existing anchor; no second estimator.
- Mid-glide entry uses the actual remaining distance at entry; it is
  recomputed every fresh tick.
- Missed magnets: `geographicMarkerIrUm_` advances by the mapped interval
  (`Core:521`), the same IR-plus-map inference NAVI already uses; the remaining
  distance is continuous across the 15% detection delay.
- Validity is false when IR is stale, the IR/map relationship is unreliable,
  and after reversal until the next confirmation (`Core:121–124`) — except
  after declaration (Major 1).

## Findings

### MAJOR 1 — After declaration, the coordinate claims marker precision

- **Where:** `NaviIntegratedCore.h:69–71`.
- **Code behaviour:** `declare()` records the current IR value as the marker's
  origin and marks the coordinate valid, asserting that the locomotive is at
  marker `mm` now. `start_interval` declares a position *between* markers, and
  the core's own comment says the declaration origin is "not a known marker
  position".
- **Requirement:** represent uncertain position honestly; do not manufacture
  an IR origin (as this commit's reversal path correctly refuses to).
- **Consequence:** until the first Hall confirmation, remaining distance can be
  overstated by up to one interval (~300 mm); a stop could land near +2.5.
- **Smallest correction:** `geographicMarkerValid_ = false` in `declare()`.

### MAJOR 2 — IR loss during a plateau holds PWM indefinitely

- **Where:** `NaviFourStationLocal.h:113`, `:129`.
- **Code behaviour:** without fresh valid IR and a valid coordinate the update
  block is skipped, `plateau_` stays set and `:129` keeps requesting
  `actualPwm`; `mm` also stops advancing without IR.
- **Requirement:** Decision 0123 and Document C §35.6: during deceleration NAVI
  continues the established monotonic reduction and makes no feedback
  correction while IR is unavailable.
- **Consequence:** with IR lost mid-plateau, Otto holds plateau PWM through and
  beyond the station.
- **Smallest correction:** clear `plateau_` whenever IR speed or the coordinate
  is invalid. If David reads "already-established" as including the plateau,
  record that decision instead.

### MINOR

1. A glide whose entry tick lacks a valid coordinate never gets trajectory
   feedback, even after recovery (`:151`), contrary to 0123's resumption on
   recovery. Rare in normal running. Fix: capture entry distance at the first
   valid coordinate.
2. Plateau entry needs three observations, release one. Document C §12 asks for
   hysteresis; confirm intent.
3. "Too slow" no longer lengthens `decelStepMs_`, so the step only shortens
   within a glide (down to 50 ms). Values 150/50/25 ms remain unapproved.
4. At the final endpoint a plateau can override the `0` request (`:114`) for up
   to three observations.
5. Tests: nothing exercises `geographicPosition()` (declaration, confirmation,
   missed magnet, reversal); `evaluate()` endpoints are untested (tests build
   destinations by hand); `remainingDistance()` is tested CW only; no tests for
   pending-target cancellation, IR loss/recovery during a plateau, mid-glide
   entry, reversal before confirmation, MM031→MM032, or a missed magnet just
   before the endpoint. No Missed Magnet test exists in this commit.

## Navigation authority and legacy machinery

Unchanged. The coordinate exposes NAVI's judgment without accepting Hall,
advancing on Hall alone, or altering localization after reversal; Decision 0116
is preserved apart from Major 1's honesty gap. No legacy station machinery is
restored.

## Build evidence

Not verified: no artifacts and no build. 1,019,019 bytes flash, 58,188 bytes
static RAM and 269,492 bytes "remaining" RAM are CODEX's figures; the last is
not a runtime free-heap measurement.

## Known blockers outside this pass (still present)

1. 95% acceleration handoff. 2. Homeostasis corrections. 3. STOP/GO
execution-state reset. 4. Packet-versus-spoke accounting. 5. Station
dwell/departure — **made more likely to bite by Correction C**: stops now land
precisely at +1.5, inside `FinalStop`, where dwell is unreachable. From
standstill in a glide region the plateau holds PWM 0, so that blocker is
neither caused nor fixed here.

## Minimum corrections for this pass

1. Major 1 (one line).
2. Major 2, or David's explicit ruling.
3. Tests for `geographicPosition()`, CCW `evaluate()` endpoints and IR loss
   during a plateau.
