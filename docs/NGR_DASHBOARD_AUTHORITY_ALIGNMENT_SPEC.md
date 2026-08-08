# NGR console — authority alignment and the AUTO handoff

**Draft 3** — incorporates the 2026-08-08 Draft 2 review
(`NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT2_REVIEW_20260808.md`): F1/F2
ruled by the operator, F3/F4 edited in, M1–M4 adopted, C1/C2 closed.
Review revalidated against HEAD (`ede7a08`, QUORUM 1.9) before adoption —
all findings survive; line references shifted only. Draft history in git.

Status: **ready for implementation review (CODEX, Sam). No code written.**
Date: 2026-08-08
Author: Claude Code
Controlling: `docs/CTO3/AUTHORITY_MODEL.md`, decision 0013, operator
rulings (§3)
Findings answered: `docs/NGR_DASHBOARD_FINDINGS_20260807.md`
Live console: v1.10.9 on ngr-pi (`/home/david/ngr_app.py`)

An earlier attempt was written as code without being proposed first and
was reverted in full (`8cd1802`). This document proposes; it does not
implement.

---

## §0 Terminology

| Term | Values | Meaning | Owned by |
|---|---|---|---|
| **ORIENTATION** | CW / CCW | which way round the loop | operator / dispatcher — part of the *orders* |
| **DIRECTION** | forward / reverse / stopped | linear motion | operator in MANUAL; see §5 Q4b |

CTO3 §7 restated: `travel_orientation = session_orientation XOR
motor_reverse`. Wire topics keep their present names; documentation and UI
adopt the ruling. "Stopped" maps onto the firmware's `DIRECTION_NEUTRAL`.

---

## §1 The problem in one paragraph

Pressing AUTO on the locomotive page appears to do nothing but flash the
display. The locomotive is innocent: it enlists every time. The console is
structurally incapable of showing it (two independent faults, §4.2), and
separately offers no way to begin automatic operations from that page —
because beginning them is a dispatcher function and belongs there.

---

## §2 Operating model (fidelity check)

Per `AUTHORITY_MODEL.md`, stated back before proposing.

**Manual is the operator's, completely.** Constraints are simulation
artifices (ramping), mechanical reality (low-voltage cutoff), and a very
few safety limits. **Manual runs without telemetry** — navigation and
position are conveniences, never preconditions. Any "computer says no" in
the manual path is a defect.

**The dispatcher starts and stops automatic operations, not
locomotives.** BEGIN means *"begin doing what you need to do"* — not
"move forward." STOP can mean pause. E-Stop is for emergencies. The
dispatcher *initiates* sub-routines without running them.

**One operator at a time.** Enlistment is the locomotive operator's
voluntary act; only the dispatcher can discharge. An enlisted locomotive
accepts no commands from the loco page.

**Enrolled ≠ running.** `cmd/auto` crosses into the AUTO chamber and
authorizes nothing to move. The dispatcher's command, after its gates,
permits motion.

**In AUTO the locomotives are the intelligence.** The dispatcher is
authority, not brains.

**Visibility is not a chamber privilege.** A manual locomotive must remain
visible to automatic ones.

**The four doors:** E-STOP, enlistment, release/END, dispatcher STOP.

**Governing principle** (operator, 2026-08-07): *"Starting auto operations
must not be done casually, since a manual operator has no protocols and
few rules but auto operations do."*

---

## §3 Operator rulings

R1–R10 dated 2026-08-07; R11–R12 dated 2026-08-08 (review findings F1,
F2).

