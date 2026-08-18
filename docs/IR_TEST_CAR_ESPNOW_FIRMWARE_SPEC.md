# IR Test Car ESP-NOW sender firmware specification

**Status:** Draft 1 — implementation specification  
**Date:** 2026-08-15  
**Firmware identity:** `IR_TEST_CAR_ESPNOW_1_0`  
**Target path:** `firmware/test-programs/IR_ESPNOW_SENDER/IR_ESPNOW_SENDER.ino`  
**Paired receiver:** Toby, locomotive 9950012, running `QUORUM_1_16R_IR_TEST_A`  
**Parent specification:** `docs/QUORUM_IR_INTEGRATION_SPEC.md`

## 1. Purpose and authority

This firmware runs on the proven IR Test Car ESP32. It samples the 10-spoke
wheel, computes qualified wheel speed locally, and sends cumulative evidence to
Toby by ESP-NOW.

It has no motor, navigation, MQTT, dispatcher, station, CTO, or locomotive
authority. Its complete output is one ESP-NOW packet stream plus bounded local
commissioning Serial output.

This is the Stage-A sender. It supports transport and Hall/IR observation only.
It is not, by itself, approval for IR to control Toby.

## 2. Lineage that must be preserved

The sensor and speed code is derived from
`firmware/test-programs/IR_SPEED_LOCAL/IR_SPEED_LOCAL.ino`, identity
`IR_SPEED_LOCAL_1_2`. The following behavior and constants are copied without
retuning:

| Item | Required value/behavior |
|---|---|
| ADC pin | GPIO 34 / ADC1_CH6 |
| ADC resolution | 12 bit |
| sample period | 1,000 us nominal |
| wheel | 10-spoke molded-plastic LGB wheel |
| rolling circumference | 96.52 mm |
| distance per completed pulse | 9.652 mm, derived from the two values above |
| envelope | 2,048-sample rolling 5th/95th percentile |
| envelope prime | 512 samples |
| envelope update | 250 ms |
| rising/falling thresholds | 2/3 and 1/3 of span |
| speed filter | median of five measured nonzero intervals |
| minimum usable span | 120 counts |
| marginal span boundary | 300 counts |
| stale timeout | 2,500 ms |
| open-pulse abort | 2,500 ms, re-arm only after physical low |

The proven local acquisition validity values remain exactly:

```text
0 INVALID_CONTRAST
1 MARGINAL
2 REACQUIRING
3 VALID
4 STALE
```

The Stage-A wire adds one output state without changing that acquisition enum:

```text
5 STOPPED
```

`STOPPED` is derived by the packet-production layer when cumulative completed
pulses have not advanced for `IR_STOPPED_MS = 2500`. It carries numeric zero.
`VALID` carries measured speed. Every other state carries the invalid numeric
sentinel specified in §5. No interval is rejected by comparison with previous
intervals, and no missed spoke is inferred.

The sampler remains a FreeRTOS task pinned to core 0 at priority 2. It updates
`LocalSpeedSnapshot` and `HealthCounters` under the existing short critical
section. Radio code reads one coherent copy and never reads sampler-owned fields
piecemeal.

## 3. Deliberate removals from IR_SPEED_LOCAL

The sender build shall not define `IR_LOCAL_WIFI` and shall not include:

- `PubSubClient`;
- broker address or port;
- Wi-Fi SSID/password or `credentials.h`;
- MQTT topics, queues, reconnect logic, or per-pulse publication;
- an NTP/time dependency.

`IR_LOCAL_DEBUG` may remain as a compile-time switch for per-pulse Serial during
bench commissioning. It is off for field operation.

The original `IR_SPEED_LOCAL` sketch remains unchanged as a frozen reference.
The implementation report must identify any acquisition-line difference
between it and the sender; radio additions are not acquisition differences.

## 4. Radio configuration

The sender uses Wi-Fi STA radio mode without associating to an access point:

```text
IR_ESPNOW_CHANNEL = 11                 // provisional current railway EAP channel
IR_SENSOR_ID      = 0x49523031         // ASCII "IR01"
IR_TARGET_LOCO_ID = 9950012
IR_TX_INTERVAL_MS = 50                 // 20 reports/s, provisional Stage-A value
IR_STOPPED_MS     = 2500               // no pulse-count progress -> STOPPED/0
```

The target address is Toby's Wi-Fi STA MAC in a non-secret local configuration
header. The implementation must refuse to start transmission if the configured
MAC is all-zero, broadcast, or multicast. Boot Serial prints both local and
target MAC addresses and the configured channel for physical verification.

