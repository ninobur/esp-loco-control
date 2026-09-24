# CTO3 implementation and field evidence review

Date: 2026-09-24. Reviewer: Codex.
Scope: source/history inspection and host role tests; no control edits, deployment,
flashing, or live train commands. Repository inspected at `50730d2`.

## Conclusion and operator ground truth

David reports: **"the follower kept rear-ending the leader."** This is the
operational result, not something successful role or throttle messages can
overrule. Coordination did not reliably maintain physical separation.

My earlier agreement that CTO3 coordination worked and only navigation failed
was not established by an implementation review. It was too broad. This review
finds useful implemented coordination mechanisms, NOT an accepted collision-free
system. Nor does it establish which exact version or incident caused each contact.

Historical documents (`CTO2_HARVEST.md`, decision 0031) attribute CTO2 collisions
to confidently wrong peer positions. That is a recorded historical diagnosis,
not independent proof here that every subsequent collision had that cause.
Do not silently attach David's report to a particular August session/build.

## What was actually implemented

Inspected both the current QUORUM lineage and the historical field article:
`31c4898:firmware/QUORUM/QUORUM.ino` (QUORUM 1.16R).
Historical line numbers below belong to that git object, not today's file.

- **Position-based clearance:** `ctoGapAhead`/`ctoGapBehind` (3896-3904)
  compare direction-aware arcs between consist boundaries, not four-block
  occupancy. The v1 boundary implementation explicitly omits navigation
  uncertainty. These are marker-space bounds, not continuous measured clearance.
- **Traffic separate from mission:** `ctoDesiredPwm`, `requestPwm`, and
  `requestPwmOver` (2379-2415) preserve the uncapped operating request.
  `ctoLimitPwm` (4101 onward) constrains it, and `ctoService` (4293-4308)
  reapplies the constraint every loop. Lifting a hold restores the preserved
  request through a ramp. This is real implementation, not just a design proposal.
- **Following does not require a paired leader:** the limiter scans fresh,
  same-direction peers. Role pairing separately influences service behavior.
- **A hold before the station ramp need not fulfill the visit:** traffic can
  stop a follower while its station phase remains Approach/Final, allowing it
  to continue toward the station after clearance. This is not a blanket
  guarantee: phase timeouts and the Ramp completion rule still interfere.
- **The leader no longer waits for follower arrival:** the Dwell code
  (2633 onward) records removal of an Arches deadlock on August 14. Opening the
  gap belongs to leader departure; following separation is the follower's limit.

Current source counterparts: `firmware/programs/QUORUM/QUORUM.ino`,
`ctoDesiredPwm` at 2868, `serviceStations` at 2983, `ctoGapAhead` at 4443,
`ctoLimitPwm` at 5315, continuous enforcement around 5610.

## Why a zero cap does not prove collision avoidance

The historical constants (533-551) select 18 markers for slowing, 9 for a
stop request, and 6 for the claimed clearance invariant. The source itself
warns that changing the stop gap from 12 to 9 reduces experimental margin:
crossing the 6-marker boundary cannot brake harder because zero was already
requested at 9.

All traffic reductions use 200 ms per PWM decrement. `servicePwmRamp`
(2417-2443) changes actual PWM by only one step per eligible call. Therefore
60 to zero takes approximately 12 seconds electrically, potentially longer
with loop delays. This is NOT a claim that the train physically moves for the
whole 12 seconds: it can stop before PWM reaches zero. It IS proof that a
zero target does not immediately remove applied propulsion.

Even the contact guard uses that same ramped zero request. The controller has
no remaining-distance/closing-speed calculation proving that its chosen ramp
will preserve the clearance. Matching a moving leader's PWM is likewise not
matching its speed across differing locomotives, loads, or grades.

These are concrete limitations relevant to rear-ending, not an established
incident diagnosis. Distinguishing wrong reported separation from insufficient
response requires a collision-linked trace of both positions, freshness,
boundaries, requested/applied PWM, and physical observations. This review did
not replay such a trace. No new stopping or emergency policy is approved here.

## What the written field records establish

