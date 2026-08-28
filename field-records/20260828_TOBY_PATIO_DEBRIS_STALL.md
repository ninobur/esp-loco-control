# Toby — blocked at Patio, and seven minutes of confident silence

**Date:** 2026-08-28
**Locomotive:** Toby (9950012), `QUORUM_1_13`, manual throttle, PWM 90, CW, consist attached
**Found by:** operator, physically — wheels spinning against track debris
**Location:** between MM011 and MM012, on the approach to **Patio (MM015)**

## What happened

| | |
|---|---|
| Last marker before | **MM011 at 11:38:37** |
| First marker after | **MM012 at 11:45:18** |
| Duration of the block | **401.7 s — 6 min 42 s** |
| Expected gap at PWM 90 | ~1.2 s |
| Overrun | **~335x** |

The locomotive was cleared by hand and resumed normally. It is running as this
is written.

## Position was NOT lost

Worth stating first, because it is the good news. `lc_mm` and
`dead_reckoned_mm` both **froze at 11** and did not creep. Nothing was invented
while the wheels turned. On release the next marker read **MM012** — the correct
successor — and the run continued without a correction.

## What the telemetry said throughout

Sampled from 420 alert publishes across the block. Every one of the ~100 status
rows between 11:38:38 and 11:45:18 was **byte-identical in these fields**:

```
level CLEAR   moving 1   pwm 90   est_mm_s 172   nav NORMAL
lc_mm 11      dead_reckoned_mm 11             lostm 0
```

For six minutes and forty-two seconds, stationary against an obstruction, Toby
reported:

- **`moving: 1`** — it was not
- **`est_mm_s: 172`** — 172 mm/s, held perfectly constant for 402 seconds
- **`nav: NORMAL`** — no concern of any kind
- **`level: CLEAR`** — the alert channel never left clear
- **`lostm: 0`** — the lost-marker counter never fired

At the reported speed those 402 seconds are about **69 metres of travel that did
not occur**. Nothing in the published telemetry could have told an operator, or
a dashboard, that the locomotive was stuck. It was found by eye.

## Why est_mm_s says 172

`est_mm_s` frozen at *exactly* 172 for a hundred consecutive publishes is not a
measurement of a stopped train — it is the **last value computed when a marker
was last crossed**, republished unchanged because nothing has recomputed it.
`moving` is likewise derived from commanded PWM, which is a request, not an
observation.

This is **[0024](../docs/decisions/0024-a-request-counter-is-not-a-measurement.md)**
in a third place. A counter of send *requests* was read as send *success*
(1.16Ra). A list of *intended* improvements was read as *achieved* reliability
([0047](../docs/decisions/0047-a-sketch-is-chosen-on-crossings-measured-not-on-features-listed.md)).
Here a *stale cached* speed is published in the position of a *current* one.
None of the three fields is lying about itself; each is being read as something
it never was.

The honest publication of a speed nothing has recomputed in 402 seconds is not
172. It is unknown, or it is zero.

## What would have caught it

- **The steady-running maximum.** Under the operator's timing rule a maximum
  elapsed time applies while running steadily. NAVI implements exactly this:
  expected 1.2 s against 401.7 s observed is a 335x overrun and fails the
  `tim` test immediately. This incident is a concrete case where that rule
  earns its place — a rule that only ever refuses would be worthless, and this
  is what it refuses.
- **IR wheel movement.** The IR car measures wheel rotation, which under a
  spinning-wheel block still reports motion — so IR alone would NOT have caught
  this one. Marker absence is the signal that matters here, not wheel turn.
  Worth recording, because the intuition runs the other way.

## Effect on the calibration data

**None, automatically.** `navi_calibrate.py` breaks a segment on any gap over
300 s, and this gap is 401.7 s, so the stall splits the run into two proven
segments rather than contaminating either. No manual exclusion is needed. This
was not designed for this case; it is the continuity check doing its job.

## Also in today's stream, and NOT faults

Repeating ~30 s gaps at MM017-018, MM109-110 and MM158-160 during the
10:50-11:06 window are **station dwells from the accidental auto run**, not
stalls. The 1471 s gap at 11:06 is the operator stopping to restart in manual
at PWM 90.

## Open

- **Track debris at Patio (MM015) approach.** Cleared once. If it recurs, the
  ballast there wants inspection.
- **`est_mm_s` and `moving` publish stale/requested values as current
  observations.** Not a fault of this run and not fixed here. Recorded so the
  dashboard is never read as proof of motion.
