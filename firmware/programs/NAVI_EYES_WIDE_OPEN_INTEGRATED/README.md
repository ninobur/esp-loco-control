# NAVI_EYES_WIDE_OPEN_INTEGRATED_R2

Authorized correction of integrated candidate `cd55929`, based on governing
documentation `612b791`. The stale pre-integration `638c635` is not this build.
**Not field accepted. Otto's supervised diagnostic flash was authorized by the
operator; do not flash Toby.** See
[`NAVI_EWO_REVIEW_CORRECTIONS_20260929.md`](../../../docs/NAVI_EWO_REVIEW_CORRECTIONS_20260929.md).
The Otto Wi-Fi diagnosis and supervised flash are recorded in
[`20260929_OTTO_EWO_WIFI_DIAGNOSIS_AND_FLASH.md`](../../../field-records/20260929_OTTO_EWO_WIFI_DIAGNOSIS_AND_FLASH.md).
The build requires the real, locally held repository-root `credentials.h` on
the compiler include path; credentials are not part of this candidate.

## Authority and evidence paths

- Hall task: one `analogRead(33)` conversion becomes one `HallSample` with raw
  value, serial, 64-bit local microsecond timestamp, PWM, and route direction.
  The task puts every sample into the observation queue for `NaviIntegratedCore`.
  Queue loss is counted and reported to NAVI. The old five-conversion batch is
  retained only as a legacy NSR1 wire record; its median field is `INT16_MIN`.
- ESP-NOW callback: queues the received IR bytes, source MAC, receive timestamp,
  applied/commanded PWM and a coherent published-context snapshot. Ingress verifies the type-5 wire envelope, physical counter
  consistency, and CRC; every valid snapshot is delivered to NAVI. It does not
  infer movement or filter by the paired MAC. NAVI owns continuity, health,
  recency, applicability, and distance judgment. Invalid wire packets and queue
  loss are counted separately.
- NAVI: owns a provisional single-sample boot Hall reference (0115); rolling median-of-five in the known
  target-polarity direction; latest-received applicable IR ±15% target distance;
  650-ms degraded target confirmation when IR is unavailable; Missed Magnet
  progression only with applicable IR; and the 0–100/100–200 mm spatial Hall
  reference cycle. The leading landmark is the first raw expected-sign ≥70
  sample in the five-sample population that produces a qualifying median.
  Its applicable IR context also anchors the next target-distance interval;
  the later median decision point does not replace the physical boundary.
  Wrong/opposite Hall evidence is a shrug, not a second location theory.

Judgment-time freshness is explicit: the loop passes its local processing time
to NAVI. Hall acquisition timestamps remain landmarks, not a reason to discard
healthy IR already available to NAVI. No interpolation, future-packet wait or
cross-device synchronization is introduced. A source/frame/continuity break,
staleness encountered during judgment, or PWM-zero displacement cancels a
dependent spatial cycle and retains the previous valid reference.

At boot the first nonzero-PWM native Hall ADC establishes a provisional
reference immediately. No IR movement, five-position collection, boot median,
startup window, retry, or permanent boot-readiness gate is used. The normal
leading-boundary 0–100 / 100–200 mm spatial cycle replaces that provisional
reference at 200 mm. Telemetry identifies the reference as provisional until
that replacement.

After a Missed Magnet, NAVI advances only the mapped target. The last physical
field-boundary origin is retained; mapped intervals accumulate from it, while
the ±15% coherence tolerance is applied only to the current interval. A later
confirmed target reanchors the physical origin.

At PWM=0 all observations still reach NAVI. Hall cannot alter navigation state.
Continuous IR no-change retains the existing IR/MM relationship, including
when the raw detector reason is `INADEQUATE_CONTRAST`; the raw reason remains in
NSR1 and telemetry. Measured displacement at PWM=0 invalidates the *map/IR
relationship*, not the IR instrument; a later confirmed expected MM may
re-anchor it. An actual continuity break still causes degraded IR operation.

