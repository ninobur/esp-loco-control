# NAVI stop-display field check, 2026-09-23

## Observed result

The operator announced the final run after the cleared R3 deployment. A bounded
120-second, read-only MQTT subscription observed stopped startup, manual motion,
a stop and restart. No locomotive commands were sent by Codex.

Toby rebooted to R3 boot `2C074C69CDA60A10`; coupling cleared as designed.
The operator was reminded to confirm coupling, and telemetry subsequently
reported ir_coupled=1. AUTO was off at startup; applied PWM rose during motion.

The requested display behavior was observed:

- Motion: positive MEASURED speed, including approximately 47-54 pKPH.
- Stop: pulse count steady at 1268, applied PWM 0. Three consecutive captured
  reports (TX sequences 5490, 5500, 5510) showed raw ir_valid=0 and
  ir_speed_reason=INADEQUATE_CONTRAST, while navi_speed_valid=1,
  navi_speed_mmps=0.000 and navi_speed_reason=STOPPED.
- Restart: by sequence 5590, count 1274, raw and interpreted speed both
  MEASURED at 28.956 mm/s / 5.389 pKPH; next throttle report was 40.

Thus NAVI's system-level stopped judgment reached telemetry without relabeling
the raw instrument reading. Startup R3 transport through the live dashboard
was already verified in the deployment record. Operator visual confirmation
of the field display itself is separate from these MQTT observations.

## Evidence and limits

`field-records/logs/20260923_navi_stop_display_field_excerpt.log.gz` preserves
102 IR reports (sequences 4579-5590) plus interleaved applied-throttle messages.
This is a partial excerpt of the bounded subscription, not a complete run log;
earliest startup messages were inspected but are not in the excerpt. Messages
have no added wall-clock timestamps; use TX boot D542D673B80110D9 and sequence.
The final "Timed out" is the intentional 120-second subscriber limit, not a
radio or firmware fault. No ongoing subscription remains from this check.

This establishes the narrow display stop/restart behavior. It does not certify
count/distance accuracy, calibration, the TX detector revision or AUTO operation.
It does not withdraw the documented IR Test Car R2 detector limitations.
