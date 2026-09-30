# 2026-09-29 — Otto EWO Wi-Fi diagnosis, corrected build, and supervised flash

## Scope and outcome

Otto (9950011) was stale in the dashboard after the integrated EWO candidate
was flashed. The operator confirmed that the repository-root private
`credentials.h` contained the correct SSID/password and that Otto was near the
access point. The work below diagnosed startup resource pressure, produced the
reviewed fix on `codex/ewo-review-corrections` at `b7bdfb5`, compiled it, and
flashed Otto through `/dev/cu.wchusbserial10` with the operator's permission.
Otto associated, connected to MQTT, and the operator confirmed the dashboard
was online and both session-orientation and interval-location controls worked.
The motor was not commanded to move during this validation. This is **not field
acceptance** of navigation, stations, AUTO, or IR-assisted running.

## Evidence and single-variable checks

1. The failing EWO serial output repeatedly reported
   `wifi=DISCONNECTED wifi_status=6 mqtt=DISCONNECTED mqtt_state=-1` and
   `wifi_disconnect_reason=2` (authentication exchange expiry). A scan found
   the intended AP on channel 11, BSSID `DC:62:79:D9:E5:9C`, RSSI about -38
   dBm. These observations did not establish a bad password or antenna.
2. Deferring ESP-NOW initialization until after Wi-Fi association did not
   remove reason 2. Removing `WiFi.setSleep(false)` also did not remove it.
   Those two changes alone were insufficient in the tested builds.
3. A motor-off, Wi-Fi-only probe (`codex/otto-wifi-minimal-probe`, `b35103a`)
   associated promptly using the same board, credentials, and AP, obtained
   `192.168.68.71`, and remained connected for a one-minute observation at
   about -33 dBm. This established that Otto and the AP could associate under
   a smaller sketch; it did not identify the failing allocation by itself.
4. Starting Wi-Fi before EWO's recorder/queues allowed association but then
   produced `[BOOT] FATAL: queue allocation failed`. The original 48-entry
   publish queue stores 1,273-byte messages and reserves about 61 KB. Reducing
   it to 16 entries (about 20 KB, roughly 40 KB saved) while leaving the
   recorder and other queues in place allowed Wi-Fi, MQTT, and ESP-NOW to start.
   The diagnostic branch `codex/otto-wifi-auth-diag` at `182a202` preserves
   that test build. The ordering/size crossover is strong evidence that
   startup memory pressure, rather than the credentials or ESP-NOW startup
   alone, caused the EWO association failure. It does not prove the ESP32
   driver's precise internal allocation failure mode.

## Reviewed implementation and verification

Commit `b7bdfb5` on `codex/ewo-review-corrections` retains the proven
16-entry publish queue, reports queue allocation and free/largest-block heap
at boot, reports Wi-Fi disconnect reasons, and starts/retries ESP-NOW from the
network task after Wi-Fi is connected. It keeps the motor PWM output LOW during
early setup. The boot banner remains labeled integration candidate, not field
accepted. No credential values were committed or printed in this record.

`git diff --check` passed. The integrated host runner
`sh firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/run_tests.sh` passed. The
`esp32:esp32:esp32` compile passed with the private credentials header supplied
via include path; it reported about 1,011,935 bytes flash (77%) and 57,516
bytes global RAM (17%). These compile figures exclude runtime queue, recorder,
task-stack and network allocations. The reviewed binary was built in
`/private/tmp/otto-ewo-build-reviewed-fix`, uploaded to Otto, and the upload
hash was verified. The development branch was pushed to the repository; the
diagnostic probe branches were also pushed separately. Nothing was merged into
another branch and Toby was not flashed.

After reset, serial reported all four queues allocated, free heap 154,524
bytes and largest allocatable block 73,716 bytes. Wi-Fi connected as
`192.168.68.71` at roughly -32 to -36 dBm; ESP-NOW reported `radio=READY`;
MQTT connected. Broker state reported `wifi=1`, `mqtt=1`, `ir_radio=1` and
`online=1`, with `state/loopstat` published. The operator then reported:
“Online; both controls work,” referring to session orientation and interval
location. These are at-rest observations, not a running-train test.

