# QUORUM 1.13W — waveform capture, instrumentation only

**NOT FLASHED.** Built and verified on the host; awaiting operator approval.

## What it is

QUORUM 1.13 — the sketch Toby ran on 2026-08-28 for the calibration laps — plus
capture of the raw Hall excursion for every admitted marker. Nothing else.

Published on `ngr/loco/<id>/mm/wave`:

```json
{"t":69,"mm":51,"pol":"N","pk":200,"dur":137,"pwm":90,
 "n":162,"sc":3,"pre":12,"tr":0,"clip":0,"drop":0,"d":"<base64 int8>"}
```

`d` is `n` samples of `raw - baseline`, divided by `sc`, offset by 128, base64.
`clip` counts samples that hit the int8 limit -- **it must be 0**; any
other value means the curve saturated and its shape is not trustworthy.
`pre` samples precede the excursion opening. `mm` is the marker the navigator
accepted; `null` if none. `drop` is the cumulative count of captures lost to a
full queue — lossy by design, and visible rather than assumed.

## Why

Peak and duration cannot identify a magnet. Measured on the 2026-08-28 tables,
a typical marker has **~60 others it cannot be told apart from** on polarity +
strength + duration, and only **1 of 171** is uniquely identified. If a magnet
is to be positively identified rather than merely *verified against an
expectation*, the discrimination must come from the shape of the excursion —
and the shape has never been recorded. The detector reduces every passage to
two numbers and discards the curve.

A cautionary datum: the 2026-06-01 Otto survey logged at ~40 ms intervals and
caught a magnet in **three points**. A survey at that rate contains no shape at
all, and "shape does not discriminate" would have been concluded from data that
never held any. This records every sample the detector takes — 162 samples for
a 137 ms passage in test.

## Why 1.13 and not NAVI

NAVI 1.6 already carries this capture, but it also ramps to a stop on any
refusal. Toby on 1.13 measured **0.5% disagreement over 733 crossings**, the
best-evidenced result on the railway; NAVI has none. Decision
[0047](../../../docs/decisions/0047-a-sketch-is-chosen-on-crossings-measured-not-on-features-listed.md)
exists to stop exactly that trade. This build keeps the measured behaviour and
adds one publish.

## Proof it is instrumentation

- **One existing line differs** from stock 1.13: `SKETCH_NAME`. Everything else
  is 130 added lines. Verify with `diff` against the tree at `f1e6978`.
- **The replay suite output is byte-identical** to stock 1.13's — the full
  2026-08-10 session, three incident goldens, evidence properties, synthetic
  cases and the counterfactual. Run `tests/run_suite.py` on both and `diff`.
- The capture hangs off the event close **after** the MarkerEvent is already
  queued, and the wave queue is separate from both the event and marker-publish
  queues. A survey sample can never evict a marker.

## Cost

| | stock 1.13 | 1.13W | delta |
|---|---|---|---|
| flash | 983171 (75%) | 984183 (75%) | +1012 B |
| RAM | 52468 (16%) | 54540 (16%) | +2072 B |

## tests/wavetest.cpp

The replay harness injects `MarkerEvent`s directly and **never calls
`detectorSample()`**, so nothing in the existing suite exercises this path. The
same blind spot hid a bug in NAVI's T16 until the real detector was driven.
`wavetest.cpp` drives the actual detector through the ADC with a synthetic
half-sine and checks the captured curve, then decodes what was published.

```
clang++ -std=c++17 -Wno-format -I tests/shim -I . -o /tmp/wavetest tests/wavetest.cpp
/tmp/wavetest
```

11 checks, 0 failures. Requires the `g_hostAnalog` hook added to
`tests/shim/Arduino.h` (stock shim returns a constant 0 from `analogRead`).

## Before flashing

`LocoConfig.h` is a live per-flash selector. **Read the active `#include`, not
the header comment** — on 2026-08-12 they disagreed and nearly put Otto's
identity onto Toby. Currently `LL_LocoConfig_9950012.h` (Toby).
