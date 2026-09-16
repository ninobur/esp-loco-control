# X18 Lowline Hall recorder — continuous 1 kHz capture to the Pi

**2026-09-16.** Built from X18 exactly as flown, commit `242109e`
(`NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST`). This is an **observation build**:
it records, and it changes nothing.

Purpose: capture Otto's complete Hall signal across magnets *and the track
between them*, so an offline analysis can measure when an ordinary-track
baseline can be established and how that level moves from interval to
interval. That analysis chooses its own measurement boundaries, afterwards,
from the raw trace. **No baseline rule is encoded anywhere in this build.**

Nothing here has been flashed to Otto and the railway was not run.

---

## 1. The draft, and why nothing was carried over from it

`firmware/NAVI_ONE_X18_LOWLINE_HALL_RECORDER.zip` does not exist — not in the
working tree, not in any branch, not anywhere on this machine. It could not be
reviewed. The two defects named in the brief were treated as requirements
instead, and both are now structurally impossible rather than merely fixed:

| the draft's defect | what this build does |
|---|---|
| records through USB serial, unusable while Otto runs | UDP to the Pi; no serial path exists, so it cannot be used by accident |
| `HallRecord` prototype generated before the type is declared | **every** added type lives in `HallRecorder.h`, which the sketch includes above the prototype generator's insertion point. A comment in that header says so and says why. |

The second one is worth stating precisely, because it is a trap this
repository has fallen into before: the Arduino preprocessor inserts its
generated prototypes immediately after the last `#include`. A struct defined
in the `.ino` is therefore invisible to a prototype that mentions it, and the
error points at a line nobody wrote. X18's own `.ino` already hoists `Judged`,
`HallDiag`, `PubMsg` and `CmdMsg` for this reason. The recorder's types are in
a header, which is stronger.

The batching and receiver pattern comes from `HALL_WAVEFORM_TEST`/`QuorumTrace`
on the `claude/quorum-hall-waveform-diagnostic-plutez` branch — the sealed
batch header, the counted-drop ring, the write-first receiver. **None of those
sketches' driving or detection behaviour was copied**, and the wire format is
deliberately not interchangeable with theirs: `xhr_format.py` refuses `HWT1`
and `QTR1` magic by name rather than reading either as this format.

---

## 2. What is recorded

### 2.1 The value X18 actually navigates by

The tap sits in `hallTask`, immediately after

```cpp
const int16_t raw = hallRead();
```

and **before** `capture.sample()` has seen it. `raw` is the median-of-five ADC
value — the identical number, on the same tick, not a re-read and not a second
conversion. A recorded trace and X18's own rulings therefore cannot disagree
about what the sensor said.

### 2.2 Per sample, 8 bytes, nominally 1 kHz

| field | notes |
|---|---|
| `dtUs` | microseconds since the previous sample, **saturating at 65535**. A saturated value means "at least this long" and is never a silent wrap. |
| `raw` | the median-of-five Hall value |
| `pwmActual`, `pwmCommanded` | |
| `flags` | direction, e-stop, auto, `mayAdapt`, `capture.open()`, late, low-voltage |

A monotonic `sampleSeq` is carried once per batch as `firstSampleSeq` and
implied by position, so it costs nothing per sample and cannot drift.

### 2.3 Per batch, 44 bytes of header, every 100 ms

`magic`/`version`/`recType`/`nItems`, `locoId`, `sessionId`, `batchSeq`,
`firstSampleSeq`, `t0Ms`, `t0Us`, `crc32`, plus the navigation context that
ties the trace to the map:

* `baselineAtT0` — X18's **operative** `baseline_`. Context, not a rule: what
  the flown firmware was using at that moment, which is what an analysis needs
  to align this trace against X18's existing telemetry.
* `navMm`, `navDir`, `stPhase`, `ctxFlags` — mirrors written by `loop()`, read
  by `hallTask`. Carried per batch rather than per sample because none of them
  can move meaningfully faster than 100 ms, and because their job is to say
  *which mapped interval* a stretch of trace sits in.
* `ringDropsLo` — see §4.2. This is the field that makes loss attributable.

`t0Ms` is `millis()`, which is what every other X18 telemetry record is
stamped in, so the two align directly. `t0Us` is `micros()` and wraps every
71.6 minutes; intra-batch time is reconstructed from measured `dtUs`, so the
wrap is invisible in the deltas. Gate 15 tests exactly that.

