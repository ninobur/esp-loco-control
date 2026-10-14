# NAVI EWO DART Experiment 1 implementation and validation

**Version naming update, October 14:** this tested increment is now
`firmware/programs/NAVI_EWO_0_4/`. Runtime name and primary file are
`NAVI_EWO_0_4`. Its navigation source matches the verified first-increment
snapshot. The unfinished continuation is `NAVI_EWO_0_5` and does not inherit
this report's validation verdict. Original names in the report and captured
results below are historical; runnable verification scripts now target 0_4.

October 14, 2026. First authorized increment, stopped before committing, deployment or field operation.

**Implemented and host-tested; ESP32 core 3.3.12 compilation passed. Not field ready.** The experiment replaces the old geographic distance gate with the dart comparator, freezes opening-time IR associations, saves the same coordinate it evaluated, and preserves diagnostic evidence. It adds no automatic IR credibility decisions, historical-time navigation, morphology veto or missing-target stop threshold.

## Source and changes

Repository: `ninobur/esp-loco-control`. Exact foundation: `85a2cf2a16d262abdf7258c60f60508b823913ee`, `NAVI_EWO_0_1_MM045_STOP`.

The isolated checkout is `/Users/davidbrown/NGR/NGR-Files/navi-ewo-dart-experiment-1`, detached at the requested commit. New sketch: `firmware/programs/NAVI_EWO_0_1_DART_EXPERIMENT_1/`. The original EWO source and COHERENCE sources were not edited. No maps, calibration, shared firmware headers or operating systems changed. Git LFS downloading was unavailable during checkout; retrying with LFS smudge disabled retained pointers for unrelated captures. The replay reads the already available, hashed September 28 capture, not an LFS pointer.

Production changes in the separate sketch:

- `NaviIntegratedCore.h`: median-of-five tick 70×2 observations; causal IR and spatial-reference association; bounded candidate history and iterative replay; dart selection, midpoint ties and consistent anchors; observational residual and timing records; removal of distance-window admission and distance-only advancement.
- `NaviDartEvidence.h` and `NaviDartTelemetry.h`: bounded evidence structures and versioned diagnostic serialization.
- `NAVI_EWO_0_1_DART_EXPERIMENT_1.ino`: comparison completion after processed Hall evidence, diagnostic context/timing taps, separate compact MQTT diagnostic queue and runtime measurement counters.
- `NaviStopOverlay.h`: selected identity/tile availability exposed separately from fine position. Existing command behavior remains unchanged.
- `tests/test_dart.cpp`, `tests/test_dart_invariants.cpp`, `run_tests.sh`, README and history: authorized behavior tests and experimental status. Other copied headers and inherited tests remain unchanged.

The only modified pre-existing tracked file is `firmware/README.md`, with an experimental catalog entry. `dart-validation/` contains the source checks, production replay adapters, fixtures, logs, manifests and this report. `manifest.json` lists exact created/modified files and hashes; `build-provenance.json` identifies verification binaries, board, options and core. These binaries use placeholder credentials and are not deployment images.

## Decision correspondence

The first target after an authoritative interval declaration follows U1, without a minimum travel requirement. Subsequent targets retain the first eligible opening and replace it only with a closer one. Early candidates set `B = D + |x−D|`; a better post-target opening selects immediately; otherwise completed movement through B selects the retained opening. Exact opposite-side ties save their inferred midpoint and preserve both IDs.

A packet cannot complete comparison ahead of queued Hall evidence. The native acquisition frontier and receiver-time history determine which information was available. A selected opening cannot be reused; later retained openings can be evaluated against the next target without recursive stack growth. Sustained threshold support does not repeatedly open events; real threshold re-entry remains observable. Wrong-polarity observations remain in diagnostics but cannot identify the current target.

Every ordinary selection saves the same immutable measured coordinate used for comparison. An explicit inferred flag distinguishes midpoint anchors. The coordinate association retains IR boot/epoch/sequence, count, source and receive timestamps, and applicability. Source MAC and the underlying wire packet remain in the unchanged IR recorder. Missing coordinates are marked unavailable, not filled from future packets. Anchor mismatch has a counter and diagnostic reason.

The historical percentage gate and `evaluateMissing()` advancement loop are absent from experimental navigation. No replacement distance threshold, timer or stop policy was introduced. No candidate, stale IR, or unavailable coordinate leaves the current identity unresolved; **the existing motor/overlay commands may continue**. Comparison-affecting acquisition loss or pending-buffer overflow is reported as incomplete and cannot justify a commitment. This does not initiate a new stop.

