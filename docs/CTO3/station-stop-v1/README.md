# CTO3 Station Stop v1 — discussion specification and implementation prompt

**Status:** approved as a narrow implementation work item; field validation is
blocked until the live dashboard can enroll a locomotive in AUTO
**Date:** 2026-08-06
**Proposed field subject:** Otto (9950011), CCW, Arches only
**CTO3 plan:** `CTO3_SPEC.md` §12 step 3

## Decision requested

Approve a deliberately narrow first proof: one locomotive performs one existing
station cycle at Arches—approach, full stop, dwell, restart, and clean release of
the station state machine—with no IR authority, traffic logic, peer, or bubble.

The first source audit must answer whether a firmware change is needed at all.
QUORUM 1.8 inherits the complete station phase chain and the governing
CTO3 sequence says to preserve the QUORUM R21 single-locomotive station logic
unchanged. A controlled one-stop field run may therefore be the correct step.
Creating a second control sketch or changing code merely to produce a new version
would violate the one-sketch rule and add no evidence.

If discussion identifies a real missing capability—for example, selecting one
destination without enabling every station—that capability must be specified as a
small mission-layer addition around the station machine. It must not rewrite the
station phase logic inside this work item.

## Purpose

Prove the smallest end-to-end automatic mission behavior on the repaired,
navigation-stable locomotive:

```text
AUTO launch -> arm Arches -> approach -> final target -> zero ramp
            -> full stop -> dwell -> depart -> clear station state
```

This proves that QUORUM position can drive an onboard stop/dwell/restart sequence
without entangling it with the still-developing IR sensor or CTO3 traffic system.
It is a state-machine and authority-boundary proof, not the final CTO3 movement
service.

## Governing constraints

1. **Authority fidelity first.** Read `../AUTHORITY_MODEL.md` and explain the
   Manual/AUTO model back before editing. The station routine may write PWM only
   while AUTO is enrolled and running. The dashboard's current inability to
   enroll AUTO must not be bypassed in firmware.
2. **One control sketch.** Work branches from `firmware/QUORUM/QUORUM.ino`.
   Do not create a separate station sketch.
3. **Bicameral authority remains constitutional.** Station code may command PWM
   only while `autoRunning`. Manual remains sovereign. E-STOP remains immediate.
4. **Preserve R21 station logic.** Do not redesign `serviceStations()`, its phase
   transitions, PWM ramp ownership, NO_QUORUM reset, overshoot escape, or M+1
   fallback for this proof.
5. **No new MQTT contract unless approved separately.** Existing controller and
   dashboard behavior must continue to work.
6. **No IR dependency or authority.** IR development continues in parallel.
7. **No traffic behavior.** No peer registry, ESP-NOW, envelope, pairing, role,
   MHE, hold-behind, or bubble logic belongs in this step.
8. **No SPEED_HOLD claim.** The existing station controller is PWM-profile based.
   CTO3 §6's measured-speed staircase remains later work under §12 step 5.
9. **No final precision claim.** The ±150 mm, load-invariant distance-stop gate
   belongs to IR-backed closed-loop stopping, not this first proof.

## Existing behavior to preserve and observe

Current QUORUM defines Arches with centre 107, zone PWM 60, final PWM 45, and a
configured stop trigger two markers beyond the centre in the direction of travel.
It arms in the range M-12 through M-10, transitions through approach and zone hold,
holds zone speed through the centre, selects the final target at M+1, begins its
zero ramp at M+2, dwells for 15 seconds, then ramps to cruise and releases the
station after clearing M+5.

The primary zero-ramp trigger for a clean Arches trial is M+2. The five-second
M+1 timeout remains a safety fallback but does not count as the nominal path.

Expected station telemetry, in order, is:

1. `ARMED`
2. one or more `APPROACH` / `ZONE_HOLD` transitions as applicable
3. `FINAL_APPROACH`
4. `FINAL_TARGET`
5. `ZERO_RAMP` with `TRIGGER_M2_REACHED`
6. `DWELL_BEGIN`
7. `DWELL_COMPLETE`
8. `DEPARTURE_COMPLETE`
9. `RESET` with `DEPARTED`

The exact number of intermediate transition messages depends on the declared
starting interval and received markers; their order and phase ownership do not.

## Proposed test configuration

- Otto only; no other locomotive moving on the Lowline.
- Genuine QUORUM 1.8, verified by `[BOOT] QUORUM_1_8` plus the unconditional
  `pwm` and `v` fields on an
  `mm/marker` payload, not by `state/bootid` alone.
- Repaired Hall terminal using the short solid-wire pigtail.
- CCW session and forward motor direction.
- Declare a clean adjacent start interval sufficiently before Arches to enter the
  existing arming window naturally; do not declare inside the final approach.
- Existing Arches profile unchanged.
- Existing controller command surface: session direction, start interval, AUTO
  enrollment, GO, STOP/RELEASE, and E-STOP.
- Full MQTT capture of Otto topics plus an operator note of the physical resting
  point.

## Preconditions

Before a trial counts:

1. The locomotive identifies genuine QUORUM 1.8 by boot and marker payload
   behavior.