### 2.4 X18's marker rulings, 44 bytes, one per ruling

Tapped in `loop()` **after** `navigator.judge()` has returned — every field is
a copy of a decision X18 has already made. `openedAtMs`/`closedAtMs`/`peak`/
`polarity`/`ruling`/`outcome`/`amplitudeRatio`/`entryBaseline`/`closeBaseline`/
`shadowBaseline`/`navMmBefore`/`navMmAfter`/`stPhase`.

`sampleSeq` is the join key: the most recently completed trace sample at the
moment of the ruling. Every recorded interval can therefore be tied to the
mapped magnet X18 believed it had just crossed.

Passages whose declaration had already ended never reach `navigator.judge()`.
They are recorded as `STALE_EPOCH` rather than dropped, so the continuous
trace is never left with an unexplained magnet-shaped hole.

### 2.5 Health, 52 bytes, ~1 Hz

Cumulative since boot, so a decoder that missed one status datagram still
recovers the totals from the next: samples, ring drops (sample and ruling),
UDP failures, worst tick gap, **worst tick body**, free heap, both task stack
high-waters, measured rate, ring high water, RSSI, MQTT state.

Stack high-water is reported in **bytes**. `uxTaskGetStackHighWaterMark`
returns bytes on ESP-IDF because `portSTACK_TYPE` is `uint8_t`. This was
labelled "words" once in this project and the mistake outlived the build that
made it; the field name now says `hall_stack_free_bytes` and the comment says
why.

### 2.6 Session identity

`sessionId` is random per boot. Sample sequence, batch sequence and `millis()`
all restart at zero on a reboot, so a tool that joined two sessions would
splice two different runs into one continuous-looking trace. **Every tool here
refuses to.** The receiver announces a new session on the console, the decoder
reports sessions separately and says they must not be joined, and `--session`
isolates one. Gate 15 tests it.

---

## 3. Every operational change against `242109e`

Fourteen of the sixteen X18 source files are **byte-identical** to `242109e`:

```
HallCapture.h   MagnetRecognizer.h   Navigator.h   Ops.h   RouteMap.h
Stations.h      LapBaselineController.h            TwoSided.h
WaveformDump.h  WaveformWindow.h     NAVIFieldConfig.h
LocoConfig.h    LL_LocoConfig_9950011.h   LL_LocoConfig_9950012.h
```

Acquisition, recognition, navigation, stations, throttle, safety and the map
were not edited at all. The full diff is in
`docs/NAVI_X18_RECORDER_VS_X18.diff`.

`NAVI_ONE_X18_RECORDER.ino`: **+279 lines, −5**. All five removed lines, in
full:

| removed | why, and what replaced it |
|---|---|
| `#define SKETCH_NAME "…X18_LAP_BASELINE_FIELDTEST"` | build identity → `…X18_LOWLINE_HALL_RECORDER` |
| `#define BUILD_SUBTITLE "X17 behavior; …"` | build identity |
| `T_…,T_BASELINE_LAP,T_CNT };` | `T_RECORDER` added to the topic enum |
| `xTaskCreatePinnedToCore(hallTask,…,nullptr,0)` | `&hallTaskHandle` — an output parameter, so the Hall task's stack high-water can be reported. No behavioural change. |
| `if (j.epoch != navEpoch) { staleJudged++; continue; }` | same two statements, now with a `STALE_EPOCH` record between them |

**No operational line of X18 was altered.** Everything else is inserted.

New files: `HallRecorder.h` (534 lines), `tests/gate_recorder.cpp`.
`tests/run_tests.sh` gained gates 14 and 15.

### 3.1 Why the recorded data cannot acquire authority

* Every recorder function is called **from** X18's control path and returns
  nothing that path reads. There is no call in the other direction.
* Every value carried is a copy, taken after X18 has already decided.
* The context mirrors are written by `loop()` and read by `hallTask`; a copy
  travels outward, and nothing `hallTask` does can reach the navigator through
  them.
* Losing every record, at any time, in any order, changes nothing.
* The recorder socket is **send-only** — no `bind()`, no listening port, no
  inbound parse. This build adds no surface that can be commanded from the
  network, and the receiver is listen-only for the same reason. (This is a
  deliberate difference from `hwt_receiver.py`, which does have a command
  path.)

### 3.2 Deliberate omissions

