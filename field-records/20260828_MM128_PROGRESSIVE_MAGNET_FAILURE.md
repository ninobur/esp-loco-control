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
