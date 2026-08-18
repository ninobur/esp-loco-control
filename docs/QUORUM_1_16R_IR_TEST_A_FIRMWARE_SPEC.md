# Toby QUORUM 1.16R IR Test A firmware specification

**Status:** Draft 1 — implementation specification  
**Date:** 2026-08-15  
**Firmware identity:** `QUORUM_1_16R_IR_TEST_A`  
**Target:** Toby, locomotive 9950012  
**Base:** field-accepted QUORUM 1.16R, with no intervening behavior change  
**Paired sender:** `IR_TEST_CAR_ESPNOW_1_0`  
**Parent specification:** `docs/QUORUM_IR_INTEGRATION_SPEC.md`

## 1. Purpose

IR Test A proves that Toby can receive trustworthy cumulative wheel evidence,
publish continuous IR and mile-marker speed, and observe Hall/IR agreement
without allowing IR to affect navigation or motion.

It is intentionally an observation build:

- IR has no motor authority;
- IR has no navigation authority;
- Q1.16R quarantine, scoring, suffix rescue and self-resolution are unchanged;
- station and CTO Bubble behavior are unchanged;
- MANUAL and AUTO behave exactly as in the accepted Q1.16R baseline.

The result of Test A is the evidence needed to specify the later supervised
solo speed-control build. It is not that speed-control build.

## 2. Build and profile identity

Implementation remains at `firmware/QUORUM/QUORUM.ino` under the repository's
one-control-sketch rule. The sketch identity changes to exactly:

```cpp
#define SKETCH_NAME "QUORUM_1_16R_IR_TEST_A"
```

`LocoConfig.h` must select `LL_LocoConfig_9950012.h` for the Toby field binary.
The implementation report records the selector line, boot line, binary size,
and Toby's Wi-Fi STA MAC/channel used by the sender configuration.

The Otto profile must also compile with IR Test A disabled. In that build, the
new services are inert and existing Q1.16R outputs/behavior remain unchanged.

Per-locomotive IR configuration is explicit:

```text
IR_TEST_A_ENABLED = 1 on Toby, absent/0 on Otto
IR_SENSOR_ID      = 0x49523031 ("IR01")
IR_SENSOR_MAC     = the Test Car ESP32 STA MAC
IR_LINK_STALE_MS  = 500 ms, provisional observation-only value
IR_PROJECT_MAX_MS = 150 ms, provisional observation-only value
```

An all-zero, broadcast, or multicast sensor MAC disables IR reception and is
reported. It never falls back to accepting any sender.

## 3. Non-authority is a hard requirement

No code reachable only because of an IR packet may:

- call `requestPwm()` or `requestPwmOver()`;
- assign `commandedPwm`, `actualPwm`, `pwmStepMs`, `autoRunning`,
  `autoEnrolled`, `navMm`, `navState`, `navDir`, `stPhase`, or CTO state;
- call a station, navigation-decision, direction-pin, or motor-write function;
- populate `CtoPeerPacket.speedValid` or `speedX10`;
- engage E-STOP, stop, pause, resume, or refuse an operator command.

The only permitted effects are loop-owned IR observation state, diagnostic
counters, and new telemetry messages. IR invalidity, disagreement, silence,
queue overflow, reboot, or malformed packets are all observational facts.

## 4. Shared wire definition

The receiver includes `firmware/common/IRSpeedWire.h`, the exact 72-byte
`IrSpeedPacketV1` contract in
`docs/IR_TEST_CAR_ESPNOW_FIRMWARE_SPEC.md` §5. It does not duplicate the struct
inside `QUORUM.ino`.

The existing frozen wire contracts remain unchanged:

- `CtoPeerPacket`: magic `0xC4`, version 3, exact existing size/layout;
- `Cto3RoleEcho`: magic `0xC5`, version 1, exact existing size/layout;
- `IrSpeedPacketV1`: magic `0xC6`, version 1, exactly 72 bytes.

Compile-time assertions pin all three sizes. The implementation report prints
the sizes and versions in Serial and the new IR status record; it does not add
fields to an existing Q1.16R payload merely for this purpose. The current packed
sizes are also pinned explicitly: `CtoPeerPacket == 45` bytes and
`Cto3RoleEcho == 15` bytes.

## 5. ESP-NOW receive path

### 5.1 Router

QUORUM retains one ESP-NOW callback registration. The callback becomes a packet
router using exact length plus magic/version; it must not enlarge or reinterpret
the CTO packets.

The callback receives and uses `esp_now_recv_info_t` so the source MAC is part
of validation. For an IR frame, the validation order is:

