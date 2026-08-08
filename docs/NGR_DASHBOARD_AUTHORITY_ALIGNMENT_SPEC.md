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

## §6 Open questions — operator ruling required

**Q1 — E-Stop and the one-operator rule.** The operator has suggested that
once enlisted, even E-Stop should come only from the dispatcher. Decision
0013 and `AUTHORITY_MODEL.md` currently make E-Stop door 1: *"acts in
either chamber and overrides everything."* These conflict. Narrowing
E-Stop is a constitutional change requiring a new decision record
superseding part of 0013. Practical consideration: if an operator at the
trackside cannot stop an enlisted locomotive, the dispatcher console must
be reachable within seconds. **No console change is proposed either way
until this is ruled.**

**Q2 — GO semantics in firmware (not a console change).** The model says
GO means "begin doing what you need to do." The firmware does:

```c
autoRunning=true;
motorDirection=DIRECTION_FORWARD; applyDirection();
requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
```

— it forces FORWARD and commands cruise, i.e. "move forward." For a
locomotive starting a lap the two coincide, so this is latent rather than
broken. It stops being latent when GO is pressed while a locomotive should
dwell at a station or hold behind traffic. Flagged for CTO3; **out of
scope** here.

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
- No change to E-Stop pending Q1.
- No weakening of the firmware authority boundary to work around a console
  defect.

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
