# CTO3 Bubble v1 — two-train operations specification

Status: **design specification, settled with the operator 2026-08-13.**
Not an implementation authorization. Every firmware change it implies owes its
own scope under v1.14 planning, the replay suite, and a decision record.

Provenance: operator dictation and rulings in the 2026-08-13 session; the CTO2
audit (`docs/CTO2_AUDIT_DISPOSITION.md`, amended per Sam's provenance review);
`LL_CTO_v2_1_GoldCore.ino` and `LL_LBO_v1_2.ino` (iCloud, Close Train
Operations / LBO lineages), read 2026-08-13; measured deceleration data from
`field-records/20260813_TOBY_QUORUM_1_13_VERIFICATION.md` session captures.

---

## 1. Purpose and scope

Two locomotives run the Lowline loop in the same direction, forming a moving
bubble: leader and follower, coordinating spacing and station work between
themselves, with no collisions and no operator intervention. This is the first
CTO3 operating routine (`CTO3_INTENT_BASELINE.md`: the turntable's first
record). Lowline only. Two locomotives in v1; nothing here may assume the count
cannot grow (`MAX_CTO_PEERS` is 8).

Independent speed (IR) is **not** required for v1 — operator ruling
2026-08-13. Stops are marker-based with measured, field-verified deceleration
distances.

## 2. Governing rules

1. **Truth, not authority** (Sam's restatement, adopted in the audit
   amendment): *a locomotive may not transmit another locomotive's required
   behavior. Each locomotive broadcasts its own physical and operational
   truth. Leader/follower relationship may be established from shared peer
   information and retained — and stated — as coordination state.*
2. **Each locomotive assesses the situation and decides its own action**
   (operator, 2026-08-13, and the LBO/CTO lineage from its first sketch).
3. **Silence never clears anything.** A hold releases on positive evidence
   only — a peer *reporting* a position that clears the conflict — never on
   timeout or absence. Present in `LL_LBO_v1_2` (conflict clears only when the
   peer reports a *different* block) and carried forward unchanged.
4. **Fleet stop** (operator ruling, 2026-08-13): **NO_QUORUM on any locomotive
   stops every locomotive.** Enforced by absence, not announcement: each
   locomotive runs only while every expected peer is currently fresh (≤ 3 s)
   and reporting a quorum-holding navigation state. A locomotive that loses
   position may also lose its radio — the rule cannot depend on the lost
   locomotive saying so. Untestable with one locomotive; owed a decision
   record when two-train testing begins.
5. **Bicameral authority holds** (spec §0.2, decision 0002): missions are
   dispatcher authority (CE is a mission command); **spacing decisions are
   onboard, always**. E-stop crosses everything.
6. **LOST ends AUTO** (M4). Under two-train operations there is no
   slow-and-continue: a locomotive without position stops and hands control
   back. Rule 4 stops the other one.

## 3. Geometry and numbers

All distances in **markers (MM)** — operator ruling on decision 0030: markers
are what the railway measures. Nominal spacing 300 mm, route range 280–355.

| quantity | value | source |
|---|---|---|
| Train extent, ahead of Hall | +2 MM | decision 0030 |
| Train extent, behind Hall | −4 MM | decision 0030 |
| Hall-to-Hall contact point | **6 MM** | operator: "can be 6 mm apart and not touch" |
| **Separation target (stop behind a stopped leader's Hall)** | **12 MM** | operator ruling |
| Usable margin inside the target | 6 MM | 12 − 6 |
| Deceleration distance, cruise → stopped | **~6 MM** | measured, below |
| **Begin-deceleration gap (moving follower, stopped leader)** | **≥ 18 MM** | 12 + 6 |
| Restart clearance | 12 MM | matches `TRAFFIC_RESTART_HALL_GAP_MM` |

Two independent convergences worth noting: the 18 MM begin-decel gap equals
CTO2 r12's approach-ladder start (18 Hall-to-Hall), and the operator's 12 MM
stop target equals r12's restart clearance — chosen this session from the
measured numbers, not copied.

### Measured deceleration (Toby, 2026-08-13, 38 station stops)

- Ramp rate **200 ms per PWM step** — 12.0 s from PWM 60, 14.4 s from PWM 72,
  both exactly 0.2 s/step.
- Cruise → PWM 40: ~5 MM (operator's figure, consistent with the ramp rate).
- PWM 40 → stopped: **under 1 MM** (~100–150 mm). PWM 40 is ~57 mm/s and the
  model zero is PWM 25.4; motion ceases ~3 s into the 8 s ramp tail.
- Stops land 1–3 MM past station centre — the leader's stopped position
  carries ±1 MM scatter, so **12 MM is a minimum to hold, not a point to
  hit.**

Tuning hierarchy (operator): these are the defaults. First tuned **per
station**, then **per locomotive** as an override in `LL_LocoConfig_<id>.h` —
defaults live with the station, overrides live with the locomotive.

## 4. Peer truth on the wire

Port from CTO2 r12 per the audit — structs frozen (`CTO2_VERSION 3`,
`ESPNOW_VERSION 14`, per `docs/CLAUDE.md`):

- `CtoPeerPacket` at 2 Hz (`CTO2_STATUS_INTERVAL_MS 500`), broadcast always —
  every powered locomotive, in every mode, moving or not.
- 8-slot registry keyed by loco ID; a report from one locomotive can never
  overwrite another's record.
- Freshness: `CTO2_PEER_STALE_MS 3000` gates every logic path (GoldCore's
  `peerIsLogicFresh` discipline; LBO v1.2's missing freshness gate is the one
  defect of that lineage **not** ported).
- **Stopping intent is already on the wire**: `stationPhase` (`ZERO_RAMP` =
  committed to stop), `trafficPhase`, `rampPwm` (actual PWM at 2 Hz — a
  follower sees a falling ramp ~24 samples before the leader is stationary),
  `motionState`, `running`. No new fields required for v1 movement logic.
- **Position must be an occupancy bound, not a sensor point** — the v1.14
  work item: navigation bound composed with configured extent, applied by the
  producer (decision 0030). The r12 fixed 5/5 offsets are the one rewrite.
- **Role echo**: after latching, each locomotive broadcasts its derived
  relationship ("I follow 9950011") as its own state — same category as
  r12's `trafficStopForId`, which already crossed this line healthily.
  Candidate slot: the vestigial `TrainPacket.roleId`; confirm in v1.14 scoping
  rather than assume.

## 5. Roles

**Derived, never negotiated or transmitted as instruction.** Each locomotive
computes its role from its own peer table. The GoldCore Q1/Q2 pattern,
markers replacing blocks:

- **Q1 — TRAILING**: a fresh, same-direction peer's occupancy bound overlaps
  or immediately precedes mine ahead → I follow; hold behind.
- **Q2 — LEADING**: a fresh, same-direction peer sits within pairing range
  behind me → I lead; at a station this means holding for the follower.
- Neither → **UNPAIRED**, which is a real operating state: run the normal
  solo mission.

Rules carried from the lineage and this session's analysis:

1. **Evaluate on your own physical events** (your marker acceptances), not on
   packet arrival — no message-ordering races.
2. **Latch at pairing range**, where geometry is decisive. Q1/Q2 cannot fire
   at a distance, so the antipodal tie (85 vs 86 MM) can never produce a
   split-brain pairing.
3. At long range, derived order (smallest my-rear-to-your-front leads;
   ~12 MM hysteresis band resolved by **lower loco ID leads**) decides only
   *who waits* during formation — an action whose worst failure is both
   trains waiting: visible, recoverable, never a collision.
4. **Roles persist once latched** (`CTO3_DESIGN_NOTES.md`: assigned once,
   stay assigned) and dissolve on: peer staleness > 3 s (fleet stop follows
   anyway), direction change, CE severance, or operator reset.
5. **Role echo cross-check**: if the two broadcast conclusions disagree,
   that is a detectable fault → both hold. Never proceed on disagreement.

## 6. Formation

Same-direction trains at cruise never close their gap; formation requires one
train to wait, and the locomotives arrange it themselves:

1. Both running, unpaired, far apart. Each derives the provisional order
   (rule 5.3) from the 2 Hz broadcasts.
2. **The derived leader holds at its next station** (a normal station stop
   that simply does not depart). The follower keeps running its mission.
3. The follower approaches under §7: begins deceleration at ≥ 18 MM from the
   leader's Hall, stops at 12 MM.
4. At pairing range Q1/Q2 latches on both sides; role echoes confirm; the
   bubble exists. Choreography (§8) takes over — the formation stop *is* the
   first "follower arrives at hold point" event.

If both provisionally conclude "leader" (tie mis-resolution at range): both
wait at stations, the gap freezes, the role echoes disagree, and the fault is
visible on the console. Conservative by construction.

## 7. Movement and stopping — one profile, one flag

**One deceleration profile** for every planned stop. What differs is only the
resume action, selected by a flag:

```
DECEL (identical)  →  STOPPED  →  wait for positive clearance  →  resume
                                                  ├─ STATION: crawl to platform
                                                  └─ TRAFFIC: ramp to cruise
```

- **Trigger** (follower, leader stopped or committed to stopping —
  `ZERO_RAMP` / `trafficPhase` / falling `rampPwm` / stationary position):
  begin deceleration in time to stop **12 MM behind the leader's Hall**;
  with ~6 MM of stopping distance that means acting by **18 MM**.
- **The rule is universal** — the follower never rear-ends the leader
  *anywhere*: mid-route stop, station, failure. Station choreography changes
  only what happens after the stop, never whether it happens.
- **Clearance is positive evidence** (rule 2.3): the leader's broadcast
  position opening the gap — for traffic, beyond 12 MM; for stations, the
  leader's rear bound clear of the platform zone. Never a timeout.
- Resume-to-cruise and crawl-to-platform reuse the existing station machine's
  ramps; no new motor behaviour is invented.

## 8. Station choreography (operator dictation, 2026-08-13)

Lead and follower approach a station as a formed bubble:

| step | actor | behaviour |
|---|---|---|
| 1 | Leader | Standard station stop at the platform (current stop behaviour, ~M+2). |
| 2 | Follower | Stops **12 MM behind the leader's Hall** (§7; supersedes the earlier "lead stop −5" dictation). |
| 3 | Leader | Follower's arrival (read from its broadcast: stopped, in position) starts the leader's **10 s** release dwell; then the leader departs. |
| 4 | Follower | When the platform area is positively clear — leader's rear bound out of the platform zone — **crawls to the platform** and executes the standard station stop. |
| 5 | Follower | Dwells **20 s** (operator revision of the earlier 15 s), then departs to the next station. |
| 6 | Both | Rule at all times: the follower does not rear-end the leader if the leader stops — §7 is always armed, independent of station state. |

Dwells are fixed for v1; randomization (CTO2 supported ranges) comes later.
The two mechanisms stay separate, as r12 had them: **station synchronization**
(steps 1–5, the MHE lineage) and **traffic protection** (step 6, universal).

## 9. CE — severance and re-pairing

The dispatcher's CE command is a **mission** assignment (bicameral: allowed),
never a spacing decision:

1. CE severs the pair. Both locomotives return to UNPAIRED with missions:
   the leader becomes the **EXPRESS** — faster cruise, skips designated
   stops; the follower becomes the **LOCAL** — slower, every stop.
2. The express eventually closes on the local from behind. No special logic
   watches for this — it is simply §7: a moving locomotive approaching a
   slower/stopped one decelerates and holds at 12 MM.
3. At pairing range, Q1/Q2 runs exactly as at first formation. The local is
   ahead, so **the local becomes the leader and the express the follower** —
   the new relationship falls out of geometry, as it did in the original CTO.
4. Missions revert to standard bubble service on re-pairing.

## 10. What this spec does not authorize, and what it needs first

- **No firmware change is authorized by this document.** The v1.14 scope
  (occupancy bound per 0030; role derivation/latch/echo; the §7 profile and
  flag; fleet-stop enforcement) owes decision records, the replay suite, and
  `verify_inert.py` discipline per standing practice.
- **Port inventory** is the audit's: registry/packet/freshness as-is; Q1/Q2
  and approach ladder after review; fixed offsets rewritten to bounds; Hall
  timing speed coordination discarded.
- **Testing requires two locomotives.** The fleet-stop rule and everything in
  §§5–9 is untestable solo. M7's crossing test remains the goal: ten laps,
  two trains, zero collisions, zero interventions, one induced peer failure
  handled without contact.
- Open items: role-echo field placement (`TrainPacket.roleId` candidate);
  dwell randomization; platform-zone definition in markers per station;
  per-station then per-loco tuning pass (§3).

## References

- `docs/CTO2_AUDIT_DISPOSITION.md` — dispositions and the role-invariant amendment
- `docs/decisions/0030-*` — extent; `0023`/`0024`/`0025` — navigation lineage
- `docs/CTO3/CTO3_INTENT_BASELINE.md`, `docs/CTO3_DESIGN_NOTES.md`
- `docs/CTO3/resources/CTO2_BUBBLE_PRINCIPLE.txt` — the bubble concept this implements
- iCloud: `LL_CTO_v2_1_GoldCore.ino` (Q1/Q2, encounter latching),
  `LL_LBO_v1_2.ino` (peer table, positive-evidence release)
- `field-records/20260813_TOBY_QUORUM_1_13_VERIFICATION.md` — deceleration data
- `docs/CTO_RESTORATION_STATE_20260813.md` — how the milestones got here
