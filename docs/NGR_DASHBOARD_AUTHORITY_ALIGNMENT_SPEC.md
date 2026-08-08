# NGR console — authority alignment and the AUTO handoff

**Draft 4** — incorporates the three Draft 3 reviews (CODEX repository
review; Sam; Claude chat — preserved as
`NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT3_REVIEW_{CODEX,SAM,CLAUDE}_20260808.md`).
CODEX's five findings dispositioned: F1/F2/F4 ruled by the operator
(R13–R15), F3 resolved by the convergent asymmetric E-STOP design (all
three reviewers reached it independently), F5 adopted as wording. Sam's
two clarifications and T10 adopted. Claude's G1–G5 adopted.

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
| **R13** | **(2026-08-08, CODEX-F1) E-STOP recovery is throttle-zero, not NEUTRAL.** Clearing E-STOP no longer drops DIRECTION to NEUTRAL; DIRECTION is preserved and **zero throttle is the protection** — motion resumes only when a throttle is deliberately advanced (MANUAL) or BEGIN AUTO OPERATIONS is issued (enlisted). The dispatcher restarts an enlisted, E-stopped locomotive **with BEGIN alone** — no release, no re-setup. In the operator's words: regressing to manual to reset NEUTRAL is tedious; everything is recorded on the Pi; the operator judges whether location needs re-declaring after an incident. Consequence: the console's throttle slider must also zero on E-STOP — the operator has observed it does not, so a clear could re-command the stale slider value. |
| **R14** | **(2026-08-08, CODEX-F2) Refusals are always observable.** `*_REFUSED` responses bypass the station-transition dedup and carry a sequence number, so every operator command gets a response — repeats included. The transition dedup stays for what it was built for. |
| **R15** | **(2026-08-08, CODEX-F4) R9 is a firmware invariant.** `cmd/auto 1` is refused unless ORIENTATION is set and navigation is ready — alongside the motion guard, same pattern as R12: the rule lives where the truth lives. The console's pre-flight sequencing remains UI, not authority. |

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

### 4.3 E-STOP: console vs firmware (review C2 closed; Draft-3 F1/F3 resolved)

R4 needs no constitutional change. Firmware door 1 stands. The loco
page's E-STOP is simply the last item in R7's withdrawal.

**Recovery (R13):** E-STOP asserts PWM zero and clears `autoRunning`;
enlistment survives. Clearing it preserves DIRECTION (no NEUTRAL drop —
firmware change P14), so BEGIN AUTO OPERATIONS restarts the locomotive
directly. T6 passes without crossing the release door. The prior
NEUTRAL-on-clear design guaranteed a fresh human direction choice; under
R13 that guarantee is carried by zero throttle instead, in both chambers.

**Set/clear asymmetry (Draft-3 CODEX-F3, Claude-G1, Sam-S2 — the
convergent design):** a broadcast toggle has no unambiguous payload under
mixed per-locomotive states, and a broadcast clear would execute the
clear path on locomotives that were never stopped — including a manually
running one. Therefore: **SET broadcasts** (`ngr/dispatcher/cmd/estop`,
everyone stops — correct for an emergency); **CLEAR is per-locomotive**
(`ngr/loco/<id>/cmd/estop 0`, already subscribed, same fan-out shape as
P2); the dispatcher console renders each locomotive's E-STOP state from
the **retained `state/estop`** — never from the last command sent, never
optimistically (P4's rule, extended by Sam's S2). No ambiguous toggle
exists in the design.

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

**Q4b — Does the locomotive take over DIRECTION on enlistment?** Narrowed
by events: the **throttle clause is satisfied in advance** (Claude-G4 —
P11 refuses enlistment at any energisation, so enrollment only ever
succeeds at zero throttle; refusing beats mutating, and no zero-on-enroll
write is added). The **E-STOP recovery slice is ruled** (R13). What
remains open is only the DIRECTION-transfer semantics — CODEX/Sam
preference: transfer DIRECTION authority at enlistment without changing
its physical state; BEGIN determines what the orders require. Firmware
scope, needs a decision record; the GO handler's silent REVERSE→FORWARD
flip is superseded by whatever is decided. Non-blocking for this
iteration (T6 passes via R13).

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
*(R7, R8, D-c)* Additionally *(R13)*: **the console throttle slider
zeroes on E-STOP** in every chamber, so a later clear cannot re-command
the stale slider value.

**P6 — The console shall not issue `cmd/auto 1` until ORIENTATION and
LOCATION have been supplied** *(Sam's rewording — sequencing, not
authority)*, sequence orientation → location → enlist, with the reason
shown. Enlistment authority itself is firmware's (P11/R15); the console
presents firmware refusals. *(R9)*

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

**P11 (FIRMWARE, R12+R15) — Enlistment guards.** `cmd/auto 1` is refused,
with a published reason, when: energised (`ENLIST_REFUSED` /
`WAIT_FOR_STOP`), ORIENTATION unset (`NO_SESSION_DIRECTION`), or
navigation not ready (`NO_POSITION_DECLARE_START_MM`). Enforced by the
authority's owner, whatever the command source. **Invariants** *(Claude
G3/G4)*: `cmd/auto 0` is **never** refused — disenrollment is a safety
action that zeroes PWM; and no zero-on-enroll write is added — the guard
means enrollment only ever succeeds de-energised, and refusing beats
mutating. **Wording narrowed per CODEX-F5:** the guard proves *propulsion
de-energised*, not physical rest — a pushed or coasting locomotive passes
it; the honest physical-stop witness is decision 0005's motion witness,
recorded as the residual. *(D-d owned)*