## Open observations, not hidden by the connectivity result

- The reviewed build showed `pub_drop=5` after startup. Three samples at
  roughly 64–66 seconds showed no further increase. An earlier diagnostic
  build had shown `pub_drop=0` at about 88 seconds; the reviewed result takes
  precedence for this flashed binary. Reduced queue depth needs load and
  reconnect testing.
- Telemetry observed NSR UDP failure counts rising from 46 to 50, native
  recorder drops at 211, and NSR Hall drops at 18. The receiver/capture path
  was not diagnosed in this session. These counters must not be treated as
  proof of lossless evidence capture.
- The IR radio initialized but `ir_seen=0` and `ir_coupled=0`; no received
  frames or useful IR measurements were demonstrated. The historical
  `IR_ESPNOW_SENDER` config contains a stale unicast Otto MAC, but the
  [2026-09-10 car status addendum](20260910_IR_CAR_STATUS_DETERMINATION.md)
  identified the car then running `IR_SCOPE_ESPNOW_TX`, a broadcast/raw packet
  sketch. Its current flashed firmware and power state were not established
  here. Do not attribute today's zero frames to that stale MAC or claim the
  car is feeding EWO without a live receive trace.
- No motor movement, navigation judgment on track, station operation, AUTO,
  sustained data capture, or reconnect recovery was tested. `b7bdfb5` is a
  connectivity/control recovery on Otto, not a field-accepted train build.

## Recordkeeping convention for follow-up work

This record and the accompanying README/source-comment corrections are
documentation only. They do not change the executable EWO build or require a
new flash: Otto remains on the binary built from `b7bdfb5`.

For each further change, record the source revision, exact hardware target,
commands/tests run, observed serial and broker evidence, operator observations,
and unresolved limits. Keep private credentials out of committed records.

## Addendum — first operator test-run watch, 2026-09-29 ~20:39–20:41 PDT

The operator announced a test run. A read-only subscription to Otto's MQTT
topics (`ngr/loco/9950011/state/loopstat`, `state/connectivity`, `online`)
received live telemetry; no commands were published and no hardware or firmware
was changed. At 20:39:04 PDT, connectivity reported Wi-Fi/MQTT/IR radio all
ready (`1`), RSSI -51 dBm, and 3,133 total radio frames seen. The corresponding
loop status reported MM 122, target 123, direction +1, PWM 0, `running=0`,
`confirmed=2`, `missed=1`, 1,574 accepted IR observations, 1,644 invalid IR
packets, `ir_applicable=0`, `degraded=1`, `estop=0`, `lowvolt=0`, and
`pub_drop=0`. The confirmed/missed counters are cumulative; this watch cannot
assign those events to a specific portion of the operator's run.

Through 20:39:12 PDT, accepted IR observations rose to 1,650 and invalid
packets to 1,723, while the sampled state stayed at PWM 0, MM 122, target 123,
`running=0`, `ir_applicable=0`, and `degraded=1`. Thus the radio was receiving
both valid type-5 observations and packets rejected by EWO's wire validator;
their senders and the reason for rejection were not identified. The last
connectivity sample received at 20:39:10 PDT still showed Wi-Fi/MQTT ready,
RSSI -40 dBm, and nine cumulative NSR UDP failures.

Fresh telemetry then stopped. A further 12-second fresh-message check and a
20-second online watch received no updates. The broker's retained
`ngr/loco/9950011/online` value was `0`; its retained connectivity JSON still
said connected, but was stale. Otto's former `/dev/cu.wchusbserial10` device
was also absent from the Mac. This evidence establishes loss of observable
connectivity, **not its cause**: intentional unplugging/power-off, battery or
Wi-Fi trouble, reboot, and other possibilities remain unresolved pending the
operator's report or a new serial/live telemetry trace. No movement command
was sent by the observer.

