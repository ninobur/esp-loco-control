# NAVI Legacy CTO Assumptions Audit

## Scope and Verdict

Read-only control review at `57d34d6`, prompted by David after the
POSITION_STATIONS_R1 change: look for concepts inherited from block-era Close
Train Operations that no longer fit an onboard decision agent.

Reviewed the current 0.6 sketch, its local headers, committed dashboard,
decision0002, decision0031, CTO2 harvest and CTO3 intent/design notes.
Dashboard references below use the committed file at57d34d6; collaborators'
uncommitted dashboard edits were inspected only to distinguish them and were
left untouched. No Pi inspection, MQTT command, flash or control edit occurred.

**No active fixed-block occupancy, West/East dispatch, permanent leader/follower
assignment or train-peer clearance engine was found in this NAVI build.** Its
ESP-NOW receive path handles the IR instrument, not a locomotive peer table
(`NAVI_COHERENCE_0_6_IR_HEALTH.ino:566`). Thus this is not a finding that legacy
block traffic logic secretly controls Toby, nor evidence of working train-to-train
collision protection. Existing CTO-looking console names do not prove that capability.

The important inheritance is in the command/status contracts and the remaining
routine-based control. Some contracts now disagree across layers. Provenance
must not be exaggerated: several findings are NAVI_ONE/QUORUM carryovers or
analogous assumptions, not proven descendants of the original block algorithm.

Throughout, `V/` means
`firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/`.

## Concrete Contract Problems

### 1. Dispatcher STOP can cross into Manual

`V/NAVI_COHERENCE_0_6_IR_HEALTH.ino:788` handles dispatcher STOP by setting
`autoRunning=false` and requesting PWM0, without checking enrollment. In
contrast, dispatcher release at755 checks `autoEnrolled || autoRunning`.
The committed dashboard's `dispatcher_endcto()` at server line2507 sends release
and then dispatcher STOP to every `console_order()` entry, not just AUTO trains.
That list includes named locomotives independently of their current mode.

Consequently END AUTO can stop a manually driven Toby that was never enrolled.
The host probe reproduced a manual target80 becoming0 through the actual STOP
branch, and the real END AUTO function emitting STOP for a listed manual train.

This is a concrete authority-boundary defect, not a philosophical objection to
enrollment. Decision0002 explicitly records the same historical dispatcher-STOP
problem and says Manual is operator-owned. Recommended focused fix: distinguish
dispatcher routine STOP from the operator's direct stop and from universal
E-stop; test END AUTO with one manual and one automatic train. No fix applied.

### 2. AUTO ownership is misreported as running

Firmware `serviceStatus()` at895 publishes `auto=autoEnrolled` and
`running=autoRunning` (arguments at921). The committed server at574 reads
`d["auto"]` into `st["auto"]` under a comment explicitly calling it autoRunning;
it does not consume the separate `running` field. JavaScript at2083 treats
`s.auto` as running, and at2211 labels an enrolled train AUTO RUNNING on that basis.

For paused enrollment (`autoEnrolled=true`, `autoRunning=false`), the firmware
truth is `{auto:1,running:0}`, but the page's running test is true. This is a
source-level counterexample, not a claim about the exact cause of an earlier
field incident. Preserve ownership/activity separation and reconcile the
consumer contract with backward compatibility; do not collapse the two flags.

Related inherited UI restriction: the per-locomotive E-stop button is disabled
while enrolled (committed server2231), although the backend `_cmd()` accepts
E-stop in every mode and firmware gives it hard priority. The dispatcher
E-stop remains available. Review the UI restriction against decision0002's
"E-STOP ... works in every chamber and every state" before changing it.

### 3. Hall speed assumes one accepted event means one MM interval

`V/NAVI_COHERENCE_0_6_IR_HEALTH.ino:1078` explicitly includes
`MissedAndAdvanced`, but at1080 uses only
`spanMm(before.navMm,before.navDir) / elapsed` for all accepted advances.
The navigator can legitimately advance across multiple mapped intervals.

Hardware-free reproduction used the real navigator and extracted calculation:
MM040 accepted, then MM042 after2 seconds with IR evidence for the skipped
marker. NAVI returns `MissedAndAdvanced`; the speed expression gives150 mm/s.
The actual mapped MM040->042 distance is600 mm, giving300 mm/s over that interval.
This is an inherited one-event/one-interval assumption, not proven block ancestry.

There is also the already observed stale-speed problem: the last estimate is
republished on later judged events and in the1 Hz status payload. The dashboard
only forces zero from status when `moving == 0` (committed server565), while
firmware status at905 emits moving1 or null, never0. The independent
`NaviSpeedInterpretation` can meanwhile correctly report STOPPED and speed0
for the IR tile. A fresh message can therefore carry an old Hall measurement.

Recommended bounded follow-up: preserve Hall speed as interval evidence, sum
the distance actually covered by an accepted multi-step advance, handle repeated
landmarks after reversal, exclude corrected reference frames, and report its
measurement window/age. Operator train speed should consume NAVI's judgment,
not make the dashboard reconcile contradictory meanings of "current speed."
Do not silently replace raw Hall interval history with a different sensor.

## Architectural Carryovers to Reconsider

### 4. Stop, loss of authority and operator recovery remain bundled

`withdraw()` at342 combines a controlled stop, AUTO disenrollment, sticky warning,
speed0 publication and nav-ready0. It is used for unresolved location, station
overshoot/phase timeout and Hall-queue loss. `warnSticky` can only be cleared
through declaration (`declarePosition()` at494), even when the warning describes
a successful proximal position correction (`applySequenceCorrection()` at392).

