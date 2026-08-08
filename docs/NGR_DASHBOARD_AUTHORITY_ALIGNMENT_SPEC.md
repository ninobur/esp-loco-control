# NGR console — authority alignment and the AUTO blocker

Status: **proposal for review (CODEX, Sam). No code written.**
Date: 2026-08-07
Author: Claude Code
Operator model of record: `docs/CTO3/AUTHORITY_MODEL.md`, decision 0013
Findings this addresses: `docs/NGR_DASHBOARD_FINDINGS_20260807.md`
Live console: v1.10.9 on ngr-pi (`/home/david/ngr_app.py`)

An earlier attempt at this work was written as code without being proposed
first, and was reverted in full (`8cd1802`). Its five embedded design
decisions were the operator's to make, and several were wrong against the
authority model. This document proposes; it does not implement.

---

## §0 Terminology (operator ruling, 2026-08-07)

Two different things have been called "direction" throughout this project,
including in earlier drafts of this document. They are now named
separately:

| Term | Values | Meaning | Owned by |
|---|---|---|---|
| **ORIENTATION** | CW / CCW | which way round the loop the locomotive travels | operator / dispatcher — it is part of the *orders* |
| **DIRECTION** | forward / reverse / stopped | linear motion of the locomotive | see Q4 |

§7's rule is restated in this vocabulary as
`travel_orientation = session_orientation XOR motor_reverse`.

Existing MQTT topics keep their names (`state/session_direction`,
`cmd/direction`) — renaming the wire protocol is out of scope and would
break the console/firmware contract. The *documentation and UI language*
adopt the ruling; a future firmware revision may align the topic names
deliberately.

Note "stopped" is a DIRECTION value in this taxonomy, which maps onto the
firmware's existing `DIRECTION_NEUTRAL`.

## §1 Fidelity check

Per `AUTHORITY_MODEL.md`, stated back before proposing anything.

**Manual** is the operator's, completely. Locomotive and dispatcher have
essentially no say. The only constraints are simulation artifices
(ramping), mechanical reality (low-voltage cutoff), and a very few safety
limits. **Manual runs without telemetry** — navigation, position and peer
awareness are conveniences, never preconditions. Any "computer says no" in
the manual path is a defect.

**The dispatcher starts and stops automatic operations, not locomotives.**
GO means *"begin doing what you need to do"* — **not** "move forward."
STOP can mean pause. Either may address one locomotive or both. E-Stop is
for emergencies. The dispatcher also *initiates* sub-routines (Circuit
Express) without running them — the locomotives renegotiated roles between
themselves once the express caught up.

**One operator at a time.** Enlistment is the locomotive operator's
voluntary act, like joining the service; like the service, only the
dispatcher can discharge. Hence End Automatic Operations lives on the
dispatcher console, and an enlisted locomotive accepts no commands from
the loco page.

**Enrolled ≠ running.** `cmd/auto` crosses into the AUTO chamber and
authorizes nothing to move. Dispatcher GO, after its gates, permits
motion.

**In AUTO the locomotives are the intelligence** — they assess position,
act on it, follow rules and routines, and communicate with each other. The
dispatcher is authority, not brains.

**The four doors:** E-STOP, enrollment, release/END, dispatcher STOP.

## §2 Evidence

From 2026-08-07 captures, distinguishing the two flags — `state/auto` is
`autoEnrolled` ([QUORUM.ino:2353]), the alert's `"auto"` is `autoRunning`
([QUORUM.ino:2196]):

| Observation | Source | Meaning |
|---|---|---|
| `state/auto = 1` | broker | enlistment **succeeded** |
| alert `"auto":0` for the **entire day**, all three captures 11:41–13:03 | live 1 Hz alert stream | the locomotive was **never once in AUTO running** |

