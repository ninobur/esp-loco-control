# NGR IR–NAVI Communication: Revised Engineering Goal
Date: 2026-10-07

## Governing principle

**Consistency is more important than granularity.**

Our objective is to deliver a continuous, accurate, and timely account of physical movement to NAVI—not to guarantee delivery of every individual IR pulse packet.

## 1. The IR instrument measures; NAVI interprets

The IR instrument has demonstrated excellent physical measurement consistency.

It should maintain:
- A cumulative count of completed physical pulses.
- The corresponding cumulative distance.
- Accurate physical timestamps.
- A measurement boot identifier.

These constitute the instrument's physical evidence.

## 2. Transmit current knowledge, not obsolete history

The IR car should transmit its **latest cumulative movement state**.

If NAVI receives observations 100, 101, and 105, it knows that four additional wheel increments occurred between 101 and 105.

It can calculate distance and average speed over that interval without receiving observations 102–104.

**Missing packets need not mean missing movement information.**

## 3. Improve transmission rather than compensate for losses

Our previous design accumulated complexity through acknowledgments, retransmissions, buffering, session management, and historical recovery.

We now favor:
- Simple ESP-NOW unicast.
- No application-level acknowledgments.
- No retransmission of obsolete observations.
- No 256-event receiver ledger.
- No additional ACK/retry task.
- Minimal receiver state and telemetry.

The priority is improving **first-attempt delivery consistency and timeliness**.

## 4. Preserve operational independence

IR communications must never interfere with:
- MQTT connectivity.
- Manual throttle, direction, or STOP.
- NAVI execution.
- Hall navigation and station operations.
- The IR detector's 1-kHz sampling.

The successful rollback demonstrated that preserving Otto's existing communications is essential.

## 5. One instrument, two transport options

**WIRELESS:** ESP-NOW delivery of cumulative observations for locomotives requiring a separate IR car.

**DIRECT:** Hardwired IR input to the locomotive ESP32, eliminating wireless pulse transport.

Both should present the same physical movement evidence to NAVI.

## 6. Measure success at NAVI

NAVI should report:
- Latest accepted cumulative pulse count.
- Distance advanced.
- Time between physical observations.
- Age of the most recent evidence.
- Reception gaps and discontinuities.
- Movement-information continuity.

We should measure how consistently NAVI receives **useful current information**, rather than counting every missed packet as a failure.

## 7. Validate before implementation

Analyze the October 6 logs to establish actual reception-gap distributions and determine whether the existing cumulative measurements support this simpler architecture.

Do not redesign the detector or introduce new navigation authority during this evaluation.

## Engineering objective

> **Deliver the freshest trustworthy cumulative movement measurement to NAVI, consistently and promptly, with minimal communications overhead and no interference with locomotive operation.**

The value of an old individual observation diminishes with every new pulse. A newer cumulative measurement supersedes it.

**Preserve physical knowledge, not packet history.**

_Status: revised engineering direction for evaluation; not authorization to implement or promote IR to navigation authority._