| # | Ruling |
|---|---|
| **R1** | **GO belongs to the dispatcher console. Period.** No launch control on the locomotive page. |
| **R2** | **Rename to BEGIN AUTO OPERATIONS** — "assess the situation," not "step on the gas." Labels for pause/end per CODEX: distinct **PAUSE AUTO OPERATIONS** (dispatcher STOP: clears running, keeps enlistment) and **END AUTO OPERATIONS — RETURN TO MANUAL** (release: clears both). The two outcomes are materially different and must not share a label. |
| **R3** | **Iteration 1 scope:** BEGIN means *"perform the action required for your starting position within the orders you have."* No general assessment engine. |
| **R4** | **E-STOP stays on the locomotive page, operable in MANUAL only**, inert once enlisted. Console control, not the firmware door (§4.4). |
| **R5** | **A train must be stopped to initiate auto operations.** The autopilot conception is retired. |
| **R6** | **A moving locomotive's enlistment is refused, with a notification** — never accepted-and-stopped. |
| **R7** | **Manual controls grey out on enlistment** — throttle, DIRECTION, and E-STOP, no carve-out. |
| **R8** | **The absence of change is the signal.** Enlisted = AUTO lights, controls grey; refused = nothing changes. The console renders enlistment from reported state, **never optimistically on button press.** |
| **R9** | **ORIENTATION and LOCATION are required at and prior to enlistment.** Order fixed: orientation → location → enlist. |
| **R10** | **Orientation and location are retained on release.** *(Confirmed already firmware behaviour — review C1: `T_CMD_RELEASE` touches neither `sessionDir` nor `navMm`. Confirmation, not new work.)* |
| **R11** | **(F1) The dispatcher E-STOP becomes a toggle.** It can clear what it sets, and the dispatcher console renders E-stopped state. The surface that stopped an enlisted locomotive can restart it. |
| **R12** | **(F2) The motion refusal lives in firmware.** QUORUM's `cmd/auto` handler refuses enlistment while energised, with a published reason — whatever the command source. The console's R6 behaviour is presentation of that refusal, not the enforcement of it. |

---

## §4 Root cause and evidence

### 4.1 The two flags

| Published | Variable | Meaning |
|---|---|---|
| `state/auto` | `autoEnrolled` | enlisted |
| alert `"auto"` | `autoRunning` | automatic operations under way |

The console has **one** `auto` flag fed from both, and the alert wins.

### 4.2 Why AUTO looks dead — two independent faults

1. The AUTO control navigates and redirects — the flash is a page reload.
2. `cmd/auto 1` publishes; the locomotive genuinely enlists.
3. `state/auto = 1` is published **retained**; the console discards all
   retained messages. **Fault A.**
4. Independently, the console writes `st["auto"]` from the 1 Hz alert —
   `autoRunning` — which overwrites enlistment back to `0` within a second
   and marks it fresh. **Fault B, dominant.**
5. The console displays MANUAL for an enlisted locomotive, permanently.

Field evidence 2026-08-07: `state/auto = 1` on the broker while the live
alert stream showed `"auto":0` all day. Enlistment works; automatic
operations have never been exercised under QUORUM.

### 4.3 E-STOP: console vs firmware (review C2 — closed)

R4 needs no constitutional change. Firmware door 1 stands (E-STOP acts in
either chamber; `T_CMD_ESTOP_ALL` broadcast is genuinely subscribed, so
withdrawing the loco-page control does not orphan the door). The loco
page's E-STOP is simply the last item in R7's withdrawal. Recovery of an
enlisted, E-stopped locomotive is R11's dispatcher toggle — the gap the
review's F1 identified: `pub_dispatcher()` could only assert `"1"`, so
the dispatcher could stop what it could not restart.

### 4.4 R9 and R7 are coupled

R7 greys the loco page's setup controls; the dispatcher console has none.
Without R9, a locomotive enlisted without pre-flight would be permanently
stuck. R9 closes the trap R7 opens. Firmware position (console
deliberately stricter, no firmware change needed): `cmd/session_direction`
refuses only while running; `cmd/start_interval` has no AUTO guard.

### 4.5 Why R10 is safe

Navigation observes in both chambers — a locomotive *driven* manually
carries its position with it; QUORUM refuses BEGIN on NO_QUORUM, so a
genuinely lost locomotive cannot be handed over. The residual is manual
*handling* (`QUORUM_HAND_REPOSITION_HAZARD.md`); mitigation is the
operational rule: re-declare after handling. The affirmative case is
safety, not convenience: **the transponder must stay on** — clearing setup
on release would blind AUTO locomotives to the one train whose behaviour
they cannot predict (CTO2 failure W3).

### 4.6 Defects