Enlistment works. Automatic operations have never started. Additionally
`ngr/dispatcher/cmd/go` (the console's BOTH column) is not a topic QUORUM
subscribes to at all — it listens only on
`ngr/dispatcher/cmd/go/<id>` ([QUORUM.ino:1957]).

### §2.1 Root cause of "AUTO is a non-functional button"

The operator's report is: pressing AUTO flashes the phone display and does
nothing. Every step of that is now accounted for, and there are **two
independent faults**, either of which alone is sufficient.

1. The AUTO control is an `<a href>`; clicking it navigates, the server
   publishes, and it redirects back. **The flash is a full page reload.**
2. `cmd/auto 1` is published and the locomotive genuinely enlists.
3. The locomotive publishes `state/auto = 1` **retained**
   (`pubStateIntChanged` → `pub(t,b,true)`, [QUORUM.ino:2260]). The console
   discards every retained message (§5). **Fault A.**
4. Independently, the console's `st["auto"]` is written from the **1 Hz
   alert's `auto` field** ([v1.10.9:545]) — which is `autoRunning`, not
   `autoEnrolled`. Within a second of any enlistment the alert overwrites
   the value back to `0` and marks it fresh. **Fault B.**
5. The console therefore displays MANUAL for an enlisted locomotive,
   permanently, and will continue to until the locomotive is *running*.

**Consequence for the fix:** repairing the retained drop alone (Fault A)
would not have fixed the operator's complaint — Fault B would still win
every second. Fault B is the dominant defect, and it is precisely the
enrolled-vs-running conflation that `AUTHORITY_MODEL.md` warns against:
the console has one flag where the model has two distinct states.

## §3 Defects, and the model each violates

| # | Defect | Violates |
|---|---|---|
| D1 | Dispatcher **BOTH** GO/STOP publishes `ngr/dispatcher/cmd/go` with no locomotive suffix — a topic no locomotive subscribes to. Dead buttons, silently. | Dispatcher must be able to start automatic operations for both |
| D2 | GO refusals are published by the firmware on `state/station` and discarded by the console. A refused GO is indistinguishable from a broken button. | Dispatcher is the authority; authority must see whether its command took |
| D3 | After a reconnect the console can display MANUAL while the locomotive is enrolled — see §5. | One operator at a time: the console must not misreport who holds the locomotive |
| D4 | The loco page continues to offer manual throttle/direction while the locomotive is enlisted. | **One operator at a time.** An enlisted locomotive takes commands only from the dispatcher |
| D5 | The loco page gives no indication that starting automatic operations is a dispatcher function, so AUTO appears to do nothing. | Explanatory only — no authority violation |

## §4 Proposed changes

**P1 — GO and STOP stay on the dispatcher console. Period.** No launch
control is added to the locomotive page. (The reverted attempt put one
there; that was a category error, not a UI preference.)

**P2 — Fix BOTH.** Fan `go`/`stop` out to one publish per locomotive on
the topic QUORUM actually subscribes to. BOTH then means both, and each
locomotive still applies its own gates and reports its own outcome.

**P3 — Surface the locomotive's response on the dispatcher console**, next
to the button that was pressed. QUORUM publishes an event on every
transition, including a named reason for each GO refusal. Subscribe to
`state/station` (already arriving under `state/#` and discarded) and show
the latest per locomotive.

**P4 — The loco page shows enlistment status, and withdraws manual
controls while enlisted.** When `state/auto = 1`: manual throttle and
direction are removed or visibly inert, and the page states that the
locomotive is enlisted and under dispatcher authority, released only from
the dispatcher console. This is *stricter* than today's console, which
still offers manual controls to an enlisted locomotive.

**P5 — MANUAL remains the locomotive operator's own act.** Enlistment
(`cmd/auto 1`) stays on the loco page. **Release does not** — the reverted
attempt added a release control there, which would have let the loco page
discharge a locomotive the dispatcher owns.

**P6 — Never gate manual on telemetry.** Restated as a standing
constraint, not a change: no manual control is disabled because navigation
is unset, telemetry is stale, or position is unknown.

**P7 — Track ENLISTED and RUNNING as two separate states.** *(The primary
fix — see §2.1.)* The console currently has one `auto` flag written from
two different firmware variables, and the alert's `autoRunning` wins. The
model has two states and so must the console:

| Console state | Source | Meaning |
|---|---|---|
| **ENLISTED** | `state/auto` (`autoEnrolled`) | the locomotive has enlisted; the dispatcher holds it; manual controls withdraw |
| **RUNNING** | alert `"auto"` (`autoRunning`) | automatic operations are under way |

The alert must never write the enlistment state. Enlistment is the
handoff the operator is asking for, and it is what the AUTO button should
visibly accomplish — independent of, and prior to, any automatic operation
beginning.

**P8 — Rename the dispatcher control to BEGIN AUTO OPERATIONS.** Operator
ruling (§6, Q2): GO does not mean "step on the gas," it means "assess your
situation and start doing what you need to do." The label should say so.
STOP becomes END/PAUSE AUTO OPERATIONS to match. Console labelling only —
the underlying topic is unchanged, and the firmware behaviour behind it is
a separate work item.

## §5 The retained-state problem, explained plainly

*(This is item 4 from the operator's list — "I don't understand this.")*

MQTT lets a publisher mark a message **retained**, meaning the broker keeps
the last one and hands it to anyone who connects later. It is how a
locomotive's current state survives a console restart.

In July, stale retained messages from long-dead firmware were being
displayed as current — the "ghost tile" failure. The fix in v1.10.0 was
blunt and effective: **the console ignores every retained message.**

The side effect: on reconnect, the console throws away the locomotive's
*real current* state along with the ghosts — and then displays a built-in
default in its place. The default for `auto` is `0`, so the console can
show **MANUAL for a locomotive that is enlisted**. It is not showing stale
data; it is showing invented data. This already caused the v1.10.9 Toby
bug, where a default of NEUTRAL was displayed as Toby's reported direction.

Three options:

- **(a) One-shot authority seed.** On each connect, accept the *first*
  retained value for a short list of authority topics (`auto`, `estop`,
  `direction`, `session_direction`, `start_interval`), then resume ignoring
  retained messages. Store it without marking it fresh, so it displays as
  reported state, not as a live report. *This was in the reverted code and
  is the option needing the most scrutiny — it modifies a deliberate fix
  to a real field failure.*
- **(b) Ask instead of remember.** On connect, query current state over a
  request/response topic rather than trusting retention. Cleaner
  semantically; requires firmware support that does not exist.
- **(c) Display nothing rather than a default.** Show "—" until the
  locomotive speaks. Never wrong, but the console is blank about authority
  until the next change — possibly a long time.

**Recommendation: (a)**, narrowly scoped, with (c)'s discipline applied to
everything not on the authority list. The ghost-tile failure was stale
*telemetry* rendered as current; authority state is different in kind —
being wrong about who holds the locomotive is worse than being briefly
blank, and worse in the dangerous direction.

**Correction (added after §2.1 was traced).** This section originally
presented the retained drop as *the* cause of the MANUAL display. It is
not — it is Fault A of two, and the lesser one. Even with (a) implemented,
the 1 Hz alert would overwrite the enlistment flag back to `0` within a
second (Fault B). **P7 is the fix that actually resolves the operator's
complaint; this section is necessary but not sufficient.** Implementing
(a) without P7 would produce no visible change and would look like another
failed attempt.

## §6 Open questions — operator ruling required

**Q1 — E-Stop. RULED (operator, 2026-08-07).**

> "There should be an EStop on the locomotive dashboard. It should only be
> operable when the loco is in manual operation."

**No constitutional change is required, and none should be made.** The
ruling governs the *console control*, not the firmware door, and the
distinction matters:

- **Firmware:** decision 0013 door 1 stands unchanged — E-STOP acts in
  either chamber and overrides everything. It must, or the dispatcher's
  E-Stop could not reach an enlisted locomotive.
- **Console:** the locomotive page keeps its E-STOP, operable in MANUAL
  and inert once enlisted — because an enlisted locomotive is not the loco
  operator's to command. Consistent with P4 (all manual controls withdraw
  on enlistment); E-Stop is simply the last of them, not an exception.

An enlisted locomotive is stopped from the dispatcher console, which is
another page of the same Flask app and reachable by anyone with it open.

**Amends P4:** the withdrawal on enlistment covers throttle, direction,
*and* E-Stop — no carve-out. Draft 1 of this spec assumed E-Stop would
need one; it does not.

**Q2 — GO semantics. RULED (operator, 2026-08-07).**

> "GO should mean 'assess the situation', 'where are you', 'is there
> another loco in front', 'are you at a station stop already.' In a race
> car go means step on the gas. Probably we should change it to BEGIN AUTO
> OPERATIONS. Like when I start my car in the garage or the street. What I
> do depends on the info I have about the situation."

Historical context supplied with the ruling: in CTO 4-block mode
locomotives *had* to start moving on GO, because they did not know where
they were and had to reach a magnet pair to find out — relying on the
dispatcher having placed them somewhere that would not collide first. Once
both had magnets they used the peer table to negotiate roles. **That
constraint is gone.** The locomotives now know where they are on
enlistment and have collision-avoidance ability, so the first act of
automatic operations should be assessment, not motion.

The firmware does the opposite:

```c
autoRunning=true;
motorDirection=DIRECTION_FORWARD; applyDirection();
requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
```

It forces FORWARD and commands cruise — the 4-block behaviour, preserved
past the reason for it. A locomotive already standing at a station would
be launched out of it.

Console scope: **rename only** (P8). The firmware change — GO becomes
"assess, then act on what you find," with staying put a legitimate outcome
— is a separate CTO3 work item and belongs with Station Stop v1, since
"already at a station" is exactly one of the situations the assessment
must recognise. Recorded here so the ruling is not lost.

### Q2.1 — Iteration-1 scope (operator, 2026-08-07)

> "For the first iteration, BEGIN AUTO OPERATIONS may very well mean
> 'perform the action required for your starting position within the
> orders you have.' In the earlier versions, the Loco had to be set to
> forward and pointed CCW to start auto operations."

This bounds the work usefully — no general situational-assessment engine
is required for iteration 1, only *"consult where you are and what you
were told, then do that."* With Station Stop v1's orders (run the loop,
stop at Arches) the cases are few:

