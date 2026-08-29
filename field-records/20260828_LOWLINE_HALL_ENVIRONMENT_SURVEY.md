# The Lowline Hall environment survey — runs 1 and 2

**Date:** 2026-08-28
**Locomotive:** Toby (9950012), `QUORUM_1_13X`
**Purpose:** find out what, other than a magnet, the Hall sensor ever sees
**Status:** runs 1 and 2 complete; run 3 (the circuit) pending

## Why the survey exists

Until 1.13X the sensor only ever reported what it *accepted*. Everything refused
left a tally mark and no shape, so the question "does anything else look like a
magnet?" had no data behind it — only amplitude and duration, the two scalars
the refusal was made on. 1.13X records every excursion: `rej=0` admitted,
`rej=1` refused by the 40 ms floor, `rej=2` sub-threshold, never an event at
all. The watch aperture sits near 15 counts, about three times the sensor's own
noise of 4.1 counts RMS.

## Run 1 — stationary, motor off, 90 s

```
captures       0
floor_rejects  0
baseline    1828, raw 1826-1827
```

**Nothing.** Not one disturbance reached 15 counts in ninety seconds.

## Run 2 — stationary, motor turning at PWM 90, 100 s

```
captures       0
floor_rejects  0
baseline    1826, raw 1826, pwm 90 confirmed
```

**Also nothing.** The motor, its H-bridge and its commutation put no measurable
disturbance into the Hall sensor. This was the run most likely to produce
electrical noise, and it produced none.

## The positive control — four hand passes

Two silent runs prove nothing if the capture path is dead, so a magnet was
passed by the sensor by hand. Every pass registered, and registered in three
parts:

```
rej=2  peak  33  dur 118 ms     the approaching fringe
rej=0  peak 498  dur 441 ms     the magnet itself
rej=2  peak  18  dur  46 ms     the departing fringe
```

The instrument is awake, and the sub-threshold watch is seeing field the old
firmware was blind to. **The silence in runs 1 and 2 is therefore a measurement,
not a failure.**

## What the control taught, beyond working

Applying the four tests to the hand passes:

| | jitter (limit 12.04) | Gaussian fit (limit 0.1461) |
|---|---|---|
| all passes | 3.3 – 8.7 — **pass** | 0.106 – 0.304 — **several fail** |

The fit failures are an **artefact of truncation**. A hand pass takes 441–733 ms
against a 240-sample window, so the stored curve is a cropped centre section,
and a cropped arch looks flat-topped — which fits a Gaussian badly however
magnetic it is.

**So of the four tests, jitter survives a truncated capture and shape does not.**
Truncated records (`tr=1`) must be excluded from any shape test. They may still
be judged on jitter, amplitude and duration. This was already being done by
accident; now it is being done for a reason.

## Interrupted by low voltage, not by fault

Telemetry went stale repeatedly near the end. Cause: `state/lowvolt 1`, with the
pack down to **15.27 V** from 16.31 V that morning, after the motor ran at PWM 90
on the bench with no load. The ESP32's radio is the first casualty of a soft
supply — uptime showed 768 s with no reboot, so the locomotive never faltered;
only its link did. Motor off, battery disconnected for charging.

Worth remembering: **a bench run at speed drains the pack as fast as a lap does
and buys less.**

## Still to run

**Run 3, the circuit at PWM 90.** Runs 1 and 2 establish that neither the sensor
at rest nor the motor produces anything. Whatever appears in run 3 is therefore
**the track** — rail joints, points, fixings, ballast — with no confounding
source left to argue about.

The test is pre-registered in
[0051](../docs/decisions/0051-detection-by-template-the-test-registered-before-the-data.md).
