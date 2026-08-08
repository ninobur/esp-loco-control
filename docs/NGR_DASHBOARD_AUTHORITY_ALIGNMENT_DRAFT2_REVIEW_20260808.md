# Review — NGR console authority alignment spec, Draft 2

Status: **review comments. No code written, no spec edited.**
Date: 2026-08-08
Reviewer: Claude (chat)
Document under review: `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 2,
2026-08-08, author Claude Code
Repository state: `9459e36`
Requested by: operator

---

## §0 Scope and method

This reviews Draft 2 only. It does not propose an alternative spec, and it does
not authorize implementation.

Every claim below was checked against source at `9459e36` rather than against
recollection or against the spec's own account of the source:

- `firmware/QUORUM/QUORUM.ino`
- `server/ngr_app_v1_10_9.py` (the live console)
- `server/ngr_app_v1_10_0.py` (subscription set)

File and line references are to that commit. Where a handler was located by
pattern rather than by line, it is cited by its topic constant.

---

## §1 Verdict

**Structurally sound. Ready to implement once F1 and F2 are ruled and F3 and F4
are edited in.**

The diagnosis is correct. Fault A (retained dropped), Fault B (the 1 Hz alert
overwriting enlistment), D-a, D-c and D-d were all confirmed in source. The
enrolled-versus-running conflation is exactly as described, and P4 is correctly
identified as the primary fix — repairing the retained drop alone would indeed
have changed nothing visible.

The document's discipline is also worth recording, because it is the reason this
review is short: rulings are separated from analysis, defects are numbered and
traced to proposals, and §9 states what is deliberately excluded. Four findings
follow. Two need an operator ruling, two need a spec edit, four are minor, and
two threads can be closed as already correct.

---

## §2 Findings requiring an operator ruling

### F1 — R7 opens a second trap: E-STOP has no clear path once enlisted

**Severity: high. Safety-relevant.**

§4.5 identifies that R7 traps a locomotive by withdrawing its setup controls,
and closes that trap with R9. The identical argument applies to E-STOP, and the
spec does not run it.

Verified:

| Fact | Evidence |
|---|---|
| The dispatcher E-STOP is a one-way assert — it always publishes `"1"` and can never publish `"0"` | `ngr_app_v1_10_9.py:666-667`, `pub_dispatcher()` |
| The only clear path is the loco-page toggle, which reads current state and publishes the inverse | `ngr_app_v1_10_9.py:2169-2173` |
| R7 withdraws that control on enlistment | spec §3 R7, §6 P5 |
| E-STOP clears `autoRunning` but **not** `autoEnrolled` | `QUORUM.ino`, `T_CMD_ESTOP` / `T_CMD_ESTOP_ALL` handler |

Consequence: an enlisted, E-stopped locomotive cannot be un-stopped from the
surface that stopped it. A path exists — Release, which ungreys the loco page,
then clear E-STOP there — but it crosses a different door and is nowhere
documented.

§4.4 argues persuasively that E-STOP is "the last item in the withdrawal, not an
exception to it." Accepted. But the corollary is that the dispatcher console must
be able to clear what it sets. As written the spec establishes that the
dispatcher can stop an enlisted locomotive, which is true, and leaves the reader
to assume it can restart one, which is not.

**Ruling needed.** Either:

- **(a)** the dispatcher E-STOP becomes a toggle — `pub_dispatcher()` gains a
  payload argument, and the dispatcher console renders E-stopped state; or
- **(b)** it stays one-way, and §4.4 documents the Release-first recovery path
  explicitly, with a step in the §10 field check.

(a) is the smaller surprise for an operator under pressure. (b) is less code.

### F2 — R6 places a safety property in the least durable layer

**Severity: high. Safety-relevant.**

D-d is confirmed. `cmd/auto` sets `autoEnrolled` with no motion guard and does
not zero the throttle on enrollment; disenrollment does zero it
(`QUORUM.ino:2536-2540`). The asymmetry is real.

R6 states that a moving locomotive's enlistment is refused. As scoped, that
refusal exists **only in the console**. Any other publisher — `mosquitto_pub`, a
second browser tab, an operator's phone alongside a laptop, the voice layer on
the horizon — enrolls a rolling locomotive into precisely the state §4.7
describes: *"Neither chamber actively in charge while the train moves."*

There is also an unstated tension with §9, which forbids client-side pre-judging
of BEGIN AUTO OPERATIONS conditions because it would constitute a second
eligibility gate. The spec applies strict layer-boundary discipline to one door
and deliberate console-side strictness to another. §4.5 gestures at this ("the
console will be deliberately stricter") but only in respect of pre-flight, not as
a stated principle.

**Ruling needed.** State plainly which of these is true:

- **(a)** R6 is a **convenience**. D-d remains an open hazard, owned by Station
  Stop v1, and the spec says so in one line; or
- **(b)** D-d is a **prerequisite**, not a deferral, and the console work does
  not ship without the firmware guard.

As written, "no firmware change of any kind" (§9) standing next to R6 reads as
though the hazard is closed. It is not.

---

## §3 Findings requiring a spec edit

### F3 — §7 option (a) reintroduces ghosts on the authority topics

**Severity: medium. Wrong in the dangerous direction.**

Option (a) — accept the first retained value for `auto`, `estop`,
`session_direction`, `start_interval` on connect, then resume ignoring — does not
distinguish a live locomotive from a dead one. Retained state outlives the
locomotive. With Otto powered off, the broker still serves `state/auto = 1`, and
the console seeds ENLISTED for a locomotive that is not there.

That is Finding 1 of the 2026-08-07 findings inverted: over-reporting authority
rather than under-reporting it, which is the worse of the two.

QUORUM already states the governing contract, in the retained-state hazard
comment above `pubStateIntChanged()` (`QUORUM.ino`, ~line 2249):

> these retained values OUTLIVE the locomotive … The retained `online` flag,
> driven to "0" by the MQTT last will, is what makes them interpretable: any
> consumer MUST treat all of this state as stale when online is 0.

**Proposed edit.** Replace option (a)'s framing with the contract: *accept
retained state if and only if `online == 1`.* This is general rather than a
per-topic allowlist, and it is the rule the firmware already publishes against.

Implementation notes that belong in the edit:

- `online` is `ngr/loco/<id>/online` (`QUORUM.ino:1920`) — a **sibling** of
  `state/`, not a child. It is subscribed separately
  (`ngr_app_v1_10_0.py:174-179`), so arrival order relative to `state/#` is not
  guaranteed.