1. exact length 72;
2. magic `0xC6` and version 1;
3. `bytes == 72` and `reserved == 0`;
4. source MAC exactly equals configured `IR_SENSOR_MAC`;
5. `sensorId == IR_SENSOR_ID`;
6. `targetLocoId == LOCO_ID`;
7. validity is in the closed 0–5 wire enum;
8. speed encoding is canonical: measured numeric for `VALID`, exactly zero for
   `STOPPED`, sentinel otherwise.

A failure increments its literal reject counter and returns. It neither enters
the CTO queue nor the IR queue.

The callback may copy a valid packet, source MAC, and Toby-side receive
timestamp into the IR queue. It may not call MQTT, Serial, navigation, station,
CTO evaluation, or motor code.

### 5.2 Separate bounded queue

IR uses a dedicated queue so a 20 Hz sensor cannot evict or delay CTO truth.
The normative initial allocation is 32 `IrRxItem` entries. The existing CTO
queue remains eight entries and unchanged in item type and drain behavior.

An IR queue overflow increments a monotonic `irRxQueueDrops` counter and drops
that report. Because the next report carries cumulative pulses, losing a report
does not lose physical distance. The callback never blocks waiting for room.

### 5.3 Loop-owned acceptance

`serviceIrRx()` runs from `loop()` before Hall marker draining. It is the only
writer of accepted IR state. It drains all currently queued IR reports and:

- recognizes a new nonzero `bootId` as a new sensor epoch;
- accepts forward `txSequence` under wrap-safe unsigned sequence arithmetic;
- counts but rejects duplicates and out-of-order reports;
- counts missing report sequences without fabricating packets;
- rejects a pulse-count regression inside one boot epoch and invalidates the
  distance anchor;
- records current and maximum accepted inter-arrival gap;
- stores a bounded history of the latest 32 accepted snapshots with Toby-side
  receive timestamps.

A sensor reboot clears sequence and IR-distance history, makes IR speed
non-valid until a canonical current packet arrives, and increments
`irSensorReboots`. It does not change Q1.16R state.

No mux is required for accepted state because the loop owns it. Callback-side
reject/drop counters crossing into loop telemetry are copied under a short
portMUX critical section; the mux is never held while formatting or publishing.

## 6. Freshness and speed truth

For Test A, a numeric IR speed is reportable only when:

- at least one packet from the current boot epoch has been accepted;
- its validity is `VALID` or `STOPPED`;
- its encoding is canonical; and
- its Toby-side receive age is no more than `IR_LINK_STALE_MS` (500 ms).

`VALID` carries the measured positive speed. `STOPPED` carries numeric `0.00`.
Otherwise IR speed is JSON `null`. A report whose packet validity remains
`VALID` or `STOPPED` but whose radio age exceeds 500 ms is receiver state
`LINK_STALE`, not a current stopped-wheel report and not sensor state `STALE`.

The 500 ms value is provisional and has no authority in Test A. The field report
must measure the inter-arrival distribution and recommend the later control
threshold.

Conversion is:

```text
speed_mmps = speedMmpsX100 / 100.0
pkph       = speed_mmps / 5.37325
distance_mm = completedPulses * 9.652
```

Received packet count is never used as pulse count or distance.

Test A has no motor authority. In the later governor, `STOPPED` may confirm an
active zero-speed intent, but it must never cause an upward PWM correction. If
the intent requests motion, control falls back to its existing PWM preset and
publishes `MOTION_UNCONFIRMED` while the zero remains visible as sensor output.

## 7. Mile-marker speed

Q1.16R shall publish marker-derived speed from navigation-accepted physical
events. The observer hook is inside `acceptEvent()` and does not decide whether
the function is called.

For two successive accepted anchors in the same declared direction:

```text
route_distance_mm = spanMm(previousAcceptedMm, navDir, acceptedSteps)
dt_ms              = current.detectedAtMs - previousAcceptedDetectedAtMs
marker_speed_mmps  = route_distance_mm * 1000 / dt_ms
marker_pkph        = marker_speed_mmps / 5.37325
```

Ordinary acceptance has `acceptedSteps == 1`. When a quarantined event is later
vouched, its original `detectedAtMs` is used and it becomes an anchor before its
successor, exactly matching Q1.16R's existing commit order. A discarded or
`PHANTOM_REJECTED` event does not become a marker-speed anchor.

Marker-speed state resets, without a fabricated sample, on:

- boot or position declaration;
- direction change;
- sensor-independent Q1.16R position relabel (`SELF_RESOLVED`);
- zero/non-forward event time;
- any route transition that cannot be expressed as the accepted marker steps.

This observer must not replace `lastMarkerMs`, `lastSegmentDt`,
`previousAcceptedDt`, or any Q1.16R timing-gate state.

## 8. Hall/IR observation

### 8.1 Event-time IR estimate

Test A observes every received Hall detector event, including quarantined and
later-discarded events. Before `navOnMarker(e)` changes any state, the observer:

