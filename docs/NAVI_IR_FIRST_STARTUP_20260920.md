# NAVI_IR 0.1 first field startup - 2026-09-20

## Scope and operator setup

All times are PDT. The operator supplied the NAVI_IR 0.1 boot output and
announced a clockwise run from MM040-041. The measuring IR sensor is approximately
8 inches (203.2 mm) behind the Hall sensor. This is an operator estimate, not a
new wheel calibration or a measured timing correction.

The run-start message was checked at approximately 13:33:12. Logged commands
give more precise events: CW declaration at 13:32:02.225, interval 040-041 at
13:32:06.886, throttle 90 at 13:33:18.938.

## Deployment confirmation

Pi telemetry at 13:31:50.731 confirms:

- Sketch: NAVI_IR_0_1, Toby 9950012.
- Boot ID: 391FD3B844D1CA21.
- AUTO enabled: 0; field accepted: 0; traffic coordination: 0.
- Hall entry 38 counts, window 400 ms, refractory guard 0 ms.
- IR source type 5, distance bounds UNVALIDATED.

This updates the prior build-only status: the operator has now flashed Toby.
It does not establish field acceptance or successful IR navigation.

## Initial blocker and response

The receiver was listening on channel 11 with radio_ready=1 and channel_ok=1.
It continuously saw CRC-checked movement frames from 38:18:2B:30:8C:2C, but
paired=0, accepted=0 and fresh=0. Thus the locomotive was hearing IR but was
not accepting any source as its movement input.

Codex asked the operator to stop for pairing and confirm that the car behind
Toby was the only powered IR test car. No motor, pairing, declaration, or other
MQTT command was sent by Codex. Source verification is pending at this snapshot.

The operator's throttle-0 command appears at 13:33:48.642. At 13:34:24.228 Toby
reported PWM 0, AUTO 0 and NAV LOST. The committed MM remained 40 with zero
advances and 11 refusals: that is the held declaration, NOT Toby's current
physical location. Hall event_serial had reached 30 by 13:34:13; these are
candidate observations, not 30 verified magnets. The recovery window expired
without usable IR. This attempt cannot validate joint Hall/IR navigation or
be treated as evidence that the optical sensor failed.

The independent Pi recorder health checked at approximately 13:33:31 reported
active output in /home/david/NGR/ir_espnow/ir_espnow_raw_20260920.log, PID 578939,
69,495 recorded lines and 27,642,422 bytes. This is separate from the unpaired
locomotive receiver.

## Meaning of the 8-inch separation

For fixed separation, the displacement of the wheel between two simultaneous
Hall-event snapshots equals the Hall sensor's displacement over that interval,
subject to the actual mechanical/path geometry. Do not add 203.2 mm to every
magnet interval or shift the wheel stream as though it detects the same magnet
later. It measures wheel rotation, not a delayed Hall landmark crossing.

The car position matters for associating sun/shade exposure with route sections
and for assessing coupling changes, reversal and relative motion. No offset,
pulse count, pitch or firmware parameter was changed here.

## Next steps

1. Confirm the intended IR source while Toby is physically stopped.
2. Pair that source; verify accepted snapshots, freshness and wheel response.
3. Redeclare Toby's actual current interval and CW direction. Do not reuse the
   old MM040-041 declaration unless that is where the operator places him.
4. Begin a fresh Manual test segment with paired IR. Keep this pre-pairing
   segment separate in analysis.

## Evidence and provenance

Read-only checks used the Pi daily telemetry log
/home/david/NGR/telemetry/all_20260920.log and the recorder health JSON
/home/david/NGR/ir_espnow/ir_espnow_health.json. The compact captured telemetry
snapshot is archived alongside this record in
[the startup snapshot](../field-records/logs/20260920_navi_ir_startup_snapshot.log).
It is a snapshot through 13:34:24, not a completed-run archive.

See the [implementation report](NAVI_IR_0_1_IMPLEMENTATION_REPORT.md) and
[operating notes](../firmware/test-programs/NAVI_IR/README.md).

## Pairing confirmed at 13:35:57

The operator subsequently confirmed Toby was stopped, no other locomotive was
powered, and the IR test car and Toby were close together. In that context,
Codex sent only the source-pairing command, not a motor or navigation declaration:

- 13:35:57.538: cmd/ir_pair = 38:18:2B:30:8C:2C.
- 13:35:57.590: acknowledgement, "IR source paired; new movement frame, no position change".
- Two consecutive live reports: accepted 47 then 57; fresh=1; age 53/54 ms;
  pulse count 673 in both; channel 11; radio ready; no queue drops.
- 13:36:20.304: accepted=227, fresh=1, age=51 ms, pulses still 673.
  The rejection total remained 2474 from the pre-pairing period; it was not
  increasing as a new fault.