**P12 (R11, R13, §4.3 design) — Dispatcher E-STOP: broadcast set,
per-locomotive clear.** The dispatcher console keeps a broadcast
**E-STOP ALL** (`ngr/dispatcher/cmd/estop 1`) and gains per-locomotive
**CLEAR** controls publishing `ngr/loco/<id>/cmd/estop 0` (topic already
subscribed — no firmware change for the clear path). Displayed E-STOP
state per locomotive derives from **retained `state/estop`** — never the
last command sent, never the button press *(Sam S2; P4's rule)*. No
ambiguous toggle exists; mixed and offline states render individually.
The loco-page E-STOP toggle remains, greyed per R7 when enlisted. *(D-e)*

**P13 (FIRMWARE, R14) — Always-observable command responses.**
`*_REFUSED` publications bypass `stationPublish()`'s transition dedup
(which compares event+offset and ignores the reason) and carry a
monotonic sequence number, so a repeated command yields a visible
repeated response and a changed reason is never suppressed. The
transition dedup remains for station-machine flooding, its original
purpose. *(CODEX-F2)*

**P14 (FIRMWARE, R13) — E-STOP clear preserves DIRECTION.** The
clear path no longer forces `DIRECTION_NEUTRAL`; PWM zero (asserted at
E-STOP, still zero at clear) is the protection in both chambers. An
enlisted locomotive is then restartable by BEGIN alone; a manual one
moves only when the operator advances the (now-zeroed, P5) throttle.
*(CODEX-F1, T6)*

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
- No firmware change **except P11, P13, P14** — each an explicit operator
  ruling (R12/R15, R14, R13). Sequencing with the QUORUM 1.9 line per
  CODEX (§11). *(Claude G2)*: the console iteration may ship with T2
  marked **blocked pending P11** — R6 presentation-only, D-d open and
  owned — so a firmware delay does not silently block the independently
  useful handoff deliverable (§7).
- No change to the E-STOP firmware door.
- No MQTT topic renames. No Hans scope change (§4.6 note).

---

## §9 Traceability — defect → proposal → field test

| Defect / requirement | Proposal | Field test (§10) |
|---|---|---|
| Fault A (retained dropped) | P8 | T5, T7 |
| Fault B (alert overwrites enlistment) | P4 | T3, T5, T10 |
| D-a (BOTH dead) | P2 | T8 |
| D-b (refusals invisible / suppressible) | P7 + **P13** | T2, T9 |
| D-c (manual offered while enlisted) | P5 | T3, T6, T10 |
| D-d (rolling enlistment) | **P11** | T2 |
| D-e (E-STOP recovery trap) | **P12 + P14** | T6 |
| D-f (seed ghost) | P8 revised | T7 |
| E-STOP re-commands stale slider (R13) | P5 | T6 |
| R2/R3 semantics, PAUSE≠END | P3 | T9, **T10** |
| R8 never-optimistic | P4/P5 | T3 (unreachable-loco case) |
| R9 pre-flight | P6 + **P11** | T1 |

## §10 Field check, if approved

1. **T1** — enlist with pre-flight incomplete → refused, reason shown.
2. **T2** — complete pre-flight, enlist while rolling → **firmware**
   refusal (P11) shown by the console.
3. **T3** — stop, enlist → AUTO lights, manual controls grey (throttle,
   DIRECTION, E-STOP), page states dispatcher holds it. **R8 case:**
   repeat with the locomotive unreachable — the console must show no
   change, not an optimistic ENLISTED.
4. **T4** — reload the page → still ENLISTED.
5. **T5** — restart the console with Otto powered and enlisted → still
   ENLISTED (seed promoted on `online = 1`).
6. **T6** — E-STOP ALL from the dispatcher console while enlisted;
   confirm the console throttle slider zeroes (P5) and per-loco E-STOP
   state renders from `state/estop`; **clear per-locomotive** (P12);
   confirm DIRECTION preserved (P14); press BEGIN AUTO OPERATIONS and
   confirm the locomotive restarts — recovery without crossing the
   release door.
7. **T7** — restart the console with Otto **switched off** → must NOT
   display ENLISTED (the test Draft 2's option (a) would have failed).
8. **T8** — BOTH buttons reach both locomotives (fan-out verified in the
   packet log).
9. **T9** — press BEGIN AUTO OPERATIONS → operations begin **or** the raw
   refusal reason is displayed; press it twice against the same refusal
   and confirm the second response is visible (P13). First attempt at
   automatic operations under QUORUM.
10. **T10** *(Sam)* — PAUSE/BEGIN authority persistence: enlist → BEGIN →
   PAUSE → RUNNING false, ENLISTED true, manual still withdrawn → BEGIN
   resumes without re-enlistment → END → ENLISTED false, manual returns.
   Proves PAUSE changes operating state; END changes authority.
2b. **T2 note** *(Claude G2)* — T2 tests the firmware refusal and is
   **blocked pending P11**; the console may ship with T2 outstanding,
   D-d recorded open.

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
