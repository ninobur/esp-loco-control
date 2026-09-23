# Toby CAL0_FIX1 flash and initial telemetry

2026-09-23: David authorized USB flashing Toby. Uploaded the tested 0.6
CAL0_FIX1 artifact using arduino-cli, esp32:esp32:esp32, UploadSpeed=115200,
/dev/cu.wchusbserial10. ESP32-D0WD-V3, MAC ec:e3:34:78:a2:60.
All written data hashes verified; upload exited successfully and reset via RTS.

Pi MQTT at 13:56:26-27 confirmed health_revision=CAL0_FIX1 on Toby's
diag/ir_health, loco_boot D510A9D420B0566B. Accepted packets increased 42 -> 52,
rejected stayed 0, fresh=1. calibration_id=0, distance_bounds=UNVALIDATED.
Health was INADEQUATE_CONTRAST, readiness UNAVAILABLE, epoch inactive,
no MM reference. Cumulative pulses were 125 in both observations.

This confirms the firmware packet-admission correction on hardware, not optical
accuracy or field acceptance. Earlier capture had 119 pulses; no conclusion
about the intervening six pulses is drawn without handling/motion context.
No motor command or Pi/TX/RX change was performed. Operator was asked to hold
stationary; full stationary dwell and movement recovery remain to be tested.