- Sender boot ID is 3B48EBDDC9F3895F. It is a different session from the noon
  evidence and must not be merged into that run's cumulative counter.
- Optical reason=1 (INADEQUATE_CONTRAST), span=15, while the operator reports
  stationary. This is not a valid moving-distance interval. A quiet wheel may
  have little changing optical contrast; link success does not yet validate
  rolling pulse quality. No physical zero-motion claim is inferred from this.
- Toby reports PWM=0, AUTO=0, ir_fitted=1 and NAV LOST. Pairing correctly did not
  invent a location or clear the failed navigation state.

Codex instructed the operator to redeclare the actual current CW interval before
a new Manual segment; MM040-041 is valid only if that is the actual placement.
Rolling response and joint navigation validation remain pending.

The [pairing snapshot](../field-records/logs/20260920_navi_ir_pairing_snapshot.log)
preserves the acknowledgement and the later stopped-state check.

## First paired Manual run: low-contrast input, no navigation confirmation

The operator redeclared CW interval 069-070 at 13:38:26.760 and commanded Manual
throttle 90 at 13:38:34.624. This is a new segment, distinct from the earlier
unpaired start at 040-041. The following findings are from a snapshot through
13:39:49, not a completed-run verdict.

- Direct car-to-Toby delivery worked: paired=1, fresh=1, accepted snapshots
  increasing, with no new rejects or receiver queue drops in the inspected
  reports. Manual PWM reached 90; AUTO stayed disabled.
- Independent Pi data agrees that optical contrast was mostly low. At cruising
  portions, typical reported spans were approximately 191-239 ADC counts,
  versus median spans around 1247-1263 in the good noon run. This is a changed
  observed signal, not evidence by itself of the physical cause.
- Over 13:38:26.104-13:39:49.337, 535 received movement snapshots comprised
  516 INADEQUATE_CONTRAST, two REACQUIRING and only 17 TRACKING. This includes
  the initial stationary/ramp period; it is not a pure steady-speed rate.
- Counts rose from 699 to 1820. Counts alone do not make the interval valid:
  the sender's detector can count transitions above its 120-count contrast
  gate but only reports TRACKING at span >=300 with the other conditions met.
  The inspected bins showed unreliable-sample increases, not ADC saturation
  or sampling-gap increases. No counts were repaired or recalibrated.
- The first Hall observation at 13:38:42.595 was N, while the declared next
  marker MM70 expects S. Three alternatives were retained. This is a separate
  initial discrepancy, not proof of which physical magnet was observed.
- The navigator never committed an advance in this segment. It reported ten
  AMBIGUOUS observations and RECOVERY_EXHAUSTED at 13:38:58.555 (event 41).
  The held MM69 is not a live physical position. Subsequent NO_POSITION reports
  continue logging observations but do not recover automatically from LOST.
- Many interval comparisons report OPTICAL_INVALID. Their zero ir_pulses/
  ir_nominal_mm fields are default values on the rejected-interval path, NOT
  evidence that the wheel count stayed zero. The increasing cumulative count
  is independently visible in diag/ir_link.
- Pi reception over the scoped endpoints lost about 38.36% of raw packet
  sequence positions and 35.77% of movement snapshots. Those are Pi receiver
  statistics, not the nearby locomotive's loss rate. The optical invalidity
  is transmitted by the car and is not explained away by Pi packet loss.

Codex asked the operator to stop and asked whether the IR car's sensor position,
wiring or wheel insert changed since the good noon run, or only Toby was
reflashed. No answer or confirmed final stop is included in this snapshot.
No motor command, threshold adjustment, firmware change or reflash was made
during this check.

Conclusion: pairing works, but this segment did not deliver consistently valid
IR distance or demonstrate working joint navigation. The new evidence calls for
checking the physical optical signal and comparing the setup to noon before
considering any threshold change. It does not identify sunlight, wiring,
alignment or another cause by itself, and lowering the validity threshold would
not establish count accuracy.

Reproducible evidence:

- [Raw IR snapshot](../field-records/logs/20260920_navi_ir_first_paired_rx_snapshot.log)
- [MQTT snapshot](../field-records/logs/20260920_navi_ir_first_paired_mqtt_snapshot.log)
- [Structured audit](../field-records/analysis/20260920_navi_ir_first_paired_audit.json)
- [Audit script](../tools/navi_ir_first_run_audit.py)

Run from the repo root:

```sh
python3 tools/navi_ir_first_run_audit.py field-records/logs/20260920_navi_ir_first_paired_rx_snapshot.log field-records/logs/20260920_navi_ir_first_paired_mqtt_snapshot.log
```
