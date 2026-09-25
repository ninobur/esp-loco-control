# IR_SCOPE_ESPNOW packet format

Decoding reference for the raw IR waveform captures in `field-records/logs/`
(`20260826_ir_lap01_right_CCW.log`, `20260826_ir_lap02_left_CW.log`,
`20260826_ir_lap03_04_ccw_side_compare.log`, `20260826_ir_daylight_0{1..5}_*.log`).

**Firmware is not in this repository.** Both sketches live at
`/Users/davidbrown/NGR/NGR-Files/`:

| Role | File | Version string |
|---|---|---|
| Transmitter (test car, GPIO34) | `IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino` | `IR_SCOPE_ESPNOW_TX_1_0` |
| Receiver (Pi, `/dev/ttyUSB0`) | `IR_SCOPE_ESPNOW_RX/IR_SCOPE_ESPNOW_RX.ino` | `IR_SCOPE_ESPNOW_RX_1_0` |

Line references below are to those files. Every layout claim here was
verified against 4,817 real packets, not just read off the struct — see
*Verification* at the end.

## 0. Two corrections to the brief

**`tools/IR_SCOPE_Replay.py` does not exist, and nothing parses this format
as its native input.** The replay is at
`firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py` and reads the *IR_SCOPE
CSV* produced by `IR_SCOPE_Plotter.py` over MQTT — a different, older
instrument. So this document is the authoritative definition, not a restatement
of parsing code. What does exist in this repo:

| Tool | Reads | Purpose |
|---|---|---|
| `tools/ir_scope_espnow_analyze.py` | this format, natively | packet parser + per-capture summary; `parse_file()` is the reference decoder |
| `tools/ir_scope_espnow_to_csv.py` | this format → IR_SCOPE CSV | bridge to the replay; lossy, see §6 |
| `firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py` | IR_SCOPE CSV only | falling-threshold replay |

`IR_SCOPE_Replay.py`'s CLI and output are documented in §6 anyway, since the
question was asked — but note `2026-08-26_IR_SCOPE_REPLAY_ON_FOUR_LAPS.md`:
its self-validation **fails** on converted ESP-NOW captures, so its candidate
table is not currently usable.

**The log file is `20260826_ir_lap03_04_ccw_side_compare.log`**, not
`20260826_ir_ccw_side_comparison.log`. 4,817 RX packets is correct.

## 1. The struct

`IR_SCOPE_ESPNOW_TX.ino:19–27`:

```c
struct __attribute__((packed)) Packet {
  uint16_t magic; uint8_t version, type;
  uint32_t sid, batchSeq, firstSample;
  uint16_t count, missedBefore;
  int16_t runMin,runMax,thrHigh,thrLow;
  uint32_t lateTotal,missedTotal,queueDrops,sendErrors,pulses,latch,contrastLoss;
  uint16_t sample[BATCH_N];          // BATCH_N = 96  (line 15)
  uint16_t crc;
};
static_assert(sizeof(Packet)<=250,"ESP-NOW packet too large");   // line 28
```

**`__attribute__((packed))` — there is no padding or alignment anywhere.**
Offsets are the running byte sum. Total is exactly 250 bytes, the ESP-NOW
payload maximum. **All multi-byte fields are little-endian** (Xtensa LX6).