- Therefore hold the seed **provisional** and promote it when `online = 1`
  arrives; discard on `online = 0`. This yields option (c)'s discipline for
  free — blank until proven, never invented.

**Related, for the record, not for this iteration.** §7 describes option (b),
"ask instead of remember," as requiring firmware support that does not exist.
The publishing half already exists: `publishAllStatesRetained()`
(`QUORUM.ino:~2371`) republishes all ten state topics retained on every
successful MQTT connect, called from `attemptReconnect()`. Only a trigger is
missing — a `cmd/report` topic calling that existing function. Worth a sentence
in §7 so it is not rediscovered from first principles later.

### F4 — D-d is the only defect with no proposal

**Severity: low as documentation, but it is the vehicle for F2.**

Traceability is otherwise complete: D-a → P2, D-b → P7, D-c → P5. D-d appears in
the §4.7 table, is discussed in §9, and has no P-number and no owner.

**Proposed edit.** Give it one — either a P11 covering the firmware guard, or an
explicit deferral line naming the residual risk and its owner. Whichever F2 is
ruled, D-d should not be the one row in the defect table that leaves the document
unattached to anything.

---

## §4 Minor findings

**M1 — The dispatcher broadcast convention is honoured for exactly one command.**

| Constant | Topic | Broadcast form subscribed? |
|---|---|---|
| `T_CMD_ESTOP_ALL` | `ngr/dispatcher/cmd/estop` | **yes** (`QUORUM.ino:1956`, `2663`) |
| `T_CMD_GO` | `ngr/dispatcher/cmd/go/<id>` | no (`QUORUM.ino:1958`) |
| `T_CMD_STOP` | `ngr/dispatcher/cmd/stop/<id>` | no (`QUORUM.ino:1959`) |

So the console's BOTH buttons assume a convention that exists for one of three
dispatcher commands. P2's fan-out in Flask is the right fix for this iteration.
Recording the asymmetry lets a future firmware decision — subscribe to broadcast
`go`/`stop` as `estop` already does — be made deliberately rather than
rediscovered.

