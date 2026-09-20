# Hall and IR navigation integration model

Date: 2026-09-19
Status: Proposed integration specification and replay acceptance criteria.
No firmware behavior, protocol, operator decision, or field acceptance changed.

## Objective and evidence boundary

Build navigation that combines mapped Hall landmarks with independently
measured travel from the IR sensor on a non-powered wheel. Resolve bounded
observation errors while preserving station, dispatcher, manual, e-stop and
low-voltage authority. Publish uncertainty before it can produce an unjustified
position-dependent action.

The active IR work reports a shared detector candidate that suppresses a
recorded stationary-noise failure. Moving replays contain radio gaps and do
not yet establish physical missed/doubled-pulse budgets. Optical TRACKING is
not a bounded distance measurement. Navigation must accept UNKNOWN distance
as a normal input. No assumed pulse-error percentage becomes a safety bound.

The earlier NAVI_HYPOTHESIS_SHADOW prototype is untested and is not the
implementation baseline. This specification does not inherit its one-error
lifetime, branch-on-contradiction rule, default Vmax, or certainty claim.

## 1. IR interface

Use a logical adapter contract first; do not change the frozen 72-byte
IRSpeedWire.h packet or existing MQTT and peer packets in this work item.
Some required evidence is absent from that packet. A later versioned extension
must be designed with the IR implementation owner before authority is enabled.

An immutable IrSnapshot contains:

| Field | Required meaning |
| --- | --- |
| sensorId, targetLocoId | Source and intended locomotive; reject mismatches |
| bootId, sequence | Sensor restart identity and ordered report identity |
| capturedUs | Monotonic extended sensor time, with declared clock domain |
| receivedUs | Local receive time, used for freshness, not travel duration |
| completedPulses, observedRises | Separate monotonic 64-bit counters |
| inferredCorrections | Separately reported; zero until independently validated |
| calibrationId | Wheel/encoder identity, distance per pulse and its bounds |
| validity, reasonBits | Optical condition and distance-bound status separately |
| continuityEpoch | Changes on any unbounded acquisition interval or reset |
| healthTotals | Persistent sample gaps, aborts, saturation and contrast losses |
| boundProfileId | Evidence-backed error budget and its operating envelope |

The adapter exposes distanceBetween(anchor, event), returning:

    lowerUm, upperUm, bounded, reasonBits,
    anchorIdentity, eventIdentity, calibrationId, boundProfileId

Use checked integer arithmetic. Unknown is an explicit flag, never zero distance
or an arbitrary large sentinel that can participate in arithmetic.

For completed counts differing by C, with validated maximum extra counts F,
maximum missed counts M, calibrated pulse pitch [pLo,pHi], and endpoint phase
allowance Q:

    wheelLower = max(0, (C-F)*pLo - Q)
    wheelUpper = (C+M)*pHi + Q

Use signed/checked intermediates before clamping. A uniform perfect encoder
counted on one phase needs up to one pitch of endpoint uncertainty in either
direction. Unequal spoke geometry requires its own measured phase bound.
M, F and Q are evidence inputs, not tuning constants. If no bound profile covers
the interval, bounded=false. Do not subtract two cumulative nominal distances
and pretend the resulting difference has no uncertainty.

A healthy endpoint cannot erase an intervening invalid episode. If continuity
epochs differ, the adapter returns UNKNOWN unless a validated outage bound
explicitly covers the gap. Radio packet loss is different from lost samples:
cumulative counters and persistent health history can bridge missing reports,
but event-time alignment may still be too uncertain. Reordered/duplicate
reports cannot roll counters back or count travel twice.

Single-channel IR provides travel magnitude, not direction. Direction reversal,
rollback, handling, or an unverified consist attachment invalidate the signed
route interpretation. A direction command alone does not prove motion direction.

## 2. Alignment with Hall and the route map

A HallObservation contains boot/frame/event identity, opening timestamp,
opening and later-window signed measurements, reference provenance, and the
IR distance bracket at opening. Later Hall evidence updates that event identity;
it does not create another crossing. Never increment location at opening and
silently undo station or traffic effects after the window arrives.

For local acquisition, snapshot counts and timestamps coherently. For a remote
IR car, bracket a Hall event with IR reports mapped into the locomotive clock
using a bounded clock-offset/drift estimate. Without that estimate, include
the entire possible count/time bracket or report UNKNOWN. Never subtract
sender millis() from receiver millis(), or treat packet arrival as capture time.

