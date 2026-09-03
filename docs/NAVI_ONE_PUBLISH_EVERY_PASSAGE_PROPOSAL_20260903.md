# NAVI_ONE — publish every judged passage: a proposal, with the field's numbers

**Date:** 2026-09-02, rewritten 2026-09-03 on the operator's field run and
Sam/CODEX's review.
**Locomotive:** Toby (9950012)
**Status:** PROPOSAL, GATED. No permanent firmware is proposed for building.
The measurement gate this document used to ask for has been **run** — by the
operator, with his own diagnostic sketch — and its results are below.
**Serves:** decision 0075 (proposed).

---

## What changed since the first draft

Everything quantitative. The first draft estimated the archive's size and rate
from the 215 waveforms in the mirror, which were there **because** they were
refusals or members of a withdrawal window — a sample selected for being
abnormal. The review said so, and asked for instrumented measurement over
complete runs before anything was built.

The operator built `NAVI_ONE_STATION_CURVES_0_1` from X11 and ran it. It keeps
X11's Hall acquisition, median-of-five reads, Gaussian recognizer, navigation,
station ramps, dwell, departure and stopping behaviour unchanged, and adds one
thing: every completed passage is published immediately — the binary waveform
on `diag/waveform` as before, plus a companion JSON on `diag/wave_meta`.

Every number below is from that run, decoded into the curve database.

## The measurement

**The run.** 2026-09-03, 15:08:00 to 15:52:04, 44.1 minutes. 1,177 passages,
sequence 1 to 1177, **no gaps**. `pub_drop` 0, `cmd_drop` 0. 351 CW, 826 CCW.
344 station-associated. Every station-associated passage accepted. Five
refusals, all `TOO_SOON`, all off-station near MM001–002. No paused or
stitched passage occurred.

**Size and rate.**

| | |
|---|---|
| passages a minute | 26.7 mean; busiest minute **50** |
| samples per passage | median 148, p95 270, max 494 |
| bytes on the wire (40-byte header + 2 per sample) | median 336, mean 375, max 1,028 |
| fitting one 704-byte MQTT payload (n ≤ 332) | **1,135 of 1,177 = 96.4%** |
| frames a second | 0.46 mean, about 0.9 in the busiest minute |
| **archive traffic** | **167 B/s mean, 319 B/s in the busiest minute** |

Against Toby's own 2.1 KB/s at cruise, the archive is about **8%** of his
traffic, and under one message a second against his 8.5. The first draft
guessed 370 to 600 B/s. The field says 167.

**Residual and record length by station phase** — the part that is evidence
about the recognizer and not only about the radio:

| phase | n | median resid | max resid | refused | median samples |
|---|---:|---:|---:|---:|---:|
| cruise / IDLE | 833 | 0.0691 | 0.0990 | 5 | 145 |
| approach | 135 | 0.0735 | 0.0885 | 0 | 161 |
| zone | 128 | 0.0735 | 0.0897 | 0 | 222 |
| **zero ramp** | 54 | 0.0820 | **0.1150** | 0 | **331** |
| **departure** | 27 | 0.0741 | **0.1180** | 0 | 309 |

**The longest records are made where the locomotive moves slowest.** A
zero-ramp passage runs to a median of 331 samples against 145 at cruise —
exactly the 332-sample one-frame boundary. So the records that a dwell-time
drain would carry are the *largest* ones, and a station is where they are
produced, not merely where they would be sent. And 0.1180 against the 0.13
ceiling is the closest any accepted passage came all day: the departure is
where the recognizer has least margin, which is consistent with the
archaeology's own warning about deceleration within a fragment.

## Verbatim or 8-bit

Unchanged from the first draft, and still measured on the 140 accepted 16-bit
passages of 08-31 to 09-02, quantized to int8 at scale 2 and run through
`tools/two_sided/calibrate.cpp` and `consistency.cpp`:

| | 16-bit verbatim | 8-bit, scale 2 |
|---|---|---|
| whole-passage residual, median / p95 / max | 0.0727 / 0.1034 / 0.1109 | 0.0728 / 0.1035 / 0.1107 |
| tail residual against the trunk, median / max | 0.0084 / 0.0206 | 0.0093 / 0.0204 |
| tails over the 0.13 ceiling | 0 of 137 | 0 of 135 |
| **refused as "two apexes"** | **3 of 140** | **5 of 140** |

The fit does not care; the structural rule does. Two counts of quantisation
noise on a flank manufacture a second apex twice more per 140 real magnets.
Halving the bytes now saves about 80 B/s of a 167 B/s stream, and it makes the
stored copy differ from the copy the firmware judged, so a replay would no
longer reproduce the field verdict to the count (0065, 0071).

**Recommendation: verbatim.** The trailer below still carries a `quant` byte,
and the decoder already reads both, so the choice stays open without being
taken now.

## What a permanent protocol needs that the diagnostic does not have

The operator's own list, and it is the right one:

1. `seq` restarts at every reboot.
2. There is no generated unique `boot_id`.
3. `seq` is only in `wave_meta` — not in the binary waveform, not in
   `mm/marker`.
4. Delivery is best-effort through the existing RAM queue at QoS 0.
5. NAVI_ONE has no CTO, so contention is untested.

Item 4 of that list is now on the Pi side: `decode_curves.py` ingests
`wave_meta`, joins it to the binary waveform, and stores `seq`, the phases,
the station, `stitchAt`, `restLevel` and `preSamples` with `wire` provenance.
All 1,177 metas joined to their waveforms exactly.

