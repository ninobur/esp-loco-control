# NAVI IR Pulse-Event Physical Evidence Architecture — 2026-10-06

Original 1.7 status at commit `65210f3`: implemented, source-reviewed and
compiled diagnostic experiment; then not flashed or field accepted. David authorized review, direct defect fixes,
repository documentation, tests, commit and push. No NAVI/control implementation
or deployment is authorized by this change.

This applies the [NGR design principles](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md):
put judgment with NAVI, preserve direct physical observations, distinguish
measurement from interpretation, and make degradation visible. No departure is
intended. The experiment asks:

**Are native completed-pulse intervals sufficiently regular that NAVI can use
pulse-level physical speed evidence without averaging?**

## Scope, authority and provenance

Hall anchors geographic identity/location; IR measures continuous physical
movement; NAVI interprets both and controls the locomotive. This change improves
the immediate physical evidence supplied by IR. It does not implement
[Document C](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md) or alter
[decision 0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md).

Starting branch: `codex/ewo-pwm-zero-localization`.
Starting HEAD: `e1ec763d20f6ec8ca84707afd2116a597cea981a`, also verified as the
remote branch HEAD before editing. Firmware/test baseline supplied at handoff:
`0116518b6d5d429fce3281505611d4fa17e2a350`.
The matching checkout had four unrelated, untracked NAVI control/test files;
none was edited, staged or used as implementation authority. The older primary
checkout on `claude/navi-ps-r1-20q-standards` was not used for this change.

Candidate: David's `IR_SCOPE_ESPNOW_PULSE_EVENT_TX_1_7_TEST.ino`, independently
inspected against repository source and the supplied specification in this
single-agent task. This is not a claim of a separate second-agent review.
The reviewed sketch occupies the existing canonical path:

`firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`

Its adjacent `PulseEventEvidence.h` contains the experimental packet and event
capture helper so deterministic tests exercise the actual implementation.
The prior transmitter remains recoverable from the starting commit. Reverting
this scoped commit restores repository behavior; no hardware rollback is needed
because nothing was flashed. This does not change the older local Arduino copy.

No PWM, throttle, five-beat homeostasis, glide path, station control, adaptive
learning, locomotive model, consist calibration, NAVI estimator, common detector,
MQTT integration, RX firmware or QUORUM firmware changes are included.

## Why expose physical events

The existing transmitter samples GPIO34 at approximately 1 kHz. Its Type-5 v1
movement snapshot is authored every 100,000 microseconds and contains cumulative
completed pulses. That reporting period is a scheduling choice, not a hardware
limit. Successive reports can contain zero, one, two or more new pulses.

`NAVI_EYES_WIDE_OPEN_INTEGRATED/NaviIntegratedCore.h::addSpeedPoint` explicitly
uses a 900,000–1,500,000-us history window to avoid the “0/1/2-pulse, 100-ms speed
steps.” This was reasonable compensation for periodic reporting. It does not
establish that the physical optical train needs averaging.

Installed pitch is **9,652 µm = 9.652 mm per completed pulse**, as recorded in
`firmware/common/IrMovementWire.h::kInstalledPitchUm`. The existing NGR house
conversion is **1 pKPH = 5.37325 mm/s**, verified in `IR_DIAG.ino`, the integrated
NAVI speed conversion and [decision 0099](decisions/0099-canonical-house-pkph.md).
This is the established house unit, not an independently inferred scale ratio.

| Physical objective | mm/s | Completed events/s | Nominal interval |
|---|---:|---:|---:|
| 20 pKPH | 107.465 | 11.134 | 89.815 ms |
| 45 pKPH | 241.79625 | 25.051 | 39.918 ms |

One second at cruise spans about 242 mm and 25 completed-pulse intervals.
That delay can matter to future physical speed and glide-path control.
Oscilloscope observations suggest regular steady-motion modulation; only
recorded numerical evidence can establish how regular it actually is.

## Event definition and distinct clocks

The unchanged `IrMovementDetector.h` is authoritative. On an open pulse's
return below the low threshold, it closes the pulse, increments `completed`,
sets `lastCompleted_ = us`, and sets `fall = true`. The experiment observes
that exact `fall` immediately after every `Measurement::sample` call. There is
no additional edge detector or debounce and no inferred or repaired pulse.

The evidence chain is:

```text
approximately 1-kHz ADC sample
→ existing detector completion (fall and completed increment)
→ capture that sample timestamp and cumulative count
→ zero-wait FIFO admission
→ radio task submission and send callback
→ receiver arrival and later NAVI interpretation
```

