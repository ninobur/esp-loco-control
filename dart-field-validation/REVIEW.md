# NAVI EWO 0 5 integration review — October 14, 2026

**Historical review of commit `4b6fd66`.** The manual STOP/GO decision has since
been settled and tested; see [the deployment follow-up](STOP_GO_AND_OTTO_DEPLOYMENT_20261014.md)
for the current status. Statements below about pending approval and unflashed
firmware describe that earlier review snapshot.

**The settled rules are integrated, compiled and host tested. No locomotive was
flashed or operated. Manual STOP/GO clock handling remains one explicit open
operating decision; this is not a field-ready declaration.**

This implementation keeps geographic judgment within NAVI, preserves sensor
measurements separately from conclusions, and carries measurement disagreement
forward. Throttle timing cannot identify a magnet. Morphology and Type-6 remain
observational. No historical percentage gate, 650 ms rule or PWM-derived
geographic estimate is used.

## Provenance and exact scope

Operational foundation: `NAVI_EWO_0_1_MM045_STOP`, commit
`85a2cf2a16d262abdf7258c60f60508b823913ee`. The first DART increment was renamed
`NAVI_EWO_0_4` and published for review at
`e827b4c860ad5dfeea207dbeb8be8dc82d792266`; this continuation is `NAVI_EWO_0_5`.
The earlier review described an unfinished draft. This report supersedes that
status while retaining the original commit and validation evidence.

[Source verification](source-verification.json) confirms all 31 rollback files
match their manifest and the 0_4, original EWO, COHERENCE and common source trees
are unchanged from the review commit. Original maps, calibration, wire formats,
ADC acquisition, IR acquisition and electrical protection remain unchanged.

Production changes are confined to `firmware/programs/NAVI_EWO_0_5/`:

| File | Change |
|---|---|
| `NaviIntegratedCore.h` | Credibility endpoint tracking; previous-lap timing comparison; stop reasons; consistent measured/unavailable anchors; bounded replay on method changes |
| `NaviDartOperations.h` | Two complete/collecting timing banks, interval provenance and named stop reasons |
| `NaviDartEvidence.h` | Endpoint, method, authority, timing-source and transition diagnostic fields |
| `NaviDartTelemetry.h` | Serialize those fields; distinguish milliseconds' underlying microseconds from distance units |
| `NaviTileTiming.h` (new) | Throttle-only entry-to-entry timing ramp and mid-tile hold |
| `NaviStopOverlay.h` | Read-only execution-state getter; original geometry and executor unchanged |
| `NAVI_EWO_0_5.ino` | Navigation-stop handoff, throttle fallback adapter, bounded telemetry integration |
| `tests/test_ewo_0_5.cpp` (new), `run_tests.sh` | Production-code integration tests and active test runner |

The previously approved `NaviIrCredibility.h` is now used without changing its
criteria. README/catalog and this validation directory record status, decisions,
replay tools, adapter tests, generated test sources, raw results and provenance.
The historical `dart-validation/` package is unchanged.

## Decision correspondence

1. Preserve a native median-five 70×2 opening with its ID, polarity, time,
   reference and latest receiver-available applicable IR evidence at that time.
2. For the expected MM, retain and successively replace qualified candidates
   by closeness. A better candidate after the target selects immediately;
   otherwise the symmetric boundary completes after the Hall processing
   frontier. Ties retain both observations and infer a midpoint.
3. Advance once. Save the chosen measured coordinate, not the later packet or
   a map coordinate. An unavailable measurement remains explicitly unavailable;
   an inferred midpoint is not labelled a real Hall measurement.
4. Assess the actual unique Hall-to-Hall interval against the approved IR
   criteria. Record signed, absolute, prior and cumulative residuals. Keep the
   geographic decision even if one ≥48.260 mm discrepancy withdraws IR.
5. During IR unavailability/withdrawal, compare openings against the immediately
   preceding completed lap's directed interval time. Keep the collecting lap
   separate; do not average or substitute an older lap. Record actual departure
   so a current station restart uses the previous departure-to-landmark duration.
6. After withdrawal, the next independently selected real opening starts a new
   restoration reference. Its independently selected successor may restore IR
   if the applicable new interval is below five pulses. Fresh packets alone do
   not restore authority. The withdrawal interval remains recorded unchanged.
7. A candidate nearer MM+2 with its compatible polarity, or no qualifying
   opening by expected MM+2, requests a stop and report without geographic
   advancement. Missing applicable timing history also requests a stop.
