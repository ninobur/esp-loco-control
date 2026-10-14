# NAVI U10 — TILE and STOP OVERLAY: Governing Concept and Reconciliation

**Recorded:** 2026-10-14  
**Status:** Governing operator architecture; implementation reconciliation required.  
**Source:** October 13–14 design discussion. This document reconstructs the discussion, not a verbatim historical specification.  
**Scope:** Documentation only. No firmware change or authorization to modify operational behavior.

## The rainbow-carpet model

The railroad is divided into geographic **tiles**, each representing an MM-to-MM interval. The basic TILE layer assigns **PWM 90** (green) to every tile in the current EWO_0_1 operating concept.

The **STOP OVERLAY** is a movable geographic throttle program. Its anchor is the designated **stopping tile**, represented as **red**. From that anchor, the overlay extends *backward along the approach route* across the **eleven preceding tiles**, creating a twelve-tile approach pattern. The PWM assignments or ramps progressively increase backward from red through orange and yellow to deeper green. The approaching locomotive traverses the carpet **green → yellow → orange → red (STOP)**, with prescribed PWM progressively decreasing.

This is the operator's original visualization of the TILE/OVERLAY architecture, not an analogy invented after implementation. The same pattern should be positionable anywhere on the railroad by selecting a different stopping tile and approach direction.

## Responsibilities

- **NAVI** establishes geographic identity and identifies the active tile.
- **TILE** supplies the ordinary geographic throttle assignment.
- **STOP OVERLAY** supersedes those assignments for the twelve-tile approach anchored to the selected stopping tile.
- **Motor control** executes the selected PWM and ramp commands.

The OVERLAY is **not** a separate geographic navigator and **not** merely a routine triggered on arrival at a station magnet. Geographic identity and optional fine IR offset must be distinguishable: an accepted MM must remain available even when fine offset is unavailable, but no fine position should be invented.

## Field observation and competing explanations

During the October 9 EWO_0_1 MM045 stopping experiments, the stopping maneuver looked satisfactory, but the locomotive reportedly stopped approximately **one MM short** of its intended destination. This observation does **not** establish whether the cause was (a) an incorrect NAVI geographic identity delivered to an otherwise correct overlay or (b) an overlay tile-indexing, orientation, or anchoring error. The firmware and field trace must distinguish these possibilities before proposing corrections.

## Required source and evidence review

Inspect the operational **NAVI_EWO_0_1_MM045_STOP** program at commit `85a2cf2a16d262abdf7258c60f60508b823913ee`, including `NaviStopOverlay.h`, the TILE mapping and the NAVI-to-overlay-to-motor handoff. Verify:

1. Stopping tile is the overlay anchor.
2. The eleven preceding tiles are selected in the correct approach direction, including wraparound and reverse travel.
3. PWM values and ramps are applied to the intended tiles.
4. NAVI's selected MM maps to the intended active tile, without an off-by-one offset.
5. The overlay can receive a selected MM when fine IR position is unavailable; behavior in that condition is explicitly tested, not assumed.
6. Braking, dwell, departure, manual controls, E-stop and electrical protections remain unchanged.
7. ESP32 timing, memory, queue and MQTT overhead from any future navigation changes do not interfere with operation.

Separate **governing concept**, **actual source behavior**, and **field observations** in the reconciliation report. Do not presume that a plausible-looking stop proves correct tile placement.

## Relationship to the geographic decision model

The new NAVI candidate-comparison mechanism chooses one qualifying geographic target. TILE/OVERLAY then acts on NAVI's selected tile. Better geographic identity may improve stopping accuracy, but this is a hypothesis to verify against firmware and field records, not proof that the overlay is defect-free.

**Decision:** Preserve the TILE/STOP OVERLAY twelve-tile architecture and preset-PWM operation. Do not redesign speed control or add a separate station controller. Investigate the one-marker stopping discrepancy and request authorization before any code changes.

**Governing test:** Is the rainbow carpet anchored to the correct stopping tile, laid backward in the correct direction, and delivering the intended PWM on each tile?


## October 14 addendum — Magnetic-field openings versus visible disc magnets

**Operator clarification and instruction to Codex:** The apparent one-marker-short STOP landing must not be treated as an established firmware defect. The operator observed stopping position relative to the **round, physical disc magnet**, whereas NAVI's navigation coordinate is based on the **Hall opening at the leading magnetic field**. The opening normally occurs before the Hall sensor reaches the disc itself. The locomotive also continues moving after opening during its final PWM ramp.

In the present MM045 STOP experiment, the programmed point described as between two magnets is in NAVI's **field-opening coordinate frame**: between successive detected magnetic-field openings, not necessarily halfway between the centers of two visible disc magnets. The physical stopping position was observed near or on a disc magnet. That physical landmark is useful and repeatable, provided the same translation convention is used consistently. This is an operator observation and interpretation, not a measured universal field-opening-to-disc offset.

A second observation from October 13 supports investigating this distinction: the operator saw the Hall sensor mount physically **before** the disc magnet while NAVI described its position as **between magnets**. This apparent disagreement may arise from using the disc center versus magnetic opening as the position reference, rather than an incorrect MM identity. Also distinguish the physical Hall sensor location from the locomotive's own reference point.

### Consequences for source reconciliation and field evaluation

1. **Preserve the existing STOP geometry and PWM ramps.** No offset adjustment or firmware change is authorized by this clarification.
2. For Codex's source audit, explicitly identify whether each reported coordinate is relative to a Hall opening, a physical disc center, an MM-to-MM tile boundary, or a locomotive/sensor reference point. Do not assume these are interchangeable.
3. Inspect the exact MM045 +150 mm calculation in the operational EWO_0_1 source and explain its reference frame. A numerical +150 mm from a Hall opening is not necessarily +150 mm from the visible disc magnet.
4. Reinterpret the October 9 short-landing report and October 13 observed Hall-mount discrepancy using this distinction. Report what can be established from the actual code and field observations; do **not** invent an opening-to-center offset or claim that this alone proves the stopping discrepancy resolved.
5. Treat the observed landing near the disc as a potentially useful **physical station landmark**. Future field notes should name the measured physical point (Hall sensor, locomotive nose, etc.) and the disc landmark used.
6. If source evidence reveals an actual geographic indexing or coordinate error, report it separately and seek approval before modifying firmware.

**Governing conclusion:** NAVI navigates using detected magnetic-field openings. The operator assesses stops using visible physical landmarks. The coordinate-frame distinction must be explicit before diagnosing or correcting a stopping-position discrepancy.

**Codex action requested:** Read this addendum together with the rainbow-carpet U10 model and the October 14 source reconciliation. Incorporate the coordinate-frame distinction into the architectural and field-evidence interpretation. Research/documentation only; do not change firmware, calibration, maps, or STOP geometry without separate authorization.