`completedUs` is `Snapshot::capturedUs` on the completion sample, the same `us`
passed to the detector and assigned to its private `lastCompleted_`. As in the
existing sampler, the timestamp is acquired just before `analogRead`. It is a
sample timestamp with roughly 1-ms sampling resolution, not a hardware edge
interrupt measurement. The microsecond unit does not claim microsecond physical
accuracy. ADC conversion/scheduling uncertainty remains for field measurement.

Packet construction, queue wait, ESP-NOW submission, send callback and receiver
arrival occur later. None substitutes for `completedUs`. Transmitter uptime is
not synchronized with receiver uptime. Boot identity separates time domains.
The 100-ms Type-5 `capturedUs` retains its existing snapshot semantics.

## Interval and distance semantics

For each completion, preserve the authoritative cumulative completed-pulse
number and `nominalUm = completedPulses * 9652`. These are nominal unsigned wheel
distance, not direction, geography, calibrated error bounds or operating authority.
Type-5's existing calibration ID zero and distance-validation behavior remain.

`intervalUs` is the native difference from the immediately preceding usable
completion. Zero means **unavailable**, never measured zero speed. The first
completion after boot has interval zero. Predecessor timing is also cleared
when the detector reports a sample gap, saturation, open-abort/continuity loss,
or a reason other than TRACKING or SIGNAL_STALE. Every completion is still
emitted, including one with inadequate optical quality. A recovered completion
establishes the next predecessor only if its optical state is usable.

Quiet SIGNAL_STALE retention alone does not impose a new timeout or discard a
known timestamp. A completion after a retained stop can therefore carry a long
interval including the stopped time. Preserve it and identify stop/start context
in analysis; do not interpret it as instantaneous restart velocity. No pulse is
invented while stationary. A dropped radio packet does not erase the producer's
preceding physical completion: the next packet's `intervalUs` still spans one
physical interval, not the time since the last successfully received packet.

Diagnostic arithmetic, only when `intervalUs > 0`:

```text
speed_mm_s = 9652 * 1000 / intervalUs
pKPH       = speed_mm_s / 5.37325
```

The serial record prints `pkph=nan` for an unavailable interval. No rolling mean,
median, low-pass filter, exponential smoothing, one-second averaging, or new
pulse rejection is added. Existing optical envelope/detector processing is
unchanged. Whether any future averaging is justified is David's later decision
based on field evidence.

## Wire contract and compatibility

The additive packet is **IR-family magic 0x4952, Type 6, version 1**.
Searches of firmware, common headers, tools and documentation found existing
family Types 1–5 and no Type-6 assignment. The separate `IRSpeedWire.h` magic
`0xC6` is a different protocol and is not consumed by this assignment.

ESP32 little-endian, packed, 61 bytes; no pointers, floats or native `bool` fields:

| Offset | Bytes | Field | Meaning |
|---:|---:|---|---|
| 0 | 2 | magic | 0x4952 (wire bytes 52 49) |
| 2 | 1 | version | 1 |
| 3 | 1 | type | 6 |
| 4 | 4 | sid | Existing random transmitter session ID |
| 8 | 4 | sequence | One increment per detector completion, before queue admission |
| 12 | 8 | bootId | Same 64-bit movement boot identity as Type-5 |
| 20 | 8 | completedPulses | Authoritative cumulative completions |
| 28 | 8 | completedUs | Physical completion sample, transmitter-local microseconds |
| 36 | 8 | intervalUs | Native predecessor interval; zero unavailable |
| 44 | 8 | nominalUm | completedPulses × 9652 |
| 52 | 4 | pitchUm | 9652 |
| 56 | 1 | opticalReason | Existing detector reason enum |
| 57 | 2 | span | Current completion sample's detector high − low |
| 59 | 2 | crc | CRC-16/CCITT-FALSE over bytes 0–58 |

CRC uses initial 0xffff, polynomial 0x1021, no reflection or final XOR; check
value for `123456789` is 0x29b1. It is filled after all fields and excludes only
its own two bytes. Assertions fix size, timestamp and CRC offsets and the
250-byte transport ceiling. Python `struct` equivalent: `<HBBIIQQQQQIBHH`.
Reason values: 0 PRIMING, 1 INADEQUATE_CONTRAST, 2 SATURATION, 3 SAMPLE_GAP,
4 SIGNAL_STALE, 5 REACQUIRING, 6 TRACKING.
Sequence/cumulative diagnostic counters are finite unsigned integers; interpret
32-bit sequence rollover modulo 2^32 within the same boot, and never join boots.