The route spacing describes the Hall sensor's track path. IR measures the
unpowered wheel's path. Establish their separation, mounting and coupling
configuration. Curves, slack and reversal can change relative travel; either
bound that difference experimentally or include it as unknown. A constant
wheel diameter does not remove this geometry requirement.

For a candidate k markers ahead, sum the surveyed directional spacings from
the hypothesis anchor. Expand that expected distance by surveyed uncertainty,
Hall trigger-position variation at both endpoints, and wheel-to-Hall geometry
uncertainty. A candidate is distance-compatible iff this interval overlaps the
measured travel interval. Equality is compatible. Evaluate the actual map:
spacing is not uniformly 300 mm.

Near-zero travel may support a same-magnet interpretation, but duplicate
qualification must account for Hall field extent and direction. Overlap is
compatibility, not identity. IR never increments marker number on its own.

## 3. Navigation mechanism

Retain hypotheses with: direction/frame, last interpreted marker, original
event/IR anchor, candidate latest event, error history, and provenance. Report
the last confirmed crossing separately from the current along-track interval.

For each observation, enumerate same-marker, next-marker and skipped-marker
interpretations within measured distance and the configured bounded search.
Also retain a false-event alternative where allowed by the explicit fault
model. Evaluate these on every observation, including matching polarities:
same-polarity insertions and omissions may remain invisible until later.

Polarity disagreement is ambiguity evidence. Opening/window agreement does
not establish that an event is real. For UNKNOWN distance, do not eliminate
distance alternatives. Independently justified physical maximum-speed bounds
may still exclude too-early travel; recent Hall-derived speed and commanded PWM
cannot establish a minimum travel distance.

Anchor distance separately for each hypothesis. A branch that treats an event
as false must keep its previous physical anchor. Do not reset distance or pulse
count globally on an unconfirmed event. Confirm only when all retained,
admissible interpretations agree on the required location/action, and preserve
different anchor histories when they imply different future bounds.

The fault model must state limits on insertions, omissions, polarity errors,
and consecutive unknown-distance intervals over a defined rolling horizon.
Choose those limits from replay and sensor evidence. Do not discard a branch
solely because a fixed array fills: capacity overflow becomes HOLD_REQUIRED.
Likewise, exhausting an error budget reports MODEL_LIMIT, not proof that the
remaining interpretation is true. Never automatically reset an error budget
just because pruned branches leave one survivor.

## 4. States and authority

| State | Entry and permitted transition | Operational meaning |
| --- | --- | --- |
| UNLOCATED | Boot, reset, invalid direction/frame; declaration -> ANCHORED | Automatic location authority unavailable |
| ANCHORED | Known crossing or operator-declared interval; adequate evidence -> TRACKING | Declaration retains its spatial uncertainty; it is not an exact crossing |
| TRACKING | Bounded observations and agreed route interval; conflict -> AMBIGUOUS | Existing controller may consume justified position/actions |
| AMBIGUOUS | Multiple admissible interpretations; evidence resolves -> TRACKING | Continue only within a verified common safe travel allowance |
| HOLD_REQUIRED | No survivors, capacity/model limit, or insufficient safe allowance | Latched request through existing stop path; no automatic acceleration/restart |
| REACQUIRING | Fresh sensor stream following a hold/outage | Optical recovery alone cannot restore location or release hold |

E-stop, low-voltage protection and Manual retain existing precedence. Pending
station deceleration and stop requests remain latched through ambiguity. Do not
reset the station machine and return to cruise because location is uncertain.
CTO/dispatcher consumers must not receive a best guess as confirmed position.
A shadow implementation publishes separately and has no control authority.

For each hypothesis, compute the lower bound on distance to the next required
stopping boundary. Take the minimum over hypotheses. Compare with a measured
upper stopping distance including current motion uncertainty, decision latency,
radio latency and motor ramp. If the allowance cannot be proved sufficient,
request a controlled stop immediately. Timers service this check even when Hall
and IR produce no new events. Unknown braking bounds prohibit autonomous
continuation through an unresolved interval. Station behavior changes require
a scoped integration review against the existing operational contract.

At dwell, ignore optical silence as proof of stationary state. On departure,
retain the last anchor and old-field possibility until measured travel and Hall
evidence distinguish a new crossing. A direction change begins a new frame;
queued old-frame events are inert. After a sensor reboot, a new optical stream
does not repair the unmeasured interval; recovery needs an independently
justified location anchor or an operator declaration.

