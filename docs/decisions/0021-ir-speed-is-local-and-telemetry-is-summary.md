# 0021 — IR speed is computed locally and telemetry reports summaries

Status: Accepted (2026-08-10)

## Decision

The locomotive's IR sensor is sampled and converted to speed on the same
ESP32 that will supply the local governor. MQTT is not in the measurement or
control path.

Normal telemetry is limited to the latest speed plus a validity flag at 1-2 Hz
and factual health counters in the 5 s status beat. Per-pulse margins, widths,
peaks and raw evidence are commissioning diagnostics kept behind a debug
build switch; they are not normal production traffic.

## Context

The synchronized 2026-08-09 night lap showed approximately 5,238 local IR
detections while only 1,310 PULSE lines reached the Pi. The 2026-08-10
daylight capture repeated the result at larger scale: approximately 6,574
local pulses, 1,477 published details (22% delivery), and IR telemetry silent
for 77% of Otto's powered time. Counting delivered MQTT records made a sound
sensor appear defective.

The local sensor counter and Hall reference agreed across direction and car
orientation. The radio is therefore useful for reporting and commissioning,
but neither necessary nor trustworthy as part of speed acquisition.

## Alternatives considered

- **Continue per-pulse MQTT with a larger queue.** Rejected: it preserves the
  unnecessary backlog/reconnect failure and still makes delivery look like
  measurement.
- **Make delivery reliable with acknowledgements.** Rejected for the control
  path: radio reliability cannot improve a measurement already available
  locally, and loss of radio must not remove local speed.
- **Publish no telemetry.** Rejected: low-rate speed, validity and span trends
  are operationally valuable, especially for detecting a dirty lens or mount
  drift before failure.

## Consequences

- A governor consumes a local speed snapshot, never an MQTT echo.
- Normal traffic is approximately two messages per second, not one message
  per spoke.
- Source counters remain monotonic and distinct from received-message counts.
- Span is retained as the principal long-term optical health measurement.
- Detailed evidence remains available in an explicit commissioning build.
- `IR_SPEED_LOCAL` proves this contract as a test artifact; nothing is
  promoted to QUORUM by this decision alone.

## References

- `docs/IR_DEV_REC/2026-08-09_SYNCHRONIZED_HALL_IR_LAP.md`
- `firmware/test-programs/IR_SPEED_LOCAL/README.md`
- decision 0020