## Addendum — operator-reported direction reversal

After the first test-run watch, the operator reported that Otto was set to
forward but physically ran backward, and said they intended to switch the
motor leads. This physical observation is the operator's report; it was not
independently witnessed through MQTT. The broker's retained state at the time
read `state/direction=2` (forward), `state/session_direction=CW`, and
`state/throttle=0`; because Otto was offline, these are last-published values,
not a live electrical measurement. The EWO source uses Otto's X22 profile
(`MOTOR_DIR_PIN=16`) and drives that pin HIGH for `motorDirection=1`, matching
the X22 sketch's forward mapping. No software inversion was found in those
specific paths. This does not establish the actual polarity of Otto's motor
leads or explain why physical travel was reversed. The observer neither
changed wiring nor sent a direction/throttle command. A powered-off wiring
change, if made by the operator, needs a separate low-speed physical direction
check before relying on NAVI's route direction or continuing an assisted run.

## Addendum — later moving run and dashboard speed, ~21:43–21:45 PDT

At the operator's request, read-only MQTT and dashboard-state checks were made
while Otto was online; no commands, wiring, firmware, or server files were
changed. Live telemetry at 21:43:53 showed Wi-Fi/MQTT/IR radio ready,
`ir_coupled=1`, CCW session and NAVI direction -1. The dashboard state showed
MM 147 then 146, with reported throttle 55. During the 21:44:09–17 sample,
PWM was 40, MM advanced 142 to 140, `position_reliable=1`, `degraded=0`,
`ir_applicable=1`, `ir_health_fault=0`, and cumulative confirmations rose 250
to 252 while cumulative missed markers remained zero. This is encouraging
short-window navigation telemetry, not independent physical position proof or
field acceptance. `running=0` in EWO loopstat means AUTO is not running; it
does **not** mean the manual motor is stopped when PWM is nonzero.

The operator reported that the main pKPH tile did not show a useful speed.
The live dashboard `/loco/9950011/state` returned `pkph="0.0"`, but its
`ages.pkph` was about 21,871 seconds (roughly six hours) while the MM, PWM,
IR-link, and speed-view ages were about one second. Thus the main pKPH value
was stale, not a current zero-speed measurement. The current dashboard source
refreshes this field from `alert.est_mm_s` or a `mm/speed` object with
`source="SEGMENT_MEASURED"`; EWO emits neither. EWO's `telem/speed` is an IR
JSON object and is stored as `speed_view`, not interpreted as the main pKPH
input. This is a confirmed producer/consumer contract mismatch. It should be
fixed with an explicitly defined main-speed source, not by silently relabeling
IR speed as Hall/segment speed.

The operator also reported IR pKPH alternating around 18.0 and 35.9. EWO
calculates `ir_mmps` from the pulse difference between **consecutive** valid
IR frames using their sender capture timestamps. The observed IR sequence
advanced about ten frames per second. With the transmitted nominal wheel pitch
of 9.652 mm, one pulse per roughly 100-ms frame interval gives about 96.52
mm/s, or 18.0 house pKPH (`mm/s / 5.37325`); two pulses give about 193.04
mm/s, or 35.9 pKPH. Those two display numbers are therefore the expected
quantized outcomes of a one-frame window, not evidence that Otto's physical
speed doubles every second. The live 21:44:09–17 sample actually showed
`ir_mmps` toggling between 0 and 96.52 while the cumulative pulse count rose
8001 to 8037. That 36-pulse change over approximately eight seconds implies
roughly 43.4 mm/s, or 8.1 house pKPH, averaged over that window; it is only
an approximate window average, not a calibrated speed verdict. In particular,
EWO marked some zero-pulse single-frame results `ir_valid=1` and `MEASURED`
while PWM was 40 and the pulse total was advancing across seconds. The raw
short-window value is unsuitable as a smooth dashboard speed display.

