# Otto X21 — a 37 ms phantom near Bamboo stops the first CW run

**Date:** 2026-09-16
**Locomotive:** Otto, 9950011
**Build:** `NAVI_ONE_1_0X21_HALL_ONLY_FIELDTEST`
**Run log:** `9950011_20260916_121639.log` (12:16:39 – 12:19:31, CW)
**Stop:** 12:17:31.742, `WRONG_MAGNET` / `POLARITY_MISMATCH`, mm 153 tgt 154,
obs N expected S, `gap_ms 784`, `adv 38`

## Verdict first

This is the Bamboo short-transient of decisions 0084/0085, recurring. It is
**not** a consequence of X21's polarity rule, and **not** a consequence of the
645 ms guard. X20 makes the identical call on the identical record.

The only screen that rejects this event is the 82 ms completed-passage floor
the operator approved on 2026-09-12 ("Yes. I approve 82", decision 0085).
That screen exists in the code as `DetectorConfig::widthFloorMs`, wired and
working, and is set to `0` in the flashed X21 build — deliberately disabled,
because requirement 8 of the X21 brief forbids a width requirement.

## The record

Nine clean advances on the run into Bamboo, decelerating from PWM 90:

| mm | gap_ms | pwm | depart | peak | w_caliper_ms | exc_n | ratio |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 145 | 1270 | 90 | +73 | 170 | 144 | 120 | 1.062 |
| 146 | 1220 | 90 | +72 | 160 | 130 | 115 | 1.000 |
| 147 | 1167 | 90 | +74 | 170 | 131 | 106 | 1.062 |
| 148 | 1171 | 84 | −73 | 145 | 111 | 103 | 0.906 |
| 149 | 1187 | 79 | +71 | 153 | 147 | 128 | 0.956 |
| 150 | 1372 | 73 | −71 | 164 | 147 | 124 | 1.025 |
| 151 | 1507 | 68 | +74 | 140 | 161 | 142 | 0.875 |
| 152 | 1832 | 62 | +78 | 120 | 186 | 173 | 0.764 |
| 153 | 1808 | 60 | +71 | 169 | 214 | 184 | 1.105 |

Then, 784 ms after MM153's detection:

```
det=3130494  raw 2047  local_ref 1972  rest_ref 1972  depart +75  -> N
             peak 75  peak_signed +75  exc_sum 2158  exc_n 37
             w_caliper_ms 37  w_frac_ms 37  ratio 0.490  pwm 48
             -> judged against MM154 = S -> STRIKE
```

Every genuine magnet on the approach is 111–214 ms wide with a peak of
120–170. This event is **37 ms wide with a peak of 75** — the peak *is* the
opening sample. It departs to +75 and comes straight back.

## It is not a magnet, by arithmetic

The nine preceding gaps run 1167–1832 ms for a 300 mm pitch: 164–257 mm/s,
falling as the brake comes on. A genuine magnet 784 ms after MM153 requires
**383 mm/s** — a 130% speed increase while PWM drops 60 → 48 and the
locomotive is 2.1 s from a dead stop (`DWELL_BEGIN` at 12:17:33.669).

Read the other way: at ~130 mm/s average over those 784 ms the sensor moved
roughly **100 mm**, about a third of a magnet pitch past MM153. There is no
magnet there.

## Why X21 is not the cause

- **Polarity.** The event is 37 ms long, so the opening sample and the window
  argmax are the same sample: `depart +75`, `peak_signed +75`. X19/X20's
  signed excursion sum is `+2158`, also N. Every rule in the lineage returns
  North here. X21's fixing of polarity at detection changed nothing.
- **Amplitude.** `ratio 0.490`, above the recognizer's `amplitudeFloor` of
  0.34. X20's screen admits it too.
- **The guard.** 784 ms clears the 645 ms guard, and also clears X19's 500 ms
  guard. Neither value is implicated. A guard wide enough to cover it would
  have to exceed 784 ms at PWM 48 — i.e. be speed-dependent, which is one of
  the things requirement 8 exists to prevent.
- **Width.** `width_rejected: 0`, because `widthFloorMs = 0`. This is the one
  discriminator that separates the phantom (37 ms) from every genuine magnet
  on the approach (111 ms and up), and it is switched off.

## Relation to the 2026-09-12 event

| | 2026-09-12 CCW | 2026-09-16 CW |
|---|---|---|
| after | MM151 N accepted | MM153 N accepted |
| phantom | N, 558 ms later | N, 784 ms later |
| expected | MM150 S | MM154 S |
| duration | 47 ms | 37 ms |
| peak | 86 | 75 |
| result | position withdrawn | position withdrawn |

Same station area, same signature: a short, weak transient carrying the
polarity of the magnet just passed, which is by construction the opposite of
the polarity expected next — so it always strikes rather than miscounting.

The two are not at the same physical spot (one is between MM151 and MM150
travelling CCW, the other between MM153 and MM154 travelling CW). What they
share is the documented 40–58 ms Bamboo band, which today extends to 37 ms.

## Session tally

822 advances across today's eight running logs; seven stops:

- five at Grillers, MM60 → 59, the rest-migration fault recorded in
  `20260916_OTTO_X21_GRILLERS_REST_MIGRATES_ON_THE_RAMP.md` (a sixth CCW
  strike at 12:08:28 is the same fault and is included in that five)
- one at MM117, the 30 ms burst recorded in
  `20260916_OTTO_X21_MM117_BURST_OPENING_STRIKE.md` — the only stop
  attributable to X21's own rule
- one at Bamboo, this record, which X20 would also have produced

## What is not proposed here

Nothing. `widthFloorMs` is a live, tested configuration value and setting it
to 82 would take one line, but decision 0085 authorised a field trial on the
X16 acquisition path, not adoption, and requirement 8 of the X21 brief
forbids a width requirement in this build. This record states the finding and
stops there.
