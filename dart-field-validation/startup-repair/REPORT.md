# EWO 0_5 startup resource repair — October 14, 2026

**Outcome: the completed EWO 0_5 repair is installed on Otto. Two stationary
starts passed with zero allocation failures, live MQTT and zero throttle.
Normal-power INA219 operation remains unconfirmed; the train has not been
operated or cleared for a railway test.**

David authorized startup diagnosis and the necessary focused repair after the
first EWO 0_5 upload failed. The failed source is commit `83889d9`; the failure
and verified R2 restoration are recorded at `44d0275`. This work preserves the
approved NAVI model and all recorder capacities. It changes memory placement,
main-task stack allocation and additive resource reporting only.

## Measured cause

The diagnostic build reproduced the shortage on Otto, MAC
`68:09:47:ac:4e:b4`, using ESP32 core 3.3.12 and the existing private credentials.
Its [serial trace](diagnostic-serial.log) reports ordinary internal RAM:

| Startup point | Free byte-addressable RAM |
|---|---:|
| Before recorder | 204,276 B |
| After recorder | 125,420 B |
| After queues | 74,880 B |
| Before Wi-Fi | 69,808 B |
| After Wi-Fi starts | 17,812 B |
| Before MQTT connects | 5,084 B |

Repeated 2,308-byte allocations then fail. At the same time roughly 44 KB of
internal memory usable through aligned 32-bit access remains free. This
establishes runtime allocation exhaustion; the compiler's aggregate remaining
RAM figure did not describe usable startup headroom.

The September 29 queue reduction is already present: the legacy publication
queue remains 16 entries. Repeating that historical fix was not possible
without another capacity reduction.

The first repair relocated only consumption/action trace rings. It connected
to MQTT and remained online, but diagnostic traffic still produced allocation
failures (including 998-byte requests), and main-task stack high-water reports
fell to 444 bytes spare. This intermediate image is evidence, not the accepted
repair. Its build, upload, serial and MQTT artifacts use the `repair-` prefix;
its build identity is in [build-provenance.json](build-provenance.json).

The next image moved native Hall storage too and enlarged the main-task stack.
Its first boot showed zero allocation failures and 22,003 CRC-valid received
packets. A second boot exposed 20 brief allocation failures during the initial
backlog, with no further increase during steady running. This evidence prompted
relocating the remaining 64-entry NAVI record ring through the same wrapper.
Artifacts named `final-*` and `restart-*` preserve that intermediate result;
the completed image and checks use the `completed-*` prefix.

## Final implementation

- `NaviWordStorage.h` stores opaque wire bytes in aligned 32-bit internal
  memory. Only volatile 32-bit loads/stores access that backing memory; packed
  wire objects are copied through ordinary RAM at the API boundary. There is
  no byte access or floating-point access to word-only memory.
- `NaviSyncRecorder.h` relocates the existing 8 consumption batches, 24 action
  records, 24 native Hall batches and 64 NAVI records. Their wire formats, CRCs, capacities,
  ordering and overflow behavior are unchanged. The backing allocation totals
  **42,688 B**. Allocation failure still prevents startup with PWM held low.
- `NAVI_EWO_0_5.ino` allocates a **12,288 B** main-task stack instead of 8,192 B,
  responding to the measured small margin. Priorities, task scheduling and
  navigation decisions are unchanged. Startup measurements and bounded
  allocation-failure reports use ROM serial output; additive `diag/navi_status`
  fields expose ordinary-RAM availability and allocation failures.
- The original EWO, COHERENCE R2, map, calibration, detector, candidate model,
  credibility rules, STOP/GO clocks and motor/overlay implementation are not
  changed by this repair. No recorder limit was reduced.

## Software verification

[Completed host checks](completed-host-tests.log) pass: the original eight active test
executables, the new word-storage test, 76 extracted motor/overlay assertions
and 712 transport assertions. The active core includes 223 credibility, 85
DART and 410 integrated assertions. The pre-existing legacy failures remain
documented in [the earlier review](../REVIEW.md); they are not relabelled passes.

The word-storage test covers all bytes of aligned and odd-sized records,
multiple reuse cycles, invalid indices, allocation failure and allocation
release. [The production-recorder differential test](recorder-comparison.json)
compares the actual old and new recorder implementations under identical input:
**773 records / 286,368 bytes are identical**, including native Hall records,
consumption records, action records and CRCs. Overflow counts also match:
29 consumption, 46 action, 25 native Hall and 106 NAVI drops in the deliberately
overflowed test. These are test inputs, not field losses.