* **No operator-anchor command.** `HALL_WAVEFORM_TEST` had one; adding an MQTT
  subscription to X18 would be an operational change for a run boundary the
  receiver already establishes by starting a new file. Recorded as a decision,
  not an oversight.
* **The boot record was not extended.** X19 halted on a 874-byte boot record
  before `loop()` ever ran. The recorder's description goes to its own retained
  topic `diag/recorder`, bounded at 320 bytes, and that guard **reports and
  carries on** rather than halting — losing a diagnostic description must never
  cost a run.

---

## 4. Loss is counted, attributed, and never repaired

### 4.1 The rings

`addSample()` costs a bounds check, an 8-byte store and two increments. No
allocation, no lock, no `snprintf`, no float, nothing that can block. The
critical section is taken only on the 1-in-100 batch handoff.

Overflow drops the **oldest** batch and counts it. Never silent, and never the
newest: when the radio is gone, the interesting evidence is what is happening
now.

### 4.2 Telling on-board loss from transport loss

Gate 14 caught a real defect here during development, and it is worth
recording because the first design looked correct.

The batch was originally sealed when it was *filled*, stamping the drop
counter at that moment. When the radio came back, every surviving batch in the
ring had been stamped **before** the drops that emptied it — so the counter did
not move across the very gap it existed to explain, and on-board loss was
indistinguishable from transport loss.

The seal now happens in `popBatch()`, at **send** time. `ringDropsLo` means
"how many batches the locomotive had thrown away as of the moment this
datagram left", which is the only reading that lets a receiver attribute a
sequence gap. Both the receiver and the decoder now say which kind of loss a
gap was, and neither has to guess.

The CRC therefore runs on `networkTask`, ten times a second. Never on
`hallTask`.

### 4.3 The receiver writes first

`xhr_receiver.py` writes every datagram to disk byte-for-byte, with its
arrival wall-clock time, **before** it parses anything. The console commentary
is computed afterwards and is allowed to be wrong about a damaged datagram
without the datagram being altered or dropped. It never reorders, never fills
a gap and never repairs.

A capture cut short by power loss reads cleanly up to its last whole frame;
the partial frame ends iteration quietly and is not an error and not repaired.

### 4.4 The decoder reports damage

`xhr_decode.py --report` names missing sequence numbers, duplicates,
reordering, corruption (bad magic, unknown version, truncation, CRC),
tick gaps with the sample they follow, and sessions. `--csv` writes a
`gap_before` column marking every discontinuity, so a plotting tool that
honours it cannot draw a straight line across evidence that was never
received.

---

## 5. Measured cost

### 5.1 Build

| | X18 `242109e` | this build | delta |
|---|---:|---:|---:|
| flash | 970,571 (74%) | 979,119 (74%) | **+8,548** (+0.9%) |
| RAM | 60,044 (18%) | 97,652 (29%) | **+37,608** |

`arduino-cli` 1.5.1, `esp32:esp32` 3.3.11, FQBN `esp32:esp32:esp32`, Otto's
profile active. **Zero warnings from our own code** under `--warnings all`.

The RAM is all ring, and it is all accounted for:

```
sample ring   40 batches x 844 B        33,760
current batch                              844
ruling ring   32 x 44 B                  1,408
networkTask's static send buffer           844
status/ruling scratch                      108
CRC nibble table                            64
                                        ------
                                        37,028   + WiFiUDP, IPAddress, mirrors
```

230 kB remains for local variables. The 40-batch ring buys **4.0 s** of
buffering across a radio dropout.

### 5.2 Timing — measured, not assumed

The brief says not to claim "diagnostic only" means timing is unchanged
without checking. It was checked.

* `addSample()`: **44.2 ns/call** over 20 million calls on this Mac, batch
  handoff included. That is a host figure on a different ISA and it does not
  transfer directly; as an order of magnitude it is well under 1 µs of the
  1000 µs tick, against `hallRead()`'s five ADC conversions which X18's own
  comment already puts at ~100 µs.
* `xhrSeal()` over 844 bytes: **3.92 µs/call** on this host, ten times a
  second, on `networkTask`.

**The authoritative number is `max_tick_body_us`**, which this build measures
on the locomotive itself, every tick, and reports once a second. §7 says what
must be seen there before a field run.

### 5.3 Wire and storage

844-byte datagram, 10/s, plus ~1 status/s and rulings at the marker rate:

