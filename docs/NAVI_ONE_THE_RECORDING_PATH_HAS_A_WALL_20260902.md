# The recording path has a wall, and three attempts have now hit it

**Date:** 2026-09-02
**Locomotive:** Toby (9950012)
**Status:** MEASUREMENT. No change kept. This records a boundary, not a fix.

---

## The claim

**Any retention rule sensitive enough to recover the falling flank of a
dwell-departure is also sensitive enough to make section E's electrical step
look like a magnet.** Three independent attempts, three different mechanisms,
the same two failures each time.

| attempt | mechanism | F13 clean | electrical step |
|---|---|---|---|
| reach-back fix (morning) | band walk, **both** excisions | 0.1271 → **0.1507** | **0.1196 → advances** |
| X6 departure retention | band, departure side only | 0.1271 → **0.1507** | **0.1264 → advances** |
| bounded band (afternoon) | plateau brackets, band finds onset | **refused** | **0.1264 → advances** |

The third was the most carefully reasoned of the three: the plateau transition
bounds the search to one window, so creep cannot accumulate beyond
`settleWindowMs`, and inside that bound the per-sample band finds where the
field actually left the resting level. It **fixed** the approach-ramp sweep
outright — gate 12 section J went from 12 of 15 to **15 of 15** — and it still
broke the same two cases.

The shipped rule is the plateau test alone, which is safe and insufficient: it
protects F13 and the artifacts, and it does not recover the falling flank.

## Why the wall is where it is

The two things being separated look the same to any rate or level test:

- **A dwell departure** from 133 counts: the field leaves the resting level
  slowly, because the locomotive is accelerating from nothing.
- **An electrical step with a slow leading edge, then parked**: the field
  leaves the resting level slowly, because that is what the artifact does.

Both are "a sustained departure from a plateau, beginning slowly". The
information that distinguishes them is not in *when* the departure began. It is
in *what shape the whole thing turns out to be* — and a single Gaussian
residual over one concatenated record cannot express that, because it has one
amplitude, one centre and one width to say it with.

## What this argues for

This is the strongest evidence so far for the operator's two-sided
interpretation, and it sharpens why. The two-sided model is not a looser test —
it is a test with **more evidence to discriminate on**: two amplitudes that must
agree, two widths that may differ, an ordering that must be one rise-peak-fall,
and a segment boundary that says which samples belong to which movement
interval.

An electrical step and a real dwell-departure are hard to tell apart by *onset*.
They are not hard to tell apart by *whether both halves describe one magnet of
one amplitude*. The step's two halves do not: its "arrival" is a ramp with no
apex to extrapolate, and the amplitudes its halves imply do not agree.

That is a discrimination the current single-residual judgement cannot make and
the two-sided one can, which is the case for building it.

## The field records this rests on

Three faults at Arches CCW today, all at MM107 → MM106, all different:

| time | build | paused | resid | what was lost |
|---|---|---|---|---|
| 12:13:49 | X4 | 37,211 ms | 0.2695 | falling side — closed out of the pause |
| 14:14:40 | X6 | 0 ms | 0.2706 | arrival — discard thrashing (X6's own regression) |
| 15:03 | X7 | 1,072 ms | 0.2816 | falling side — paused on the approach ramp, could not resume |
| 15:21 | X7 | 35,928 ms | 0.2460 | falling side — 133 down to 22 in one sample, 287 ms of 903 |

The 15:03 one is fixed in X8. The other three are the same underlying shortfall
in different clothes, and none of them is fixed.

## What is not concluded

That the retention idea is wrong. It is not — the bounded-band version fixed
every approach-ramp case in the sweep. What is concluded is that it cannot be
shipped **alone**, because the judgement it feeds cannot tell its successes from
its failures. Retention and two-sided judgement are one change, not two, and
attempting the first without the second has now cost three builds.
