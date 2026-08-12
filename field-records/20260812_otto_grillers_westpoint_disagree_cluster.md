# Otto — mm 65 is the return-flux lobe of the magnet at mm 64

Status: **Cause identified. The phantom is systematic, on every pass, and the
2026-08-12 magnet replacement did not remove it.**

`mm 65` is not a magnet. It is the trailing fringe of the magnet at `mm 64`,
detected on departure at low speed. It fires on all 12 passes recorded, always
at the opposite polarity to 64, and the navigation map has absorbed it as though
it were real.

The four DISAGREE reads at 15:18 were a *second* insertion on top of it: the
fringe lobe fragmented into two threshold crossings instead of one, adding an
extra marker and shifting the phase.

The operator reached both conclusions from the layout and the disagreement
pattern, ahead of the data:

> *"That suggests that a 'correct' phantom, agreeing with expected, was inserted
> before this group."*

> *"MM63. Command stop. MM64 while slowing Sensed correctly. Miliseconds later,
> The edge of MM64 field which is opposite polarity of 64. and same polarity of
> 65 fools the sensor."*

An earlier draft of this record attributed the cluster to a speed spike, then to
a split read of a real magnet at 65, and tested the hypothesis by field
*strength*. All three were wrong, and the third was wrong in an instructive way —
see "Why the strength test misled" below.

## The decisive evidence: polarity

Real markers alternate N/S. Across **all 12 passes** the sequence through this
stretch is:

```
mm 63 = N     mm 64 = S     mm 65 = N     mm 66 = N     mm 67 = S
                                    ^^^^^^^^^^^^^^^
                             two consecutive N — impossible
```

Two same-polarity reads in succession cannot both be magnets. Remove 65 and the
sequence is clean:

```
mm 63 = N     mm 64 = S     mm 66 = N     mm 67 = S
```

`mm 64` is **S**; its phantom at 65 is **N** — the opposite polarity, on every
pass without exception. That is the return-flux signature: the normal component
of a magnet's field reverses sign in the fringe beyond the pole face.

### Why it appears on departure, and why the dwell is long

While parked, `delta ≈ 0` (raw within ±13 of a baseline that moves 2 counts all
session, against a ±38 entry threshold). The sensor is sitting in the **null**
between the centre lobe and the trailing fringe — clear of both. Departing walks
it into the trailing lobe.

That lobe is traversed at the slowest speed of the lap, which is why `mm 65` has
the longest dwell of any marker in the stretch — 416–712 ms, against 130–490 ms
for real markers.

### Baseline poisoning is ruled out

| stop | baseline range | delta range |
|---|---|---|
| 15:05 (old magnet) | 1903..1903 | −4..+13 |
| 15:11 (old magnet) | 1903..1904 | −9..+10 |
| 15:17 (old, the bad pass) | 1904..1904 | −3..+3 |
| 15:53 (old magnet) | 1904..1905 | −3..+4 |
| 16:05 (NEW magnet) | 1905..1905 | −5..+10 |

The baseline moves two counts across the entire session and Otto parks clear of
the field every time. The parked-baseline hypothesis from
`QUORUM_STATIONARY_BASELINE_POISONING.md` is not what is happening here.

## The evidence

| time | mm | obs | peak | ms | dt | dt_expected |
|---|---|---|---|---|---|---|
| 15:17:20.548 | 63 | N | 228 | 355 | 2162 | 1762 | ← Grillers |
| 15:17:23.582 | 64 | S | 120 | 452 | 2953 | 0 |
| **15:18:02.188** | **65** | **N** | **41** | **59** | **38975** | **0** | ← **phantom** |
| 15:18:02.706 | 66 | N | 130 | 401 | **195** | 0 |
| 15:18:03.860 | 67 | N | 147 | 157 | 1403 | 0 |
| 15:18:04.851 | 68 | S | 122 | 134 | 1010 | 813 |
| 15:18:05.814 | 69 | S | 145 | 136 | 965 | 827 |
| 15:18:06.729 | 70 | N | 133 | 134 | 917 | 813 |
| 15:18:07.677 | 71 | S | 128 | 130 | 919 | 813 |
| 15:18:08.570 | 72 | N | 104 | 124 | 892 | 799 | ← Westpoint |

Four independent tells identify mm 65 as spurious:

1. **Peak 41.** Every other marker in the stretch is 104–228. The entry
   threshold is ±38 counts from baseline (`QUORUM_STATIONARY_BASELINE_POISONING.md`),
   so it cleared the threshold by **three counts**.
2. **Duration 59 ms.** Real markers here run 124–452 ms. This is two to seven
   times too brief.
3. **`dt` 195 ms to the next marker.** At ~300 mm spacing and the speed Otto was
   actually doing, two real magnets cannot be 195 ms apart.
4. **Three consecutive `obs":"N"`** at mm 65, 66, 67. Real markers alternate
   N/S. The phantom is the surplus N.

`dt=38975` on mm 65 records that Otto had been **stationary for 39 seconds**
before this marker. This is the first detection after departure, at PWM ramping
45 → 58.

**`dt_expected` was 0 across mm 64–67** and only re-armed at mm 68. The
conservation gate was inactive for the entire insertion; nothing was in a
position to reject it.

## Why the disagreements followed

The phantom agreed with the expected polarity, so the adjudicator accepted it
silently and incremented position. From that point the locomotive's idea of
which marker it was at was one ahead of the track. Every subsequent real marker
therefore presented the opposite pole to what was expected:

