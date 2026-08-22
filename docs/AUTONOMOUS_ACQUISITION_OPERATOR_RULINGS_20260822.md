# Operator rulings for autonomous acquisition

Date: 2026-08-22

Status: Operator direction for the next specification correction. This note
does not authorize firmware implementation, flashing, or a train run.

## Startup and restart

- The normal launch region is **MM030 through MM055 inclusive**.
- Self-acquisition at startup is optional, not mandatory.
- Exact-MM/interval declaration remains available and enters positioned
  navigation immediately.
- After a power cycle inside MM030--MM055, the operator may declare the launch
  region and orientation and allow the locomotive to acquire its exact
  position from subsequent magnet observations.
- After a power cycle outside MM030--MM055, the operator may provide an exact
  MM interval or deliberately operate manually without a declared position.
- Manual operation without position must remain possible. AUTO station
  behavior is not "held" or actively inhibited in that condition; it is
  simply unavailable because the positional input required to calculate a
  station approach does not exist.
- While manually unlocated, the navigator may continue observing and report a
  self-acquired position. Acquisition does not itself start AUTO.
- Navigation loss without a power cycle retains the last trustworthy anchor,
  direction and motion evidence. Recovery begins from that bounded knowledge,
  not from route-wide ignorance.

## Two-locomotive launch

The ordinary procedure is operator-supervised and sequential:

1. Assemble both consists within MM030--MM055.
2. Declare their orientation and launch-region startup.
3. Manually start the leading locomotive.
4. Wait until it clears Grillers when CW, or Patio when CCW.
5. Manually start the trailing locomotive.

Do **not** add an automated `LAUNCH_HOLD` state or command. The trailing
locomotive remains stationary because the operator has not commanded it to
move. The operator is expected to be present and to ensure the first
locomotive cannot strike the second before acquisition and initial separation
are established.

A locomotive with no position cannot independently create a protected region.
A region must come from operator-known placement, configured physical limits,
or independent infrastructure. A peer's stopped/moving status is not itself a
position bound. Safety is evaluated from the pair of conservative occupancies.

## Operator information and authority

The operator can meaningfully provide:

- launch region;
- orientation;
- relative launch order;
- exact MM/interval after deliberate stationary identification;
- initial movement, manual throttle, STOP/E-STOP, and restart commands.

The operator cannot meaningfully confirm a moving locomotive's exact MM from
the MQTT console because observation and reporting are delayed. Routine
operator position confirmation and routine post-recovery GO are not part of
the design.

## STOP and HOLD design posture

Suggest or introduce STOP and HOLD orders reluctantly. They are final safety
responses to a concrete hazard that cannot be managed by bounded authority,
pending evidence, conservative speed, or operator-supervised manual movement.
They are not the default response to uncertainty and must not recreate CTO2's
frequent crawl/hold behavior.

An unscheduled navigation stop must be safe and rule-compliant, but it counts
as an operational failure requiring diagnosis. The design goal is that such
stops are rare.

## First-station rule

The operator will normally place the locomotive at least **12 MM markers**
before its intended first station stop. Once position is acquired, if the
intended stop is fewer than 12 markers ahead, the locomotive must not attempt
that stop; the next permitted station is used instead.
