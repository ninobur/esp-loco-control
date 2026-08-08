# NGR console — authority alignment and the AUTO handoff

**Draft 2** — restructured from eight incremental drafts; rulings
consolidated, analysis moved behind them. Draft 1 history is in git
(`2325144`…`a180266`).

Status: **proposal for review (CODEX, Sam). No code written.**
Date: 2026-08-08
Author: Claude Code
Model of record: `docs/CTO3/AUTHORITY_MODEL.md`, decision 0013
Findings: `docs/NGR_DASHBOARD_FINDINGS_20260807.md`
Review comments: `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT2_REVIEW_20260808.md`
— comments only; findings remain pending until operator rulings and spec edits
Live console: v1.10.9 on ngr-pi (`/home/david/ngr_app.py`)

An earlier attempt was written as code without being proposed first and
was reverted in full (`8cd1802`). Its embedded design decisions were the
operator's to make, and several were wrong against the authority model.
This document proposes; it does not implement.

---

## §0 Terminology

Two different things have been called "direction," including in earlier
drafts of this document.

| Term | Values | Meaning | Owned by |
|---|---|---|---|
| **ORIENTATION** | CW / CCW | which way round the loop | operator / dispatcher — part of the *orders* |
| **DIRECTION** | forward / reverse / stopped | linear motion | operator in MANUAL; see §5 Q4b |

§7 of the CTO3 spec restated in this vocabulary:
`travel_orientation = session_orientation XOR motor_reverse`.

Wire topics keep their present names (`state/session_direction`,
`cmd/direction`) — renaming the protocol is out of scope. Documentation
and UI language adopt the ruling. "Stopped" maps onto the firmware's
existing `DIRECTION_NEUTRAL`.

---

## §1 The problem in one paragraph

Pressing AUTO on the locomotive page appears to do nothing but flash the
display. The locomotive is innocent: it enlists every time. The console is
structurally incapable of showing it, for two independent reasons, and
separately offers no way to begin automatic operations from that page —
because beginning them is a dispatcher function and belongs there. Nothing
in this is a firmware defect.

---

## §2 Operating model (fidelity check)

Per `AUTHORITY_MODEL.md`, stated back before proposing anything.

**Manual is the operator's, completely.** The locomotive and dispatcher
have essentially no say. The only constraints are simulation artifices
(ramping), mechanical reality (low-voltage cutoff), and a very few safety
limits. **Manual runs without telemetry** — navigation, position and peer
awareness are conveniences, never preconditions. Any "computer says no" in
the manual path is a defect.

**The dispatcher starts and stops automatic operations, not locomotives.**
GO means *"begin doing what you need to do"* — not "move forward." STOP
can mean pause. Either may address one locomotive or both. E-Stop is for
emergencies. The dispatcher also *initiates* sub-routines (Circuit
Express) without running them; the locomotives renegotiate roles among
themselves.

**One operator at a time.** Enlistment is the locomotive operator's
voluntary act, like joining the service; like the service, only the
dispatcher can discharge. End Automatic Operations lives on the dispatcher
console, and an enlisted locomotive accepts no commands from the loco
page.

**Enrolled ≠ running.** `cmd/auto` crosses into the AUTO chamber and
authorizes nothing to move. The dispatcher's command, after its gates,
permits motion.

**In AUTO the locomotives are the intelligence** — they assess position,
act on it, follow rules and routines, and communicate with each other. The
dispatcher is authority, not brains.

**Visibility is not a chamber privilege.** Every powered locomotive
publishes self-truth in either chamber. A manual locomotive must remain
visible to automatic ones.

**The four doors:** E-STOP, enlistment, release/END, dispatcher STOP.

**Governing principle for this work** (operator, 2026-08-07):

> "Starting auto operations must not be done casually, since a manual
> operator has no protocols and few rules but auto operations do."

---

## §3 Operator rulings

All dated 2026-08-07 unless noted. These are settled; §5 holds what is
not.

