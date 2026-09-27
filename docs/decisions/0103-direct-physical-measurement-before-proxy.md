# 0103 - Direct physical measurement before proxy

Status: Accepted as principle (2026-09-27, David with Sam). Documentation only.
Decided by: David, after review with Sam. The quotation is from David's
codification instruction of 2026-09-27.

## Decision

"When NAVI has a direct physical measurement relevant to the question being decided,
it uses that measurement rather than a proxy for it."

| Use | Not |
|---|---|
| measured IR distance | elapsed time as a proxy for distance |
| actual Hall-field behavior | a fixed timer as a proxy for field resolution |
| measured movement, where applicable | motor command as a proxy for movement |
| mapped physical geometry | generic timing as a proxy for reachability |

Time, PWM and other indirect indicators remain useful when the corresponding physical
measurement is unavailable. They do not displace applicable direct evidence because
the proxy is easier to code.

## Context

Many inherited time and PWM rules were rational open-loop workarounds from before
physical-state feedback existed. NAVI is intended to exploit feedback and move NGR
toward closed-loop physical-state judgment and control. The historical example
(David): a following train once could not leave a station until the leader reached
the next one, because that arrival was effectively its only reliable information
about the leader. Keeping such restrictions after acquiring better information adds
telemetry without the intelligence and freedom of action NAVI exists to provide.

Evidence from 2026-09-26:
- **Time used although distance was measured.** X22R's 3000 ms cadence refused every
  baseline refresh in run 1 (4 of 4) while Epoch IR distance was judging the same
  MMs (`field-records/verdicts/20260926_toby_20q3_run1.md`).
- **A time fit standing in for distance.** The 80 ms SETTLE traces to a time fit on
  Otto for clearance of a magnet's return-flux shelf, which Otto's raw data show is a
  distance (`../NAVI_BASELINE_TIMING_20260916_C3B93D0B.md` §4; plan §4).

## Alternatives considered

- **Keep time/PWM rules everywhere as simpler and field-proven.** Rejected where
  direct measurement is applicable; retained as degraded-mode evidence where it is not.

## Consequences

- For each inherited restriction ask: what uncertainty made it necessary, and does
  NAVI now have physical data that resolves it? If yes, reason from the data; if no,
  the proxy may remain as degraded-mode evidence.
- The 650 ms Hall-only fallback (0093) stays, as degraded-mode timing used only when
  valid MM-referenced IR distance is unavailable. No additional time gates are created
  to replace mechanisms removed from the distance-based path.
- **Risk.** A direct measurement is only as good as its validity: IR is unvalidated
  in scale (`distance_bounds: UNVALIDATED`) and can be disturbed (tilt, hand movement,
  IR-car-only motion). "Applicable" must be judged by NAVI in context (0105, 0106), not
  assumed whenever a number exists.

## References

- `../NAVI_DECISION_MODEL.md` §2, §5
- `0093-provisional-navi-coherence-hall-only-guard-650-ms.md`
- `../NAVI_DECISIONAL_INERTIA_CLARIFICATION_20260923.md` ("without converting
  commanded PWM into measured distance")
