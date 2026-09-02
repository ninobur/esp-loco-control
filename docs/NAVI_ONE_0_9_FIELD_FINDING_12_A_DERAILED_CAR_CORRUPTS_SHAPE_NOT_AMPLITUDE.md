# NAVI_ONE 0.9 — Field finding 12

## A derailed power car corrupts the shape and leaves the amplitude alone — the first physical mechanism for the residual excursions

**Date:** 2026-09-01
**Locomotive:** Toby (9950012), `NAVI_ONE_0_9`
**Status:** Observed. Operator's physical evidence. Not a decision.

---

## The event

At 19:00:52 a magnet at MM113 was rejected `WRONG_SHAPE`:

```
peak 207   ratio 1.089   resid 0.1455   gap_ms 1775   gain 190
```

Amplitude entirely healthy, shape 12% over the 0.13 ceiling. The rejection cost
a marker; MM107–113 is a seven-long run of N so the lag stayed invisible for six
advances, and the first S magnet, MM106, struck.

## The cause was mechanical, and the operator found it

**"the power car was derailed so I adjusted it. that may have caused the
problem."**

He also confirmed the resting position independently: the Hall sat between MM106
and MM105, which is exactly where the one-marker lag predicts — the last counted
magnet was physical MM107 while the dashboard read 108, and the locomotive then
crossed MM106 and braked past it. Track measurement and log agree to the marker.

## The evidence separates cleanly

Residuals on this boot, before and after the car was put back on the rails:

| | residual |
|---|---:|
| the derailed pass, MM113 | **0.1455** |
| worst of the 42 passes since | 0.0863 |
| typical | 0.054 – 0.079 |

42 advances, zero disagreements, since the car was adjusted. The rejected pass
is not merely the worst; it sits alone, nearly double the worst of everything
that followed.

---

## Why this matters beyond tonight

Findings 02 and 03 have been open since 0.3: genuine magnets scoring 0.1422
(MM110) and 0.1811 (MM146) with peak, ratio, gap and gain all sitting inside the
range of the clean passes around them. Finding 04 put it plainly — every
dimension NAVI_ONE publishes was normal except the residual, and no scalar could
separate the possible causes.

**This is the first known physical mechanism that produces exactly that
signature.** A car riding at the wrong height or attitude changes the sensor's
geometry over the magnet — the arc becomes asymmetric or flat-topped — while the
peak field it passes through barely moves. Shape degrades, amplitude does not.

This does **not** establish that MM110 and MM146 were derailments; they were
different sessions and nobody looked at the running gear at the time. It
establishes that the class of fault exists and is mechanical, which is more than
findings 02/03 have had in nine days.

### The operational consequence

**A `WRONG_SHAPE` on a healthy amplitude is a reason to look at the running
gear, not only at the code.** Peak and ratio in range with the residual alone
excursive is now a recognised signature of a mechanical problem at the sensor.

---

## The recorder was too shallow to see it

The rejected passage had aged out of `WaveformWindow<6>` before the strike
dumped: six advances plus the strike is seven passages after the rejection, and
the window holds the strike plus five. We have the scalars and no curve.

Finding 04 predicted this precisely, naming the MM107–113 run of 7 and the
MM36–41 run of 6 as the places a lag could outlive the recorder. Tonight it did.

A seven-long polarity run needs a depth of at least 8; 10 would leave margin.
Cost is roughly 4.3 KB against 268 KB free. **Proposed, not taken.** With a
curve in hand this event would have shown whether a derailed car flattens the
top, skews the arc, or doubles the bump — which is the thing findings 02/03 have
never been able to ask.

---

## Carried forward

- Findings 02/03 remain open, but no longer without a candidate mechanism.
- Window depth remains 6, and remains one size too small for this track.
- 42 advances, zero disagreements since the car was adjusted.
