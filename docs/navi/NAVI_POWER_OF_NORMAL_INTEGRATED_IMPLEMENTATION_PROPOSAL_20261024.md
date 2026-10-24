# NAVI “Power of Normal” integrated firmware proposal

**Status:** design proposal for operator review. No firmware has been created or changed.

**Governing sources:** `NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md` at `9a6bbb7b`; independent host-sketch audit `NAVI_HOST_SKETCH_BEHAVIORAL_AUDIT_20261024.md` at `160945ab`; audited baseline `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1 at `5b7a518b`.

## 1. Architecture understanding

NAVI is a sequential target-acquisition system. At any instant it has one confirmed location and one legitimate next mapped magnet. Map polarity and spacing plus recent movement evidence predict where and when that target should arrive. That prediction narrows the geographic expectation; it does not identify a magnet or replace Hall evidence.

Hall acquisition runs continuously, including while stopped or in a NAVI hold. A 70-count excursion on two consecutive native observations opens a candidate and preserves the pre-trigger samples. NAVI then waits for the Hall signal itself to return to baseline, and evaluates the completed record retrospectively for opening polarity and a minimum physically plausible width. Shape fitting may describe the record but cannot veto it. A qualifying encounter at the one expected target confirms it and advances the route exactly one marker. A wrong-polarity, too-short, or otherwise nonqualifying encounter leaves the same target pending; it does not cause a map search, skip, contradiction stop, or recovery.

Each mapped interval has its own pace-normalized latest permissible arrival. Only expiration of that boundary without confirmation may issue NAVI's protective stop-and-hold. Position remains the last confirmed marker, and Hall sampling/recording continues during the hold. This arrival deadline is distinct from station braking, operator stop, emergency stop, battery protection, and convoy holds, all of which retain their existing operational motor authority.

The conference does not settle the minimum width, the empirical boundary estimator, pace-scaling limits, or how onset-detector variants compare. These require evidence and operator approval before automatic operation.

## 2. Proposed components and authority

```text
Hall ADC (native timed samples)
  ├──> unconditional recorder / diagnostics
  └──> ordered Hall observation stream
        ├──> onset sentinel + pre-trigger ring
        ├──> retrospective candidate completion (observed RTB)
        ├──> qualification: polarity + approved minimum width
        └──> ExpectedTarget evaluator <── map + movement-time model
                 ├── qualified at expected target -> advance one position
                 ├── nonqualifying -> keep target pending
                 └── interval deadline expires -> NAVI stop-and-hold request

Confirmed-position snapshot (read-only)
  └──> station / grade / bubble operations -> motor request arbiter

