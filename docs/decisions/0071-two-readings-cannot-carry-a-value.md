# 0071 — Two readings cannot carry a value

**Date:** 2026-09-02
**Status:** PROPOSED. Not authoritative until the operator reviews and approves it.
**Amends:** 0065 (one reading cannot carry a value)
**Build:** NAVI_ONE 1.0X10

## The decision

The judgement copy of a passage — the median-filtered copy the peak and the
shape are read from, while the recording itself is never altered — is a
**median of five**, not three. A lone bad reading is outvoted by the two
beside it; an adjacent pair by the three around them.

## What forced it

Bamboo CCW, 2026-09-02 18:55:54, build X9. An ordinary crossing of MM157 on
the station's zero ramp: 356 samples, a clean rise to 192 against a gain of
221, a broad top, and on the falling flank a burst at samples 272–280:

```
105  -2  -13  108  109  174  68  48
```

Adjacent pairs. A three-wide median removes lone samples and leaves pairs in.
The one-Gaussian fit scored 0.1506, the recognizer refused a good magnet, and
because this was a stop episode the train stopped. The operator: *"Nothing
physically wrong. It is a mental problem."*

Both refusal chunks arrived and `pub_drop` was 0; the record was neither
truncated nor mis-reassembled. The firmware judged the whole record. The
trigger was the burst.

## What was measured

| the MM157 record | residual |
|---|---|
| three-wide median (the field) | **0.1506 — refused** |
| five-wide median | **0.0816** |
| seven-wide median | 0.0817 |
| burst interpolated away, three-wide | 0.0825 |

The five-wide median recovers exactly what the burst cost and nothing else.

Across the **312 real accepted passages** in the records: residual p50 0.0709
under either width, largest change 0.0009, none over the ceiling either way,
peak moved by at most 4 counts (p95 3). Every waveform-replay gate passes with
it — the 2,799 survey records (gate 1), the 2026-08-29 lap (3), both polarity
replays (6, 7), the interrupted-traversal gate (12) and the archaeology (13).
Gate 6 part A asserted the peak *to the count* against a three-wide build and
now allows the four.

## The unintended consequence, stated now

The copy is built on the **stored** record, which may be decimated. Five
samples at decimation 1 is 5 ms and smooths nothing real; at decimation 128
it is 640 ms, and a real feature shorter than that would be flattened. Nothing
this recognizer accepts is that short — the eight-sample departure stub of
the Bamboo 12:29 record was unjudgeable for other reasons — but a future
change that judges very short fragments at heavy decimation will have to
remember this. Registered in the header, not fixed.

## What it does not change

- The recording. `oriented` is what the sensor produced, published and stored
  spikes and all. Only the copy that is judged is filtered.
- The ceiling. 0.13 stands.
- The stop-episode rule (X5). A refusal around a stop still stops the train;
  what changed is that a burst of bad readings no longer manufactures one.