## Tests and failures

`active-regressions.log` records seven passing executables: five applicable inherited suites plus the two new production-code suites. The latter pass **85 scenario assertions and 643 invariant assertions**. They exercise the actual core and detector, including candidate replacement, post-target selection, ties, wrong polarity, repeated openings, first acquisition, stale/missing/inconsistent IR, coordinate association, both directions, route wrap, PWM-zero intervention/redeclaration, motionless dwell, buffered subsequent magnets, residual compensation, timing diagnostics and bounded overflow.

Synthetic exact-distance arithmetic fixtures deliberately set known coordinates inside the production core; they do not alter installed pitch or substitute a second comparator. Native-sample tests independently exercise real packet validity and 70×2 processing.

The exact baseline's inherited runner passes. Of ten distinct inherited suites run unchanged against both versions, all ten pass on the baseline and five pass on the experiment. These five fail on the experiment:

| Suite | Failure disposition |
|---|---|
| `test_integrated_core` | 16 assertions specify rolling native medians, earliest raw anchors, old window/missed progression or setup dependent on those rules |
| `test_review_regressions` | Stops at old single-window first-confirmation setup |
| `test_ir_authority` | Stops at old detector's first-confirmation setup |
| `test_first_target_after_declare` | Stops at a fixture using the replaced sampling/onset contract |
| `test_pwm_zero_localization` | Stops at its old detector setup, before intervention checks |

These failures are preserved in `inherited-results.json` and individual logs, not relabelled as passes. New production-code tests cover the retained intervention, validity, direction and declaration requirements under the authorized detector. The old suites remain intact for historical comparison.

`transport-test.log` records **512 passing assertions** using the actual `publishDart()` and `serviceDartTelemetry()` implementations with mock queue/network outcomes: queue full, queue absent, disconnected MQTT and failed publication. Geographic identity, selected anchor, candidate boundary and Type-5 sequence remain identical under identical navigation inputs. Separate tests inject Type-6 parser failures and diagnostic-consumer starvation. This establishes software isolation; it does not prove ESP32 scheduling immunity under network load.

Source checks confirm unchanged native acquisition, Type-5/Type-6 ingress, command handling, motor ramp/protection routines, map adapter, E-stop adapter, compatibility payloads and native recorder. Existing STOP geometry/ramp/dwell/departure regression passes. Physical control timing still needs hardware measurement.

## Identical-input differential evidence

The two replay executables include the actual baseline and experimental navigation code. Each pair receives the same fixture. Native September 28 segments use original five ADC values per recorded tick and actual Type-5 packet timing/counts. Individual conversion timestamps were not recorded in that format, so all five conversions receive their recorded batch timestamp. Each segment is seeded from a historical identity/reference; those labels are not independent physical truth. The adapters do not emulate a new motor trajectory.

| Case | Baseline | Experiment |
|---|---|---|
| October 9 early-opening mechanism | Rejects the 250.952 mm observation against the 325 mm target; later emits one Hall-free missed advance | Retains the opening; selects at symmetric completion; saves 250.952 mm while latest IR is 405.384 mm; no missed advance |
| October 13 MM017, both repeat timings | EWO baseline already excludes north for south MM018 | Preserves repeated north observations, no extra advance or 650 ms fallback |
| September 28 254–257 segment | Two confirmations, ending MM127 | Two selections, ending MM127; no missed advance |
| September 28 425–426 segment through 427 +200 ms | Two confirmations, ending MM110 | MM109 selected; MM110 candidate still pending, progress 1.576 mm short of its boundary |
| Separately labelled continuation through 427 +1 s | MM110 | Selects MM110 after the next applicable movement update; 259.057 ms after its chosen opening |

October 9/13 have no recovered continuous ADC. Their fixtures use explicitly synthetic threshold envelopes around the reported polarity, count and timing facts. They demonstrate code mechanisms, not reconstructed field waveforms. The October 13 failed firmware was COHERENCE, not this EWO baseline. The October 9 approximate ten-marker divergence and later paired physical reports are not reproduced or resolved by this bounded test.