8. The overlay uses selected identity. Mid-tile IR loss holds the current
   commanded PWM. The next selected opening applies entry PWM and starts the
   previous-lap-duration ramp toward the next tile's entry PWM. Elapsed time
   never advances the tile. Existing final braking/dwell/departure continues;
   manual authority, E-stop, electrical protection and navigation failure prevail.

The initial operator declaration identifies an interval, not an exact Hall
coordinate. The inherited first correct-polarity opening establishes the first
physical anchor. The MM+2 coordinate deadline cannot be claimed before that
anchor. No fabricated declaration-to-magnet distance was added.

## Verification results

| Verification | Result and evidence |
|---|---|
| Active host runner, sanitizers and warnings as errors | Eight executables pass; [log](active-0_5-tests.log) |
| Isolated approved IR evaluator | 223 assertions, both signs and exact 10 mm / five-pulse boundaries |
| Original DART scenarios | 85 assertions; causal IR association, selected anchor, polarity, ties, repeated openings, wrap, reversal, stale/inconsistent IR and protected controls |
| Integrated 0_5 production code | 376 assertions; previous-complete-lap isolation, withdrawal/restoration, missing-target stops, preserved pending observations, interval compensation, throttle fallback and diagnostics |
| Actual `.ino` motor/overlay functions | 62 assertions; [runtime log](runtime-test.log) |
| Actual `.ino` queue/MQTT functions | 712 assertions; [transport log](transport-test.log), including absent/full queues, disconnected MQTT and failed publication |
| Baseline/proposed production replay | Six paired fixtures; identical inputs, JSON decoded, every selected measured coordinate equals saved anchor |
| Original broader regressions | Six failing executables retained explicitly; see below |
| ESP32 compile | PASS, core 3.3.12, `esp32:esp32:esp32`; [log](esp32-0_5.log), [provenance](build-provenance.json) |
| Pi MQTT recorder receipt | Two labelled host transition records read back from its actual file; see below |

The throttle adapter test verifies the existing final ramp from 35 to zero at
300 ms/count, five-second zero-PWM dwell, and departure at 150 ms/count despite
unavailable IR. It also verifies mid-tile hold, immediate next-tile entry PWM,
time progression, timer expiry without geographic advancement, E-stop and
navigation-stop priority. These are software execution tests, not stopping
measurements on the railway.

[Regression results](regression-results.json) retain five previously failing
legacy detector/window suites: `test_integrated_core`, `test_review_regressions`,
`test_ir_authority`, `test_first_target_after_declare`, and
`test_pwm_zero_localization`. These already failed the first increment's changed
70×2/dart contract. They are not counted as passes.

`test_dart_invariants` additionally fails its original 250→610 mm expectation.
Its first accepted interval is 50 mm short; the new approved rule withdraws IR.
The new production tests establish both outcomes: without a preceding lap, stop
at MM018; with that lap, re-evaluate the preserved later opening by time and
select MM019 once. The original test is unedited. Other cases after its first
failure are not claimed to have executed in that run. The original operational
adapter and measured-station-stop executables pass; the actual new motor adapter
above is the relevant test of this integration.

## Baseline comparison and physical-evidence limits

[Full paired results](differential-summary.json) and the corresponding CSV and
JSONL files retain the exact decisions.

| Fixture | EWO 0_1 | EWO 0_5 |
|---|---|---|
| September 28 events 254–257 | Two confirmations, through MM127 | Two confirmations, same selected measured coordinates; first waits for boundary |
| September 28 events 425–426 | Two confirmations, through MM110 | One confirmation; MM110 candidate still awaiting its boundary at clip end |
| Same clip continued through 427 | Two confirmations | Two confirmations; retained MM110 coordinate equals saved anchor |
| October 9 early-opening mechanism | No confirmation; one automatic missed advance | Selects preserved early opening; no automatic missed advance |
| October 13 MM017, 1,028 and 1,179 ms repeat facts | No extra advance | Preserves repeats; no extra advance |

September 28 uses recorded ADC batches and IR packets; per-conversion times were
not separately recorded. October 9/13 use explicitly synthetic ADC envelopes
around recorded event facts. Historical NAVI labels are not independently
verified physical positions. The October 13 failed sketch was COHERENCE, not
the EWO baseline. The October 9 ten-marker divergence is not reconstructed or
resolved by this bounded replay.

