# 0111 - IR is a normal navigation sensor; NAVI decides applicability; degraded operation stays inside NAVI

Status: Partially superseded by 0116 (2026-09-30): EWO degraded Hall-only
navigation and the health veto are withdrawn. Other principles remain.
Originally accepted 2026-09-29 by David with Sam.
Documentation only. No implementation, flashing or deployment is authorized by
this record.
Decided by: David, after review with Sam.

## Decision

**IR is a normal navigation sensor.** NAVI_EWO is a Hall + IR navigation system.
IR is no longer treated as an experimental supplement. NAVI receives the factual
IR measurement together with its health, recency and provenance, and NAVI
determines applicability.

**A measurement of no movement is a measurement result.** `INADEQUATE_CONTRAST`
while stationary is expected detector behavior and must not, by itself:

- constitute an IR health failure;
- end measurement continuity;
- trigger degraded navigation;
- invalidate a valid no-change result.

The raw diagnostic is preserved for telemetry and analysis. While movement is
occurring, `INADEQUATE_CONTRAST` may be relevant applicability evidence for NAVI.

**Temporary Hall/IR association.** For the current two-ESP architecture: when Hall
evidence supports the target, NAVI uses the **most recently received factual IR
observation.** No elaborate cross-device clock synchronization, interpolation, or
waiting for a future IR packet is required. This is deliberately simple and
**temporary**, because Hall and IR are expected to move onto the same locomotive
ESP32. The association remains NAVI-owned.

**Degraded IR operation.** When NAVI determines from IR health and recency that
usable IR measurement is unavailable, the established **650-ms degraded
target-confirmation mechanism** remains authorized. It:

- belongs inside NAVI;
- is explicitly degraded evidence;
- does not become a physical odometer;
- does not authorize no-IR Missed Magnet progression.

There is **no no-IR, distance-based Missed Magnet mechanism.** AUTO may continue
in degraded Hall navigation while expected targets continue to be coherently
confirmed. A non-confirming Hall phenomenon alone receives the NAVI shrug (0109).
**No automatic discrepancy-stop threshold is created in the current
architecture.**

## Context

0108 established that absence of measured change is not sensor failure, using the
2026-09-27 capture in which 699 movement-window snapshots labelled
`INADEQUATE_CONTRAST` showed no displacement. The 2026-09-27 spatial-reference
capture repeats it: long stationary `INADEQUATE_CONTRAST` runs with an unchanged
pulse count, and only one boundary snapshot carrying the label while the count
advanced. No sustained movement-period IR distance gap was found. 0103 established
measured distance before proxy, and retained the 650 ms mechanism (0093) only as
degraded-mode evidence.

## Alternatives considered

- **Cross-device clock synchronization with interpolation between IR packets.**
  Not chosen: complexity for an association that disappears when Hall and IR share
  one ESP32.
- **Treat stationary `INADEQUATE_CONTRAST` as degradation.** Not chosen (0108, and
  the evidence above).
- **A distance-based or time-based Missed Magnet when IR is unavailable.** Not
  chosen: the 650 ms mechanism is confirmation evidence, not an odometer, so it
  cannot certify that a target interval has been passed.
- **An automatic stop when non-confirming Hall accumulates in degraded mode.**
  Not chosen: no threshold has evidence behind it.

## Consequences

- Health, recency and applicability are NAVI judgments, made from facts delivered
  unfiltered. Stationary no-change keeps measurement continuity.
- Missed Magnet requires applicable IR. Without it a target simply stays open and
  NAVI keeps seeking.
- **Risk: "most recent" IR can be stale** by the packet latency between the two
  ESPs. Applicability by recency is NAVI's protection; the simplification is
  accepted as temporary.
- **Risk: a real IR failure while stationary is not distinguishable from a
  no-change result** until movement begins. Per 0108, failure is established by
  independent evidence of movement the instrument did not measure.
- **Risk: degraded confirmation is weak evidence** (0093's own provisional
  status). Continued coherent confirmation is what permits AUTO to proceed.

## Relationship to earlier decisions

- **0093** (650 ms Hall-only guard, scope NAVI_COHERENCE only): *scope extended.*
  The mechanism is unchanged and remains provisional. This record authorizes it
  inside NAVI_EWO for degraded IR operation only.
- **0103:** reaffirmed. 650 ms remains degraded-mode proxy evidence.
- **0108:** refined and extended, not overridden: adds the stationary and moving
  cases for `INADEQUATE_CONTRAST`.
- **0094** (IR validity as continuous health): consistent. Health is a
  NAVI-evaluated fact stream.

## References

- `0093`, `0103`, `0108`, `0109`, `0110`, `0112`
- `../NAVI_EWO_GOVERNING_DOCUMENTS.md`
- `../../field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference_report.md`
- `../../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md`