The peer is unencrypted unicast for Stage A. This is acceptable only because
the receiver has no motor or navigation authority. Before any received IR value
can control Toby, encryption/spoofing risk must receive an explicit decision;
Stage A must not acquire authority merely by changing a flag.

Setup order is:

1. `WiFi.mode(WIFI_STA)` with persistence and sleep disabled;
2. set the radio to `IR_ESPNOW_CHANNEL` using `esp_wifi_set_channel()`;
3. initialize ESP-NOW;
4. add exactly Toby's unencrypted unicast peer on that channel;
5. register the send callback;
6. start the existing sampler task;
7. begin the 50 ms latest-snapshot reporting service.

Any failure in steps 2–5 leaves sampling operational but transmission disabled.
It prints one factual failure and continues local measurement. It must not enter
a reboot loop.

## 5. Frozen Stage-A wire packet

The sender and receiver include one shared definition from
`firmware/common/IRSpeedWire.h`. The packet is packed, little-endian on the two
ESP32 endpoints, exactly 72 bytes, with:

```cpp
static constexpr uint8_t  IR_SPEED_MAGIC   = 0xC6;
static constexpr uint8_t  IR_SPEED_VERSION = 1;
static constexpr uint32_t IR_SENSOR_ID_IR01 = 0x49523031UL;
static constexpr uint32_t IR_SPEED_INVALID_X100 = 0xFFFFFFFFUL;

struct __attribute__((packed)) IrSpeedPacketV1 {
  uint8_t  magic;
  uint8_t  version;
  uint16_t bytes;
  uint32_t sensorId;
  uint32_t targetLocoId;
  uint32_t bootId;
  uint32_t txSequence;
  uint32_t capturedMs;
  uint32_t completedPulses;
  uint32_t lastIntervalMs;
  uint32_t speedMmpsX100;
  uint16_t opticalSpan;
  uint8_t  validity;
  uint8_t  reserved;
  uint32_t sampleMissedSlots;
  uint32_t openAborts;
  uint32_t contrastInvalidEpisodes;
  uint32_t saturatedSamples;
  uint32_t sampleMaxGapUs;
  uint32_t txAttempts;
  uint32_t txImmediateErrors;
  uint32_t txDeliveryFailures;
};
static_assert(sizeof(IrSpeedPacketV1) == 72, "IR wire size changed");
```

Field offsets are normative:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | magic |
| 1 | 1 | version |
| 2 | 2 | bytes, always 72 |
| 4 | 4 | sensor ID |
| 8 | 4 | target locomotive ID |
| 12 | 4 | sensor boot ID |
| 16 | 4 | report sequence |
| 20 | 4 | sampler snapshot time, sensor `millis()` |
| 24 | 4 | cumulative completed pulses |
| 28 | 4 | most recent measured interval in ms |
| 32 | 4 | speed in 0.01 mm/s, or `0xFFFFFFFF` |
| 36 | 2 | current optical span |
| 38 | 1 | closed validity enum |
| 39 | 1 | reserved, must be zero |
| 40 | 32 | eight monotonic health/radio counters; final field starts at 68, packet ends at 72 |

Canonical speed encoding is structural:

- `validity == VALID`: `speedMmpsX100` is a finite measured value and is not
  `IR_SPEED_INVALID_X100`;
- `validity == STOPPED`: `speedMmpsX100 == 0`;
- any other validity: `speedMmpsX100 == IR_SPEED_INVALID_X100`.

An invalid measurement is therefore impossible to mistake for a measured stop.

## 6. Packet production

Every 50 ms the loop copies `LocalSpeedSnapshot` and `HealthCounters` under the
sampler mux, releases the mux, and then constructs and sends the packet. No lock
is held across `esp_now_send()`.

`txAttempts` and `txSequence` increment once per attempted report. Immediate
`esp_now_send()` failures increment `txImmediateErrors`. A non-success delivery
callback increments `txDeliveryFailures`. The callback touches only its bounded
counter state; it performs no Serial output and no retry.

Packets report the latest cumulative state. They are not queued. If one report
is lost, the next report supersedes it and carries the complete pulse count and
current health counters. There is no catch-up burst.

The packet-production layer remembers when it last observed
`completedPulses` advance. While pulses advance, it transmits the acquisition
state unchanged. Once no advance has occurred for `IR_STOPPED_MS`, it transmits
wire state `STOPPED` with speed zero, even if the stationary wheel has left the
local envelope collapsed or an optical pulse held open. The acquisition state,
envelope, open-abort logic and counters themselves are not modified.

