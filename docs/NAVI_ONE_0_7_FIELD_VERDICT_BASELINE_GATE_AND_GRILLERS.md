# NAVI_ONE 0.7 — Field verdict

## The motion gate holds through a station stop, and Grillers CW launches unaided

**Date:** 2026-09-01
**Locomotive:** Toby (9950012), `NAVI_ONE_0_7`, boot 17:54:22
**Bootid:** `{"sketch":"NAVI_ONE_0_7", ..., "motion_gate":25}`
**Status:** First field run of both changes. Observed, not ratified.

---

## 1. Grillers CW launches unaided

Operator, lineside: **"Excellent launch from Grillers."**

```
17:59:59.311  ZERO_RAMP   off -1  MM62      <- a marker before centre, off the climb
18:00:11.133  DWELL_BEGIN off  0  MM63      <- comes to rest ON centre
18:00:41.139  DEPART      off  0  MM63  pwm 110
18:00:48.774  DEPARTED    off +2  MM65
```

**7.6 s from departure order to clearing MM65.** The same two markers on 0.6 took
**40 s and the operator's hand**, at 90 PWM with the wheels spinning. The stop
offset of −1 and the 110 departure target do what the operator diagnosed they
would: he is no longer starting the grade while on the grade.

Landing note: rest is one marker past the ramp point, here MM63 against a ramp
at MM62. That matches 0.6's Grillers behaviour (ramp +1, rest +1) and is a
useful constant for tuning the other platforms.

---

## 2. The baseline is not captured by the platform

This is the change that matters, and the telemetry shows the gate opening and
closing exactly on the constant.

```
   pwm 90..29   baseline wanders 1824-1838     adapting normally at speed
   pwm 24       baseline 1830                  <- GATE CLOSES
   pwm 19,14,9,4,0                1830
   ... the whole 30 s dwell ...   1830
   pwm 5, 21                      1830
   pwm 38       baseline 1831                  <- GATE REOPENS
```

Frozen at 1830 from throttle 24, through the stop and the dwell, and back up
through 21 on departure. It resumes at 38. **The boundary is exactly PWM 25**,
the measured tractive floor from this locomotive's own PWM/speed fit.

**1830 going in, 1830 coming out.** Compare the same stop one firmware earlier:

| | approach | after the stop | error on departure |
|---|---:|---:|---:|
| 0.6, Bamboo CW | 1853 | **1899** | 46 counts, above entryMargin |
| 0.7, Grillers CW | 1830 | **1830** | none |

The reference did not learn the magnet it parked on. Gate 11's counter-test
predicted 1899 for the ungated case and reproduced it exactly; the gated case
predicted no movement and the field shows none.

---

## 3. The session so far

Four station stops since boot, all clean:

| station | dir | rest | dwell | departed to | clear in |
|---------|-----|-----:|------:|------------:|---------:|
| Bamboo   | CW | MM160 (+3) | 30.0 s | 90  | 4.8 s |
| Patio    | CW | MM17  (+2) | 30.0 s | 90  | 6.6 s |
| Grillers | CW | MM63  ( 0) | 30.0 s | **110** | 7.6 s |

**154 advances, 0 disagreements, 3 not-magnets, trust PROVEN.**

---

## 4. A diagnostic that is now retired

Through 2026-09-01 a frozen `baseline` in the 1 Hz status payload was reliable
evidence of the finding-08 latch. It no longer is: the reference is *supposed*
to sit still whenever the locomotive is at or below PWM 25, which includes every
station stop and every pause.

**The tell is now a frozen baseline while moving ABOVE PWM 25.** Gate 8 asserts
that condition clears within about three seconds; if the field ever shows it
persisting, that is new.

---

## Carried forward

- Bamboo landed at +3 here against +2 yesterday, so the landing point varies by
  a marker between runs. Worth a few more observations before anyone tunes a
  stop offset on the strength of one landing.
- CCW is entirely untested on 0.7 — the curve into Patio, Grillers CCW at 72,
  and the CCW stop offsets, all still on their first field outing.
- The residual risks recorded in the change-2 commit stand: a locomotive
  coasting below the floor cannot correct its reference; a passage longer than
  the 2 s migration guard taken above the floor can still be closed early.
- Findings 02/03 remain open.