The route adapter uses the common empirically established Hall sign convention
for Toby and Otto: mapped North is AboveReference, South is BelowReference.
Otto's legacy `HALL_POLARITY_INVERTED=true` setting is explicitly undefined
after selecting Otto's profile; it is not navigation authority.

Operational consumers receive NAVI position/events through `mm()`, `target()`,
`positionReliable()`, and `takeEvent()`. The shell retains manual motor control,
ramps/braking, direction, estop, INA219 battery protection, Wi-Fi, MQTT,
ESP-NOW, station approach/stop/dwell/departure, AUTO, telemetry, and NSR1.
Stations consume NAVI's position; they do not judge magnets or infer location.
The output-only compatibility adapter emits console `NORMAL`/`UNSET` state and
MM, plus factual consecutive-IR-measurement speed in `ir_valid`/`ir_mmps`.
It cannot alter NAVI. No controller file changes are required.
ESTOP assertions latch at callback arrival independently of queue success;
only a demonstrably newer release can clear the latch. Ordered delivery is not
assumed, and a dropped assertion still withdraws motor power in the control loop.

Runtime connectivity is explicit. Serial output reports the selected profile,
broker/sync destinations, Wi-Fi transitions with IP/RSSI, MQTT connect failures
and successes, IR radio readiness and observed-frame count. When MQTT is
connected, retained `state/connectivity` reports Wi-Fi/MQTT state, broker,
IR-radio/source activity, and NSR UDP failure count so a stale dashboard can be
distinguished from a navigation or sensor problem.

Otto's 48-entry MQTT publish queue allocated about 61 KB before Wi-Fi started.
With the recorder and other queues present, Wi-Fi repeatedly timed out during
authentication. Starting Wi-Fi first connected but then the queue allocation
failed. A 16-entry queue saved about 40 KB; Wi-Fi, MQTT, and ESP-NOW then came
up. The diagnostic build had `pub_drop=0` at an 88-second check, but the
reviewed build reported five startup publish drops, unchanged in three checks
at 64–66 seconds. The boot log reports queue allocation, free heap, and largest
free block. The reduced outage buffer still needs sustained-load and reconnect
testing before field acceptance.

## Record and test

NSR1 version 1 types 1–5 remain readable by the updated tools. R2 emits **version
2 for type 4 only** (`NAVI`, 63-byte snapshot: original 51 bytes plus 64-bit
consumption ID and 32-bit IR sequence). Type 5 is unchanged (`HALL_NATIVE`,
16 bytes per ADC observation, 48 observations per datagram). IR records retain the full raw
110-byte wire snapshot, including `opticalReason`. Type 1 grouped Hall records
are for older tooling only and carry no acquisition median. The receiver keeps
every datagram in the capture file, including malformed ones; the updated
decoder can export native Hall CSV and NAVI JSONL:

```
python3 tools/navi_sync_decode.py capture.nsr \
  --native-hall-csv native.csv --ir-jsonl ir.jsonl --navi-jsonl navi.jsonl
```

New version-1 record types:

- **6 CONSUMPTION:** 32 × 36-byte items. Each contains NAVI's input ID, decision
  time, source observation time/serial, input kind, PWM fields and pre-consumption
  context. Kinds: 1 Hall, 2 IR, 3 declaration, 4 reversal, 5 loss notification.
  Hall `direction` retains acquisition's 1/2 encoding; other kinds use route ±1.
  Hall commanded PWM is processing context, since native Hall carries applied
  PWM only. IR PWM fields and its type-2 context come from reception.
- **7 ACTION:** 175-byte snapshot. Kinds: 1 command received, 2 command consumed,
  3 requested PWM/ramp, 4 applied PWM, 5 station order. Command receipt has no NAVI
  consumption ID. Other actions carry the last consumed NAVI ID and command ID
  (state correlation, not a claim that every action was caused by that command).
  Topic/payload hold the bounded queued command, or station name/event.

