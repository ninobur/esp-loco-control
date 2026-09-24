# IR Speed Dashboard Deployment and Toby Flash, 2026-09-23

Operator requested working dashboard deployment ASAP and connected Toby by
USB for the firmware flash. Completed around 17:50-17:53 PDT.

## Pi Dashboard

Installed /home/david/ngr_app.py and restarted ngr-app.service; active verified.
Rollback copy: /home/david/ngr_app.py.pre_ir_speed_20260923_1800 (name is a
unique backup label, not the actual deployment time).

Deployed SHA256:
7d5f56fb4b45837a243fb4d9c2f9fa5e497fe7d887071fd11c16cf44fb3077fc

Focused deployment includes canonical pKPH, genuine IR speed source, validity,
reason display and approved coupling confirmation. Unrelated pending Hall
baseline/NAV display edits remain in the working tree, not deployed or staged.
The committed server source matches the deployment; its working-tree copy
also contains those separate pending edits. Three offline tests and 14 JS
assertions were rerun against the exact deployment candidate before install.
Live HTTP verifies IR pKPH, irSpeedView and canonical multiplier.

## Toby

Verified ESP32-D0WD-V3 rev3.1 MAC ec:e3:34:78:a2:60 on
/dev/cu.wchusbserial110. Recompiled core 3.3.11: 1,017,499 bytes flash and
73,068 bytes static RAM. Uploaded at 115200 baud; all flash hashes verified.

Application binary SHA256:
b4f9891da05902e7d9061a044a9ba354d8bfb832295a112669b0b09f3da25880

Live MQTT boot: NAVI_COHERENCE_0_6_PROXIMAL_R1_IR_SPEED_R1,
boot_id 008F48EB7E351960. Existing Arduino folder/shortcut unchanged.

## End-to-End Check and Remaining Field Check

Real Toby telemetry reached the real dashboard API: fresh ir_link, paired=1,
radio_ready=1, seen=0, ir_valid=0, ir_mmps=null, ir_pkph=null,
ir_speed_reason=NO_SOURCE, ir_coupled=0. PWM=0, AUTO=0. This demonstrates
the firmware-to-dashboard path and truthful unavailable state, not a measured
moving-speed field acceptance. IR car had not supplied packets at verification.

Operator should power IR car, physically couple it and confirm the dashboard
checkbox. Then verify a stationary valid zero and a positive hand-roll reading.
Do not confirm coupling merely to hide NO_SOURCE. No throttle, AUTO or
coupling commands were sent by the agent. No TX/RX firmware was changed.
Navigation changes and the earlier END AUTO release defect are outside this
deployment; this is display telemetry only.
