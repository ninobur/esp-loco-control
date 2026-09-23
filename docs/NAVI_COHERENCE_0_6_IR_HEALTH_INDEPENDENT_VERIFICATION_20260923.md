# NAVI_COHERENCE 0.6 IR_HEALTH — independent verification before field test

2026-09-23, Claude. David asked for a review of the field-handoff message for
`584f724` before flashing Toby. Re-derived the report's claims from the
committed code rather than trusting the report text.

## Checked

- All eight baseline headers (`Navigator.h`, `Ops.h`, `Stations.h`, `RouteMap.h`,
  `MovementEvidence.h`, `HallObserver.h`, `RecoveryControl.h`, `LocoConfig.h`)
  diffed byte-identical between 0.5 AUTO_ENABLED and 0.6 IR_HEALTH.
- The two `.ino` entry points diffed: the only changes are the `IrHealthMonitor`
  include/instance, banner/version strings, the new `diag/ir_health` topic, and
  `irHealth.*` calls at existing call sites. Data flow is one-directional —
  the monitor reads `navigator.status()` / `positionKnown()` as input and
  nothing reads the monitor back into navigation, station, or motor code.
  `grep` for `digitalWrite|ledc|analogWrite|pwm|motor|pinMode` across
  `IrHealthMonitor.h` and the three `IR_ARCHITECTURE_0_4` headers it pulls in
  returned only the file's own "never returns evidence" comment.
- Rebuilt `test_ir_health_monitor.cpp`, `test_coherence.cpp` and
  `test_station_correction.cpp` with
  `-std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined
  -fno-omit-frame-pointer`: all PASS, same counts as the report (2,052 offset
  corrections, 6,840 single-misread holds, 32 station approaches). Re-ran
  `test_health_json.py` against a fresh binary: 11 payloads, max 546 bytes.
- Recompiled for `esp32:esp32:esp32` (installed core 3.3.11) with
  `--warnings all`: 1,012,715 bytes flash (77%), 72,204 bytes static RAM
  (22%), only the three pre-existing `Adafruit_INA219` enum warnings — no
  sketch warnings. Exact match to the report.
- Confirmed the Arduino-sketchbook copy at
  `~/NGR/NGR-Files/NAVI_COHERENCE_0_6_IR_HEALTH/` is identical to the repo
  copy (`diff -rq`, no output), and that `584f724` is present on
  `origin/agent/toby-1-13-flash`.

## Result

No discrepancy found. The message's "observation-only," "no IR TX/RX reflash
needed," 115200/115200, and 30-60s-dwell-then-manual-lap claims all match the
sketch README and the code as committed.

## Not covered

This is code inspection, not field evidence — it cannot confirm the TX
stationary-contrast behavior or anything about how 0.6 performs on the
railway. The handoff message summarizing this for Sam dropped the sketch
README's explicit "Stop if locomotive behavior is unexpected"; worth
restating to whoever is at the controls.
