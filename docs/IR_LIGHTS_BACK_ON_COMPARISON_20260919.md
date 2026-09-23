# Lights back on: failure did not recur

Operator reported lights back on after the request to leave the car untouched.
Freshness was verified against Pi time and recorder health: service active,
recent packet timestamp approximately two seconds behind Pi time.

Same boot 0x4d1e4080e443f121:
- Start seq 18337, captured_us 1834590178, completed 27045, rises 27239.
- End seq 18886, captured_us 1889511220, completed 27045, rises 27239.
- Elapsed sensor time 54.921042 seconds: zero new completed pulses or rises.
- No increase in saturation or sample-gap counters.
- 88 received snapshots, all INADEQUATE_CONTRAST.
- Reported span min/median/max: 15/31/47.

This follow-up does NOT reproduce false counts when the light returns.
Earlier noisy lights-on (2779 false pulses), quiet lights-off, then quiet
lights-back-on cannot establish light alone as the cause. Operator reported
a bump during the earlier lights-off change; sensor position/coupling or
electrical intermittency remain possible confounds, not diagnosed causes.
The earlier "strongly points toward lighting" interpretation must be qualified
by this failed reversal. No new firmware change was made between these windows.

Next diagnosis should compare saved noisy/quiet waveforms and isolate physical
coupling and power effects without changing multiple variables. Stationary
quietness does not establish movement accuracy; the confirmed 240/274 counts
for 100 expected remain failures requiring resolution.

Raw archive: `field-records/logs/20260919_ir_lights_back_on_capture.log`.
