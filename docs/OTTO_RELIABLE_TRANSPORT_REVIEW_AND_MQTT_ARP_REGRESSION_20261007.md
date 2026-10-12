# Otto: reliable IR transport review and MQTT/ARP regression — 2026-10-07

Status: **investigation record. Mechanism NOT confirmed.** No code was changed,
compiled for ESP32, or flashed by the author of this report. No commands were
sent to Otto. This document records evidence, analysis and proposed
experiments for David's decision (AGENTS.md §1, §2, §5, §7).

Principles applied (`docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`): field
evidence is kept separate from interpretation; installed-hardware evidence
outranks dry analysis (AGENTS.md §8); measurement is preferred to speculation,
so bounded discriminating experiments are proposed instead of a patch. No
principle is departed from. This record proposes no architecture.

| Revision | Role | NAVI `SKETCH_NAME` |
|---|---|---|
| `442a3d70599a403ba2fef168503d05f060e5e72c` | Working baseline, restored on Otto, confirmed operational | `NAVI_EYES_WIDE_OPEN_INTEGRATED_PULSE_EVENT_TEST` |
| `83462fee6ff56ff85caa3a9d9d6e8a8d84c50925` | Intermediate (addressed unicast, v1 hello) | `NAVI_EYES_WIDE_OPEN_INTEGRATED_IR_UNICAST_RX_TEST` |
| `48ccfb2646c2ee29ac9137f03d51715f48afc0be` | Failed candidate (selective-ACK reliable transport) | `NAVI_EYES_WIDE_OPEN_INTEGRATED_IR_RELIABLE_RX_TEST` |

Design under test: `docs/NAVI_IR_RELIABLE_TRANSPORT_20261007.md`.

---

## Part A — Independent pre-flash review of `48ccfb2` (summary)

Verdict given before flashing: **PASS WITH RESERVATIONS** for an
observation-only field test. The review examined source only; it did not
anticipate the field failure in Part B. That omission is itself a finding:
the review did not account for internal-RAM budget or for Wi-Fi receive
behaviour, and no heap/stack measurement existed for either revision.

Confirmed defects found in review (none fixed):

- **D1 — Hello identity hard-coded.** `NAVI_EYES_WIDE_OPEN_INTEGRATED.ino:1022`
  sends `ir_link::Hello` with its default `loco=Otto` (9950011) instead of
  `LOCO_ID`. The `83462fee` hello used `LOCO_ID`. Correct only while Otto's
  profile is active.
- **D2 — Host tests depend on the compiler.** The harness invokes `c++ -Wall
  -Wextra -Werror`. Under GCC 13 compilation fails on
  `ReliablePulseTransport.h:138` (`-Wmisleading-indentation`; behaviour is
  correct), a generated `memset` on `ir_link::Status` in
  `tools/tests/test_navi_pulse.py` (`-Wclass-memaccess`), and the pre-existing
  `tests/test_pulse_observation.cpp:29`. With those downgraded to warnings, all
  23 Python tests, `run_tests.sh` and `test_ir_reliable_transport.cpp`
  (ASan/UBSan) pass.
- **D3 — Timeliness flags almost always 0/"uncertain" during motion.** The
  upper bound is set only when a Type-7 names the exact current count
  (`ReliablePulseTransport.h:185`) and is cleared by the next accepted event
  (`:173`), about 40 ms later; NAVI samples it once per second. Use the
  transmitter's `ack_timely`, `ack_uncertain`, `ack_age_max_us` instead of
  `ir_pulse.valid` / `current_timing`.

Other review conclusions: transport logic (first-send priority, no
head-of-line blocking, immutable native bytes, duplicate/historical isolation,
session/boot keys, overflow accounting) is internally consistent;
WIRELESS/DIRECT separation is correct in both images. A missing ESP-NOW send
callback stalls the IR car's entire radio path, but `83462fee` line 239 had the
same permanent stall, so it is pre-existing, not introduced. The IR car retries
indefinitely to an absent Otto (`naviAccepted` never expires); airtime cost is
unmeasured.

---

## Part B — Field failure after flashing `48ccfb2` to Otto

### B.1 Field evidence (as reported by David; preserved unchanged)

Failed candidate `48ccfb2`:

- Otto associated with Wi-Fi on channel 11; RSSI about −33 to −39 dBm.
- MQTT repeatedly failed with PubSubClient states `-2` and `-4`.
- Dashboard cycled ONLINE/STALE; locomotive could not be operated.
- Broker (`192.168.68.142`) received Otto's TCP connection attempts and UDP.
- Broker sent 16 unanswered ARP requests during a 25-second capture; its
  neighbour entry for Otto remained `INCOMPLETE`/`FAILED`.
- Dashboard orientation, location and direction commands were sent but not
  received by Otto.
- No ESP32 reboot or panic observed.

Rollback to `442a3d7`, same hardware, IP `192.168.68.81`, same broker: Wi-Fi
normal, MQTT stayed connected, dashboard ONLINE, all commands worked.

The IR car was not reflashed during the comparison (it runs a pre-v2 image).
This establishes a firmware-dependent regression. It does not establish the
mechanism.

### B.2 What the supplied log contains

`9950011_20261007_181017.log` (MQTT capture, 2,814 lines, 18:10:17–18:16:32)
contains **only the working baseline**: all five `state/bootid` messages report
`…PULSE_EVENT_TEST`; no `…IR_RELIABLE_RX_TEST`, no `…IR_UNICAST_RX_TEST`, no
`telem/ir_link`. It therefore carries no direct evidence of the failure.
Observations it does contain:

1. **Four baseline restarts in 40 s.** Boot IDs `C9F712B9…` (18:12:13),
   `760795CD…` (18:12:23), `1A5230BC…` (18:12:38), `6D979BF2…` (18:12:46),
   `2776D0F4…` (18:12:52); stable thereafter. Possibly USB/monitor (DTR)
   resets during restore; **not yet confirmed as intentional.**
2. **Baseline UDP send failures.** `udp_failures` rises steadily from 0 to 537
   between 18:12:52 and 18:15:52, and `nsr_native_drop` reaches 228. These count
   `beginPacket()`/`endPacket()` failures in `syncSend` (`442a3d7` line 831),
   which on ESP32 Arduino usually indicate the network stack could not obtain a
   buffer. The working firmware may already operate near a buffer or memory
   limit.

### B.3 Reading of the field evidence

- Association intact and RSSI updating → beacons are being received.
- TCP/UDP from Otto reach the broker → Otto's transmit path works.
- ARP requests unanswered → the receive side fails: Otto either does not take
  in the broadcast ARP request or its stack never processes it.
- Without ARP the broker cannot return the SYN-ACK, which yields exactly
  PubSubClient `-2` (connect failed) and `-4` (timeout). **MQTT failure is a
  consequence, not an independent defect.** Connect/subscribe code is identical
  across all three revisions.

Why ARP fails first: neither revision calls `WiFi.setSleep(false)`, so Otto runs
in modem power-save (Arduino default). The AP buffers broadcasts and releases
them after DTIM beacons; broadcasts have no link-layer retransmission. Any
receive-side shortage at that instant loses the broadcast permanently, whereas
unicast is retried by the AP.

Unresolved: Otto evidently knew the broker's MAC (its SYN arrived). If Otto's
own ARP requests had reached the broker, the broker's entry would normally be
`STALE`/`REACHABLE`, not `INCOMPLETE`. A capture including Otto's ARP traffic
is needed.

### B.4 Runtime differences, IR car unchanged

| Change | `442a3d7` (works) | `83462fee` | `48ccfb2` (fails) |
|---|---|---|---|
| ESP-NOW transmit from NAVI | none | 22-byte hello 1/s from network task, no send callback | 37-byte hello 1/s plus ACKs from new `ir-ack` task; send callback registered (`…ino:1092`) |
| Receiver ledger, static RAM | — | — | +20,808 B (`…ino:89`; `ReliablePulseTransport.h:125-128`) |
| New task | — | — | `ir-ack`, 4 KB stack, prio 1, core 1, polls every tick (`…ino:1035-1068`, `:1333`) |
| MQTT buffer | 1,408 B | 1,408 B | 2,304 B (`…ino:1331`) |
| Pulse log queue (16 × 320 B) | present | present | removed (≈ −5 KB) |
| Telemetry stack frame in network task | ≈ 2.2 KB | ≈ 2.2 KB | ≈ 2.7 KB (`…ino:958-1010`, payload at `:971`) |
| Traffic from IR car | same | same | same (pre-v2 car ignores 37-byte hellos; never receives ACKs; its v1 frames counted invalid) |

