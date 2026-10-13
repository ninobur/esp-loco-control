# NAVI_COHERENCE_0_6_IR_SYNC_TEST — experimental variant

This is an **observation-only modification** of the uploaded `NAVI_COHERENCE_0_6_IR_HEALTH(1).ino` (POSITION_STATIONS_R1/20Q3/NSR1). It is not NAVI_EWO_0_4.

## What changed

- Changed sketch build identity to `NAVI_COHERENCE_0_6_IR_SYNC_TEST`.
- The ESP-NOW receiver now queues both original 110-byte Type-5 cumulative reports and 61-byte Type-6 completed-pulse events. Type-6 packet layout and CRC follow `PulseEventEvidence.h` from the repository's IR_SCOPE_ESPNOW transmitter 1.8.
- Type-5 continues through the **original** movement, health, recorder, and NAVI judgment paths unchanged.
- Type-6 is CRC/structure checked, ordered by boot, sequence and completed count, and retained in a separate diagnostic snapshot. Gaps are counted; the first post-gap event is not treated as continuous.
- At each Hall detection the Hall task snapshots the last received Type-5 and Type-6 coordinates under a short critical section.
- `[IR_SYNC]` serial lines compare pulse counts and receiver-arrival ages for the two channels. Type-6 is marked usable for comparison only with continuous events and a matching Type-5 boot (when Type-5 exists). No new geographic authority is granted.

## Requirements before building / flashing

This file is **not standalone**: the original sketch includes `LocoConfig.h`, `Navigator.h`, `RouteMap.h`, `Ops.h`, `Stations.h`, `HallObserver.h`, `RecoveryControl.h`, `IrHealthMonitor.h`, `NaviSyncRecorder.h`, QUORUM credentials and libraries. Place the `.ino` in its own matching Arduino sketch directory with those dependencies resolved from the *same historical firmware revision*. Do not combine incompatible branch versions.

The IR car must actually transmit **Type-6** packets; the September 28 transmitter did not. A transmitter implementing `PulseEventEvidence.h` Type-6 (such as the later 1.7/1.8 candidate) is necessary for dual-channel comparison. Confirm transmitter compatibility and hardware test status before operating.

**Not compiled or field tested.** No local copy of the complete historical include tree or hardware is available in the uploaded inputs. This is a source-level experimental candidate for Codex review and ESP32 3.3.12 compilation before use.

## Interpretation limits

Receiver-arrival age is **not** optical-capture age. Independent ESP32 clocks are not synchronized; a Type-6 event that arrives later than the Hall opening cannot be retroactively used as causal evidence. Type-6 may be missing, delayed, or discontinuous. A fresher packet does not prove that every physical pulse was counted. Serial diagnostics can themselves affect execution timing and should be measured during field validation.

No navigation, motor-control, station-control, or transmitter code was intentionally changed.