| Starting position | Required action | Firmware today |
|---|---|---|
| Mid-loop, clear of a station | proceed at segment cruise | **correct** |
| Already stopped at a station | dwell, then depart per the routine | launches to cruise — **wrong** |
| Inside a station's approach staircase | continue the staircase | jumps to cruise — **wrong** |

Accuracy note on the firmware: `cruiseForPosition()` ([QUORUM.ino:1579])
*is* position-aware, but only about speed — it selects a cruise PWM from
the grade segment, so Viaduct Hill gets more power. It never returns zero.
GO is therefore grade-smart and situation-blind: it always commands
motion. The iteration-1 change is to consult the station machine before
commanding cruise, not to build an assessment framework.

**Related inconsistency found while tracing this.** The GO handler refuses
a NEUTRAL locomotive with a stated reason
(`NEUTRAL_SELECT_DIRECTION`) but silently overrides a REVERSE one:

```c
motorDirection=DIRECTION_FORWARD; applyDirection();
```

Under §7's rule (`travel_direction = session_direction XOR motor_reverse`)
that silently changes the locomotive's map direction. The operator's note
that earlier versions *required* the locomotive be set FORWARD suggests
the intent was always a precondition, not a silent correction. **Operator
ruling requested:** should BEGIN AUTO OPERATIONS refuse a REVERSE
locomotive with a stated reason, as it already does for NEUTRAL?