| # | Defect | Proposal |
|---|---|---|
| **D-a** | Dispatcher **BOTH** GO/STOP publishes suffix-less `ngr/dispatcher/cmd/go`, which no locomotive subscribes to. *(Review M1: the broadcast convention exists for exactly one command — `estop`. The suffix-less topic is an ESP-NOW-era fossil: the Dispatcher ESP32 used to subscribe to it and fan out over ESP-NOW to BROADCAST_ID; when locomotives went direct to MQTT the fan-out left with the translator. Flask is now the only place it can live — P2 is restoration, not workaround. A future firmware decision to subscribe broadcast go/stop as estop does should be made deliberately.)* | P2 |
| **D-b** | GO refusals published on `state/station` are discarded by the console; a refused GO is indistinguishable from a dead button. **Eight** gates, not seven — `ALREADY_RUNNING` (review M3). | P7 |
| **D-c** | The loco page offers manual controls to an enlisted locomotive — violating one-operator-at-a-time. | P5 |
| **D-d** | `cmd/auto` has no motion guard and does not zero the throttle on enrollment; a rolling locomotive can be enlisted into a state where neither chamber is actively in charge. | **P11** (firmware, per R12) |
| **D-e** | *(Review F1)* The dispatcher E-STOP is a one-way assert; once R7 greys the loco page there is no path to clear an E-stopped enlisted locomotive from the surface that stopped it. | **P12** (per R11) |
| **D-f** | *(Review F3)* A naive retained seed shows ENLISTED for a powered-off locomotive — retained state outlives the locomotive. Over-reports authority: the worse direction. | P8 as revised |

Scope note *(review M2)*: **Hans (2095111) is out of scope for this
iteration.** He is absent from the console's dispatcher release and
per-loco go/stop sets; that pre-existing gap is recorded here, not
repaired. Say so if he should be in scope.

---

## §5 Open — non-blocking

**Q4b — Does the locomotive take over DIRECTION on enlistment?** Operator
thinking (tentative): throttle zero at enlistment; DIRECTION passes to the
locomotive. CODEX preference: enlistment requires zero throttle and
transfers DIRECTION authority without immediately changing direction;
BEGIN then selects direction from mission + orientation. Firmware scope —
belongs with Station Stop v1 follow-up; its eventual ruling needs a
decision record. Related: the GO handler's silent REVERSE→FORWARD flip is
superseded by whatever is decided here.

**Q3 — Refusal wording: RESOLVED to raw strings** unless the operator
overrules. The two reviews disagreed (CODEX: plain English at the surface
+ raw in the packet log; Draft-2 review: raw only, translation deferred).
Draft 3 adopts **raw** on the strength of M3's evidence: the translated
list drifted from the firmware within a day of being written, before any
code consumed it. A translation layer may be revisited once the strings
are stable.

---

## §6 Proposals

Console unless marked. P11 is firmware (R12).

**P1 — GO and STOP stay on the dispatcher console.** No launch control on
the locomotive page. *(R1)*

**P2 — Fix BOTH by Flask fan-out** to per-locomotive topics. *(D-a, M1)*

**P3 — Three distinct dispatcher labels** *(R2, CODEX)*:
`BEGIN AUTO OPERATIONS` · `PAUSE AUTO OPERATIONS` (STOP: keeps
enlistment) · `END AUTO OPERATIONS — RETURN TO MANUAL` (release: clears
both). Labels only; topics unchanged.

**P4 — Track ENLISTED and RUNNING separately.** ENLISTED from
`state/auto`; RUNNING from the alert. **General rule (M4): the alert
stream is never the source of record for any authority state — it may
report `autoRunning` only.** Without the rule, the next key added to the
alert JSON recreates Fault B exactly. *(Faults A/B)*

**P5 — Withdraw all manual controls on enlistment** — throttle,
DIRECTION, E-STOP — rendered from `state/auto`, never optimistically.
*(R7, R8, D-c)*

**P6 — Refuse enlistment without pre-flight**, stated reason, sequence
orientation → location → enlist. *(R9)* Console-side; presentation of
firmware refusals from P11 where they exist.

**P7 — Surface the locomotive's `state/station` responses** on the
dispatcher console, beside the button pressed; raw reason strings (Q3).
*(D-b)*

