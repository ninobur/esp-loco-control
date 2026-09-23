# IR session record - September 19, 2026

This records activities and observations from the session. Times are PDT.
User-reported phase times are approximate. Receipt timestamps and reconstructed
device event times are distinct.

## Objective and authority

IR measures an unpowered wheel on a separate battery-powered ESP32 test car.
The remaining concerns are missed/doubled pulses and sunlight interference.
Wheel diameter is a fixed calibration constant; derailment requires reset.
Engine braking is not treated as grounds to reject the unpowered wheel.

- Hall supplies landmark identity and absolute route reference.
- IR supplies movement and distance between landmarks.
- Motor feedback is optional supporting evidence.
- IR uncertainty may lower confidence or veto navigation advance. IR must
  never independently assign magnet identity or route position.

## Activities

1. Reviewed IR documentation and sketches; prepared active TX 1.3, RX 1.2,
   and `IR_ACTIVE_TEST_CAR_RUNBOOK.md`. Earlier checks reported successful
   builds of both sketches and passing NAVI_FRESH tests.
2. Helped identify the USB receiver. User chose Arduino IDE because 921600
   upload speed fails on the Mac. Upload speed and RX recording baud differ.
3. Investigated missing movement output. User verified sensor supply and
   measured approximately 2.5 V between GPIO34 and ground. Later captures
   showed transitions and movement counts.
4. Helped establish continuous Pi recording. Unit initially existed only on
   the Mac; user copied, installed, and enabled it. First start failed with
   exit status 2. After copying the updated recorder, user supplied output
   showing active service, growing raw log, and updating health JSON.
5. Examined stationary shade/sun, hand-roll, and Toby tow captures. Copied
   analysis inputs to `/private/tmp/ir_espnow_raw_20260919.log` and
   `/private/tmp/all_20260919.log`.
6. Added `ir_hall_expost_align.py`, `ir_hall_interval_diagnostics.py`, and
   `ir_envelope_replay.py` for count alignment, interval diagnostics, and
   candidate envelope comparison.
7. Prepared TX 1.4: 512-sample envelope, 50 ms refresh, 128-sample prime,
   replacing 2048 / 250 / 512. Sampling remains 1 kHz. Debounce, Schmitt
   fractions, contrast gate, and latch timeout were retained. Startup banner
   now prints the settings. Updated runbook and dated analysis report.
8. Added two synthetic replay tests in `test_ir_envelope_replay.py`.
9. Added first-STRUCK cutoff to the alignment CLI, with an explicit
   `--include-after-strike` diagnostic override.
10. Advised that the test car could be switched off after capture, leaving
    Pi/RX running. Physical power-off was not confirmed.

## Recording and installed firmware

- Pi: `david@192.168.68.142`.
- Service: `ngr-ir-espnow-record.service`, enabled and active at last supplied
  confirmation. No fresh service check is implied by this record.
- Recorder: `/home/david/NGR/ir_scope_serial_record.py`.
- Device: `/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0`.
- RX recording rate: 921600 baud.
- Raw source: `/home/david/NGR/ir_espnow/ir_espnow_raw_20260919.log`.
- Health: `/home/david/NGR/ir_espnow/ir_espnow_health.json`.
- MQTT source: `/home/david/NGR/telemetry/all_20260919.log`.

Earlier session inspection identified installed test-car firmware as
`IR_SCOPE_ESPNOW_FUSION_TX_1_2`, an older external-tree build. Prepared source
versions do not establish what is flashed. TX 1.4 has not been flashed here.

## Field chronology

- 16:52: battery operation, stationary kitchen table in shade. Recording was
  not yet confirmed; this is not a validated capture start.
- Around 17:02: recorder confirmed active. Shade replay: 17:02:00-17:04:00.
- Stationary sun replay: 17:06:18-17:08:18.
- 17:10: user clarified five-minute shade dwell began. Clean replay subset:
  17:11:00-17:15:00.
- After 17:15: shade slow/medium/fast sequence, then sun at four orientations.
  Exact phase boundaries and independently counted wheel turns were not supplied.
- 17:27: user reported Toby CW from MM 040-041 at PWM 60, sluggish, later
  about 23 pKm/h. Technical difficulty near MM 65 was addressed.
- IR analysis identified motion 17:34:17-17:43:20 (about 8604 pulses), and
  after restart 17:44:31-17:47:13 (about 2322 pulses).
- User reported program stop at Patio, restart, and passing the starting point.
- 17:43:10.442 MQTT receipt: final accepted advance to MM 10.
- 17:43:12.572 MQTT receipt: expected MM 11, `WRONG_MAGNET`,
  `POLARITY_MISMATCH`, NAVI `STRUCK`.

User considers pre-stop Hall advances probably accurate because Toby uses a
NAVI-ONE one-strike variant. They remain provisional references. Restarted-run
identities were excluded. Hall event times were mapped using device `close_ms`
and uptime-derived clock offset, rather than delayed MQTT receipts.

## Findings

Stationary shade/sun showed zero false pulse growth; inspected sensor counters
showed no sample misses, queue drops, or send errors. Earlier analysis reported
raw radio loss around 3.4% in shade and 16.8% in sun, sun RSSI around -89 dBm.
Radio loss and optical pulse loss are different quantities.

Only raw type-1 packets were available, without type-2/type-3 CTO observations.
Ex-post comparison therefore used separate IR and MQTT streams. The current
Toby configuration did not supply the direct CTO context expected by the test car.

