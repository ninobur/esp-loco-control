# Firmware — current programs and tools

## Current NAVI review candidate

**[NAVI_EWO_0_1_MM045_STOP](programs/NAVI_EWO_0_1_MM045_STOP/)**

| Item | Value |
|---|---|
| Open this sketch | [NAVI_EWO_0_1_MM045_STOP.ino](programs/NAVI_EWO_0_1_MM045_STOP/NAVI_EWO_0_1_MM045_STOP.ino) |
| Reported name | `NAVI_EWO_0_1_MM045_STOP` |
| Purpose | Cruise 90 with existing grades; bidirectional MM045 STOP, five-second applied-zero dwell, restart at 150 ms/count and repeat |
| Selected profile | Otto 9950011 |
| Evidence | Host-tested, built for ESP32 core 3.3.12; prior source independently reviewed. The 150 ms/count restart correction is compiled and actuator-tested; track calibration pending |
| Deployment | First CW trials reported on the preceding version; corrected running image not verified. Not merged or field accepted |
| Decisions, tests and rollback | [Implementation report](../docs/NAVI_MM045_STOP_IMPLEMENTATION_20261009.md) |

This was previously stored as `NAVI_EYES_WIDE_OPEN_INTEGRATED`. The old
directory now contains a signpost; the runnable program exists only under its
matching MM045 name. The rename does not change locomotive behavior.

## Diagnostic tools

These are instruments, not alternative versions of the MM045 locomotive sketch.
Their individual records determine whether a particular version has been tested.

| Instrument | Purpose | Evidence record |
|---|---|---|
| [HALL_DIAG](programs/HALL_DIAG/HALL_DIAG.ino) | Hall sensor diagnosis | Sketch header; [catalog history](HISTORY.md) |
| [IR_DIAG](programs/IR_DIAG/IR_DIAG.ino) | Wheel-sensor evidence | [IR development record](../docs/IR_DEV_REC/); catalog records Diagnostic active |
| [IR_SCOPE](programs/IR_SCOPE/README.md) | Raw optical waveforms | Instrument README and historical catalog |
| [IR_USB_BENCH](programs/IR_USB_BENCH/README.md) | USB wheel-sensor bench instrument | Instrument README; historical build/bench record |
| [IR_SCOPE_ESPNOW TX](programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/) / [RX](programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_RX/) | IR radio transport experiments | [Transport report](../docs/NAVI_IR_PULSE_EVENT_PHYSICAL_EVIDENCE_20261006.md) and catalog history; separate versions and validation states |

## Historical programs and references

- **[Complete preserved firmware catalog](HISTORY.md):** older NAVI and QUORUM
  variants, diagnostics, deployment records and evidence statuses at their
  recorded commits. An old row saying “current” does not select today's sketch.
- **[Integrated EWO history](programs/NAVI_EWO_0_1_MM045_STOP/HISTORY.md):** the
  former long README, including retired station machinery and earlier builds.
- **[Reference packages](reference/)** and **[archive](../archive/):** retained
  evidence. Historical source paths identify the corresponding Git revision.

Other program folders retain their existing names and content. This initial
cleanup renames the MM045 candidate and separates the current index from the
historical catalog; it does not promote or retire unrelated programs.

## Status and naming

**Built** means the specified profile compiles. **Reviewed** means source review
is complete. **Field accepted** requires the stated physical test evidence.
There is currently no designated production program. Preserve individual
historical acceptance claims at their recorded versions.

For new or explicitly renamed programs, folder = `.ino` basename = reported
sketch name. The [repository naming and handoff rules](../README.md#naming-and-handoff-rules)
govern this cleanup and future handoffs, superseding the older catalog's practice
of leaving a different build name behind a generic filename. Historical programs
are not implicitly renamed. Keep evidence, controlling decisions and rollback
provenance when updating this index.