1. finds the most recent accepted IR snapshot whose Toby receive timestamp is
   not later than `e.detectedAtMs`;
2. requires that snapshot to be valid and no more than
   `IR_PROJECT_MAX_MS` (150 ms) older than the event;
3. projects its cumulative distance to event time using its valid speed;
4. subtracts the projected distance at the previous navigation-accepted Hall
   anchor;
5. records the method, source age and unrounded values.

If those conditions do not hold, IR distance for the Hall event is null. Test A
does not force a fit and does not delay Hall processing while waiting for a
future packet.

The 150 ms projection bound is provisional and observational. Offline analysis
may later bracket the Hall event with the complete before/after IR stream to
measure projection error.

### 8.2 Route hypotheses

For each event with usable IR distance, Test A calculates route spans from the
previous accepted Hall anchor for `k = 0..4` in `navDir`, correctly including
MM170/MM000 wrap. It reports:

- the five route distances;
- the nearest `k` by absolute residual;
- signed and absolute residual in millimetres;
- IR pulse/distance delta;
- Hall observed polarity and the map polarity at each nonzero candidate;
- Q1.16R's resulting immediate timing gate/disposition.

There is deliberately no pass/fail tolerance and no `SUPPORTS_DUPLICATE` or
`SUPPORTS_SKIPPED` verdict in Test A. The output says
`agreement:"OBSERVE_ONLY"`. The run exists to measure the tolerance and test
whether the hypotheses discriminate real events and known phantoms.

### 8.3 Anchor discipline and reversibility

The IR distance anchor advances only through the observer hook in
`acceptEvent()`. Detection alone does not move it.

The observer keeps a bounded 16-entry event-estimate ring keyed by
`detectedAtMs`. This allows a held event, if later vouched, to use its original
IR estimate and become an anchor in the same order Q1.16R commits it. If the
estimate is no longer present or was unavailable, the IR anchor is invalidated
rather than reconstructed from a guess.

Calls added to the Q1.16R quarantine branches may finalize telemetry as
`QUARANTINE_COMMITTED` or `QUARANTINE_DISCARDED`, but their return values are
`void` and cannot affect the existing branch condition or disposition.

## 9. MQTT topics and schemas

The sensor ESP publishes no MQTT. Toby publishes all Stage-A evidence through
the existing bounded queues.

### 9.1 `ngr/loco/9950012/telem/ir_speed`

Cadence: once per second, non-retained, normal `pubQueue`.

Schema `quorum-ir-speed/1`; required fields:

```text
schema, report_seq, report_ms
sensor_id, sensor_boot, tx_seq, sensor_ms
pulses, last_interval_ms, span, state
speed_valid, speed_mmps, pkph
rx_age_ms, rx_gap_ms, rx_max_gap_ms
```

`speed_mmps` and `pkph` are numeric when state is `VALID` or `STOPPED`; both are
exactly `0.00` for `STOPPED`. Otherwise both are JSON `null`.
`speed_valid` is `1` for both `VALID` and `STOPPED`, and `0` for every state
whose speed fields are null.

### 9.2 `ngr/loco/9950012/telem/ir_status`

Cadence: once per 5 seconds, non-retained, normal `pubQueue`.

It carries sender sampling/radio counters plus receiver totals for accepted,
sequence gaps, duplicates, out-of-order, bad length, bad version, bad source,
bad target, bad enum/encoding, pulse regression, sensor reboot and IR queue
drops. Sender and receiver counters remain separately named. Its schema is
`quorum-ir-status/1`.

### 9.3 `ngr/loco/9950012/mm/speed`

Event-bearing, non-retained, `markerPubQueue`. One initial record is emitted per
received Hall event. It combines Hall/marker speed and IR comparison so normal
events add only one marker-queue message.

Schema `quorum-mm-speed/1`; required fields:

```text
event_ms, revision
from_mm, to_mm, dir, hall_pol, peak, duration_ms, baseline_drift
timing_gate, nav_accepted, final_disposition
route_distance_mm, hall_dt_ms, mm_speed_mmps, mm_pkph
ir_valid, ir_age_ms, ir_delta_pulses, ir_distance_mm
route_k0_mm ... route_k4_mm, nearest_k, residual_mm
agreement
```

Unavailable numeric evidence is JSON `null`. `revision` is 1 for the initial
event record. A held event receives a revision-2 record only when Q1.16R later
commits or discards it; `event_ms` remains its stable identity. The revision-2
record reports the final disposition and, if committed, its accepted marker
speed.

### 9.4 `ngr/loco/9950012/telem/speed`

Cadence: once per second, non-retained, normal `pubQueue`.

This is the ongoing operator view. It repeats current IR speed/age and the latest
accepted marker speed/age, plus actual/commanded PWM strictly as actuator facts:

