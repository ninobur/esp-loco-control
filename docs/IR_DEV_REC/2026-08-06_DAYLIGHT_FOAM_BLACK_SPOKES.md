# IR wheel-sensor daylight field record — foam shield and black spokes

**Date:** August 6, 2026  
**Wheel:** finescale, 7 spokes, 27.8 mm diameter  
**Geometry:** approximately 87.34 mm circumference and 12.48 mm of travel per spoke pulse

## Purpose

Evaluate the IR wheel sensor outdoors in bright sun and shade, first with a foam light shield behind the wheel and then with the inward-facing spoke surfaces painted black. The production sensor will connect directly to the locomotive ESP; per-pulse MQTT streaming is diagnostic scaffolding, not part of the production data path.

## Source logs

- Baseline without foam: `ir_daylight_20260806_124221.log`
- Foam, spokes not yet painted: `ir_daylight_foam_20260806_134824.log`
- Foam plus black-painted spokes, CCW: `ir_daylight_foam_20260806_151226.log`
- Locomotive correlation: `otto_daylight_foam_20260806_134824.log` and `otto_daylight_foam_20260806_151226.log`

The logs were captured from MQTT and therefore contain gaps when the diagnostic ESP's MQTT connection was unstable. Counts below describe received records, not necessarily every pulse generated locally.

## Findings

### 1. The foam shield materially improved the lower-tail daylight margins

The no-foam manual-loop sample and the first foam sample were not speed-matched, so this is directional rather than a controlled A/B result. Even with that limitation, the lower-tail improvement is large.

| Received-pulse measurement | No foam | Foam |
|---|---:|---:|
| Pulses analyzed | 1,121 | 751 |
| Marginal pulses | 183 (16.3%) | 0 |
| Rise headroom (`rh`) p10 | +63 | +137 |
| Fall headroom (`fh`) p10 | +53 | +247 |
| Pulses with `fh <= 50` | 107 | 0 |

Keep the foam shield in the mechanical design.

### 2. Painting the inside spoke surfaces black corrected the repeatable weak-spoke pattern

With foam but before painting, phase 6 was consistently much weaker than the other six phases:

- `rh` approximately +137, +124, and finally +58 in successive windows.
- Other phases in those windows were generally several hundred counts stronger.
- A later doubled interval was consistent with the weak spoke being missed.

With foam and black-painted spokes, the first two complete phase windows were well balanced:

| Window | `rh` range across phases 0–6 | `fh` range across phases 0–6 |
|---|---:|---:|
| 15:13 | +392 to +461 | +358 to +599 |
| 15:14 | +347 to +405 | +535 to +746 |

All seven phases had nearly equal sample counts. Phase 6 was no longer an outlier. This is the strongest evidence so far that the black spoke treatment fixed the wheel-specific optical weakness.

### 3. Bright sunlight can still reduce margin, but the painted-spoke sample remained detectable

During a later bright-sun or sun/shade transition, the 15:15 summary reported `rh` p10 of only +23 while `fh` p10 remained +621. Near the last received pulse batch, the adaptive baseline was about 3,250 with a span near 370 counts. Individual pulses still reported `OK`, with representative rise headrooms around +96 to +151 and fall headrooms around +53 to +349.

Across the received moving portion of the painted-spoke run, 802 parsed pulse records were `OK` and none were marked `MARGINAL`. Their overall p10 headrooms were approximately `rh=+96` and `fh=+184`. The low rise-headroom tail means a complete production-path daylight loop is still required before final acceptance.

### 4. Seven-spoke speed calibration agreed with the locomotive estimate

During the first foam run, IR speed and Otto's Hall-derived estimate tracked closely after acceleration:

| Time | IR speed | Otto estimate |
|---|---:|---:|
| 14:41:19 | 271 mm/s | 280 mm/s |
| 14:41:30 | 250 mm/s | 250 mm/s |
| 14:41:40 | 265 mm/s | 262 mm/s |
| 14:42:32 | 320 mm/s | 341 mm/s |

This supports the 27.8 mm diameter and 7-spoke calibration. The required hand check remains: one complete wheel revolution must produce exactly seven pulses before any speed result is trusted.

### 5. The IR diagnostic MQTT stream failed; this is not the production architecture

In the painted-spoke CCW run:

- The final diagnostic pulse batch arrived at 15:16:02, near Otto's MM73–74.
- The last pulses were valid `OK` measurements; there was no optical latch or contrast-loss event at the cutoff.
- The availability topic then cycled repeatedly between offline and online, while no further `PULSE`, `IDLE`, or `STATS` lines arrived and Otto continued moving.

This indicates loss of the diagnostic publishing path, not evidence that the optical sensor stopped detecting. In production, the IR sensor will be wired directly to the locomotive ESP and wheel pulses will be processed locally. MQTT should carry only optional summaries/status and must not be in the speed-measurement path.

USB serial and MQTT carry the same IR_DIAG text. A future diagnostic investigation can distinguish the failure cleanly: continuing USB output with absent MQTT proves a network/publisher problem; loss of both points to an ESP/main-loop problem. Resolving diagnostic MQTT instability is useful for test completeness but is not a production sensor requirement.

## Otto Hall context

One earlier Otto run contained a separate 24.46-second continuously active Hall event while Otto was moving. At the prior cadence this could hide roughly 20–22 track markers, after which bounded quorum corrections could not recover the full position error. A subsequent clean retest produced 341 consecutive Hall agreements, zero disagreements, and the exact expected final marker after almost two loops. The Hall anomaly did not reproduce in that retest and is independent of the IR foam/paint result.

## Current disposition

- Retain the foam light shield.
- Retain the black-painted inward-facing spoke surfaces.
- Treat the seven-spoke optical balance as substantially improved.
- Do not treat diagnostic MQTT cycling as a production blocker.
- Before production acceptance, perform the seven-pulses-per-revolution hand check and a full bright-sun/shade loop using local pulse counting on the locomotive ESP.
- During that loop, retain per-phase `rh`/`fh` summaries long enough to confirm that no phase again becomes a repeatable weak outlier.

