# Universal STOP: one target near MM045, approached in both directions

**Date:** October 9, 2026

**Status:** Test-target definition and subsequent operator decisions for the
authorized cruise-base then universal STOP coding commission. This document
does not authorize flashing or track operation.

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

### Subsequent October 9 commissioning decisions

David explicitly commissioned the code: **“Let's move forward. Please write
the code or commission CODEX for the job.”** He then retained geographic
grade compensation: **“The grade dependent cruise setting should stay. The
grade is still there.”** Ordinary cruise is PWM 90 after his final clarification below; the existing geographic
grade settings remain, as recorded in the primary guide.

For this STOP trial, David subsequently specified:

- **Repeat the complete sequence:** “If you can write the whole sequence with
  dwell and restart, make it repeatable. I will also test a start withing the
  sequence.” Entry partway through the approach is therefore an explicit
  commissioning test, not a future option.
- **Five seconds at applied PWM zero, then restart:** “Yes: five seconds, then
  ramp to 60”. **The 60 value is superseded by the subsequent correction below.**
  Five seconds and the existing acceleration ramp remain approved; release
  returns to the corrected ordinary cruise of 90. The geographic grades remain.
- **Earlier profile values, preserved as evidence:**
  `90,80,70,60,50,50,50,50,50,20,0`. The later refinement below governs where
  it changes this description.

**Latest correction:** David then stated: **“Ordinary cruise is 90PWM (the
previous value) 50 is the in station speed.”** Ordinary cruise and restart
therefore return to 90, with geographic grade compensation retained. PWM 50
is the in-station setting. The apparent conflict between cruise 60 and a
profile beginning at 90 is resolved; no new clamp or acceleration exception
is introduced.

These decisions replace the earlier unanswered choice between a one-time
enrollment trial and a repeating sequence, and settle the trial dwell and
departure destination. David subsequently refined the distances and final
reduction as recorded below; do not treat the earlier list as unchanged.

### Later October 9 profile refinement — governing change

David explicitly changed the profile:

> Each value is decreased 10 PWM steps over the MM interval. I am making a
> change here. 50 to 30 is one PWM per spoke interval, 30 to 20 (and then to
> zero) divided over half the interval.

The initial reductions are therefore spread across MM intervals:
**90 → 80 → 70 → 60 → 50**, with each ten-PWM decrease spanning an MM interval.
This replaces treating the list solely as abrupt PWM assignments at markers.
The final reduction now explicitly includes **50 → 30 → 20 → 0**; it must
not silently retain the earlier direct 50-to-20 description.

The following interpretation questions have been returned to David before
implementing the changed motor behavior:

1. Whether the repeated 50 values still represent a level PWM-50 section,
   and the length of that section in MM intervals.
2. Whether 50-to-30 means one PWM decrement per measured IR spoke interval,
   for twenty spoke intervals in total.
3. Which interval the final “half” refers to; whether 30-to-20 and 20-to-zero
   share that half or each occupy one; and zero's placement relative to the
   chosen midpoint target.

The existing time-based actuator ramp must still finish an already-commanded
stop without further IR arrivals. This refinement does not authorize invented
travel, a second navigation estimate, or replacing measured distance with time.

The code commission remains a single-locomotive generic STOP demonstration.
It does not authorize CTO/CE algorithms, deployment, or a new safety cap.

### Verified starting source

Verified live on October 9: `codex/navi-ewo-0-1` and
`codex/navi-station-removal` both point to stationless commit
`c7c21163b016e443524eb9fa483f8b706ce76e22`. Its integrated sketch no longer
contains `StationMachine`. Its existing map adapter uses the surveyed
[RouteMap](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h),
whose MM045–MM046 interval is 300 mm. Recheck the live source before coding;
the documentation branch is not the stationless firmware starting branch.

The [primary guide](NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md) governs the
cruise-only base (default PWM 90 with established geographic grade settings
retained) and universal STOP overlay. The subsequent decisions above settle
repeatability, trial dwell, and ramped departure to ordinary cruise. The
geographic profile mapping and instructions across gaps in usable IR position
still need an explicit record; no invented geographic travel is permitted.
It does not require resolving CTO/CE algorithms or the whole-service
overlay proposal before the single-stop demonstration.
