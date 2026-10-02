# Otto EWO R2_FT2, 2026-10-02: CCW then CW station runs, and 28 PWM-zero IR displacement events during hand turning

Log: `../logs/20261002_otto_ewo-r2-ft2_ccw-cw-stations.log` (operator capture
`9950011_20261002_124930.log`, 35,797 lines, 12:49:30 → 13:37:40 local, Otto
9950011 only). Analysis by Claude from the MQTT log; operator facts are David's,
as stated in the session. Interpreted under decision
[0120](../../docs/decisions/0120-pwm-zero-ir-displacement-retains-the-interval-and-resets-only-the-within-interval-ir-coordinate.md).

## Builds observed

| Time | `state/bootid` sketch | boot_id |
|---|---|---|
| 12:49:30 | `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2` | `41425879404E3014` |
| 13:22:07 | `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2` | `CBE44DD42DFAB094` |
| 13:22:22 | `…_R2_FT2` | `B96E8A2E1FEAA4AC` |
| 13:24:05 | `…_R2_FT2` (all runs below) | `33790A909D837180` |

**Observation:** `firmware/BUILDS.md` lists EWO-13 (`…_R2_FT2`, `ab0938b`) as
"Built, reviewed; not flashed". This log shows an `…_R2_FT2` build running on
Otto from 13:22:07. Which commit was flashed is not recorded in the log. The
register is not changed here.

## Sequence

1. **CCW AUTO (session CCW, declared 045–046, AUTO 13:24:31).** Stations
   Patio, Bamboo, Arches and Grillers each went ARMED → ZONE → ZERO_RAMP →
   DWELL → DEPART. `pwm0_ir_motion` stayed **0** through all four dwells.
2. **Dispatcher release (13:30:32.95)** while running at PWM 90. PWM ramped
   90 → 60 → 28 → 0 by 13:30:35.9. MM43 was confirmed at 13:30:34.53 and the
   target became MM42. IR pulses stopped at 5444, about 20 mm past MM43.
3. **Hand turning (13:30:40.56–13:31:05.37).** David picked Otto up and turned
   it around. That is how Otto travels the opposite direction; it is not a
   motor reverse. PWM and auto were 0 throughout. The measuring wheel spun in
   hand:
   - IR pulses went 5444 → 5496, about 540 mm of wheel rotation from the MM43
     landmark, in bursts;
   - the fastest burst was 13:30:47.65–48.94, about 350 mm in 1.3 s, peaking
     at 48 mm per 100 ms;
   - in between were pauses of 6–10 s.

   NAVI logged **28 `PWM_ZERO_IR_DISPLACEMENT` events**, one per IR frame with
   pulses. `loopstat.pwm0_ir_motion` went 0 → 28. The first event was
   accompanied by `SPATIAL_INVALIDATED` and `IR_DISTANCE_HOLD`, and
   `ir_distance_state` became `FRAME_LOST_REDECLARE`.
4. **Retained throughout:**
   - `mm 43`, `target 42`, `dir -1`, `position_reliable 1`;
   - Hall reference 1903;
   - Hall sampling (`hall_seen` 1,958,525 → 2,208,544, no new queue drops).

   No Hall event was logged. Only target-related Hall evidence is published,
   and Hall at PWM 0 is non-actionable.
5. **Opposite-direction session (session CW 13:31:18, `REVERSED` evidence
   event).** David redeclared 045–046 at 13:31:26 (`DECLARED`,
   `IR_DISTANCE_READY`, mm 45 → target 46) because **he knew he had placed
   Otto in a different interval.** AUTO followed at 13:31:29.
   - MM46 was confirmed after about 87 mm of powered travel.
   - The run then went Grillers, Arches, Bamboo, Patio with normal station
     sequences. Dispatcher release came at 13:37:01.
6. **Placement check.** The `median5` polarity of all 150 MMs confirmed in the
   CW run (46 onward) matches the CCW run's record for the same MM (150/150). That supports
   the redeclared interval being physically correct.

## Verdict

- The 28 events are **one handling episode, correctly observed.** They are
  factual wheel movement at PWM = 0, with no relationship to route travel.
  They are not an IR fault and not a navigation error.
- **Under 0120,** that displacement cost NAVI only its within-interval IR
  coordinate. Interval 43→42, the target and the context were not invalidated
  by the observation.
- **The redeclaration was correct, for the operator's reason:** Otto had been
  physically placed in another interval. The PWM-zero event itself did not
  require it.
- **Implementation as observed does not conform to 0120:**
  - it reported `FRAME_LOST_REDECLARE`;
  - it warned "IR measured motion at PWM=0; map/IR relationship unreliable"
    28 times;
  - it could not have re-established the coordinate from Hall without a
    declaration.

  Here this had no effect on the run, because David redeclared before powered
  movement. The nonconforming code locations are listed in the integrated
  README section "PWM-zero IR displacement (0120)".

## Limits

- MQTT log only; no NSR1 capture. The R2 build at 12:49 cannot be tied to a
  commit from the log.
- Interval placement is supported by the polarity sequence. It was not
  measured.
