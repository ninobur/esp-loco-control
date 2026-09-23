# IR_SPEED_LOCAL

Lean local-speed prototype for the QRE1113 wheel sensor. It is a separate test
sketch, not QUORUM and not production-approved.

## Purpose

Prove the minimum sensor-to-governor contract before integration:

- speed is calculated locally on the ESP32 that sampled the wheel;
- the consumer receives a snapshot containing speed and validity;
- MQTT reports the latest snapshot at 1 Hz and health at 5 s;
- no per-pulse MQTT stream exists, so reconnect cannot create a pulse backlog;
- commissioning detail is serial-only behind `IR_LOCAL_DEBUG`.

Calibration is the operator's three-run rolling measurement on 2026-08-10:
10 revolutions measured 38 in, 37 7/8 in, and 38 1/8 in. The accepted value
is 3.8 in/revolution = 96.52 mm; the installed wheel has 10 spokes, giving
9.652 mm per completed pulse.

## Detector

The detector contains only behavior supported by evidence or an overwhelming
case under decision 0020:

1. One real ADC acquisition per nominal 1 ms slot. Late/missed slots are
   counted; no catch-up samples are fabricated.
2. Rolling 5th/95th-percentile envelope, required because the daylight run
   demonstrated large baseline/span changes between sun, shade and orientation.
   Detection continues at spans of 120-299 counts so the envelope can recover,
   but the observed stationary-noise band proves speed must remain `MARGINAL`.
3. Rising threshold at 2/3 span and falling threshold at 1/3 span. Hysteresis
   is the basic analog-to-pulse conversion, not a post-hoc rejection filter.
4. Every measured nonzero completed-pulse interval enters a five-slot median.
   No interval is admitted or rejected by comparison with the filter.
5. Valid speed requires five intervals, usable contrast, and a completed pulse
   within 2.5 s. Invalid never means stopped.
6. An open pulse lasting 2.5 s is aborted and counted. It cannot rearm until
   the signal physically falls below `thrLow`, preventing one plateau from
   manufacturing repeated rises.

There is deliberately no inferred missed-spoke counter, debounce guard,
revolution summing, persistence, adaptive recovery rate, raw dump, per-spoke
phase model, or radio-dependent state transition.

## Local contract

`LocalSpeedSnapshot` is the intended integration seam. The sampler updates it
under a critical section. A future local governor reads it without MQTT.

Validity states:

- `VALID`: five current intervals and usable contrast;
- `REACQUIRING`: pulses are completing but the five-interval window is not full;
- `MARGINAL`: detection continues at span 120-299, but speed is suppressed;
- `INVALID_CONTRAST`: the rolling optical span is below 120 counts or unprimed;
- `STALE`: contrast exists but no completed pulse has arrived for 2.5 s.

Only `VALID` carries a consumable speed. The test sketch has no motor output.

## Telemetry

The node identity is retained and the compact telemetry contract is:

- `ngr/spoke/IR_SPEED_SENSOR/telem/speed` — latest speed snapshot, 1 Hz;
- `ngr/spoke/IR_SPEED_SENSOR/telem/status` — factual health counters, 5 s;
- `ngr/spoke/IR_SPEED_SENSOR/status/online` — retained MQTT LWT state.

The new topic deliberately does not reuse IR_TEST's incompatible per-pulse
schema. Every payload carries `"schema":"ir-speed-local/1"`. Consumers must
use `seq` as the local completed-pulse counter and must not count received
messages as wheel events. `report_ms` advances every publish even when the
source event fields remain frozen during `STALE`.

Speed payload when valid:

```json
{"schema":"ir-speed-local/1","seq":1234,"t_ms":45678,"report_ms":45700,"speed_valid":1,"speed_mmps":241.30,"span":811}
```

Invalid payload:

```json
{"schema":"ir-speed-local/1","seq":1234,"t_ms":48178,"report_ms":49178,"speed_valid":0,"speed_mmps":null,"state":"STALE","span":811}
```

The 5 s status beat reports distinct literal counters: rises, completed
pulses, missed sampling slots, open-pulse aborts, contrast-invalid episodes,
saturated ADC samples, maximum sampling gap, MQTT attempts/connects, publish
failures, span and RSSI.
There is no outbound queue and therefore no publish-queue drop counter.

## Build switches

- `IR_LOCAL_WIFI` defined: 1 Hz/5 s MQTT plus serial boot line.
- `IR_LOCAL_WIFI` commented: sensor and local computation only.
- `IR_LOCAL_DEBUG` defined: one serial line per completed pulse. It never adds
  per-pulse MQTT traffic.

Copy `firmware/config/credentials_template.h` to the git-ignored
`firmware/config/credentials.h` before building the Wi-Fi variant.

## Daylight field gate

Run a durable Pi dual capture with Otto Hall telemetry and the IR topics.
Exercise direct sun, shade, direct sun again, then reverse the car orientation
and traverse the route in reverse.

Pass requires:

1. no saturation and no sustained `INVALID_CONTRAST` while Otto is moving;
   with the car stationary and the sensor blinded or decoupled, no
   `speed_valid:1` may be published for the duration;
2. completed-pulse/Hall-marker ratio agrees between outbound and return within
   3%, unless differing route endpoints account for the difference;
3. speed remains `VALID` during uninterrupted motion and becomes non-valid
   rather than publishing stale or zero speed during edge silence;
4. `sample_missed`, `open_aborts`, and `contrast_invalid` are interpreted as
   literal events, with any nonzero moving occurrence inspected before a
   remedy is proposed;
5. MQTT interruptions do not change local counters or local speed computation;
6. received message count is never used as the physical pulse count.

Passing this gate validates the prototype only. Integration into QUORUM is a
separate scoped implementation and review.
