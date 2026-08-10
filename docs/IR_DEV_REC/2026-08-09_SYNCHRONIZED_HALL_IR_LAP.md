# Synchronized Hall + IR lap — the detector was sound and the ruler was broken

**Date:** 2026-08-09 (night; written up 2026-08-10)  
**Run:** Otto towing the IR data car, clockwise, one Lowline lap, black plastic
10-spoke wheel with sanded spoke faces  
**Locomotive:** Otto, QUORUM 1.12B  
**IR firmware:** IR_DIAG with the 2.5 ms production debounce (`8adadc8`)  
**Evidence:**

- `field-records/logs/20260809_dualcap_otto-IR_2303-2306_estop.log`
- `field-records/logs/20260809_dualcap_otto-IR_2303-2306_estop_localtime.log`

## Why this run matters

Earlier analysis counted the individual IR `PULSE` messages that arrived at
the Pi and inferred that the detector was ignoring roughly three quarters of
the spokes. That inference was wrong. The combined capture records Otto and
IR on one Pi clock, and IR_DIAG's source-generated sequence counter survives
even when individual diagnostic messages do not. It separates physical
detection from telemetry delivery.

The methodological correction is the principal result: a received MQTT line
is evidence that a message arrived, not evidence that all physical events were
delivered. The prior analysis was measuring with a broken ruler.

## Run boundaries

- Throttle ramp begins: **23:02:56.35** (commands step from 4 to 101).
- First Hall marker of the lap: marker 41 at **23:03:03.550**.
- Last Hall marker: marker 40 at **23:06:18.445**.
- E-STOP command: **23:06:18.506**.
- E-STOP state confirmation: **23:06:19.040**.
- Otto reports PWM 0 / moving 0 by approximately **23:06:20**.

Otto remained `NORMAL`, reported no lost markers, and advanced through **171
Hall marker events**, marker 40 back to marker 40: one independently witnessed
lap. Agreement advanced 165 -> 336 while disagreement remained 4.

## IR result

IR_DIAG began the run with its source counter near 12 and reached about 5,250:
approximately **5,238 accepted rises** over the Hall-confirmed lap. Only
**1,310 individual PULSE detail records** are present in the merged extract.
The missing detail records appear as large sequence gaps and coincide with IR
MQTT offline/reconnect episodes:

- 23:03:18.080 -> 23:03:28.003
- 23:04:10.093 -> 23:04:10.510
- 23:05:18.035 -> 23:06:00.764

Thus the apparent fourfold undercount was predominantly a transport-record
failure, not a detector failure. The detector continued counting locally
while QoS-0 diagnostic lines were discarded or never delivered.

Where pulse details survived, the optical measurement was regular:

- steady rate approximately **28-31 pulses/s**;
- typical interval median **31-35 ms**;
- typical pulse width median **19-22 ms**;
- peak median approximately **190-213 counts** in the stronger windows;
- no saturation;
- four reported ten-second windows had `miss=0`; one disturbed window had
  `miss=7` (see the audit caveat below: this is an interval heuristic, not a
  sampler-miss counter).

The evidence supports reliable, continuous spoke detection on this night run.
It does **not** support further threshold or filtering changes.

## Calibration remains open

The installed IR_DIAG constants describe the previous 7-spoke, 87.34 mm metal
wheel, not the 10-spoke plastic wheel used here. Therefore its printed mm/s,
pkph, and seven-bin PHASE output are invalid for this run.

Using the source counter only:

- 5,238 pulses / 10 = 523.8 indicated wheel revolutions;
- at the operator's earlier 115 mm circumference, indicated distance is
  approximately 60.2 m;
- a nominal 55 m lap implies approximately 105 mm rolling circumference;
- a 109 mm circumference implies approximately 57.1 m.

This is now a modest geometry/calibration question, not a fourfold counting
failure. Measure the loaded rolling circumference directly before using IR
for speed or distance authority.

## IR_DIAG audit — flaws and misleading labels

This audit is against the flashed 2.5 ms-debounce lineage at `8adadc8`. No
firmware change is made by this record; the anomalies are surfaced for review
as required by the repository working agreement.

### 1. Blocking instrumentation defect: MQTT drops are silent

`emit()` puts each formatted line into a 64-message `pubQueue`. When full, it
unconditionally removes the oldest message. There is no counter for this
loss. The `drops=` value printed by STATS is `eventDrops`, which measures the
different sensor-to-loop `eventQueue`; it can remain zero while thousands of
MQTT diagnostic lines disappear. That is exactly the deception in this run.

