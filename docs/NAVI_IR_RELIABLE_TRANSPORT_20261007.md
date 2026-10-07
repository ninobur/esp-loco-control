# IR/NAVI selective acknowledgment transport — 2026-10-07

Status: uncommitted implementation candidate; observation only. No commit,
push, merge, flash, operating activation, or field acceptance is authorized.
Starting/rollback revision: `83462fee6ff56ff85caa3a9d9d6e8a8d84c50925`.
Development branch: `codex/ir-navi-reliable-transport` in an isolated worktree.
The original working directories are unchanged.

## Architectural reconciliation and authorization

This applies the NGR principles of direct physical evidence, immutable
observations, visible uncertainty, NAVI-owned judgment, and separation of
measurement, delivery, judgment, and control. No principle is departed from.

Governing material reviewed:

- `AGENTS.md`, `docs/CLAUDE.md`, and
  `docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`.
- `docs/NAVI_DECISION_MODEL.md` and `docs/NAVI_EWO_GOVERNING_DOCUMENTS.md`.
- Decisions 0109–0114: target-only navigation, native evidence, contextual IR
  applicability, factual PWM-zero observations, NAVI-owned Hall reference, and
  the historical five-position proposal. The last is superseded by 0115.
- Subsequent applicable decisions 0115, 0116, 0118, 0120, 0121, and 0122:
  startup, coherent IR distance, first target, operator verification after
  PWM-zero movement, position/overlay authority, and configured physical pitch.
  None of their navigation/control implementations is changed here.
- `docs/NAVI_IR_PULSE_EVENT_PHYSICAL_EVIDENCE_20261006.md`,
  `docs/NAVI_PULSE_EVENT_PHYSICAL_SPEED_OBSERVATION_20261006.md`,
  `docs/IR_PACKET_FORMAT.md`, and the integrated NAVI README/contracts.

David explicitly approved this revision's 256 outstanding events, selective
ACKs, 100/200/400/800-ms retry backoff, 20-ms aggregate retry spacing, diagnostic
timely/late/uncertain classification using one second, receiver-boot-lifetime
RAM ownership, persistent `ir_pair` MAC admission for Type-6/7, and fixed
channel-11 reporting/mismatch detection. Automatic scanning/hopping is excluded.

That authorization supersedes the older pulse experiment's no-admission policy,
single-attempt transport, and per-event human-facing MQTT output, only for this
observation stream. Type-5 admission remains unchanged. The broader NAVI
multi-source policy remains unresolved. No new navigation or operating rule is
introduced. Source admission is not cryptographic authentication.

## Transport-independent input boundary

David additionally required interchangeable WIRELESS/DIRECT input paths, without
implementing a wired sensor or changing navigation authority in this revision.
`firmware/common/IrPulseInput.h` is the single explicit build configuration:

```cpp
#define NGR_IR_INPUT_PATH NGR_IR_INPUT_WIRELESS // default
// or: NGR_IR_INPUT_DIRECT
```

Set the same selection for both images, either in that header's default or with
`-DNGR_IR_INPUT_PATH=NGR_IR_INPUT_DIRECT` in compiler flags. Invalid values fail
compilation. This is a build-time selection, not an MQTT command or an automatic
fallback. An already-running remote IR image is not reconfigured by selecting
DIRECT in Otto's build; both images must use DIRECT (or the remote source must
be absent) to remove its experimental transmissions too.

Both paths hand off `ir_input::Arrival`, whose `NativeEvidence` is an alias for
the unchanged 61-byte `PulseEventPacket`. Receive time, queue-loss metadata, and
wireless MAC provenance remain outside physical evidence. The common
`Observation::receiveNative` handoff has no ESP-NOW, ACK, navigation, or actuator
dependency. Wireless acceptance/deduplication supplies this handoff; a future
wired producer will supply it directly, with local provenance and original
physical timestamps/counts. No wired GPIO, ISR, sampling driver, or synthetic
producer is implemented. The existing physical contract is reused in place,
not relocated or rewritten.

DIRECT bypasses pulse admission, peer discovery, joins, Type-6/7 delivery,
application ACKs and retries. Otto does not create the pulse radio task or
register its send callback/broadcast peer, and ignores experimental wireless
ingress. The IR radio submission boundary additionally refuses Types 6–10.
Existing Wi-Fi/MQTT, Type-5, raw/CTO/fusion traffic and operational processing
remain active. Boot serial output identifies the selected path. DIRECT is an
architectural placeholder with no native onboard pulse source yet, not a usable
wired-navigation build. It adds no authority and does not bypass a wireless
channel mismatch as a means of field-testing this wireless implementation.

## Wire and ownership boundary