2. Supply, Hall resting level, and both magnet polarities have already passed the
   post-repair electrical check.
3. Otto sits powered and stationary for at least 60 seconds with zero phantom Hall
   events and reported speed 0.0.
4. Navigation setup is accepted, travel direction is CCW, motor direction is
   forward, and navigation is NORMAL before GO.
5. E-STOP is tested/available and the operator has an unobstructed physical view
   of the Arches approach.
6. Battery voltage, consist, weather/rail condition, firmware commit, and starting
   interval are written into the run note.

## Trial procedure

1. Start a fresh MQTT capture before enrollment.
2. Record the stationary witness period.
3. Set CCW session direction and a valid adjacent start interval before Arches.
4. Select forward, enroll AUTO, and issue GO.
5. Do not touch throttle or move the locomotive by hand during the station cycle.
6. Observe the complete ordered station telemetry and the physical stop.
7. Allow the full dwell and automatic restart.
8. After `DEPARTURE_COMPLETE` and `RESET/DEPARTED`, issue an ordinary dispatcher
   STOP while still well before the next station, then RELEASE to Manual.
9. Record the final physical position, any warning, any manual intervention, and
   whether the train restarted decisively without stall or hesitation.
10. Repeat from a fresh setup until three consecutive qualifying cycles have been
    obtained, or stop immediately on the first unsafe/unexplained behavior.

## Proposed acceptance gate

Three consecutive cycles pass when all of the following are true:

- Arches arms once and no other station arms during the trial.
- The phase sequence is ordered and complete.
- `ZERO_RAMP` uses the normal M+2 trigger, not the M+1 timeout.
- Actual PWM reaches zero and `DWELL_BEGIN` follows without operator help.
- The physical stop is within the existing operator-accepted Arches platform
  landing; the measured error is recorded even though no new numeric tolerance is
  imposed here.
- Dwell completes and Otto restarts without manual intervention or stall.
- Departure clears the station and returns the machine to `ST_IDLE` via
  `RESET/DEPARTED`.
- Navigation remains NORMAL and no implausible Hall speed burst occurs.
- There is no `MISSED`, `PHASE_TIMEOUT`, `DEPARTURE_SLOW`, `NO_QUORUM`, queue
  drop, E-STOP, or manual movement during the counted cycle.

Any intervention, fallback trigger, navigation degradation, or unacceptable
physical landing makes the cycle diagnostic evidence, not a pass.

## Evidence package

The verdict must retain:

- exact firmware commit and behavior-based version witness;
- raw MQTT log;
- configuration and starting interval;
- ordered station events with timestamps, offsets, commanded PWM, and actual PWM;
- marker/nav events spanning at least M-12 through departure clear;
- voltage during the cycle;
- physical stop observation and operator intervention record;
- pass/fail result for each acceptance item.

The field verdict is a separate documentation commit. A failure is not tuned away
during the same capture; preserve the first behavior, diagnose it, then approve a
separate change.

## Questions for discussion

1. Is field validation of the existing QUORUM station machine sufficient for
   §12 step 3, as its wording suggests?
2. Or does Station Stop v1 need a mission-layer way to select Arches and skip all
   other stations? If so, what existing command should express that without a
   dashboard or MQTT-contract change?
3. Is three consecutive successful cycles the right reliability gate?
4. What physical Arches landing does David accept for this pre-IR proof?
5. Should the first proof remain CCW, or is there an operational reason to choose
   CW before code review?

## Prompt for Claude Code after this discussion is approved

> Read `docs/CTO3/station-stop-v1/README.md`,
> `docs/CTO3/AUTHORITY_MODEL.md`,
> `docs/CTO3/CTO3_SPEC.md` §§2, 6, and 12, QUORUM R21 §0.2, and the
> current `firmware/QUORUM/QUORUM.ino`. First report whether the approved Station
> Stop v1 behavior already exists in the current source. Trace the Arches path
> from arming through `RESET/DEPARTED`, including every PWM write and every exit.
>
> If the current sketch already satisfies the approved contract, do not create a
> firmware diff for activity's sake and do not create a second sketch. Produce the
> exact field-test command/capture checklist and identify Station Stop v1 as a
> validation work item.
>
> If an approved requirement is genuinely missing, propose the smallest addition
> around the existing station machine before editing. Preserve its phase logic,
> the one-control-sketch rule, R21 navigation behavior, the four bicameral doors,
> current MQTT/controller compatibility, and all safety paths. Add no IR, traffic,
> SPEED_HOLD, peer, or bubble behavior. Do not infer answers to the unresolved
> questions above. After David approves the proposed delta, implement it as one
> isolated firmware commit, build both locomotive profiles, and write a separate
> implementation report. Do not flash hardware. Hand the commit to Codex for
> review before the field run.

## References

- `docs/CTO3/CTO3_SPEC.md` §§2, 6, 12, 13
- `docs/CTO3/CTO3_INTENT_BASELINE.md`
- `docs/CTO3/CTO3_S7_TRAVEL_DIRECTION_VERIFICATION.md`
- `docs/QUORUM_v3_0_implementation_spec.md` R21
- `field-records/verdicts/20260806_otto_hall-terminal-repair.md`
