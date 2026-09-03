# 0073 — One reading is the median of five conversions

**Date:** 2026-09-03
**Status:** PROPOSED. Not authoritative until the operator reviews and approves it.
**Follows:** 0065 (one reading cannot carry a value), 0071 (two readings cannot carry a value)
**Build:** NAVI_ONE 1.0X11 "Epiphany" (b7a1ff8)

## The decision

`hallRead()` takes five consecutive ADC conversions of GPIO33 and returns the
middle one. Everything downstream — the capture, the recording, the judgement
— sees one sample per millisecond exactly as before. The five reads cost
about 100 µs of the 1 ms tick.

## What forced it

The operator's finding that the transients are "all the magnets" — none
with the electronics on and the motor off, none on blocks at 90 PWM — and
the survey build's clean 187 crossings on an eight-read average, against
NAVI_ONE's single read. A read that returns a wrong value is invisible while
the true signal is idle; it shows only where the signal is not, which is
where the magnets are.

## What was measured

The bench test of 2026-09-02/03 (`docs/NAVI_ONE_1_0X11_EPIPHANY_BENCH_TEST_20260903.md`):
a marker magnet held still under the sensor, locomotive still.

| build | flat-top samples | bad reads (> 20 counts, one sample) |
|---|---|---|
| X9, one conversion | 3731 | 4, plus 8 in 4593 samples of hand passes |
| X11, median of five | 5113 | 0 |

Zero against an expectation of five to seven: under one chance in two
hundred. The bad reads need a field on the sensor, are not caused by
motion, the motor, the mount or the IR channel, and are not stale or
settling conversions (they go both ways, mid-hold). What remains is one
wrong conversion or a sub-100 µs glitch on the sensor output; the median
contains either.

## The unintended consequence, stated now

**The fault is now invisible in the record.** A stored sample is a median;
the wild conversion is thrown away before anything sees it. The curve
database (0072) will record clean samples and the rate of the underlying
fault will no longer be measurable from the railway. If that rate matters —
because the sensor is degrading, say, or because a second locomotive has a
worse one — it has to be counted where it is discarded. An X12 tally of
five-read sets whose spread exceeds 20 counts, by position and sign, in the
status alert, is the proposed instrument. Registered here; not built.

**It does not reach a fault longer than 100 µs.** The MM157 burst that
stopped X9 (0071) spanned several stored samples at 1 ms. That is a different
fault of a different duration, and 0071's judgement median is what handles
it. Both stand, for different reasons, and tonight's evidence is for this
one only.

## What it does not change

- The recording is still what the sensor produced, one value per
  millisecond, never edited after the fact.
- The ceiling (0.13), the floor (0.34), the guard (200 ms).
- 0071. The judgement copy is still a five-wide median of stored samples.