| Off | W | Type | Field | Meaning |
|---:|---:|---|---|---|
| 0 | 2 | `uint16` | `magic` | `0x4952`. On the wire, little-endian, bytes are `0x52 0x49` = ASCII **`RI`**. (The firmware comment says `"IR"`, describing the numeric value, not the byte order.) |
| 2 | 1 | `uint8` | `version` | `1` (line 13). Observed: only 1. |
| 3 | 1 | `uint8` | `type` | `1` = sampler batch (line 40). Only value that exists — see §5. |
| 4 | 4 | `uint32` | `sid` | Session id, `esp_random()` at boot (line 65). **Changes on every TX reset** — the reboot marker. |
| 8 | 4 | `uint32` | `batchSeq` | Batch counter from 0, +1 per packet. Gaps = transport loss. |
| 12 | 4 | `uint32` | `firstSample` | Sample number of `sample[0]`. **Also milliseconds since capture start** — see §2. |
| 16 | 2 | `uint16` | `count` | Samples present. Always 96 in practice (batches ship only when full, line 54). |
| 18 | 2 | `uint16` | `missedBefore` | Sampler slots *skipped* immediately before this batch; cumulative in `missedTotal`. Non-zero means the sample grid jumped — no data is fabricated for the skipped slots. |
| 20 | 2 | `int16` | `runMin` | Envelope 5th percentile of raw. |
| 22 | 2 | `int16` | `runMax` | Envelope 95th percentile of raw. |
| 24 | 2 | `int16` | `thrHigh` | Rising threshold `= runMin + 2*span/3`. |
| 26 | 2 | `int16` | `thrLow` | Falling threshold `= runMin + span/3`. |
| 28 | 4 | `uint32` | `lateTotal` | Cumulative samples taken >250 µs off their nominal slot. |
| 32 | 4 | `uint32` | `missedTotal` | Cumulative skipped slots. |
| 36 | 4 | `uint32` | `queueDrops` | Batches dropped at the TX queue (radio couldn't keep up). |
| 40 | 4 | `uint32` | `sendErrors` | ESP-NOW send failures/timeouts. |
| 44 | 4 | `uint32` | `pulses` | Cumulative accepted **rises** — see the caveat in §3. |
| 48 | 4 | `uint32` | `latch` | Cumulative latch discards. |
| 52 | 4 | `uint32` | `contrastLoss` | Cumulative contrast-loss discards. |
| 56 | 192 | `uint16[96]` | `sample` | Waveform. See §2. |
| 248 | 2 | `uint16` | `crc` | CRC-16/CCITT-FALSE over **bytes 0–247**. See §4. |

All seven counters at 28–55 are **cumulative as of publish time**, not as of
capture. They are exact for whole-capture deltas and approximate to within one
batch (96 ms) for finer alignment. The per-sample flags in `sample[]` are
capture-exact.

## 2. Waveform samples

- **Begin at byte offset 56.** 96 samples, `uint16` little-endian, 192 bytes,
  ending at 247.
- **Rate: 1 kHz**, scheduled with `vTaskDelayUntil` (line 55) — a real fixed
  grid, not a drifting `vTaskDelay`. **Sample number is exactly milliseconds
  since capture start**, so `sample[i]` was acquired at `firstSample + i` ms.
  This is the property the whole format depends on; a stall skips slot numbers
  (`missedBefore`) rather than backfilling them with catch-up reads.
- **Bit layout** (line 53, `(raw&0xfff)|flags`; flags at line 14):

| Bits | Mask | Meaning |
|---|---|---|
| 0–11 | `0x0FFF` | raw ADC count, 12-bit |
| 12 | `0x1000` | `FLAG_INPULSE` — detector is inside a pulse |
| 13 | `0x2000` | `FLAG_RISE` — accepted rising edge at this sample |
| 14 | `0x4000` | `FLAG_FALL` — falling edge at this sample |
| 15 | `0x8000` | `FLAG_CONTRAST` — contrast gate valid at this sample |

**Invariant, verified across all 4,817 packets:** bits 12–14 never appear
without bit 15. The detector only runs inside the contrast-valid branch
(line 51), so rise/fall/in-pulse are impossible while contrast is invalid.
Observed flag nibbles are exactly `0x0`, `0x8000`, `0x9000`, `0xB000`,
`0xC000`. Note `0xC000` (fall) carries no in-pulse bit — the fall clears
`inp` before the flag is applied.

- **ADC**: GPIO34, ESP32 **ADC1**, `analogReadResolution(12)` (line 65) → 0–4095.
  Observed range across the side-comparison capture: 48–3583.
- **Reference / attenuation is NOT set by the sketch.** There is no
  `analogSetAttenuation()` call anywhere; the Arduino-ESP32 core default
  applies (11 dB / 12 dB depending on core version, nominally ~0–3.3 V and
  markedly non-linear near both rails). **Counts are therefore uncalibrated
  and not proportional to irradiance.** For the ratio-based contrast work in
  §3 this is fine. For any absolute or cross-device radiometric claim it is
  not — pin the attenuation explicitly first.

## 3. Recovering optical-quality fields

**Contrast / peak-to-trough — two measures, pick deliberately:**

1. **Envelope span = `runMax - runMin`** (offsets 22, 20). This is the
   firmware's own contrast measure: 5th/95th percentile over a rolling
   `ENV_N = 2048`-sample (2.048 s) window, recomputed every 250 ms from a
   256-bucket histogram of `raw>>4` (lines 15, 48–49). Two consequences:
   **quantised to 16 counts** (the `>>4`), and **smoothed over ~2 s** — at
   0.30 m/s that is ~600 mm, roughly two mile markers of blur. Robust to
   speed; poor spatial resolution.
2. **Per-packet raw peak-to-peak** = `max(raw) - min(raw)` over the 96 samples.
   96 ms resolution, unquantised, but depends on how many spokes fall inside
   the window, so it **drops at low speed** and is not comparable across
   speeds without care.

For contrast-versus-position work, use (1) as the primary and (2) as the
cross-check. Both are computed in `tools/ir_scope_espnow_analyze.py`.

**`contrast_valid` is derived from** (line 51):

```
valid  ==  primed  AND  (runMax - runMin) >= MIN_SPAN
primed ==  at least PRIME_N = 512 samples accumulated in the envelope window
MIN_SPAN = 120                                          (line 15)
```

So the gate is purely "the envelope has warmed up and spans ≥120 counts". It
is not a signal-shape test, and — per
`IR_STATIONARY_TRUTH_TEST_2026-08-25.md` — it does not distinguish spokes
from illumination artefacts.

**`latch` counts** pulses open longer than `LATCH_MS = 2500` (lines 16, 51).
The detector clears `inPulse` **without emitting a fall flag** — deliberately,
so the record stays honest. Any consumer pairing rises to falls must handle
unpaired rises.

**`contrastLoss` counts** only the case where contrast dropped *while a pulse
was open*; the open pulse is discarded. Contrast dropping while idle
increments nothing.

**`pulses` counts accepted rises, not completed pulses.** It increments at the
rising edge (line 51) and is never decremented by a subsequent latch or
contrast discard. This is the same semantic defect recorded against IR_DIAG in
`IR_DEV_REC/2026-08-09_SYNCHRONIZED_HALL_IR_LAP.md` §3. For completed pulses,
count `FLAG_FALL` occurrences in `sample[]` instead — but note that only works
where packets survived, whereas `pulses` is a cumulative counter that survives
transport loss. Use `pulses` deltas for distance, fall flags for pulse shape.

Detector constants for reference: `DEBOUNCE_US = 15000` rise-to-rise,
`thrHigh/thrLow` as above (line 16).

## 4. Locating a packet in time and space

**There is no shared clock and no shared counter with QUORUM.** The TX knows
only milliseconds since its own boot; Toby knows only mile markers. The join
is through **the Pi's wall clock**, and it is the weakest link in the chain.

Three clocks are involved:

| Clock | Where | Notes |
|---|---|---|
| `firstSample` + index | TX | ms since TX capture start. Exact 1 kHz grid; `missedTotal = 0` on all captures to date, so the grid has never actually slipped. No absolute epoch. |
| `millis` (RX line field 1) | RX | ms since *receiver* boot. Unrelated to TX time. Useful only for RX-side ordering. |
| Pi `time.time()` | Pi | Prepended to every line by `ir_scope_serial_record.py` at `readline()` return. **This is the join key.** |

Toby's telemetry lands in the same Pi wall clock, via the MQTT broker
(`~/NGR/telemetry/all_YYYYMMDD.log`, and the per-run
`field-records/logs/20260826_toby_9950012_*.log`). So:

> **join IR packet → Toby position on Pi wall-clock time, nearest-neighbour.**

**Offset and drift budget** — the dominant term is not clock drift:

- **Toby's `alert` cadence is 1 Hz.** Position is therefore known only to
  ±0.5 s. At 0.30 m/s that is ±150 mm; at the slower Auto Solo laps ±80 mm.
  This is the floor on spatial resolution and it is **coarser than one mile
  marker interval** (mean 305 mm) at speed.
- **Physical tow offset:** the IR sensor rides ≈1 mile marker *behind* Toby's
  Hall sensor. When joining, IR position = Toby's reported position displaced
  one marker **against** the direction of travel. This is comparable in size
  to the timing uncertainty above, so treat "±1–2 markers" as the honest
  combined error.
- **Serial/broker latency:** IR path is TX→ESP-NOW→RX→USB 921600→`readline()`,
  order of milliseconds. MQTT path is ESP32→WiFi→broker→subscriber, tens of
  milliseconds. Both negligible against the 1 Hz cadence. Measured
  nearest-neighbour match lag on landmark joins was 0.00–0.30 s.
- **Crystal drift:** ESP32 crystals are ~±20 ppm, so over a 187 s lap the TX
  grid could drift ~4 ms. Negligible.
- **`sid` changes ⇒ the TX rebooted**; TX-relative time restarts at zero and
  nothing before the change is comparable.

Worked join, as used to produce the contrast-vs-mile-marker profiles in
`2026-08-26_IR_ILLUMINATION_AND_MOUNT.md`: parse IR packets, take Pi
timestamp per packet, nearest Toby `alert` row within 1.5 s, read
`dead_reckoned_mm`. Discard the packet if no row is within tolerance. Filter
to `moving == 1` to exclude station dwell, and prefer `lc_mm` /
`last_confirmed_landmark` over `dead_reckoned_mm` where a lap ran
`NO_QUORUM` (lap 4 did).

## 5. Packet types

**The format is not multiplexed.** `type` is hard-coded to `1` at line 40 and
never varies; `version` is likewise always `1`. There is exactly one packet
type, carrying waveform.

**The log file, however, is multiplexed** — the recorder captures the RX's
whole serial stream, so a capture contains four line kinds:

| Line | Emitted | Content |
|---|---|---|
| `READY IR_SCOPE_ESPNOW_RX_1_0 channel=11 baud=921600 mac=…` | RX boot (`RX:53`) | Banner. **Known defect: `mac` reads `00:00:00:00:00:00`.** Harmless for broadcast; must be fixed before any unicast/ACK work. |
| `FORMAT RX <millis> <rssi> <len> <crc16> <hex>` | RX boot (`RX:54`) | Self-describing header for the next kind. |
| `RX <millis> <rssi> <len> <crc16> <hex>` | per packet (`RX:60–63`) | The payload. `rssi` dBm, `len` 250, `hex` 500 chars. |
| `STAT rx=… qdrop=… badlen=… q=… heap=…` | RX, every 5 s (`RX:68`) | Receiver health. `qdrop`/`badlen` were 0 on every capture to date. |

Captures may also open with a burst of binary boot noise from the ESP32 ROM
loader at 115200 into a 921600 stream — read files as bytes and skip
undecodable lines. Note the TX's own `STAT` line (`TX:73`) is **not** in these
logs; it goes to the TX's local USB, which is unattached in normal operation.

There is **no marker mechanism of any kind** — no revolution, illumination, or
operator markers. The older IR_SCOPE took these over MQTT; this firmware has
no equivalent, which is why `IR_SCOPE_Replay.py` reports `p/rev-mk: ---` on
every converted capture and why phase boundaries have had to be carried
out-of-band in `phases.tsv` and separate files.

## 6. `IR_SCOPE_Replay.py` — CLI and output

Reads **IR_SCOPE CSV**, not this format. Bridge with
`tools/ir_scope_espnow_to_csv.py capture.log out.csv [--start EPOCH] [--end EPOCH]`.

```
python3 firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py capture.csv
                                    [--frac 0.45] [--plot 0.50] [--start S]
                                    [--duration S] [--save overlay.png]
                                    [--prom 0.25] [--rev-marker rev]
```

It replays candidate falling fractions (1/3, 0.40, 0.50, 0.60, plus `--frac`)
over the recorded waveform with real detector semantics — contrast gate, 15 ms
debounce, 2500 ms latch — holding the rising rule at production. Output: a
self-validation block (the 1/3 candidate must reproduce the firmware's own
recorded edges), then a table per candidate with `pulses`, `latch`, `open`,
`unkn`, `short`, `merged`, `mpk`, `duty%`, width and interval percentiles,
`p/rev-int`, `p/7pk`, `p/rev-mk`; plus a candidate-independent physical-peak
count. `--plot` renders a waveform overlay.

**The bridge is lossy in one specific way that currently breaks the replay:**
this packet carries a single envelope per 96-sample batch, sampled at batch
start, whereas IR_SCOPE's CSV carried mid-batch recompute offsets. Replayed
thresholds are therefore stale by up to 96 ms. Together with 20–27 % transport
loss, the replay's self-check fails (81–97 % edge match) and it declares its
own candidate table suspect. Detail and consequences:
`IR_DEV_REC/2026-08-26_IR_SCOPE_REPLAY_ON_FOUR_LAPS.md`.

## Verification

Every structural claim above was checked against the 4,817 packets of
`20260826_ir_lap03_04_ccw_side_compare.log`:

- `struct.calcsize` of the header = 56; 56 + 192 + 2 = 250 = observed length
  on every packet.
- **Offsets proven by identity, not assertion:** the firmware's own relation
  `thrHigh == runMin + 2*span/3 && thrLow == runMin + span/3` held on
  **4,817 / 4,817** packets. Misplaced offsets could not satisfy it.
- `version` ∈ {1}, `type` ∈ {1}, `count` ∈ {96} across all packets.
- `firstSample` deltas are exact multiples of 96.
- Flag-nibble invariant (§2) holds on all packets.
- **Both CRCs validate 4,817/4,817**, and they differ on 4,816 of 4,817
  packets — they are genuinely different quantities (§4).

### The two CRCs — do not confuse them

| | Embedded `crc` (offset 248) | Printed `<crc16>` (RX text line) |
|---|---|---|
| Computed by | TX, `TX:54` | RX, `RX:61` |
| Over | bytes **0–247** | **all 250** bytes, including the embedded CRC |
| Detects | over-the-air corruption | serial/file transcription errors only |

Both are CRC-16/CCITT-FALSE: init `0xFFFF`, poly `0x1021`, MSB-first, no
reflection, no final XOR (`TX:36`, `RX:22–26`).

**Checking only the printed CRC does not validate the payload.** The RX
computes it over whatever it received, so a corrupted packet yields a
self-consistent line. `tools/ir_scope_espnow_analyze.py` currently validates
the printed CRC; for integrity work, check the embedded one at offset 248
over bytes 0–247.

## Reference decoder

```python
import struct
HDR = "<HBBIIIHHhhhhIIIIIII"           # 56 bytes, no padding
assert struct.calcsize(HDR) == 56
(magic, version, type_, sid, batchSeq, firstSample, count, missedBefore,
 runMin, runMax, thrHigh, thrLow, lateTotal, missedTotal, queueDrops,
 sendErrors, pulses, latch, contrastLoss) = struct.unpack_from(HDR, data, 0)
assert magic == 0x4952                  # bytes on the wire are 'R','I'
samples = struct.unpack_from("<96H", data, 56)
crc     = struct.unpack_from("<H", data, 248)[0]

raw           =  s & 0x0FFF             # per sample, 12-bit ADC count
in_pulse      = (s & 0x1000) != 0
rise          = (s & 0x2000) != 0
fall          = (s & 0x4000) != 0
contrast_ok   = (s & 0x8000) != 0
# sample i was taken at (firstSample + i) milliseconds since capture start
```

Working implementation: `tools/ir_scope_espnow_analyze.py`, `parse_file()`.
