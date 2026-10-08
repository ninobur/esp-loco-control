# NGR Architectural Principle: Geographic Consist and Movement Awareness

**Date:** 2026-10-08  
**Status:** Architectural design decision recorded from discussion; not implemented or field validated.  
**Scope:** NAVI geography, station overlays, stopping maneuvers, and two-train following.  
**Relationship:** Interpret alongside Document C (`NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md`), `NAVI_DECISION_MODEL.md`, and applicable governing decisions. This document does not silently supersede them. Any conflict requires reconciliation before implementation.

## Principle

**NAVI must know the geographic extent of its entire consist, not merely its Hall sensor's position. One geographic stopping model serves station stops, following stops, and other designated stopping points. Operating overlays define service intent, not navigation authority.**

## 1. Consist as a geographic envelope

The locomotive's Hall sensor is its positional anchor. Given its map position, orientation/direction and configured physical dimensions, NAVI derives both ends of the consist. Reversing changes the leading and trailing ends, not the geographic identity of the consist. Physical geometry must be configured per consist and verified.

The previously discussed nominal geometry is **450 mm ahead of Hall and 1,200 mm behind Hall** (1,650 mm total). These are reference dimensions to verify for each actual consist, not universal constants.

Two-train separation is measured as **clear track between the following consist's leading boundary and the leading consist's trailing boundary**, not Hall-to-Hall or block-to-block. Consist geometry is distinct from the required following buffer.

## 2. One stopping maneuver, defined backward from its destination

Designating a geographic stopping point defines the approaching tiles backward toward NAVI, in its direction of travel. NAVI continuously determines the applicable maneuver from current geography and motion; no approach-entry event, station arming latch, or Station-0 Hall prerequisite is necessary to recognize the requirement.

The agreed full-cruise planned sequence is:
- **5 MM:** ramp from 45 pKPH cruise toward 20 pKPH;
- **5 MM:** reduced-speed station-zone travel at 20 pKPH;
- **1.5 MM:** final controlled deceleration to zero.

Total **11.5 MM**, rounded upward to **12 MM**, approximately **3,600 mm** at the stated nominal 300 mm per MM. This is a planned stopping envelope, not a measured emergency braking guarantee. Verify physical braking performance and position freshness before using it for two-train operation.

The same maneuver can be projected backward from a station stopping point or a following stop point anywhere on the railroad. A changing or withdrawn stopping point causes geographic reevaluation, not a procedural reset. Speed transitions remain ramped.

## 3. Station overlays and tile requirements

The base service is **FOUR_STATION_LOCAL**: STOP at Patio, Bamboo, Arches, and Grillers. Individual station PASS instructions and **CIRCUIT_EXPRESS** (PASS at all four) are other overlay patterns. The effective STOP/PASS profile is replaced atomically; overlay changes apply immediately, and NAVI evaluates the current geographic tile.

A PASS station retains the five 20-pKPH station-zone tiles. The two tiles otherwise used for final deceleration and stop instead direct a ramp back toward 45-pKPH cruise, beginning at the former deceleration tile. STOP uses the geographic final approach and station stop. No speed target is an instantaneous PWM jump.

A PASS instruction removes only the station-service stop. It cannot override an independent following-related stop or other existing authority. Dispatcher intent and NAVI movement judgment remain separate.

## 4. Two-train following within traditional blocks

Traditional blocks were exclusive because position within a block was not reliably known. With trustworthy, sufficiently current geographic positions and consist envelopes, two trains may occupy the same traditional block when the minimum separation is satisfied.

At full cruise speed, the **minimum** clear following separation is **12 MM / approximately 3,600 mm behind the end of the leading consist**. It is not a maximum or an instruction to close a larger gap. At lower speeds the minimum may decrease, using the *same stopping model*, rather than a separate independent braking theory. Before acceleration, sufficient separation must exist for the higher intended speed.

Following may require **speed reduction without stopping**. If a stop is required, NAVI designates a stopping point behind the leader and applies the common backward-derived stopping maneuver. No station-specific stop machinery is required. The detailed intermediate-speed separation schedule and the evidence-freshness requirements remain to be established and approved before implementation.

The earlier alternating station release rule (leader departs when follower reaches a station; follower departs when leader reaches the next) was a response to block-only knowledge. It may remain an optional service pattern but is not a fundamental geographic separation requirement.

## 5. Dwell, release, and operational variety

Station STOP completion may initiate a random dwell (the previously discussed experimental range is **0–30 seconds**), beginning only when NAVI confirms actual stopping. PASS has no station dwell. Dwell expiration does not override manual STOP, following separation, or other departure requirements.

Variable leader departure timing or release points can introduce variety. These are service/scheduling choices; they do not replace NAVI's continuous geographic separation judgment or require a new latch. Exact ranges and policies remain to be approved.

## 6. Boundaries and implementation discipline

- NAVI retains navigation and movement judgment; the dispatcher/overlay expresses service intent.
- Hall-derived geographic reference, movement evidence, and consist dimensions must have explicit provenance and freshness.
- No fabricated positions, synthetic pulses, or procedural station prerequisites.
- No new PWM governor, cap, braking limit, or other performance restriction is authorized by this document.
- Do not remove the ability to decelerate and stop **anywhere** for following operations.
- This is an architectural record, **not** approval to implement, merge, flash, or activate two-train control.
- Reconcile with Document C and governing decisions before code changes; identify unresolved geometry, intermediate-speed separation, release policy, and field-validation requirements.

**Design economy:** One consist-aware geographic model; one reusable stopping maneuver; multiple station and two-train applications.
