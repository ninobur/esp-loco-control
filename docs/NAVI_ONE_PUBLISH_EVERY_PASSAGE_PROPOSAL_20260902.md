# NAVI_ONE — publish every judged passage: a proposal with numbers

**Date:** 2026-09-02, evening
**Locomotive:** Toby (9950012)
**Status:** PROPOSAL. No firmware has been modified. Nothing has been flashed.
Every figure below is measured on the telemetry mirror or computed from the
fielded wire format; none is from a build that exists.
**Serves:** decision 0072 (proposed), the magnet-curve database.

---

## What is asked

Publish every passage the recognizer judges — accepted or refused — so the
database on the Pi holds every curve, not only the 2% that today's build
publishes on refusal or on withdrawal. Carry `stitchAt` and `restLevel` on the
wire. Do it so that archive traffic never competes with running.

The operator's concern is stated precisely: not bits, **radio airtime**. The
fielded QUORUM firmware shares the 2.4 GHz radio with ESP-NOW for inter-loco
CTO; NAVI_ONE does not have ESP-NOW yet, and the build that first has both
inherits whatever this proposal costs.

## The numbers

### What the archive would carry

From the 215 wire passages in the mirror (2026-08-31 to 2026-09-02):

| | |
|---|---|
| samples per passage, mean / max | 221 / 501 |
| bytes per passage on the wire (40-byte header + 2 per sample + 12-byte trailer), mean / max | 494 / 1,054 |
| passages that fit **one** 704-byte MQTT payload (n ≤ 326) | 186 of 215 (87%) |
| passages needing two | 29 (13%): the slow and the decimated ones |

At 45 markers a minute, mean records: **370 B/s**, under one message a second.
If every record were the two-frame maximum: 790 B/s, 1.5 messages a second.

Against Toby's own traffic at cruise, measured 2026-09-02: **2.1 KB/s** by
the operator's measurement; 8.5 messages a second on the mirror (10:49–10:52,
98 markers in 180 s, payload only 1,258 B/s, of which `alert` alone is 549 B/s).
The archive is +18% of bytes and +9% of messages in the typical case.

### Airtime

One full frame — 704 B payload, MQTT fixed header and topic, TCP/IP, 802.11
MAC — is about 825 bytes on the air.

| rate | one frame, with preamble, SIFS, ACK, DIFS | at 45 passages/min, mean record | if every record were two frames |
|---|---|---|---|
| 802.11n MCS0, 6.5 Mbps | ≈ 1.2 ms | ≈ 0.9 ms/s (0.09% of the channel) | ≈ 1.8 ms/s |
| 802.11b, 1 Mbps (the floor a weak link falls to) | ≈ 7 ms | ≈ 5 ms/s (0.5%) | ≈ 10 ms/s (1%) |

Toby's existing 8.5 messages a second cost roughly 4 ms/s at MCS0. The archive
adds about a quarter to *Toby's* airtime and under a tenth of a percent to the
channel's.

### ESP-NOW

QUORUM 1.14's CTO beacon runs at `CTO_TX_INTERVAL_MS = 500`. ESP-NOW frames
default to 1 Mbps and take about a millisecond each. A Wi-Fi frame already on
the air is not pre-empted, so the worst an archive frame can do to a beacon is
delay it by **one frame's airtime: 1.2 ms typical, 7 ms at the 1 Mbps floor**,
against a 500 ms period. That is the bound the design below guarantees. It is
a bound by construction, not a measurement: the first build carrying both must
measure the beacon's jitter with the archive running and without.

### 8-bit or 16-bit

The survey of 2026-08-28 published 8-bit samples at scale 2 and the
calibration used 187 of them without complaint. Measured on the 140 accepted
16-bit wire passages, quantized to int8 at scale 2 and run through the same
tools (`tools/two_sided/calibrate.cpp`, `consistency.cpp`):

| | 16-bit verbatim | 8-bit, scale 2 |
|---|---|---|
| whole-passage residual, median / p95 / max | 0.0727 / 0.1034 / 0.1109 | 0.0728 / 0.1035 / 0.1107 |
| half-amplitude disagreement, median / p95 / max | 1.9% / 6.1% / 15.2% | 2.3% / 7.1% / 20.9% |
| tail residual against the trunk, median / max | 0.0084 / 0.0206 | 0.0093 / 0.0204 |
| tails over the 0.13 ceiling | 0 of 137 | 0 of 135 |
| **refused as "two apexes"** (the structural rule) | **3 of 140** | **5 of 140** |

The fit does not care. The structural rule does: two counts of quantization
noise on a flank is enough to manufacture a second apex twice in 140. Halving
the bytes saves about 0.5 ms of airtime per passage and costs the archaeology
two false structural refusals per 140 real magnets, and it makes the archive's
copy differ from the copy the firmware judged, so a replay would not reproduce
the field verdict to the count (0065, 0071).

**Recommendation: verbatim.** Bytes were never the concern, and the airtime
difference is half a millisecond. The trailer carries a `quant` byte so the
8-bit encoding can be switched on for a passage that would otherwise need two
frames, if the field ever says the second frame matters; the Pi decoder already
reads both.

## The design

### 0. First, a number nobody has

`state/loopstat` gains `heap_free` (`ESP.getFreeHeap()`) and `heap_min`
(`ESP.getMinFreeHeap()`). The static RAM figure is 64,508; free heap under
Wi-Fi, MQTT and the Hall task has never been reported. No queue is sized
until it is.

### 1. Wire format: the same topic, the same header, a trailer

`diag/waveform` and its 40-byte `WavHeader` stay **byte-for-byte** what they
are. After the samples of a chunk, 12 bytes are appended:

