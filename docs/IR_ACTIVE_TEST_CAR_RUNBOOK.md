# IR active test-car runbook

## Current candidate: TX 1.5

Use the established sketch at
`firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino` in
`/Users/davidbrown/esp-loco-control`. Reopen that sketch in Arduino IDE.
TX 1.4 failed stationary acceptance and is superseded by candidate TX 1.5.
The older sections below describe the initial setup and TX 1.4 experiment,
not acceptance of that version.

TX 1.5 uses `firmware/common/IrMovementDetector.h` for completed-pulse
detection. Expect banner `IR_SCOPE_ESPNOW_ACTIVE_TX_1_5` and five-second
`MOVE TX_1_5` lines. Reason codes: 0 priming, 1 inadequate contrast,
2 saturation, 3 sample gap, 4 stale signal, 5 reacquiring, 6 optical tracking.
Optical tracking does not establish validated distance. `distance_valid=0`
is deliberate until calibration and pulse-error budgets are commissioned.

The 110-byte type-5 packet carries boot id, sensor microseconds, completed
count, observed rises, nominal micrometres, optical reason and cumulative
health. Nominal pitch 9652 um comes from the previous wheel calibration;
calibration id remains zero pending confirmation of the installed wheel.
Inferred corrections remain zero. Raw type-1 and old fusion packets retain
observed-rise semantics; use type 5 for completed pulses.

RX records all payloads up to 250 bytes, so no RX reflash is needed.
Decode recorded snapshots with `python3 tools/ir_movement_decode.py LOG`.
No live NAVI consumer is enabled by this firmware.

First test after flashing: leave the wheel completely stationary on USB for
two minutes. Completed counts must not rise. Record raw capture, then repeat
on battery and in direct sun. Only proceed to known-turn rolling after clean
stationary checks. Preserve zero and nonzero results; never infer stopping
from silence. Full shade/sun speed and towed tests follow the original order.

This is the active IR path for the separate ESP32 mounted on the IR test car.
It is diagnostic only: it has no motor control and no authority to identify
Mile Markers or advance route position.

## Authority split

- Hall magnets remain landmark identity and absolute route reference.
- The IR wheel sensor reports movement and distance between landmarks.
- Motor feedback is optional supporting evidence for stalls, traction loss, or
  abnormal operation.
- IR uncertainty may reduce confidence or veto a navigation advance later, but
  it must never invent a magnet identity or route position.

## Active firmware pair

- Test-car transmitter: `IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`
  - Startup banner: `IR_SCOPE_ESPNOW_ACTIVE_TX_1_4`
  - GPIO 34 analog IR input, sampled at 1 kHz.
  - 512 ms dynamic envelope refreshed every 50 ms, contrast gate, Schmitt
    thresholds, 15 ms debounce.
  - Broadcasts raw waveform batches continuously on ESP-NOW channel 11.
  - Also listens for Toby CTO packets and emits read-only Hall/IR interval
    observations when those packets are present.
- USB receiver: `IR_SCOPE_ESPNOW_RX/IR_SCOPE_ESPNOW_RX.ino`
  - Startup banner: `IR_SCOPE_ESPNOW_ACTIVE_RX_1_2`
  - Prints every received ESP-NOW frame as one durable serial line.
  - ACKs retained fusion-interval packets so moving runs survive packet loss.

The raw IR packet is always emitted; CTO/Hall reception is extra context, not a
requirement for basic pulse work.

## Flash

Use the Arduino ESP32 core and select the ordinary ESP32 board profile used for
the existing builds.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 IR_SCOPE_ESPNOW_TX
arduino-cli upload --fqbn esp32:esp32:esp32 --port <test-car-port> IR_SCOPE_ESPNOW_TX

arduino-cli compile --fqbn esp32:esp32:esp32 IR_SCOPE_ESPNOW_RX
arduino-cli upload --fqbn esp32:esp32:esp32 --port <receiver-port> IR_SCOPE_ESPNOW_RX
```

After reset, verify the serial banners exactly name the active versions above.

TX 1.4 is the sunlight-transition correction. Offline replay of the September
19 Toby run found that the former 2048 ms envelope could remain wholly above or
below the optical waveform after a rapid common-mode light change. The 512 ms
candidate recovered 30% more received transitions across the two affected
windows (45% and 19% separately), kept all stationary shade/sun phases at zero,
and matched the former detector on healthy repeated track sections. Shorter
candidates began dropping healthy pulses, so they were not promoted.

## Record

Manual one-file capture:

```sh
python3 ir_scope_serial_record.py \
  --port <receiver-port> \
  --baud 921600 \
  --out ir_active_$(date +%Y%m%d_%H%M%S).log
```

Continuous Pi capture uses `ngr-ir-espnow-record.service`, which records daily
raw logs and writes `ir_espnow_health.json`.

## First active checks

1. Stationary shade, 2 minutes: expect zero pulse growth after priming.
2. Stationary direct sun, 2 minutes: expect zero pulse growth; record contrast,
   saturation, latch, and contrast-loss counters.
3. Hand-rolled shade speed sweep: crawl, normal tow speed, fast hand spin.
4. Hand-rolled direct-sun speed sweep with the same wheel orientation.
5. Towed run behind Toby only after the stationary checks are clean.

For each phase, record phase times and lighting/orientation notes in a sibling
`*_phases.tsv` file. Pulse-error analysis should compare pulse intervals and
phase plausibility first; isolated repair is allowed only as telemetry, never
as Hall identity.