`ReliablePulseTransport.h` is shared by the two sketches and host tests.
Existing Type-6 v1 `PulseEventPacket` construction and all 61 native bytes,
including its CRC, remain unchanged. The wire uses these version-2 frames:

| Type | Bytes | Purpose |
|---|---:|---|
| 6 | 97 | Immutable native event inside a delivery envelope |
| 7 | 115 | Cumulative transmitter accounting and receipt-age bound |
| 8 | 37 | Otto identity, current session/challenge, admitted IR MAC, channel, accepted transmitter boot |
| 9 | 135 | Selective ACK, capacity eight individual event identities |
| 10 | 26 | Transmitter boot joins the current receiver challenge/session |

All are packed little-endian, CRC-16/CCITT-FALSE protected, below 250 bytes.
ACK keys include receiver session, transmitter boot, slot, slot generation,
and native sequence. Old-session or old-generation ACKs cannot free new data.
Legacy Type-6/7 decoders still support archived v1 captures; they do not decode
this new envelope. No Pi service or scope receiver deployment is included.

NAVI validates source, session, boot, wire layout/CRC, configured pitch,
overflow-safe cumulative distance, native sequence/count relation, and
chronology relative to retained observations. It then copies the entire native
event into its receiver-owned slot ledger before producing an ACK. Optical
diagnostic reasons and unavailable intervals remain evidence; delivery acceptance
does not certify an interval as a usable speed measurement.

The callback only admits a bounded copy. If its queue is full, there is no ACK.
If the ACK output queue is full, acceptance persists; the next identical retry
gets another ACK, without another physical observation. Conflicting bytes under
the same identity are rejected. A slot's accepted record remains until the next
generation proves that the transmitter released the previous one. This is
bounded receiver ownership, not an unlimited pulse archive or a broker-delivery
guarantee. RAM acknowledgment ownership ends at receiver reboot.

## Buffering, scheduling, and recovery

The sampler's only change is zero-wait admission through a 256-credit counter.
One credit covers an event across acquisition handoff, retained radio state,
transmission, and ACK wait. ACK releases the credit. The existing 256-entry
acquisition queue and a radio-owned slot table use separate storage; together
they cannot own more than 256 unacknowledged events. The duplicate staging
storage is an explicit RAM cost, not a second 256-event delivery allowance.

The sampler never waits for the radio, an ACK, discovery, serial, or MQTT. Its
detector calls, sampling cadence, timestamps, pulse construction, physical
distance, and Type-5 production are unchanged. A full credit pool rejects the
new admission and increments overflow. It does not overwrite retained evidence
or prevent physical sequence/count progression. At about 25 events/s, 256 slots
span approximately 10.2 seconds without acknowledgment. No finite buffer can
preserve an unlimited outage.

New events get first-send priority over retries. A missing low sequence cannot
pin the window: acknowledged slots are independently reusable, and old holes
can be recovered after thousands of later accepted events. Receiver reboot
replay remains retry traffic, preserving new-event priority.

After attempts 1, 2, 3, and 4+, the retry eligibility intervals are respectively
100, 200, 400, and 800 ms. Round-robin selection serves due retries; aggregate
retry spacing is at least 20 ms. Delayed service creates no catch-up burst.
There is no expiration or retry-count abandonment. Radio availability and
existing traffic can delay attempts beyond eligibility.

One MAC submission at a time avoids callback misattribution, without waiting
inside a task. Type-5 is serviced before experimental work; fresh Type-6 first
attempts precede retry work. Existing fusion, observation, and raw traffic keep
service. Completion callbacks supply MAC-success/failure accounting separately
from application ACK counts. A 100-ms callback delay increments a timeout once;
the outstanding callback retains ownership. A callback that never returns can
leave that device's ESP-NOW submissions stalled. No unapproved radio-reset policy
is added. Sampling, NAVI, and command tasks do not wait for it.

NAVI processes at most four experimental ingress items after its existing
control services. It opportunistically ACKs the accepted items in that pass;
the packet supports eight entries, with no delay to fill a batch. A separate
low-priority radio task sends ACKs and discovery, independent of MQTT connect,
publish, and broker stalls. Failed ACK sends need no autonomous ACK retry storm:
retained transmitter events trigger subsequent ACKs.

## Current evidence, history, and time domains

Unique arrivals below the highest accepted physical count are historical
recovery. They reduce missing counts and retain original bytes, but never
replace the current pulse observation or enter operational NAVI. Duplicates
only increment duplicate accounting and generate another ACK. A fresh current
event may pass earlier holes; no missing interval or pulse is manufactured.

`missing_since_tx_boot = highest_received_completed_count - unique_received`
includes initial holes, including events predating this receiver session. It
does not include an unseen tail; compare Type-7 generated/overflow/unresolved
counts too. It must not be interpreted as all losses caused during NAVI's current
boot. The ledger is not a general permanent historical database.