**P8 — Authority state survives reconnect, gated on `online == 1`**
*(D-f, review F3 — replaces Draft 2's option (a)).* The firmware already
publishes the governing contract: retained state is interpretable only
while the retained `online` flag (last-will-driven) reads 1. `online` is a
**sibling** of `state/`, subscribed separately, so arrival order is not
guaranteed: hold any retained authority seed **provisional**, promote it
when `online = 1` arrives, discard it on `online = 0`. Result is
blank-until-proven — never invented, in either direction. Per-connection
seed state; retained values carry no freshness timestamp; live messages
always supersede the seed *(CODEX P8 notes, adopted)*. For the record:
option (b) "ask instead of remember" is nearer than Draft 2 claimed —
`publishAllStatesRetained()` already republishes everything on each
connect; only a `cmd/report` trigger is missing. Not this iteration.

**P9 — Enlistment stays on the loco page; release does not.** *(model §2)*

**P10 — Never gate manual on telemetry.** Standing constraint restated.

**P11 (FIRMWARE, R12) — Motion guard on enlistment.** `cmd/auto 1` is
refused while `motorIsMoving()`, publishing a stationPublish refusal
(`ENLIST_REFUSED` / `WAIT_FOR_STOP`) like the existing gates. Enforced by
the authority's owner, whatever the command source. Version-bumped
firmware change, sequenced with Station Stop v1's 1.9 line; the console's
R6 behaviour becomes presentation of this refusal. *(D-d, F4: D-d now has
an owner and a P-number.)*

**P12 (R11) — Dispatcher E-STOP becomes a toggle.** `pub_dispatcher()`
gains a payload; the dispatcher console renders E-stopped state per
locomotive and can clear it. The loco-page E-STOP toggle remains, greyed
per R7 when enlisted. *(D-e)*

---

## §7 Primary objective

> "It should at least hand off operation of the locomotive to the
> dispatcher. This function is the predicate for starting auto
> operations, our next goal."

Pressing AUTO must visibly hand the locomotive to the dispatcher: manual
controls withdraw, the page states who holds it, and the state survives
reload, reconnect, and a powered-off locomotive. Achievable without
automatic operations working at all.

---

## §8 Explicitly not proposed

- No launch or release control on the locomotive page.
- No client-side pre-judging of BEGIN AUTO OPERATIONS conditions — QUORUM
  owns those gates and names its refusals. (The enlistment-side stance is
  the same after R12: firmware owns the motion refusal; the console
  presents it. The console's pre-flight refusals (P6) are the one
  deliberate console-side strictness, because the dispatcher console has
  no setup controls to recover with — §4.4.)
- No firmware change **except P11**, which is R12's explicit ruling.
- No change to the E-STOP firmware door.
- No MQTT topic renames. No Hans scope change (§4.6 note).

---

## §9 Traceability — defect → proposal → field test

| Defect | Proposal | Field test (§10) |
|---|---|---|
| Fault A (retained dropped) | P8 | T5, T7 |
| Fault B (alert overwrites enlistment) | P4 | T3, T5 |
| D-a (BOTH dead) | P2 | T8 |
| D-b (refusals invisible) | P7 | T2, T9 |
| D-c (manual offered while enlisted) | P5 | T3, T6 |
| D-d (rolling enlistment) | **P11** | T2 |
| D-e (E-STOP one-way) | **P12** | T6 |
| D-f (seed ghost) | P8 revised | T7 |
| R2/R3 semantics | P3 | T9 |
| R9 pre-flight | P6 | T1 |

## §10 Field check, if approved

1. **T1** — enlist with pre-flight incomplete → refused, reason shown.
2. **T2** — complete pre-flight, enlist while rolling → **firmware**
   refusal (P11) shown by the console.
3. **T3** — stop, enlist → AUTO lights, manual controls grey (throttle,
   DIRECTION, E-STOP), page states dispatcher holds it.
4. **T4** — reload the page → still ENLISTED.
5. **T5** — restart the console with Otto powered and enlisted → still
   ENLISTED (seed promoted on `online = 1`).
6. **T6** — E-STOP from the dispatcher console while enlisted, then
   **clear it from the dispatcher console** (P12); confirm recovery
   without crossing the release door.
7. **T7** — restart the console with Otto **switched off** → must NOT
   display ENLISTED (the test Draft 2's option (a) would have failed).
8. **T8** — BOTH buttons reach both locomotives (fan-out verified in the
   packet log).
9. **T9** — press BEGIN AUTO OPERATIONS → operations begin **or** the raw
   refusal reason is displayed. First attempt at automatic operations
   under QUORUM.

## §11 Sequence

1. CODEX/Sam review of this draft. 2. P11 implemented as the next QUORUM
version bump (with Station Stop v1's 1.9 under review, sequencing per
CODEX). 3. Console implemented as `ngr_app_v1_10_10.py`. 4. Deploy by
scp + restart (no repo clone on the Pi). 5. §10 field check, in order.

## §12 References

`AUTHORITY_MODEL.md` · decision 0013 ·
`NGR_DASHBOARD_FINDINGS_20260807.md` ·
`NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT2_REVIEW_20260808.md` ·
`QUORUM_HAND_REPOSITION_HAZARD.md` · CODEX Draft-2 comments (operator
relay, 2026-08-08)
