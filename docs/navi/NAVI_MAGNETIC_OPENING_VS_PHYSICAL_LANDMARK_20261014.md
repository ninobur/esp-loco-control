# NAVI — Magnetic-Field Opening Coordinates Versus Physical Magnet Landmarks

**Date:** 2026-10-14  
**Status:** Operator-observed coordinate convention; explanatory hypothesis for historical stopping discrepancies.  
**Scope:** Documentation and Codex review only. No firmware change, calibration adjustment, or stop-offset change authorized.

## Core distinction

NAVI's navigation events refer to **Hall detection openings**: the point in a passage where the sensor first qualifies the magnetic-field excursion. This is not necessarily the physical center of the round disc magnet embedded along the track.

The operator's visible observations refer to **physical disc magnets** and the position of the **Hall sensor mount** (or other locomotive reference point) relative to those discs. The reference point on the locomotive must be stated explicitly.

Thus:
- **Magnetic-coordinate interval:** distance between qualifying Hall openings of successive magnetic fields.
- **Physical-coordinate interval:** distance between visible disc magnets (e.g., their centers).
- **Stop reference:** the physical point on the locomotive being observed when it comes to rest.

These are distinct definitions; an apparent disagreement is not automatically an error in NAVI's marker identity or STOP OVERLAY indexing.

## Operator observations motivating the clarification

1. **October 9 EWO_0_1 MM045 STOP runs:** braking/stopping behavior looked satisfactory, but the apparent stop was approximately one MM short when judged against the visible magnet reference.
2. **October 13–14 position observation:** the Hall sensor mount was visibly before the physical disc magnet, while NAVI reported a position between magnetic markers. The parties initially investigated this as a possible discrepancy.
3. **Subsequent interpretation:** the software's between-opening position and the operator's disc-relative observation can both be internally consistent. The operator reports that the locomotive actually stopped close to or on the visible magnet. This is an observation, not an instruction to move the stopping point.

The existing EWO_0_1 stop calculation has been described as targeting a point between two magnet events, including an MM045 +150 mm fine-position offset in prior source review. If the events are Hall field openings, their midpoint need not be the physical midpoint between the disc magnets. The exact relationship depends on opening offsets, sensor placement, direction and the selected physical reference point. **Do not assume a universal offset or assert that this explanation fully proves the October 9 cause.**

## Governing decision

**Leave the EWO_0_1 stopping geometry, PWM ramps, and TILE/STOP OVERLAY pattern unchanged.** Record and consistently apply the distinction between magnetic-field-opening coordinates and visible disc-magnet landmarks when interpreting navigation and stop observations.

The operator's governing TILE/STOP OVERLAY concept remains the twelve-tile rainbow carpet anchored at the designated stopping tile; see `docs/navi/NAVI_U10_TILE_STOP_OVERLAY_RAINBOW_CARPET_20261014.md`.

## Explicit Codex handoff

Codex: incorporate this distinction in the U10 source reconciliation, field-analysis interpretations, and future measurement descriptions. Before describing a stop as one marker early/late or proposing an offset correction, identify:

1. The NAVI Hall-opening event used as the geographic reference.
2. The corresponding visible disc magnet and the physical point on the locomotive being observed.
3. The actual fine-position origin and target used by the STOP OVERLAY implementation.
4. Whether the claimed discrepancy persists after comparing equivalent reference points.

Treat the operator's reported physical stopping position as observed evidence; treat the proposed explanation of earlier discrepancies as a hypothesis to verify. Do not silently equate Hall opening with magnet center. Do not alter firmware or create an offset correction based on this note. Retain the separate source-audit findings about evaluated-versus-saved IR anchor mismatch and loss of overlay geographic input without fine IR; this coordinate convention does **not** erase those confirmed software discrepancies.

**Desired result:** a clear, consistent translation between NAVI's magnetic navigation coordinates and visible track landmarks, without introducing an unnecessary stopping-code change.
