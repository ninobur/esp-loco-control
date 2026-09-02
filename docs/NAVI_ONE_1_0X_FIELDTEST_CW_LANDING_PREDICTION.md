# NAVI_ONE_1_0X_FIELDTEST — what to expect from a clockwise lap

**Date:** 2026-09-02, written while Toby was starting CW.
**Tool:** `tools/interrupted_replay/cw_landings.cpp`.
**Status:** prediction from a model. Not a field result.

## Why this was written

Gate 12 section H — the only end-to-end sweep decision 0070 has — is
**counter-clockwise only**: `declare(128, -1)`. Findings 11 and 13 were both
CCW, so that is where the work went. The clockwise stop offsets are different
numbers (`+1, −1, +1, +1` against the retuned CCW `0, −1, 0, −1`, Stations.h)
and **had never been run through the pause/resume path at all.**

## The headline

**At the measured coast, no clockwise station rests in a magnet's field.**

| station | CW stop | at rest | field | outcome |
|---|---|---|---|---|
| Patio | +1 | −106 mm | 0 | rests clear |
| Grillers | −1 | −136 mm | 0 | rests clear |
| Arches | +1 | −96 mm | 0 | rests clear |
| Bamboo | +1 | −116 mm | 0 | rests clear |

96–136 mm from the nearest marker is six or more sigma out. Nothing pauses,
nothing stitches, nothing is refused.

**Therefore: a clean CW lap does not test decision 0070.** If the telemetry
shows no `stitched` and no `paused_ms` this lap, that is the expected result and
it is *not* evidence the pause/resume design works. The machinery will simply
not have been exercised. **The finding-13 measurement lives at Arches CCW.**

## But the landing is not certain to 100 mm

Markers are ~300 mm apart and the landing is only good to roughly ±100 mm, so a
modest coast error puts the sensor in a field. Sweeping the coast 60–140 % — the
same coverage device section H uses CCW, which walks the landing 390 mm and so
visits every resting place — across all four stations:

**Of 36 CW landings: 12 produce a stitched waveform, 6 of those are refused, and
6 stop the locomotive.**

Two distinct stop modes appear, and they are different things:

### 1. Rests inside a field, stitched waveform refused

| | at rest | field | residual | |
|---|---|---|---|---|
| Patio 120 % | −11 mm | −161 | **0.1378** | refused |
| Bamboo 60 % | −5 mm | −199 | **0.1303** | refused |
| Bamboo 130 % | +26 mm | +45 | 0.2698 | refused |

Bamboo at 0.1303 is **three ten-thousandths over the 0.13 ceiling.** That is
finding 13's margin again — same knife-edge, different station, in the direction
being run today. Finding 13's risk is **not confined to Arches CCW**, and this
is a second reason not to answer it by moving the ceiling.

### 2. Stopped on the approach, before ever reaching the dwell

Patio, Grillers and Bamboo at 70 % coast all do this. The trace:

```
PAUSE @54751
STITCHED pol N peak 184 ratio 0.880 resid 0.2717 shape 1 guard 1 paused 1204ms -> WRONG_SHAPE
STITCHED_REFUSED (advance zero, stop)
```

Peak 184 against 209 for a full crossing: he crawled over a marker on the zero
ramp, progression was lost mid-arc, the measurement paused, the field then
settled and the passage **closed while paused** on requirement 8's rule — a
truncated arc, correctly refused at 0.27.

**This is not a new failure.** It is the known limit already recorded in
decision 0070 — *a magnet crossed almost entirely at a crawl is not
recognisable, stop or no stop*; gate 12 section D refuses 6 of 18 stop
geometries and 18 of 18 identical creeps with no stop in them. What is new is
that the field-test build now **stops the locomotive** where the accepted build
lost the marker quietly.

So: **a stop partway down the approach ramp, before the station, is an expected
outcome of this build.** It reads like a fault and is not one.

## What this does not prove

- Model magnets (210 counts, 15 mm sigma) on modelled track. Only the speed fit
  `3.990 × (PWM − 25.1)` and the marker spacings are measured.
- The coast sweep is a **coverage device**, not a claim that the real coast
  varies by 40 %. It says *where each resting place leads*, not *how likely each
  resting place is*.
- No grade. Grillers CW sits at the foot of the climb (`GRADE_FROM_CW` 65,
  centre 63) and departs to 110; that departure is modelled as a throttle number
  only.

## Recommendation

Fold this sweep into gate 12 as a section I, so clockwise stops are gated and
not merely predicted. Not done today: the committed test tree should keep
matching the image that was flashed.