Type-5 remains the same 110-byte v1 packet at the existing approximately 100-ms
production cadence and retains its latest-value queue. The shared headers are
unchanged. Current NAVI still uses its existing historical speed estimator.
This enables later side-by-side comparison without prematurely changing control.

The existing scope RX forwards arbitrary payloads of up to 250 bytes as hex,
so it can record Type 6 without a reflash. Its displayed CRC is over the entire
received payload, whereas the embedded Type-6 CRC excludes the last two bytes.
Existing Type-5-only decoders do not decode Type 6. Current integrated NAVI
rejects the 61-byte packet by length and may increment invalid-frame counters;
those counts should be expected, not mistaken for corrupt Type-5 packets.
Added traffic also consumes shared receiver/airtime capacity, to be checked in
the field. No claim of zero scheduling impact or successful receiver delivery
follows from wire compatibility.

## Queue, radio and loss behavior

- The sampler copies each event into a **32-entry FIFO**, with zero wait.
  At 45 pKPH this is about 1.28 seconds of completions; at 20 pKPH, 2.87 seconds.
  A full queue rejects the new event, preserves queued order and increments
  `drop`. Sequence and completed count advance even on rejection. No event
  queue overwrite or replacement by a later snapshot is used.
- A separate **64-entry serial FIFO** receives every event and its radio queue
  admission result, also with zero wait. It is drained in `loop`, up to eight
  records per pass. A full serial FIFO increments `logdrop`; lost serial rows
  are visible through sequence gaps and the continuing summary counter.
- Only the radio task invokes `radioSend`. It services one pulse first per
  pass, then Type-5 and the existing fusion/observation/raw scheduling. This
  avoids the candidate's unconditional pulse `continue` starving the comparison
  stream under sustained pulse backlog. Fusion ACK, retention and retry rules
  remain materially unchanged.
- The former up-to-20-ms idle wait on the raw queue is removed. A one-tick yield
  at the end of an idle/ordinary pass is task scheduling, not an event production
  period. In-flight sends and other traffic can still delay a pulse. There is
  no hard latency guarantee; `lag_max_us` measures the lifetime maximum from
  physical completion to the radio service attempt (saturating at UINT32_MAX).
- A dequeued pulse has one send attempt, no application ACK or retry. `sent`
  counts timely ESP-NOW success callbacks. `fail` counts unconfirmed attempts,
  including immediate errors, callback failures, timeouts and a still-busy radio.
  Neither success nor failure conclusively establishes receiver delivery for a
  broadcast; check receiver sequence/count/CRC evidence.
- After an accepted send times out at 100 ms, a new send is not submitted until
  its callback arrives. This prevents a late callback being credited to a later
  event. Busy attempts fail visibly. A callback that never arrives leaves sends
  inhibited until restart; it cannot silently fabricate successes. `timeout`
  and `busy_drop` are radio-wide counts, while `fail` is pulse-specific.
- The new counters and callback handoff use atomics. No radio wait or serial
  printing occurs in the sampler. Queue operations and CRC work are bounded,
  but actual 1-kHz scheduling and serial throughput require hardware validation.

Once queues are drained and no attempt is in flight, `generated = sent + fail +
drop` modulo counter rollover. Values printed during activity are separate
atomic reads and need not form a transactional snapshot. `qhigh` is the highest
observed post-admission queue occupancy; a concurrent dequeue can reduce it.
No counters are persistent across reboot.

## Telemetry and initial field protocol

Capture the test-car's **115200-baud** serial output from boot for direct numeric
pulse evidence and transport health. Capture the existing **921600-baud** scope
RX stream as well when checking delivery. The existing wireless RX alone records
Type-6 packets but does not provide the transmitter's new serial counters.

Each `PULSE` row includes `seq`, `completed`, `t_us`, `dt_us`, `pkph`,
`nominal_um`, `span`, `reason`, `queued`, `sent`, `fail`, `drop`, `logdrop`, and `q`.
The per-event values describe the completion; queue/counter values describe
logging time, not necessarily the send result for that particular row.
The boot banner and one-second `PULSE_STAT` identify the movement boot.
`PULSE_STAT` also prints generated count, queue high-water observation, serial
queue depth, maximum radio lag and timeout/busy counts, even when pulses stop.
The one-second summary does not select, aggregate or replace pulse rows.
Existing raw waveform, MOVE and STAT diagnostics remain available.

