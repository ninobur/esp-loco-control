# NAVI native Hall + accepted IR recorder

Status: **implemented, compiled, flashed for validation, and captured on the Pi**.

Build: `NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3_SYNC_R1`

This is a narrow, observation-only instrument for Toby. It records the two
physical streams on the locomotive's ESP32 clock so offline analysis can join
them without reconstructing timing from PWM or from the 1 Hz dashboard.

## What is recorded

### Hall stream

`hallTask()` records one `NSR1` Hall sample on every existing 1 kHz task tick.
Each sample contains:

- the measured inter-sample interval in microseconds, saturating at 65535;
- all five ADC readings used by `hallRead()`;
- the exact median-of-five value passed to `ExcursionDetectorX22R`;
- applied PWM, commanded PWM and context flags.

The recorder does not reread the sensor and does not replace the value used by
the detector. The five raw readings and median are copies made after the same
reads that the existing detector receives.

Samples are sent in batches of 48. A batch header contains the first sample
sequence, local `esp_timer_get_time()` timestamp, local `millis()` timestamp,
NAVI MM/direction/station context, locomotive boot ID and cumulative recorder
ring drops.

### Accepted IR stream

The existing ESP-NOW callback and admission path remain unchanged. After
`MovementSource::receive()` accepts a type-5 snapshot, the recorder copies:

- local ESP32 receive timestamp;
- source MAC and accepted-vs-new-TX-boot result;
- IR health fault and readiness at admission;
- applied/commanded PWM and NAVI context;
- the complete 110-byte `IrMovementWire::WireSnapshot`, including TX
  `capturedUs`, sequence, cumulative pulses, pitch, optical reason and all
  continuity counters.

This is the accepted approximately 100 ms packet stream. It is independent of
`serviceIr()`, which continues to publish the existing approximately 1 Hz
dashboard/status representation.

### Loss and gap evidence

The receiver writes every UDP datagram before decoding it. The package makes
loss visible rather than repairing it:

- Hall sample timing records measured gaps and a late flag;
- a full locomotive Hall ring drops whole batches and increments a cumulative
  Hall-drop counter;
- accepted IR records use a separate bounded ring and cumulative IR-drop
  counter;
- ESP-NOW input queue drops remain visible in the status record;
- UDP send failures are cumulative;
- the Pi receiver detects sequence gaps and compares them with the
  locomotive's drop counters;
- a reboot starts a new recorder session and carries a new locomotive boot ID.

No record is fed back into NAVI, the detector, IR health, stations, throttle,
AUTO or safety. The recorder is send-only and the Pi receiver is listen-only.

## Wire and capture format

The UDP datagram magic is `NSR1`, version 1. The Python source of truth is
[`tools/navi_sync_format.py`](../tools/navi_sync_format.py), and the ESP32
layout is [`NaviSyncRecorder.h`](../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/NaviSyncRecorder.h).

| Record | Payload | Expected rate |
|---|---:|---:|
| Hall batch | 56-byte header + 48 x 18-byte samples | about 20.8/s |
| accepted IR | 56-byte header + 133-byte snapshot | about 10/s |
| status | 56-byte header + 52-byte status | 1/s |

The expected payload plus UDP/IP overhead is approximately 20–21 KB/s. The
Hall and IR records do not rely on arrival order; their local ESP timestamps
are the join key.

The Pi capture is an `NSRCAP01` file: a 24-byte file header followed by
`receiver_wall_time_us`, datagram length and the exact datagram bytes. A
truncated final frame is left as-is and is ignored only at the end of decode.

Decode without interpretation:

```sh
python3 tools/navi_sync_decode.py /path/to/navi_sync_YYYYMMDD_HHMMSS.nsr \
  --hall-csv hall.csv --ir-jsonl ir.jsonl --status-jsonl status.jsonl
```

## Pi receiver

The firmware sends to the fixed Pi address already used as the MQTT broker:
`192.168.68.142:47620`.

Install the two receiver files without touching firmware:

