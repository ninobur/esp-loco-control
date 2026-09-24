# NAVI COHERENCE 0.6 PROXIMAL_R1 + IR_SPEED_R1

## Manual Throttle Correction (IR_SPEED_R2)

Prepared, not flashed: manual PWM uses the full 0..255 hardware range.
The inherited 120 profile ceiling remains for AUTO only. Enrollment,
emergency stop, low-voltage handling and ramp behavior are unchanged.
See `docs/NAVI_MANUAL_THROTTLE_FULL_RANGE_20260923.md` and
`tools/test_manual_pwm.py`. Boot identifier ends in `IR_SPEED_R2`.

## IR Speed Telemetry Revision (2026-09-23)

Flashed to Toby and dashboard deployed 2026-09-23; not field accepted.
Boot name ends in `PROXIMAL_R1_IR_SPEED_R1`.
Same Arduino sketch filename and NGR-Files shortcut. Navigation, stations,
motor control and existing Hall-derived `telem/speed` are unchanged.

Additive `telem/ir` (also `diag/ir_link`) fields: `ir_valid`, `ir_mmps`,
`ir_pkph`, `ir_speed_reason`, `ir_coupled`, `ir_window_us`, `ir_delta_pulses`,
`ir_pitch_um`, `ir_speed_authority:OBSERVE_ONLY`. Missing speed is null, not zero.
Nominal speed uses successive 1 Hz samples of healthy same-epoch cumulative
pulses, TX capture timestamps, and TX pitch (this car: 9652 um per pulse).
pKPH = mm/s / 5.37325, a prototype/house unit, not physical km/h.

Confirm **IR car coupled** on the dashboard after each Toby boot or pairing
change. This sends new `cmd/ir_coupled` 0/1; it affects speed display only.
Uncheck before uncoupling. Radio pairing is not physical coupling proof.
Fresh stationary zero is valid only with coupling confirmed. Hall advance
without IR pulses or three seconds of powered zero makes speed unavailable,
with an explicit diagnostic, not an inferred locomotive stall or stop.

First bench checks after review/flash: coupled stationary, hand roll, pause,
restart, uncheck coupling, power off IR, restore IR. Expect zero, positive,
zero, positive, unavailable, unavailable, warmup then measurement respectively,
provided optical health remains READY. Do not run AUTO to test this display.
No TX/RX update is needed. See `docs/NAVI_IR_SPEED_TELEMETRY_20260923.md`.

## Prior Recovery Revision

**PROXIMAL_R1 was flashed 2026-09-23; not field accepted.**
Same Arduino filename and NGR-Files shortcut. The boot sketch name identifies
PROXIMAL_R1. The previous CAL0_FIX1 observation-only build is preserved at89dc369.

Recovery now uses epoch-tagged measurements: no whole-route search, absolute
+/-10-MM outer boundary, physical filtering before scoring, ten physical-position
history slots including UNKNOWNs, actual-observation denominator, unique strictly
better candidate only, retained history after correction. Single wrong poles and
tied alternatives retain the incumbent. Established recovery without continuous
IR travel holds its position. Starting declarations remain locally provisional.

`diag/recovery` exposes reason, scores, actual observation count, reference MM,
travel and candidate masks. Bit0 means-10, bit10 means0, bit20 means+10.
reference_mm=-1 / travel_mm=null mean unavailable. `diag/ir_health` now reports
shadow:0 and health_revision:PROXIMAL_R1. IR remains a measurement source, not a
position decider. The existing ref_basis:NAV05_ACCEPTED identifies inherited
MM-reference plumbing, not the replaced recovery algorithm.

See docs/NAVI_PROXIMAL_R1_IMPLEMENTATION_20260923.md and decision0098 for choices,
tests and limits. This does not implement the other Twenty Questions changes
(70-count Hall, 650ms fallback, background missed-marker inference). UNKNOWNs
are materialized when a later Hall observation supports a multi-marker advance.
The existing15% nominal screen is not a certified distance error bound.

Independent review and a manual field check precede any operating-baseline
claim. Upload Toby only at115200, Serial115200; keep Hall clear during the2s
baseline, confirm PROXIMAL_R1 and record30-60s stationary. First moving check
manual only, declared physical interval/direction. Stop for unexpected behavior.
No TX/RX/Pi update is required. No flash performed during preparation.

## Historical CAL0_FIX1 Handoff (Superseded Below)

Toby field-test build, 2026-09-23. **Not field accepted.**
Updated with `CAL0_FIX1`: accepts the deployed TX's zero calibration ID
without claiming validated distance. Health JSON identifies this revision,
reports `calibration_id`, and explicitly states `distance_bounds: UNVALIDATED`.
Repeat the stationary check before authorizing motion. Optical contrast faults
remain faults; this update only removes the incorrect packet rejection.
Derived from the complete 0.5 AUTO_ENABLED tree. AUTO remains enabled; all
navigation, Hall, station, recovery and legacy movement decisions remain 0.5.
The new IR health consumer observes alongside them and cannot command movement
or supply evidence to Navigator. It adds processing and telemetry load, not a
claim of zero runtime overhead.

