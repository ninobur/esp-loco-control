# 0097 - Zero calibration ID is not an optical fault

Status: Accepted (2026-09-23; David authorized the proposed correction)

## Decision

Accept the deployed IR TX's calibrationId=0 as nominal, unvalidated scale
metadata, not malformed transport or failed optical health. Continue reporting
the actual detector reason. Publish calibration_id and distance_bounds separately
in shadow health telemetry; all distances remain UNVALIDATED.

## Context

The first stationary 0.6 field check rejected every packet because the new
monitor required a nonzero calibration ID. The existing TX intentionally sends
zero. The Pi correctly displayed the loco's rejection. Nonzero synthetic test
fixtures missed this compatibility error.

## Alternatives

Inventing a nonzero ID on TX would hide the mismatch. Changing the Pi decoder
would not correct a firmware-side rejection. Suppressing inadequate contrast
would conceal a separate detector report. None is used.

## Consequences

Zero/nonzero calibration transitions still end continuity. Epoch/MM ownership
rules, legacy navigation, motor logic, TX and RX remain unchanged. A READY
instrument can report nominal distance without certifying its accuracy. A
captured 110-byte field packet is now a regression fixture with its original CRC.

## Verification

CAL0_FIX1, same 0.6 sketch path. ESP32 core 3.3.11 compile passed: 1,012,763
bytes flash; 72,204 static RAM. Three existing INA219 enum warnings only.
ASan/UBSan architecture and monitor suites passed. JSON: 13 payloads, maximum
636 bytes of 960. Navigation: 2,052 corrections and 6,840 holds passed.
Stations: 32 approaches passed. Not field accepted; no upload performed.

The repository move also required three include-path repairs and repair of
the existing NGR-Files Arduino shortcut. No navigation logic was changed.

## Next Field Check

Flash Toby only at 115200 baud, then Serial Monitor at 115200. Keep Hall clear
of magnets during its two-second baseline. Hold stationary 30-60 seconds.
Verify health_revision=CAL0_FIX1, accepted increasing and actual detector health
visible. An INADEQUATE_CONTRAST report is not a packet failure and must not be
relabeled healthy. Review this hold before authorizing the next moving test.

See docs/NAVI_COHERENCE_0_6_STARTUP_REJECTION_20260923.md for original evidence.
