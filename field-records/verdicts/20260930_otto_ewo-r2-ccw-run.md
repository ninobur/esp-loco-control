# Otto EWO R2 CCW run, 2026-09-30 14:40–15:04 — IR went blind at Patio; NAVI fell behind Otto

**Authorship:** This analysis is **Claude's work product** (Claude Code session,
2026-09-30), written at David's request. It has not yet been reviewed by
David or Sam. The operator observations below are David's; everything under
"Interpretation" is Claude's reading of the logs and is marked as such.

**Locomotive:** Otto, 9950011
**Build:** `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2`, boot class
`INTEGRATION_CANDIDATE_NOT_FIELD_ACCEPTED`, boot id `5A425CED22B59823`
(booted 14:39:55)
**Session direction:** CCW, start interval 044–045
**Sources:**
`field-records/logs/20260930_otto_ewo-r2-ccw-run_part{1..4}_*.log` and the
matching `.meta.json` files. These are four consecutive logger captures of a
single run, not separate runs.

## Operator observations (David, as reported)

1. Otto did not stop at Grillers.
2. The dashboard showed MM 002 while Otto was physically on the patio near
   marker 040.
3. David shut down the run (dispatcher release, logged 15:04:23.455). Otto
   stopped physically between markers 014 and 015.
4. After the analysis, David reported that the IR car was physically coupled
   to Otto and powered up during the run.

## Logged facts

### Run start and lap 1 (IR healthy)

- 14:40:21 `cmd/session_direction CCW`; 14:40:33 `cmd/start_interval 044-045`
  (start MM 45); 14:40:36 `cmd/auto 1`.
- 14:40:47–14:47:09: NAVI advanced 45 → 15 → 0 → 170 → … → 14. There were
  **202 marker advances: 50 `TARGET_CONFIRMED` (Hall + IR) and 152
  `MISSED_MAGNET` (IR distance alone)**. One continuous stretch, MM23 →
  MM121, advanced on IR distance with no Hall confirmation at all.
- The IR reason was `TRACKING` (6) with optical span of about 1100–1150
  while moving. The cumulative `ir_abort` count was 8 at the end of lap 1.
- At PWM 90 the marker rate was about one marker every 1.1–1.3 s. For
  example, MM146 → MM121 took 14:43:21 → 14:43:51, about 7.5 m in 30 s,
  roughly 250 mm/s.
- Station stops commanded (`state/station ZERO_RAMP`):

  | Station | MM | Time |
  |---|---|---|
  | Patio | 15 | 14:41:31 |
  | Bamboo | 157 | 14:42:39 |
  | Arches | 108 | 14:44:13 |
  | Grillers | 63 | 14:45:32 |
  | Patio | 15 | 14:47:05 |

  **Not verified:** whether these stops were physically at the stations.
  That needs David's confirmation. Lap-1 correctness is assumed below.

### IR failure onset: second Patio stop

- During the stop (14:47:20) the IR read `INADEQUATE_CONTRAST` (1) with
  span 15. That was also normal at the first Patio stop, and the sensor
  recovered on departure then.
- 14:47:22 `DEPART` at PWM 90. This time the sensor **did not recover**.
  Pulses went 6403 → 6404 over the next ~20 s, and span stayed around 95.
- The Patio `DEPARTED` event, which waits for 3 markers of distance, came
  at 14:48:32. That was **70 s** after departure, against about 12 s at the
  other stations. It is the first visible symptom.

### 14:47:22 → 15:04:23 (17 min at PWM 90, IR mostly blind)

- IR pulses 6403 → 6768: **365 pulses ≈ 3.5 m** at 9.652 mm per pulse.
- `ir_abort` rose from 8 to 687.
- In parts 2–4 (14:54:41–15:04:23, PWM 90), the IR reason was
  `INADEQUATE_CONTRAST` in 73% of 1 Hz loopstat samples (median span 79),
  `REACQUIRING` in 22%, and `TRACKING` in 5%. The IR speed was mostly
  `0 / STOPPED`, and `ir_stationary_pwm_warning` was set for most samples.
- For comparison, the 2026-08-26 daylight test recorded rolling span 447
  (shade) and 1855 (sun), and stationary span 31.
- NAVI advanced only MM14 → MM2: **8 `TARGET_CONFIRMED` and 0
  `MISSED_MAGNET`**. The confirmations spaced out from about 20 s apart to
  about 3 min apart.
- There were 123 `HALL_SUPPORT` events in this period. Hall support fired
  in bursts every 1–2.5 s whenever NAVI's support gating was open.
- Late confirmations landed near the low edge of the ±15% IR window, each
  pairing IR progress with whichever Hall burst came next:

  | Advance | IR distance | Mapped interval |
  |---|---|---|
  | MM5 | 289.6 mm | 325 mm |
  | MM3 | 270.3 mm | 315 mm |
  | MM2 | 289.6 mm | 330 mm |

- `ir_distance_state` stayed `HALL_IR_READY` throughout. No distance hold
  was declared.
- No station MM was reached after the second Patio stop, so no station stop
  was commanded.
