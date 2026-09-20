# IR RX startup checks, 2026-09-20

## Observed

The car's USB boot output confirmed IR_SCOPE_ESPNOW_ACTIVE_TX_1_5,
GPIO34, 1 kHz, channel 11. Moving the wheel increased counts and produced
an approximately 1300 ADC signal span. This does not establish count accuracy.

The Pi-connected RX 1.1 kept reporting an unchanged received-frame count.
After the operator rebooted RX, serial text was garbled. Restarting only
ngr-ir-espnow-record.service restored readable output. RX then reported
zero received frames and MAC 00:00:00:00:00:00. The cause is not established;
an all-zero printed MAC is not by itself proof of a failed radio.

## Change

RX 1.2 preserves the existing receive queue, wire format, channel, baud rate,
and fusion ACK behavior. It reports and checks station mode, the ESP-IDF MAC
read, channel setting and readback, ESP-NOW initialization, and callback
registration. Invalid MAC or channel mismatch stops startup with an explicit
FATAL line. READY uses measured MAC and channel, not a requested channel or
an unchecked Arduino MAC string. The existing disconnect result is printed
but not fatal because an already-disconnected station can return false.

This is instrumentation, not a demonstrated reception fix. No car sensor,
transmitter, detector, locomotive firmware, or MQTT controller was changed.

## Verification and next test

Compiled with Arduino ESP32 core 3.3.11, esp32:esp32:esp32:
885156 bytes program and 45472 bytes global RAM. Review confirms startup
cannot reach READY after a failed checked call. Hardware execution pending.

The Pi has no esptool executable or Python module available through the
checked default environment. No firmware was flashed during this change.

Flash only the RX board with IR_SCOPE_ESPNOW_RX.ino, then reconnect it to the
Pi. Capture BOOT, RADIO, FATAL or READY output at 921600 baud. Require a valid
nonzero station MAC, actual channel 11, successful ESP-NOW and callback setup,
then fresh RX records from the car. Successful startup alone is not reception.
If a check fails, preserve its exact step and error before making further
changes. Do not substitute the RX sketch for the car's TX sketch.
