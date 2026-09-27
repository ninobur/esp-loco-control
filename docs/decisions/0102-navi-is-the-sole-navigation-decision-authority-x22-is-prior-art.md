# 0102 - NAVI is the sole navigation decision authority; X22 is prior art

Status: Accepted as principle (2026-09-27, David with Sam, first-pass review of
D1–D10). Documentation only; no implementation authorized.
Decided by: David, after review with Sam. The quotations below are from David's
codification instruction of 2026-09-27.

## Decision

"NAVI is the sole navigation decision-making authority. Processing may be
distributed for computational efficiency, organization, testing, and signal
conditioning, but navigation judgment is not distributed."

- Hall, IR, PWM, direction, map, history and other sources provide data. Low-level
  processing may condition or characterize them. NAVI determines their navigation
  significance with the context available when the decision is required.
- "Delegating a system-significant decision to the component with the least relevant
  information is the wrong decision structure." A subordinate module must not make an
  irreversible navigation judgment because it is closer to the sensor, nor withhold
  relevant data because it has already decided what the data mean.
- **Create the authority structure anew.** Do not refactor X22 into NAVI and call it
  the new architecture. X22 is prior art. Its knowledge is preserved (Hall conditioning,
  baseline work, the 70-count departure, two-sample persistence, opening polarity,
  electrical return, quiet/spread characterization, waveform observations, degraded-mode
  concepts). Its authority structure, gates, state machine and assumptions carry no
  presumption of survival.
- **No X22 shadow decision system (D7).** Reporting "what X22 would have done" would
  give X22 a path to survive by renaming, comparison or gradual reintegration. NAVI
  itself is instrumented instead. Historical X22 behavior remains in old code and
  field records.

## Context

X22 was developed under a system with far less physical-state information: no IR
distance, no MM/IR synchronization, no mapped-distance windows. The 2026-09-26 X22R
audit found X22R exercising navigation consequences before NAVI could judge
(N1–N8 in the reconciliation plan):
- the 400 ms window suppressed openings;
- PWM-zero and old-field holds suppressed openings;
- cadence, MOVING and STOPPED refused baseline opportunities;
- the baseline cycle was keyed to raw openings that NAVI had rejected.

There is one ESP32. No hardware or computational reason has been identified for a
lower-context decision-maker between the data and NAVI.

## Alternatives considered

- **Refactor X22R into a "measurement stage" with most gates removed** (plan §5.1 as
  drafted 2026-09-26). Rejected as the framing: it starts from X22's structure. The
  plan's inventory and evidence remain useful.
- **Keep X22 as a shadow comparator (D7).** Rejected, for the reason above.

## Consequences

- Every inherited mechanism is examined from its physical purpose: what problem it
  solved, what information was missing then, how NAVI should address it now
  (`NAVI_DECISION_MODEL.md` §5).
- D2's framing (reproduce X22's PWM-zero and stopped-in-field outcomes) is
  superseded: preserve the information and concepts, not outcomes.
- **Risk.** Field-proven 20Q3 behavior (117/117, 926/926 on 2026-09-26) was achieved
  *with* X22R's gates in place. A new structure must re-earn that record on track;
  some gate may turn out to have been protecting against a real phenomenon that was
  never characterized. The response is to characterize that phenomenon when it
  appears, not to reinstate the gate by default.

## References

- `../NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md` (enduring principles)
- `../NAVI_DECISION_MODEL.md`
- `../NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md`
- `../../field-records/verdicts/20260926_toby_20q3_run{1..5}.md`
- `../NGR_NAVI_ARCHITECTURAL_LINEAGE_DRAFT_v0_1.md` ("Sensors observe; NAVI knows")
