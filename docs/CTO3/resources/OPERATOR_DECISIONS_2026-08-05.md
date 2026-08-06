# CTO3 operator decisions — 2026-08-05

Provenance: decisions stated directly by David Brown during the CTO historical
and design-record review. This note records the source decisions that supersede
conflicting historical material.

## Present scope

- CTO and CTO3 apply to the Lowline only. Highline sketches and trackside ESPs
  are not germane.
- The Lowline presently has four identified station stops. The block concept is
  gone and is unnecessary; station stops are not called QUORUM positions.
- The physical consist extends 18 inches in front of the Hall sensor and 48
  inches behind it. This is the guiding collision-avoidance constraint.

## Control and coordination

- Control resides aboard the locomotives, with locomotive-to-locomotive
  communication over ESP-NOW.
- The dispatcher is initially limited to Start, Stop, eStop, and Release to
  Manual.
- The locomotives need to establish their front/back relationship and still
  need to "pair up," although the need for a historical-style peer table is not
  settled.
- IR sensor development is approaching integration as a second source of
  navigational truth.

## Architectural intent

- The expanding-and-contracting bubble is one activity CTO3 can perform, like
  one LP played on a turntable. CTO3 is the turntable and must support other
  records without being dismantled.
- Future capabilities include voice control, stops between current stations,
  routines such as Circuit Express, and structured dispatcher missions with
  destinations, platform/HOLD positions, conditional waits, and skipped stops.
- Example future mission: proceed to Bamboo platform; wait for Toby to reach
  Bamboo HOLD; then proceed, skip Arches, and stop at Grillers.
- The architecture must also support Lowline expansion with turnouts, passing
  sidings, switching sidings, and the route/topology concepts those require.

## CTO2 failure interpretation

- Debugging suggestions in `CTO2_FAILURE_ANALYSIS.txt` are fixes for a
  superseded paradigm. Preserve them as history; do not adopt them as CTO3
  requirements.