Historical note, offered because it changes P2 from workaround to restoration:
`ngr/dispatcher/cmd/go` without a suffix was correct in the ESP-NOW era. The
Dispatcher ESP32 subscribed to exactly that topic and fanned it out as an ESP-NOW
broadcast to `BROADCAST_ID`. When the Dispatcher ESP32 left the path and
locomotives went direct to MQTT, the fan-out left with it. The BOTH button is a
fossil of a translator that no longer exists, and Flask is now the only place
that fan-out can live.

**M2 — `/dispatcher/endcto` hardcodes Otto and Toby.** Hans (2095111) receives no
dispatcher release (`ngr_app_v1_10_9.py:2088-2092`) and is absent from the
`allowed` set for per-loco go/stop (`:2083`). Same family as D-a. If Hans is out
of scope by intent, say so; if not, it is a third fan-out gap.

**M3 — The GO gate list is eight, not seven.** `ALREADY_RUNNING` is published by
the GO handler (`QUORUM.ino:2541+`) and is absent from the findings table. The
list drifted before any console code existed to consume it. That is the argument
for **Q3 → raw strings**: a translated list is a second copy of firmware
knowledge, and this copy drifted within a day. Recommend raw, with translation
deferred until the strings are stable.

**M4 — P4 should carry a general rule, not only a specific fix.** Fault B's root
cause is that the console permits a 1 Hz channel to write a field of record
(`ngr_app_v1_10_9.py:545-547`, which writes `st["auto"]` and marks it fresh,
overriding `state/auto` mapped at `:402`). P4 fixes `auto`. Without a stated
rule — **the alert stream is never the source of record for any authority
state; it may report `autoRunning` only** — the next key added to the alert JSON
recreates this bug exactly.

---

## §5 Confirmed correct — these threads can close

**C1 — R10 is already firmware behaviour, not new work.** The `T_CMD_RELEASE`
handler clears `autoRunning`, `autoEnrolled`, commanded PWM and the station
mission, and publishes a warning and nav state. It does **not** touch
`sessionDir` or `navMm`. §4.6's argument for retaining orientation and location
across release is sound, but it is defending something the firmware already does.
R10 is a confirmation, not a requirement to build.

**C2 — §4.4's firmware position holds.** `T_CMD_ESTOP_ALL` is genuinely
subscribed (`QUORUM.ino:2663`), so withdrawing the loco-page E-STOP does not
orphan door 1. Decision 0013 needs no change. This is subject to F1, which
concerns the *clear* path, not the *set* path.

---

## §6 Summary of proposed changes to Draft 2

| # | Section | Change | Type |
|---|---|---|---|
| F1 | §4.4, §3 R4, §10 | Dispatcher E-STOP clear path — toggle, or document Release-first recovery | ruling |
| F2 | §3 R6, §9, §4.7 | State whether R6 is convenience or D-d is a prerequisite | ruling |
| F3 | §7 | Gate the retained seed on `online == 1`; note `online` is not under `state/#`; note `publishAllStatesRetained()` exists | edit |
| F4 | §6 | Give D-d a P-number or an explicit deferral with an owner | edit |
| M1 | §4.7 D-a | Record the broadcast-convention asymmetry and the ESP-NOW provenance | edit |
| M2 | §4.7 | Hans absent from dispatcher release and go/stop | edit |
| M3 | §5 Q3 | Eight gates, not seven; default Q3 to raw strings | edit |
| M4 | §6 P4 | Add the general rule about the alert stream | edit |
| C1 | §4.6 | Note R10 is already firmware behaviour | edit |

---

## §7 Suggested additions to the §10 field check

The existing five steps are well chosen. Two are missing, both covering
failure modes introduced by this iteration rather than fixed by it:

6. **Enlisted E-STOP and recovery.** Enlist Otto, E-STOP from the dispatcher
   console, then clear it and restore manual control. Confirms F1 either works
   or is documented.
7. **Cold console against a powered-down locomotive.** Restart the console with
   Otto switched off. It must **not** display ENLISTED. Confirms F3 — and this
   is the test option (a) as drafted would fail.

---

## §8 References

- `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md` — Draft 2, under review
- `docs/NGR_DASHBOARD_FINDINGS_20260807.md` — findings this spec answers
- `docs/CTO3/AUTHORITY_MODEL.md` — model of record
- `docs/decisions/0013-bicameral-four-door-authority.md`
- `firmware/QUORUM/QUORUM.ino` at `9459e36`
- `server/ngr_app_v1_10_9.py` at `9459e36` — live console