- `field-records/20260813_QUORUM_1_14_FIRST_CONTACT.md`: peer discovery while
  stationary; explicitly no moving-train acceptance.
- `field-records/20260815_QUORUM_1_16R_BUBBLE_TEST.md`: records following at a
  9-marker gap and later caps of 42 PWM at 12 markers and zero at 6. Also
  records a bad position declaration and radio-staleness fleet stops.
  These are evidence that coordination paths engaged, not proof of physical
  separation throughout the run. The radio channel explanation was not proven
  by contemporaneous channel telemetry.
- `field-records/20260816_QUORUM_1_16R_SECOND_SESSION.md`: records displaced
  magnets, repeated navigation corrections, a role remaining LEADER after
  its partner moved ahead, and 17 STALE fleet stops plus two NO_POSITION stops.
  Thus even these records do not support "navigation was the only problem."

Evidence limitation: these are reviewed written run reports. The August 15/16
raw logs were not located in the tracked-file search or independently replayed
here. David's physical contact observation prevents interpreting partial
telemetry successes as overall operational acceptance.

## Relevant inheritance in current NAVI

Compared with `NAVI_COHERENCE_0_6_IR_HEALTH/Stations.h` and
`RecoveryControl.h`, plus the sketch's `stationService`:

1. Current-position acquisition (-10 through +5) removes the requirement to
   have crossed the old approach trigger. Keep that improvement.
2. The station task still has a phase identity and a 120-second watchdog.
   In historical CTO3, a traffic hold during Approach could consume this
   timer; preserved PWM intent alone did not preserve the station task forever.
3. Current NAVI starts Dwell when Ramp sees actual PWM zero (Stations.h:345),
   not when NAVI establishes arrival at a physical destination. Ramp-start
   offset and destination are not interchangeable.
4. Depart completion still uses stop-offset plus three markers (line 361).
   This is procedure-derived visit bookkeeping, not a general destination model.
5. `RecoveryControl.h:8` rejects MISSED/PHASE_TIMEOUT trial results before
   adopting their reset state; the sketch withdraws AUTO on that rejection.
   That is a newer coupling between station procedure and movement authority,
   not simply the historical CTO3 response (which reset and requested cruise
   only when AUTO and navigation allowed it).

The useful inheritance is **preserve the task while applying current traffic
constraints**. It is not "restore yesterday's phase regardless of position."
On clearance/restart, NAVI should evaluate the destination, current position,
motion, interval requirements, and intervening traffic. Stopping short for
traffic does not itself fulfill a platform visit. This restates David's
direction; it does not select a new overshoot/timeout policy, which is deferred.

## Host verification and limits

Built today's actual QUORUM source with its existing host harness. The README's
include paths initially failed after repository relocation; adding
`-I firmware/programs` resolves `../common/IRSpeedWire.h` without editing files.

```sh
clang++ -std=c++17 -Wno-format \
  -I firmware/programs/QUORUM/tests/shim \
  -I firmware/programs/QUORUM -I firmware/programs \
  -o /private/tmp/cto-audit-quorum-harness \
  firmware/programs/QUORUM/tests/harness.cpp
```

The unmodified `test_cto_roles.py` initially failed 15 checks because it hardcodes
Otto as self while the active profile is Toby. Reran in memory with ME=9950012
and PEER=9950011, leaving firmware and tracked tests unchanged: **27 checks
passed**. This exercises current role formation, inversion recovery, and dwell
selection. It does NOT test physical stopping distances, prove historical
1.16R passed these tests, or establish collision-free operation.

Reproduction:

```sh
python3 -c 'import runpy,sys; d=runpy.run_path("firmware/programs/QUORUM/tests/test_cto_roles.py",run_name="cto_audit"); f=d["main"]; f.__globals__["ME"]=9950012; f.__globals__["PEER"]=9950011; sys.argv=["test_cto_roles.py","/private/tmp/cto-audit-quorum-harness"]; f()'
```

No firmware modified or flashed. No full ESP32 compile or physical test in this
review. Existing unrelated dashboard and documentation changes left untouched.
