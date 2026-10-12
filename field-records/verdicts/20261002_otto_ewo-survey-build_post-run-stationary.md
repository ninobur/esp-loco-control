# Otto EWO survey build, 2026-10-02 16:51: reported failed station stop — log holds only the post-run end state

Log: `../logs/20261002_otto_ewo-survey-build_post-run-stationary.log`
(operator capture `9950011_20261002_165113.log`, 5,160 lines,
16:51:13.434 → 16:58:22.482 local, Otto 9950011 only, SHA-256
`b228f129ed9fb00ff2ed2152b86d6c80a3bcc6f9fa7b54185e7211e4c19867ab`).

Operator report (David): "Latest run. Failed station stop."
Analysis by Claude from the MQTT log and the flashed source. No firmware,
dashboard, Pi or locomotive change was made.

## Verdict

**This log does not contain the run, so it cannot establish why the station
stop failed.** It begins after the run ended. The cause of the failure is
**undetermined** pending the capture of the run itself and David's physical
observations (see "Evidence needed").

## Build and source

- Runtime sketch `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2`.
- Interpreted against `17e8ed1` on `codex/ewo-pwm-zero-localization`, the
  commit recorded as flashed in `docs/NAVI_EWO_SURVEY_MM011_014_20261002.md`
  (on that branch, `dadafa9`). The log carries no `state/bootid`, so the
  binary cannot be tied to that commit from the log alone. The loopstat
  `target_distance_mm=324` for MM012→MM013 matches the corrected survey map
  (old map: 300), which supports it.
- This verdict is committed on `claude/funny-davinci-9ksj5o`, which is based on
  `main`; `main` does not contain the EWO firmware or the survey record.

## Observations

1. **No state change in 7 minutes.** All 430 `state/loopstat` frames are
   identical except monotonic counters; `state/connectivity` changes only RSSI
   and `udp_failures` (1116 → 1129). PWM 0, `auto` 0, `running` 0, `estop` 0,
   `lowvolt` 0, voltage ≈16.0 V, `ir_pulses` constant at 4899,
   `pwm0_ir_motion` 0 throughout.
2. **No run topics.** Absent: `state/station`, `mm/marker`, `nav/evidence`,
   `state/warning`, `state/bootid`, `online`, `state/start_interval`,
   `state/session_direction`, `cmd/*`. The morning capture of the same build
   family (`20261002_otto_ewo-r2-ft2_ccw-cw-stations.log`, on
   `claude/trusting-wright-m311vv`) carried all of these.
3. **Boot time ≈16:43 local.** `hall_seen` rises at 5,000/s and was
   2,353,546 at 16:51:13.4 (≈471 s of sampling); valid IR observations
   (`ir_seen` 4650, ≈9.9/s) give the same time. The run (159 confirmations)
   therefore took place between ≈16:43 and 16:51:13, before the capture. By
   the IR Test Car sequence (10 Hz, 12043 at 16:51:13), the post-flash
   stationary check in the survey record (sequence ≈1832) was ≈16:34, so the
   run boot is a later boot than the one whose ID (`76906767B3A7C6D7`) that
   record captured. The run boot's ID is not in this log.
4. **Frozen end state.**

   | field | value |
   |---|---|
   | `mm` / `target` / `dir` | 12 / 13 / +1 (session CW) |
   | `target_distance_mm` | 324 (mapped MM012→MM013 span) |
   | `position_reliable`, `reference_ready` | 1, 1 |
   | `ir_distance_state`, `nav.state` | `HALL_IR_READY`, `NORMAL` |
   | `confirmed` / `missed` | 159 / 0 |
   | `pwm0_ir_motion` | 0 |
   | `reference` / `median5` / `hall_support` / `spatial_phase` | 1903 / 1904 / 0 / 0 |
   | `ir_health_fault` / `ir_readiness` / `ir_reason` | 6 / 3 / 1 |
   | `ir_abort` / `ir_gap` / `ir_sat` | 9 / 1 / 0 |

   `ir_health_fault=6 / ir_readiness=3 / ir_reason=1` is the ordinary
   at-rest classification: the morning log shows the same triple at rest with
   IR present. IR Test Car aborts per metre (9 over ≈47.3 m) are no higher
   than the morning (27 over ≈98.3 m).

## Interpretation (from source at `17e8ed1`; not observed)

- **Position relative to Patio.** Patio centre is MM015; CW stop offset +1, so
  CW `ZERO_RAMP` starts at MM016. MM012 is offset −3, inside Patio's CW zone
  (from MM010), where the station machine holds PWM 60. The station machine's
  only stop at offset −3 is its failure path, `PHASE_TIMEOUT` (more than
  120 s in Approach/Zone/Ramp). A normal Patio stop cannot occur at NAVI MM012.
- **AUTO enrolment was cleared.** Only three paths clear it: console
  `cmd/auto 0`, `cmd/dispatcher_release`, or `withdraw()`. E-stop, STOP and
  low voltage clear only `autoRunning`. Of the `withdraw()` callers,
  PWM-zero movement is excluded (`pwm0_ir_motion=0`) and "position
  unreliable" cannot occur after a declaration (`positionReliable_` is never
  set false). Station `MISSED` requires offset > +5. That leaves station
  `PHASE_TIMEOUT`, dispatcher release or console AUTO-off.