Envelope `ageUs` is transmitter completion-to-submission elapsed time. Adding
receiver-local time since arrival gives a lower bound on present evidence age.
The transmitter measures completion-to-application-ACK elapsed time in its own
clock, which upper-bounds receiver acceptance delay. Type-7 returns the bound
for its highest confirmed physical count. NAVI only applies it to that exact
current event. Devices' independent uptime timestamps are never subtracted.

- **Timely:** an available upper bound is at most the diagnostic one second.
- **Late:** the lower bound exceeds that reference.
- **Uncertain:** available bounds cannot establish either condition.

Acceptance timing and present evidence age are separate. Receipt is often
uncertain until a matching ACK-derived bound arrives; a large ACK round-trip
alone does not prove late receipt. Lower/upper values and their identity remain
visible. One second is solely the approved diagnostic reference, with no
navigation admission, stopping rule, new speed cap, or control deadline.

The existing `ir_pulse` topic becomes a periodic summary. `event_valid` retains
historical interval meaning; `fresh` retains receiver-age meaning;
`transport_fresh` separately gates its diagnostic `valid` field. This does not
alter any operational Type-5 or NAVI freshness threshold.

## Identity and session changes

An unconfigured `ir_pair` admits no Type-6/7. Unknown MACs are rejected and
counted. An explicit different pairing creates a new receiver session; repeating
the same pairing does not reset it. Type-5 continues its existing processing.

Otto advertises 9950011, the admitted IR MAC, and a boot/pairing-scoped random
session. The IR car accepts compatible channel-11 hellos only if they name its
own MAC and Otto's ID. The received source MAC becomes the unencrypted Otto
peer. During that IR boot, a different MAC claiming Otto cannot automatically
replace it. An Otto hardware replacement therefore needs deliberate IR restart
and pairing, not automatic peer takeover.

An IR boot joins only the current hello challenge. Accepting it consumes the
challenge; stale join frames cannot roll the active transmitter boot backward.
NAVI records boot changes, previous missing evidence, and the last known
previous transmitter unresolved count, explicitly marking whether status was
known. This is a last-observed count, not a recovered power-loss inventory.

On NAVI reboot, the session changes. The IR car retains/reoffers unresolved
events and rejects ACKs from the prior session. Already ACKed events are not
promised to survive receiver reboot. On IR reboot, its RAM backlog is lost and
its measurement boot changes. Without persistent storage, the exact final
unresolved inventory may be unknowable; session boundaries expose that limit.
CRC, MAC admission, and challenges do not provide cryptographic authentication
or protection from an attacker impersonating an authorized MAC.

## Channel and telemetry

IR stays on channel 11. NAVI follows its existing Wi-Fi configuration. No scan,
hop, AP reconfiguration, or additional channel-selection policy is introduced.
NAVI reports its actual associated channel, expected IR channel, and
`channel_state=compatible|mismatch|unknown`. `field_channel_ready` only describes
this channel check, not overall readiness. Mismatch blocks field-test approval;
unknown cannot establish compatibility. The disconnected IR car cannot identify
a remote AP's channel from silence; its local discovery age remains visible.

`ngr/loco/9950011/telem/ir_link` reports periodic cumulative transport summaries:
generated/overflow, attempts/retries, MAC outcomes, application ACKs, unresolved
occupancy/high-water/oldest age, unique acceptance, duplicates/out-of-order,
invalid/stale/source rejects, ingress/ACK-queue loss, receipt/age bounds,
Type-7 sequence gaps/order faults/staleness, session boundaries, and channels.
Missing Type-7 snapshots are diagnostic discontinuities, never synthetic pulse
loss. MQTT remains best effort; summary snapshots may be coalesced. There is
no per-pulse MQTT stream. Native serial capture remains available on the IR car.

## Verification and review

Host tests compile actual firmware callback, ingress, status, telemetry,
discovery, ACK-source filtering, and asynchronous send functions with bounded
queue/radio mocks. They exercise packet loss, ACK loss, duplicate recovery,
reordering, receiver saturation, ACK-output saturation, 256-slot exhaustion,
old holes across 1,198 later deliveries, stale slot-generation ACKs, sequence
rollover, both reboot boundaries, source rejection, channel fault/recovery,
MQTT disconnection, Type-7 discontinuities, and immutable physical bytes.

Source isolation checks compare against the exact starting commit: detector,
movement contract/wire, physical event helper, Type-5 ingress, Hall task,
station service, PWM/ramp, MQTT command callback, and operational loss handling.
The sampler differs only in its zero-wait retained-credit admission call and
excluding disabled DIRECT transport from its overflow counter.
Existing NAVI integrated/base suites and physical detector/contract suites are
also run with AddressSanitizer/UndefinedBehaviorSanitizer where applicable.

