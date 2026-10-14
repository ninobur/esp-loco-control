# EWO 0 5 manual STOP/GO and Otto deployment — October 14, 2026

David settled the remaining operating rule: **manual STOP pauses both timing
clocks; GO resumes them while preserving geographic identity.** A stop alone
does not require redeclaration. Existing safeguards still require declaration
when movement is detected while stopped. David separately authorized uploading
the passing build to USB-connected Otto.

## Implementation and tests

`NaviRunClock.h` removes manual-stop duration without rewriting raw Hall or
command timestamps. A bounded history associates queued observations with the
correct clock coordinate. Selected and pending observations retain their clock
association. Arrival comparison and throttle interpolation use the same paused
clock. The next lap's timing excludes manual-stop duration; scheduled station
braking/dwell/departure remains in the original executor and still counts.

The core no longer invalidates geographic identity merely on manual STOP.
The sketch synchronizes the clock with STOP/GO and preserves the stored tile
ramp instead of restarting it. Diagnostics explicitly identify the running
clock, each opening's removed pause duration, and the current paused state.
No detector, map, calibration or motor-protection parameter changed.

- Eight active host executables pass, including **223** credibility, **85** DART
  and **410** integrated assertions.
- Actual motor/overlay functions pass **76** assertions, including a 60-second
  pause, another pause, unchanged tile/identity, resumed PWM at the same point,
  and unchanged final braking, dwell, departure and protection behavior.
- Actual queue/MQTT functions pass **712** assertions under transport faults.
- All six paired baseline/proposed replays pass their existing checks; selected
  measured coordinates equal saved anchors.
- The original six failing legacy/first-increment suites remain recorded in the
  [earlier review](REVIEW.md). They are not counted as passing regressions.

Tests cover pending-candidate preservation, no MM+2 timeout during manual STOP,
IR becoming unavailable during a pause, raw timestamp preservation, timing
history excluding paused time, multiple pauses, and the existing movement-at-
PWM-zero declaration latch. The widest numeric diagnostic is **1,911 bytes**
inside the 2,048-byte buffer. Host NAVI object is **29,784 bytes** and diagnostic
record is **336 bytes**. Actual hardware timing remains to be measured.

## Build and recorder gate

Deployment compilation passes ESP32 **3.3.12**, `esp32:esp32:esp32`, with format
warnings treated as errors. Program **1,055,591 bytes**, static RAM **81,404 bytes**;
246,276 bytes remain before runtime allocations. Otto's existing private
credentials were used. The [sanitized build log](stop-go-esp32-deployment.log)
retains inherited INA219 warnings and a private credential/profile token-alias
redefinition warning; no credential values are published.

[Build provenance](stop-go-build-provenance.json) records source and binary
hashes. [Final-format MQTT proof](stop-go-mqtt-result.json) confirms that both
labelled, nonretained production-formatted withdrawal/restoration records were
read back from the Pi's actual MQTT log before upload. These host records are
explicitly distinguished from locomotive evidence.

## USB and rollback

The device on `/dev/cu.wchusbserial10` identifies as Otto's previously recorded
MAC **68:09:47:ac:4e:b4**. A complete **4,194,304-byte** installed-flash backup was
read successfully at 115200 baud into private temporary storage. Its SHA-256
is `16049713b534acfd978df0592e4af4625da1bd0484178ec39776fdcda418e2b8`.
The faster read failed and is labelled unverified; it will not be used as a
rollback image. Private flash images are not committed because they may contain
network configuration. Original EWO, COHERENCE R2 and the 31-file first-increment
rollback remain unchanged.

## Upload and stationary verification

At this source-review checkpoint, the passing deployment build is prepared for
the authorized upload. Upload result, exact firmware commit, new boot identity,
MQTT receipt and stationary resource checks will be recorded below after the
actual operation. No motion commands have been issued.

Field acceptance still requires David's railway test. At boot there is no
previous complete lap: normal IR must first supply that lap before timing
fallback has a reference. The Pi MQTT recorder is active; the separate native
ADC recorder service was observed inactive and has not been changed.
