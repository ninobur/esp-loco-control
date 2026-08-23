# HALL_WAVEFORM_TEST capture format

**INVESTIGATORY / UNAPPROVED.** Diagnostic instrument format, version `HWT1`.
Nothing here is an operational interface, and no field in it is navigation
ground truth.

Authoritative definition: `HallCapture.h`. The Python side
(`tools/hwt_format.py`) mirrors it and must be changed in step; the host tests
assert the struct sizes and the CRC agreement, so a drift between them fails
the test run rather than corrupting a capture.

All integers are **little-endian** and **unaligned-packed**.

---

## 1. Three classes of information, kept apart

The whole point of the experiment is that these never get mixed up.

| class | where it lives | what it is |
|---|---|---|
| **Physical measurement** | sample records; `STATUS` records | raw ADC counts, microsecond timestamps, measured cadence, loss counts |
| **Annotation** | 2 bits per channel in each sample | what the *old Module C threshold rule* **would** have said. A label, not a detection. It gates nothing |
| **Operator label** | `ANCHOR` records | a person, saying where the locomotive physically was. The only ground truth in the file |

Motor context (PWM, direction, E-stop, test-mode flags) rides with every
sample as a fourth, separate group — it describes the locomotive's state, not
the magnetic field.

Explicitly **not** in this format, by design: block identity, marker number,
QUORUM position, event counts, "genuine"/"phantom" verdicts. None of those are
measurements, and the instrument does not produce them.

---

## 2. Record (one UDP datagram)

Every datagram is a 52-byte header plus a payload determined by `recType`.

### 2.1 Header — 52 bytes

| offset | size | field | meaning |
|---|---|---|---|
| 0 | 4 | `magic` | `"HWT1"` |
| 4 | 1 | `version` | format version, currently 1 |
| 5 | 1 | `recType` | 1 `SAMPLES`, 2 `ANCHOR`, 3 `STATUS` |
| 6 | 2 | `nSamples` | samples in payload (0 for anchor/status) |
| 8 | 4 | `locoId` | 9950011 Otto / 9950012 Toby |
| 12 | 4 | `sessionId` | random per boot. **Never join records across sessions** |
| 16 | 4 | `batchSeq` | monotonic per session, shared by all record types. A gap **is** a lost datagram |
| 20 | 4 | `firstSampleSeq` | sequence number of payload sample 0 |
| 24 | 8 | `t0Us` | `esp_timer_get_time()` of payload sample 0 |
| 32 | 4 | `missedBefore` | slots skipped immediately before `firstSampleSeq` |
| 36 | 4 | `cumMissed` | cumulative skipped slots this session |
| 40 | 4 | `cumQueueDrops` | cumulative batches the locomotive's ring dropped |
| 44 | 4 | `maxGapUs` | worst acquisition gap seen this session |
| 48 | 4 | `crc32` | CRC-32 (IEEE, reflected — `zlib.crc32`) over the header with this field zeroed, then the payload |

`firstSampleSeq` is 32-bit and wraps after 2³²  samples — about **49.7 days**
of continuous 1 kHz acquisition. No session comes close; a decoder that ever
sees the wrap must treat it as a wrap, not as a reset, and the boot `sessionId`
is what actually separates runs.

`t0Us` is 64-bit microseconds from boot and does not wrap in any practical
session (≈584,000 years).

### 2.2 `SAMPLES` payload — 10 bytes per sample

`nSamples` is 125 in normal operation, so a full datagram is **1302 bytes** —
inside the 1472-byte unfragmented UDP payload. A batch is always a *contiguous*
run of really-acquired samples: a sampler stall closes the open batch short.

| offset | size | field | meaning |
|---|---|---|---|
| 0 | 2 | `ch0` | bits 0–11 **raw 12-bit ADC, exactly as read**; bits 12–13 annotation (0 none, 1 N, 2 S); bit 14 channel present; bit 15 reserved |
| 2 | 2 | `ch1` | same layout, second channel. **Bit 14 clear = the channel was not read.** Its raw bits are then meaningless and must never be plotted as a reading of zero flux |
| 4 | 2 | `dtUs` | microseconds since the previous *acquired* sample, saturating at 65535 |
| 6 | 1 | `pwmActual` | motor PWM at this sample |
| 7 | 1 | `pwmCommanded` | ramp target at this sample |
| 8 | 1 | `ctx` | bit 0–1 direction (0 REV, 1 NEUTRAL, 2 FWD), bit 2 E-stop, bit 3 fixed-PWM mode, bit 4 test step running, bit 5 this sample was late (>1.25 × nominal slot) |
| 9 | 1 | `pad` | zero |