| # | Ruling |
|---|---|
| **R1** | **GO belongs to the dispatcher console. Period.** No launch control on the locomotive page. |
| **R2** | **Rename to BEGIN AUTO OPERATIONS.** GO means *"assess the situation — where are you, is there a loco in front, are you at a station already"* — not "step on the gas." STOP becomes END/PAUSE AUTO OPERATIONS. |
| **R3** | **Iteration 1 scope:** BEGIN AUTO OPERATIONS means *"perform the action required for your starting position within the orders you have."* No general assessment engine. |
| **R4** | **E-STOP stays on the locomotive page, operable in MANUAL only**, inert once enlisted. Governs the *console control*, not the firmware door — see §4.4. |
| **R5** | **A train must be stopped to initiate auto operations.** The autopilot conception — engage while under way — is retired. It was never exercised. |
| **R6** | **A moving locomotive's enlistment is refused, with a notification.** Not accepted-and-stopped. |
| **R7** | **Manual controls grey out on enlistment** — throttle, DIRECTION, and E-STOP, no carve-out. |
| **R8** | **The absence of change is the signal.** Enlisted = AUTO lights, controls grey. Refused = nothing changes. The console must therefore render enlistment from the locomotive's reported state, **never optimistically on button press.** |
| **R9** | **ORIENTATION and LOCATION are required at and prior to enlistment.** Enlistment refuses without them. Order is fixed: orientation → location → enlist. |
| **R10** | **Orientation and location are retained on release.** A released locomotive still knows where it is and may be re-enlisted without repeating pre-flight. |

---

## §4 Root cause and evidence

### 4.1 The two flags

`state/auto` and the alert's `"auto"` are **different firmware variables**:

| Published | Variable | Meaning |
|---|---|---|
| `state/auto` ([QUORUM.ino:2353]) | `autoEnrolled` | enlisted |
| alert `"auto"` ([QUORUM.ino:2196]) | `autoRunning` | automatic operations under way |

The console has **one** `auto` flag fed from **both**, and the alert wins.

### 4.2 Why AUTO looks dead — two independent faults

1. The AUTO control is an `<a href>`; clicking navigates and redirects
   back. **The flash is a full page reload.**
2. `cmd/auto 1` publishes; the locomotive genuinely enlists.
3. It publishes `state/auto = 1` **retained** (`pub(t,b,true)`,
   [QUORUM.ino:2260]). The console discards all retained messages. **Fault
   A.**
4. Independently, the console writes `st["auto"]` from the 1 Hz alert
   ([v1.10.9:545]) — i.e. from `autoRunning`. Within a second it
   overwrites enlistment back to `0` and marks it fresh. **Fault B.**
5. The console displays MANUAL for an enlisted locomotive, permanently.

**Fault B is dominant.** Repairing the retained drop alone would have
changed nothing visible. This is exactly the enrolled-vs-running
conflation `AUTHORITY_MODEL.md` warns against: the model has two states,
the console has one.

### 4.3 Field evidence, 2026-08-07

| Observation | Source | Meaning |
|---|---|---|
| `state/auto = 1` | broker | enlistment **succeeded** |
| alert `"auto":0` all day, all three captures, 11:41–13:03 | live 1 Hz stream | **never once in AUTO running** |

Automatic operations have never been exercised under QUORUM.

### 4.4 E-STOP: console vs firmware

R4 requires **no constitutional change**, and none should be made.

- **Firmware:** decision 0013 door 1 stands — E-STOP acts in either
  chamber. It must, or the dispatcher's E-STOP could not reach an enlisted
  locomotive.
- **Console:** the loco page's E-STOP is withdrawn on enlistment along
  with every other manual control, because an enlisted locomotive is not
  the loco operator's to command.

E-STOP is the last item in the withdrawal, not an exception to it. An
enlisted locomotive is stopped from the dispatcher console, which is
another page of the same Flask app.

### 4.5 R9 and R7 are coupled

R9 is not optional once R7 is adopted. R7 greys out the loco page's setup
controls, and the dispatcher console has none. Without R9, a locomotive
enlisted without pre-flight would be **permanently stuck**: operator
locked out of setup, dispatcher unable to supply it, BEGIN AUTO OPERATIONS
refusing forever, recoverable only by release. **R9 closes a trap R7 would
otherwise open.**

Firmware position (the console will be deliberately stricter, and no
firmware change is needed): `cmd/session_direction` refuses only while
`autoRunning`, so orientation stays settable while merely enlisted;
`cmd/start_interval` has no AUTO guard at all. The console simply stops
offering the controls and refuses enlistment without them.

### 4.6 Why R10 is safe

