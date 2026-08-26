# Pre-registered prediction — afternoon right-side lap

**Written 2026-08-26 14:05 PDT, before the run and before any afternoon data
exists.** The point of this record is that it can be wrong. The morning
result (`2026-08-26_IR_ILLUMINATION_AND_MOUNT.md`) fitted a window that was
already known; this one commits in advance.

## Loop geometry, from the landmark table

`QUORUM.ino:591–593` places the cardinal landmarks, so mile marker maps to
compass bearing:

| MM | Landmark | Side of loop |
|---:|---|---|
| 0 | Southpoint | S |
| 72 | Westpoint | W |
| 98 | Northpoint | N |
| 140 | Eastpoint | E |

Mile marker increases **clockwise** on the map (S → W → N → E). Spacing is not
uniform in angle, so bearings between landmarks are interpolated loosely.

**A right-side mount running CCW faces outward** (CCW travel puts the loop
interior on the left). So the sensor's look direction at any point is the
outward normal — south at MM 0, west at MM 72, north at MM 98, east at MM 140.

## The model

Contrast is highest where the sensor looks **away** from the sun, so the sun is
behind the sensor and front-lights the spoke faces it sees. Contrast is lowest
where the sensor looks **into** the sun and the spokes are backlit.

So the bright band should sit where the outward normal ≈ the **anti-solar
azimuth**, and it should track the sun through the day.

## What the morning data showed

Laps 1 (11:03) and 4 (11:43), both CCW right-side. Sun then was roughly SE,
azimuth ≈ 120–135°, so anti-solar ≈ 300–315° (NW).

- Peak: **MM 90–109** (2007 and 2175 in lap 1; 1807 and 2079 in lap 4)
- Trough: **MM 120–129** (439 in lap 1, 415 in lap 4) and MM 30–49
- MM 0–9, facing due south into the sun: 319 / 543 — low, as the model wants

Peak observed at ~MM100 (N) against ~MM85 (NW) predicted. Close, given crude
interpolation on a non-circular loop, but not exact.

## The prediction

It is now 14:05 PDT, roughly one hour after solar noon. Sun is SW, azimuth
≈ 215–225°, so **anti-solar ≈ 35–45° (NE)**. NE on this loop is between
Northpoint (98) and Eastpoint (140), call it **MM 112–128**.

**Primary prediction — a swap:**

1. **MM 120–129 becomes bright.** This morning it was among the *lowest*
   bands, 415–439. The model says it should now be at or near the peak.
2. **MM 90–109 falls.** This morning's peak, 1807–2175. It should drop
   substantially.

That is a directional, falsifiable swap between two specific bands, not a
vague "it should shift". Both morning laps agree on both bands, so the
baseline is not a one-off.

**Secondary:** MM 0–9 (facing S, now closer to the sun's azimuth) should stay
low or drop further.

## What would falsify it

- The peak stays at MM 90–109 → illumination geometry is not what drives
  contrast; something fixed to the track or the mount is.
- Contrast goes flat everywhere → the morning profile was a speed or
  mount-vibration artefact, not illumination.
- The peak moves somewhere unrelated to NE → the model is wrong even if
  illumination matters.

## Known weakness, stated in advance

The morning profile had a **second bright band at MM 150–169** (1967 lap 1,
1511 lap 4). That region faces E/SE outward — *toward* the morning sun, where
the model predicts backlighting and low contrast. It was bright anyway.

Either that band is direct sun striking the sensor rather than reflected off
the spoke faces, or the model is incomplete. Recording it now so the afternoon
result is not read selectively. If MM 150–169 stays bright while the primary
swap also happens, the model is right about the main mechanism but missing a
second one.

## Run protocol (match lap 4 so the comparison is clean)

- Direction **CCW**, sensor on the **right**, outward-facing — unchanged from
  lap 4's final configuration.
- **120 PWM**, straight through, no station stops.
- One full circuit, start and finish at the same marker.
- Record sun conditions and any cloud at the time of the run.

Speed matters for absolute distance error but not for the spatial contrast
profile — laps 1 (slow) and 4 (fast) produced the same profile at ~2× speed
difference, which is what licenses comparing this run against both.
