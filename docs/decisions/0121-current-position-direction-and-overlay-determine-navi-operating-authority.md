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
This authority ruling does not reopen unrelated navigation decisions, physical stopping
geography, legacy protocols, or the separate adaptive-braking architecture.

The expanded [Document C](../NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
now specifies the speed-control and physical-execution architecture, including
glide paths, steady-speed homeostasis, and terminus dwell/restart and cold-entry
rules. This record's central authority ruling and scoped supersession remain
unchanged. Document C implementation is paused for review; preserving the
revised architecture does not implement or authorize firmware changes.

## Consequences

- No station ARMED admission requirement or persistent phase authority.
- No completed-visit or one-stop-per-visit suppression **as independent operating authority**. A designated STOP tile may execute its own stop → dwell → departure instruction; that tile-local execution progress is not permission inherited from a prior visit.
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

## October 8 clarification — tile-local execution is not historical authority

The October 8 [Document C architectural decision record](../NAVI_DOCUMENT_C_ARCHITECTURAL_DECISIONS_20261008.md) clarifies the distinction:

- The **base station stopping tile** contains stop, five-second dwell, and ramp-to-cruise departure. After a physical stop and completed dwell, **the same tile's active execution instruction becomes departure**. No prior approach, entry event, Hall prerequisite, or station ARMED state grants permission.
- A **temporary traffic or other mandate** suspends the base instruction without completing it. If the mandate is removed while NAVI is physically stopped on the designated station STOP tile, the base five-second dwell begins **then**; departure follows the dwell. If NAVI is on another tile, that tile's current instruction applies.
- A **limited overlay** changes only its painted geographic footprint and replaces its previous footprint; a station PASS repaints exactly the final two tiles, leaving the five 20-pKPH tiles unchanged.
- Current consist-aware traffic restrictions are additional geographic requirements, not a procedural release state. The chosen minimum separation is **600 mm terminal clearance plus the standard stopping distance at the follower's current speed**.
- Decision **0120** continues to govern movement detected at PWM zero and any resulting localization invalidation; tile execution cannot bypass operator verification/redeclaration.

This clarification **does not reinstate** 0101's completed-visit suppression, persistent station phases, or one-stop-per-visit admission. Only minimal physical execution progress for the **currently applicable tile instruction** is legitimate. A changed overlay or current geographic requirement cannot be overruled by remembered completion of an obsolete instruction. The exact representation/reset of such execution progress remains an implementation-design and test question, not a new operating-authority source.

This is documentation reconciliation only. **No firmware implementation or flashing is authorized.**

---

## Alternative rejected

Keeping historical procedure state as operating permission, including under a
renamed latch, would make identical present position/direction/overlay produce
different requirements. Document C explicitly rejects that model.

## References

- [Position/Overlay and Speed-Control Operating Architecture (Document C)](../NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md): expanded architecture and the same-present-state authority rule.
- [Documentation reconciliation](../NAVI_POSITION_OVERLAY_DOCUMENTATION_RECONCILIATION_20261005.md): exact older claims, classification, and disposition.
- [NAVI_EWO governing documents](../NAVI_EWO_GOVERNING_DOCUMENTS.md): separate read-first records for CTO/CE (A), vestigial-code cleanup (B), and position/overlay and speed-control architecture (C).
