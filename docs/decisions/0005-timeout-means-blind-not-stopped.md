# 0005 — An IR timeout means the sensor stopped seeing; STOPPED requires an independent witness

Status: Accepted  (2026-08-05; recorded 2026-08-06)

## Decision
Edge silence declares UNAVAILABLE, never STOPPED. The STOPPED state exists
in the machine but is reachable only through `motionWitnessSaysStopped()`,
which returns false on the survey car (no witness exists) and gains
PWM/Hall inputs on the production locomotive without redesign. Speed fields
are null — never 0.00 — whenever the estimate is untrustworthy.

## Context
On 2026-08-05 the sensor was blind for 479 s of a 30-minute run while the
tow locomotive was under power for 87–100% of four of the five gaps — and
reported quality "OK" throughout, with 24% of pulses publishing
`speed_mmps: 0.00` under power. A published 0.00 is a lie a downstream
governor cannot distinguish from a real stop.

## Alternatives considered
- Declaring STOPPED on timeout (the previous behaviour) — rejected: it
  asserts a fact about the wheel from evidence only about the sensor.
- Publishing 0.00 with a flag — rejected: the number itself is the hazard;
  null is unambiguous.

## Consequences
Standing rule: any gap in the IR pulse train is cross-checked against an
independent motion witness before being called a stop, and against the
pulse counter before being called a publishing failure. Parsers must handle
null speeds and the TIMEOUT/LATCH_TIMEOUT events.

## References
Commit `58f0b0f`; `IR_TEST_STATE_AND_REACQUISITION.md`; IR_SENSOR_NOTES
2026-08-05 amendments.