```text
schema:"quorum-speed-view/1"
ir_valid, ir_mmps, ir_pkph, ir_age_ms
mm_valid, mm_mmps, mm_pkph, mm_age_ms, mm_from, mm_to
commanded_pwm, actual_pwm
control_source:"PWM_PRESET"
authority:"OBSERVE_ONLY"
```

The fixed source/authority strings are acceptance checks: Test A must never
claim that IR controls speed.

### 9.5 Buffer and queue discipline

Each payload has field-by-field worst-case arithmetic proving it fits
`PubMsg::payload[512]`. Every `snprintf` return is checked. An oversize condition
publishes a small valid diagnostic instead of truncated JSON.

One-second/five-second summaries use `pubQueue` and may be dropped/coalesced under a
broker outage. `mm/speed` is physical event evidence and uses the existing
peek-publish-remove `markerPubQueue` path. Queue high-water and drop counters
must show whether the added one-message-per-event load reduced Q1.16R's outage
headroom.

## 10. Loop placement

The required loop order is:

```text
serviceCommands()
serviceIrRx()            // accept radio facts before this pass's Hall drain
drainMarkers()           // unchanged nav call; observational hooks around it
serviceIrTelemetry()     // self-rate-limited 1 s / 5 s summaries
existing status/warning/station/CTO/PWM/INA/stat services in their old order
```

`serviceIrRx()` and telemetry formatting are bounded and nonblocking. No MQTT
client method is called from loop; publication still only enqueues to the
network task.

## 11. Verification

### 11.1 Source and replay invariance

Against the exact accepted Q1.16R baseline:

- all legacy replay suites pass in both legacy and 1.16R eras;
- every legacy navigation decision, nav position/state, station event, CTO
  event, desired/commanded PWM transition and hardware PWM trace is identical;
- existing MQTT payloads are identical except the intentional sketch identity
  in boot telemetry;
- disabling `IR_TEST_A_ENABLED` compiles to behaviorally inert receiver hooks;
- `CtoPeerPacket.speedValid` and `speedX10` remain zero.

### 11.2 New deterministic tests

- exact 72-byte packet and offset fixture;
- all eight validation rejects and separate counters;
- wrong MAC and right payload still rejected;
- sequence gap, duplicate, out-of-order and wrap;
- sensor reboot and within-epoch pulse regression;
- 500 ms link-stale transition with null output;
- `STOPPED` publishes numeric zero and no other non-valid state can do so;
- cumulative pulse distance across arbitrary packet loss;
- CW/CCW route spans and MM170/MM000 wrap for `k=0..4`;
- ordinary accepted event, quarantined event, later commit, later discard and
  `PHANTOM_REJECTED`;
- direction change, declaration and `SELF_RESOLVED` reset observers only;
- marker and telemetry queue pressure;
- static/call-site audit proving no IR-to-motor or IR-to-navigation path.

### 11.3 Bench pairing

Before track use:

1. print and record Toby and sensor STA MACs and channel 11;
2. rotate the wheel by hand and show pulse count and valid speed at Toby;
3. stop the wheel, including on a spoke, and show `STOPPED` with numeric zero;
4. resume rotation and show `REACQUIRING` followed by `VALID`;
5. power-cycle the sensor and show a new boot epoch;
6. temporarily change sender channel/source/target and show factual rejects or
   delivery failures;
7. disconnect MQTT and verify reception/counters continue locally without
   navigation loop delay or marker loss.

### 11.4 Supervised field gate

Toby is the only locomotive on the track with the IR Test Car coupled into its
consist. IR Test A remains observational in MANUAL and AUTO-preset laps.

Pass requires:

- continuous 1 Hz IR and combined speed views;
- one coherent `mm/speed` record per Hall event, with revisions for held events;
- IR and accepted marker speed agree within the existing 10% expectation on
  clean steady segments, with every exception enumerated;
- known phantom sites and any Q1.16R quarantines show their raw IR distance and
  nearest route hypothesis;
- packet gaps do not become pulse/distance loss;
- numeric zero only for fresh explicit `STOPPED`, no unreported receiver
  silence, and no malformed JSON;
- no change in navigation, station, CTO, AUTO, MANUAL, E-STOP or PWM behavior;
- no event, marker-publication or IR-queue drops.

Passing Test A authorizes writing/reviewing the separate supervised solo
IR-speed-control specification. It does not activate that control.

## 12. Do not add to Test A

- an IR speed governor or any PWM correction;
- IR-based stop, resume, AUTO dropout or NO_QUORUM transition;
- automatic Hall-event acceptance/rejection from IR;
- guessed Hall/IR agreement tolerance;
- CTO peer speed publication;
- direct sensor MQTT;
- a widening/full-ring recovery search;
- protection against a merely hypothetical failure without field evidence.
