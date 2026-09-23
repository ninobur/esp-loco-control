# TX 1.5 integration and build record

Authoritative sketch:
`firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`.

TX 1.5 now runs the common completed-pulse detector on the ESP32 sampler.
It publishes a coherent 110-byte type-5 movement snapshot every 100 ms using
a single-entry overwrite queue. Count/time/health are captured together in
the sampler; serial diagnostics use a critical-section-protected copy.
There is no packet backlog interpreted as motion. Radio losses do not reset
the onboard cumulative count. The receiver remains a raw packet recorder.

Wire snapshots include 64-bit random boot id and sensor microsecond time,
observed rises, completed pulses, inferred-added/removed counters (zero),
unreliable samples, saturation, gaps, aborts, calibration id, pitch, nominal
distance, optical reason and CRC. Calibration id and distanceValidated are
zero pending field commissioning. Nominal pitch is 9652 um from the existing
calibration record, awaiting confirmation that the wheel is unchanged.
Legacy type-1/fusion counters retain observed-rise semantics.

Build passed with ESP32 core 3.3.11 / esp32:esp32:esp32:
901076 bytes flash (68%), 64288 bytes static RAM (19%). Arduino resolved the
shared relative header includes from the usual repository sketch location.

Host checks passed:
- Detector: stationary noise, completed cycles, stuck plateau, saturation, gap.
- Distance contract: unknown default range, resets, stale data, intervening
  unreliability, synthetic candidate windows (previous step).
- Wire: C++ encoder to Python decoder matches 110-byte layout and nominal
  distance; bit corruption rejected by CRC.

Tools added: `tools/ir_movement_decode.py`, `tools/test_ir_movement_wire.cpp`.
Updated `docs/IR_ACTIVE_TEST_CAR_RUNBOOK.md` with TX 1.5 and stationary-first
acceptance. No hardware flash was performed by the agent. No physical count
accuracy, distance bounds, or adjacent-marker discrimination is established.

Outstanding: flash candidate and capture stationary USB/battery/shade/sun;
known-turn and measured-travel speed sweeps; sunlight transitions; calibrated
error envelopes; decoder/transport live verification; integration with NAVI
and real route geometry; false-valid and missed/doubled-pulse assessment.
