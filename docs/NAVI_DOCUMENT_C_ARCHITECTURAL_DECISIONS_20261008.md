# NAVI Document C — Architectural Decisions: Geographic Tiles, Overlays, Consists and Following

**Date:** 2026-10-08  
**Status:** Decisions agreed in architectural discussion; implementation not authorized.  
**Related:** `NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md` (revised candidate commit `3761b2f`), `NGR_GEOGRAPHIC_CONSIST_AND_MOVEMENT_ARCHITECTURE_20261008.md`, Decision 0121 and Decision 0120.  
**Purpose:** Record answers to the six questions raised by CODEX, with the user's corrections controlling where earlier proposals differed. This record is intended for reconciliation into governing Document C and the applicable decision ledger; it does not silently supersede them.

## Governing model

The **base geographic tiles remain in place**. Limited overlays change instructions only within their own geographic footprints. NAVI follows the currently effective instruction on the tile at its present position, subject to any applicable temporary restriction. A temporary restriction does not erase the base instruction. Changes in speed are executed through normal ramps and the existing geographic stopping model, not instantaneous PWM jumps.

The **base program** is Four-Station Local: Patio, Bamboo, Arches and Grillers STOP, each with a **five-second dwell**. Cruise target is 45 pKPH and station-speed target is 20 pKPH. Dwell variations and Circuit Express are overlays, not changes to the base program.

## Decision 1 — Station STOP tile contains stop, dwell and departure

The designated base-layer station stopping tile carries the full instruction: physically stop at the designated stopping point, dwell five seconds, then **change that same tile's active instruction to ramp toward cruise speed**. Departure is from that stopping tile, not a different tile or a separate station procedure.

The tile's execution progress is local to carrying out this physical instruction. It must not become an ARMED prerequisite, remembered approach history, Hall-entry permission or an independent authority to ignore current geography. The apparent tension with Decision 0121's rejection of completed-visit suppression must be reconciled explicitly: **the stop/dwell instruction itself advances to its departure instruction**.

## Decision 2 — A temporary stop on the station tile does not complete station service

A geographic or other higher-priority mandate can stop NAVI on the same tile as the base station stopping tile. While that mandate applies, it takes precedence.

When it is lifted, **the base-layer instruction resumes from the locomotive's actual stopped position**. If NAVI is physically stopped on the designated station stopping tile, the base instruction then begins its **five-second dwell from the moment control returns to the base layer**, followed by a ramp toward cruise speed. No second stop is required.

If the temporary stop was on an earlier tile, NAVI resumes that earlier tile's instruction. Temporary traffic stops have **no independent station dwell**. This is geographic instruction execution, not a procedural station-arrival latch.

## Decision 3 — Limited overlay footprint; station PASS modifies exactly two tiles

An overlay **paints a limited geographic section**, not an entire new railroad program. It specifies which tiles and which instructions change; all other base tiles remain untouched.

A station PASS overlay modifies **exactly two tiles**: the tiles otherwise assigned to final deceleration and stopping. They instead instruct NAVI to **ramp toward cruise speed**. The preceding **five station-speed tiles at 20 pKPH remain unchanged**.

An overlay's updated footprint **replaces its previous footprint completely**. When withdrawn, its modified tiles revert immediately to the underlying base instructions. No accumulated obsolete tile changes or full four-station STOP/PASS profile is required merely to bypass one station.

The same limited-painting principle applies to temporary traffic restrictions. Exact footprint is determined by the geographic requirement; approximately 12 tiles ahead was an example, not a universal overlay size.

## Decision 4 — Initial fixed consist geometry

For the initial design, use the **fixed offsets previously chosen**:
- **450 mm ahead of the Hall sensor**;
- **1,200 mm behind the Hall sensor**;
- **1,650 mm total physical consist length**.

NAVI derives the leading and trailing physical boundaries from Hall-anchored position and direction/orientation. These initial dimensions are **set values**, not per-locomotive configurable values in the first implementation. Per-consist configuration may be introduced later.

Two-train clear separation is measured between the **front boundary of the following consist** and the **rear boundary of the leading consist**, not Hall-to-Hall.

## Decision 5 — Retain last known leader position during communication loss