Additionally, diagnostic messages use MQTT QoS 0. A successful
`mqtt.publish()` means accepted by the local client path, not durably recorded
on the Pi.

Required correction before IR_DIAG is again treated as a measuring
instrument: separately expose at least `event_drops`, `pubq_drops`, current
publish-queue depth/high-water mark, publish failures, and the monotonic local
detector counters. The durable Pi dual-capture should remain the evidence
record.

### 2. Major transport defect: reconnect drain can repeat the overload

The network task may publish up to eight full text messages every 5 ms with no
inter-message pacing. It calls `mqtt.loop()` only before that burst. After an
outage this can refill the TCP path immediately, starve keepalive servicing,
and prolong the reconnect/drop cycle. IR_DIAG also relies on Wi-Fi auto
reconnect without supervised recovery. The three online flaps in this lap are
consistent with the already identified transport-resilience failure class.

For a diagnostic instrument, recovery should be paced and observable. A
summary/counter channel must take priority over per-pulse prose, so the facts
needed to quantify loss cannot themselves be crowded out by the backlog.

### 3. Major semantic defect: `pulseCount` counts rises, including discards

`pulseCount` increments at the rising edge, before a falling edge completes
the event. A latch timeout or contrast-loss discard does not decrement it.
Consequently the IDLE `pulses=` value and the sequence number count accepted
rises, not necessarily completed pulse events. An open pulse at shutdown can
also add one.

This does not overturn tonight's result: latch and contrast-loss counters did
not advance during the lap, so the rise count is a close proxy for completed
pulses here. The firmware should nevertheless publish distinct monotonic
counters such as `rise_attempts`, `completed_pulses`, `latch_discards`, and
`contrast_discards`; sequence numbers for completed events should not share a
counter with discarded attempts.

### 4. Major configuration defect for the installed wheel

`SPOKES_PER_WHEEL=7` and `WHEEL_CIRCUMFERENCE_MM=87.34` describe the metal
wheel. The test used a 10-spoke plastic wheel. Printed speed and pkph therefore
have no evidentiary value, and seven-bin PHASE aggregation mixes physical
spoke identities. Correct these only after confirming the installed wheel and
measuring its loaded rolling circumference.

### 5. Major sampling limitation: claimed 1 kHz is not scheduled 1 kHz

The sensor task uses `vTaskDelay(1)` after acquisition and processing. Its
period is therefore work time plus at least one scheduler tick; it drifts and
does not represent missed slots explicitly. IR_SCOPE's later
`vTaskDelayUntil`/missed-slot accounting was built precisely to avoid
fabricated or ambiguous timing. At the observed 31-35 ms intervals this did
not prevent useful counting, but sunlight could narrow features and make this
limitation material.

### 6. `miss=` is an inference, not a measured miss

STATS increments `miss` whenever an interval exceeds 1.8 times the rolling
median. Acceleration, deceleration, a stop, a discarded event, or a true missed
spoke can all produce that shape. The label overclaims what is known. Rename
it to an interval anomaly (for example `long_interval`) unless an independent
physical witness establishes a missed spoke. It must not be confused with
IR_SCOPE's measured missed sampling slots.

## Conclusions

1. The black plastic 10-spoke wheel and the 2.5 ms detector produced reliable
   local counts during this night lap.
2. The large apparent undercount was caused by incomplete diagnostic
   telemetry. Previous conclusions based on counting received PULSE lines are
   reopened.
3. Do not tune thresholds or add rejection logic to cure this run; no such
   sensor fault was demonstrated.
4. Preserve the current detector behavior while repairing the instrument's
   accounting and transport semantics.
5. The next field question is daylight optical margin, not night counting.

## Daylight test obligation

Repeat the synchronized dual capture in direct sun, mixed sun/shade, and with
lateral halogen illumination if useful. For every run retain:

- Hall marker sequence and start/stop commands;
- IR source counters, completed-event and discard counters;
- raw/threshold evidence or compact optical summaries;
- explicit sensor-queue and publish-queue loss counters;
- source timestamps/session identifiers and one durable Pi log.

Judge daylight performance from local counts aligned to the Hall-confirmed
lap plus waveform/contrast evidence. Do not judge it by the number of MQTT
detail lines received.
