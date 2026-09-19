# NAVI_SIMPLIFIED — recovered Codex reports

**Provenance:** Reconstructed from the saved response items in Codex task `01a0b3ff-f9f6-7951-b8a1-cb2b3c2edf9a`. This document preserves the original conclusions and records later amendments. It is historical input; the current controlling state is `NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md`.

## Original implementation reconciliation — accepted at the time

The prospectus's central boundary was not implemented in `NAVI_ONE_SIMPLE`. The experimental sketch used five ≥70-count samples, timestamped the first, relied on closure to permit another opening, allowed PWM-zero openings to reach NAV, struck immediately on polarity disagreement, and lacked physical reachability, transparent discrepancy judgment, and end-to-end event identity.

The report's subsystem dispositions were:

| Subsystem | Verified starting point | Required disposition |
|---|---|---|
| Opening and polarity | Five samples; timestamp first sample; signed polarity captured. | Change to two 1-kHz samples, declare/time sample two, retain polarity. |
| Baseline and waveform | Closure, 80 ms guard, 200-sample median and spread affected acquisition; no NAV waveform vote. | Keep acquisition work under review; never grant closure/morphology NAV authority. |
| Zero, restart, same magnet | Openings could queue at PWM zero; no physical same-magnet test. | Suppress zero-transition manufactured advances and preserve a physical anchor through stop/restart. |
| Timing | No navigation-active physical guard; legacy profile guard unused. | Do not inherit a fixed timer as reachability proof. Use mapped spacing and a justified bound. |
| Queues and drops | Bounded queues existed, but accounting and diagnostic visibility were incomplete. | Preserve bounded handoff; make loss and suppression reconstructable. |
| Judgment | Matching polarity advanced; mismatch struck immediately. | Add physical suppression then inspectable judgment and visible ambiguity. |
| Map and direction | Mapped 280–355 mm spacing existed; speed was display-only; direction changes altered the frame. | Preserve map and locomotive clock; expose direction disagreement and bounded history. |
| Stations | Programmed ramp machine existed and required known position. | Preserve ramp semantics; obtain an operator ruling for ambiguity. |
| Telemetry | Build identity existed but no unique event identity. | Carry `(boot_id,event_serial)` end-to-end and publish discrepancies reliably. |

The authority audit concluded that closure was indirectly navigation-active because it rearmed detection; peak/spread were acquisition-active but could affect later detections; five-sample persistence was a navigation-active gate; and legacy guards/fields were inactive. It warned not to import X19/X20 historical mechanisms by implication.

At that time the remaining operator decisions were physical Vmax, station authority during unresolved position, and the first-flight action for an unresolved physically possible contradiction. No files were changed.

## Post-reconciliation starting-point report

A second review clarified that the prospectus already settled the behavior of zero-transition suppression, stop-on-magnet protection, observable ambiguity, event identity, and drop accounting. Their state machines and schemas were engineering work, not new architectural decisions.

Still open at that point were a defensible physical bound, station authority while unresolved, and the developmental action after judgment could not resolve a contradiction. The report recommended targeted rechecks of Hall-task ownership, PWM transitions, declaration/direction epochs, station orders, and publication loss paths rather than another full audit.

## Original pre-implementation contract

The proposed contract specified:

- boot-unique identity plus monotonic event serial assigned on detection sample two;
- immediate opening delivery and diagnostic-only waveform capture;
- separate electrical rearm and physical protection;
- explicit stationary/zero-transition suppression;
- detection-anchored mapped-distance reachability;
- transparent ambiguity without silent position correction;
- ambiguity-opening event numbered 0 and ten subsequent qualifying observations;
- no counting of physically suppressed, stationary, diagnostic, duplicate, or lost events;
- controlled stop only after judgment of unresolved observation 10;
- event-keyed telemetry and focused pre-railway tests.

It also proposed a constrained global 400 mm/s rule while PWM stayed at or below 90, retained 30-second dwell, and left station authority unresolved.

## Later amendments

The following later operator rulings and railway evidence supersede those parts of the original contract:

1. Do not use the global 400 mm/s rule. Recover locomotive-, direction-, and mapped-interval-specific bounds from recorded interval data.
2. Do not replace it with the failed global 1000 mm/s rule or a global PWM-120 envelope.
3. When unsure, skip the station stop to improve the opportunity to reestablish location. Define safe phase-specific cancellation before coding.
4. Station dwell is five seconds.
5. The first SIMPLIFIED implementation was built, flashed, and failed after one physical magnet; statements that implementation had not begun are historical and no longer current.
6. The build must remain quarantined until all required tests and review dispositions are complete.