Other boot-cumulative counters at 21:44 included Hall input queue drops 7,
native recorder drops 182, invalid IR packets about 37,300, and 324 NSR UDP
failures in the preceding connectivity snapshot. Hall/recorder drop counters
did not rise during the eight-second sample, but the capture path is not
lossless. The invalid packets' source was not established. A follow-up speed
design could aggregate multiple sender-timestamped pulses over a longer,
clearly labeled display window while leaving NAVI's raw IR observation path
unchanged. No speed or navigation algorithm was altered in this review.

### Correction to the speed interpretation after operator challenge

The operator correctly objected that describing 18.0/35.9 (and 18.0/0) as
"expected quantization" understated a **new EWO display regression**. The
earlier NAVI_COHERENCE IR telemetry calls `IrSpeedTelemetry::sample()` once per
second. It subtracts cumulative pulses and sender capture times across that
roughly one-second interval, reports `ir_window_us` and `ir_delta_pulses`, and
uses `NO_PULSES` rather than `MEASURED` for a zero-pulse interval. Its
qualification can mark zero invalid when Hall has advanced and can flag a
prolonged powered zero. A committed 2026-09-26 Toby run log contains actual
one-second windows (for example 6 pulses / 1,000,000 us = 57.912 mm/s =
10.778 house pKPH), not adjacent 100-ms snapshots.

EWO's `NaviIntegratedCore::observeIr()` replaces that operator-facing speed
path with the difference between each pair of consecutive type-5 frames,
roughly 100 ms apart, and `NaviCompatibility::formatConsoleIr()` labels every
available result `MEASURED`, including zero pulses while PWM is nonzero.
EWO also omits the earlier display qualification and the window/pulse-count
fields. This is the causal software difference; the one-/two-pulse arithmetic
explains the numbers but **does not make them valid speed readings**. The
short sample at PWM 40 gave an IR cumulative-pulse average near 8.1 house
pKPH over eight seconds. A 300-mm mapped Hall interval advanced in about
seven seconds, roughly 8.0 house pKPH if the landmark timing is representative.
Those two *rough averages* are mutually plausible and far below the displayed
18.0/35.9 spikes. The source does not establish absolute speed calibration,
but it strongly points to the EWO display calculation, not newly erratic wheel
motion, as the immediate defect.

The appropriate correction is a separate, display-only speed estimator with
the prior continuous-epoch, sender-timestamped approximately one-second
window and explicit validity/reason fields; NAVI should continue receiving
every raw IR observation unchanged. The missing main pKPH contract is a
separate producer/consumer issue and should not be "fixed" by silently
putting IR speed into the Hall/segment tile. This addendum is diagnostic only:
no firmware, dashboard, or hardware change was made or tested here.

### Navigation-consumer audit and attribution

The operator reports that IR speed had been reliable and in line with Hall
speed on prior programs, and correctly identifies EWO/NAVI as the new problem.
The prior one-second telemetry implementation and the current pulse/Hall
averages support that attribution; this session found no evidence that the
IR car itself began producing the displayed 18.0/35.9/0 jumps. The incorrect
rate calculation is in EWO's `NaviIntegratedCore`, and the incorrect
`MEASURED` presentation is in EWO's compatibility output.

Those displayed rates cannot be used as navigation speed. A source-reference
check found `speedMmS_` and `speedAvailable_` consumed only by
`NaviCompatibility::formatConsoleIr()` for `telem/ir` and `telem/speed`.
NAVI's target coherence, missed-magnet, and spatial-reference paths instead
use the type-5 packet's cumulative `nominalUm` (completed pulses multiplied
by pitch), together with Hall evidence. Thus the faulty adjacent-frame
**display-rate values are not directly fed into NAVI's decisions**. This is
an architectural boundary, not a defense of defective EWO or proof that its
navigation is sound: cumulative distance is still nominal/unvalidated, and
the short Hall/IR agreement does not certify every marker. EWO remains an
integration candidate, not field accepted. Correlated Hall/IR replay across
the full run is needed before judging NAVI's navigation accuracy. No motion
command or firmware change was made during this audit.

