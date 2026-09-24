# NAVI stopped-state display, 2026-09-23

Deployment update: independently cleared by the review supplied by the operator,
dashboard deployed and Toby flashed at 115200 baud on 2026-09-23. Live R3
boot and new telemetry confirmed. Physical roll/stop check remains pending.
See `NAVI_STOP_DISPLAY_CLEARANCE_AND_DEPLOYMENT_20260923.md`. The implementation
notes below preserve the pre-deployment status at commit 427c68b.

## Request and implementation

The operator requested NAVI's interpretation on the dashboard, rather than a
raw optical health state masquerading as the train's speed. Implemented in the
existing NAVI_COHERENCE_0_6_IR_HEALTH sketch, identifier
NAVI_COHERENCE_0_6_PROXIMAL_R1_IR_SPEED_R3. No new sketch path.

At the existing one-second telemetry cadence, commanded PWM zero AND actual
PWM zero, confirmed coupling, fresh advancing reports from the same IR boot
and calibration, unchanged pulse count and no accepted Hall advance start a
two-second settling period. Quiet usable IR or INADEQUATE_CONTRAST qualifies.
Then NAVI's display result is valid 0 mm/s, reason STOPPED. A changed pulse
count, Hall advance, new command/power, stale link, duplicate capture, source
change or other fault cancels this inference. Valid positive IR speed remains
visible at zero PWM. At startup/source change a new observation baseline is
required; timing is sampled, so display latency can exceed two seconds.

This is a system inference, not a claim that PWM itself measures speed.
It neither changes odometry/epochs nor advances, forgets or relocates NAVI.
The stop classifier does not implement general sensor-fault diagnosis.

## Interface and deployment

Additive fields on existing telem/ir and diag/ir_link:
`navi_speed_valid`, `navi_speed_mmps`, `navi_speed_reason`.
Existing ir_valid/ir_mmps/ir_pkph and diagnostic reasons retain their meaning.
Thus raw ir_mmps can be null while navi_speed_mmps is zero with STOPPED.
Dashboard prefers the NAVI fields, retains old-firmware fallback, and applies
the canonical /5.37325 conversion. Stale telemetry still displays unavailable.
No dashboard-side PWM inference. Bare Hall telem/speed remains unchanged.

Extended serialization, publication queue and MQTT buffers accommodate the
additive fields. Dashboard tests check the conservative payload bound through
all three. Existing unrelated dashboard working-tree changes are preserved
and excluded from this commit.

Repository implementation only: no flash, Pi deployment, motor command or
independent review in this change. Existing prepared manual-range R2 is already
in the source lineage and would also be included by a future full sketch flash.
IR Test Car R2 bench limitations and pending R3 review are unaffected.

## Verification

Host ASan/UBSan stop-display tests and existing IR speed tests pass. Dashboard
Flask/MQTT and rendered-JavaScript checks pass, including stopped interpretation,
old firmware compatibility, explicit NAVI unavailability and stale telemetry.
The retained-history test passes all 342 route/direction scenarios.
ESP32 target esp32:esp32:esp32 builds successfully: 1,018,459 bytes flash,
73,180 bytes static RAM. Final incremental build emitted no warnings.
Payload bounds include the 1,200-byte publication queue and 1,408-byte MQTT
buffer. These checks were performed by the author, not an independent reviewer.

After coordinated Toby flash and dashboard deployment, check a coupled roll,
commanded stop, at least several seconds of dwell, and restart. Expected:
MEASURED -> STOPPED at zero -> MEASURED, with raw contrast diagnostics still
available. No sensor reflash is needed for this display change.
