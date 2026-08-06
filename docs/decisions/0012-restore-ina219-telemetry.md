# 0012 — INA219 telemetry is restored to the control firmware

Status: Accepted  (operator, 2026-08-06)

## Decision
The INA219 returns to QUORUM per the CTO3 plan step labelled "QUORUM 1.5":
voltage/current/power telemetry on the r12 topics, the low-voltage state
flag, and `v=` added to the mm/marker line. Otto's open INA219 hardware
fault is resolved as part of the same work.

## Context
r12 carried a full INA219 service: 5 s telemetry (`telem/voltage`,
`telem/current`, `telem/power`) and a low-voltage flag against
`LOW_VOLTAGE_THRESHOLD_V`. SOLONAV 2.1 dropped it without a record; the
dashboard's power tile went permanently stale and the gap was discovered
by archaeology. CTO3 §9 now depends on it: the voltage-compensated PWM
fallback table (`estimated_pKPH ≈ table_pKPH × v_now/v_ref`) cannot exist
without bus voltage, and the calibration grid is blocked until it flows.

## Alternatives considered
- Leave it dropped, remove the dashboard tile — rejected: CTO3's
  speed-fallback design needs the measurement, independent of the tile.
- External voltage monitor node — rejected: the compensation needs the
  voltage at the locomotive, correlated with its own PWM, on its own
  telemetry.

## Consequences
Restores hardware truth the low-voltage cutoff claim depends on (CLAUDE.md
lists low-voltage cutoff among the authorities that precede automation —
currently unbacked by any measurement). I²C on SDA 21/SCL 22 returns to
the sensing path; boot must tolerate a missing/faulted INA219 (r12's
`ina219Available` pattern) so a sensor fault never blocks a run — Otto's
current fault is exactly that case until fixed.

## References
`docs/CTO3/CTO3_SPEC.md` §9 and plan step 2;
`archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino:1544,2224`;
`DASHBOARD_1_10_4_IMPLEMENTATION_REPORT.md` (INA219 root cause).