| | |
|---|---|
| sustained | **~8.8 kB/s** including IP/UDP overhead |
| two circuits (~8 min) | **~4.2 MB** |
| one hour | **~32 MB** |
| a 90-minute session | **~48 MB** |

Comfortably inside the brief's 16 kB/s budget. The Pi has 100 GB free.

Batch size was chosen at 100 samples rather than the ~180 that would also fit
in one datagram: a lost datagram costs exactly its own span of trace, and
100 ms is already comparable to a 140 ms magnet passage. Bigger batches would
make each loss swallow a whole magnet.

### 5.4 The receiver and the Pi, measured on the Pi

An 8-minute synthetic stream at the locomotive's exact rate, sent from this Mac
across the railway network to `xhr_receiver.py` running on the Pi
(192.168.68.142, aarch64, Python 3.13.5, 100 GB free):

```
sent     5,280 datagrams, 3.91 MB in 480.0 s   worst send-schedule slip 0.0 ms
received 5,280 datagrams, 480,000 samples, 480 status
         488.0 s wall clock, 10.8 datagrams/s, 8.2 kB/s sustained
         0 unreadable datagrams
         0 transport gaps covering 0 batches
```

Decoded back on the Pi itself:

```
SAMPLES  4800 of 4800 datagrams; 0 missing, 0 duplicate, 0 reordered
STATUS    480 of 480 datagrams; 0 missing, 0 duplicate, 0 reordered
samples  480,000 received over 480.0 s — 1000.0 Hz of a nominal 1000 Hz
tick gaps 0
```

Not one sample lost in eight minutes, end to end. Decoding the whole capture
to CSV took **5.2 s** on the Pi and produced 37 MB; the capture itself was
4.0 MB, as predicted.

**What this does and does not establish.** It establishes the receiver, the
network path, the Pi's write capacity and the decoder, for longer than two
circuits. It establishes nothing about the ADC, the Hall task's timing, the
ESP32 or Otto's radio — the sender was a Python script on a Mac, not a
locomotive. §7.

---

## 6. Verification

### 6.1 Gates — 15 of 15 pass, 0 failures

X18's own thirteen gates are unchanged and all pass, so acquisition,
recognition, navigation, stations, cruise and the lap baseline behave exactly
as they did on `242109e`. The full suite reports **1,197 checks, 0 failures**;
101 of those are the two new gates.

**Gate 14** compiles the *shipped* `HallRecorder.h` and drives it as `hallTask`
does — 12,000 ticks with synthetic magnets, ring overflow, `micros()` wrap, dt
saturation, a ruling, a status, a reboot — and writes the datagrams into a real
capture file. 29 checks.

**Gate 15** reads that file with the real `xhr_format.py`. This is the only
check that the C++ and the Python agree about the wire: the firmware emits
bytes and the decoder consumes them, so a layout drift, a CRC-polynomial
disagreement or an off-by-one in the sequence arithmetic fails a test instead
of quietly corrupting a field capture. It then synthesises damage — dropped
datagrams, truncation, payload and header bit flips, bad magic, a foreign
`HWT1` record, an unknown version, garbage, reordering, duplicates, a
`micros()` wrap, a `millis()` wrap, a 200 ms stall, a reboot mid-capture, and a
half-written capture file — and asserts that **every one is reported**. 72
checks.

A decoder that quietly recovered from a corrupt datagram would fail these
tests. A capture that reads clean when it is not is worth less than no capture.

### 6.2 Two findings the gates caught before anything was built

1. **The wire sizes were wrong in my own header.** The `static_assert`s I wrote
   for `XhrRulingRec` (48) and `XhrStatusRec` (56) were arithmetic errors; the
   true sizes are 44 and 52. The build stopped rather than producing a decoder
   that silently misread every capture.
2. **The drop counter was sealed at the wrong moment** — §4.2. This one would
   not have shown up until a field run had already lost data and the analysis
   could not say where.

---

## 7. What is *not* verified, and what must be before a field run

This is the honest limit of what was done here. **No hardware was involved at
any point.** Otto was not flashed, no ESP32 was run, and the railway was not
moved.

Everything in §5.2 and §6 is either a host measurement or a host simulation.
In particular, **none** of the following is established:

* that the Hall task still holds 1 kHz on the actual hardware with the tap in
  it (`max_tick_body_us`, `max_gap_us` and `measured_hz` in STATUS will say —
  they exist for this);