Net internal-RAM effect: about 25 KB less heap in the candidate (≈21 KB static,
4.4 KB task, 0.9 KB MQTT buffer, offset by the removed queue). Existing
application allocations are already large: recorder 60,448 B (host-measured)
plus ≈13 KB trace storage, 21 KB publish queue, 4.6 KB Hall queue, and 4/8/8 KB
stacks. Remaining free heap is **unknown**; neither revision reports it.

### B.5 Hypotheses, ranked by evidentiary support

1. **Most likely (unconfirmed): Wi-Fi driver receive-buffer starvation from
   reduced internal heap.** The ESP32 Wi-Fi driver copies each received data
   frame into a newly allocated buffer before passing it to lwIP; if allocation
   fails the frame is dropped. Beacons are handled inside the driver and
   transmit uses separate buffers. This is the only mechanism found that fits
   all four facts (association intact, TX works, RX fails, no crash). The
   baseline's UDP failures suggest it was already close to a limit. Against:
   no heap measurement exists; the driver-allocation detail is from general
   ESP-IDF knowledge, not verified against core 3.3.12.
2. **ESP-NOW transmission on a power-saving STA disturbs broadcast reception.**
   `442a3d7` never transmits ESP-NOW; the candidate does. One hello per second
   does not obviously explain 16 consecutive missed ARPs. Moderate only because
   it is a real difference.
3. **`ir-ack` task or send callback disturbs Wi-Fi/lwIP tasks.** Weak: the task
   runs at priority 1 and yields every tick; Wi-Fi (23) and lwIP (18) pre-empt
   it; the callback sets two flags; the only new critical section copies 6 bytes.
4. **Silent network-task stack overflow.** Weak: frame grew ≈500 B; overflow
   normally triggers a canary panic, and none was observed.
5. **MQTT application defect.** Not supported (see B.3).

### B.6 Discriminating experiments (no new code; each needs David's approval)

Keep `442a3d7` as the rollback image throughout.

- **E1 — Flash `48ccfb2` built DIRECT** (`-DNGR_IR_INPUT_PATH=NGR_IR_INPUT_DIRECT`).
  Removes the 21 KB receiver (static RAM 79,148 → 57,988 B per Codex's build
  table), the `ir-ack` task, the send callback, all ESP-NOW transmission and
  pulse telemetry; keeps the 2,304 B MQTT buffer and everything common.
  Works → cause is in WIRELESS-only additions; run E2. Fails → cause is in
  shared changes; re-examine.
- **E2 — Flash `83462fee`.** Transmits hello 1/s, adds almost no memory, no new
  task. Fails → ESP-NOW transmit implicated. Works → memory leads. If Otto has
  previously run `83462fee`, its recorded behaviour answers E2 without flashing.
- **E3 — Broker-side static ARP, only if E1/E2 are ambiguous.** While the
  candidate runs: `ip neigh replace 192.168.68.81 lladdr <Otto MAC> dev <if>
  nud permanent`; undo with `ip neigh del`. MQTT connects → unicast RX works and
  only broadcast/ARP handling fails. Changes live Pi network configuration;
  requires separate authorization.

Direct confirmation of hypothesis 1 would require instrumentation (free heap,
minimum-ever free heap, largest free block at boot and every 5 s) in **both**
baseline and candidate. That is a code change and is not proposed until David
decides.

### B.7 If a mechanism is confirmed

No correction is proposed yet. If memory is confirmed, candidate remedies that
leave the transport rules, the 256-event window and all control paths unchanged
include: a smaller receiver-ledger representation, avoiding duplicated
256-entry storage on the receiver, and reverting the MQTT buffer increase. Each
is a design change for David. If ESP-NOW transmission is implicated, its
coexistence with power-save Wi-Fi is an architectural decision; Wi-Fi power
save must not be changed without authorization. No performance limitation or
control restriction is proposed.

### B.8 Evidence still needed

1. Serial and broker MQTT logs from the **failed** firmware.
2. Broker packet capture during failure:
   `tcpdump -e -n arp or host 192.168.68.81`, including whether Otto's own ARP
   requests arrive.
3. Whether Otto has ever run `83462fee`, and how it behaved.
4. Whether the four baseline restarts at 18:12 were deliberate.
5. Results of E1, and E2 if needed.

---

## Governing note

The reliable IR transport must not interfere with manual controls, STOP, MQTT
command delivery, NAVI execution, Hall navigation, station operation, or the
IR detector's 1-kHz sampling. On Otto, `48ccfb2` interfered with MQTT command
delivery. It must not be redeployed to an operating locomotive until the
mechanism is identified and a correction is approved and tested.
