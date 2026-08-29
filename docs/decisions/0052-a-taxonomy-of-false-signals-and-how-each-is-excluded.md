# 0052 — A taxonomy of false signals, and how each is excluded

**Date:** 2026-08-28
**Status:** Proposed — categories are measured, exclusions partly implemented
**Evidence:** `QUORUM_1_13X` full-capture survey, 2026-08-28 — every excursion the
sensor met on a full circuit, not only the ones it accepted

## Why a taxonomy

Until today every false signal was treated as one problem, "a phantom," and met
with one test: the conservation arithmetic. That test rejected 42 crossings and
**29 of them were real magnets**. One rule cannot serve five different things.

The survey separates them. Each has a distinct signature, and each has an
exclusion that is cheaper and more certain than the one now in use.

---

## 1. The rebound — a magnet's own departing tail

**~150 per lap.** By far the most numerous.

| signature | measured |
|---|---|
| arrives | 14–64 ms after the magnet's event closes |
| polarity | **opposite to its magnet, 149 of 150** |
| amplitude | 16–34 counts (sub-threshold), or 38–52 (floor-refused) |
| shape | smooth, often magnet-like — **30% pass every morphology test** |

It is the dipole reversing as the sensor clears the magnet. It looks like a
magnet because it *is* one, seen on the way out.

**Excluded by amplitude, and only by amplitude.** None of the 149 reaches 140
counts, the weakest real magnet on the lap. Morphology does not exclude it:
jitter passes 100%, the Gaussian fit passes 82%, the template passes 33%.

---

## 2. The re-read — a rebound strong enough to become an event

**~2 per lap.** 21 in twelve laps.

| signature | measured |
|---|---|
| arrives | 139–347 ms after the previous accepted magnet |
| polarity | **opposite, 13 of 13** |
| amplitude | 38–44 counts, against 140 for the weakest real magnet |
| duration | 40–103 ms, against 131 ms for the shortest real magnet |

**Three independent tells**, any one decisive: too soon, too weak, wrong pole.

**Currently excluded by the conservation test** — which works, but by the wrong
reasoning and with heavy collateral damage (category 3).

**Should be excluded by a refractory period**: ignore everything for 250 ms
after accepting a magnet. On today's data that removes 13 of the 15 sub-250 ms
intervals — and the other two are a 243 ms outlier and one following a 5283 ms
stall. It uses **no velocity model**, so it cannot drift.
Margin at the fastest running ever recorded (PWM 214): 158 ms.

---

## 3. The model mismatch — NOT a false signal at all

**29 today.** These are real magnets, wrongly refused.

| signature | measured |
|---|---|
| interval | 1116–2410 ms — a full marker gap |
| polarity | mixed, 15 same / 16 opposite |
| amplitude | normal |
| ratio | 1.22–1.30, hugging the 1.30 tolerance edge |

They fail because `expected` is computed as
`spacingMm / (3.90 × pwmActual − 99.2)` — **a request treated as a measurement**,
which is [0024](0024-a-request-counter-is-not-a-measurement.md) exactly. At
cruise the model is right and the test works. When the locomotive slows, the
model does not know, `expected` stays fixed, and real markers start failing.

Four of them, ratios walking 1.23 → 1.25 → 1.28 → 1.30, immediately preceded the
14:38 missed magnets and the 14:39 NO_QUORUM. The odometer was told four times
that the train had not moved when it had.

**Fix:** make the conservation rejection conjunctive — it may only fire when
`dt` is also short. Duplicates sit at 139–347 ms; mismatches at 1116–2410 ms.
A 4.7× gap with nothing in it. On today's data this returns all 29.

---

## 4. Boot and handling artefacts

**3 today**, at device time 3.5, 5.3 and 8.5 s, with no magnet for two minutes
either side. Weak (41–52 counts), brief (21–22 ms), all one pole. Consistent
with the locomotive being placed on the track by hand.

**Already excluded categorically**, by design rather than luck: `navOnMarker`
returns at the `NO_POSITION` gate before `acceptEvent` is reachable. Until a
position is declared, nothing the sensor sees can move the odometer.

### The related risk that is NOT excluded

`calibrate()` averages for two seconds and primes the whole median ring from it.
If those two seconds happen with the sensor beside a magnet, the baseline is
biased and every threshold with it.

It is self-healing — `updateBaseline` refills the ring from live samples, so it
corrects within about 64 s of travel — but the first minute runs on false
thresholds.

**Detect and announce; never refuse.** Boot baselines across every session fall
in **1795–1903** (today: 1826, 1828, 1835, 1836). One comparison at boot catches
a contaminated calibration. On failure: publish a loud warning and **recalibrate
once the locomotive is moving**, when the sensor is known to be between magnets.
Do not refuse to run — a locomotive that will not move is a worse failure than
one that corrects itself. Do not simply reboot either: parked over a magnet, a
reboot recalibrates in the same place and gets the same answer.

---

## 5. The environment — empty

**Zero observed.** Over a full circuit, every one of 153 excursions belonged to a
magnet. No rail joint, no fixing, no point, no fastener, no motor transient. The
motor at PWM 90 with the locomotive stationary produced nothing at all in 100 s;
the sensor at rest produced nothing in 90 s.

Nothing to exclude, because nothing is there.
[The survey record](../../field-records/20260828_LOWLINE_HALL_ENVIRONMENT_SURVEY.md)
carries the caveat: one lap, one direction, one dry day.

---

## What this changes

**Amplitude is the load-bearing test**, not morphology. Every category that
actually threatens the odometer is excluded by being too weak, too soon, or the
wrong pole — never by being the wrong shape. 30% of rebounds pass the full
morphology suite. The day's shape work identified magnet *type* at 99.4% and
caught MM128 by its variance; as a detector it would have been a downgrade.

**Two changes are worth making**, and both remove a model rather than adding one:

1. a **250 ms refractory period** after an accepted magnet, replacing the
   conservation test's role for category 2;
2. the conservation rejection becomes **conjunctive on short `dt`**, so it can no
   longer refuse real magnets (category 3).

Neither needs `spacingMm[]`, a velocity model, a template, or a stored curve.