* that the 4 KB Hall task stack is still sufficient. It should be — the tap adds
  almost nothing to it and the send buffers are `static` on `networkTask`
  precisely because X19's latent overflow was real — but `hall_stack_free_bytes`
  must be read on hardware and not assumed;
* that Otto's Wi-Fi link sustains 10 datagrams/s through the Grillers cutting,
  the station approaches and the far side of the loop. **This is the most
  likely thing to be wrong**, and the build is instrumented to prove it either
  way rather than to hide it: `cum_sample_ring_drops` counts what the radio
  could not take, and a batch gap with no matching drop is the network.
* that marker rulings align with sample timestamps on a real run. The
  mechanism (`sampleSeq` as the join key) is tested; the alignment is not,
  because that needs real magnets.

**Before a field run, a bench run should be done with the locomotive on
blocks**, receiver going, for at least the length of two circuits, and the
decoder's report read. What it must show:

| | must be |
|---|---|
| `measured_hz` | 1000.0, steady |
| `max_tick_body_us` | well under 1000; report the number |
| `max_gap_us` | ~1000, no saturated gaps |
| `hall_stack_free_bytes` | comfortably positive and not falling |
| `cum_sample_ring_drops` | 0 |
| `cum_udp_failures` | 0 |
| tick gaps in the report | none |

If ring drops are non-zero on the bench, the link will not carry a field run
and the ring should be enlarged or the batch rate reconsidered **before**
Otto moves, not after a run has already lost the evidence it was made to
collect.

One further limitation for the analysis itself, stated plainly: this trace
carries X18's operative `baseline_` as context, and X18's baseline is known to
be wrong in a way this project has already documented — the Hall zero moves
20–30 counts on a timescale of seconds to minutes
(`field-records/20260915_OTTO_HALL_ZERO_SHIFT.md`). That is **the reason to
record the raw trace**, not a defect in the recording. The analysis must work
from `raw`, and treat `baselineAtT0` only as "what the firmware happened to
believe at the time".

---

## 8. Running it

### 8.1 On the Pi, before Otto moves

```
python3 tools/xhr_receiver.py --outdir ~/NGR/hall_records
```

It prints the capture path, the port and the socket buffer size, then a
`session <id>, loco <n>` line the moment the first datagram lands. That line
is the confirmation that packets are arriving. Every 10 s it prints a running
count of datagrams, bytes, samples, rulings and losses; every second it prints
the locomotive's own STATUS line.

If nothing appears: the locomotive prints its destination at boot
(`[REC] continuous Hall record -> <host>:<port> session <id>`), and publishes
the same thing retained on `ngr/loco/9950011/diag/recorder`.

### 8.2 Stopping

`ctrl-c`. The file is flushed and `fsync`ed on the way out, and the summary
names the totals, the losses and the decode command.

### 8.3 Decoding

```
python3 tools/xhr_decode.py ~/NGR/hall_records/<file>.xhr --report
python3 tools/xhr_decode.py ~/NGR/hall_records/<file>.xhr --csv samples.csv --rulings rulings.csv
```

Read the report before the CSV. If it names missing sequence numbers, that
trace has holes and the report says how wide in milliseconds and whose fault
they were.

### 8.4 Proving the receiver before the locomotive is involved

```
# on the Pi
python3 tools/xhr_receiver.py --outdir /tmp/soak
# anywhere on the railway network
python3 tools/xhr_soak.py --host <pi> --seconds 480
```

`xhr_soak.py` sends a synthetic XHR1 stream at the locomotive's real rate. It
proves the receiver, the network path and the card. It proves nothing about
the ADC, the Hall task or the ESP32, and says so in its own `--help`.

---

## 9. Files

| | |
|---|---|
| `firmware/test-programs/NAVI_ONE_X18_RECORDER/` | the sketch, X18 as flown plus the tap |
| `…/HallRecorder.h` | the whole recorder: wire format, rings, batching |
| `…/tests/gate_recorder.cpp` | gate 14 |
| `tools/xhr_format.py` | the format, single source of truth |
| `tools/xhr_receiver.py` | the Pi receiver — write first, listen only |
| `tools/xhr_decode.py` | decode and audit |
| `tools/xhr_soak.py` | synthetic sender, for proving the receiver |
| `tools/tests/test_xhr_decoder.py` | gate 15 |
| `docs/NAVI_X18_RECORDER_VS_X18.diff` | the full diff against `242109e` |