For MM127, both EWO versions use a later qualifying opening than historical event257: their evolving reference is 2074, while the old event used a locked reference2007. The experiment preserves the target selection but does not reproduce the historical opening coordinate. No threshold was tuned to force agreement. This reference difference and its field consequence remain evidence for review.

Selected-coordinate equality is checked for every ordinary replay selection. The synthetic October 9 result is evaluated/saved **250,952 µm**, despite latest IR **405,384 µm**. In the native replay, MM126 is evaluated/saved **91,134,184 µm**, despite latest IR **91,192,096 µm** at commitment. No mismatch was observed. Tests cover inferred midpoint handling separately.

These are conditional software results, not a field reliability, false-exclusion or impostor-acceptance rate.

## Overlay and coordinate convention

`StopView` preserves geographic identity when fine IR disappears, while retaining the old `available`/`fine` command semantics. No identity-only throttle policy is activated. A selected MM is exposed without filling a missing offset with zero. The command continues its existing behavior, including holding its previous instruction where geography was previously unavailable.

The MM045 +150 mm target, twelve-tile architecture, preset values, grade settings, final ramp, dwell/departure and manual/electrical protections remain unchanged. Hall opening coordinates are not disc-center coordinates. The apparent physical stopping discrepancy does not authorize an offset correction. A future identity-only operating policy still needs separate approval.

## Resource and recording evidence

Both builds compile with **ESP32 core3.3.12**, FQBN `esp32:esp32:esp32`, using the same options and credential template. Logs are retained. Earlier clean compilation showed warnings in the installed Adafruit INA219 dependency; no project-source warning was reported.

| Compile metric | Baseline | Experiment | Change |
|---|---:|---:|---:|
| Program storage | 1,018,847 B | 1,033,063 B | +14,216 B |
| Global/static RAM | 58,124 B | 67,076 B | +8,952 B |

Host core object size is 6,592→15,528 bytes. The ESP32 symbol sizes are recorded in `esp32-core-symbol.txt`. The separate transport queue requests eight diagnostic records (1,472 bytes of payload, plus queue overhead). These are allocation requirements, not measured runtime free heap. No existing large publish queue was enlarged.

An identical-input, optimized host microbenchmark over50,000 native samples recorded mean five-sample processing times of247.9 ns baseline and147.7 ns experiment, with maxima19.584 µs and35.958 µs respectively. This is a single macOS quiet-sample measurement, including host scheduling variation; it is not ESP32 timing or a speedup claim. The actual loaded replay reports pending-history high-water17 in the254–257 segment and6 in425–427, with no comparison-history overflow. Stress tests explicitly exercise overflow.

`diagnostic-traffic.json` measures serialized payloads from the production formatter:76 records and45,721 payload bytes for254–257;20 records and12,108 bytes for the shorter425–426 segment. MQTT framing, periodic status and existing native telemetry are additional. Decision records use a separate eight-item queue, at most two sends per network service; loss is counted. Existing acquisition/recording formats remain intact.

Runtime counters were added for processing duration, Hall consumption latency, incomplete batches, pending high-water/overflow, diagnostic queue/publication loss, anchor mismatches, static core size, free/minimum heap and loop-task stack high-water. Actual ESP32 sampling jitter, runtime queue occupancy, largest free allocation, protection latency and recording under reconnect load remain unmeasured. No new binary was flashed, and no live Pi service was operated. Actual Pi receipt is therefore **not verified**; a future authorized bench check must read records from the recorder, not rely on publish success.

## Remaining decisions and limits

- Approve missing-target search completion and stopping. This build may continue motor operation with unresolved navigation; no new stopping threshold is hidden in it.
- Define and measure IR credibility withdrawal/restoration. Residuals are observations only; zero-valued invalid residual slots are explicitly flagged.
- Validate historical arrival-time repeatability before granting authority. The bounded ring records directed intervals/context and departure time; it contains no prediction or timing-navigation code. An uninterrupted interval flag does not certify physical identities.
- Approve any coarse identity-only overlay operating behavior. The new interface exposes identity but deliberately does not invent a new command under missing fine position.
- Measure actual ESP32 resource headroom and acquisition/control timing; verify Pi recording; review selection latency and reference convention in a separately authorized bounded field test.

No governing algorithm was replaced with a new threshold or policy. The latest limited authorization intentionally excludes the broader plan's automatic credibility, timing authority and stop proposals. The first increment is available for review, not deployment. No commit, push, merge, flash or live operation occurred.
