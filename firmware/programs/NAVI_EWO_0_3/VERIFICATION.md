# NAVI_EWO_0_3 verification record

2026-10-11. Complete sketch: [NAVI_EWO_0_3.ino](NAVI_EWO_0_3.ino).
Review branch: `codex/navi-ewo-0-3-complete`, descended from
`sam/navi-ewo-0-3-initial-restoration` at `60dd1bc`.
Operational shell foundation: `85a2cf2a16d262abdf7258c60f60508b823913ee`.
This record establishes a software review candidate, not track acceptance.

## Reproducible host verification

```sh
sh firmware/programs/NAVI_EWO_0_3/run_tests.sh
```

GCC 13.3.0, C++17, `-Wall -Wextra -Werror`, AddressSanitizer and
UndefinedBehaviorSanitizer. LeakSanitizer is disabled because this container
cannot inspect process tasks; ASAN/UBSAN remain enabled. No assertion was removed
from the existing operational suite to obtain a pass.

| Verification | Result |
| --- | --- |
| Complete active 0.1/EYES_WIDE_OPEN host suite | 12 executable runs pass |
| New 0.3 evidence, IR, judgment, bridge, navigation and pulse tests | 6 executable runs pass |
| Existing map (both polarity profiles), recorder, STOP and IR configuration assertions against 0.3 headers | 5 executable runs pass; include paths alone redirected |
| Existing `tools/tests` regression suite | 14 unittest methods pass |
| Actual 0.3 shell STOP/actuator integration | 6 unittest methods pass |
| Actual 0.3 Type-6 ingress and telemetry | 2 unittest methods pass |
| Source preservation against immutable 0.1 commit | 4 unittest methods pass |
| Native radio transport host scenarios | Pass |
| Transport status tests | 5 unittest methods pass |
| Recorded pulse fixtures | 5 verified fixture hashes; 4 nonempty traces; 22,940 events |

Raw output is committed in [verification/host-results.txt](verification/host-results.txt).
The standalone radio test covers immediate/callback failure, timeout, late
callback isolation, recovery, and CRC coverage. Maximum exercised native pulse
JSON is 826 bytes, within the unchanged buffer contract.

## Navigation acceptance coverage

`tests/test_navigation.cpp` calls the real integrated core. Primitive tests call
the same detector and judgment code used by the ESP32 sketch.

| Requirement | Demonstrated software behavior |
| --- | --- |
| 70 counts, two native samples | 69 counts, one sample, serial gaps and cadence gaps cannot qualify; opening and detection times remain separate |
| One physical field | Sustained field cannot retrigger; credible distance excludes a renewed excursion at insufficient displacement; unavailable timing uses 650 ms |
| Opening coordinate | Packet between opening/detection and later queue consumption cannot replace the first sample's IR coordinate; duplicate exclusion also uses that opening |
| Continuous Hall observation | Qualification remains active in spatial clearance/collection, including stalled displacement |
| Sequential identity | Credible IR agreement identifies the next MM; undercount with withdrawn/unavailable IR authority still identifies the next MM |
| IR credibility | Five preceding intervals, unchanged PWM, anomalous interval and normal following bracket question one interval; no replacement pulses or counter edits |
| Telemetry gap | Loss of observability is distinguished from physical pulse loss; PWM change/reversal cannot inherit the old population's credibility |
| Re-anchor and restoration | Every verified opening sets a fresh origin; a subsequent credible pulse measurement restores authority after an anomalous span |
| Continuous DNA | 21 ordinary confirmations retain 12 observations and verify each encounter; checks also run while IR agrees |
| Contradiction | Wrong pole remains in the ledger and reports unreliable geography; ten following poles can resolve one leading contradictory encounter or extra excursion |
| Geographic bounds | A remote valid DNA sequence cannot move the incumbent MM; ten-marker uniqueness checked across every map pair in both directions |
| Declaration | 106-mm first target in both directions, post-declaration onset, repeated declaration, and reversal before first anchor; two reversals restore the original target |
| Reversal | Current-MM return after leaving a field, adjacent target while still in field, retained history, and recovery with unavailable IR |
| Historical sustained-field failure | MM013 with unchanged IR cannot advance merely because time passes |
| Uncertain geography | Pending contradiction reports unreliable location; no new AUTO withdrawal threshold is introduced |
| STOP | Real shell follows DECELERATE → STOP → WAIT → RESTART → ACCELERATE; five-second applied-zero dwell and PWM timing retained |

The declaration/106-mm and stationary-field scenarios are deterministic
reconstructions of recorded failures, not complete raw-Hall field replays.
Retrospective correction is deliberately local: one leading contradictory or
extra observation and ten following sequential poles. Multiple unresolved
encounters or missing physical markers are not repaired by a map-wide search.

## October 9–10 pulse replay

The manifest commits source/fixture SHA-256 hashes, time windows and PWM
provenance. Fixtures contain measured boot, cumulative count, completion time,
last reported PWM and event validity. No replacement pulse is generated.

| Trace | Events | Timing questions | Missing-compatible brackets | Observability gaps |
| --- | ---: | ---: | ---: | ---: |
| October 9 healthy control, 19:31–19:41 | 9,484 | 136 | 17 | 2,061 |
| October 9 first failing onset, 19:05:15–19:05:30 | 275 | 1 | 0 | 127 |
| October 9 second failing onset, 19:50:15–19:50:35 | 333 | 22 | 6 | 21 |
| October 10 12:18 log | 12,848 | 46 | 7 | 3,961 |
| October 10 12:45:55 log | 0 | 0 | 0 | 0 |