Use `--consumption-jsonl consumed.jsonl --action-jsonl actions.jsonl` to export.
Join by locomotive boot/session, consumption ID and command arrival order;
join native Hall via serial and IR via sequence/receive time. NAVI events now
carry actual decision time. Receiver stream identity includes locomotive,
boot, session and record type. Recorder-drop deltas do not prove that *all*
missing datagrams had one origin; mixed/unreported losses remain explicit.

New trace storage is a checked, fixed 16,416-byte allocation: eight consumption
batches plus a current batch and 24 action records. Overflow counts are visible
in wire headers and `state/trace`; acquisition queue drops remain distinct.
Final partial batches can be lost on power removal. Full-rate consumption
recording adds roughly 180 KB/s at 5,000 Hall observations/s; test sustained
throughput, task latency and heap/stack margins before relying on captures.
Deploy matching receiver/decoder tools separately before hardware evaluation.
**No exact/lossless replay claim is made.**

Run `sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh` for
sanitized host tests and the original EWO regression suite. Compile, without
upload, with the private repository-root credentials directory on the include
path: `arduino-cli compile --fqbn esp32:esp32:esp32 --build-path <build-dir>
--build-property compiler.cpp.extra_flags=-I<repo-root>
firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED`. Python NSR1 tests run with
`python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration`.
The shell suite also runs the shared IR architecture and station-position tests.

## Review limits

- The stored `ir_pair` MAC is telemetry provenance only; source selection is
  not implemented. A second valid IR sender could disrupt continuity. David
  and Sam need to decide a NAVI-owned source policy before field use. No IR
  observation is hidden from NAVI by this candidate.
- Finite Hall/IR/event/UDP/MQTT queues expose loss counters but are not proven
  lossless under hardware load or Wi-Fi outage. Boot storage is now bounded,
  but its spatial assumptions and the larger trace rate need hardware validation.
- Native Hall NSR1 timestamps/serials and decision records enable offline
  reconstruction; no on-track capture or end-to-end receiver throughput test
  has been performed. Fixed network addresses are inherited operational
  configuration, not a deployment recommendation.
- The reviewed build was flashed to Otto on 2026-09-29. Wi-Fi, MQTT, ESP-NOW
  initialization and the dashboard's session-orientation/interval-location
  controls were verified at rest. No movement, valid IR reception, station, or
  AUTO behavior was verified on Otto; none of this build was tested on Toby.

## 21-item disposition audit

1. X22R detector: excluded.
2. HallObserver: excluded.
3. `hallRead` median: individual ADC results to NAVI; no upstream median.
4. `hallDecisionQ`: replaced with raw-observation queue.
5. X22R baseline/lock/settle/quiet/cadence/rearm/dwell/old-field: excluded.
6. `Navigator::judge`: excluded; NAVI judges.
7. `Navigator::traverse`: excluded; NAVI advances targets.
8. 650-ms degraded mechanism: inside NAVI, marked degraded; Hall backlog cannot manufacture it.
9. `Navigator::acceptThrough`: excluded.
10. ProximalRecovery: excluded.
11. Sequence correction/alternative-position search: excluded.
12. Missed Magnet reporting: inside NAVI; 10-MM AUTO limit excluded.
13. `compareMovement`: excluded.
14. `IrHealthMonitor::acceptedMm`: excluded; observed Hall boundary is NAVI-owned.
15. `MovementSource::at`: excluded; NAVI uses latest received applicable IR at judgment time.
16. Legacy detector telemetry: EWO evidence/state/events and correlated NSR1; output-only dashboard adapter.
17. `hallReady` or equivalent upstream readiness flag: excluded.
18. PWM-zero observation suppression: excluded; NAVI still receives both streams.
19. Legacy `NavState::Lost`: excluded; factual losses are reported.
20. Stations: consume NAVI position; start-within-zone behavior tested.
21. IR health: raw wire/diagnostic delivered; applicability and spatial-frame invalidation decided inside NAVI.

Historical files are not deleted, but none of the excluded navigation and
detector mechanisms is an active dependency of this sketch.