After separate authorization to flash and run hardware:

1. Record commit, boot identity, direction, lighting, physical operating context
   and all unedited logs. Check the READY 1.7 identity and 61-byte Type-6 format.
2. Roll from rest. The first usable predecessor interval must be unavailable;
   subsequent rows must follow completed pulses, not 100-ms boundaries. Check
   count/sequence order and distance arithmetic. Inspect stationary periods and
   both stop/restart orientations; keep long stop-spanning intervals as evidence.
3. At approximately 20 and 45 pKPH, inspect every interval (roughly 90 and 40 ms),
   timestamps, optical span/reason and raw waveform. Targets are context, not
   acceptance bounds to force onto observations. Account for roughly 1-ms sample
   quantization. Plot/inspect native interval distributions and runs of outliers;
   descriptive statistics after capture must not replace the preserved data.
4. Change lighting and exercise ordinary start/stop transitions. Inspect faults
   between pulses, zero/unavailable intervals after recovery, missed samples,
   and existing saturation/contrast diagnostics. Never replace a missing pulse
   or interval with an expectation based on target speed or PWM.
5. Check `drop`, `fail`, `logdrop`, `q`, `qhigh`, `logq`, `lag_max_us`, `timeout`
   and `busy_drop`. Aim for zero losses in the initial regularity capture. If
   loss occurs, preserve it and distinguish optical behavior from radio/serial
   congestion. Verify Type-5/raw/CTO/fusion traffic still gets service.
6. Compare receiver Type-6 sequence/counts and CRCs with transmitter evidence.
   Later compare native pulse pKPH with existing NAVI approximately one-second
   pKPH over aligned physical episodes. Do not subtract unrelated uptime clocks
   or treat arrival jitter as physical interval jitter.

No numerical “sufficiently regular” tolerance is invented here. David and ChatGPT
will evaluate the captured irregularity and its practical significance before
choosing any future filtering or control use. Successful synthetic tests and a
compile do not answer the physical regularity question.

## Review corrections and retained limitations

Compared with David's candidate:

1. Replaced the once-per-second latest-event serial display with per-event FIFO
   logging; corrected the literal backslash-n and made unavailable speed `nan`.
2. Preserved all completions using detector `fall`; retained the already-correct
   sample timestamp semantics and cleared predecessor timing after actual
   detector continuity loss. No detector semantics changed.
3. Captured span from the current detector result, not the prior loop's globals.
4. Added exact packed layout assertions and reused the installed pitch constant;
   restored the settled-pitch interpretation of the legacy calibration ID.
5. Added pulse-specific send failure, serial loss, queue and latency diagnostics,
   prevented pulse priority from skipping every Type-5 service pass, and removed
   the raw-queue idle wait from the pulse response path.
6. Prevented accepted-send timeout/late-callback overlap in the shared serializer.
   No reliable-transport protocol or pulse retry was introduced.

Known detector limitations in the R2 review and subsequent stop tests remain;
this experiment does not claim to fix optical acquisition, missed modulation,
lighting adaptation, stopped-wheel interpretation or distance accuracy. A
completed optical pulse is the instrument's observation, not independent proof
of locomotive travel. Existing unrelated diagnostics are retained, not certified
anew. Finite queues, extra traffic, ADC timing and serial throughput need a
supervised physical test.

## Validation record

- Genuine Arduino/ESP32 compile using installed Arduino ESP32 core **3.3.12**:
  `arduino-cli compile --fqbn esp32:esp32:esp32 --build-path /private/tmp/ir-pulse-event-1-7-build firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX`.
  Passed: **904,512 bytes flash (69%); 64,656 bytes static RAM (19%)**.
  No compiler warnings were emitted in the captured build log. Dynamic queue
  allocations are additional to the reported static RAM. No upload command ran.
- `tools/test_ir_pulse_event.cpp`, compiled with C++17, `-Wall -Wextra -Werror`
  and AddressSanitizer/UndefinedBehaviorSanitizer: passed authoritative completion
  correspondence, timestamp/count/distance coherence, exact wire assertions,
  native regular and irregular intervals, first/recovery interval handling,
  sample-gap/saturation/contrast/time-order faults, retained stops and 64-bit time.