**No averaging.** One `analogRead()` per channel per slot; the recorded number
is the number the ADC returned. The base sketch's 12-read, 300 µs-settled
average is not carried over — it cost 3.6 ms per reading and would have made
the waveform unrecoverable.

Sample timing is reconstructed as `t0Us` plus the accumulated `dtUs` of each
following sample — **measured**, never assumed from a nominal 1 kHz grid.

### 2.3 `ANCHOR` payload — 60 bytes

| size | field |
|---|---|
| 4 | `anchorId` (monotonic per session) |
| 4 | `sampleSeq` — the sample this anchor names |
| 8 | `tUs` |
| 1 | `dir` |
| 1 | `pwmActual` |
| 1 | `pwmCommanded` |
| 1 | `textLen` |
| 40 | `text` — operator's own words, unvalidated |

Anchors are the only physical-position statement in the format. They may be
inserted as often as the operator likes, including many times per lap.

### 2.4 `STATUS` payload — 53 bytes

`uptimeMs`, `sampleSeq`, `cumMissed`, `cumQueueDrops`, `cumBatches`,
`maxGapUs`, `measuredMilliHz`, `freeHeap`, `udpSendFailures`, `maxSlotUs`
(all `uint32`), then `queueHighWater`, `baselineA`, `baselineB` (`uint16`),
then `channels`, `dir`, `pwmActual`, `pwmCommanded`, `estop`, `fixedMode`,
`seqRunning` (`uint8`).

`measuredMilliHz` is **counted acquisitions over elapsed wall time × 1000**. It
is the cadence measurement the experiment needs. It is not derived from, and
bears no relation to, the 2 s telemetry rate that carries it.

`baselineB` is meaningless when `channels == 1`.

---

## 3. Capture file (what `hwt_receiver.py` writes)

```
"HWTCAP01"  uint64 start_unix_us            16-byte file header
repeat: uint64 recv_unix_us, uint16 len, len bytes   one frame per datagram
```

Datagrams are stored **verbatim, in arrival order**, including ones that fail
their CRC. The receiver never reorders, repairs or discards: the file is the
evidence, and decoding is a separate program so that a bug in analysis cannot
damage it. A truncated tail (power loss mid-write) is tolerated by the reader
and reported.

---

## 4. Decoded CSV (`hwt_decode.py`)

One row per sample, plus self-contained rows for everything that is not a
sample. Column prefixes carry the class: `phys_*` measured, `ann_*`
annotation, `ctl_*` motor context, `op_*` operator.

| `row_type` | meaning |
|---|---|
| `SAMPLE` | one acquired sample pair |
| `ANCHOR` | operator anchor, emitted immediately **before** the sample it names |
| `MISSED` | the locomotive declared slots it never acquired. **No samples exist for them** |
| `GAP` | batches lost between locomotive and recorder; the row names the exact absent sample range |
| `DROP` | batches the locomotive's own ring dropped during a transport outage |
| `SESSION` | a reboot. Nothing may be joined across this line |
| `STATUS` | a health record, with the measured cadence |
| `BAD` | an unreadable or CRC-failed datagram, with the reason |

Four different things can make a hole, and they are never merged:

- **`MISSED`** — acquisition itself stalled. The samples were never taken.
- **`GAP`** — acquisition was fine; the network lost the datagram.
- **`DROP`** — acquisition was fine; the locomotive's ring overflowed during an
  outage and dropped its oldest batches, counted.
- **`SESSION`** — the locomotive rebooted; sequence numbers restart.

Nothing is ever interpolated to fill any of them.

---

## 5. Second channel

The record always carries channel B's slot. In the default single-sensor build
its `present` bit is clear and the decoder writes the column blank — never
zero. Enabling `HWT_SECOND_CHANNEL 1` costs **no extra bandwidth or storage**
(the slot is already there); it costs one more ADC conversion per slot, and it
requires hardware that does not currently exist on either locomotive.

Because both channels are acquired inside one slot and stored in one record,
their alignment is structural: there is no sample-pairing step to get wrong,
and the host tests assert it in both builds.

---

## 6. What a decoder must never do

- Join records across `sessionId`.
- Fill a `MISSED`, `GAP` or `DROP` hole with interpolated samples.
- Read channel B's raw bits when its `present` bit is clear.
- Treat the annotation bits as detections, or as evidence that a marker exists.
- Treat sample counts, event counts or the old block decoder as position.
- Attribute a waveform to a specific magnet or track location without an
  operator anchor bracketing that pass.
