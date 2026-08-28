# MM152 — stable and weak, which is not the same as faulty

**Date:** 2026-08-28
**Status:** CLOSED — no excavation recommended
**Operator observation:** embedded in ballast, nominal; not checked for a second
magnet underneath.

## Question

MM152 was on the inspection list as a suspected stacked double magnet, on the
strength of a drop recorded as **123 -> 85**. Should the ballast be opened to
look for a magnet beneath it?

## Answer: no

Peak expressed as a percentage of each session's own median cruise peak, which
removes session gain drift:

| session | MM152 | MM151 | MM153 |
|---|---|---|---|
| 26 Aug | 75.4% | 89.5% | 92.7% |
| 27 Aug | 81.6% | 87.6% | 93.0% |
| 28 Aug CW (6 laps) | 75.8% | 87.1% | 92.6% |
| 28 Aug CCW (12 laps) | 78.7% | 86.6% | 94.5% |

**Four sessions, ±3 points around 78%.** Set against MM128, which over the same
window walked 186 -> 173 -> 168 -> 108 and inverted its polarity, MM152 is not
moving at all.

### The 123 was never a measurement of this magnet

`123` was the value in the shipped `strengthPct[]` table. The magnet did not
fall from 123 to 85; **the table was wrong and the magnet was always ~78%.**
The recorded "drop" was the distance between an expectation and reality, read as
a change over time. Rebuilt tables now record ~78% with a tolerance band derived
from MM152's own spread, and the discrepancy disappears.

## Two faults it does not have

**Not displaced.** The leg-asymmetry test that measured MM128's 66 mm shift
gives, for MM152: first leg 54.0% of the pair CW, 48.4% CCW — an implied shift
near 15 mm and inconsistent in sign between directions, which is grade and noise
rather than movement. MM128's signature was 66 mm and consistent.

**Not sitting deeper.** Duration is normal: 144 ms CW against neighbours at
138-152, 158 ms CCW against 150-162. A magnet further from the sensor spreads
its footprint and reads *longer*. MM152 reads normal-width and low-amplitude.

## What it is

Normal duration, reduced amplitude, no movement. Consistent with an opposing
magnet beneath — and equally consistent with a plainly weaker magnet. Peak and
duration cannot separate those two, and **it does not matter which it is.**
Either way the field is stable, and a per-marker ratiometric table expects
MM152 at its own value rather than at its neighbours'. Markers are not required
to match each other, only themselves.

## Standing check

MM152 needs no action while it stays within its band. What would justify
reopening it is **drift**, not weakness — the MM128 pattern of a value walking
session over session. The calibration tables surface exactly that, because a
marker leaving its own band is what NAVI refuses on.

Recorded because "weak" and "failing" were being treated as the same finding,
and the distinguishing evidence is a time series, not a single reading.
