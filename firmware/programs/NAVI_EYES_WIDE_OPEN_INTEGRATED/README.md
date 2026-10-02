# NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2 — first-target Hall-onset candidate

This scoped 2026-10-02 revision replaces the first-target rule at
**`c3c925ac10c55bf3a9be1ed4a1a31d0f4af117f1` (immediate rollback point)**.
**`d0185be25ec51f9ba6d58567458a59d3f1c289ca`** remains the rollback point for
the build David identifies as currently on Otto. The task branch remains
`codex/ewo-first-target-after-declare`.
**NOT field accepted. This revision has not been flashed.** David reviews the
commit and decides whether to flash and run one CW and one CCW lap with NSR1
recording enabled. No running Pi service is changed.
Full scope and verification: [change log](../../../docs/NAVI_EWO_FIRST_TARGET_CHANGELOG_20261002.md).

## PWM-zero IR displacement (0120): governing; this candidate does not conform

Read [decision 0120](../../../docs/decisions/0120-pwm-zero-ir-displacement-retains-the-interval-and-resets-only-the-within-interval-ir-coordinate.md)
before modifying navigation behavior. It refines 0112 and partially supersedes
0116. Where the implementation description further below differs from it, 0120
governs and the difference is a finding.

When NAVI observes IR displacement while motive PWM = 0, the observation is
factual but has no known relationship to signed route travel. **NAVI loses IR
knowledge of where the locomotive is within the already-established MM
interval. It does not lose the identity of that interval.**

- **Retained:** established interval, last MM, expected adjacent target,
  operator route direction/context, Hall reference (unless independently
  invalidated). The observation cannot infer that the locomotive left the
  interval.
- **Lost:** the within-interval IR coordinate (distance-to-target). PWM-zero
  counts are never signed route distance and never advance position.
- **On powered movement:** interval known, coordinate unknown, as at startup.
  The 0118 first-target rule re-establishes the IR/MM landmark at the next
  appropriate Hall+IR target encounter. NAVI does not become undeclared.
- **Redeclaration** belongs to the operator, who uses it when they know the
  locomotive is in another interval. PWM-zero displacement alone does not
  require it. Genuine frame breaks (source, boot, calibration, scale, order)
  still hold for redeclaration under 0116.
- **Terminology:** say "interval identity" or "within-interval IR coordinate",
  never "map/IR relationship destroyed". PWM-zero movement is an observed
  event, not a navigation error.

**Audit of this candidate (`958ef9a`, 2026-10-02):**

- *Conforms:*
  - `mm_`, `target_`, `direction_`, `declared_`, `positionReliable_` and
    `activeReference_` are kept.
  - Hall at PWM = 0 is ignored (`NaviIntegratedCore.h:200`), and IR at PWM = 0
    returns before any distance ruling (`:183`).
  - A spatial cycle that depends on the lost coordinate is cancelled; the prior
    reference is kept (`:499-503`).