The leading locomotive broadcasts position and speed. If those reports cease, the following NAVI **retains the last known geographic position** and **treats the leader as stopped at that position**. NAVI does not project unobserved forward movement.

This naturally tolerates transient losses: a distant leader (for example 20 MM ahead) need not provoke an immediate intervention, while a nearby leader requires timely reduction of speed or a stop based on available distance.

When valid transmissions resume, NAVI **updates the retained leader position** and reevaluates its temporary restriction. The retained position must remain distinguishable from fresh observations; no fabricated movement or invented new location is permitted.

## Decision 6 — One stopping model and a fixed 600-mm clearance

**The user's final governing formulation is:**

> **Maintain 600 mm plus the distance required for a standard stop at the following locomotive's current speed.**

Formally, for clear distance between consist boundaries:

`minimum clear distance = 600 mm + standard stopping distance at follower's current speed`.

The **600 mm** is the intended clear separation **after** the follower has stopped, not a Hall-to-Hall offset. NAVI places a **temporary geographic stop tile 600 mm behind the rear boundary of the leading consist** and projects the established standard stopping maneuver backward toward the follower. This applies when the leader is observed stopped **and** when leader transmissions disappear and its last position is retained.

When the leader moves and valid new reports arrive, NAVI updates the dynamic overlay. The restriction can relax as geography permits; there is **no separate leader-cruise-speed release threshold, release message, or procedural traffic latch**. A moving follower may be regulated to a lower speed without necessarily stopping.

The standard full-cruise stopping sequence was previously described as **5 MM cruise-to-20 deceleration + 5 MM at 20 + 1.5 MM final stop = 11.5 MM, rounded to 12 MM (~3,600 mm)**. Under the now-explicit additional 600-mm clearance, the **nominal full-cruise minimum is approximately 4,200 mm between consist boundaries**, subject to the actual mapped distances and verified stopping performance. Earlier language describing 3,600 mm as the complete minimum separation must be reconciled; it is the planned stopping component, **not** the stopping component plus the 600-mm clearance.

No new independent braking formula or second following controller is authorized. The intermediate-speed stopping-distance mapping must use NAVI's **existing standard stop** and be verified before activating two-train operation. The user's answer did not specify an exact interpolation rule; do not invent one.

## Practical acceptance case — Occupied platform

1. Leader is stopped at the platform.
2. Follower follows its base station approach tiles until its dynamic traffic overlay becomes more restrictive.
3. NAVI derives a temporary stop 600 mm behind the leading consist's rear boundary and executes the standard stopping maneuver.
4. This temporary stop does not count as station dwell unless it occurs on the base station stopping tile; if it does, the five-second base dwell begins **when the higher-priority mandate is lifted**.
5. As valid leader movement and adequate separation permit, the dynamic restriction relaxes. NAVI follows the base instruction on its **current** tile.
6. The follower reaches the platform stopping tile, completes its five-second dwell, and ramps toward cruise.

A dispatcher PAUSE is analogous to the temporary traffic stop, but is lifted by dispatcher authority; the traffic restriction is relaxed by geographic separation.

## Existing governing constraints and unresolved implementation evidence

- **Decision 0120 remains binding:** movement while motive PWM is zero that invalidates localization requires operator verification/redeclaration. A temporary traffic-stop release cannot bypass this.
- A fixed initial consist geometry does not establish that every actual train physically matches it; field verification remains necessary.
- The leading train's report provenance, freshness, geometry and direction must be represented honestly. Last-known-position-as-stopped is the chosen operating assumption during loss, not a claim that the leader physically stopped.
- Actual braking/glide-path performance and the intermediate-speed distance mapping must be measured or validated. The nominal 12-MM maneuver is not itself proof of stopping performance.
- The station tile's minimal execution state and reset conditions must be reconciled with Decision 0121 without restoring historical station-procedure authority.
- No new governor, cap, brake, PWM restriction, communications protocol or operational authority is authorized by this document.
- The known-good Otto firmware is not changed; two-train activation and flashing require separate explicit approval.

**Architectural economy:** A geographic tile supplies the normal instruction. An overlay changes only the tiles necessary. A consist envelope supplies physical boundaries. NAVI applies **one** standard stopping model everywhere.
