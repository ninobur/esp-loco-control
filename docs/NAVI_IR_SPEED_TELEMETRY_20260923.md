# Toby IR Speed Telemetry, 2026-09-23

**Deployment update:** Dashboard deployed and Toby flashed on 2026-09-23.
Real firmware-to-dashboard telemetry verified (NO_SOURCE while IR car absent).
See NAVI_IR_SPEED_DEPLOYMENT_20260923.md. The implementation-stage notes below
record the state before that deployment; moving-speed field verification remains.

## Scope and Status

Operator requested canonical pKPH conversion plus true IR Test Car speed on
Toby's dashboard. Repository implementation only: no Pi deployment, flash,
motor command or field acceptance. Existing recovery/navigation/control
algorithms are unchanged. The separate stale AUTO enrollment defect is not
fixed by this telemetry work.

Sketch folder/Arduino shortcut remain NAVI_COHERENCE_0_6_IR_HEALTH. New boot
identifier: NAVI_COHERENCE_0_6_PROXIMAL_R1_IR_SPEED_R1.

## Measurement and Authority

Chose preferred option (a): additive fields on existing `telem/ir`, mirrored
on `diag/ir_link`, once per second. Existing keys/types remain. `telem/speed`
remains a bare Hall-derived mm/s number, preserving its existing consumers.
Tradeoff: dashboard must read `telem/ir` for Toby; it no longer mislabels the
bare Hall estimate as IR. Older object-form IR `telem/speed` remains supported.

`IrSpeedTelemetry.h` takes successive healthy same-epoch points from the
existing IR health monitor. It uses TX capture-time delta, not receiver
arrival jitter, and integer pulse subtraction before floating conversion:

`ir_mmps = delta_pulses * pitch_um * 1000 / delta_captured_us`

Decision 0022 documents this car's ten-spoke wheel, measured 96.52 mm rolling
circumference, 9.652 mm per pulse. The code uses the transmitted `pitchUm`,
not a duplicated constant. These are nominal measurements, not newly
certified error bounds. One pulse per one-second window is about 9.652 mm/s
or 1.796 pKPH; quantization and averaging remain visible.

`ir_valid`, `ir_mmps`, `ir_pkph`, `ir_speed_reason`, `ir_coupled`,
`ir_window_us`, `ir_delta_pulses`, `ir_pitch_um`, and
`ir_speed_authority:OBSERVE_ONLY` are added. Invalid speed values are JSON
null, never a fabricated zero. pKPH uses decision 0099. Dashboard recomputes
from raw `ir_mmps`; `ir_pkph` is explicitly redundant cross-check telemetry.

Warmup, unavailable optical health/readiness, stale/unpaired radio, no new
sample, epoch changes (including faults entirely between ticks), reboot,
calibration change, counter regression and queue loss cannot bridge speed
intervals. Healthy zero with fresh advancing timestamps remains a measurement.

## Coupling and Zero

Radio pairing does not prove the car is coupled. A new dashboard checkbox
**IR car coupled** sends `cmd/ir_coupled` with exactly 0 or 1. Its acknowledgment
is the next `telem/ir.ir_coupled`; no retained command or persisted confirmation.
Toby boot and source re-pair clear it. Uncheck before uncoupling. This is an
operator assertion, not automatic detection, and can be wrong if left checked.

Until confirmed, Toby's IR speed is unavailable: COUPLING_UNCONFIRMED. Fresh
coupled stationary zero is valid. Hall advance with zero pulses yields
HALL_WITHOUT_IR_PULSES. Three continuous seconds of powered zero yields
NO_PULSES_WHILE_POWERED. This conservative display diagnostic is an explicit
engineering choice, not a validated stop/stall classifier; PWM is not proof
of movement. It neither ends the IR epoch nor vetoes navigation or throttle.
These controls do not address unexpected uncoupling while the car still rolls.

The dashboard shows one decimal, `--` for unavailable, and the reason. It
uses receipt age (existing five-second display freshness), does not accept
string truthiness or nonnumeric speed, and never substitutes a bare Hall
number. Firmware's existing one-second IR link freshness remains stricter.

## Working Tree and Conversion Audit

Claude's pending dashboard changes were present before this task, including
the main canonical factor, IR link storage, Hall baseline and NAV rows. They
are preserved; this work must not stage unrelated baseline/NAV changes.

Updated speed implementation/test, current dashboard/test, live comparator,
and the pKPH conversion lines in versioned dashboards (1.9.5; 1.10.0 through
1.10.11; 1.11.0 and 1.11.1). In 1.9.5 the input is mm/ms, so it is multiplied
by 1000 before dividing by 5.37325. No physical-unit factor was changed.

Remaining literal `0.162` matches are historical comments/docs (current
dashboard explanation, IR_TEST lineage comment, IR_SENSOR_NOTES, the old
dashboard implementation report, and this decision/audit) or numeric parts of
timestamps in raw field logs, navlab result JSON and baseline_windows_C3B93D0B.csv.
No remaining executable dashboard pKPH expression uses the old factor.

Raw numeric/timestamp matches retained verbatim (full file inventory):

- analysis/ramp_templates/baseline_windows_C3B93D0B.csv
- tools/navlab/results/iter2_toby_strict.json
- tools/navlab/results/iter3_dev_toby_b2.json
- tools/navlab/results/nav_toby_b2_continue.json
- field-records/logs/navlab-inputs/20260820_morning_session.log
- field-records/logs/20260810_IR_SPEED_LOCAL_1_2_otto.log
- field-records/logs/20260811_QUORUM_1_13_beta_otto.log
- field-records/logs/20260820_espnow_repeater_shadow_run.log
- field-records/logs/20260920_navi_ir_first_paired_mqtt_snapshot.log
- field-records/logs/20260920_navi_ir_first_paired_rx_snapshot.log
- field-records/logs/20260922_navi_coherence_0_4_lap.log
- field-records/logs/20260922_navi_coherence_0_5_auto_run.log

## Verification

- ESP32 core 3.3.11 target esp32:esp32:esp32 compile; only the three existing
  Adafruit_INA219 enum warnings. Final build: 1,017,499 bytes flash,
  73,068 bytes static RAM.
- ASan/UBSan host speed checks: measured deltas, genuine zero, coupling,
  powered-zero doubt, Hall discrepancy, contrast outage, hidden epoch break,
  reboot, pitch/counter changes, queue loss, stale link and radio failure.
- Existing navigation suites: 2,052 corrections, 6,840 single-misread holds,
  32 station approaches; proximal recovery 421 ties and 65 one-vote cases.
- Existing health monitor and JSON payload checks retained.
- Offline Flask/MQTT tests plus rendered JavaScript syntax and 14 display
  validity/conversion assertions, coupling command routing and malformed-value
  rejection. Existing dashboard route suite passes. Python syntax checks pass.
- Mocked Playwright desktop/390px/320px checks: speed values fit, unavailable
  displays as a dash, no JavaScript errors. Corrected checkbox text contrast.
  Screenshots: /private/tmp/ir-speed-render/. No live control endpoints used.

Review and coordinated dashboard deployment/Toby flash are still required.
Then stationary/roll/stop/restart, coupling-off and IR power-cycle checks
precede any further moving field test. No TX/RX reflash required.