Before the strike: 287 accepted Hall events, 244 aligned count packets, 213
adjacent-marker intervals, 62 repeated segments. Median nearest-packet offset
was 32.7 ms, maximum 237.8 ms. These are matching distances, not absolute clock
accuracy bounds.

Healthy repeated sections: MM 120-127 counted 199 pulses twice; MM 111-117
counted 185 and 186. MM 127-143 counted 316 and 393, including near-zero
intervals. Affected raw waveforms remained on one side of stale thresholds
despite optical contrast. This supports envelope tracking lag during changing
illumination. Actual light boundaries were not independently timestamped.

Five envelope candidates were replayed. Selected 512 / 50 / 128 changed bad
window counts from 227+293 to 329+349, about 30% more overall (45% and 19%
separately). Stationary replay stayed zero. Healthy MM 120-127 replay counts
were 144 versus 144 and 161 versus 160. Shorter windows lost healthy counts.
An earlier commentary claim of 45% across both windows was overstated; that
percentage applies only to the first window. Full table is in the run analysis.

## Verification and limitations

- TX 1.4 compiled using ESP32 core 3.3.11, target `esp32:esp32:esp32`:
  898624 bytes flash (68%), 65176 bytes global RAM (19%). Initial sandbox
  cache access failed; authorized retry succeeded.
- Two synthetic Python replay tests passed: stationary noise and modulation
  during a common-mode shift. These do not execute the firmware detector.
- Python syntax checks passed for recorder and analysis scripts.
- NAVI_FRESH passed: zero core failures, 192 labelled primaries accepted,
  3 non-magnets rejected, 150 rebounds suppressed.
- Replay was repeated against stationary, healthy, and affected captures.
- Full-range alignment correctly excluded post-strike events automatically.

TX 1.4 remains a candidate, not a proven cure. More detected transitions alone
do not establish accurate distance or missed-pulse rate. Radio gaps omit raw
samples; independent pulse truth was unavailable. Replay is approximate: it
re-arms across gaps, uses received sample history, and differs from firmware
in priming and percentile details. Crawl behavior, stationary lighting changes,
and isolated-error handling still require work. No navigation authority or
automatic pulse repair was added.

Documentation review found an outstanding compatibility defect:
`parse_hall()` now returns three values, but `ir_hall_interval_diagnostics.py`
still unpacks two. Update that caller and enforce the intended trust cutoff
before its next use. Its earlier results preceded the API change.

Analysis depends on external parser
`/Users/davidbrown/esp-loco-control/tools/ir_scope_espnow_analyze.py`.
Local log copies are temporary; Pi paths above are source locations. This
documentation task did not make an additional archive or verify live hardware.
The project folder is not a Git checkout: `git status` returned that error.
Files are saved locally; no commit or push was made.

## Next actions

1. Flash `IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino` through Arduino IDE at a
   working upload speed. At 115200 monitor baud verify
   `IR_SCOPE_ESPNOW_ACTIVE_TX_1_4` and `env=512 update=50 prime=128`.
2. Confirm fresh Pi recording for the new session. RX needs no update for
   the TX parameter change.
3. Test stationary shade two minutes, then stationary sun two minutes.
4. Hand-roll slow/medium/fast in shade, then direct sun. Include crawl and
   stationary changes in illumination. Record phase times and measured travel
   or independently counted turns.
5. Only after clean bench/sweep checks, tow repeated steady-speed circuits
   through the affected region. Exclude Hall identities after a strike.
6. Repair the diagnostic caller, quantify timing uncertainty, and compare
   pulses with independent movement truth to establish error rates.
7. Develop conservative interval/phase plausibility and isolated-error handling,
   keeping inferred corrections separate from raw measured counts.

## Related documents

## Post-flash monitor inspection

After the user reported reflashing and opening Serial Monitor, direct Arduino
IDE inspection showed the open sketch was the external-tree copy:
`/Users/davidbrown/esp-loco-control/firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`.
Its visible header identified `IR_SCOPE_ESPNOW_FUSION_TX_1_2`, not active TX 1.4.
The monitor was on `/dev/cu.wchusbserial110` at 115200 baud. Visible cumulative
pulse counts rose 915 -> 1069 -> 1212 at approximately five-second intervals;
missed, queue-drop, send-error, and saturation counters were zero. Physical
wheel motion was not confirmed, so these counts alone do not establish correct
movement sensing. No TX 1.4 startup banner was visible. User must flash the
NGR-Files sketch and verify its banner before treating this as TX 1.4 testing.

## Links

## Authoritative path correction

Following the user's clarification, TX 1.4 was copied into the established
repository sketch path:
`/Users/davidbrown/esp-loco-control/firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`.
The destination was compared before replacement; differences were the active
version banner, envelope settings, diagnostic banner fields, and CTO target
selection (old copy targeted Otto 9950011; TX 1.4 targets Toby 9950012 for this
test). The established repository path is authoritative for future edits.
The user should reopen their usual sketch and upload. No hardware flash was
performed by this file correction. Unrelated repository changes were preserved.

## Document links

- [Run analysis](IR_TOBY_RUN_ANALYSIS_2026-09-19.md)
- [Active runbook](IR_ACTIVE_TEST_CAR_RUNBOOK.md)
- [Project index](PROJECT_NOTES.md)