```
struct WavTrailer {        // little-endian, packed, AFTER the samples
  char     magic[2];       // "S1"
  uint16_t stitchAt;       // first sample of the departure; 0 = not joined
  int16_t  restLevel;      // level rested at during a pause; 0 = never paused
  uint16_t preSamples;     // stored points before the excursion opened
  uint8_t  quant;          // 1 = int16 verbatim; q > 1 = int8, each q counts
  uint8_t  flags;          // bit0 archive, bit1 refusal dump, bit2 window dump
  uint16_t seq;            // judged passages since boot; a gap is visible
};
```

Why a trailer and not a longer header: every decoder in the repo reads
`b[40:40+2n]` and stops. A trailer leaves all of them correct;
`decode_curves.py` looks for the magic at the end and reads it. Why the same
topic: the Pi's logger base64-encodes binary **only** on `diag/waveform`
(`ngr_runlog.py`, `topic.endswith("/diag/waveform")`). A new topic published
before the logger was updated would be written raw and lose every byte over
127 — which is precisely how six lines of 2026-08-31 were lost. Reusing the
topic removes a deploy-ordering hazard instead of documenting one.

**Gate 5** (`gate_waveform.cpp`) gains: the first `40 + 2n` bytes of a chunk
are unchanged by the trailer; a decoder that ignores the trailer reads the
samples correctly; the trailer round-trips every field; a chunk of 326 samples
plus trailer fits 704 bytes.

The chunk capacity drops from 332 samples to 326 to make room for the trailer.
Six samples of a 700-byte payload; no passage in the mirror crosses from one
frame to two because of it.

### 2. Where the record is made

In `hallTask()`, where `waveformWindow.push(p, v)` already copies the passage
while its buffer is still valid: encode the archive frames there, into a byte
FIFO, on the Hall task. The Hall task never blocks; encoding 500 samples is a
`memcpy`.

Every judged passage is encoded once, flagged `archive`. A refusal still
publishes its slot at once, flagged `refusal`, as today; a withdraw still
dumps the window, flagged `window`. The Pi dedupes on the passage's own
open/close times, so the extra copies cost nothing but the bytes, and each
arrival is recorded.

### 3. A queue below every other queue

A **byte FIFO of 32 KB** in the Hall task's memory, holding about 65 mean
passages, about 87 s of cruise at 45 markers a minute — longer than the longest
inter-station run at cruise (Patio to Grillers, 48 markers). Frames are cut
from it on the way out. Sized only after step 0 says the heap allows it; 16 KB
with the trickle rule below is the fallback.

The network task's loop today: connect if needed, `mqtt.loop()`, then drain
`pubQ` completely. Add, after that and only if `pubQ` came up empty on this
pass:

| state | archive frames sent on this pass |
|---|---|
| `stationMachine.holding()` (zero-ramp or dwell), or `!autoRunning` (MANUAL, released, e-stopped, low voltage) | **one** — about 100 a second, the FIFO empties in under a second at a dwell |
| running, FIFO below 75% | **none** — it waits for the dwell |
| running, FIFO at or above 75% | one, and not again for 2 s — a trickle, so a long run without a stop cannot fill it |
| broker away | none; the FIFO holds. When full, the **oldest** record is dropped and `arch_drop` counts it in the status line. Loss is counted, never silent — the rule the transport was rebuilt on. |

Three properties follow, and they are the whole point:

- **Never ahead of a running message.** An archive frame is enqueued to the
  radio only on a pass where nothing else was waiting. A marker event, a
  station order, an alert, an e-stop acknowledgement never queue behind a
  waveform.
- **At most one frame in the air at a time from this path**, so the delay it
  can impose on anything — a CTO beacon included — is one frame's airtime.
- **By preference, none of it while moving.** On an ordinary lap with station
  stops every archive frame goes out during a dwell, when nothing about the
  locomotive's movement depends on the radio.

### 4. What the Pi sees

`decode_curves.py` already: reads the trailer when present and records
`stitch_at`, `rest_level`, `pre_samples` as `wire` instead of `derived`;
reads `quant` and widens int8 records on the way in; records `flags` and
`seq`; dedupes the archive copy against the refusal and window copies; matches
the marker event by peak and residual over a thirty-minute window, so a record
that waited at a dwell still finds its event. Nothing on the Pi needs to change
for this proposal, and it has been proven on the fielded format and on a
synthetic trailer round-trip.

## Cost summary

| | |
|---|---|
| flash | small: one encoder call, one FIFO, one drain rule |
| RAM | 32 KB (or 16 KB) FIFO, after step 0 |
| bytes | +370 B/s typical at 45 markers/min, +790 worst |
| airtime | +0.9 ms/s typical at MCS0; one frame (≤ 7 ms at the floor) is the most any single message can be delayed |
| when | by preference at station dwells; otherwise trickled at one frame per 2 s |
| wire compatibility | every existing decoder unchanged; gate 5 asserts it |
| Pi | no change needed; the logger already encodes this topic |

## What needs the operator's hand

1. Whether to do it at all, and whether verbatim (recommended) or 8-bit.
2. Step 0: a build that reports free heap, flashed and read, before the FIFO
   is sized.
3. The FIFO size and the 75% / 2 s trickle numbers are engineering guesses to
   be replaced by the field's: how long is the longest run between dwells at
   the marker rate he actually runs.
4. The ESP-NOW bound is by construction. The first build with both must
   measure beacon jitter, archive on and off.
5. This modifies `NAVI_ONE.ino`, `WaveformDump.h` and gate 5. Not to be done
   without his say-so.