Replay seeding supplies historical identity/coordinate context without inventing
an earlier selected Hall observation. Consequently the first replay interval
cannot establish a new credibility residual. Production integration tests cover
actual selected endpoints separately. No replay proves field reliability or
that a different throttle command would produce the recorded trajectory.

Magnetic-field opening positions are the navigation reference; visible disc
centres are different landmarks. Existing stopping geometry is unchanged.

## Resources, timing and diagnostic load

| Metric | Earlier first increment | 0_5 |
|---|---:|---:|
| Program bytes, core 3.3.12 | 1,033,063 | 1,053,907 |
| Static RAM bytes | 67,076 | 80,668 |
| Host NAVI object bytes | 15,528 | 29,032 |
| Host diagnostic-record bytes | 184 | 328 |

The 0_5 ESP32 NAVI symbol is 28,992 bytes. The compiler reports 247,012 bytes
remaining before runtime heap, queues and task allocations. This is not measured
free heap. The two-bank timing history is 10,992 host-layout bytes. The separate
eight-record diagnostic queue requests 2,624 payload bytes using that layout;
MQTT buffer is 2,304 bytes. The existing large publish and sensor queues are
unchanged. New timing/candidate/diagnostic storage is bounded; the inherited
spatial-reference implementation still has its existing temporary vector.

[Host benchmark](host-benchmark.json): identical 50,000 quiet native samples,
10,000 five-sample batches; mean 104.2 ns baseline and 47.2 ns 0_5, maxima 667 ns
and 500 ns. This single optimized Mac run is neither ESP32 timing nor a field
speedup claim. Native sampling code remains unchanged. Actual ESP32 sampling
jitter, processing latency under load, stack/heap headroom and control-response
timing require an authorized hardware run; existing counters remain available.

[Replay traffic](diagnostic-traffic.json): 76 decision records / 90,571 payload
bytes in 254–257; 20 / 23,838 in the short 425–426 segment. Largest replay record
is 1,279 bytes. A widest-numeric-field formatter test is 1,807 bytes within the
2,048-byte buffer; truncation is detected explicitly. MQTT framing, periodic
status and existing native telemetry are additional. At most two decision
records are sent per network service. Diagnostic overflow/failure counts loss
without altering decisions. Navigation-history overflow is a separate inability
to complete a comparison and stops navigation. Replay pending high-water is
17 of 32; this does not establish hardware queue headroom.

## Actual Pi receipt

[Receipt result](mqtt-recorder-result.json), [sent records](mqtt-host-validation-sent.jsonl)
and [read-back proof](mqtt-pi-recorder-proof.log) preserve two production-formatted
withdrawal/restoration records. Both were read from
`/home/david/NGR/telemetry/all_20261014.log` under
`ngr/loco/9950011/diag/navi_decision`.

They were nonretained, explicitly labelled `HOST_VALIDATION_NOT_LOCOMOTIVE`,
with `field_run=false` and a unique bench ID. This proves Mac→MQTT→Pi recorder
receipt, not ESP32 Wi-Fi delivery. No control message or service change was
made. `ngr-runlog.service` was active; the separate native ADC service
`ngr-navi-sync-record.service` was observed inactive and left unchanged.

## Remaining decision and field prerequisites

The previous-lap rule, IR thresholds/restoration, MM+2 stop and mid-tile hold
are settled in [the operator decisions](OPERATING_DECISIONS_20261014.md).

**Manual STOP/GO during timing fallback remains pending:** pause/resume both
arrival and throttle clocks, or require a new declaration? The pre-existing
draft currently invalidates the timing context. It is preserved visibly pending
an answer, not presented as an approved policy. No default has been silently
chosen in response to the latest clarification.

A boot has no completed previous lap. If IR becomes unusable before one is
collected, NAVI stops rather than fabricating a reference. A reversed direction,
invalid/inferred interval or incomplete lap cannot supply missing timing data.
Timing reproducibility and both provisional IR discrepancy limits require field
assessment; they do not adapt themselves. Startup lacks an exact Hall anchor
until the first accepted opening.

After the STOP/GO answer is implemented and tested, a separately authorized
bench flash would still need actual ESP32 acquisition/queue/memory measurements,
protection response and board-originated MQTT receipt. Native ADC recording must
be available for a useful field comparison. A subsequent supervised normal-IR
lap should establish timing before a separately authorized degraded-IR test.
No flash, physical operation or live-service modification is authorized here.