The objection to retention was that stale declared position is
*confidently wrong* position — the failure that killed CTO2, where correct
traffic mathematics consumed a leader position asserted at certainty 1.000
after a handful of bad reads. *"CTO cannot be safer than the position
reports it consumes."*

That objection was **overstated**:

- **Navigation observes in both chambers.** QUORUM advances the odometer
  on every accepted marker regardless of who holds the throttle. A
  locomotive *driven* manually carries its position with it.
- **QUORUM refuses to launch when lost.** NO_QUORUM is a BEGIN AUTO
  OPERATIONS gate; a genuinely lost locomotive cannot be handed over.

The residual is manual **handling**, not manual **driving** — hand-pushing,
lifting, rolling to position. That is
`QUORUM_HAND_REPOSITION_HAZARD.md`, mitigated by the existing operational
rule: re-declare after handling.

And the affirmative case, which is a safety argument rather than a
convenience one: **the transponder must stay on.** A manual locomotive
running alongside automatic ones must remain visible to them, and
visibility is worthless without position. Clearing setup on release would
blind the AUTO locomotives to the one train whose behaviour they cannot
predict — CTO2 failure W3 reintroduced deliberately.

### 4.7 Additional defects found

| | Defect |
|---|---|
| **D-a** | Dispatcher **BOTH** GO/STOP publishes `ngr/dispatcher/cmd/go` with no locomotive suffix. QUORUM subscribes only to `ngr/dispatcher/cmd/go/<id>` ([QUORUM.ino:1957]). **Dead buttons, silently.** |
| **D-b** | GO refusals are published on `state/station` and discarded by the console. A refused GO is indistinguishable from a broken button. |
| **D-c** | The loco page offers manual throttle and DIRECTION to an enlisted locomotive — violating one-operator-at-a-time. |
| **D-d** | `cmd/auto` has **no motion guard and does not zero the throttle on enrollment** ([QUORUM.ino:2535]). Disenrollment zeroes PWM; enrollment does not. Enlist a rolling locomotive and it keeps rolling: `autoEnrolled` true, `autoRunning` false, so no AUTO code commands PWM while MANUAL has just been relinquished. **Neither chamber actively in charge while the train moves.** Firmware scope; see §6. |

---

## §5 Open — operator ruling still required

