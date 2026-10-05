# 0121 — Current position, direction, and overlay determine NAVI operating authority; historical procedure state does not

Status: Accepted (2026-10-05). Documentation only; no implementation authorized.
Decided by: David, in the supplied Position/Overlay Operating Architecture and
the explicit documentation-reconciliation instruction of 2026-10-05.

## Decision

```text
current MM interval + direction + current operating overlay → current operating requirement
```

Historical procedure state does not grant or retain operating authority.
The dispatcher owns operating intent; NAVI owns position and execution.
Decision 0101's current-position insight remains foundational.

## Context and supersession

0101 applied current-position reasoning while retaining visit memory and
station-procedure mechanisms. Earlier 0068 and 0090 also described phase,
arming, and overshoot mechanisms. Those mechanisms must not be mistaken for
the settled operating architecture merely because they remain implemented.

This decision supersedes only their conflicting **NAVI operating-authority
claims**: 0101's completed-visit suppression and preserved station procedure;
0068's station arming, persistent phase/throttle authority, procedural
overshoot/watchdog and marker/PWM sequences as operating authority; and 0090's
station-arming workaround and phase/overshoot-based MISSED consequence.
Their reasoning, original statuses, dates, and evidence remain intact.
This does not reopen unrelated navigation decisions, physical stopping
geography, legacy protocols, or the separate adaptive-braking architecture.

## Consequences

- No station ARMED admission requirement or persistent phase authority.
- No completed-visit or one-stop-per-visit suppression.
- STOP/GO reevaluates present position, direction, and overlay.
- Overlay replacement replaces prior operating authority.
- Station-procedure MISSED and station PHASE_TIMEOUT are not governing
  operating concepts; this does not remove NAVI's distinct Missed Magnet rule.
- Hall and IR supply evidence, not operating permission. Necessary physical
  references may still be required to execute the current maneuver accurately.
- Short-lived execution state remains legitimate for a currently applicable
  physical maneuver or dwell; it cannot keep an obsolete instruction in force.

The implementation task must distinguish necessary execution state from
procedural authority carefully. This record does not claim that existing
firmware already conforms, authorize its cleanup, or choose actuator behavior
for an overlay transition.

## Alternative rejected

Keeping historical procedure state as operating permission, including under a
renamed latch, would make identical present position/direction/overlay produce
different requirements. Document C explicitly rejects that model.

## References

- [Position/Overlay Operating Architecture (Document C)](../NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md): full architecture and acceptance questions.
- [Documentation reconciliation](../NAVI_POSITION_OVERLAY_DOCUMENTATION_RECONCILIATION_20261005.md): exact older claims, classification, and disposition.
- [NAVI_EWO governing documents](../NAVI_EWO_GOVERNING_DOCUMENTS.md): separate read-first records for CTO/CE (A), vestigial-code cleanup (B), and position/overlay authority (C).