This is recognizably the NAVI_ONE one-strike/operator-recovery convention.
It is not necessarily wrong to stop or require approval to restart. The question
is whether each cause warrants the same loss of operating authority and whether
acknowledging a resolved condition should require redeclaring a known position.
Do not remove queue-loss or other hard protections under the banner of confidence.

Precision matters: current ordinary Hall rejection retains the incumbent
position (`Navigator.h:68`). The `Uncertain` handling branch records observations
without recovery, but this revision has no assignment entering that state.
Do not describe that dormant branch as today's ordinary discrepancy response.
Actual `Lost` is entered through `haltForLoss()` on queue-loss handling.

### 5. Position-based targets are still executed by a station routine

57d34d6 removes the exact-entry trigger, but retains approach/zone/ramp/dwell/
departure state, timed PWM pacing, a120-second active-phase watchdog and a
stop-offset+3 departure boundary (`Stations.h:305`,345,352,360). Those were
deliberately preserved to isolate the correction. They are not all obsolete:
a requested dwell is inherently timed, smooth PWM changes still matter, and
visit history prevents repeated service.

The remaining limitation is clear: station requirements use the current
landmark index, not a continuously updated within-segment position. They launch
a ramp at a mapped point; they do not yet regulate arrival at an arbitrary
physical destination. IR supplies the relative distance needed for that next
step, but there is no general destination/speed/dwell mission layer yet.

Preserve useful mechanics while moving toward a shared answer to "What is
required here, and what remains to be done?" Avoid replacing a few functioning
phases with a sprawling new framework merely because they are inherited.

### 6. Compatibility vocabulary can impersonate a train-state model

Status `level=CLEAR` at917 means `navigator.positionKnown()`, not traffic clear,
all protections satisfied, or permission to move. `dead_reckoned_mm` is the same
integer as `navMm`, not a continuous IR-propagated position. `moving` is another
local IR interpretation, separate from NAVI's STOPPED speed interpretation.
`agree/disagree` are advance/refusal counters, not independent witness counts.

These existing fields should not become the contract for future locomotive
cooperation without semantic review. Publish NAVI-owned location, movement,
speed, authority, current obligation and reasons with explicit freshness and
reference frames, preserving compatibility where needed. A single vague CLEAR
must not stand in for all of those dimensions.

Low-priority fossils also exist: `Ops.h:36` defines an unused StopArmingPolicy;
`Stations.h:250` defines an unused holding helper. `StopCause` is accepted by
`requestPwm()` but is not read there; actual E-stop and low-voltage priority
come from explicit paths in `serviceRamp()`. These names/comments are not
evidence that the corresponding old mechanism remains active. Clean them up
separately, after confirming consumers; do not mix cleanup with control changes.

## Lessons Worth Keeping

The older `docs/CTO3/CTO3_INTENT_BASELINE.md` already says "Blocks are gone"
and places intelligence aboard each locomotive. It treats timed station
choreography, fixed blocks, permanent roles and AUTO-only traffic visibility as
historical ideas, not automatic requirements. The repo itself anticipated much
of this transition.

Keep the durable principles: explicit Manual/AUTO ownership; E-stop precedence;
strict command parsing; navigation continuing to observe in Manual; retained
context and visit history; missing peer reports not proving empty track; and
physical train extent rather than point-only separation. Historic consist
dimensions or stopping constants must still be verified for the actual trains.
None of these principles requires resurrecting a fixed-block dispatcher.

Historical records include conflicting proposals and later operator refinements.
This audit does not repeal decision0002 or0031, enable peer operations, or treat
an old CTO design as permission to install it in this single-loco build.

## Evidence Dependence

David's correction is important: trustworthy mapped Hall-to-Hall distance
divided by elapsed time is a separate speed observation from IR pulses.
IR distance and IR speed share one pulse source; commanded and applied PWM
are related propulsion facts, not two independent travel measurements.

Hall interval speed has its own observation window and naturally lags changes.
Its map association must be credible. Because current NAVI uses IR to admit
some Hall advances, agreement of accepted Hall speed with IR is not wholly
independent validation of the complete navigation pipeline. That qualification
does not discard the Hall timing evidence; it prevents counting shared
decisions twice. Compare measurement windows and retain each source's provenance.

## Verification and Follow-Up

Ran `python3 tools/audit_navi_legacy_contracts.py`. It extracts the actual
firmware STOP branch and speed expression, compiles host probes with ASan/UBSan,
and executes only the AST-extracted committed END AUTO function with publishing
stubs. It confirms the manual-STOP and150-vs300 mm/s counterexamples. It is an
audit reproducer whose current-defect assertions must change when fixes land,
not a green acceptance test claiming those behaviors are desirable.

Other findings are source-traced, not field reproductions. No firmware source
was edited or recompiled for this audit; no safety behavior was weakened.

Suggested small follow-ups, each separately reviewed:

1. Reconcile command ownership and AUTO activity reporting; include Manual
   plus AUTO test cases and review E-stop access on both consoles.
2. Repair the Hall interval-speed distance/window contract and connect operator
   reporting to NAVI's shared movement judgment without corrupting raw evidence.
3. Classify stop reasons and recovery/acknowledgment requirements, retaining
   unaffected knowledge rather than using declaration as a universal reset.
4. Extend mapped operating requirements to within-segment distance and speed
   goals, then explicit destination/dwell missions. Keep the station candidate's
   independent review and field acceptance separate from this audit.
