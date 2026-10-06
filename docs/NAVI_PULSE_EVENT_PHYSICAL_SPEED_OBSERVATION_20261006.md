# NAVI Pulse-Event Physical Speed Observation Architecture — 2026-10-06

Status: implemented and reviewed observation experiment; genuine ESP32 compile
and deterministic host tests passed. Not flashed or field accepted. No throttle,
Engineer School or Document C control implementation is authorized by this work.

**Are native consecutive completed-pulse intervals sufficiently regular to
provide useful physical speed evidence without averaging?** Only field evidence
can answer this question. This implementation preserves the observations needed
to answer it; it does not assume a numerical regularity tolerance.

This follows the [NGR design principles](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md):
prefer direct physical evidence, preserve discrepancies and provenance, put
judgment with NAVI, and distinguish observation from authority. No departure is
intended. [Document C](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
and [decision 0121](decisions/0121-current-position-direction-and-overlay-determine-navi-operating-authority.md)
remain the governing operating architecture, unchanged by this experiment.

## Provenance and scope

Repository: `ninobur/esp-loco-control`.
Branch: `codex/ewo-pwm-zero-localization`.
Starting local and verified remote HEAD:
`65210f3d70cf33aeae270cae65776fe8dd2412aa` — committed IR pulse-event transmitter.
Earlier documentation HEAD `e1ec763` and firmware baseline `0116518` are ancestors,
not this task's starting HEAD. Four pre-existing untracked control/test files
remain untouched and excluded. Rollback is a revert of this scoped commit;
there is no hardware deployment to undo.

David supplied `NAVI_PULSE_EVENT_TEST_0116518.patch`. It is a candidate hunk
outline with bare `@@` markers, not an automatically applicable unified patch.
Each proposed hunk was reviewed against current source and the actual transmitter.
Review and implementation were performed in this single-agent task, not by a
separate independent agent. The canonical working sketch remains
`firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino`,
identified as `NAVI_EYES_WIDE_OPEN_INTEGRATED_PULSE_EVENT_TEST`.

The [committed IR specification](NAVI_IR_PULSE_EVENT_PHYSICAL_EVIDENCE_20261006.md)
and transmitter at starting HEAD govern the wire contract. NAVI imports its
`PulseEventEvidence.h` packet definition directly; no competing struct or IR
transmitter/header edit is introduced. `NaviPulseObservation.h` is an isolated
receiver/measurement state machine. `NaviPulseTelemetry.h` is an output formatter.
Neither has a NAVI instance or any route/PWM/station reference.

## Two evidence streams, different authority

Hall anchors geography; IR measures physical movement; NAVI interprets the
measurements; the dispatcher supplies operating intent. PWM is an actuator.
This experiment improves observation only.

| Stream | NAVI use in this revision |
|---|---|
| Type-5 v1, 110 bytes | Existing `navi.observeIr()` path, cumulative movement, continuity/epoch, Hall/IR reasoning, position, PWM-zero displacement handling, station behavior and legacy speed estimator; unchanged |
| Type-6 v1, 61 bytes | Parallel pulse observations and telemetry only; never calls `navi.observeIr()`, `noteObservationLoss()`, recorder consumption, station logic or an actuator API |

Type-6 cannot intentionally alter actual/commanded PWM, ramp target/rate,
cruise, approach/stopping, GO/STOP, enrollment, Hall target confirmation,
missed-magnet reasoning, position propagation, navigation epoch or dispatcher
authority. The existing Type-5 decoder, processing function, observation-loss
handler and control functions remain byte-identical. New instrumentation still
uses CPU, memory, serial and network capacity: logical authority isolation is
not a claim of zero hardware scheduling impact.

## Measurement rationale and mathematics

The detector samples approximately every 1 ms. Its authoritative completion is
the open pulse returning below the low threshold, incrementing completed pulses
and recording that sample timestamp. The committed transmitter emits that event.
Packet generation, radio submission, receiver arrival and telemetry publication
are separate, later events. Receiver arrival jitter must not become physical
interval jitter; clocks on different devices must not be subtracted.

Existing Type-5 reports every approximately 100 ms. Quantized increments of
0/1/2 or more pulses motivated `NaviIntegratedCore::addSpeedPoint`'s unchanged
900,000–1,500,000-us window. Its source explicitly explains the 0/1/2-pulse speed
steps. Retaining it provides a useful side-by-side comparison instrument.

For two consecutive usable physical completions:

```text
interval_us = current completedUs - previous completedUs
speed_mm_s  = 9652 * 1000 / interval_us
pulse_pKPH  = speed_mm_s / 5.37325
```

Installed pitch is `ir_movement::kInstalledPitchUm == 9652`, or 9.652 mm.
Conversion uses the existing `EWO_PKPH_MM_PER_SEC` from `EwoStationStop.h`,
consistent with [decision 0099](decisions/0099-canonical-house-pkph.md).
At 20 pKPH expect about 11.134 events/s (89.815 ms); at 45 pKPH about 25.051
(39.918 ms). A one-second cruise window spans about 242 mm and 25 intervals.
Microsecond timestamp units do not improve the approximately 1-ms detector
sampling resolution into microsecond edge precision.

No rolling mean/median, exponential smoothing, low-pass filter, packet-count
average, multi-pulse average or five-beat average is added. Stop-spanning native
intervals are preserved when the transmitter retains continuity; they include
the stopped time and are not instantaneous restart velocity. No observation at
rest implies neither measured zero pulse speed nor an electrical fault.

## Wire acquisition and identity

Wire: magic `0x4952`, version 1, Type 6, little-endian packed 61 bytes. CRC is
CRC-16/CCITT-FALSE over bytes 0–58; its field begins at offset 59. See the IR
specification for all offsets. Assertions fix size/offsets, and verify capacity
against the old 110-byte ingress buffer and new exact 61-byte buffer. Repository
search confirms Type 6 is now assigned by the preceding IR commit; separate
`0xC6` IR-speed magic is not this protocol.

Before the operational queue, the callback diverts exact 61-byte frames or
frames with recognizable IR magic and Type-6 header to a separate 32-entry FIFO.
Recognizable Type-6 frames with wrong lengths, including 110 bytes, therefore
cannot consume operational queue capacity. Exact-length candidates with corrupt
headers are also rejected in the experimental parser. Arbitrary traffic whose
length and header are both unrecognizable remains subject to the existing
callback/parser behavior; no general radio-protocol redesign is claimed.

Validation checks exact length, magic/version/type, nonzero boot/completion
count/timestamp, pitch, overflow-safe nominal distance, enum range and CRC.
A source's SID must remain stable within its same MAC/boot stream. SID is not
assumed to equal the boot's low word: the transmitter's nonzero-boot fallback
makes that relationship non-universal. Raw fields of a rejected packet are
untrusted diagnostics and never a valid speed measurement.

Current NAVI accepts all structurally valid Type-5 sources and uses source MAC
plus movement boot to identify a stream. Stored `pairedIrMac` and `ir_coupled`
are display/provenance information, not source-admission policy. Type-6 preserves
that policy: it records incoming MAC/boot and resets its own predecessor on a
change. It introduces no pairing filter, automatic pairing, authoritative source
selection or per-source control model. `same_source` compares the pulse MAC/boot
with the latest Type-5 MAC/boot; `coupled` is contextual only. Pairing metadata
remains in existing `diag/ir_link` output. Multiple simultaneous instruments
remain an inherited source-selection question; source switching is visible and
cannot be silently treated as one continuous stream.

## Continuity and recovery

A valid native observation requires an accepted usable predecessor, identical
MAC/boot, stable SID, sequence increment exactly one modulo 2^32, completed count
increment exactly one, increasing completion time, and a positive reported
interval exactly equal to the completion-time difference. Receiver time must
not move backwards, but it is never used in the physical speed denominator.

The first accepted event is reference-only, even if the transmitter reports a
nonzero interval. A missing sequence or completed count invalidates the gap
observation and records the discontinuity; that event establishes the next
reference. The next genuinely consecutive usable event can produce speed.
A reported zero interval means no usable transmitter predecessor. That event is
reference-only, and no interval is invented. TRACKING and retained SIGNAL_STALE
are usable exactly as in the transmitter. Other optical reasons are recorded,
with no speed, and do not establish a usable predecessor.

Duplicate/reversed sequences, counter reversal and non-increasing completion
time are rejected. They clear predecessor eligibility but cannot move the last
accepted ordering reference backwards. Bad packets similarly clear eligibility.
The next acceptable event is reference-only; the following consecutive event
can resume. Forward sequence differences use modulo arithmetic with a half-range
ordering rule; a UINT32_MAX-to-zero single increment is continuous. Boot/source
changes establish a new frame without speed. No missing timestamps are fabricated.

The receiver queue attaches its cumulative drop count to each admitted item.
A changed count clears predecessor eligibility at the first admitted post-loss
item. Older items queued before that loss retain their historical interval facts.
The current display also withholds `valid` if newer queue loss exists beyond
that item's recorded loss count. Experimental loss never increments `irQueueDrops`
or calls NAVI's operational loss handler.

## Telemetry contract

New topic: **`ngr/loco/<LOCO_NAME>/telem/ir_pulse`**, non-retained, best-effort.
Existing `telem/ir`, `telem/speed`, NSR1 records and control topics are unchanged.
Legacy `seenIrFrames`/invalid-wire counters continue to describe the old ingress
queue; recognized experimental traffic has its own `received`/`invalid` counters.

Each dequeued event, including rejected candidates, produces a compact report
for a **16-entry telemetry FIFO**. A **one-entry latest status queue** provides a
one-second heartbeat during silence. Only heartbeat snapshots may be overwritten;
per-event reports are never replaced by a newer event. EVENT and STATUS records
are distinguished explicitly; do not count STATUS as another physical pulse.

The loop performs at most four pulse acquisitions per pass, after existing
control services, and never prints serial or waits for networking on this path.
It reads current legacy speed/identity only to build the comparison report.
The network task services existing MQTT/commands/NSR work first, then at most one
pulse event plus a pending status snapshot per pass. Experimental MQTT bypasses
the operational `pubQ`; pulse telemetry cannot fill it or change `pubDrops`.
Queue admission is zero-wait. Allocation failures are visible in the pulse boot
queue banner; they do not introduce a new control-startup refusal policy.

| Fields | Meaning |
|---|---|
| `kind` | EVENT or STATUS |
| `event_valid` | Historical native interval passed all continuity checks |
| `fresh` | Receive age is within existing `NaviIntegratedCore::kIrFreshUs` (1 s) |
| `valid` | event_valid AND fresh AND no newer receive-queue loss |
| `seq`, `completed`, `completed_us`, `interval_us` | Raw event sequence/count and transmitter physical timing |
| `mmps`, `pkph` | Native interval result if event_valid, otherwise JSON null; historical numbers remain visible when stale, with valid=0 |
| `legacy_valid`, `legacy_pkph`, `compared_us` | Existing estimator result and NAVI-local time when the comparison was captured; null when unavailable |
| `legacy_boot`, `same_source`, `coupled` | Comparison provenance; same_source requires MAC and boot agreement |
| `received_us`, `age_ms` | NAVI receive time and age at serialization; age null before an event or on reversed local time |
| `boot`, `sid`, `mac`, `reason`, `span` | Raw instrument identity and optical context |
| `flags` | Per-event reason bitmask below |
| `received`, `accepted`, `invalid`, `discontinuities` | Cumulative candidate frames, accepted ordered wire events, rejected packets/order, and discontinuous observations |
| `seq_breaks`, `pulse_breaks`, `time_breaks`, `interval_breaks` | Distinguish sequence, count, timestamp/receive-time and interval disagreement |
| `source_changes`, `boot_changes` | MAC and movement-boot transitions |
| `rx_drop`, `log_drop`, `mqtt_drop` | Experimental input rejection, event-output queue loss, and MQTT unavailable/failed/format-overflow attempts |
| `authority` | Always OBSERVATION_ONLY |

Flags: 1 BAD_WIRE; 2 SOURCE_CHANGE; 4 BOOT_CHANGE; 8 SEQUENCE_GAP;
16 PULSE_GAP; 32 TIME_ORDER; 64 INTERVAL_MISMATCH; 128 NO_INTERVAL;
256 OPTICAL_UNUSABLE; 512 QUEUE_LOSS; 1024 FIRST; 2048 SEQUENCE_ORDER;
4096 PULSE_ORDER; 8192 RECEIVE_TIME_ORDER. Multiple bits can coexist.
Accepted events may be reference-only; accepted does not mean valid speed.
Bad wire increments invalid; a queue-loss flag also increments discontinuities.
Time-break counts can count both bad completion and receiver time in one record.
Counters are unsigned 32-bit and reset with NAVI boot.

The 1-s freshness threshold is reused solely for displaying current applicability;
it neither filters physical intervals nor expires a known predecessor. Historical
validity remains independently visible. A legacy comparison is captured at
processing time, not synchronized to the optical completion; `compared_us`,
source matching and ages must be considered when analyzing lag. No new clock
synchronization or resampling is introduced.

At 115200 baud, `[IR_PULSE]` serial rows expose kind, valid/event_valid,
same-source/boot, sequence, completed count, completion time, interval, pulse and
legacy pKPH/validity, age, flags, discontinuities, invalid and loss counts, and
OBSERVATION_ONLY. Unavailable numbers print `nan`; MQTT uses valid JSON null.
One row is attempted per event plus one summary/second, not one selected pulse
per second. Network stalls can fill the finite telemetry queue and cause visible
`log_drop`; no lossless serial/MQTT delivery claim is made. MQTT failure does not
suppress the associated serial row. The MQTT failure counter in a payload is
sampled before that publish attempt; later output reflects that attempt's failure.

## Candidate corrections

- Reused the exact committed packet with exact size/offset checks, rather than
  maintaining a second approximate layout.
- Added overflow-safe distance checks, SID stability, source MAC, ordered-event
  handling and honest predecessor recovery after invalid or missing evidence.
- Preserved optical unusability and zero-interval semantics from the transmitter.
- Separated input queues/loss and output queues from operational IR/MQTT paths;
  the candidate's shared IR queue could otherwise inject Type-6 loss into NAVI.
- Replaced once-per-second latest-pulse output with event records plus a heartbeat.
- Used null/unavailable output instead of stale numeric values masquerading as
  invalid measurements; added historical-validity/freshness, source-comparison
  provenance and separately visible loss/discontinuity causes.
- Checked serialization bounds and retained the existing conversion and legacy
  speed estimator. No filtering or control implementation was added.

## Field protocol and unresolved physical questions

After separate flash/run authorization, capture NAVI serial and the new MQTT
topic from boot, along with transmitter serial/scope RX and existing legacy
speed telemetry. Confirm the correct Otto 9950011 profile/build and pulse queue
allocation banner. Verify source/boot agreement and observation-only authority.

1. Check first/reference-only events and ordinary consecutive recovery. Inspect
   zero intervals, unavailable values, count/sequence gaps and optical flags.
2. Record steady-motion runs near 20 and 45 pKPH in both directions. Preserve
   every interval. Inspect variation against the approximately 1-ms sampling
   granularity, without replacing the native series with an average.
3. Compare pulse and legacy pKPH for the same instrument/boot and physical run.
   Record starts, accelerations, decelerations and stops to measure how much
   sooner native evidence exposes actual changes. Keep long stop-spanning
   intervals visible and distinct from instantaneous restart speed.
4. Inspect normal ESP-NOW delivery, `rx_drop`, `log_drop`, `mqtt_drop`, invalid
   and discontinuity counters. Separate radio/queue loss, optical quality and
   physical interval variation. Verify existing Type-5/command telemetry stays
   healthy under added CPU, heap, serial, network and airtime load.
5. Ask whether native intervals are sufficiently regular, how large variation
   is, whether it is consistent with sample resolution, how closely legacy
   pKPH agrees, and whether additional filtering is justified. No answer to the
   filtering question is preselected in firmware.

Finite queues, MQTT reconnect delays, CPU scheduling, multiple simultaneous
instruments and inherited detector limitations remain field considerations.
The instrument reports optical completions, not independently verified travel.
No measured hardware congestion, response-latency or accuracy claim follows
from passing deterministic tests. Existing Type-5 control behavior remains the
operational path and this experiment does not repair its historical limitations.

## Validation record

Validation uses the selected Otto 9950011 profile and private credentials only
through the existing external include path; credentials are not added to Git.

```sh
sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration tools.tests.test_navi_pulse
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all \
  --build-path /private/tmp/navi-pulse-event-20261006 \
  --build-property 'compiler.cpp.extra_flags=-I/Users/davidbrown/esp-loco-control -Werror=format' \
  firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED
git diff --check
```

- Existing integrated/base NAVI runner passed, including map profiles, core,
  recorder, operations, regression, authority, first-target, PWM-zero,
  measured-station-stop and configuration/station checks. New pulse test uses
  C++17, `-Wall -Wextra -Werror` and Address/UndefinedBehavior sanitizers.
- New pulse tests cover actual transmitter interoperability, first events,
  native irregular timing/arrival jitter, CRC and malformed fields, overflow,
  gaps, invalid optical state, source/boot changes, reordered/duplicate evidence,
  loss recovery, retained stops, 32-bit sequence wrap and 64-bit time, including
  the transmitter's zero-SID/nonzero-boot fallback.
- Eleven existing Python interoperability/ingress/controller/recorder tests
  passed. Two new Python tests compile actual callback/ingress functions with
  bounded queue stubs and verify Type-6 overflow/invalidity cannot call NAVI or
  alter its observation count, epoch, position, consumption ID, Type-5 snapshot,
  PWM-zero hold, or operational loss counters. Type-5 remains consumable afterward.
- Strict JSON parsing exercises actual formatter output at absent, valid, stale
  and maximum-field cases, including nulls and payload capacity; largest tested
  payload is 826 bytes within the unchanged 1200-byte ceiling.
- Genuine ESP32 build passed using installed core 3.3.12: **1,022,959 /
  1,310,720 bytes flash (78%); 58,220 / 327,680 bytes static RAM (17%)**.
  Dynamic queue allocations are additional. The initial full build emitted only
  the three existing Adafruit INA219 enum-conversion deprecation warnings; the
  final incremental build emitted no warnings. No new source warnings.
- Scoped diff confirms all IR transmitter/shared files, Document C/0121, NAVI
  core, ramp, station, command, Hall, PWM-zero, existing status, Type-5 ingress
  and observation-loss implementations are unchanged. No flash/upload or merge.

Host queue tests model bounded software behavior, not ESP32 scheduling or physical
radio/serial throughput. This is not a claim of field acceptance.

## Future work boundary

Rapid physical observation does not authorize rapid throttle intervention.
Five fresh beats followed by a possible ±1 PWM intervention and an observation
moratorium remain separate future steady-speed work. Glide-path control is
also outside this implementation. No Document C text or control mechanism is
changed to accommodate the experiment.

Future Engineer School may relate location + direction + physical speed + PWM
+ throttle change to physical response, including grade/curve effects, response
latency, locomotive history and today's inferred consist/load. This receiver
preserves immediate physical evidence for that later work. It implements no
learning, inference of consist, persistent response model or throttle judgment.
