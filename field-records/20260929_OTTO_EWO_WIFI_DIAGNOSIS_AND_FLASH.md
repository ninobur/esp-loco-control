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
