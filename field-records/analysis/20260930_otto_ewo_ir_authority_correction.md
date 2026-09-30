# Otto EWO IR-authority correction — candidate review record (2026-09-30)

Status: candidate compiled and host-tested; **not flashed to Otto or Toby**.
David and Sam authorized the architecture change after Otto's 9/29–9/30
shakedown. This record documents the implementation and its limits; it does
not authorize deployment.

## Field failure and exact cause

In the first 9/30 AUTO run, a normal stationary optical window produced raw
`INADEQUATE_CONTRAST` and changing `openAborts`. EWO's `observeIr()` treated
either the diagnostic classification or changed diagnostic counters as loss
of odometry continuity. That invalidated the IR/MM relationship and enabled
the 650-ms Hall-only fallback in `observeHall()`. One sustained North Hall
field then confirmed MM13 at 10:24:29.851, MM12 at 10:24:30.495, and MM11 at
10:24:31.142. The IR cumulative distance stayed **62,844,172 µm** for all
three confirmations; the three medians were 2066, 2067, 2065 against a
reference of 1946. This was not evidence of three physical marker passages.

## Health-versus-integrity audit

| Input/path | Candidate treatment | Why |
|---|---|---|
| `INADEQUATE_CONTRAST`, `SATURATION`, `SAMPLE_GAP`, `PRIMING`, `REACQUIRING`, `SIGNAL_STALE` optical reason | Preserve raw reason and interpreted diagnostic; no distance veto | Detector health observation is not a counter discontinuity. |
| Changed `sampleGaps`, `saturatedSamples`, `openAborts`, inferred counts | Preserve in NSR1/status/IR telemetry; no distance veto | Diagnostic counters do not alter the decoded cumulative completed-pulse value. |
| Unchanged completed pulses through a stop | Measured zero travel; no MM advance | Zero is a valid distance result. |
| Stale IR packet or dropped queue item | Temporarily not applicable; retain physical origin | A later same-frame cumulative counter can bridge the gap. |
| Changed source/boot/calibration/pitch; impossible order or counter reversal; invalid scale | Break mapped distance frame; hold MM/target | Distance cannot be compared across incompatible frames. |
| Invalid transport envelope/CRC | Ingress counts/rejects packet; no navigation judgment from its bytes | A corrupt packet never reaches NAVI as a measurement. |
| Completed pulses while PWM=0 | Record physical wheel motion; break map/IR relation, retain established MM | Wheel movement is real but not known signed route displacement. |
| Normal reversal with coherent frame | Capture latest received cumulative count as new origin; choose target ahead in new direction | Type-5 reports cumulative pulses/nominal distance with identity and ordering. |

NAVI_COHERENCE's general `IrOdometryEpoch` still encodes its earlier health
policy for older programs. EWO no longer delegates navigation applicability
to that class; it keeps its own frame identity while continuing to record the
raw diagnostic and NSR1 packet.

## Active NAVI rule

Known target + NAVI-owned median-of-five Hall support + correct direction +
applicable cumulative IR distance inside the existing interval-local ±15%
window confirms a target. Missed Magnet still requires IR passage of the
mapped target. No IR distance means **hold position and target**. The 650-ms
guard, Hall-only confirmation and degraded Hall re-anchor are removed.

Reversals do not themselves break a coherent frame, including a second
reversal before reaching the first reversed target and reversal after an
IR-dependent Missed Magnet. An actual frame reset preserves last MM/target
but requires operator redeclaration. NSR1's old type-4 `degraded` byte remains
for binary compatibility and is now a distance-holding indicator, never a
Hall-navigation mode. MQTT emits `IR_DISTANCE_HOLD` / `IR_DISTANCE_READY`
and `ir_distance_state`; raw detector diagnostics remain visible.

## Speed/dashboard contract

Prior EWO published speed from adjacent ~100-ms IR packets, creating 0/1/2
pulse quantization. The dashboard's IR tile could display it, but the main
pKPH was fed only by legacy alert/segment topics that EWO did not publish.
The candidate computes an approximately one-second measured pulse-window
speed for `telem/ir` and `telem/speed`. A fresh zero is `ir_mmps:0` with
`STOPPED`; a stale or not-yet-measurable speed is `null`/`UNAVAILABLE`.
The dashboard now ingests EWO's `telem/speed` for primary pKPH using the
existing house conversion, shows IR mm/s and IR pKPH, and reports distance
hold versus normal tracking. The dashboard remains display-only. These
changes are in the repository candidate, **not deployed to the live Pi**.

## Verification performed

- EWO sanitizer host suite: PASS, including stationary diagnostic counter
  changes, sustained Hall field over repeated 650-ms periods with zero
  confirmations, no-IR hold, single/double reversal, reversal after Missed
  Magnet, genuine reset/redeclaration, station and ESTOP tests.
- NSR1 format/decoder and EWO integration tests: PASS. The dashboard contract
  tests compile a C++ emitter using the actual firmware adapter and feed its
  moving/stopped/stale/frame-reset JSON into the server and rendered JS.
- Legacy dashboard script: PASS when invoked directly. Generic `unittest
  discover` cannot import that script because it calls `sys.exit(0)` at
  module scope; this is a test-discovery artifact, not a test failure.
- `arduino-cli compile --fqbn esp32:esp32:esp32` with the local private
  credentials include path: PASS. No credentials were copied into this
  branch. The compilation was not an upload.

## Limits and next review

The reversal origin is the latest *received* IR packet at the stopped
direction command, not an atomic wheel-counter read at that exact instant;
the accepted temporary Hall/IR association can lag by a report period.
Source-selection policy for multiple IR senders remains open. Hardware run
validation, NSR1 throughput, and live dashboard deployment remain separate
steps. The existing AUTO/station shell still uses established-position status
for operating admission; it does not automatically withdraw solely on an IR
distance hold. Whether AUTO should stop on such a hold is an operator policy
decision not silently added in this correction. Review this candidate and
its test evidence before authorizing either flash or Pi deployment.
