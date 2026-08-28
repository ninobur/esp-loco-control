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
