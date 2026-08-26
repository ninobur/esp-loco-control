# Prediction result — the specific band was wrong, the model survives, the mount finding replicates

**Date:** 2026-08-26, run at 14:09–14:17 PDT
**Prediction:** `2026-08-26_IR_ILLUMINATION_PREDICTION.md`, committed `b337605`
at 14:05 PDT — before the run, before any afternoon data existed.
**Conditions:** sunny, **104 °F**, sun SW (roughly one hour past solar noon).
**Runs:** two CCW circuits at constant **PWM 90**, car reversed at MM 036.

| Lap | Mount | Window | Toby nav |
|---|---|---|---|
| 5 | RIGHT / outward | 14:09:17 → 14:13:06 | NORMAL throughout |
| 6 | LEFT / inward | 14:13:19 → 14:16:56 | NORMAL throughout |

Operator illumination survey was recorded in-band at 14:12:49, before analysis
(`phases.tsv` `ILLUMINATION_MAP`).

## Scorecard

| # | Prediction | Result | Verdict |
|---|---|---|---|
| 1 | MM 120–129 becomes bright — "at or near the peak" (was 439) | 439 → **863** | **Partial.** Direction right; roughly doubled. But 863 is barely above the lap median (767) and nowhere near the peak (2671). Magnitude wrong. |
| 2 | MM 90–109 falls (was 2007 / 2175) | → **751 / 1583** | **Confirmed.** |
| 3 | Peak lands at MM 112–128 | Peak at **MM 20–29** (2671), broad bright band **MM 130–159** (1983–2351) | **Wrong.** |

**The headline claim — that the peak would move to MM 112–128 — is wrong.** It
moved, but not to where I said.

## What the prediction got right, and it is the part that matters

The pre-registered falsifiers were: peak stays at MM 90–109, or contrast goes
flat. Neither happened. The profile **rearranged completely**:

> Right-mount profile, morning vs afternoon: **r = −0.20** across 18 bands.

A three-hour sun swing scrambled the spatial contrast profile of the same
mount on the same track at the same speed. Contrast is **not** fixed to track
features, mount vibration, or curve geometry — it follows the sun. That was
the model's core claim and it is confirmed, independent of my failure to
predict the band.

The morning peak sat near MM 100 and the afternoon bright band near MM 140:
in bearing terms roughly N → E, about +90°, against a sun azimuth change of
about +95° (SE → SW). **The magnitude of the shift matches the sun's
movement.** My absolute placement carried a constant offset of roughly 50°,
which is unsurprising — bearings here are interpolated linearly between four
cardinal landmarks on a loop that is demonstrably not circular, and the mount's
look angle relative to true radial was never measured. Both are fixable with a
track survey and a protractor; neither is a defect in the model.

## The pre-registered weakness turned out to be a real second mechanism

The prediction document flagged, in advance, that the morning profile had a
bright band at **MM 150–169** facing *toward* the morning sun, where the model
predicts backlighting and low contrast — and said that if it persisted, the
model was "missing a second mechanism."

It did not persist. It moved — to **MM 20–29** in the afternoon (2671, the
single brightest band of the lap). MM 20–29 faces roughly SSW/SW outward,
which is approximately **straight into the afternoon sun**, exactly as
MM 150–169 faced into the morning sun.

So there are two illumination mechanisms, both tracking the sun:

1. **Anti-solar front-lighting** — sun behind the sensor, illuminating the
   spoke faces it looks at. Genuine spoke contrast.
2. **Direct solar loading** — sensor looking into the sun. Also produces large
   envelope span, but there is no reason yet to believe that span represents
   spoke modulation rather than glare chopped by passing spokes.

**Mechanism 2 is a hazard, not a benefit.** A large `runMax − runMin` raises
`thrHigh`/`thrLow` and satisfies the contrast gate, so the firmware will read
"good contrast" while looking directly at the sun. That is precisely the
false-confidence path the 2026-08-25 stationary test demonstrated with a work
light. This record does not establish that mechanism 2 causes miscounting —
only that the existing contrast gate cannot distinguish it from mechanism 1.

## The mount finding replicates, and is now the robust result

| Run | Mount | Median span | Bands below 700 | AM↔PM profile correlation |
|---|---|---:|---:|---:|
| Lap 1 (AM) | RIGHT | 503 | **56%** | r = −0.20 |
| Lap 5 (PM) | RIGHT | 555 | **56%** | |
| Lap 3 (AM) | LEFT | 1827 | **11%** | r = +0.18 |
| Lap 6 (PM) | LEFT | 1895 | **11%** | |

Across a three-hour sun swing that scrambled *where* the contrast is, the
summary statistics barely moved. The right mount spends **56% of the loop
below 700 counts** in both morning and afternoon; the left mount, **11%** in
both. Peak-to-median ratio: right 6.9× (AM) and 4.4× (PM), left 1.8× (AM) and
1.5× (PM).

> **Where contrast falls is set by the sun. How much contrast there is, is set
> by the mount.** These are separable, and only the second is under
> engineering control.

This is the second independent confirmation of the mount difference, now at
two sun angles. It is a stronger result than the illumination model and it is
the one with an action attached.

## What this does and does not license

- **Does not** license a threshold change. No single falling fraction serves a
  mount that spends over half its loop below 700 counts and peaks above 2600.
- **Does not** establish that direct solar loading causes false pulses — only
  that the contrast gate cannot tell it from real signal. Testing that needs
  a stationary run pointed into the sun with waveform capture, the experiment
  `IR_STATIONARY_TRUTH_TEST_2026-08-25.md` already called for.
- **Does** establish that the right-side mount is not fit for purpose as
  installed, at any sun angle, and that the left-side mount is close to
  illumination-independent.
- **Does** establish that the illumination model is directionally right and
  that its absolute predictions need a real track survey before being trusted.

## Next

1. **Measure the two mounts.** Standoff, angle to the spoke face, what sits
   behind the wheel on each side. A 2.5–4× intrinsic contrast difference
   between two nominally equivalent installations remains the largest and
   least explained effect in this whole series, and it is almost certainly
   measurable with a ruler.
2. **Survey the track bearings** if the illumination model is to be used
   predictively rather than descriptively. Four cardinal landmarks are not
   enough to interpolate a non-circular loop.
3. **Test mechanism 2 directly** — stationary, sensor into the sun, waveform
   captured — before any contrast-gate change.

## Process note

The IR capture for lap 5 was interrupted at 14:13:02, four seconds before the
lap ended, because a second reader was opened on `/dev/ttyUSB0` while the
operator's own recorder was running (`multiple access on port`). The operator's
capture was the better-instrumented of the two — it carried ARMED,
ENVIRONMENT, ILLUMINATION_MAP and LAP_BOUNDARY markers in `phases.tsv`, the
in-band marking this firmware otherwise lacks. Lap 5 lost only its final four
seconds and lap 6 was captured intact by the second recorder, so no analysis
was affected. Check for an existing reader before opening the port.
