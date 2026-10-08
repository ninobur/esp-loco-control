# IR–NAVI communication simplification reconciliation

Date: 2026-10-07
Baseline: `442a3d70599a403ba2fef168503d05f060e5e72c`

## Architectural result

This change simplifies only delivery of Type-6 physical pulse evidence.  It
does not change NAVI's navigation, speed-control, station, Hall, PWM, manual,
AUTO, dispatcher, MQTT, or Type-5 path.

The governing distinction is retained: the IR instrument measures completed
wheel pulses; the transport delivers observations; NAVI interprets its movement
history and makes every navigation/control judgment.  A missed Type-6 delivery
therefore loses temporal detail, but a later same-boot cumulative observation
still supplies the total completed-pulse distance and physical elapsed time.

This follows the decision-system principles to preserve direct observations,
avoid upstream navigation decisions, and make loss visible.  It is consistent
with decisions 0109–0122, including 0116: no Hall-only fallback, inferred
location, or new navigation authority is introduced.  Type-5 pitch/configuration
semantics remain unchanged under 0122.  The reviewed revised communication goal
at `23feba9a6cd064d506380e1a8ccfc3e8ef7a234f` specifically supports newest
cumulative evidence without application ACK/recovery.  The recorder proposal at
`ec1d62c0398af26018c8d03d3949894f5e38458c` is future-only and is not implemented.

No governing-document conflict was found: the earlier Type-6 physical-evidence
contract requires that the detector's native completion, cumulative count,
nominal distance, and original completion timestamp remain truthful.  The
transport may supersede unsent observations; it must not manufacture a missing
completion or timestamp.

## Existing NAVI history and interfaces

`NaviIntegratedCore::observeIr()` is the existing operational Type-5 ingress.
It owns the current cumulative IR snapshot, movement epoch/order checks,
IR/Hall relationship, location confirmation, PWM-zero handling, and a bounded
speed history.  `addSpeedPoint()` calculates interval-average speed from
cumulative Type-5 pulse counts and the transmitter physical `capturedUs` over
a 0.9–1.5 second window.  There is no acceleration calculation in this
firmware baseline.

Type-6 currently enters through `onIr()` and `servicePulseIngress()` into the
isolated `navi_pulse::Observation` state.  It is explicitly
`OBSERVATION_ONLY`: it never calls `NaviIntegratedCore::observeIr()`,
`noteObservationLoss()`, recorder input, station logic, or an actuator API.
That separation is retained.  Consequently Type-6 cannot silently substitute
for Type-5's existing movement-history interface or alter NAVI's judgment.

## Latest-value handoff assessment

A one-slot handoff is safe for this Type-6 observational ingress because each
packet carries a self-contained cumulative count, nominal distance, and the
physical timestamp of its latest completion.  The receiver keeps its last
accepted endpoint, detects a source/boot/order discontinuity, and derives only
the average distance/time between accepted endpoints.  It does not synthesize
the skipped pulses.

The one-slot handoff does **not** reduce NAVI's existing Type-5 history or
change its history depth.  Type-6 has no existing NAVI control/history consumer
in this baseline.  Routing it into `observeIr()` would change NAVI's reasoning
architecture and Type-5 operational behavior, so this assignment does not do
so.

## Scoped change boundary

Strictly communications changes are: replace the transmitter's historical
Type-6 FIFO with a latest-value slot, remove Type-7 transport accounting, and
make the observation-only receiver retain one newest pending Type-6 observation
while validating source, boot, integrity, ordering, cumulative distance, and
physical-time continuity.  Telemetry reports accepted endpoint distance/time
and visible discontinuities.

No proposed change alters NAVI judgment or control behavior.  The transmitter
remains broadcast in this baseline because no authoritative fixed Otto ESP-NOW
MAC is configured in the IR-car firmware; inventing one would break the known
working receiver.  The design is otherwise unicast-ready (one peer/address
selection), and a field-approved pairing/configuration value is required before
changing radio destination.
