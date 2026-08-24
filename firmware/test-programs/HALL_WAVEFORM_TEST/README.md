# HALL_WAVEFORM_TEST — Hall-sensor waveform recorder

```
HALL_WAVEFORM_TEST_0_1
DIAGNOSTIC ONLY — NO NAVIGATION AUTHORITY
```

**Status: INVESTIGATORY / UNAPPROVED.** Development; built for the bench, not
compiled against the ESP32 toolchain in the environment where it was written,
and never flashed. Not operational firmware, not a QUORUM change, not a Module
C successor, and not evidence of anything until it has recorded something.

**Role:** diagnostic instrument that can drive the locomotive, because Hall
waveforms only exist while the locomotive is moving.

---

## What it is for

Record the complete Hall waveform, continuously, while the locomotive runs
under manual Blynk control or at a programmed fixed PWM, so that offline
analysis can:

- map the magnetic fingerprint of each mile marker;
- look directly at the responses that have been called "phantom";
- measure maximum positive and negative flux;
- see how an event actually opens and closes, rather than how a threshold says
  it did;
- compare repeated passes over the same marker at different speeds and
  directions;
- evaluate a possible future dual-Hall target-acquisition scheme.

It records evidence. **It classifies nothing.** There is no CFAR, no matched
filter, no dead time, no event rejection, no onboard waveform classification,
and no amplitude threshold deciding what gets recorded.

## Base and what was removed

Built from `LL_PM_Loco_ModuleC_v0_4_1.ino`, operator-selected.

**Kept:** Blynk throttle, Blynk direction, motor ramping, local E-STOP, safe
boot at PWM 0, Otto/Toby profile support.

**Removed completely:** PROTECTED/ACC/AOP modes and the mode framework; block
decoding; magnet-pair navigation; pre-acquisition reverse lockout; reversal
detection; CTO/dispatcher packet behaviour and the ESP-NOW block/status
broadcasts (ESP-NOW is gone entirely — transport here is UDP); and every path
by which a Hall reading could influence the motor.

`tests/test_no_control_authority.py` enumerates every motor-write call site in
the sketch and fails if one appears outside `setup`, `serviceRamp`,
`engageEStop`, the operator `DIR` command, or the Blynk direction handler. It
also fails if any of the removed Module C symbols reappear.

## Hardware

| | |
|---|---|
| Hall A | **GPIO 33** (ADC1_CH5) — the installed sensor, unchanged from every sketch in this repository |
| Hall B | **GPIO 35** (ADC1_CH7) — compiled out by default; see below |
| Motor | PWM GPIO 4, DIR GPIO 2, per `LL_LocoConfig.h` (same on Otto and Toby) |

**One sensor** (operator direction, 2026-08-23). `HWT_SECOND_CHANNEL` is `0`:
channel B is not read, not wired, and not assumed to exist. Setting it to `1`
is the single change that enables it — the record layout already carries
channel B's slot with its "present" bit clear, so no format, decoder, plot or
test change is needed when a second sensor is fitted.

**GPIO 35 is an unverified assumption.** No sensor is installed there on either
locomotive, and this build never reads it. It was chosen over GPIO 34 because
34 is the IR wheel sensor in `IR_DIAG`, `IR_SCOPE`, `IR_SPEED_LOCAL`, `IR_TEST`
and `Spoke_IR_RSSI_survey`, and QUORUM's pin block warns about exactly that
confusion. Both pins are ADC1 because **ADC2 cannot be read while WiFi is up**.
Before wiring anything to GPIO 35, check it against the locomotive as built —
this sketch's choice is a reading of the repository, not of the hardware.

## Build

```bash
cp ../../config/credentials_template.h credentials.h   # then fill it in
# select the locomotive in LL_LocoConfig.h (Otto by default)
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all \
  firmware/test-programs/HALL_WAVEFORM_TEST
```

Upload speed 115200 — the repository-wide rule; 921600 fails on this adapter.

Do not flash without the operator's go-ahead (standing rule).

## Running a capture

On the Pi (or any host on the railway network):

```bash
python3 tools/hwt_receiver.py --outdir ~/NGR/hwt_logs --loco <locomotive-ip>
```

Then drive. Two ways:

**Manual** — the Blynk throttle and direction controls, exactly as in the base
sketch. E-STOP works in every mode and always wins.

**Direction and Brake — CODEX safety review, 2026-08-24, changed from the
base:** the base sketch wrote the direction pin unconditionally, so a
direction command under power could reverse the H-bridge while current was
flowing. `DIR F` / `DIR R` (and the Blynk direction control) are now
**refused unless the motor is fully at rest** — PWM must already be at zero
(`STOP` or E-STOP) before a reversal is accepted; `DIR N` never touches the
pin and is unaffected. **Brake is not implemented.** It was never in this
instrument's preserve list; the Blynk Brake control now visibly refuses and
resets itself rather than silently doing nothing — use `STOP` or E-STOP to
actually stop the locomotive.

**Fixed PWM** — arm a speed, then start it explicitly. Nothing ever moves on
its own, and nothing moves at boot.

