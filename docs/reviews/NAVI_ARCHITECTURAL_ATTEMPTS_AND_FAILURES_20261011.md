# NGR NAVI — Architectural approaches that failed or were withdrawn

Status: Historical engineering review, 2026-10-11. Not a governing decision. No firmware change authorized.

## Scope and evidence
This document summarizes the development discussion and field findings through the October 2, 2026 EWO trials. “Failed” distinguishes a demonstrated unsafe/incorrect mechanism from a promising proposal not yet field validated. Do not treat an untested proposal as a failed implementation.

## Failed, rejected, or superseded approaches

1. **650-ms Hall-only fallback after IR became unavailable.** A sustained magnetic field was counted repeatedly as MM013, MM012 and MM011 after the timer expired. Elapsed time did not demonstrate a new magnetic encounter. Withdrawn by decision 0116; no Hall-only EWO advance.

2. **IR health diagnostics as veto over coherent cumulative distance.** Stationary low-contrast and diagnostic-counter changes invalidated otherwise coherent IR data, unnecessarily invoking degraded behavior. Decision 0116 separates instrument health diagnostics from mathematical frame integrity. Low contrast alone does not prove distance invalid.

3. **Automatic within-interval coordinate reacquisition after IR movement at PWM=0.** Candidate b146815 retained the declared interval and tried to anchor on the next fresh matching-polarity Hall onset. Codex demonstrated ambiguity: CW MM001→002 is 340 mm and MM002→003 is 330 mm; both targets are South. An onset after 335 mm can be either MM002 or MM003 if MM002 was missed early. The first powered IR report can also arrive after the intended landmark. Automatic recovery was rejected, and decision 0120 requires operator verification/declaration. Candidate ad89e670 removed this recovery.

4. **Treating IR wheel displacement at PWM=0 as signed route movement.** Wheel rotation during lifting/turnaround is factual sensor motion, not established track travel. On October 2, 28 such events accompanied operator handling between CW and CCW sessions. Decision 0120 requires navigation hold and explicit declaration, while a motionless PWM-zero station dwell retains localization.

5. **Assuming the full surveyed interval begins at the declaration point.** In an earlier October 2 run, the initial MM045 CCW declaration preceded 24 consecutive Missed Magnet advances despite Hall support. The origin was incorrectly treated as though Otto were exactly on the boundary rather than somewhere inside the declared interval. The subsequent startup/first-target correction was associated with 324 Hall-supported confirmations and zero misses in the reported post-fix test. Preserve the distinction between interval identity and exact within-interval coordinate.

6. **Treating a single optical pulse stream as inherently trustworthy despite hardware faults.** A displaced IR housing, then intermittent sensor wiring, caused undercounting and false ADC excursions. September 30 observations suggested ~77–80% distance reporting in one test; October 1 stationary wire manipulation produced chaotic marginal pulses. These are hardware/signal-chain failures, not proof that the EWO architecture itself failed. The sensor was rewired and the housing/track repaired.

7. **Using diagnostic sketch settings interchangeably with production TX calibration.** IR_DIAG initially used seven flags/revolution while the production Type-5 transmitter used ten and 9.652 mm/pulse. The resulting mismatch made comparisons misleading. Always identify the exact sketch and calibration.

8. **Confusing the legacy IRSpeedPacketV1 transmitter with the EWO Type-5 transmitter.** The older IR_ESPNOW_SENDER uses a different wire protocol. Integrated EWO receives 110-byte Type-5 WireSnapshot packets from IR_SCOPE_ESPNOW_TX (TX 1.6 R2 lineage). The similarly named non-integrated EWO shell lacks that receiver; the integrated build contains it. This was an artifact/protocol selection failure, not evidence that integrated EWO lacked reception.

9. **Inferring a software navigation failure from physical obstruction or track defects.** October 2 station approach stops coincided with improperly reconnected turnout rail and uneven support, then another obstruction. Repair and ballast restored smooth running. Do not count such stops as demonstrated NAVI algorithm failures.

## Explored but not demonstrated failures

- **IR-distance-defined post-magnet Hall reference:** 0–100 mm clearance and 100–200 mm reference collection were criticized as a proxy for ordinary track. One earlier run showed suspicious high references, but a post-startup-fix run had stable references (~1897–1908). Not established as failed.
- **Direct Hall recognition of magnetic encounter end / ordinary track:** proposed for baseline eligibility and potentially a physical re-arm, but not yet established from field evidence or implemented as approved navigation authority. Avoid circular dependence on a potentially contaminated baseline.
- **Native IR observation and NAVI-owned optical interpretation on the same ESP32:** future architectural proposal; the separate IR car still needs local preprocessing/transport. Not a failed field build.
- **Hall-only navigation with physical re-arm rather than a timer:** conceptual alternative to the withdrawn 650-ms fallback, not validated. Must not be equated with that failed fallback.

## Engineering lessons

- An observation is not a location declaration.
- Elapsed time does not prove passage of another magnet.
- Preserve coherent cumulative measurements independently of advisory health diagnostics.
- A declared interval is not an exact coordinate.
- Repeated polarity can make automatic reacquisition fundamentally ambiguous.
- Separate hardware faults, physical track faults, protocol mistakes and navigation algorithm faults.
- Preserve operator authority when evidence cannot establish a unique physical landmark.
- Do not change safety/operating policy or flash firmware without David's approval.

## Key records
Decisions 0112, 0116, 0119 (superseded), 0120; October 2 Otto telemetry and field discussion; EWO integrated core and TX 1.6 R2 documentation.

This review is retrospective. It does not change governing decisions, firmware, or operational policy.