- *Does not conform:*
  1. `NaviIntegratedCore.h:174-180` routes PWM-zero displacement through
     `invalidateRelationship()` (`:494-505`). That sets `frameLost_`, the same
     state as a genuine source/order/scale break (`:150-168`).
  2. `NaviIntegratedCore.h:253-265`: once `relationshipReliable_` and
     `targetOriginValid_` are false, Hall cannot confirm the retained target.
     Only `declare()` (`:48-72`) restores them, so redeclaration is in effect
     required.
  3. `NaviIntegratedCore.h:312` reports `FRAME_LOST_REDECLARE` for this case.
     The dashboard `server/ngr_app_v1_11_2.py:2366-2369` then shows "IR
     DISTANCE FRAME LOST … RE-DECLARE LOCATION TO RESUME NAVIGATION".
  4. Wording that uses the terminology 0120 rejects:
     - `NAVI_EYES_WIDE_OPEN_INTEGRATED.ino:400-401` warns "map/IR relationship
       unreliable";
     - `NaviIntegratedCore.h:181-182`;
     - this README's paragraphs on PWM=0 and on frame breaks below, which group
       PWM-zero displacement with true frame loss.
  5. `tests/test_integrated_core.cpp:146-162` asserts the superseded rule ("Hall
     cannot re-anchor after PWM-zero displacement; operator redeclaration is
     needed").
  6. `NaviIntegratedCore.h:84-113`: a reversal after PWM-zero displacement finds
     no coherent coordinate and sets `frameLost_`. 0120 does not settle
     reversal while the coordinate is unknown; this is open for David and Sam.
  7. Consequence found by reading the code, not observed on track:
     - AUTO admission (`.ino:411`) checks only `declared() &&
       positionReliable()`, and both stay true. AUTO can therefore start after
       PWM-zero displacement without redeclaration.
     - NAVI can then neither confirm (item 2) nor rule Missed Magnet
       (`:476-478`), so MM would stay frozen while the train runs.
     - Conforming re-establishment removes this gap.

**Minimal conceptual change (not authorized; for review):**

- On PWM-zero displacement, stop routing through frame loss. Mark only the
  within-interval coordinate unknown, keeping MM, target, direction and Hall
  reference.
- Re-enter the existing 0118 first-target state:
  - Hall onset after the event, using the absent→present guard;
  - applicable IR;
  - positive powered travel measured from the IR count captured at the latest
    PWM-zero displacement;
  - no distance window or Missed Magnet ruling until the retained target is
    confirmed.
- Report it as a distinct `ir_distance_state` rather than
  `FRAME_LOST_REDECLARE`. The value name is for David; the MQTT field is
  unchanged.
- Leave the genuine frame-break path and the 0116 reversal arithmetic unchanged
  unless David and Sam decide the reversal question.
- Replace the test in item 5.

## Scoped declaration rule (0118, supersedes 0117)

The first target after declaration is found by target-polarity Hall onset,
with **applicable IR and positive travel, but no distance window or upper bound**. A full
median-of-five window made from actionable Hall samples acquired strictly
after the declaration must first show support absent, then a later window
must show support present. An initially supported field does not qualify:
it must drop and return. The default/reset false support value is not evidence
of absence. Pre-declaration queued samples cannot establish that onset.

**No MISSED_MAGNET is ruled for the first target, regardless of travel.**
The declaration IR position establishes only whether positive travel occurred;
it does not locate the first magnet. The unknown starting point within the
interval makes a first-target distance window or missed ruling unsound: its
unknown offset would carry into later expected positions and cascade misses.
The first confirmation establishes the physical IR origin at the observed
landmark exactly as before. The exception clears on confirmation or reversal.
Subsequent targets use the unchanged cumulative-distance window,
interval-local ±15% tolerance and missed-magnet policy. Polarity, ±70 threshold,
direction, median-of-five, leading-landmark selection and spatial reference
cycle remain unchanged. MQTT and NSR1 formats are unchanged. The `.ino` changes
are its version comment and `SKETCH_NAME`, now
`NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2` so logs identify the build.

**Accepted limit:** if the first magnet is not detected, the next target-polarity
magnet is accepted as the first, silently, one or more markers off.
**Other known limit, not fixed:** reversal before the first confirmation still
computes position from the assumed declaration origin.

Regression `tests/test_first_target_after_declare.cpp` reconstructs David's
reported Otto 2026-10-02 CCW case (`9950011_20261002_120424.log`): declaration
45, target 44 at 106 mm, then target 43 at its normal window. This is a
deterministic reconstruction, not a full-log/native-NSR replay. It also covers
first targets at 10, 300 and 500 mm, no first-target miss over extended travel,
initially supported fields, zero-travel rejection, normal second-target bounds
and miss, wrong polarity, queued old samples, redeclaration and reversal.
Normal missed-magnet fixtures start from a confirmed physical landmark. See
[decision 0118](../../../docs/decisions/0118-first-target-after-declaration-is-found-by-hall-onset-without-a-distance-window.md).

## Prior build provenance

Authorized correction of integrated candidate `cd55929`, based on governing
documentation `612b791`. The stale pre-integration `638c635` is not this build.
**This corrected candidate is not field accepted and has not been flashed.
Otto's earlier supervised diagnostic flash was authorized by the operator;
do not flash Otto or Toby from this branch pending David/Sam review.** See
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
  target-polarity direction; latest-received applicable IR ±15% target distance
  after the first Hall-onset confirmation (0118 startup exception above);
  no Hall-only target confirmation or re-anchor when IR is unavailable; Missed Magnet
  progression only with applicable IR; and the 0–100/100–200 mm spatial Hall
  reference cycle. The leading landmark is the first raw expected-sign ≥70
  sample in the five-sample population that produces a qualifying median.
  Its applicable IR context also anchors the next target-distance interval;
  the later median decision point does not replace the physical boundary.
  Wrong/opposite Hall evidence is a shrug, not a second location theory.

Judgment-time freshness is explicit: the loop passes its local processing time
to NAVI. Hall acquisition timestamps remain landmarks, not a reason to discard
healthy IR already available to NAVI. No interpolation, future-packet wait or
cross-device synchronization is introduced. A genuine source/frame/scale/order
break or PWM-zero displacement cancels a dependent spatial cycle and retains
the previous valid reference. Staleness suspends applicability; a later
same-frame cumulative snapshot can bridge the gap.

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
relationship*, not the IR instrument. A true frame loss cannot be re-anchored
by a timed Hall field; operator redeclaration restores navigation context.
The last established MM and target are held while IR distance is unavailable.

Raw health observations—including `INADEQUATE_CONTRAST`, saturation, sample
gaps, pulse aborts and reacquisition—continue through NSR1, loop status and
IR telemetry. They do not veto a coherent cumulative counter. A stationary
diagnostic with no pulse progress is provisionally STOPPED. PWM >60 with a
measured zero speed raises a diagnostic warning, not a movement judgment.
Packets with incompatible boot/source/calibration/pitch, counter reversal or
unusable ordering break the mapped frame. `IR_DISTANCE_HOLD` reports that
condition; there is no degraded Hall navigation mode.

Normal reversal keeps the frame: NAVI captures the latest received cumulative
IR count as a new directional origin, computes the signed position relative
to the last mapped MM, and seeks the marker ahead in the new direction with
ordinary Hall+IR coherence. This also covers a second reversal before the
first reversed target. Packet cadence means the observed origin may precede
the direction command slightly; field validation is still needed. If no coherent origin is available, the
position is held for operator redeclaration. Missed Magnet keeps its physical
origin and interval-local tolerance in both directions.

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
MM, plus an approximately one-second pulse-window IR speed in
`ir_valid`/`ir_mmps`. Zero measured speed is published as `0`/`STOPPED`;
stale or insufficient-window speed is `null`/`UNAVAILABLE`.
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
110-byte wire snapshot, including `opticalReason`. The legacy NSR1 type-4
`degraded` byte is retained for binary compatibility but now records
`ir_distance_holding` only; it never means Hall-only navigation. Type 1 grouped Hall records
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
- Otto's 2026-09-29 moving run exposed 100-ms pulse-rate quantization in EWO
  speed telemetry, and the dashboard's main pKPH did not ingest EWO's
  `telem/speed`. This candidate uses an approximately one-second pulse window
  and explicitly ingests EWO speed for both IR Speed and house-unit pKPH.
  These values remain display-only; NAVI uses cumulative IR distance.
- Native Hall NSR1 timestamps/serials and decision records enable offline
  reconstruction; no complete on-track NSR1 capture or end-to-end receiver
  throughput test has been performed. Fixed network addresses are inherited
  operational configuration, not a deployment recommendation.
- The reviewed build was flashed to Otto on 2026-09-29. Wi-Fi, MQTT, ESP-NOW
  initialization and the dashboard's session-orientation/interval-location
  controls were verified at rest; a later manual moving run produced valid
  type-5 IR and Hall/NAVI telemetry but exposed the speed defects above.
  Subsequent Otto 2026-09-29/30 runs exercised stations and AUTO and exposed
  false 650-ms Hall-only MM confirmations during a stopped Hall field. This
  candidate removes that path but has not yet been validated on track. Toby
  was not flashed.

## 21-item disposition audit

1. X22R detector: excluded.
2. HallObserver: excluded.
3. `hallRead` median: individual ADC results to NAVI; no upstream median.
4. `hallDecisionQ`: replaced with raw-observation queue.
5. X22R baseline/lock/settle/quiet/cadence/rearm/dwell/old-field: excluded.
6. `Navigator::judge`: excluded; NAVI judges.
7. `Navigator::traverse`: excluded; NAVI advances targets.
8. 650-ms degraded mechanism: removed from active EWO navigation by 0116.
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