- `python3 tools/test_ir_pulse_transport.py`: passed. It extracts and compiles
  the sketch's actual CRC/callback/radioSend functions against a deterministic
  host stub; checks immediate and callback failure, timeout, no overlapping
  accepted sends, late-callback isolation/recovery and CRC byte coverage. This
  is a host regression, not an ESP32 compile or physical congestion test.
- Existing `test_ir_movement`, `test_ir_movement_contract`,
  `test_ir_movement_wire`, and `test_ir_stationary_revision` compiled/run with
  C++17 and both sanitizers: passed.
- Repository wire-namespace search and RX/NAVI parsing inspection completed;
  packet sizes/CRC offsets are asserted in the real firmware compile. Type-5
  encoding and production block, common detector/contract/wire headers and NAVI
  firmware remain unchanged. Final scoped diff reviewed and whitespace checked.

## Relationship to NAVI Engineer School

Future NAVI work may relate location + direction + physical speed + current PWM
+ throttle change to observed response. Long-term locomotive/location experience,
today's consist/load context, immediate physical evidence, current geographic
requirement and NAVI's throttle judgment are distinct layers. Event boot identity,
physical timestamps, counts and unaveraged intervals preserve evidence that those
layers may later need. This experiment supplies only the immediate evidence
layer. It grants no authority to learn, command, or change throttle behavior.


## 2026-10-06 follow-up: remote transmitter transport truth (TX 1.8)

### Provenance, first field findings and scope

Starting branch: `codex/ewo-pwm-zero-localization`; local and remote HEAD
`c5e6594b7fe3f610a65896d41ae3bf5acf1ebd11`. Commit `65210f3` is its parent.
The intervening commit adds the separate NAVI pulse observation experiment;
its firmware, tests and specification remain untouched. Four pre-existing
untracked NAVI control/test files are preserved and excluded. Rollback is a
revert of this scoped instrumentation commit; this task does not deploy or flash.

David supplied `IR_PULSE_TRANSPORT_STATUS_TYPE7.patch`, a candidate hunk outline
with bare `@@` markers, not an automatically applicable unified diff. It was
reviewed against the actual canonical IR source. No separate updated full sketch
was needed to interpret those supplied hunks. The review was performed by one
agent. The original Type-6 helper and all three common detector/contract/wire
headers remain byte-for-byte unchanged from `65210f3`.

The earlier “not flashed” statements above describe the original implementation
stage. Subsequently, 1.7 was flashed and passed a 60-second stationary USB boot
check. David then conducted the first field run. The following field results
are David's supplied experiment summary, not a fresh independent reanalysis of
the complete capture in this task:

- Main movement boot: `7fa47b3e1c614974`; 4,983 received Type-6 events over
  approximately 459 seconds. Of these, 4,981 carried TRACKING and only two
  INADEQUATE_CONTRAST. Six `dt_us=0` observations correctly represented missing
  predecessor timing, not measured zero speed.
- Around 20 pKPH: approximately 75–86 ms median intervals. Around 40–46 pKPH:
  approximately 39–42 ms medians, 5.8–7.1% CV. Around 47–51 pKPH: approximately
  35–38 ms medians, 4.4–5.8% CV. These are reported physical examples, not new
  acceptance thresholds or a revised speed/pitch conversion.
- Native, unsmoothed timing showed strong regularity during steady motion and
  physically coherent acceleration, approaches, final deceleration and stopping.
  Station stops continued to operate correctly. This run does not justify
  mandatory averaging. No filtering, averaging, interpolation or repaired pulse
  evidence is introduced here.
- Sequence range 4–7,367 contains 7,364 positions: 4,983 received and **2,381
  missing**, approximately **32.33% receiver-observed discontinuity**. RX reported
  `qdrop=0`, `badlen=0` and queue depth zero. These cannot establish where the
  events disappeared before the Pi log. They do not prove successful transmission.

The IR car has no USB serial connection during normal railway operation, so the
transmitter's serial-only PULSE_STAT truth was unavailable in that run. Type 7
makes that cumulative transport truth remotely observable. It is diagnostic
information only, with no movement, speed, navigation, throttle or control
authority. Engineer School and NAVI compensation remain out of scope.

### Type-7 contract and candidate corrections

A repository-wide search of tracked firmware, tools, documentation and historical
source found no conflicting Type-7 assignment under IR magic `0x4952`, version 1.
Other namespaces (including magic `0xC6` and `0x49523031`) are unrelated. Retain
**magic 0x4952 / version 1 / type 7**. Definition: adjacent
`PulseTransportStatus.h`, separate from the unchanged Type-6 helper.