At boot, the no-progress clock begins after sampler startup; a wheel that
remains stationary therefore becomes `STOPPED` after 2.5 seconds. The first
later pulse immediately leaves `STOPPED` and exposes the acquisition layer's
normal `REACQUIRING` then `VALID` sequence.

This is an observation, not proof of cause: a blinded sensor on a physically
moving wheel can imitate no pulse progress. The receiver therefore publishes
the zero and state explicitly, and future motor control is forbidden from
using `STOPPED` to increase PWM.

`bootId` is a nonzero `esp_random()` value generated once at boot. It is not an
NVS identity and need not survive a restart. A restart intentionally gives the
receiver a new pulse-count epoch.

All integer scaling is range-checked before conversion. A valid speed too large
for the wire representation increments a local diagnostic and is transmitted
as non-valid rather than wrapped. With the chosen `uint32_t` scale this is not
expected on the railway.

## 7. Serial contract

Normal field Serial is limited to:

- one boot identity/configuration line;
- one radio initialization success/failure line;
- one health line every 5 seconds containing validity, pulses, span, sampling
  counters, tx attempts/errors, and current channel.

No per-pulse Serial is emitted unless `IR_LOCAL_DEBUG` is explicitly enabled.

## 8. Failure semantics

| Condition | Sender behavior |
|---|---|
| no optical contrast | keep sampling; transmit `INVALID_CONTRAST` and sentinel speed |
| marginal contrast | keep measuring; transmit `MARGINAL` and sentinel speed |
| fewer than five strong intervals | transmit `REACQUIRING` and sentinel speed |
| no completed-pulse progress for 2.5 s | transmit `STOPPED` and numeric zero |
| packet send failure | count it; next scheduled snapshot proceeds normally |
| target absent or wrong channel | delivery failures rise; sensor computation continues |
| sensor task failure at creation | fatal boot halt; never pretend to transmit valid data |
| ESP-NOW initialization failure | measurement continues; radio disabled and reported |

Radio failure never resets the envelope, interval median, pulse count, or
validity state.

## 9. Required verification

### 9.1 Desk and bench

- Both endpoints compile against the same shared header and assert 72 bytes.
- A packet byte fixture proves every offset, the invalid sentinel, and the
  unique `STOPPED`/zero encoding.
- Sender compilation contains no MQTT, credentials, broker, or Wi-Fi
  association symbols.
- The acquisition constants and validity transitions match
  `IR_SPEED_LOCAL_1_2`.
- Forced packet loss produces sequence gaps but the next packet's cumulative
  pulse count includes all wheel motion.
- Wrong/zero target MAC prevents transmission startup.
- Wrong channel produces delivery failures without changing sensor results.
- Sensor reboot changes `bootId` and restarts `completedPulses` without any
  claim of continuous distance across the reboot.
- A stationary wheel, including one held on a spoke, becomes `STOPPED` with
  numeric zero after 2.5 seconds; the first new pulse leaves `STOPPED`.

### 9.2 Field gate

Couple the IR Test Car to Toby, run Toby alone, and capture Toby's republished
IR topics. Pass requires:

- continuous received snapshots around the full railway;
- `VALID` during ordinary motion;
- a deliberate physical wheel stop, including a stop on a spoke, becomes
  `STOPPED` with numeric zero;
- deliberate sensor obscuration while the wheel is moving is recorded as the
  known no-pulse ambiguity; if it reaches `STOPPED`, that zero still grants no
  upward motor authority downstream;
- numeric zero appears only with explicit wire state `STOPPED`;
- cumulative pulse distance survives observed sequence gaps;
- no sustained saturation or unexplained pulse-count regression;
- sender tx counters and Toby receiver counters reconcile within their stated
  meanings;
- the sensor ESP emits no MQTT traffic.

Passing this gate approves the sender/transport, not motor or navigation
authority.

## 10. Do not add

- MQTT or Wi-Fi association as a shortcut to telemetry or channel discovery;
- packet acknowledgements or retransmission queues;
- per-pulse radio reports;
- inferred missed spokes;
- a radio-loss state machine that changes sensor truth;
- PWM, Hall, CTO, or locomotive state in the sensor packet;
- a numeric zero for link loss, marginal/reacquiring motion, or malformed data;
- any rule by which zero speed requests additional motor power.
