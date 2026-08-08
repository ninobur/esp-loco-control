# Repository review — NGR console authority alignment, Draft 3

**Reviewer:** Codex (current repository review)  
**Review date:** 2026-08-08  
**Subject:** `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 3  
**Method:** Revalidated against the current Flask console and QUORUM firmware  
**Disposition:** Do not approve implementation until three blockers and two specification mismatches are resolved

This review was performed directly against the repository after Draft 3 landed.
It records review findings; it does not itself amend the controlling
specification.

## Verdict

Draft 3 is substantially improved, but it is not ready for implementation as
written. Three issues block the proposed field behavior, and two requirements
do not match what the proposed firmware guard can actually prove.

## F1 — E-STOP recovery cannot pass T6

R11, P12, and T6 say the dispatcher can clear E-STOP and recover an enlisted
locomotive without crossing the RELEASE door.

The current QUORUM behavior makes that impossible:

1. Asserting E-STOP clears `autoRunning` but preserves `autoEnrolled`.
2. Clearing E-STOP sets `motorDirection=DIRECTION_NEUTRAL`.
3. BEGIN/GO refuses while direction is NEUTRAL with
   `NEUTRAL_SELECT_DIRECTION`.
4. R7 removes the locomotive-page DIRECTION control once enlisted.
5. The dispatcher console has no direction control.

The result is an enrolled locomotive that the dispatcher cannot restart. The
only available recovery is RELEASE, directly contradicting T6.

Q4b is therefore blocking for this workflow, not non-blocking. Before
implementation, the specification must define how AUTO recovers direction
after E-STOP. Possibilities include preserving a safe prior logical direction
while keeping PWM zero, or having BEGIN derive direction from the mission and
orientation. Either choice is firmware behavior beyond P11 and requires an
explicit ruling.

Relevant firmware: `QUORUM.ino`, GO gate near line 2575 and E-STOP clear path
near lines 2621–2625 at the reviewed revision.

## F2 — `state/station` is not a reliable command-response channel

P7 assumes a locomotive refusal can be displayed beside the button that
provoked it. The current `stationPublish()` implementation was designed for
station-machine transitions, not request/response acknowledgment.

It suppresses a publication whenever the event pointer and offset match the
previous publication:

```cpp
if(ev==lastEv && off==lastOff) return;
```

The comparison ignores `note`, which carries the refusal reason. Therefore:

- one `GO_REFUSED` reason can suppress a later, different `GO_REFUSED` reason;
- repeating the same rejected command may produce no response at all;
- repeated `ENLIST_REFUSED / WAIT_FOR_STOP` responses may disappear; and
- `state/station` is non-retained, so a missed response cannot be recovered
  after reload or reconnect.

P7 and T9 need an always-emitted operator-command response path. A dedicated
response publisher or a response sequence identifier would make acknowledgment
observable without restoring the station-transition flood that the existing
deduplication prevents. This is another firmware dependency not currently
allowed by §8's “P11 only” firmware scope.

Relevant firmware: `QUORUM.ino`, `stationPublish()` near lines 2124–2147 at the
reviewed revision.

## F3 — A broadcast E-STOP cannot safely be an undefined toggle

Every locomotive subscribes to the shared dispatcher topic
`ngr/dispatcher/cmd/estop`, while P12 proposes displaying E-STOP state per
locomotive.

Per-locomotive states can legitimately disagree. One locomotive may have been
E-stopped from its locomotive page while another was not; another may be
offline or have unknown state. In such a mixed state, a single toggle has no
unambiguous next payload:

- payload `1` asserts E-STOP on all locomotives;
- payload `0` clears E-STOP on all locomotives and executes the clear path even
  on one that was not previously E-stopped.

Deriving a broadcast clear from one locomotive's displayed state can therefore
alter another locomotive, including a manually operated one.

The specification should define aggregate, mixed, and offline behavior. The
safer interface is asymmetric: an explicit broadcast **E-STOP ALL**, with
deliberate per-locomotive clearing or a separately labelled and confirmed
**CLEAR E-STOP ALL** action. Individual authoritative state indicators should
remain visible in either design.

Relevant firmware: `QUORUM.ino`, topic construction near lines 1977–1983 and
the shared handler near line 2619 at the reviewed revision.

## F4 — R9 is not enforced by the locomotive

R9 states that ORIENTATION and LOCATION are required before enlistment. P6
implements that rule only as a Flask-console pre-flight check.

The current `cmd/auto` firmware handler accepts `cmd/auto 1` without checking
session orientation or usable navigation state. Consequently another MQTT
client, a future console, or direct command injection can enlist a locomotive
that fails R9. The console then withdraws the setup controls under R7, leaving
the operator unable to repair the missing pre-flight without releasing the
locomotive.

If R9 is an authority invariant, QUORUM should enforce it alongside the P11
motion guard, using its authoritative session-direction and navigation-ready
state. Flask may still withhold an obviously premature request as UI
sequencing, but it should not be the sole enforcement point.

If firmware enforcement is deliberately not wanted, R9 must instead be
described as a console workflow rule rather than an unconditional enlistment
requirement.

Relevant firmware: `QUORUM.ino`, `T_CMD_AUTO` handler near lines 2563–2567 at
the reviewed revision.

## F5 — “Stopped” and “not energised” are not equivalent

R5, R6, and T2 speak in terms of a physically stopped or moving locomotive.
P11 proposes using `motorIsMoving()` as the firmware test.

That helper is explicitly an energization test:

```cpp
return actualPwm>0 || commandedPwm>0;
```

It does not observe physical motion. A coasting, rolling, or manually pushed
locomotive with both PWM values zero can pass the guard and enlist. The same
limitation applies to the alert's current `moving` field, which is derived from
PWM rather than measured wheel or ground motion.

The specification must either:

1. narrow the guarantee to “propulsion is de-energised,” or
2. identify an independent motion observation capable of proving the
   locomotive is physically stopped.

If the stronger interpretation remains, T2 should include an unpowered-motion
case rather than only a locomotive moving under applied PWM.

Relevant firmware: `QUORUM.ino`, `motorIsMoving()` near lines 1548–1558 at the
reviewed revision.

## Required disposition before implementation

1. Resolve AUTO direction ownership and recovery after E-STOP; reclassify Q4b
   as blocking for T6.
2. Define an always-observable command-response mechanism for P7/T9.
3. Define safe dispatcher E-STOP behavior for mixed and unknown per-locomotive
   states.
4. Decide whether R9 is a firmware invariant or only console sequencing.
5. Reconcile the physical-motion language with the PWM-based P11 test.

No objection was found to the core ENLISTED/RUNNING separation, retained-state
provisional seeding, restoration of Flask GO/STOP fan-out, distinct
BEGIN/PAUSE/END semantics, or the principle that reported locomotive state—not
optimistic console state—governs the display.