The candidate's `sizeof == 66` was incorrect: its proposed fields total **70
bytes**. The implementation retains all those fields and corrects the assertion,
adds offsets/transport-ceiling assertions and an exact Python decoder.
Little-endian packed layout, Python `struct` **`<HBBIIQ12IH`**:

| Offset | Bytes | Field | Meaning |
|---:|---:|---|---|
| 0 | 2 | magic | 0x4952 |
| 2 | 1 | version | 1 |
| 3 | 1 | type | 7 |
| 4 | 4 | sid | Existing transmitter session ID |
| 8 | 4 | sequence | Status report attempt sequence; starts at 1 |
| 12 | 8 | bootId | Same movement boot as Type 5 and Type 6 |
| 20 | 4 | generated | Detector completions captured by the Type-6 producer |
| 24 | 4 | sent | Type-6 send-service successes, defined below |
| 28 | 4 | failed | Type-6 send-service failures |
| 32 | 4 | dropped | Type-6 FIFO admission failures |
| 36 | 4 | logDropped | Per-event serial FIFO admission failures |
| 40 | 4 | queueDepth | Current Type-6 FIFO occupancy |
| 44 | 4 | queueHigh | Highest observed post-admission occupancy |
| 48 | 4 | logQueueDepth | Current serial event FIFO occupancy |
| 52 | 4 | lagMaxUs | Largest completion-to-radio-service lag, capped at UINT32_MAX |
| 56 | 4 | timeouts | Radio-wide accepted-send timeout count |
| 60 | 4 | busyDrops | Radio-wide service attempts rejected while callback outstanding |
| 64 | 4 | sendErrors | Radio-wide failed service count, including status failures |
| 68 | 2 | crc | CRC-16/CCITT-FALSE over bytes 0–67 |

CRC parameters remain init 0xffff, polynomial 0x1021, no reflection/final XOR;
`123456789` yields 0x29b1. Embedded CRC excludes itself. The RX serial envelope
continues to carry a separate CRC over all 70 bytes, including embedded CRC.
The Pi verifies both. Size 70 is below the existing 250-byte transport ceiling.

Cadence is **1000 ms minimum between status attempt starts**, checked by the
radio task after one Type-6 and the latest Type-5 have been serviced. The first
eligible report is at uptime >=1000 ms. Radio scheduling may delay it; no
catch-up burst or retry occurs. It is independent of speed and serial output.
This corrects the candidate's dependency on a flag set by the serial loop.
Status sequence increments before every attempt, including failed attempts,
and wraps modulo 2^32. A missing status does not erase cumulative transmitter
truth; the next report exposes the then-current counters. No status FIFO is added.

The optical sampler is unchanged and never waits for status transmission. Type-5
production, cadence, wire layout and latest-value queue are unchanged. Type-6
capture, native timestamps/intervals, FIFO and send accounting are unchanged.
Status uses the existing `radioSend()` serializer with its **100-ms maximum wait**;
it adds at most one such wait per second and cannot repeatedly run ahead of pulse
or movement service. It cannot preempt an in-flight send. During a radio fault,
that bounded wait can add pulse lag, and all traffic remains inhibited while a
previous accepted send has no callback, exactly as in the existing serializer.
There is no claim of zero added airtime or field-proven lack of interference.
Measure queueHigh/lagMaxUs and receiver loss in the next physical run.

Status failures change only existing radio-wide counters, never Type-6
sent/failed/dropped. Its snapshot precedes its own send result, so that result
can appear in a later report. `sendErrors` is now atomic for safe cross-task
reads; other transported shared counters already use atomics. Queue depths use
FreeRTOS queries. Sequence/timer are radio-task-owned; boot/sid are initialized
before tasks start. These safe individual reads are **not a transactional
snapshot**. Preserve transient arithmetic discrepancies rather than rejecting or
smoothing them. Counters wrap modulo 2^32 and reset on transmitter reboot;
queue occupancy is instantaneous and queueHigh/lagMaxUs are boot maxima.

### What the counters establish

- **generated:** one event for each authoritative detector completion observed
  by the producer, including optically degraded completions. This is not proof
  of valid physical speed, displacement or delivery.
- **admitted:** derivable as `generated - dropped` modulo 2^32 after concurrent
  updates settle; no additional counter is necessary.
- **sent:** `radioSend()` returned true: `esp_now_send()` returned ESP_OK and the
  corresponding callback reported ESP_NOW_SEND_SUCCESS within the existing wait.
  This is transmitter send accounting, **not acknowledgment by the Pi**.