- 15:04:23 dispatcher release → PWM 0. The IR counted **0 pulses** during
  the coast.
- **Bounded comparison with David's observation:**
  - NAVI first showed MM2 at 15:02:40, so David's sighting of "MM 002 with
    Otto near 040" came no earlier than that.
  - From 040 to between 014 and 015 is about 7.8 m on the surveyed map.
  - From 15:02:40 to the stop, the IR counted 20 pulses, about 0.19 m.

## Interpretation (Claude)

1. **NAVI lagged Otto; it was not ahead of him.** After 14:47, Otto kept
   moving at PWM 90, but the IR reported almost no distance. Under decision
   0116, NAVI can advance only on Hall support plus coherent IR distance, so
   it held back.
   - MM 002 with Otto at 040 means NAVI was about 133 markers behind, plus
     any whole laps.
   - Grillers and the other stations were passed because NAVI never
     reached their MMs.
2. **Otto probably lapped several times.** This is an estimate, not a
   measurement. If he held lap-1's speed of roughly 200–250 mm/s for the 17
   minutes, he covered about 200–250 m, or 4–5 laps of the 52.15 m route.
   Stopping between 014 and 015 fits a whole number of laps from the Patio
   departure, but the lap count cannot be determined from the logs.
3. **The firmware appears to have behaved as specified.** 0116 treats
   absent pulse progression as measured zero travel, "provisionally
   STOPPED, even at nonzero PWM." PWM above 60 produces only a warning. The
   0116 correction record leaves AUTO-stop-on-hold as an open operator
   policy decision. `IRSpeedWire.h` already notes that a blinded sensor on a
   moving wheel can imitate a stop, and that is what happened here.
4. **Why the IR went blind is undetermined.** The failure begins at a stop
   and never recovers anywhere on the loop, which argues against
   location-specific lighting. Competing explanations:
   - (a) The measured wheel was binding, skidding, or derailed and being
     dragged.
   - (b) The sensor was knocked, shifted, or fouled, reducing contrast while
     the wheel turned.

   Occasional `TRACKING` bursts (span up to about 1400) and the slow pulse
   trickle show the wheel sometimes turned. That argues against an
   uncoupled, stationary car. The ESP-NOW link stayed up throughout.

   David's report that the car was coupled and powered (observation 4)
   rules out an uncoupled or unpowered car. It does not separate (a) from
   (b).

   The logs lean slightly toward (b), though not decisively:
   - In the blind period, median span was about 79–95, against 15–31 at the
     lap-1 station stops where the wheel was genuinely still.
   - `ir_abort` kept climbing at about 0.5/s, against about 2 per stop in
     lap 1.

   So the optical signal varied more than a stopped wheel's, but not enough
   to form clean pulses. A wheel turning intermittently (binding or skidding
   under (a)) could produce the same pattern.

   Physical checks that would settle it:
   - whether the car is on the rails and the measured wheel spins freely;
   - the sensor's aim and gap;
   - the condition of the reflective pattern on the wheel;
   - anything that happened to the car at the second Patio stop.

5. **The "IR car coupled" dashboard checkbox was off for the whole run, and
   this does not explain the failure.**
   - Every `telem/ir` message in all four logs (9,597) carries
     `"ir_coupled":0`.
   - In R2 the `ir_coupled` command only sets `irCarCoupled`, which is
     echoed in telemetry (`state/connectivity`, `telem/ir`, `diag/ir_link`)
     and never read by NAVI. `diag/ir_link` reports
     `"paired_for_display_only":1`, the boot message states
     `"ir":"ALL_TYPE5_TO_NAVI"`, and the pairing handler warns that "every
     IR report remains visible to NAVI."
   - Lap 1 navigated correctly with the same flag at 0, and the failure
     began at ~14:47 with no change to the flag.

   The checkbox is still a potential operator trap, in the other direction
   from the one first suspected. It suggests the operator controls whether
   NAVI uses the IR car, but NAVI consumes every IR report regardless of
   the checkbox. Whether that control should be changed, relabelled or
   removed is a design question for David and Sam.

## Separate observation (not the cause of this failure)

In lap 1, with the IR healthy, 75% of NAVI's advances (152 of 202) were
`MISSED_MAGNET` steps driven by IR distance alone. That includes a run of
about 100 markers with no Hall confirmation. Whether this Hall confirmation
rate is expected under EWO has not been examined here.

## Open items for David

- Confirm whether the lap-1 station stops were physically at the stations.
- Inspect the IR car: whether it is on the rails, wheel rotation, sensor
  aim and gap, and the condition of the reflective pattern. Coupling and
  power are already confirmed.
- Design question (David and Sam): the "IR car coupled" checkbox has no
  navigation effect in R2 (interpretation 5).
- Policy decision (David's): whether a sustained PWM-without-IR-progress
  condition should hold NAVI or stop AUTO. No code has been changed.
- Record currency: the R2 record on `codex/ewo-ir-authoritative`
  (`field-records/analysis/20260930_otto_ewo_ir_authority_correction.md`)
  still says "not flashed," but Otto ran R2 on 2026-09-30 from 12:24.
  Updating it is left to that branch's owner.