## Flash and field test

Open `NAVI_COHERENCE_0_6_IR_HEALTH.ino` in this directory. Use ESP32 Dev Module,
upload speed **115200**, Serial Monitor **115200**. Keep the repository layout:
the sketch includes shared IR architecture from `../../../../reference/NAVI_COHERENCE/`.
Do not copy only the INO elsewhere. The familiar NGR-Files shortcut points here.

1. Flash Toby only. Keep clear of magnets during the two-second Hall baseline.
   Confirm the 0.6 boot banner and the saved IR pairing. No TX/RX reflash is
   required by this build.
2. Before moving, confirm fresh `diag/ir_health` packets on the Pi and record
   30-60 seconds stationary. A contrast fault during this dwell is useful
   evidence, not a reason to relabel the sensor healthy.
3. Declare the actual starting interval and direction normally. Run the first
   lap manually at a comfortable steady speed, with one 20-30 second stop and
   restart. Note approximate time, MM interval and direction.
4. If operation is normal, run another lap; include sunlight/shade boundaries
   if available and note their times. An AUTO/station lap can follow using the
   existing controls. Stop if locomotive behavior is unexpected.

This is a diagnostic trial, not permission to rely on unvalidated IR for safe
navigation. No software flash or motor command was performed during preparation.

## Indicators

Serial prints `[IR HEALTH]` with health, readiness, epoch, epoch activity and
MM-reference availability, explicitly marked SHADOW ONLY. MQTT adds the existing
loco topic prefix plus `diag/ir_health`, nonretained. No Pi UI change is required.
Heartbeat is one second; changed states are coalesced to at most five reports
per second. Counters preserve transition totals, not a lossless event timeline.

- `health`, `readiness`, `fresh`, `age_ms`, `detector_reason`: instrument and
  transport observations, not interpretations of commanded locomotive motion.
- `epoch`, `epoch_active`, `epoch_starts`, `epoch_ends`, `epoch_reason`:
  uninterrupted measurement lifetime; an ended epoch cannot reopen.
- `epoch_mm`, `delta_pulses`, `delta_us`: measured travel within continuity.
  Unavailable values are JSON `null`; valid unchanged measurement is zero.
- `ref_valid`, `ref_mm`, `ref_ms`, `ref_event`, `ref_alignment_us`, `distance_mm`:
  NAVI-owned shadow MM reference aligned to the original Hall event time,
  not to its roughly 400 ms later judgment time.
- `ref_basis: NAV05_ACCEPTED`: identity comes from the existing navigator,
  including its corrections. This is NOT independent proof of MM correctness.
- `accepted`, `rejected`, `duplicate`, `old`, `queue_gaps`, `changes`: diagnostic
  admission and transition counts. Join epoch IDs with `loco_boot`; IDs restart
  when the locomotive reboots, not when the reference is cleared.

The monitor validates paired-source wire data separately from the old adapter.
Freshness is one second and nearest-snapshot Hall alignment is at most 150 ms,
matching existing gates, not newly certified accuracy. Silent gaps and receive
queue losses end epochs. Unavailable history samples cannot be bypassed to use
an older healthy point. A valid calibration/pitch change starts a new shadow
epoch, even though the legacy navigation adapter still rejects that transition.
Eight retired TX boot IDs are retained; exhaustion requires pairing again.

## Limits and verification

Fresh READY zero counts preserve continuity; `unreliableSamples` alone does not
end it. PRIMING and REACQUIRING are healthy but not measurement-ready. Current
TX can report INADEQUATE_CONTRAST after a stop; that honestly ends continuity.
This receiver-only addition does not repair the TX stationary-readiness issue.
Navigation declarations/direction changes clear the shadow reference, not the
instrument epoch. New READY data restores odometry immediately; only a later
accepted, alignable Hall event establishes a new shadow MM reference.

The twenty-question changes to Hall threshold, fallback timing, mapped window
semantics and proximal correction are intentionally not implemented here.
Existing values remain 38 counts, 500 ms fallback and the 0.5 correction logic.

Original pre-fix verification: ESP32 core 3.3.11 compile PASS (1,012,715 bytes flash, 72,204 static
RAM). Only three existing Adafruit_INA219 enum warnings; no sketch warnings.
ASan/UBSan host monitor tests PASS; 11 actual JSON payloads parsed, maximum
546 bytes against a 960-byte buffer. Existing navigator tests PASS (2,052
offset corrections, 6,840 single-misread holds), station tests PASS (32 cases).
All eight baseline headers and two original host tests are byte-identical to
0.5. See `docs/NAVI_COHERENCE_0_6_IR_HEALTH_20260923.md` for design and evidence.

Example host check from repository root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/tests/test_ir_health_monitor.cpp -o /tmp/test_ir_health_monitor
python3 firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/tests/test_health_json.py /tmp/test_ir_health_monitor
```
