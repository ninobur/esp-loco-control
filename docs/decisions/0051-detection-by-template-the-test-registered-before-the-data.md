# 0051 — Detection by template: the test, registered before the data

**Date:** 2026-08-28
**Status:** Proposed — the measurement is pre-registered, the result is not in
**Firmware:** `QUORUM_1_13X` (built, proven inert, NOT flashed)

## The operator's challenge

> "What we need is a no blinders look at the circuit through the eyes of the
> Hall sensor. We should be able to travel the circuit and see every magnet by
> matching to what a magnet looks like, verifying by pattern."

Three pieces, in order: **capture everything**, **detect by matching the
magnet's shape** rather than by thresholds, **confirm by the polarity pattern**.

The present detector does none of the middle step. It admits an excursion if the
amplitude crosses a threshold and the duration exceeds 40 ms — two scalars. That
is the one-dimensional reasoning the waveform exists to replace, and it is why
"does anything else look like a magnet?" could not be answered: the shape of
everything refused was discarded before it was recorded.

## What is already true

- **1.13X captures everything**: `rej=0` admitted, `rej=1` refused by the floor,
  `rej=2` sub-threshold, never opened an event at all. Proven inert against the
  replay suite; 22 host checks.
- **The magnet class is compact and its shape reads magnet type at 99.4%**
  ([the type map](../../field-records/20260828_MAGNET_TYPE_MAP_AND_SHAPE_VALIDATION.md)),
  so there is a real template to match against — two of them, one per type.

## The boundary, fixed now

From **1717 clean magnet passages** (admitted, unclipped, untruncated, before
the NO_QUORUM stretch, split reads removed), distance to the nearer of the two
type templates:

```
median  0.0200      99%  0.0788      99.9%  0.2379      max  0.2508
```

**Acceptance radius r = 0.2508.** It accepts 1717 of 1717 known magnets — 100%,
zero false rejects by construction.

This number is written down **before any non-magnet has been measured**, and it
is not to be adjusted after the data arrives. Twice today an in-sample figure
looked decisive and did not survive honest validation (the k=3 shape band at
0.0% that leave-one-out put at 7.6%; the `clip` ceiling that read as a real
peak). Pre-registration is the answer to that, not more care.

## The test

Flash 1.13X. Run the circuit. Measure every `rej=1` and `rej=2` capture against
the same two templates by the same metric.

- **Zero non-magnets inside r = 0.2508** — detection by template is sound. The
  magnets are clones and nothing else resembles them, so class membership does
  the work that discrimination could not, and the amplitude/duration thresholds
  can be replaced by one shape test.
- **Some inside** — then something on this railway genuinely looks like a
  magnet, and the count and identity of it is the most important thing we will
  have learned. It is also exactly what the current thresholds are silently
  relying on being rare.

Either result is worth the run. Only the first permits a firmware change.

## Not decided here

- Whether the template test would *replace* the entry threshold and the 40 ms
  floor or sit behind them. That depends on the result.
- Cost on the ESP32: 64-point resample and two distance computations per event,
  at event close, off the sampling path. Not measured yet.
- The pattern-verification step is unchanged and already works — the ten-magnet
  word, which is what makes a deterministic map self-checking
  ([0050](0050-the-map-is-deterministic-so-the-metric-is-detection-not-discrimination.md)).
