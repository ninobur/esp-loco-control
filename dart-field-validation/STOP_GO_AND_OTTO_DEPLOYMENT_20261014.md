# EWO 0 5 manual STOP/GO and Otto deployment — October 14, 2026

**Later follow-up:** the [startup memory repair](startup-repair/REPORT.md) is
now installed on Otto. The report below preserves the initial failure and
rollback; its statement of installed firmware is historical.

**Outcome: EWO 0_5 passed compilation and the active host checks but failed the
stationary hardware startup gate. It is not cleared for operation. Otto has
been restored to its exact prior installed COHERENCE R2 image; the restored
boot and zero throttle were read back from the Pi recorder.**

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

The authorized image was built from commit
`83889d9921e7231fd41e082ce5579165481d7a89`. Application SHA-256:
`f4470e670e3503c4af2a2f5b2758e7de2a7ec862eb8801d11fe56c7fc3c8a7a4`.
The [upload log](otto-0_5-upload.log) confirms successful writes and hash
verification. No motion commands were issued.

The [stationary serial capture](otto-0_5-serial-stationary.log) then showed
Wi-Fi association, MQTT connection timeout (`state=-4`), and an abort/reboot.
Its embedded ELF identifier matches the compiled ELF. No fresh EWO 0_5 boot
record reached MQTT; old retained R2 messages are not evidence of an EWO boot.
The EWO loop-stat capture is empty. Consequently actual EWO heap margin, Hall
sampling timing, queue behavior and live diagnostic throughput are **unverified**.

The [decoded backtrace](otto-0_5-crash-decoded.txt) enters
`lock_init_generic` from UART/console logging in the Wi-Fi timer path. Runtime
lock allocation/resource pressure is the leading explanation, not a confirmed
allocation budget or identified offending buffer. The upstream ESP-IDF
[lock implementation](https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/newlib/src/locks.c)
aborts when mutex creation fails; its source line numbers differ from the
packaged ELF, so this supports the interpretation rather than proving the exact
packaged implementation. No buffer reduction, navigation change or speculative
startup workaround was made after the failure.

The USB session also reported `INA219 NOT FOUND — no battery protection this
session`. The restored firmware reports the same warning. Battery/INA219 power
conditions were not established, so this is neither a diagnosis of permanent
hardware failure nor clearance for powered operation.

## Verified restoration and remaining gate

The [rollback upload](otto-rollback-upload.log) restored all 4,194,304 bytes of
the successful pre-upload backup; the written hash verified. The new R2 boot
ID is **7A67A7D7538F82FD**, distinct from pre-upload **B1D4169F542C2B54**.
[Live MQTT capture](otto-rollback-mqtt.log) and
[readback from the Pi recorder](otto-rollback-pi-proof.log) establish that the
restored `NAVI_COHERENCE_0_6_IR_SYNC_TEST_R2` reconnected and reported zero
throttle. The Pi records the fresh boot at `2026-10-14T14:43:42.248`; subsequent
loop-stat records report zero Hall/IR/publication/command queue drops during
this stationary observation. These are R2 results, not EWO acceptance evidence.

The EWO 0_5 STOP/GO change is implemented and host tested but is **not currently
running on Otto**. The [deployment result](otto-deployment-result.json) records
firmware identity, outcome and evidence hashes. The original build provenance
remains the pre-upload record; later status documentation does not rewrite it.

Further work must diagnose the startup resource failure, propose a bounded
correction, obtain approval, and repeat the stationary hardware gate before
any field test. This follows [AGENTS.md](../AGENTS.md): “If an approved design
or implementation does not work, STOP and report what happened.” The failure
does not authorize a different navigation design or changed recording limits.

At a future EWO boot there will be no previous complete lap: normal IR must
first supply that lap before timing fallback has a reference. The Pi MQTT
recorder remains active; the separate native ADC recorder service was observed
inactive and has not been changed. No Pi services were modified.