## Addendum — provisional NAVI navigation rating from full run snapshot

At the operator's request, a **read-only** snapshot of the Raspberry Pi MQTT
run log was analyzed. Source:
`/home/david/NGR/telemetry/runs/9950011_20260929_204400.log` on
`192.168.68.142`; local temporary snapshot:
`/private/tmp/otto-ewo-run-20260929-204400-final.log` (11,070,940 bytes,
SHA-256 `a8e8914147ddcbd6e6be895265590eee638d1543868316944e3b22f39f94f023`).
The snapshot spans 20:44:00–21:59:18 PDT, including the final `online=0`.
JSON events and one-second loop status were counted; the
committed 171-marker route map was used to audit next-marker order, polarity,
and leading-boundary nominal IR distances. This did not alter the Pi, Otto,
or any command topic.

The operator clarified that they did **not** follow Otto along the whole
route, so their earlier MM/direction agreement was approximate. The
independent physical check is the endpoint: after a slow section, a stop, and
backing up approximately 30 MM, Otto finished between MM049 and MM050.
The log records `REVERSED` at MM16 at 21:56:46, then confirms MM16 in the
opposite direction at 21:57:01 and progresses to MM49 at 21:58:41. At the
final stop, NAVI reported `mm=49`, `target=50`, `dir=1`, `pwm=0`, and
`position_reliable=1`, matching the physical interval. The final NAVI state
also reported `degraded=1`, so the endpoint match is not a claim that every
sensor remained healthy. Within the completed log:

- 410 target confirmations, zero `MISSED_MAGNET` events, one commanded
  reversal, zero out-of-sequence confirmed marker transitions, and zero
  confirmed-marker polarity mismatches against the map.
- All 1,526 one-second samples with PWM > 0 reported
  `position_reliable=1`. Nine confirmations were marked degraded, with nine
  `POSITION_REANCHORED` events. About 13.4% of powered samples reported
  `degraded=1`; `ir_applicable=1` in about 87.8% of powered samples. Nine
  `IR_DEGRADED` events, nine `IR_NORMAL` events, four spatial invalidations,
  and 406 spatial-reference completions were recorded. Some degraded stretches
  lasted roughly 40–70 seconds; continued correct physical MM reporting is
  encouraging evidence for the fallback path, not proof it is universally safe.
- The median absolute difference between consecutive confirmed leading-boundary
  nominal IR distance and mapped spacing was about 3.0%; the 95th percentile
  was about 9.9%. Twice at MM 20→19, on different passes, the nominal IR
  difference was about 270.3 mm against the map's 320 mm (15.5% short).
  This repeated near-boundary discrepancy merits separate review; it is not
  evidence by itself of a wrong MM. Normal NAVI confirmations are already
  IR-gated, so these distance statistics are not an independent accuracy proof.
- Hall input queue drops were 7 at the first loop-status sample and still 7
  at the last; IR input queue drops, NAVI event drops, PWM-zero IR motion
  events, and MQTT publish drops were zero in this run. Native NSR
  recorder drops rose 43→252 and NSR Hall drops 0→2, so a lossless full replay
  is not claimed. Rejected ESP-NOW packet counts were not attributed to the
  IR car without source identification.

**Provisional rating: 8/10 for observed manual navigation behavior.** The
successful endpoint match after reversal is meaningful independent evidence,
while 410 internally consistent confirmations are not 410 separate physical
checks. Points are withheld for the
degraded/re-anchor stretches, repeated MM 20→19 distance discrepancy,
recorder losses, and absence of supervised AUTO/station validation. This is
not a rating of EWO's defective speed telemetry, nor field acceptance of the
firmware. Intermediate marker assignments could not be independently verified
by this run's endpoint observation alone.