- **failed:** one dequeued Type-6 event whose `radioSend()` returned false due to
  an outstanding prior callback, immediate API error, callback failure, or the
  100-ms callback timeout. A later success callback does not reclassify it.
  Consequently a timed-out event may still arrive at RX.
- **dropped:** a generated event could not enter the 32-entry outgoing FIFO;
  older entries are retained. Sequence/count still advance. Serial `logDropped`
  is separate and does not mean the Type-6 radio event was dropped.
- **received:** the Pi parser observed a structurally valid, CRC-checked Type-6
  frame in its RX serial stream. Duplicates/late rows are counted and flagged,
  never silently substituted for gaps or new physical events.

When settled, `generated = sent + failed + dropped + queueDepth` modulo 2^32;
an in-flight Type-6 and concurrent sampling can temporarily break this equality.
`sent + failed` counts resolved send-service attempts, not API submissions:
failed includes attempts rejected before `esp_now_send()`. These counters localize
the broad loss boundary without claiming to separate every driver/RF failure.
No reliable-transport protocol, retry or synthetic evidence is introduced.

[Espressif's ESP-NOW documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html)
explicitly distinguishes send callback success from application receipt. Broadcast
success supplies no acknowledgment from this particular receiver or its Pi logger.
Compare same-boot counter **deltas over aligned capture windows**, with boundary
queue/in-flight allowance, not a lifetime generated count against a mid-boot log.

### Pi implementation and deployment boundary

Read-only inspection identified the live recorder as
`/home/david/NGR/ir_scope_serial_record.py`, used by
`ngr-ir-espnow-record.service` at 921600 baud. Its original source was absent
from the repository; baseline SHA256:
`3a46a13b2bda26c7350b1c865e94b92ea155221a6642cebb68056b820b1c585f`.
It preserved raw lines and basic health only; it had no live Type-6 sequence
accounting to reset. Its actual service definition is now versioned unchanged
as `tools/ngr-ir-espnow-record.service` for reference, not installed in this task.

`tools/ir_scope_serial_record.py` preserves the existing CLI, daily raw filenames,
serial settings and basic health keys. `tools/ir_pulse_transport.py` adds strict
Type-6/Type-7 decoding and separate per-(sid,boot,type) sequence state. The existing
RX firmware already forwards arbitrary payloads up to 250 bytes and needs no
reflash. Its serial envelope has no sender MAC, so sid/boot provide stream
provenance but cannot resolve an actual identifier collision.

Every original RX line is preserved after the Pi epoch prefix. Each accepted
Type-7 adds **`<epoch> PULSE_TRANSPORT <JSON>`** to the same daily raw log. JSON
includes all wire fields (boot as 16-digit hex), `received_epoch`, receiver
milliseconds/RSSI, the matching Type-6/Type-7 accounting snapshots and the latest
RX STAT values with their own timestamp. No event is filtered or repaired.
Receipt time is now taken immediately **after** `readline()` returns, correcting
the old recorder's pre-blocking timestamp. It measures Pi read completion,
not RF arrival. Existing raw-record consumers ignore the additive line marker.

`ir_espnow_health.json` retains basic health and adds `pulse_transport`: per-stream
Type-6/Type-7 received counts, first/last forward sequence, forward gaps,
non-forward observations, latest status, malformed counts and last RX STAT
(`qdrop`, `badlen`, `q`, etc.). A Type-7 never changes Type-6 sequence state.
The first observed sequence does not imply loss before capture. Wrap is handled
modulo 2^32; duplicate/backward/late rows increment non_forward without moving the
high-water backward. Forward gaps are unrepaired discontinuities and may include
late packets; if non_forward is nonzero, use the raw log for exact sequence-set
analysis. Counts are local to the recorder process; its started_epoch identifies
restarts. Raw daily logs are durable reconstruction authority across restarts.
Malformed frames remain raw evidence and are counted, not allowed into decoded
status. Counter inconsistencies in otherwise valid Type-7 are logged verbatim.

**No Pi deployment occurred.** The repository's explicit deployment-authority
policy requires a separate operator action. After review, the narrowly scoped
commands from the repository root are:

```sh
ssh david@192.168.68.142 'cp -p /home/david/NGR/ir_scope_serial_record.py /home/david/NGR/ir_scope_serial_record.py.pre-type7'
scp tools/ir_scope_serial_record.py tools/ir_pulse_transport.py david@192.168.68.142:/home/david/NGR/
ssh david@192.168.68.142 'python3 -m py_compile /home/david/NGR/ir_scope_serial_record.py /home/david/NGR/ir_pulse_transport.py && sudo systemctl restart ngr-ir-espnow-record.service && systemctl is-active ngr-ir-espnow-record.service'
```

Use a fresh backup name if `.pre-type7` already exists. No new service or
system-wide Python dependency is needed; the active recorder already uses
pyserial and the decoder uses only the standard library. Deploy while stopped;
the service restart creates a visible capture boundary. Keep earlier logs.

### Next field-test objective

**Determine where receiver-observed Type-6 sequence loss occurs by reconciling
transmitter cumulative transport truth with Pi Type-6 reception.**

After separate authorization, deploy the two Pi files above, compile/flash the
canonical IR sketch, and verify banner
`IR_SCOPE_ESPNOW_PULSE_EVENT_TX_1_8_TRANSPORT_TEST` plus
`TRANSPORT FORMAT type=7 version=1 bytes=70 period_ms=1000 authority=DIAGNOSTIC_ONLY`.
Do not flash NAVI or RX for this extension. Capture stationary startup first and
confirm Type-7 reports reach the Pi even when generated=0. Then David can conduct
one normal untethered run, recording boot, direction, lighting, starts/stops and
all unedited daily logs/health. Capture status before and after motion to expose
the drained totals, including failed/dropped/logDropped, queue depth/high-water,
lag, timeouts, busy drops and total send errors.

Compare generated, derived admitted, sent/failed and Pi received/gaps for matching
sid/boot and windows. Generated much larger than sent identifies a deficit before
successful send accounting; generated approximately equal to sent but Pi counts
lower identifies a deficit after that accounting boundary. Matching totals with
reported gaps call for parser/window/duplicate/wrap review. RX queue/badlen and
Type-7 reception corroborate transport health. These are diagnostic cases, not
acceptance thresholds. Missing status during a total radio outage cannot itself
reveal transmitter counters; preserve that limitation. This task neither flashes
hardware nor begins the second field run.

### TX 1.8 validation record

- Genuine firmware compile: Arduino CLI **1.5.1**, ESP32 core **3.3.12**, FQBN
  **`esp32:esp32:esp32`**, default board options, clean fresh build directory:
  `arduino-cli compile --fqbn esp32:esp32:esp32 --clean --build-path /private/tmp/ir-type7-build-jrxzhzo_ firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX`.
  Passed: **904,900 bytes flash (69%); 64,664 bytes static RAM (19%)**.
  No compiler warnings emitted. Dynamic queues remain additional to static RAM.
- `python3 tools/test_ir_transport_status.py`: **five tests passed**. Compiles the
  sketch's actual status constructor/cadence/CRC functions against host stubs;
  checks packed bytes against the Python decoder, offsets, cumulative fields,
  1000-ms eligibility, no catch-up bursts, timer/sequence rollover, failed-status
  isolation, per-byte CRC corruption, schema/length/boot/envelope rejection,
  mixed Type-6/7 streams, gap/non-forward accounting, boot separation, RX STAT,
  exact raw preservation and the actual recorder loop/health-file outputs.
- The same suite verifies Type-5 decode/raw preservation and exact source
  equivalence of the sampler, `radioSend()`, Type-6 helper and common detector,
  contract and wire headers against `65210f3`. Type-6/Type-5 radio service occurs
  before the status check. No source-level evidence semantics changed.
- `python3 tools/test_ir_pulse_transport.py`: passed actual CRC/callback/send
  function tests for success, immediate/callback failure, timeout, busy rejection,
  late-callback isolation, recovery and Type-6 CRC coverage.
- Existing C++ tests `test_ir_movement`, `test_ir_movement_contract`,
  `test_ir_movement_wire`, `test_ir_pulse_event`, `test_ir_stationary_revision`:
  passed with C++17, `-Wall -Wextra -Werror`, AddressSanitizer and
  UndefinedBehaviorSanitizer, using `-I firmware/common`. The C++ harness in the
  new Python suite and the existing transport harness use those sanitizers too.
- Pi validation used the actual fetched production recorder source as baseline
  and exercised its modified loop with synthetic serial input and real local
  raw/health output files. No deployment or live Type-7 reception is claimed.
- Full scoped diff and whitespace checked; NAVI, RX, Type-6 helper and common
  detector files are unchanged. No flash, Pi deployment or second field run.