Manual / AUTO / station / bubble / low-voltage / E-stop -> motor arbiter
```

| Component | Owns state and responsibility | Geographic authority |
|---|---|---|
| `HallAcquisition` | ADC values, native timestamps, sample serials, direction/PWM context; delivers every observation in order | None |
| `HallRecorder` | Raw sample batches, sequence/gap diagnostics, operational context | None |
| `MagneticEncounter` | Rolling baseline estimator, 70x2 onset state, preserved pre-trigger samples, candidate extrema/polarity, observed RTB and width | None; emits completed candidate evidence |
| `ExpectedTarget` | Current confirmed marker, direction, the single next marker, candidate result and per-interval timing state | **Sole writer** of confirmed position; advances only on qualifying expected-target confirmation |
| `ArrivalBoundary` | Expected interval identity, movement-derived elapsed/residence evidence, pace normalization and latest permissible arrival | No position writes. Sole NAVI source of the stop-and-hold request, after approved boundary expiry |
| `RouteMap` | Static polarity, adjacency, spacing, station locations | Declaration only; no runtime position mutation |
| `LocoOperations` | Manual/AUTO modes, station/bubble state, ramping, comms, battery, E-stop | Receives read-only confirmed-position snapshots. Cannot write NAVI location |

The navigation task should own `MagneticEncounter`, `ExpectedTarget`, and `ArrivalBoundary` state. Other tasks submit operator declarations through a bounded control queue and read consistent snapshots. Only an accepted declaration can establish initial geographic position; an encounter must never directly mutate a second copy of position. The motor task consumes NAVI stop requests alongside all existing motor authorities, with E-stop and low-voltage protection retaining their existing precedence.

Successive encounters are deliberately simple: expected marker `k` is armed; raw observations produce candidate evidence; a qualifying candidate whose expected position is `k` advances confirmed position to `k` and arms `k+dir`; nonqualifying candidates are recorded and discarded as candidates while `k` remains pending; if the empirically bounded arrival for `k` expires first, the last confirmed position stays unchanged and NAVI requests hold. While held, observation and recording continue; any subsequent re-entry/confirmation behavior must follow the approved arrival-hold decision below.

## 3. End-to-end observation and geographic process

1. **Acquire:** timestamp each native Hall ADC read from the monotonic hardware clock, assign a sequence number, and publish it to both the evidence recorder and ordered navigation stream. Include motor/direction metadata for analysis, but do not use PWM-off state to suppress Hall observations.
2. **Preserve:** retain enough preceding samples to include baseline context when two consecutive samples first meet the 70-count onset sentinel. Record both samples and the candidate start time. Gaps, timestamp irregularity, and queue pressure are explicit evidence fields, not silently repaired samples.
3. **Complete from Hall evidence:** continue collecting until observed return to the approved baseline region establishes RTB. A time estimate may report lateness but must not declare waveform completion. Analyze the whole candidate after RTB.
4. **Qualify:** determine opening polarity from the candidate's principal peak under the agreed definition, measure candidate width from observed onset/RTB, and compare it to a minimum derived from the fastest physically plausible movement plus margin. Record shape descriptors diagnostically only.
5. **Evaluate the one target:** compare qualification with `ExpectedTarget`'s sole mapped next marker and expected neighborhood/time. A qualified, expected encounter confirms and advances once. Any nonqualifying candidate leaves the expected marker unchanged and observation continues.
6. **Apply the boundary:** independently compute interval-specific latest permissible arrival using approved pace normalization and empirical bounds. On expiry without confirmation, issue the NAVI hold request, retain last confirmed geographic position, and keep sampling. Do not turn a wrong polarity or fitted-shape score into a stop.
7. **Supply operations:** publish a read-only snapshot `{confirmed_mm, next_mm, direction, tracking/hold state, evidence age}`. Station stopping, grade operation, and bubble coordination consume this snapshot. Peer state and station completion are never alternative sources of local position.

The existing source provides useful timing context but not a substitute for replay of native waveforms: the audited acquisition reads Hall at 5 kHz; the r5.1 recognizer receives 20 ms means (nominally 100 reads), which cannot implement a two-consecutive-native-sample onset. The conference reports that the historical onset timestamp predicts arrival with 5.5–5.9 ms MAE versus 71–87 ms for a uniform baseline across about 171 Otto positions in both directions; Toby automatic data has 69 ms MAE and 1.86% CV. It also reports that two Otto waveform onset-to-peak and peak-to-RTB portions split almost 50/50. These measurements support using movement history to predict the neighborhood and arrival time, while the Hall waveform remains the confirmation evidence. Exact representative sample traces and replay labels should be attached to the implementation verification package before thresholds are frozen; the governing record does not publish sample-by-sample ADC sequences.

## 4. Reuse / adaptation / replacement inventory

Assessment is against the audited r5.1 commit, with the conference as the behavior authority.

| Source in audited sketch | Proposal |
|---|---|
| `LL_LocoConfig_9950011.h`, `LL_LocoConfig_9950012.h` | Reuse hardware pin, PWM, battery, and motor-profile values after confirming hardware revision. Do not carry forward RVM/continuity/model settings as NAVI settings. |
| `NaviEstop.h`, ordered E-stop behavior | Reuse unchanged if interface/precedence remain intact; regression-test wired/command stop behavior. |
| `RvmOps.h` command parsing and manual/AUTO admissibility | Reuse parsing/admissibility functions where independent of RVM state; adapt the `positionKnown` input to the new immutable NAVI snapshot. |
| `RvmStations.h` station geometry, station dwell, bubble coordination | Preserve station definitions and operational state machines. Adapt input to read-only confirmed position and make explicit that peer/station states cannot write local geography. Verify expected behavior on NAVI arrival hold. |
| Main sketch motor writer/ramp, MQTT/ESP-NOW transport, battery monitoring, startup checks | Preserve the working operation paths and single motor write funnel. Integrate NAVI hold as a distinct request; preserve E-stop and low-voltage overrides. Do not let comms callbacks own navigation state. |
| `RouteMap.h` polarity, adjacency, spacing, landmarks and station table | Reuse surveyed route data after independent consistency check. Retain only geometry and needed operational profiles; keep open-loop PWM policy separate from NAVI prediction. |
| `NaviSyncRecorder.h` storage, timestamps, CRC/batch mechanics | Reuse recorder mechanics where possible; adapt schema to capture onset, candidate, RTB, qualification, expected marker, pace/deadline, and stop events. Recorder failure/overflow policy needs approval if it can make the observer lose samples. |
| `RvmTracing.h` 20 ms block means | Do not use as the authoritative recognition input. It may remain as a diagnostic/compatibility derived stream; new recognizer requires native-sample timing for 70x2 and RTB evidence. |
| `RvmShape.h`, `RvmShapeModel.h` | Replace. DCT/Gaussian-model admission, fitted-window selection, 70-count completed-window gate, and half-window delay do not implement the agreed onset/observed-completion cycle. Shape measurements become diagnostics only. |
| `RvmContinuity.h` and `RVM_CONTINUITY_ENABLED` behavior | Replace/remove from navigation observation path. PWM-based pauses and flat off-baseline withholding violate unconditional observation; baseline handling must be redesigned without dropping observations. |
| `RvmNavigator.h` | Replace with a small `ExpectedTarget` state holder implementing confirm-one-or-wait plus a separate `ArrivalBoundary`. Its immediate wrong-polarity contradiction and stop semantics are incompatible. |
| `LocoConfig.h` RVM-specific explanatory/build macros | Replace with a clean NAVI profile that cannot accidentally enable legacy recognizer or continuity gates. |

Main dependency hazard: copying the old include graph wholesale would silently retain `RvmContinuity::admit`, `feedRecognizer` shape admission, or the navigator contradiction transition even if a new class were added. The clean sketch must have no compile-time or runtime path from navigation observations through these components. Add a source-level dependency check to implementation review.

## 5. Decisions and approval register

### Established by the conference

- Exactly one next geographic target; no alternative identity search or recovery.
- Hall observations are continuous and unconditional, including during stop-and-hold.
- 70-count excursion across two consecutive samples is the onset sentinel.
- Retrospective evaluation after observed Hall return-to-baseline completion.
- Polarity and a minimum physically plausible width are qualification criteria; general shape is diagnostic only.
- Nonqualifying evidence leaves the same target pending; it is not a stop trigger.
- An empirically established, pace-normalized, interval-specific latest arrival is the sole NAVI stop-and-hold authority.
- Hold retains last confirmed position and continues Hall recording.

### Ordinary engineering choices (document and verify, no behavior change)

- Fixed-size ring buffers and preallocated candidate storage; monotonic 64-bit timestamps; sequence IDs; bounded task queues; lock-free/critical-section handoff as supported by ESP32.
- A single task owns geographic state; other subsystems receive immutable snapshots.
- Separate raw, candidate, confirmation, and arrival-deadline records with versioned schema and replay tooling.
- Static route lookup and wraparound arithmetic covered at route endpoints; no heap allocation in the acquisition path.

### Architectural choices requiring approval before implementation

| Choice | Alternatives and consequence | Recommendation |
|---|---|---|
| Minimum physical width | Fixed sample count vs duration vs speed-adjusted distance. Too high rejects real magnets; too low admits short non-magnet excursions. | Derive a lower bound from fastest plausible physical passage and measurement margin using native recordings across directions/speeds; no value is set here. |
| Baseline and RTB definition | Fixed declared baseline/noise band vs continuously adapted local baseline; symmetric band vs robust/noise-aware band; possible hysteresis to prevent chatter. This defines onset amplitude and observed completion. | Replay both declared and continuous estimators on raw records; choose the simplest estimator that preserves real events without withholding observations. Operator approves thresholds and baseline ownership. |
| Peak / opening-polarity definition | Raw maximum/minimum vs filtered/robust peak, and how multiple lobes identify the principal peak. Wrong definition can reverse or misattribute an encounter. | Use a retrospective, minimally processed peak rule; compare against labeled historical events, including rebounds, before approval. Filtering remains diagnostic unless separately approved. |
| Movement-time predictor | Historical onset time, speed estimate from route spacing/lap timing, or a blended predictor. The conference supports movement-derived timing but not one formula. | Begin with the conference-supported historical timing reference and compare a simple pace-scaled model in replay; report per-direction/per-interval errors. Do not let timing create magnetic confirmation. |
| Latest-arrival estimator and pace limits | Empirical quantile/nonparametric bound, held-out tolerance bound, or another validated method; three-sigma was rejected. Pace scaling can hide traction loss if its own degraded history expands the bound. | Select only after replay with held-out laps and route/direction coverage. Put a ceiling/floor on pace adaptation only if data supports it; operator must approve both estimator and degraded-pace protection. |
| What happens after boundary hold | Latch until operator redeclares, or resume waiting if a late qualifying encounter arrives while movement is stopped/coasting. Conference says hold retains location and continues observing, but does not define whether late evidence may clear hold. | Default proposal: hold remains latched until an explicit operator action; a late candidate may be recorded but cannot silently restart movement. Approve whether it can confirm geography while held and what command releases movement. |
| Observation-integrity failure | Continue with a flagged gap, inhibit recognition, or request an operational hold after loss/overflow. Each choice can affect the conference's sole-stop rule or allow a missed target. | Guarantee adequately sized ordered buffers and measure worst-case task load first. If loss still occurs, flag the interval and present an explicit policy for approval; do not silently drop samples or invent a new stop trigger. |
| Initial position / declaration contract | Declare last and next marker; or declare standing marker plus direction and derive the next marker. Existing r5.1 has multiple declaration command forms. | Preserve the established operator workflow where unambiguous, but issue one canonical declaration object and require zero applied/commanded motion; approve exact semantics before coding. |

## 6. Source organization and integration plan

Create a clean sketch, e.g. `firmware/programs/NAVI_POWER_OF_NORMAL_INTEGRATED/`, with no dependency on `RvmShape*`, `RvmContinuity`, or `RvmNavigator`:

- `NAVI_POWER_OF_NORMAL_INTEGRATED.ino` — task startup, queues, command routing, snapshot publication, operational integration.
- `NaviHallAcquisition.h` — ordered native samples, timestamps, sequence/gap metadata.
- `NaviHallRecorder.h` — raw/candidate/decision evidence records; adapt proven ring/CRC transport.
- `NaviMagneticEncounter.h` — baseline, 70x2 onset, pre-trigger retention, candidate RTB and qualification measurements; threshold values injected from a calibration profile.
- `NaviExpectedTarget.h` — sole geographic position writer and one-target confirm/continue state machine.
- `NaviArrivalBoundary.h` — pace-normalized expected arrival and protective hold request; no waveform or position authority.
- `NaviRouteMap.h` — verified route polarity, spacing, station geometry.
- `NaviOpsBridge.h` — read-only position snapshot and NAVI hold request to station/AUTO/motor arbiter.
- Existing independent operational modules adapted/reused: hardware profiles, operator parsing, ordered E-stop, station/bubble driver, MQTT/ESP-NOW, battery checks, ramp/motor writer.

Interfaces should be narrow: acquisition publishes `HallSample{adc,t_us,sequence,direction,pwm}`; encounter emits `CompletedCandidate{start_us,rtb_us,peak,polarity,width,sample_quality,diagnostics}`; target evaluator consumes candidate + expected-neighborhood context and emits `ConfirmedPosition` only on an approved match; arrival module emits `NaviHoldRequest{interval,deadline,evidence}`; operations consume the latest immutable `NaviSnapshot` and `NaviHoldRequest`.

## 7. Phased implementation and verification

1. **Approve open decisions and freeze evidence.** Assemble provenance-checked Otto/Toby native Hall records, map/version identity, movement direction/PWM, labels, and the MM110/146/169/113 documented rejection cases. Identify missing raw waveforms rather than treating undocumented events as replayable.
2. **Offline replay first.** Implement a reference cycle in host-testable code. Run onset variants and baseline/RTB candidates against recorded waveforms; measure false openings, event completion, peak polarity, widths, expected-location error, interval residuals, and deadline misses. Keep train/lap holdouts separate from calibration.
3. **Pure unit tests.** Cover 70x2 and one-sample non-trigger, pre-trigger retention, noise/baseline shifts, observed RTB, incomplete excursions, both polarities, minimum-width boundary cases, multi-lobe cases as approved, nonqualifying-no-advance, exact one-marker advance, route wraparound, pace calculations, deadline expiry, and held-position invariance.
4. **Recorder/acquisition tests.** Validate native timestamp monotonicity, serial continuity, queue capacity under worst-case load, raw recording through motor-off and hold, and replay equivalence. Deliberately stress transport/recorder separately from the recognition feed.
5. **Operational integration tests.** Compile and exercise manual throttle, direction lockout, AUTO/GO/STOP transitions, station approach/dwell/release, bubble hold/release, comms loss, battery protection, ramp behavior, and ordered emergency stop. Assert station/peer/motor code cannot write NAVI geography. Assert NAVI deadline requests preserve the last confirmed marker and Hall acquisition remains active.
6. **Firmware build.** Compile the clean sketch for ESP32 Arduino with the same board profile and required libraries (Arduino core/FreeRTOS, Wi-Fi/UDP, PubSubClient, Wire, INA219, ESP-NOW). Build Otto and Toby profiles, record compiler/core/library versions, static memory, queue budgets, stack high-water marks, and task timing.
7. **Controlled field progression.** Bench sensor/magnet sweeps; restrained low-speed single-locomotive runs in both directions; normal speed/consist coverage; repeated interval calibration; then two-locomotive station/bubble integration. Start with NAVI observation and motor operation disabled from NAVI requests. Compare all candidate, confirmation, and deadline records to independent raw traces before enabling automatic movement.

Before authorizing AUTO, require: approved calibration and boundary method with held-out per-interval results; correct sequential geographic traces in both directions; no unreported native-sample loss; successful regression of every retained locomotive operation; verified hold and E-stop precedence; field evidence across supported locomotives, speed and consist ranges; and operator sign-off of the open decisions.

## 8. Operator decisions requested

1. Approve or amend the architecture and reuse boundaries in this proposal.
2. Approve the decision register choices, especially the minimum-width/RTB policy, arrival-boundary method and pace-degradation limit, observation-integrity policy, and whether a late event can confirm position while held.
3. Confirm the initial declaration command semantics and whether existing operator command topics must remain wire-compatible.
4. After those decisions, explicitly authorize implementation in the proposed clean sketch. Until then this document is the completed deliverable; no firmware changes are authorized by the pasted assignment.