**Q3 — Refusal wording.** QUORUM's raw strings
(`NO_QUORUM_DECLARE_POSITION`) or plain English ("position lost —
re-declare the start interval")? Raw cannot drift out of sync with the
firmware; translated is easier to read at speed. **Cosmetic; will default
to raw if not ruled.**

**Q4b — Does the locomotive take over DIRECTION on enlistment?** Operator
thinking, explicitly tentative:

> "I think that the throttle should be at zero. Direction should be taken
> over by the locomotive once Manual control is relinquished. If you think
> about it, the throttle position beforehand is immaterial as long as the
> loco is stopped. I recall that this decision has been made differently
> in the past, this is my thoughts on it tonight."

Consistent with §0: ORIENTATION is *orders* and stays with the
operator/dispatcher; DIRECTION is *means* and would pass to the
locomotive. This retires the forward-only constraint at its root — it
existed because locomotives had to move to discover position, which is no
longer true.

Related, and superseded if Q4b is adopted: the GO handler refuses a
NEUTRAL locomotive with a stated reason but **silently flips REVERSE to
FORWARD**, which under §7 changes its map direction.

**Firmware scope, not console. Does not block this spec.**

---

## §6 Proposed console changes

**P1 — GO and STOP stay on the dispatcher console.** No launch control is
added to the locomotive page. *(R1)*

**P2 — Fix BOTH.** Fan `go`/`stop` out to one publish per locomotive on
the topic QUORUM subscribes to. *(D-a)*

**P3 — Rename** the dispatcher controls to BEGIN AUTO OPERATIONS and
END/PAUSE AUTO OPERATIONS. Labels only; topics unchanged. *(R2)*

**P4 — Track ENLISTED and RUNNING separately.** *The primary fix.*

| Console state | Source | Meaning |
|---|---|---|
| **ENLISTED** | `state/auto` (`autoEnrolled`) | dispatcher holds it; manual controls withdraw |
| **RUNNING** | alert `"auto"` (`autoRunning`) | automatic operations under way |

The alert must never write enlistment state. *(§4.1, §4.2)*

**P5 — Withdraw all manual controls on enlistment** — throttle,
DIRECTION, E-STOP. Rendered from `state/auto`, never optimistically.
*(R7, R8, D-c)*

**P6 — Refuse enlistment without pre-flight**, with a stated reason.
Sequence: orientation → location → enlist. *(R9)*

**P7 — Surface the locomotive's response on the dispatcher console**, next
to the button pressed. Subscribe to `state/station` (already arriving
under `state/#` and discarded). *(D-b)*

**P8 — Authority state must survive a reconnect.** See §7.

**P9 — Enlistment stays on the loco page; release does not.** Release is
the dispatcher's act. *(model, §2)*

**P10 — Never gate manual on telemetry.** Standing constraint, not a
change: no manual control is disabled because navigation is unset,
telemetry stale, or position unknown.

---

## §7 The retained-state problem

*(Explaining P8 — the operator asked for this in plain terms.)*

MQTT lets a publisher mark a message **retained**: the broker keeps the
last one and gives it to anyone connecting later. It is how a locomotive's
current state survives a console restart.

In July, stale retained messages from dead firmware were displayed as
current — the ghost-tile failure. The v1.10.0 fix was blunt and effective:
**ignore every retained message.**

The side effect: on reconnect the console discards the locomotive's *real
current* state along with the ghosts, and displays a built-in default
instead. The default for `auto` is `0`. It is not showing stale data; it
is showing **invented** data. The same mechanism caused the v1.10.9 Toby
bug, where a default of NEUTRAL was presented as Toby's reported
direction.

Options:

- **(a) One-shot authority seed.** On each connect, accept the *first*
  retained value for `auto`, `estop`, `direction`, `session_direction`,
  `start_interval`; then resume ignoring. Store without marking fresh.
- **(b) Ask instead of remember.** Query current state on connect. Cleaner;
  requires firmware support that does not exist.
- **(c) Show nothing rather than a default.** "—" until the locomotive
  speaks. Never wrong; blank about authority until the next change.

**Recommendation: (a)**, narrowly scoped, with (c)'s discipline everywhere
else. Being wrong about who holds a locomotive is worse than being briefly
blank, and wrong in the dangerous direction.

**Necessary but not sufficient:** without P4, fixing this changes nothing
visible (§4.2).

---

## §8 Primary objective

> "My complaint today about auto mode is that it is a non functional
> button in terms of locomotive operation. It makes the dashboard flash on
> my phone. Nothing [else]. It should at least hand off operation of the
> locomotive to the dispatcher. This function is the predicate for
> starting auto operations, our next goal."

The deliverable is narrow and testable: **pressing AUTO must visibly hand
the locomotive to the dispatcher.** Manual controls withdraw, the page
states who holds it, and the state survives a reload and a reconnect.

Achievable **without** automatic operations working at all. A locomotive
that enlists, displays as enlisted, and correctly refuses everything else
is a complete and useful result — the handoff is the predicate, not the
operation.

---

## §9 Explicitly not proposed

- No launch or release control on the locomotive page.
- No client-side pre-judging of BEGIN AUTO OPERATIONS conditions. QUORUM
  owns those gates and names its own refusals; a console that second-
  guessed them would be a second eligibility gate, which the layer-boundary
  rule forbids.
- **No firmware change of any kind.** D-d and Q4b are recorded here but
  belong to Station Stop v1.
- No change to the E-STOP firmware door (§4.4).
- No weakening of the firmware authority boundary to work around a console
  defect.
- No renaming of MQTT topics (§0).

---

## §10 Sequence if approved

1. CODEX/Sam review. Operator rules Q3.
2. Implement as `ngr_app_v1_10_10.py` — version-bump, never edit in place.
3. Deploy by scp + `systemctl restart ngr-app`; there is no repo clone on
   the Pi.
4. Field check:
   - enlist Otto with pre-flight incomplete → **refused**, reason shown;
   - complete pre-flight, enlist while rolling → **refused**, reason shown;
   - stop, enlist → AUTO lights, manual controls grey, E-STOP inert;
   - reload the page and restart the console → still shows ENLISTED;
   - press BEGIN AUTO OPERATIONS on the dispatcher console → operations
     begin, **or** the refusal reason is displayed.

The last step is the first time automatic operations will have been
attempted under QUORUM.
