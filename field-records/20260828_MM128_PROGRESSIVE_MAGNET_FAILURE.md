# MM128 — a magnet failing in real time, found by calibration

**Date:** 2026-08-28
**Locomotive:** Toby (9950012), `QUORUM_1_13`, manual PWM 90, CW, consist attached
**Air temperature:** 99 °F (operator)
**Status:** OPEN — wants physical inspection

## The finding

The map records MM128 as **S**. Over three days the readings have walked away
from it, and today they inverted completely.

| day | reads N | reads S | median peak |
|---|---|---|---|
| 26 Aug | 3 | **14** | 186 |
| 27 Aug | 8 | 15 | 173 |
| 28 Aug, earlier | 4 | 6 | 168 |
| **28 Aug, manual CW** | **6** | **0** | **108** |

Six laps today, six N readings, zero S. Peak collapsed to **108 against
neighbours at 181-187** — about 58% of normal.

This is not a map error. On 26 August the map and the magnet agreed, 14 times
out of 17.

## Temperature is excluded

The operator reports 99 °F at the time of the CW run, and session-to-session
gain shifts of up to 13% have been seen before. That cannot explain this:
**MM127 (187) and MM129 (181) are unchanged**, measured on the same laps, by
the same sensor, at the same temperature, minutes apart. A thermal effect does
not single out one magnet between two healthy neighbours.

## Leading hypothesis

The magnet at MM128 is **displaced, sinking, or gone**, and the detector is
triggering on the fringe field of its neighbours. MM127 and MM129 are both
**N** — which is precisely the polarity MM128 has adopted — and a fringe field
read at distance would arrive weak, which matches the 108.

Alternatives not excluded: a magnet physically flipped in place (but that
should preserve magnitude, and magnitude fell by 42%), or one pushed deeper
into the ballast by the same kind of debris that blocked Patio today.

**The distinguishing test is visual.** Look at MM128.

## Why it mattered beyond one marker

MM128 broke the proven segment at the same point on every lap. Because the
calibration gain was (wrongly) restarting per segment and needs 8 samples to
yield a value, **MM129 through MM136 were blinded on every lap** and vanished
from the tables. Nine markers appeared uncalibrated; only one was faulty.

That is also, in hindsight, what the MM128-135 "contamination" block seen
earlier today was pointing at. It was read as skipped magnets in an old sketch's
data. The same nine markers, the same cause, twice missed.

The gain scope is corrected (see `tools/navi/navi_calibrate.py`). With the fix,
today's single CW run calibrates **170 of 171 markers** — everything except
MM128 itself.

## Recommended

1. **Inspect MM128 physically.** It is the only marker on the railway that
   cannot currently be calibrated.
2. Until then NAVI would refuse it and stop the train there — correctly.
   `tolStrPct[128]` / `tolDurPct[128]` emit as **0 = uncalibrated, abstain**.
3. Re-read MM128 after any repair and confirm it returns **S at ~185**.
4. Watch MM152 the same way — it fell 123 -> 85 and is already on the
   double-magnet inspection list. Two markers now show strength anomalies.

---

## RESOLVED — 2026-08-28, same day

**Operator found the magnet had come loose from the track.** The leading
hypothesis was correct: displaced magnet, detector reading neighbouring fringe
field. Refitted by the operator and re-sampled at 12:47-12:50.

| | before (broken) | after refit | map / expectation |
|---|---|---|---|
| polarity | N, 6 of 6 | **S, 5 of 5** | **S** |
| peak | 108 | **209** (208-212) | 184 was the last good |
| % of session gain | — | **111.2%** | — |

Neighbours confirm nothing else was disturbed: MM127 at 97.3% of gain, MM129 at
92.8%, both normal.

The repair run segmented at **100.0% — 112 of 112 crossings proven, zero
discarded**, the first perfect run in the record. The break that had split every
lap at MM128 is gone.

### The baseline is superseded, not restored

MM128 now reads **209 against a pre-failure 184**, about 14% stronger. The
magnet is seated closer or straighter than it was before it worked loose, so
the historical value is stale and must not be used as its expectation. Tables
take the post-repair figure.

Worth noting for MM152, which is on the double-magnet list at 123 -> 85: a
refit does not necessarily return a magnet to its old number, and the old number
is not the target. What matters is that the new value is stable across laps.

### Still open

The repair sampling is **3 CW and 2 CCW passes** at MM128. Calibration requires
**4 per direction**, and the two directions cannot be pooled
([0048](../docs/decisions/0048-expectation-tables-are-per-direction-because-the-railway-has-grades.md)).
MM128 therefore remains uncalibrated in both tables, one or two passes short in
each. A single further lap each way closes it and takes both tables to 171/171.

Duration already shows the expected direction split even in this small sample —
CW 137-145 ms against CCW 165-171 ms, the same sense as the MM129-140 block
(-12 of 12) that 0048 attributes to grade.

---

## CORRECTION — the diagnosis was half right, and the wrong half matters

**Operator, on recovering it:** MM128 was **upside down, tilted, and lying
between sleepers two sleepers from its assigned position.**

This record claimed the failure was a displaced magnet with the detector
reading **neighbouring fringe field**, and explicitly *dismissed* a flip on the
grounds that flipping preserves magnitude while the observed magnitude fell 42%.

**That was wrong.** The magnet was flipped. The `N` readings were the real
magnet, inverted — not fringe field from MM127 and MM129. The reasoning failed
because it treated *flipped* and *displaced* as competing explanations when they
were concurrent: the tilt and the offset account for the lost magnitude that
was used to rule the flip out. A single cause was sought where three faults had
occurred together.

What survives: the magnet was displaced, and it was not doing its job. What does
not: the mechanism. The polarity reading was true information about the magnet's
orientation and was read as an artifact.

## The displacement was measurable in the existing data

`dt` — time since the previous accepted marker — at held PWM 90:

| leg | before repair | after repair | map |
|---|---|---|---|
| MM127 -> MM128 | **1251 ms** | 1055 ms | 300 mm |
| MM128 -> MM129 | **836 ms** | 1108 ms | 290 mm |
| pair total | 2087 ms | 2163 ms | 590 mm |

**A long leg followed by a short leg, with the pair total conserved.** The
neighbours had not moved, so the sum of the two legs is fixed by them; only the
split changed. Before the repair MM128 sat at 59.9% of the pair, after it sits
at 48.8%.

Converting the shift against the mapped 590 mm:

```
implied displacement along the track:  66 mm
operator's observation:                two sleepers
```

About 33 mm per sleeper. **The telemetry predicted the physical finding.**

## Why this is worth building on

**Polarity checking cannot detect displacement.** A magnet knocked 66 mm along
the track but still the right way up passes every polarity test on the railway,
every lap, silently — while corrupting the timing NAVI uses and the position it
reports. MM128 was only caught because it *also* flipped. Nothing in the current
firmware was watching the thing that actually moved.

The signature is specific and cheap to test: **consecutive legs that deviate in
opposite directions while their sum stays true to the map.** Ordinary speed
variation moves both legs the same way; a displaced magnet moves them apart.
That distinguishes a moved marker from a slow locomotive without a speed model.

Proposed as a check for NAVI — not yet built, not yet a decision. It wants its
own record and a test against the four days of data now available, including
whether it would have flagged MM128 on 26 August, before the polarity went.

**MM152 should be examined this way too** (123 -> 85, double-magnet list). If
its leg pair is skewed with the sum conserved, it has moved rather than doubled.