The 12:45:55 log contains status information but no usable pulse events; it is
explicitly excluded from meaningful pulse replay. Its empty fixture documents
that exclusion. Logs do not supply the complete raw Hall observations necessary
for magnetic DNA replay. **No successful field DNA replay is claimed.**

Timing questions also occur in the healthy control. These results therefore do
not validate a physical missed-pulse classifier or establish the historical
failure's mechanical cause. Every replay reports zero *proven* physical missed
pulses. A bracket means compatibility with an uncounted cycle; slowdown, slip or
binding may remain possible. Historical PWM is the last one-second report, not
PWM at pulse completion, and absent log records are not proof of absent physical
pulses or receiver deliveries. Review the conservative timing-quality withdrawal
before field acceptance. It changes measurement authority, not cumulative counts,
calibration, speed limits or motor commands.

## Full ESP32 build

Arduino CLI 1.5.2-rc.1, ESP32 Arduino core 3.3.12; board
`esp32:esp32:esp32`, selected inherited Otto profile `9950011`.
Libraries: PubSubClient 2.8, Adafruit INA219 1.2.3, Adafruit BusIO 1.17.4.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-path /tmp/navi-ewo-0-3-build \
  --build-property compiler.cpp.extra_flags=-I/path/to/external/config \
  firmware/programs/NAVI_EWO_0_3
```

The external configuration directory supplies `credentials.h` using the existing
inclusion method. Verification used a copy of the safe repository template,
not operational credential contents. No credentials were displayed or committed.
Build exit status: **0**. Program storage: **1,027,895 / 1,310,720 bytes (78%)**.
Global memory: **62,492 / 327,680 bytes (19%)**, leaving 265,188 bytes.
Exact memory output is committed in
[verification/esp32-build.txt](verification/esp32-build.txt).

## Second source inspection and preservation

After integration, a separate inspection pass compared the entire sketch with
the foundation, examined the core's decision paths, and ran direct source-equality
checks. This is a second pass by the implementing agent, not a claim of external
independent review. It found and corrected population inheritance after PWM/
direction change, decision-time duplicate distance, and double reversal before
the first anchor; regression cases cover those corrections.

Eight operating headers are byte-identical to the immutable foundation:
`NaviStopOverlay.h`, `EwoStationStop.h`, `NaviEstop.h`, `NaviCompatibility.h`,
`NaviMapAdapter.h`, `NaviSyncRecorder.h`, `NaviPulseObservation.h` and
`NaviPulseTelemetry.h`. Actuator/ramp, brake-step, command/manual handling,
battery, MQTT callback, Type-5 ingestion and STOP recording functions are equal.
Operating constants and installed **9,652 um/pulse** remain equal.

The two explicit STOP-shell integration differences are covered by tests:
verified retrospective geography resets a stale overlay instruction; pending
DNA does not become a new AUTO withdrawal threshold. Pending location remains
unreliable in telemetry and the unchanged overlay retains its existing execution
instruction when fine geography is unavailable. AUTO/GO admission, manual and
E-stop behavior retain the existing rules. No speed governor, braking limit or
new performance restriction was added.

Acquisition changes only the Hall sample cadence needed for native 70×2;
Type-6 ingress captures PWM/direction and provides credibility evidence before
Hall judgment. Existing MQTT topics and binary wire layouts remain; navigation
events add boundary metadata and append event/input identifiers. Governing
decision records and the calibration file are unchanged.

## Failures, migrations and exclusions

- Initial baseline builds failed due to one misleading-indentation warning and
  missing Type-7/transport/PWM mocks in the existing Python extraction harness.
  Commit `9dc1df4a3a1ada777754ceb9aae1dfe0d38e055e` repairs those harness defects;
  original assertions are preserved. The repaired complete suite passes.
- The initial 0.3 pulse primitive accepted too little timing context. Its tests
  now require the implemented five-interval population and a following bracket,
  retaining gap/clock/PWM cases. Failed initial expectations are not field proof.
- Original tests that require authoritative median-of-five detection, IR-only
  missed-marker assignment, silently discarded opposite poles or the old
  assumed-origin first-reversal distance window remain historical 0.1 tests.
  They pass against 0.1. Those requirements are superseded by this assignment,
  so they are not claimed as unchanged 0.3 acceptance assertions. New cases test
  the authorized replacement rules. Unchanged applicable map/recorder/STOP/IR
  configuration assertions run against 0.3 as well.
- `test_measured_station_stop.cpp` and `test_operational_adapters.cpp` were
  already retired by the original active-suite script; they reference removed
  station machinery and remain historical evidence. No machinery is reintroduced.
- LeakSanitizer and physical track testing are not performed. The no-pulse log
  and absent raw Hall DNA are explicit replay exclusions, not successful tests.

## Remaining review and operator decisions

Review the conservative timing interpretation against the healthy control and
the bounded retrospective rule. Full physical pulse-loss discrimination, fine
boundary sampling/delivery uncertainty, unresolved multiple encounters and track
performance remain limitations described in [STATUS.md](STATUS.md).
An independent reviewer and separately authorized field validation are needed
before deployment. This work does not merge the operational branch, flash Otto
or Toby, access the Pi, or modify live services.

## Publication provenance

The shell has no GitHub credential helper. The connected GitHub Git-object API
publishes the same reviewed source trees in three logical commits: harness
restoration, firmware integration, and acceptance/verification. The final tree
is compared with the local reviewed tree; no operational ref is updated.