An extracted transport-test compilation initially lacked stubs for the new
heap-reporting APIs. Adding those hardware-query stubs resolved the harness
compilation; the final 712 decision-invariance assertions pass unchanged.

[Completed compilation](completed-compile.log) passes ESP32 **3.3.12**, with format
warnings treated as errors: **1,057,275 B program**, **81,412 B static RAM**.
Inherited INA219/token-alias warnings remain in the sanitized log. Private
credentials and raw flash images are not committed.

[Completed build provenance](completed-build-provenance.json) records every production
source hash and binary identity. Application SHA-256:
`44a1088adb92ca56a5f5e4d0fa1dcaa62ce13b7ebba1abf2cf0239d87588c4d5`.
The maximum-width resource-status JSON is 637 bytes including its terminator,
within the existing 650-byte buffer.

## Hardware verification

[Upload hashes verified](completed-upload.log). The exact completed image is
now installed, including the approved manual STOP/GO clock behavior. No motion
commands were issued. [Actual Pi recorder readback](completed-pi-proof.log),
not merely a successful publish call, records both new boot identities:

| Measured result | First boot, receiver absent | Second boot, receiver present |
|---|---:|---:|
| Boot ID | `645456DDC3FCE5AB` | `109584913DF9565C` |
| Pi record span | 81.323 s | 70.329 s |
| Allocation failures | **0** | **0** |
| Minimum sampled ordinary free RAM | 29,204 B | 28,164 B |
| Minimum sampled largest ordinary block | 26,612 B | 23,540 B |
| Main-task minimum unused stack | 4,556 B | 4,608 B |
| Zero-throttle records | 80 | 72 |

The reported whole-internal-heap historical minima were 13,816 and 8,880 B;
the sampled free-RAM values in the table are not continuous minima. The
word-only heap is nearly full (268 B remained immediately after recorder
allocation in this binary), so future storage additions require a fresh budget.
The initial MQTT connection retained about 38 KB of ordinary free RAM,
compared with the original failing build's approximately 5 KB before connection.

[Serial captures](completed-serial.log) and
[repeat-start capture](completed-restart-serial.log) show successful connection
without aborts or allocation-failure messages. [Machine-readable results](completed-result.json)
record the ranges and limitations. These are stationary boot observations,
not a long-duration or moving-train reliability claim.

Startup acquisition/recording losses remain visible: Hall input drops were
709 and 379, native-ring drops 120 and 107, legacy Hall-ring drops 4 and 2, and
publication drops 2 in each boot. None of these counters increased between
the first ten loop-status records and the final record; IR input drops remained
zero. Startup Hall processing latency maxima were 192.147 and 126.369 ms.
These startup losses are not erased or called lossless acquisition. No target
was declared or advanced; the normal initial position declaration is required.

The temporary receive-only [UDP capture](completed-udp.jsonl) checked **21,290
packets with zero CRC failures**, including native Hall, NAVI, consumption and
action records. It covered both the tail of the first boot and the second
boot. Across 365,518 consecutive native-sample gaps, median was 46 µs, p99
864 µs and maximum 4,028 µs. These are individual ADC acquisition intervals
inside the existing five-sample bursts, not detector qualification intervals.
No serial gaps were observed within the received native stream and every
sample reported PWM zero; startup samples overwritten before receipt remain
accounted for by the ring-drop counters. Maximum observed NAVI Hall-processing
duration was about 1.027 ms. Timing was measured at rest and is not an
identical-load comparison with R2.

Both temporary UDP capture and serial observation have ended. The continuing
MQTT recorder was unchanged. No Pi service or dashboard configuration changed.

## Operational limits

- Stationary testing cannot establish navigation or stopping reliability on
  the railway. Historical NAVI labels remain distinct from physical truth.
- INA219 was unavailable in the USB sessions. The operator has been asked
  whether normal battery power is on; normal-power sensor operation must be
  established before clearing a train run.
- EWO still needs an initial location declaration and a complete IR-supported
  lap before previous-lap timing is available. The approved manual STOP/GO
  preservation rule applies once a geographic position has been established.
- The separate native-ADC recorder service was inactive and has not been
  enabled or altered. Temporary diagnostic capture ends when the script exits.
- The exact pre-EWO installed 4-MB R2 image and its verified SHA-256 remain
  available privately for rollback.
