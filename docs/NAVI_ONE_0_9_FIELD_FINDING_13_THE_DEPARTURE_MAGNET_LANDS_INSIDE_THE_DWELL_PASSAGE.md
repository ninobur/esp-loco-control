# NAVI_ONE 0.9 — Field finding 13

## Arches CCW: the dwell passage swallowed the DEPARTURE magnet, and the remedy proposed for finding 11 would not have caught it

**Date:** 2026-09-01
**Locomotive:** Toby (9950012), `NAVI_ONE_0_9`
**Status:** Observed and decoded. Not a decision. Nothing changed.

---

## The event

```
19:16:48.790  ZERO_RAMP   off  0  MM108        the 0.9 offset, working
19:17:00.619  DWELL_BEGIN off +1  MM107        coast of 1 marker, as measured
19:17:30.606  DEPART      off +1  MM107  pwm 90
19:17:33.186  NOT_A_MAGNET  WRONG_SHAPE  peak 157  ratio 0.737  resid 0.3825
19:17:35.683  AGREE  "MM106"
19:17:37.104  AGREE  "MM105"
19:17:38.704  DISAGREE  obs N, expected S  ->  STRUCK
```

The baseline gate was again perfect: frozen at 1837 from PWM 23, through the
whole dwell, resumed at PWM 43. No capture, no latch.

## The decoded passage — and it is NOT finding 11's shape

Slot 3, open **36,689 ms**, decimated 128:1, polarity S:

```
  fifths mean/sd:  41/3 | 41/3 | 40/6 | 41/2 | 47/25
  head:  29 38 39 41 44 40 44 41 40 38 40 49 46 43
  tail:  46 40 40 42 46 39 41 44 66 94 149 189 157 64
  peak at sample 284 of 287 -- 36,352 ms into a 36,689 ms passage
```

**Flat at 41 counts for thirty-six seconds, then a spike to 189 in the final
340 ms, then it closes.**

Read that back to front. Toby did not park on a magnet. He parked in a weak
field of about **41 counts — three counts above the 38-count entry margin** —
which was enough to open a passage and hold it. Thirty-six seconds later he
pulled away, rolled into **MM106's real magnet**, and that magnet arrived at the
*tail* of the passage that was already open. The whole thing closed as one
object: a 36-second plateau with a spike on the end. Residual 0.3825.

Rejected on shape. **MM106 was consumed**, the position lagged by one marker,
and the first polarity change two markers later struck.

---

## This corrects the remedy proposed in finding 11

Finding 11 recorded a dwell passage at Grillers CCW whose magnet was at the
**head** — the locomotive rolled onto MM59, then sat on it for 37 seconds. The
remedy proposed there was to *judge such a passage on its leading edge*, since
the first second was a clean arc to peak 225.

**That remedy would not have caught this one.** Here the leading edge is 41
counts of nothing and the magnet is in the last 340 ms. A rule that reads the
head would have discarded the very samples that matter.

The two cases together define the actual requirement:

| | Grillers CCW, finding 11 | Arches CCW, finding 13 |
|---|---|---|
| plateau | 163 counts (on the magnet) | 41 counts (fringe) |
| the real magnet is | at the HEAD, on arrival | at the TAIL, on departure |
| peak | 225 | 189, in the final 340 ms |
| open | 37,733 ms | 36,689 ms |
| residual | 0.2773 | 0.3825 |

A marker can be lost at either end of a dwell. Any fix that looks at one end
only will keep half the failures.

---

## What the shape of a real fix has to be

Not implemented, not decided. Recording it so the next session does not start
from scratch.

The capture layer already knows the one thing that separates these passages from
every legitimate one: **`mayAdapt` was false**, so the locomotive was at or below
its tractive floor, for the great majority of the passage. A magnet crossed at
speed is never in that state.

The minimal honest treatment is therefore to stop a passage spanning the
resumption of motion: when motion returns after a stationary period, the passage
that was held open by standing still is closed and the departure gets a fresh
one. That fixes this case outright — MM106 would have opened its own passage and
been judged normally.

It does **not** fix finding 11's case, where the magnet was consumed on arrival
before the locomotive ever stopped. That end needs the leading-edge idea after
all. Both ends, or half the failures remain.

Either way this is a change to the recognizer's contract and it touches decision
0057's deliberate removal of the duration ceiling. It wants a decision and a
specification, not a patch at half past seven in the evening.

---

## Stop-offset tuning is not a fix for this

Two incidents in twenty-one minutes, at two different platforms, both
counter-clockwise, both from the locomotive standing in a magnetic field it did
not choose. The 0.9 offsets did exactly what they were set to do — Arches ramped
at MM108 and rested at MM107, precisely as measured — and the failure happened
anyway.

Where a locomotive comes to rest is a function of grade, load, railhead
condition and the coast of the day. Tuning moves the landing; it cannot
guarantee the landing is clear of every fringe field. **The offsets are worth
having for where the train stops in front of visitors. They are not a defence
against this.**

---

## An observation, not a cause

The reference drifted **+11 counts during the approach while still above the
floor**: 1826 at PWM 43, 1831 at 38, 1834 at 33, 1835 at 28, 1837 at 23 where
the gate closed. That is the PWM 25-39 exposure band recorded in the change-2
commit, seen in the field for the first time.

It did not cause this failure — the parked field was 41 counts against a 38-count
margin, and would have opened a passage with or without those 11 counts. But it
is real, it is the first field sighting, and if the band is ever widened or the
floor lowered it will matter.

---

## Carried forward

- Findings 11 and 13 are one problem seen from both ends. Neither is fixed.
- Finding 12's mechanical signature is unrelated to this and still stands.
- Window depth 6 remains one size too small for the MM107-113 run; it happened
  to be enough here only because the strike came two markers after the rejection.

## Artefacts

- `field-records/logs/20260901_navi_one_arches_ccw_dwell/`
