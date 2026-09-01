# 0068 — Station stops return, with stopping points tunable per station and per direction

Status: Proposed  (2026-09-01)

## Decision

NAVI_ONE stops at the four platforms again. `Stations.h` holds a pure machine —
`Idle → Approach → Zone → Ramp → Dwell → Depart` — driven from position and a
clock, with one call site in the sketch.

| | |
|---|---|
| centres | Patio 15, Grillers 63, Arches 108, Bamboo 157 |
| station speed | 60, except **Grillers CCW 72** |
| stop offset | **+1** past centre, per station **and per direction** |
| approach | arms at −10, station speed by −6, held to the stop trigger |
| zero ramp | 200 ms per count |
| dwell | **30 s** |
| departure | 200 ms per count, back to the section cruise |

Ships as NAVI_ONE 0.6 — the versions between this and the last flashed build
were never on a locomotive, so they are collapsed rather than counted.

## Context

Operator, 2026-09-01: *"It is time to return the station stops. Restore previous
pattern but Please make the stopping points tunable by station and by CW vs CCW."*

NAVI_2 had one `stopOffset` per station and a blanket correction — `if CCW,
subtract one` — applied to every platform alike. That was a 2026-08-14 fix for
CCW landings sitting a marker past the platform, and it worked, but it made one
observation about the railway into a rule about all of it. Both directions are
now explicit columns.

Three changes to the restored pattern, all the operator's:

- **No second speed step.** NAVI_2 eased from zone speed to a lower `finalPwm` at
  M+1. *"Station throttle setting is 60 PWM until the last ramp."* One speed from
  the zone to the trigger.
- **Grillers is asymmetric**: 72 CCW, 60 CW. *"72 is wrong for downhill at
  Grillers."*
- **Dwell 30 s, not 15**, and deliberately so: *"I think that the longer dwell has
  a bigger risk for latch."* The dwell is now a test of finding 08 as well as a
  station stop.

## How the approach is paced

Six PWM counts a marker, one count at a time, across the five markers from −10
to −6. The pacing lengthens at each step because the locomotive is slowing and
each successive marker takes longer to cross:

| step | marker time | ms per count |
|---|---|---|
| 90 → 84 | 1,390 ms | 232 |
| 84 → 78 | 1,539 ms | 257 |
| 78 → 72 | 1,725 ms | 287 |
| 72 → 66 | 1,960 ms | 327 |
| 66 → 60 | 2,271 ms | 378 |

Two properties matter more than the numbers.

**The table is MARKER TIMES, not per-count pacing.** Pacing is
`marker time / counts shed this marker`, so the same table serves a 90 → 60
approach at six counts and the 105 → 60 Patio runs off the CCW curve at nine.

**It is per-locomotive, and lives in the locomotive's config.** A faster
locomotive crosses each marker in less time and needs its own numbers; Toby's
would pace its ramp too slowly and the throttle would still be moving when the
next marker arrived. `Stations.h` has **no default** — a build for an unmeasured
locomotive fails at the `#error` rather than quietly using Toby's timings.

**The approach ramps from what the locomotive is actually doing**, not from a
hardcoded cruise. Patio CCW arrives at 105 off the curve of decision 0067;
starting from 90 would reinstate the step that decision removed.

## Alternatives considered

- **A per-station, per-direction pacing scale.** The four approaches genuinely
  differ — measured zone index runs 0.87 (Patio CCW, climbing) to 1.13 (Arches
  CW, dropping). Rejected by the operator: *"one table. It will provide adequate
  smoothing."* That ±15% sits inside a table whose own span is 63%, and eight
  more constants would have to be kept true as the railway changes.
- **Pacing each count off the previously measured marker.** More accurate again,
  and self-calibrating for load and battery. Rejected: *"K.I.S.S."* The
  smoothness comes from one count at a time, not from the interval being right
  to the millisecond.
- **Keeping NAVI_2's blanket CCW−1.** Rejected — it is the thing this decision
  exists to replace.

## Consequences

- **The 30 s dwell will sit the locomotive still with the baseline frozen.** That
  is the latch condition of finding 08, and it is deliberate. `stopOffset +1`
  starts the zero ramp a marker past centre and the 200 ms brake carries it
  further, so it should come to rest **between** magnets rather than on one —
  which is the safer place, but it is a should, not a guarantee. **This is the
  thing to watch on the first field run.**
- A station in charge owns the throttle: the section-cruise request is suppressed
  unless the machine is Idle, so a section boundary crossed during a stop or a
  dwell cannot wind the locomotive back up.
- The machine stands down on any frame end — declaration, direction change — and
  whenever AUTO drops. A struck locomotive has `autoRunning` false, so this can
  never restore power to one that has stopped.
- New topic `state/station`. Additive; no existing topic or payload changes.
- An overshoot past +5 stands the machine down and says so rather than chasing
  the platform. A phase that cannot complete times out at 120 s; the dwell has
  its own clock and departure is deliberately exempt.

## Verification

All ten host gates pass. Compiles clean against Toby's core (esp32:esp32:esp32
3.3.11) with no warnings from NAVI_ONE sources — 968,479 bytes flash (73%),
59,204 globals (18%).

**Gate 10**, new, 51 checks, driving the machine marker by marker with a fake
clock: the approach targets 84/78/72/66/60 and their pacing 231/256/287/326/378;
station speed held to the trigger with no intermediate step; the zero ramp
starting at exactly +1; the dwell holding at 29 s and releasing after 30; the
departure at 200 ms a count; the Patio CCW case entering at 105 and shedding
nine counts a marker at 154/171/191/217/252; Grillers 72 CCW and 60 CW; the four
centres; per-direction stop offsets; overshoot stand-down; and that an unset
direction can never arm a station.

Not field-tested. Not ratified.

## References

- `firmware/test-programs/NAVI_ONE/Stations.h`
- `firmware/test-programs/NAVI_ONE/LL_LocoConfig_9950012.h` — `NAVI_APPROACH_MARKER_MS`
- `firmware/test-programs/NAVI_ONE/tests/gate_station.cpp`
- `docs/decisions/0066`, `0067` — the section cruises the approach hands over from
- `docs/NAVI_ONE_FIELD_FINDING_08_BASELINE_LATCH_SWALLOWS_MARKERS.md`