- **Last powered position, if Otto was not redeclared or reversed after the
  stop.** `spatial_phase=0` requires ≥200 mm of powered IR travel past MM012's
  field onset; `missed=0` requires no more than 372.6 mm (324 mm + 15%).
  `median5` 1904 against `reference` 1903 means the last powered Hall samples
  were at baseline, not in a magnet field. In the morning CW run, MM013's
  onset was 299 mm past MM012's.
- **If the whole run was CW**, 159 confirmations ending at MM012 imply the
  first confirmation was MM025, i.e. a 024–025 declaration. Not verified.

### Competing explanations the log cannot separate

1. AUTO-off or dispatcher release during the Patio approach; Otto stopped
   between MM012 and MM013, short of the platform.
2. Otto stalled or crawled in Patio's zone for 120 s and `PHASE_TIMEOUT`
   withdrew AUTO (sticky warning "Station approach failed: controlled stop;
   Manual available."). The stationary-at-PWM diagnostic requires PWM > 60,
   so a stall at the zone's PWM 60 would not raise it.
3. The IR wheel stopped producing pulses while Otto kept moving. NAVI would
   hold at MM012 (no confirmation, no miss) while Otto ran through Patio at
   PWM 60 until `PHASE_TIMEOUT`, ending physically well past the platform.
   The cumulative IR health counters would not necessarily show a wheel that
   simply stopped turning.
4. The failure occurred elsewhere and this end state followed manual moves or
   a redeclaration, in which case it says nothing about the failure.

Where Otto physically came to rest (relative to MM012, MM013 and Patio) and
whether any operator command was sent are the quickest discriminators.

## Survey region coincidence

The only change in the survey build was intervals MM011–MM014, and the end
state lies inside them. The morning capture (same Otto and IR Test Car, old
map) measured IR distance between consecutive confirmed field onsets. Across
322 consecutive pairs, IR/old-map was 0.993.

| interval (index) | old map | survey (flashed) | IR CW 13:36 | IR CCW 13:25 |
|---|---:|---:|---:|---:|
| MM011–012 (11) | 290 | 335 | 299 | 299 |
| MM012–013 (12) | 300 | 324 | 299 | 280 |
| MM013–014 (13) | 315 | 360 | 328 | 309 |

- The morning CW distances fall inside the new ±15% windows (smallest
  low-side margin 14 mm, MM011–012). The map change alone therefore does not
  explain a CW non-confirmation of MM013 if the physical behaviour matched the
  morning.
- The survey values and the IR onset-to-onset distances disagree by 9–13%
  over these three intervals (CW sum 926 mm, CCW 888 mm, survey 1019 mm). Onset
  distances are not centre-to-centre distances; which measurement describes
  the railway is not established here. In CCW, the morning distances clear the
  new lower bounds by only 4.6 mm (MM013→012) and 3 mm (MM014→013).

Both are recorded as evidence. Neither the survey nor the map is changed.

## Capture and loss observations

- The capture has no retained-topic burst at its start, so a missing
  `state/warning` is **not** evidence that no warning was raised.
- Loss counters accumulated during the run boot: NSR UDP failures 1116 (91 at
  the post-flash check), trace `consumption_drop` 1330, `nsr_native_drop` 598,
  `nsr_hall_drop` 5, `hall_q_drop` 58, `pub_drop` 4. Any NSR1 capture of the
  run is materially incomplete.
- `diag/ir_link.seen` equals valid plus `invalid_wire` exactly; about half of
  received IR radio frames are rejected (≈10/s at rest). The morning log shows
  the same ratio, so this is a standing condition, already noted in the survey
  record.

## Repository condition (reported, not changed)

Decision numbers 0119 and 0120 differ between branches:

- `codex/ewo-pwm-zero-localization` (flashed): 0119 = PWM-zero IR movement
  retains interval and reacquires coordinate; 0120 = PWM-zero movement
  requires operator position verification and declaration.
- `claude/trusting-wright-m311vv`: 0119 = firmware builds numbered and
  registered; 0120 = PWM-zero displacement retains the interval and resets
  only the within-interval IR coordinate.

The morning verdict was judged against the second 0120; the flashed build
implements the first. This run did not exercise either (`pwm0_ir_motion=0`).

## Evidence needed

1. The logger capture covering ≈16:43–16:51:13 (possibly an earlier file on
   the Pi).
2. Where Otto physically came to rest, relative to MM012, MM013 and Patio.
3. Whether AUTO-off, STOP, E-stop or dispatcher release was used, and any
   dashboard warning text.
4. Whether Otto was moved or redeclared before 16:51:13, the declared
   start interval, and which stations (if any) completed earlier in the run.
5. Any NSR1 capture for the run boot, with the loss counters above in mind.

## Limits

MQTT log only, post-run, no event topics, no boot ID. Interpretations above
are derived from source and from the morning log, not from observation of
this run. Not field acceptance of anything.
