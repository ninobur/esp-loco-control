# Recovery observation: declaring a position does not re-establish the sensor

**Date:** 2026-09-02
**Locomotive:** Toby (9950012), build `NAVI_ONE_1_0X4_FIELDTEST`
**Status:** OBSERVATION. No change made, and none is warranted yet.

---

## What was seen

After the Bamboo CCW shutdown, Toby was found at rest with the Hall sensor
**directly over MM154**. The operator declared that position and restarted.

- The restart **failed twice in a row** from the same declaration.
- A **power cycle cleared it**, and the same physical position then ran
  correctly.

The operator's own reading, which the evidence supports: *"I thought I might
have gotten the position wrong, but not twice in a row."* Two identical
declarations failing and a power cycle fixing it does not describe a wrong
position. It describes state that survives a declaration and does not survive a
power cycle.

## What the code says

A declaration resets the navigation frame and any open passage. `HallCapture`'s
`reset()` clears the recording, the pause state and the progression ring, and
re-seeds `entryBaseline_` from `baseline_`.

It does **not** clear the learned reference itself: `med_`, `medLen_`, `medHead_`
and `primed_` all survive. That is deliberate — the baseline is a property of
the sensor, not of the route frame — and it is why a declaration is not a
recovery from every capture fault.

A power cycle re-primes from scratch (`primeMs` 2000).

## The mechanism this points at, unproven

Toby was parked **on top of MM154**. The baseline may only adapt with positive
evidence of tractive motion (`mayAdapt = actualPwm > 25`), and it is additionally
frozen while a passage is paused. Neither guard covers the case here: on each
restart attempt the throttle ramps **past 25 while the locomotive has not yet
moved**, and if no passage is open at that moment there is nothing to stop the
rolling median walking onto MM154's own field. The reference becomes the magnet
— findings 09 and 10 by a third road.

This is a hypothesis consistent with the symptom and with the code. It has not
been reproduced, and no telemetry was examined for the two failed starts.

## The limitation it exposes

> "Declare position" is not presently a complete recovery from every Hall-capture
> fault.

The eventual design should say explicitly which sensor state survives a
declaration, and offer a way to re-establish the reference without a full
locomotive power cycle — a "re-prime the sensor" that is separate from
"declare where I am".

## Why nothing was changed

Two reasons. The mechanism is unproven, and a change to when the reference may
be re-learned is exactly the kind of change that has already cost this build
once today: the reach-back fix of finding 14 was written, measured and withdrawn
because it let an artifact advance a marker. Reference handling is where findings
08, 09 and 10 live, and it is not a place to act on a hypothesis.

What would settle it: the telemetry for the two failed starts, and a bench
reproduction — park the sensor over a marker, declare, and ramp the throttle
through 25 with the baseline published.
