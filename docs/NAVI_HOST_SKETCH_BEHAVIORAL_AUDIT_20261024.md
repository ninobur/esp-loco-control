# NAVI host-sketch behavioral and vestigial-code audit

**Audited baseline:** `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1, commit
`5b7a518b` (object verified locally).  
**Architectural authority:** `docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md`,
commit `9a6bbb7b`.  
**Scope:** the sketch and all of its navigation-relevant included local headers
at `firmware/test-programs/NAVI_RVM_AUTO_STATIONS_BUBBLE/`. This is a
read-only source audit; no firmware was modified.

The named r5.1 worktree was not present locally, but both specified Git
objects were. All source citations below refer to paths and one-based lines in
the stated commit, not a newer checkout. The prior comparison finding was also
read from its stated documentation-only successor `cd5a6d6`.

## Executive assessment

**No: r5.1 is not a suitable unmodified foundation for the agreed NAVI model.**
It has a single map-derived next marker once declared, and no geographic
search/recovery routine was found. However, its active recognition path is the
older RVM statistical shape classifier, not `Expect -> Observe -> Qualify ->
Confirm`. Its Gaussian-mixture likelihood is an admission gate; only a
shape-selected window can reach the navigator. This directly contradicts
Proposition 7. It also does not implement the required 70-count,
two-consecutive-sample onset: it uses a 70-count amplitude gate over resampled
20-ms blocks.

Additionally, r5.1 deliberately excludes motor-off Hall blocks and can
withhold motor-on flat, off-baseline blocks from the recognizer. Raw recording
continues, but the navigation observation stream is conditional. The
previously reported baseline-discard behavior is real and follows directly
from this live control path.

Before any NAVI integration, resolve the critical recognition and observation
authority conflicts below. Retain the separately legitimate motor safety,
manual-control, station, and convoy functions, while defining their interfaces
to the new recognizer.

## Behavioral-authority inventory

| Component and reachability | Inputs / decision | Authority and consequence | Classification |
|---|---|---|---|
| `hallTask()` — sketch lines 253-287 | ADC at 5 kHz; current direction and PWM metadata | Always records native samples and emits 20-ms mean blocks. A full `blockQ` drops a block for NAVI (`xQueueSend(...,0)`), while recording proceeds. | Active acquisition; conditional NAVI delivery risk. |
| `HallContinuity::admit()` — `RvmContinuity.h`:121-155, 199-227 | block Hall, block PWM, 50-block range, baseline level/noise, declaration/ensemble state | Rejects every block with PWM <15 from the navigation waveform; withholds flat off-baseline motor-on blocks. It may apply a declaration while paused. | Active recognition gate; affects baseline and what can be recognized. |
| `Recognizer` — `RvmShape.h`:64-233 | admitted 20-ms block means, direction, local baseline | Scores multi-width windows using Gaussian-mixture LLR. Only `LLR > 0` and amplitude >=70 may be held and accepted after a half-width delay. Accepting changes the next local baseline. | Active morphology/shape admission authority. |
| `Navigator` — `RvmNavigator.h`:33-56 | operator declaration; accepted ensemble polarity; route map | The only automatic geographic advancement: correct polarity advances `last_` to `next_`; wrong polarity changes state to `Contradiction`. | Active geographic authority. |
| `serviceAuto()` / `StationDriver` — sketch:555-579; `RvmStations.h`:342-504 | navigator snapshot, station/bubble state, peer radio packet, motor state | Uses committed `lastMm` for grade/station stop outputs and withdraws AUTO/requests PWM zero when navigation is not tracking. It does not alter NAVI position. | Active and legitimate operational motor authority. |
| command and safety paths — sketch:602-703; `RvmOps.h`:103-137; `NaviEstop.h` | MQTT/dispatcher commands, battery voltage, ordered E-stop | Manual throttle, stop, AUTO/GO, direction and declaration; E-stop and low voltage can force PWM zero. Direction/session changes suspend NAVI. | Active, legitimate non-NAVI authority. |
| `RouteMap` — `RouteMap.h`:16-69, 181-198 | static route map and `lastMm` | Supplies polarity, adjacency, spacing, and location-dependent cruise PWM. It cannot itself advance position. | Active map/operational support. |
| `NaviSyncRecorder` — sketch:253-287, 739-770 | raw Hall and status | Records/transmits evidence only. Ring overflow can discard telemetry, not change navigator state. | Legitimate diagnostic/recording function. |

### Proven end-to-end paths

1. **Normal claimed advance:** ADC -> `hallTask` -> `blockQ` -> `rvmTask`
   (sketch:253-287, 356-375) -> `continuity.admit` -> `feedRecognizer`
   (315-329) -> `Recognizer::step/accept` (`RvmShape.h`:197-230) ->
   `Navigator::onEnsemble` (`RvmNavigator.h`:39-46). Only if shape LLR,
   70-count amplitude, and expected polarity all pass does `lastMm` advance.
2. **Wrong-polarity stop:** same path, except
   `Navigator::onEnsemble` sets `Contradiction` (`RvmNavigator.h`:45-46).
   `serviceAuto` sees not-tracking and calls `withdrawAuto`, requesting zero
   PWM (sketch:555-575). This is reachable in AUTO and stops before any
   arrival-boundary criterion.
3. **Standing declaration/baseline discard:** `start_mm` or `start_interval`
   is accepted only while stopped (sketch:672-687; `RvmOps.h`:103-112).
   `admit` then takes the paused branch for every PWM<15 block and does not
   emit those blocks (RvmContinuity.h:121-138). At the declaration it calls
   `onDeclare`, which starts the recognizer baseline at the current recognizer
   length (sketch:331-340); the recognizer receives its first baseline blocks
   only after PWM reaches 15. Those moving blocks can be on the approach to the
   next magnet.

## Critical findings

### C1 — Shape fitting is a reachable rejection gate (proven)

`RvmShape.h`:146-182 constructs DCT features and a Gaussian-mixture LLR;
lines 212-230 retain candidates only when `c.llr > 0` and then choose the
highest likelihood overlapping window. `feedRecognizer` passes *only accepted*
ensembles to `Navigator` (sketch:315-329). Thus an otherwise adequate,
correct-polarity Hall excursion with a non-positive model score never reaches
geographic confirmation. This is not diagnostic-only code and it is active in
the r5.1 build (`Recognizer recognizer` at sketch:137).

**Conference impact:** contradicts Proposition 7 (and amended Proposition
6A): waveform shape has independent rejection authority. It also conflicts
with Proposition 9's minimal-safeguard rule. The source supports the behavior;
whether a particular historical event is rejected this way requires the
separate replay.

### C2 — The 70x2 onset sentinel is absent (proven)

The only 70 constant in the recognition code is
`MIN_EXCURSION_ADC = 70` (`RvmShape.h`:62). It is compared to the maximum
interpolated amplitude of a completed trial window (`RvmShape.h`:160-181,
220), after shape scoring. There is no two-consecutive-sample state, no
pre-trigger preservation, and no onset event in the sketch or its RVM headers.
The source works on 20-ms block means, whereas Hall acquisition is 5 kHz
(sketch:253-285). A 70x2 opening is therefore neither represented nor the
candidate trigger.

**Conference impact:** fails the agreed Observe/Qualify entry criterion in
the navigation cycle and Propositions 1, 4, and 6A. It is a missing required
mechanism, not evidence for an invented threshold.

### C3 — Navigation observation is conditionally suppressed (proven)

Raw Hall recording is unconditional within the running task
(sketch:263-285), but the recognition stream is not:

- PWM<15 returns before the delay line/recognizer (`RvmContinuity.h`:121-138).
- A block in a 1-s flat span that is off the stored baseline is withheld
  (`RvmContinuity.h`:212-227), including a motor-on stalled/standing plateau.
- A full nonblocking `blockQ` drops a 20-ms block for NAVI
  (sketch:260-262), with no recovery/retry.

Consequently, “Hall recording” is continuous but Hall **observation by
navigation** is not unconditional. The continuity mechanism can preserve a
partial shape across a stop, but it is still an executable exclusion mechanism
that can change recognition and baseline formation.

**Conference impact:** contradicts Proposition 9 and the agreed cycle's
unconditional observation requirement. It is especially incompatible with
the requirement that Hall observation continues during stop-and-hold.

### C4 — The historical baseline-discard finding is confirmed (proven)

The prior finding is in
`docs/NAVI_RVM_CAUSAL_COMPARISON_R51_VS_ALIGNER_20261021.md` at `cd5a6d6`,
Addendum: r5.1 ignores standing readings below PWM 15, seeds its recognizer
baseline during subsequent movement, and records the 2026-10-21 MM049 case.
The source proves the mechanism exactly as described in execution trace 3.

**Trigger:** normal operating procedure—declare standing at a mid-interval
baseline, then depart.  
**Consequence:** the intended standing baseline is unavailable to the
recognizer; moving approach blocks establish its first effective baseline and
can be magnet-contaminated. In the documented field case, the resulting
off-baseline classification withheld cruising blocks and preceded the MM049
contradiction.  
**Status:** the discard and its ability to influence recognition are proven;
the claimed causal chain for that particular incident is evidence-supported by
the contemporaneous causal comparison, not independently replayed here.

This is an active r5-specific behavior (the paused branch is invoked by the
same PWM-zero condition required by `admitStartMarker`), not dead diagnostic
code. It conflicts with the agreed architecture's unconditional observation;
the conference appropriately leaves the replacement baseline policy open.

### C5 — A wrong-polarity encounter stops AUTO immediately, without an arrival boundary (proven)

Every accepted RVM ensemble is immediately treated as the expected marker or
a contradiction (`RvmNavigator.h`:39-46). On contradiction, AUTO is withdrawn
and PWM is driven toward zero (sketch:555-575). There is no per-interval
arrival clock, distance accumulation, pace scaling, or permissible-arrival
boundary anywhere in the included executable NAVI path.

**Conference impact:** contradicts Propositions 6B and 8. A nonqualifying
observation must leave the expected target pending; only failure to confirm by
the empirically set boundary may invoke stop-and-hold. r5.1 instead makes an
accepted wrong-polarity *shape* sufficient to stop.

## Geographic and legacy-code results

### Geographic authority

There is no alternative identity list, map search, skip/reacquisition,
distance-based recovery, or duplicate/stitching geographic path in the active
sketch headers. `Navigator::onEnsemble` is the sole automatic `lastMm` writer,
and it computes `nextMm` only with `nextMarker(next_, dir_)`
(`RvmNavigator.h`:41-43; `RouteMap.h`:56-63). Operator declarations can reset
both positions (sketch:331-340, 672-687); direction/session changes suspend
instead of reinterpret position (sketch:651-671). This supports Proposition 8
within r5.1's restricted geographic layer.

But geographic authority is *not* cleanly separated from recognition:
the shape recognizer decides whether there is an encounter at all, and the
polarity mismatch changes navigation state. The station driver consumes the
resulting `lastMm` for motor control (sketch:564-575); it does not write it.

### Vestigial-code inventory

| Mechanism | Reachability / consequence | Classification |
|---|---|---|
| Gaussian-mixture/DCT morphology classifier (`RvmShape.h`:11-40, 146-230; model data in `RvmShapeModel.h`) | Executable for every admitted candidate; blocks nonpositive-LLR waveform from navigator. | Active legacy morphology gate; critical conflict. |
| 70-count amplitude filter (`RvmShape.h`:29-34, 62, 220) | Executable; required alongside LLR but is not 70x2 onset. | Active threshold mechanism; significant mismatch. |
| Held-window overlap/replacement and half-width wait (`RvmShape.h`:36-40, 214-229) | Executable; selects one fitted window and delays confirmation. | Active encounter-stitching/shape-selection machinery; conflict insofar as it determines admission/completion by fitted window rather than observed RTB. |
| Continuity delay/withholding (`RvmContinuity.h`:15-24, 121-155, 212-227) | Executable on all blocks; suppresses observations under stated conditions. | Active legacy state/observation gate; critical interaction risk. |
| `RouteMap` grade PWM table (`RouteMap.h`:85-198; sketch:566) | Executable only in AUTO and changes throttle based on confirmed geography. It does not recognize/recover position. | Legitimate pre-existing operating function; preserve boundary. |
| Station/bubble stop state machines (`RvmStations.h`:125-180, 342-504) | Executable in AUTO; can stop/restart/cap motor output using committed local/peer position. No write to navigator. | Legitimate operational authority, but integration-sensitive. |
| QUORUM/other older sketches in repository | Not included by this sketch, not linked through its includes, and no call path reaches them. | Unreferenced for audited build; not evidence of r5.1 behavior. |
| Local hardware Hall threshold macros in `LL_LocoConfig_*` | Present in included profile but not referenced in this sketch/RVM headers for recognition. | Included configuration vestige / legitimate other-firmware profile data; unreachable here. |

No executable geographic-recovery/search mechanism was found. That is a
positive result, but it does not offset C1-C5.

## Conference-proposition conflict matrix

| Proposition | Relevant r5.1 code | Assessment |
|---|---|---|
| 1 Target acquisition | `Navigator::onEnsemble`, `RouteMap` | Partial: one expected target exists, but acquisition is preceded by open-ended shape classification. |
| 2 No recovery | `RvmNavigator.h`:35-46 | Supports: no automatic recovery/skip/search found. |
| 3 Timing reference verified | tracing timestamps sketch:157-159, 439-449 | Unresolved / not implemented as a navigation rule; reporting has timestamps but no specified RTB/onset architecture. |
| 4 Expected neighborhood | map gives only next polarity/adjacency; `RvmShape.h`:7-9 explicitly uses no speed/distance/time | Contradicts: no predicted position/arrival neighborhood participates in recognition. |
| 5 No new architectural change needed | whole active RVM stack | Not applicable as an implementation claim; r5.1 is a different pre-conference architecture. |
| 6A Complete retrospective ensemble | `RvmShape.h`:36-40, 197-230 | Contradicts: fit-window timing and half-width delay define completion; no observed return-to-baseline or minimum-width qualification. |
| 6B Stop on late nonconfirmation | `RvmNavigator.h`:45-46; sketch:559-575 | Contradicts: no arrival boundary; wrong-polarity accepted ensemble causes immediate AUTO stop. |
| 7 Shape diagnostic only | `RvmShape.h`:180, 220-229 | Direct contradiction: LLR is an admission/rejection gate. |
| 8 One target/no recovery | `RvmNavigator.h`:41-46 | Partial: exactly one next target and no recovery, but a nonqualifying accepted encounter produces contradiction/stop rather than continued waiting. |
| 9 Minimal evidence-justified safeguards / continuous observe | `RvmContinuity.h`:121-155, 212-227 | Contradicts: PWM pause and continuity withholding are additional behavior-specific gates; navigation stream is conditional. |
| 10 Demonstrate simplicity | model / headers and existing r5.1 tests | Unresolved: r5.1 is substantially more complex than the agreed cycle and is not a demonstration of it. Historical replay remains required. |

## Operational boundaries and interaction risks

The motor writer is centralized in `writePwm` / `serviceRamp`
(sketch:210-238), but several legitimate requesters share it: manual command,
AUTO station/bubble driver, dispatcher release/stop, E-stop, and low voltage
(sketch:486-579, 602-703). Navigation affects this path only through the
snapshot read in `serviceAuto`: loss of `Tracking` withdraws AUTO, while a
confirmed `lastMm` drives grade and station geography. E-stop and low voltage
override all AUTO commands; manual commands can suspend or redeclare
navigation. These are operational controls, not inferred geographic recovery.

The important coupling to preserve during replacement is therefore:
`confirmed NAVI position -> read-only snapshot -> station/grade driver ->
PWM request`; no station, peer, dispatcher, or motor state must become an
alternative source of NAVI location.

## Prioritized remediation sequence (recommendations only; no changes made)

1. **Freeze r5.1 as audit evidence; do not graft the new architecture into its
   current recognizer.** First remove/replace the active shape-admission,
   PWM-pause, and withheld-block authorities in a separately authorized
   implementation effort.
2. **Define the observation/baseline contract before code.** It must explicitly
   address standing declaration versus moving-baseline offset without discarding
   observations, and retain raw recording through all holds.
3. **Implement and test the agreed candidate/qualification path only after the
   open numerical questions are measured:** 70x2 onset preservation,
   observed RTB, polarity, and a measured minimum-width policy. Do not invent
   a width value.
4. **Add the independent per-interval arrival-boundary authority last, after
   its empirical method and pace limit are established.** It alone should
   command NAVI stop-and-hold; wrong/nonqualifying observations should not.
5. **Retain and regression-test operational behavior:** E-stop, low voltage,
   manual/AUTO transition, station dwell/release, and bubble holds must retain
   motor authority but never mutate confirmed geography. Then perform the
   prescribed historical replay, including MM110/146/169/113, before field use.

## Final answer to the completion question

**Yes.** `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1 contains reachable navigation
authority inconsistent with the ten propositions: the RVM morphology gate
(C1), non-70x2 candidate gate (C2), conditional navigation observation and
baseline discard (C3-C4), and wrong-encounter immediate AUTO stop without an
arrival boundary (C5). No alternative geographic search/recovery authority
was found; the retained station, bubble, manual, E-stop, and battery paths are
separate legitimate motor/operational functions whose NAVI interface must be
preserved.