```sh
scp tools/navi_sync_receiver.py tools/navi_sync_format.py \
  david@192.168.68.142:/home/david/NGR/
ssh david@192.168.68.142 \
  'mkdir -p /home/david/NGR/navi_sync && \
   python3 /home/david/NGR/navi_sync_receiver.py \
   --port 47620 --outdir /home/david/NGR/navi_sync'
```

The output is `/home/david/NGR/navi_sync/navi_sync_YYYYMMDD_HHMMSS.nsr`.
Start the receiver before powering or moving Toby. Stop it with Ctrl-C after
the run; the receiver prints packet counts, sequence gaps, and status loss
counters.

An optional unit is supplied for later installation, but it has not been
copied or enabled on the Pi:

```sh
sudo install -m 0644 tools/ngr-navi-sync-record.service \
  /etc/systemd/system/ngr-navi-sync-record.service
sudo systemctl daemon-reload
sudo systemctl enable --now ngr-navi-sync-record.service
```

The unit writes the same `/home/david/NGR/navi_sync` directory and restarts
the listen-only receiver after failure.

## Verification

Host recorder gate:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -I firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH \
  firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/tests/test_navi_sync_recorder.cpp \
  -o /tmp/test_navi_sync_recorder
/tmp/test_navi_sync_recorder
python3 -m unittest tools.tests.test_navi_sync_format -v
```

ESP32 verification:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --clean \
  --build-path /tmp/navi_sync_build \
  firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH
```

The verified compile used ESP32 core 3.3.11 and produced 1,060,831 bytes of
flash and 106,540 bytes of global RAM. The only warnings were the three
pre-existing enum-conversion warnings in the vendored Adafruit INA219 library;
there were no warnings from the sketch or NSR1 recorder.

## 2026-09-27 validation run

The recorder was flashed to Toby from the NSR1 build at commit `7b83179`.
The first capture exposed a CRC-table interoperability defect: the firmware
used `0xD6D930AC` where the standard nibble-table entry is `0xD6D6A3E8`.
Commit `d54ab3f` corrects that entry and adds the `123456789` CRC-32 test
vector. The corrected image compiled successfully and was flashed after the
fix. The Pi receiver then decoded the new frames with zero CRC errors.

The preserved capture is:

```text
/home/david/NGR/navi_sync/navi_sync_20260927_200313.nsr
```

It contains two Toby boot sessions. The corrected post-reboot session is
`2572337812` (loco boot `9034675431648489490`). The final decode reported:

```text
bad=0, hall_samples=703632, ir_snapshots=6945, status=698
```

The first approximately 30 seconds after the reboot showed a 1 kHz Hall
source cadence with `dt_us` median 1000 us, p95 1088 us, p99 1477 us and
maximum 10222 us. The initial cumulative Hall-ring count was 1 and did not
increase during that window; IR was accepted at approximately 10 Hz with no
IR input-queue or IR-ring drops and no UDP failures.

The deliberate movement window ran approximately from Toby time 447.995 s
through 665.166 s. During that window the decoded Hall samples had median
`dt_us` 1000 us, p95 1164 us, p99 1534 us and maximum 4015 us. There were no
Hall batch sequence gaps in the received movement window, and the cumulative
Hall-ring and UDP-failure counters did not increase during motion. NAVI
context changed from stationary/unknown to known position and direction; the
recorded accepted IR stream supplied 2398 additional wheel pulses in the two
movement segments.

The capture was intentionally left recording for several minutes before the
movement began. That long unattended period saturated the recorder's network
transport: cumulative `hall_ring_drops` reached 118 and `udp_failures` 99
before motion, then remained flat during motion. Those losses make the full
capture incomplete as a continuous evidence stream, but they were not caused
by the movement interval and did not coincide with a change in Hall-task
timing. Future long captures should address this transport-capacity issue
before being treated as complete evidence.

The IR stream itself was accepted and traceable, but not continuously
measurement-ready: during the movement windows 1492 snapshots were healthy/
ready and 699 reported `INADEQUATE_CONTRAST`. This is an observation from the
field run, not a navigation decision or a recorder feedback path.

The receiver was stopped after the capture to preserve it. No thresholds,
navigation logic, station logic, throttle behavior or safety logic were
changed for this validation.