| mm | observed | expected |
|---|---|---|
| 67 | N | S |
| 69 | S | N |
| 70 | N | S |
| 71 | S | N |

`nav_state` stayed `NORMAL`, `lost` stayed 0, and the adjudicator recovered on
its own — `disagree` never moved past 4 for the rest of the session
(final tally: **agree 1494, disagree 4, lost 0** over 1403 markers).

## The speed spike was a symptom, not the cause

The locomotive's own speed estimate read **1564 mm/s** at 15:18:03 while the IR
spoke read **292 mm/s** — a five-fold overestimate, physically impossible.

That is arithmetic on the phantom, not an independent fault:

```
300 mm spacing ÷ 0.195 s  =  1538 mm/s
```

which is the 1564 reported. The phantom's impossible `dt` *is* the speed spike.

An earlier draft of this record had the causality reversed — speed spike causing
the phase error. One phantom explains both, and the operator's insertion
hypothesis is what separated them.

### What the IR sensor still shows

The independent measurement is what makes the overestimate visible at all:

| time | Otto `est_mm_s` (Hall) | IR `speed_mmps` |
|---|---|---|
| 15:18:02 | 110 | 83.21 |
| **15:18:03** | **1564** | **292.48** |
| 15:18:04 | 213 | 311.35 |
| 15:18:05 | 301 | 311.35 |
| 15:18:07 | 327 | 332.83 |

`ROAD_TO_CTO.md` states the hazard: position and speed both derive from the Hall
sensor, so a bad read corrupts both at once and nothing internal can notice.
This is that hazard, observed. Every signal inside the locomotive was
self-consistent. Only the IR spoke shows the estimate was wrong — and the IR
spoke is only in the record because `ngr_runlog.py` subscribes to `ngr/#` rather
than the old loco-only pattern.

## The magnet replacement did not remove the phantom

The operator replaced the rectangular magnet bedded in the ballast at **mm 64**
with a disk sitting atop the tie, and ran two passes on it.

| pass | mm 64 (real) | mm 65 (fringe phantom) |
|---|---|---|
| 15:05–16:00, ten passes | S / 116–134 | N / 127–147 |
| 16:05 (new) | **S / 211** | N / 148 |
| 16:11 (new) | **S / 210** | N / 129 |

The pole face nearly doubled — 1.75× — and **the fringe did not move**. The
phantom still fires, at the same amplitude and the same opposite polarity.

### Why the strength test misled

This record previously argued that if mm 65 were produced by the magnet at 64,
strengthening 64 should scale 65 proportionally; it did not, so 65 must be a
separate magnet. The operator's correction:

> *"If strength were the only variable. The shape of the field is probably more
> important."*

That is right, and it is the whole point. A disk proud of the tie and a
rectangle bedded in ballast have different field *geometry*, not merely different
magnitude. Peak amplitude at the pole face says nothing about how far the return
flux extends or how strongly it presents at the sensor's height. The fringe is a
property of shape and standoff; scaling the face does not scale it.

The practical consequence: **swapping in a stronger magnet is not a fix for this
class of phantom, and may not be a fix for any of them.** What matters is the
field profile the sensor traverses.

## What is still unknown

- **Why the lobe fragmented on the 15:18 pass and no other.** The fringe is
  marginal by nature; the 15:18 read came in at peak 41 against a threshold of
  38. What decides between one crossing and two — approach speed, the PWM step,
  or nothing more than noise near the threshold — is not answerable from one
  occurrence.
- **Whether the gate being disarmed (`dt_expected=0`) through mm 64–67 is
  correct behaviour on departure**, or a window that should be closed. The gate
  re-armed only at mm 68, after the whole insertion.
- **Whether the navigation map should contain mm 65 at all.** It currently does,
  because the phantom is consistent enough to look like a landmark. That is a
  map describing the detector's artefacts rather than the railway.

## Options, none yet chosen

Recorded so the reasoning is not lost, not as a recommendation:

1. **Move the stopping point.** If Otto parked past the trailing fringe rather
   than in the null before it, departure would not walk through the lobe. The
   cheapest change, and it needs no firmware.
2. **Change the magnet's standoff or orientation**, since shape is the variable.
   Bedding it back down, or re-orienting the disk, alters the fringe the sensor
   sees at its height.
3. **Reject on duration.** Real markers here run 130–490 ms; the fragment was
   59 ms. A minimum-duration floor would have rejected it — but the intact
   phantom runs 416–712 ms and would sail through, so this addresses the
   fragmentation, not the phantom.
4. **Re-arm the conservation gate through departure**, so `dt_expected` is not 0
   across the first markers after a stop.

Note that none of 1–4 is validated, and that 0025 already established the habit
worth keeping here: a phantom on this railway has once already turned out to be
maintenance rather than firmware.

## Next time

```bash
# disagreements
grep -a 'state/nav' /home/david/NGR/telemetry/all_*.log | grep -a DISAGREE

# the markers around them — peak and ms are what expose a phantom
grep -a 'mm/marker' /home/david/NGR/telemetry/all_*.log
```

Look for: a low peak near the ±38 entry threshold, a duration well under 120 ms,
an impossibly short `dt`, and a break in N/S alternation. A deliberate test would
be to stop at Grillers repeatedly and watch the first marker after departure.

## Provenance

First finding produced by the continuous logger enabled the same day (0028), on
a Pi rebuilt the same day after its SD card failed. This session would previously
have been reconstructed by hand from an ad-hoc capture, if it were noticed at all.