## 5. Replay acceptance matrix

Every case records source files, firmware/calibration identifiers, initial
anchor uncertainty, input clock domains, truth provenance, and expected outcome.
Synthetic cases test logic; recorded cases test coverage. Keep those labels.

| Case | Required result |
| --- | --- |
| Clean CW/CCW, all mapped intervals | Correct crossing and distance interval retained |
| False Hall event with matching polarity | No sole reliance on polarity; duplicate/false alternatives evaluated |
| Wrong polarity or opening/window disagreement | Correct candidate retained or hold; no irreversible immediate advance |
| One/multiple missed Hall markers within declared model | Summed spacings evaluated; correct candidate retained |
| F02/F03, F05-F07, MM136, MM117 | Replay preserved observations; separate historical facts from injected IR |
| Baseline latch/F08/F09/F13, Grillers departure | Missing Hall events cannot disable periodic safety evaluation |
| Same-pole run | No false confirmation merely from matching route polarity |
| Missed/doubled IR pulse, isolated and burst | Truth inside reported range, or explicit UNKNOWN before relying on it |
| Stationary noise, sunlight saturation/transition | No unsupported displacement bound or silent-stopped claim |
| Invalid middle, healthy endpoints | Old distance interval remains invalid or explicitly widened |
| Radio loss/reorder/duplicates and sensor restart | No double counting or bridging unknown resets |
| Clock drift/wrap/delayed Hall-window update | Correct alignment bounds; no second event |
| Direction reversal, declaration between markers | New frame and realistic initial interval |
| Ambiguity during station ramp | Stop request preserved; no return to cruise |
| IR/Hall correlated disturbance or queue overflow | Visible loss/hold, not fabricated independence |
| Candidate capacity/error budget exhausted | Explicit model-limit hold; never silent pruning to certainty |

Primary metrics: false confident positions; independently checked distance
bound violations; correct-hypothesis survival; distance/time to resolve; holds
per lap; missed station boundaries; maximum processing time/memory and event
loss. Record continuous valid-span coverage, since always returning UNKNOWN
would otherwise pass accuracy tests without being operationally useful.

Deterministic acceptance requires zero false confident positions, zero silent
truth-bound violations, zero unsafe station releases, and a justified hold for
every model-limit case in the test corpus. These are test gates, not a claim
of zero field failure probability. Operational availability targets and test
exposure must be agreed from measured results, separately for Otto and Toby.

## 6. Implementation sequence and completion evidence

1. IR owner supplies snapshot/interval adapter and independently validated
   bounds. Until then replay uses explicitly synthetic bounded inputs or UNKNOWN.
2. Implement a host-testable pure navigation core and fixture reader using the
   existing surveyed map. Record all candidate retention/elimination reasons.
3. Replay historical Hall failures. Missing synchronized IR must be reported;
   synthetic substitutions do not demonstrate a historical prevention claim.
4. Collect synchronized Hall/IR traces and independently verified travel. Score
   continuous spans and outage transitions; test both locomotive configurations.
5. Add a telemetry-only shadow adapter at existing acquisition and controller
   boundaries. Preserve all current topics, packet structs and control behavior.
6. Review station/traffic/braking integration and grant control only after its
   acceptance gates pass. Production implementation belongs in the repository's
   designated QUORUM lineage; update the catalog and field evidence separately.

This document completes the interface, state and acceptance-criteria work item.
It does not claim implementation, successful historical recovery, validated IR
distance bounds, or production readiness. Measured error budgets, Hall sensing
extent, clock alignment, wheel/Hall geometry and stopping envelopes remain
explicit inputs to subsequent work.

## Source records inspected

- firmware/common/IRSpeedWire.h (existing frozen transport)
- firmware/common/IrMovementDetector.h (active other-task candidate, untracked)
- docs/IR_SHARED_DETECTOR_PROGRESS_20260919.md (active other-task record)
- docs/IR_NAVI_DEVELOPMENT_AUDIT_20260919.md (active other-task record)
- firmware/test-programs/NAVI_ONE/RouteMap.h (surveyed route)
- docs/NAVI_ONE_REVIEW_OF_CLAUDE_CROSS_VARIANT_ANSWER_20260919.md
- docs/CLAUDE.md (integration and production-lineage constraints)

Active other-task files may change and may not yet be on public main. Their
relevant evidence status is summarized above; this specification does not
publish or modify their implementation.