**Q4a — A train must be stopped to initiate auto operations. RULED
(operator, 2026-08-07).**

> "Previously we conceived it to be like autopilot. But it was never run
> that way. A train should be stopped to initiate auto operations."

The autopilot conception — engage while under way, as an aircraft does —
is retired. It was never exercised, and it is the conception that left
`cmd/auto` with no motion guard. Auto operations begin from rest.

*Remaining sub-question on mechanism: see the open item below.*

**Q4b — Enlistment preconditions. Operator thinking, 2026-08-07, explicitly
tentative** ("I recall that this decision has been made differently in the
past, this is my thoughts on it tonight"):

> "Previously, auto operations only happened in forward. In CTO it was one
> direction CCW and Forward for obvious reasons. The locos did not know
> where they were, otherwise. We have eliminated that limitation. I think
> that the throttle should be at zero. Direction should be taken over by
> the locomotive once Manual control is relinquished. If you think about
> it, the throttle position beforehand is immaterial as long as the loco
> is stopped."

**A defect this exposes, independent of the ruling.** `cmd/auto` has **no
motion guard and does not zero the throttle on enrollment**
([QUORUM.ino:2535]):

```c
autoEnrolled=(atoi(msg)!=0);
if(!autoEnrolled){ autoRunning=false; requestPwm(0,NORMAL_STEP_MS); }
```

Disenrollment zeroes PWM; enrollment does not. Enlist a locomotive rolling
at PWM 100 and it keeps rolling at PWM 100: `autoEnrolled` true,
`autoRunning` false, so no AUTO code commands PWM — and MANUAL has just
been relinquished. **Neither chamber is actively in charge while the train
moves.** Leaving auto is safe; entering it is not. This holds regardless
of how Q4 is decided and should be fixed either way.

**The distinction worth preserving in any fix:** direction is two things,
and §7 already relates them —
`travel_direction = session_direction XOR motor_reverse`.

- **Session direction (CW/CCW)** is *orders*: which way round the loop.
  Belongs to the operator/dispatcher.
- **Motor direction (forward/reverse)** is *means*: how the locomotive
  achieves its orders. The operator's proposal hands this to the
  locomotive on enlistment.

Read that way the proposal is consistent with §7 rather than a departure
from it, and it retires the forward-only/CCW-only constraint at its
actual root — that constraint existed because locomotives had to move to
discover their position, which is no longer true.

**Consequent proposals, for review — not adopted:**

- **Enlistment requires the locomotive to be stopped**, refused with a
  stated reason otherwise (as GO already does for NEUTRAL). "Stopped"
  currently means `actualPwm>0 || commandedPwm>0` — *energised*, not
  *stopped*; a coasting locomotive reads as stopped. Adequate for
  enlistment; the real witness is decision 0005's `motionWitnessSaysStopped()`.
- **Enlistment zeroes the throttle**, since the locomotive now owns
  propulsion. This makes the operator's slider position immaterial, as
  proposed.
- **Motor direction becomes the locomotive's on enlistment**, retiring
  both the silent REVERSE→FORWARD flip and the need for the operator to
  pre-set FORWARD (which supersedes the Q2.1 ruling request above, if
  adopted).

**Q3 — Refusal wording.** QUORUM's raw strings
(`NO_QUORUM_DECLARE_POSITION`) or plain-English translation
("position lost — re-declare the start interval")? Raw is the
locomotive's own word and cannot drift out of sync; translated is easier
to read at speed.

## §7 Explicitly not proposed

- No launch control on the locomotive page.
- No release control on the locomotive page.
- No client-side pre-judging of GO conditions. QUORUM owns those gates and
  names its own refusals; a console that second-guessed them would be a
  second eligibility gate, which the layer-boundary rule forbids.
- No firmware change of any kind.
- No change to the E-STOP *firmware* door (Q1). The console control is
  withdrawn on enlistment along with every other manual control; the
  firmware continues to accept E-STOP in either chamber.
- No weakening of the firmware authority boundary to work around a console
  defect.

## §7.1 Primary objective, in the operator's words

> "My complaint today about auto mode is that it is a non functional
> button in terms of locomotive operation. It makes the dashboard flash on
> my phone. Nothing [else]. It should at least hand off operation of the
> locomotive to the dispatcher. This function is the predicate for
> starting auto operations, our next goal."

The deliverable is therefore narrow and testable: **pressing AUTO must
visibly hand the locomotive to the dispatcher.** Manual controls withdraw
(P4), the page states who holds it (P7), and the state persists across a
reload and a reconnect (§5). Everything else in this document is
secondary to that.

Note this is achievable **without** automatic operations working at all —
the handoff is the predicate, not the operation. A locomotive can be
correctly enlisted, correctly displayed as enlisted, and correctly refuse
to do anything else, and that is a complete and useful result.

## §8 Sequence if approved

1. Operator rules on Q1 and Q3; CODEX/Sam review this document.
2. Console spec revised to Draft 2 if needed.
3. Implementation as `ngr_app_v1_10_10.py` (version-bump, never edit in
   place).
4. Deploy by scp + `systemctl restart ngr-app` — there is no repo clone on
   the Pi.
5. Field check: enlist Otto from the loco page; confirm manual controls
   withdraw; press GO on the dispatcher console; confirm either automatic
   operations begin **or** the refusal reason is displayed.

That last step is the first time automatic operations will have been
attempted under QUORUM at all.