| command | effect |
|---|---|
| `FIXED 70` | arm PWM 70. **Does not move** |
| `GO` | ramp to the armed PWM |
| `STOP` | ramp to 0 |
| `SEQ` | arm the first step of the programmed sequence (50, 60, 70, 80, 90, 100, 110, 120) |
| `NEXT` | arm the next step — again, `GO` is required to run it |
| `DIR F` / `DIR R` / `DIR N` | set direction — F/R **refused** unless PWM is at zero (`STOP` first) |
| `ESTOP 1` / `ESTOP 0` | local E-STOP |
| `MANUAL` | leave fixed-PWM mode, hand the throttle back to Blynk |
| `ANCHOR <text>` | insert an operator anchor **now** |
| `STATUS`, `HELP` | health record, command list |

Commands can be typed at the receiver's prompt (sent by UDP), typed over USB
serial on the bench, or driven from Blynk: V15 arms a PWM, V16 is GO/STOP, V17
arms the next sequence step, V19 sets the anchor text and V18 inserts it.

Moving the Blynk throttle at any time cancels fixed-PWM mode — the operator's
hand beats the test script.

**E-STOP overrides everything**, including a running test step, in every mode.

## Anchors: the only ground truth

`ANCHOR patio-marker-15` stamps the operator's words against the current
sample sequence number, the timestamp, the direction and both PWM values. Use
them freely and repeatedly — at each platform, at a known marker, at the start
and end of a pass.

Nothing else in the capture is a position statement. The old block decoder,
QUORUM's declared position and any count of detected events are **not** ground
truth and are not in the file. Without an anchor bracketing a pass, a waveform
cannot be attributed to a particular magnet or piece of track, and the analysis
should say so rather than guess.

The annotation bits — what the old Module C threshold rule *would* have said —
are there so the old rule can be judged against the waveform, not so it can be
believed. Existing "genuine" and "phantom" labels are not independent evidence.

## Decoding and plotting

```bash
python3 tools/hwt_decode.py ~/NGR/hwt_logs/hwt_20260823_193000.hwt -o run.csv
python3 tools/hwt_plot.py run.csv --save run.png
python3 tools/hwt_plot.py run.csv --start 120 --duration 5 --annotate
```

The decoder prints an integrity report — samples decoded, slots never
acquired, batches lost in transport, ring drops, duplicates, unreadable
datagrams, completeness percentage — and writes one CSV row per sample plus a
named row for every hole. See `CAPTURE_FORMAT.md`.

## Numbers

Measured from the format and the configuration, single channel, 1 kHz:

| | |
|---|---|
| sample record | 10 bytes |
| batch | 125 samples = 1302-byte UDP datagram, 8 per second |
| **stream rate** | **≈ 10.4 kB/s ≈ 83 kbit/s** (plus ~50 B/s of status) |
| per hour | ≈ 37.5 MB of capture file |
| capture ring | 48 batches ≈ **6.0 s** of buffered transport outage |
| ring memory | ≈ 63.8 kB static |
| other additions | task stacks 10 kB, command/aux queues ≈ 1.5 kB |
| **total added RAM** | **≈ 77 kB** |

Enabling the second channel costs **no extra bandwidth** — the slot is already
in the record — only one more ADC conversion per slot.

Beyond 6 s of outage the ring drops its **oldest** batches (the waveform
happening now is the one worth keeping), counts them, and every later header
carries the running total. The receiver sees the `batchSeq` gap independently.
Nothing is ever dropped silently.

## Tests

```bash
firmware/test-programs/HALL_WAVEFORM_TEST/tests/run_tests.sh
```

Three suites, all on the host — no hardware, no toolchain:

- **capture engine** (g++, runs the real `HallCapture.h`): sequence ordering,
  raw fidelity, missed slots declared rather than fabricated, bounded queue
  overflow, transport loss and CRC, timestamp and counter rollover,
  dual-channel alignment, wire-format sizes;
- **decoder** (python3): reordered and duplicated datagrams, transport loss
  with the exact missing range, sampler stalls, corruption, anchors, a reboot
  mid-file, dual-channel alignment, a truncated file;
- **control-authority audit** (python3): every motor-write call site, the
  recorder functions writing no control state, the removed Module C symbols
  staying removed, one unaveraged ADC read per slot, and the boot banner.

## Limits, honestly

- **Never compiled for the ESP32** in this environment — no `arduino-cli` and
  no ESP32 core available. Structure and balance were checked statically and
  the capture engine compiles and passes under g++, but the sketch must be
  compiled for both profiles before anyone flashes it.
- **Never flashed, never run.** No claim about real cadence, ADC timing or
  WiFi behaviour is made here; the instrument measures those and reports them,
  which is the point.
- The 1 kHz slot is nominal. Whether an `analogRead()` plus the batch CRC fits
  a 1 ms slot on this core is exactly what `measuredMilliHz`, `maxSlotUs`,
  `maxGapUs` and the per-sample `late` bit are there to answer.
- UDP is unacknowledged by choice: a retransmit would let the network stall
  acquisition. Losses are counted and located instead.
- The static analysis in the authority audit cannot see through a function
  pointer and is not a substitute for reading the diff.