### Identity on the wire

`boot_id` — generated once at boot, 32 bits, from the hardware RNG — and
`passage_seq` go into **all three** places: the binary waveform, `wave_meta`,
and `mm/marker`. `(loco, boot_id, passage_seq)` then joins them deterministically
and the Pi's derived boot and heuristic match become a legacy path.

For the binary waveform, a 16-byte trailer **after** the samples, so every
existing decoder — all of which read `b[40:40+2n]` and stop — stays correct:

```
struct WavTrailer {        // little-endian, packed, AFTER the samples
  char     magic[2];       // "S1"
  uint32_t bootId;         // generated at boot; 0 = not supported
  uint16_t passageSeq;     // judged passages since boot, from 1
  uint16_t stitchAt;       // first sample of the departure; 0 = not joined
  int16_t  restLevel;      // level rested at during a pause; 0 = never paused
  uint16_t preSamples;
  uint8_t  quant;          // 1 = int16 verbatim; q > 1 = int8, each q counts
  uint8_t  flags;          // bit0 archive, bit1 refusal dump, bit2 window dump
};
```

Same topic, because the Pi's logger base64-encodes binary **only** on
`diag/waveform`; a new binary topic published before the logger was updated
would be written raw and lose every byte over 127, which is exactly how six
lines of 2026-08-31 were lost. Gate 5 asserts that the first `40 + 2n` bytes
are untouched, that a decoder ignoring the trailer still reads the samples,
that every field round-trips, and that 326 samples plus trailer fit 704 bytes.

### Queue discipline, corrected

The first draft's rule was wrong in two ways the review named, and both are
fixed here.

**It may only send when the locomotive is stopped.** Not `!autoRunning` — a
locomotive under manual control is moving, and its commands matter more than
its diagnostics, not less. The gate is `actualPwm == 0 && commandedPwm == 0`.

**It is rate-limited in every state, including a dwell.** A token bucket, not
"one per network-task pass": at a 10 ms task period that was 100 publishes a
second, which at the 1 Mbps floor is most of the channel. Start at 2 to 5
frames a second and let measurement move it. At the measured mean of 26.7
passages a minute, 3 frames a second empties a full inter-station backlog in
well under a minute of dwell.

**It suspends entirely** while CTO is paired, while traffic is holding or
decelerating, while peer freshness is degrading, on any suspicion of a channel
change, and whenever ordinary outbound traffic is backed up. It is enqueued
only on a pass where `pubQ` came up empty, and `mqtt.loop()` is serviced on a
reserved interval rather than after an unbounded drain.

**What that does and does not guarantee.** It limits each scheduling decision
to one publish. It does **not** bound the end-to-end delay imposed on a CTO
beacon: driver contention between TCP/MQTT and ESP-NOW, retransmission,
backoff, rate fallback, the blocking `mqtt.publish()` call, task scheduling,
and several locomotives at one station are all outside it. That impact is
**unbounded until measured**, and 0075 gates a build on measuring it.

### Sizing, now from a measurement

At a median 336 bytes a passage, an inter-station run of the longest leg
(Patio to Grillers, 48 markers) is about 16 KB. A **32 KB** FIFO holds roughly
95 passages, comfortably more than the longest leg and about 3.5 minutes at
the measured mean rate. That size is now derived from the field rather than
guessed — but it is still contingent on the free-heap figure, which this
lineage has never reported. `state/loopstat` must carry `ESP.getFreeHeap()`
and `ESP.getMinFreeHeap()` first.

### Delivery, described honestly

Every judged passage is **offered** to the archive; loss is **detectable**.
The FIFO drops its oldest record when full and counts it as `arch_drop`; the
sequence number makes a gap visible on the Pi, which the decoder's `audit`
command reports per boot. That is not retention, and this document does not
claim it is. Literal completeness would need durable buffering on the
locomotive or an acknowledged protocol with retransmission, and neither is
proposed.

## Cost summary

| | |
|---|---|
| bytes | +167 B/s measured mean, +319 B/s busiest minute (~8% of Toby's own) |
| frames | 0.46/s mean; 96.4% of passages are one frame |
| airtime | ~0.6 ms/s at MCS0, ~3.2 ms/s at the 1 Mbps floor — **Toby's own share**; the effect on CTO is not bounded by this and is untested |
| RAM | 32 KB FIFO, after free heap is reported |
| flash | small: one encoder call, one FIFO, one drain rule |
| wire compatibility | every existing decoder unchanged; gate 5 asserts it |
| Pi | already done — `wave_meta` ingested, trailer decoded when present |

## What needs the operator's hand

1. Whether to build a permanent archive at all, and verbatim (recommended) or
   8-bit.
2. A build that reports free heap, flashed and read, before any FIFO is sized.
3. The token-bucket rate and the stopped-only gate are engineering choices to
   be replaced by the field's.
4. **The CTO gate.** Two locomotives, weak WiFi, broker failure, archive on
   and off, comparing peer receive gaps, beacon jitter, stale transitions,
   fleet holds, MQTT command latency, Hall-task timing and minimum heap. Until
   that is run, nothing here should be flashed on a build that has CTO.
5. Whether the station-curves sketch should keep running meanwhile. It is
   already producing exactly the records the database wants, and its 1,177
   passages are the best evidence the archive has.
