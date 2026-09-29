# NAVI_EYES_WIDE_OPEN_INTEGRATED_R1

Integration candidate based on EWO at `638c635` and the operational shell of
`NAVI_COHERENCE_0_6_IR_HEALTH`. **Not field accepted. Do not flash Toby or Otto.**
The build requires the real, locally held `firmware/programs/QUORUM/credentials.h`;
credentials are not part of this candidate.

## Authority and evidence paths

- Hall task: one `analogRead(33)` conversion becomes one `HallSample` with raw
  value, serial, 64-bit local microsecond timestamp, PWM, and route direction.
  The task puts every sample into the observation queue for `NaviIntegratedCore`.
  Queue loss is counted and reported to NAVI. The old five-conversion batch is
  retained only as a legacy NSR1 wire record; its median field is `INT16_MIN`.
- ESP-NOW callback: queues the received IR bytes, source MAC, receive timestamp,
  and PWM. Ingress verifies the type-5 wire envelope, physical counter
  consistency, and CRC; every valid snapshot is delivered to NAVI. It does not
  infer movement or filter by the paired MAC. NAVI owns continuity, health,
  recency, applicability, and distance judgment. Invalid wire packets and queue
  loss are counted separately.
- NAVI: owns the 0–10 mm boot Hall median; rolling median-of-five in the known
  target-polarity direction; latest-received applicable IR ±15% target distance;
  650-ms degraded target confirmation when IR is unavailable; Missed Magnet
  progression only with applicable IR; and the 0–100/100–200 mm spatial Hall
  reference cycle. The leading landmark is the first raw expected-sign ≥70
  sample in the five-sample population that produces a qualifying median.
  Its applicable IR context also anchors the next target-distance interval;
  the later median decision point does not replace the physical boundary.
  Wrong/opposite Hall evidence is a shrug, not a second location theory.

At PWM=0 all observations still reach NAVI. Hall cannot alter navigation state.
Continuous IR no-change retains the existing IR/MM relationship, including
when the raw detector reason is `INADEQUATE_CONTRAST`; the raw reason remains in
NSR1 and telemetry. Measured displacement at PWM=0 invalidates the *map/IR
relationship*, not the IR instrument; a later confirmed expected MM may
re-anchor it. An actual continuity break still causes degraded IR operation.

The route adapter uses the common empirically established Hall sign convention
for Toby and Otto: mapped North is AboveReference, South is BelowReference.
Otto's legacy `HALL_POLARITY_INVERTED=true` setting was historically unused;
this candidate deliberately does not activate it. The legacy profile is not
rewritten here.

Operational consumers receive NAVI position/events through `mm()`, `target()`,
`positionReliable()`, and `takeEvent()`. The shell retains manual motor control,
ramps/braking, direction, estop, INA219 battery protection, Wi-Fi, MQTT,
ESP-NOW, station approach/stop/dwell/departure, AUTO, telemetry, and NSR1.
Stations consume NAVI's position; they do not judge magnets or infer location.

## Record and test

NSR1 version 1 types 1–3 remain readable. This candidate adds type 4 (`NAVI`,
51-byte decision snapshot) and type 5 (`HALL_NATIVE`, 16 bytes per individual ADC
observation, 48 observations per datagram). IR records retain the full raw
110-byte wire snapshot, including `opticalReason`. Type 1 grouped Hall records
are for older tooling only and carry no acquisition median. The receiver keeps
every datagram in the capture file, including malformed ones; the updated
decoder can export native Hall CSV and NAVI JSONL:

```
python3 tools/navi_sync_decode.py capture.nsr \
  --native-hall-csv native.csv --ir-jsonl ir.jsonl --navi-jsonl navi.jsonl
```

Run `sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh` for
sanitized host tests and the original EWO regression suite. Compile, without
upload, with `arduino-cli compile --fqbn esp32:esp32:esp32
firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED` after supplying local
credentials. Python NSR1 tests run with
`python3 -m unittest tools.tests.test_navi_sync_format`.

## Review limits

- The stored `ir_pair` MAC is telemetry provenance only; source selection is
  not implemented. A second valid IR sender could disrupt continuity. David
  and Sam need to decide a NAVI-owned source policy before field use. No IR
  observation is hidden from NAVI by this candidate.
- Finite Hall/IR/event/UDP/MQTT queues expose loss counters but are not proven
  lossless under hardware load or Wi-Fi outage. The 0–10 mm exact boot median
  stores its Hall population in memory until closure; a stall after motion
  begins may grow that population. Both need hardware-load validation.
- Native Hall NSR1 timestamps/serials and decision records enable offline
  reconstruction; no on-track capture or end-to-end receiver throughput test
  has been performed. Fixed network addresses are inherited operational
  configuration, not a deployment recommendation.
- The sketch has not been flashed or field-tested. Compile and host tests do
  not prove motor, IR radio, station, or AUTO behavior on Toby or Otto.

## 21-item disposition audit

1. X22R detector: excluded.
2. HallObserver: excluded.
3. `hallRead` median: individual ADC results to NAVI; no upstream median.
4. `hallDecisionQ`: replaced with raw-observation queue.
5. X22R baseline/lock/settle/quiet/cadence/rearm/dwell/old-field: excluded.
6. `Navigator::judge`: excluded; NAVI judges.
7. `Navigator::traverse`: excluded; NAVI advances targets.
8. 650-ms degraded mechanism: inside NAVI, marked degraded.
9. `Navigator::acceptThrough`: excluded.
10. ProximalRecovery: excluded.
11. Sequence correction/alternative-position search: excluded.
12. Missed Magnet reporting: inside NAVI; 10-MM AUTO limit excluded.
13. `compareMovement`: excluded.
14. `IrHealthMonitor::acceptedMm`: excluded; observed Hall boundary is NAVI-owned.
15. `MovementSource::at`: excluded; NAVI uses latest received IR fact.
16. Legacy detector telemetry: replaced with EWO evidence/state/events and NSR1.
17. `hallReady` or equivalent upstream readiness flag: excluded.
18. PWM-zero observation suppression: excluded; NAVI still receives both streams.
19. Legacy `NavState::Lost`: excluded; factual losses are reported.
20. Stations: consume NAVI position; start-within-zone behavior tested.
21. IR health: raw wire/diagnostic delivered; applicability decided inside NAVI.

Historical files are not deleted, but none of the excluded navigation and
detector mechanisms is an active dependency of this sketch.