Software tests cannot establish zero shared-radio/CPU impact on real hardware.
Before separately authorized field work, verify channel 11, correct pairing,
startup heap/task creation, sampling misses, command/STOP responsiveness,
Type-5 receive cadence, and latency/backlog under normal and congested traffic.
No claim of hard real-time delivery or readiness for IR-primary control follows
from a successful compile. Independent review remains required before commit.

Final host validation passed:

- `python3 tools/test_ir_pulse_transport.py`: 5 tests, including actual radio
  submission/discovery functions in both modes and invalid configuration rejection.
- `python3 tools/test_ir_transport_status.py`: 5 tests, including v2 status
  construction/cadence, DIRECT suppression, and archived-v1 decoder regressions.
- `python3 -m unittest tools.tests.test_navi_pulse tools.tests.test_ewo_integration tools.tests.test_navi_sync_format`:
  13 tests. Actual NAVI ingress is tested in both modes. DIRECT produces no pulse
  queue/discovery/ACK output, Type-5 still reaches NAVI, and the common handoff
  preserves identical physical bytes from local/wireless provenance.
- `sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh`:
  14 test-binary executions passed, including both locomotive maps and base EWO.
- Physical suites `test_ir_movement`, `test_ir_movement_contract`,
  `test_ir_movement_wire`, `test_ir_pulse_event`, `test_ir_stationary_revision`,
  `test_ir_phase_retention`, and `test_ir_default_equivalence` passed under host
  sanitizers. The equivalence test compared four million samples against the
  required `1de06ae^` detector baseline.
- Maximum exercised JSON sizes: `ir_link` 1,411 bytes and `ir_pulse` 846 bytes.
  The configured MQTT buffer is 2,304 bytes, including topic/header allowance.
- `git diff --check` passed. HEAD remains the exact starting revision; no changes
  are staged or committed. No deployment or external service changes were made.

Actual compilation uses Arduino ESP32 core **3.3.12**, FQBN
`esp32:esp32:esp32`, `--warnings all`, and external credential-header include
path only (credentials are not copied or changed). NAVI also uses
`-Werror=format`. Both modes are compiled separately; DIRECT adds
`-DNGR_IR_INPUT_PATH=NGR_IR_INPUT_DIRECT` to `compiler.cpp.extra_flags`.

| Build | Flash bytes | Static RAM bytes |
|---|---:|---:|
| IR WIRELESS | 909,920 | 87,360 |
| NAVI WIRELESS | 1,056,775 | 79,148 |
| IR DIRECT boundary | 905,380 | 87,320 |
| NAVI DIRECT boundary | 1,018,759 | 57,988 |

Static RAM figures exclude runtime queue/recorder/task/Wi-Fi allocations and
are not a startup-heap proof. Existing volatile-increment warnings in the IR
sketch and deprecated enum-operation warnings in Adafruit INA219 remain; these
unrelated paths/libraries were not refactored.

## Changed files and disposition

Twelve files comprise this uncommitted revision:

1. `firmware/common/IrPulseInput.h` — configuration and native input boundary.
2. `firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/ReliablePulseTransport.h`
   — bounded transport/receiver ledger and v2 contract.
3. `firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`
   — asynchronous delivery integration and accounting.
4. `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino`
   — source admission, receipt ownership, ACK radio task, summaries and mode gates.
5. `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NaviPulseObservation.h`
   — common native handoff and independent diagnostic transport-validity flag.
6. `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NaviPulseTelemetry.h`
   — expose the transport-validity distinction.
7. `tools/test_ir_reliable_transport.cpp` — deterministic protocol fault tests.
8. `tools/test_ir_pulse_transport.py` — actual transmitter functions/isolation/modes.
9. `tools/test_ir_transport_status.py` — actual status builder/legacy decoder tests.
10. `tools/tests/test_navi_pulse.py` — actual receiver and telemetry/mode integration.
11. `firmware/README.md` — candidate role, provenance, evidence status.
12. This report — governing reconciliation, approved boundary, validation/limits.

Self-review found no required change to navigation authority or the proven
measurement algorithm. Both endpoints implement the same v2 contract; mixed
old/new pulse firmware is not compatible. This is software-validated observation
instrumentation, **not evidence that IR-primary navigation is ready**. Actual
AP channel compatibility, installed-hardware latency/control independence,
heap/task startup, and congested-radio behavior remain unverified. No live
channel was read or changed: WIRELESS field authorization requires confirming
Otto's associated channel is 11. A mismatch requires David's decision, not an
automatic channel-policy change.
