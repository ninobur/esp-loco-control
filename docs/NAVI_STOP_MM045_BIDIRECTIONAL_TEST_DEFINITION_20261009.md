# Universal STOP: one target near MM045, approached in both directions

**Date:** October 9, 2026

**Status:** Test-target definition for the proposed cruise-base then universal
STOP coding commission. No firmware, flash, or track operation performed.

David agreed with the proposed development order, reaffirmed that STOP can be
anywhere, authorized selection of a stopping point approachable from either
direction, and then specified: **“Stop somewhere around 045.”** CODEX selected
the midpoint of MM045–MM046 within that instruction; the exact midpoint was
not a separately specified coordinate from David.

## Fixed geographic target

**Target: 150 mm from MM045 toward MM046, halfway along that interval.**

The surveyed map has a 300 mm CW span from MM045 to MM046. Use NAVI's existing
Hall-referenced locomotive coordinate when expressing this aim point and
reporting actual stop position. This does not change consist-boundary rules.

| Approach | Description of the same physical target |
|---|---|
| CW, ascending marker numbers | MM045 + 150 mm toward MM046 |
| CCW, descending marker numbers | MM046 + 150 mm toward MM045 |

The target remains fixed when direction changes. “150 mm beyond MM045 in
the direction of travel” would incorrectly select different physical points.
The two descriptions above are coordinates, **not** requirements to pass either
marker before the STOP sequence can operate.

## What the demonstration must establish

- The same portable STOP sequence can target a non-station, non-marker point.
- The approach is constructed backward from that target toward the approaching
  locomotive, using the selected direction and actual surveyed geography.
- Both directions aim at the same point. Their geographic PWM profiles may
  differ where physical evidence warrants it; identical stopping performance
  in opposite directions is not assumed.
- Entry partway through the approach uses the current geographic instruction;
  passing a designated start marker or station admission point is unnecessary.
- Record the actual stop and its error from the target separately for each
  direction. The aim point is not a completion gate: retain the guide's timed
  completion at elapsed dwell with concurrently applied PWM=0, and do not move
  again merely to correct an early or late stop.
- On authorized release, use the existing acceleration ramp and do not reissue
  the completed stop in its former footprint.

This applies the [enduring decision-system principles](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md)
by preserving measured results independently of the desired target. It adopts
no new stopping tolerance, performance claim, controller, or safety cap.

## Source and commission boundary

Verified live on October 9: `codex/navi-ewo-0-1` and
`codex/navi-station-removal` both point to stationless commit
`c7c21163b016e443524eb9fa483f8b706ce76e22`. Its integrated sketch no longer
contains `StationMachine`. Its existing map adapter uses the surveyed
[RouteMap](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h),
whose MM045–MM046 interval is 300 mm. Recheck the live source before coding;
the documentation branch is not the stationless firmware starting branch.

The [primary guide](NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md) governs the
cruise-only PWM 60 base and universal STOP overlay. This target choice does not
settle its remaining implementation questions about execution-progress lifetime,
departure handoff, or geographic instructions across gaps in usable IR position.
The initial approach distances and test dwell must be explicit in the coding
brief. It does not require resolving CTO/CE algorithms or the whole-service
overlay proposal before the single-stop demonstration.
