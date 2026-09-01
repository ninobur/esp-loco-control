# Templates navigation concept of operations

**Status:** Working concept for operator review. Not an implementation approval.

## Purpose

Templates keeps a locomotive correctly located by identifying genuine
mile-marker magnet responses before allowing them to change position.

## Starting knowledge

The operator provides:

- position;
- orientation; and
- direction of travel.

From that starting point, the locomotive knows which mile marker it can
physically encounter next.

## Available knowledge

The locomotive has route-wide information for every mile marker and interval,
including:

- marker order;
- magnetic polarity;
- known magnetic-flux characteristics;
- distance to the next marker;
- interval timing characteristics for locomotive, PWM, direction and
  orientation; and
- the pattern of recently confirmed markers.

This information is presently available in sketch tables. It may later also be
supplied by a database on the MQTT server, but Templates does not depend on that
promise for its initial design.

## Governing observation

Every track magnet produces a Hall-sensor response. The demonstrated software
failures are classification errors:

- a genuine response is incorrectly excluded; or
- a false or duplicate response is incorrectly included.

Sensor non-response is not an accepted design assumption without independent
evidence.

## Normal navigation

1. Select the template for the only mile marker physically possible next.
2. Compare an arriving Hall response with the available identifiers for that
   marker and interval.
3. Use all evidence that is valid under the current motion conditions. Timing
   evidence may be unavailable during acceleration or deceleration; its absence
   does not erase polarity, flux, route-order or pattern evidence.
4. Accept the response only when the evidence identifies it as the expected
   marker.
5. Advance position once.
6. Select the template for the next physically possible marker.

A response that does not identify the expected marker is noise. It does not
advance position or become a provisional mile marker.

## Sequence verification and accounting

Templates also compares the recent observed sequence with the known sequence of
markers and intervals ahead of the locomotive.

If an extra response enters the accounting, later genuine responses expose a
one-place displacement between the observed pattern, the route pattern and dead
reckoning. Templates identifies and discards the extra response, corrects the
accounting, and continues from the verified marker.

The route-wide map supplies context; Templates does not discard a known position
and attempt to locate the locomotive from scratch after an isolated bad signal.

## Safety boundary

- Position changes only on an identified genuine marker response.
- Noise has no navigation or motor authority.
- Uncertainty is reported honestly and must not be converted into a confident
  position by an unsupported assumption.
- Templates navigation does not redefine manual or automatic motor authority.

## Design boundary

Templates is not QUORUM under a new name. No QUORUM mechanism or approval carries
forward automatically. Each significant Templates behavior requires its own
briefing and explicit operator approval under
`TEMPLATES_BRIEFING_AND_APPROVAL_PROPOSAL.md`.

Implementation details, numerical acceptance limits and unresolved failure
behavior remain to be briefed and approved individually.
