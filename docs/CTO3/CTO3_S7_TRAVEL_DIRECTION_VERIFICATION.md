# CTO3 §7 travel-direction verification — QUORUM 1.6 source audit

Date: 2026-08-06
Auditor: Claude (source verification, per CTO3_SPEC §12 step 1)
Subject: firmware/QUORUM/QUORUM.ino (QUORUM_1_6, commit tree as of f057e1a)
Question (CTO3_SPEC §7): does current QUORUM derive travel direction as
`travel_direction = session_direction XOR motor_reverse`, or does it score
against session direction alone?

---

## Verdict

**QUORUM 1.6 already derives travel direction correctly. No firmware change
and no QUORUM spec amendment to the derivation are required.** The navigator
never scores against session direction alone; every navigation, scoring,
odometry, station, and diagnostic site reads the derived `navDir`.

One documentation gap and two future obligations are noted below; none blocks
auto reverse on the derivation ground §7 raised.

## Evidence

**1. The derivation exists, at a single assignment point.**
`applyDirection()` (QUORUM.ino:1428) is the only place `navDir` is assigned:

```c
int8_t derived = (sessionDir==MAP_UNSET) ? MAP_UNSET
               : (motorDirection==DIRECTION_FORWARD ? sessionDir : oppositeDir(sessionDir));
```

This is exactly §7's XOR: forward keeps the session direction, reverse flips
it. The design comment (lines 269–278) states the invariant: *"sessionDir
declares which MAP direction the motor travels when it is FORWARD; navDir is
therefore DERIVED, never assigned"* — introduced after Codex found three ways
the 2_1-era independent variables could disagree.

All four rows of §7's table are realized: e.g. facing CW + motor reverse →
`navDir = CCW` → all scoring and odometry run in the CCW map.

**2. Scoring uses the derived direction.** The quorum scoring loop
(line 793) tests `dnaAt(routeMod(r->navMm + navDir*QUORUM_OFFSETS[c]))`.
`dnaAt()` is an absolute, direction-independent polarity lookup; direction
enters only through the signed offset traversal, which `navDir` carries.

**3. Every other direction-consuming site reads `navDir`, none reads
`sessionDir`.** Grep over the sketch: `sessionDir` appears only in the
derivation itself, telemetry publication (nav payload publishes *both*
`navDir` and `sessionDir`, line 2078), the NAV_READY gate, and its own
command handler. Sites verified on `navDir`: odometer advance (1086),
adoption displacement (945), retraction (999, 1005), conservation interval
index (1259), station offsets (1634, 1649), lost-envelope estimate
(2026–2042), start-interval declared-end selection (2321–2322), force_lost
displacement (2449).

**4. Reversal mid-interval odometry is handled** (F3, fixed in 1.5,
present at 1452–1453): on a real direction change the odometer steps back
along the old direction so the standard advance lands on `navMm` again;
recovery evidence is fully reset (§6.3), since readings collected in one
direction cannot be scored in the other.

**5. No stale-`navDir` hole via NEUTRAL.** NEUTRAL deliberately skips the
derivation (an interlock, not a third travel direction — comment 1429–1432).
The one path that could have gone stale — changing `cmd/session_direction`
while in NEUTRAL — cannot: the handler (2271–2292) forces
`DIRECTION_FORWARD` through `applyOperatorDirection()`, which calls
`applyDirection()`. Both direction commands are refused while energized
above the dead zone and refused entirely under AUTO
(`AUTO_IN_CONTROL`, 2280, 2377).

**6. Start-interval declaration is travel-direction-aware.** The handler
(2294–2332) accepts the console's geometric ascending pair and picks the
declared end by `navDir` — CW declares at the lower end, CCW at the upper —
matching "the next marker physically met," including the facing-CW-but-
reversing case.

## Findings that are NOT defects, for the record

- **R21 spec documentation gap.** `docs/QUORUM_v3_0_implementation_spec.md`
  uses `navDir` throughout and specifies the direction-change reset (§6.3),
  but never states the derivation formula itself; it lives only in firmware
  and version-history comments. A one-line R21 amendment stating
  `navDir = FORWARD ? sessionDir : opposite(sessionDir)` would close the
  gap CTO3_SPEC §7 worried about at the spec level. David's call; cosmetic,
  not blocking.
- **§3 broadcast obligation is future work, not a regression.** QUORUM 1.6
  contains no ESP-NOW at all (verified by grep); the self-truth broadcast
  that must publish the *derived* travel direction is CTO3 layer 3, to be
  built. Current MQTT nav payloads publish both `navDir` and `sessionDir`,
  satisfying reference-frame honesty for the console today.
- **Auto reverse itself does not exist yet.** AUTO never commands a
  direction change in 1.6 (direction commands are operator-chamber and
  refused under AUTO). When CTO3 adds an auto reverse primitive (§6
  "reverse/switch when topology permits"), it must (a) answer the bicameral
  chamber question per decision 0002/0013, and (b) route through
  `applyDirection()` — the single assignment point — so the F3 odometer
  step-back and §6.3 evidence reset apply by construction. Bypassing
  `applyDirection()` is the one way a future change could recreate the bug
  §7 guards against.

## Consequence for the CTO3 plan

§12 step 1 is satisfied: the derivation is verified present and correct
against source. Step 2 (INA219 restoration, decision 0012) is unblocked by
this verification.
