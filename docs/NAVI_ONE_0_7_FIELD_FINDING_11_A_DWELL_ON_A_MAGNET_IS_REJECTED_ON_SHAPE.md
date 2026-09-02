# NAVI_ONE 0.7 — Field finding 11

## Grillers CCW coasts two markers past its stop, parks on a magnet, and the dwell passage is thrown away on shape

**Date:** 2026-09-01
**Locomotive:** Toby (9950012), `NAVI_ONE_0_7`, boot 17:54:22
**Status:** Observed and decoded. Not a decision. Nothing changed.

---

## The headline: the baseline gate worked, and exposed what was behind it

This is **not** the capture of finding 10 and **not** the latch of finding 09.
The reference behaved exactly as change 2 intended:

```
   pwm 70..30   baseline 1829-1836     adapting on the way in
   pwm 25       1829                   <- GATE CLOSES
   pwm 20,15,10,5,0        1829
   ... the whole 30 s dwell ...  1829
   pwm 2, 18               1829
   pwm 34                  1829        <- GATE REOPENS
   pwm 50                  1826
```

1829 going in, 1829 coming out. No capture, no latch, no drift. The failure is
somewhere else entirely.

---

## What happened

```
18:20:16.384  ZONE        off -5  MM68  pwm 72
18:20:25.228  ZERO_RAMP   off +1  MM62         <- the ramp starts where it should
18:20:39.467  DWELL_BEGIN off +3  MM60         <- but it comes to rest TWO markers on
18:21:09.460  DEPART      off +3  MM60  pwm 90
18:21:11.588  NOT_A_MAGNET  MM60  WRONG_SHAPE  peak 225  resid 0.2773
18:21:14.448  DISAGREE      MM60  POLARITY_MISMATCH  obs S, expected N  -> STRUCK
```

**Grillers CCW is the descent.** It holds station speed 72 there — higher than
the 60 everywhere else, because "72 is wrong for downhill at Grillers" applies
to the approach, not the coast — and then rolls downhill. Clockwise, the same
platform climbs and stops one marker past the ramp point. Counter-clockwise it
carries two. Same station, opposite grade, different landing.

Resting on MM60 put the sensor in **MM59's** field.

---

## The decoded dwell passage

| slot | outcome | pol | open for | peak | resid |
|-----:|---------|:---:|---------:|-----:|------:|
| 5 | MAGNET | N | 193 ms | 233 | 0.0701 |
| 4 | MAGNET | S | 175 ms | 203 | 0.0830 |
| 3 | MAGNET | N | 242 ms | 235 | 0.0697 |
| 2 | MAGNET | S | 345 ms | 215 | 0.0757 |
| 1 | **WRONG_SHAPE** | **N** | **37,733 ms** | 225 | **0.2773** |
| 0 | MAGNET | S | 209 ms | 212 | 0.0772 |

Slot 1, decimated 128:1, quarter by quarter:

```
  mean/sd:  164/27  |  163/4  |  163/3  |  161/13
  head:     30 65 104 155 192 225 217 232 217 205
  tail:     159 164 154 161 166 162 156 126 104 75
  peak at 896 ms into a 37,733 ms passage
```

It rises cleanly to 225 in the first second — the locomotive rolling onto the
magnet — then **sits at 163 ± 3 counts for thirty-five seconds**, then decays
away as it departs.

**That is MM59's magnet.** Polarity N, which is what MM59 was expected to be.
Amplitude 225, entirely healthy. It was acquired correctly and held open for the
whole dwell, which is the right thing to do with the reference frozen.

Then it was measured against a Gaussian, and a 37-second plateau is not a
Gaussian. Residual 0.2773 against a ceiling of 0.13. **Rejected on shape, so
MM59 was consumed and never counted.** The next real magnet, MM58, arrived as S
against a still-pending N and struck.

No marker was double counted. One was lost. MM60 appears three times on the
dashboard because the advance and both rejected verdicts share that position.

---

## This is the case QUORUM anticipated and NAVI_ONE does not handle

`QUORUM.ino` v1.8, layer 1: *"a DWELL ON A MAGNET now legitimately holds an
event open for the whole stop (the baseline no longer migrates to close it); it
closes at departure as one arrival-stamped marker, ms saturating at 65535. That
signature is EXPECTED, not a stuck detector."*

QUORUM took the polarity from the arrival and **counted it as one marker**.
NAVI_ONE judges the whole shape and throws it away. The motion gate imported the
condition without the accommodation.

---

## Two ways out, neither taken

**Do not stop on a marker.** Grillers CCW's stop offset is a tunable column and
its CW twin is already asymmetric for exactly this reason. Moving it so the
locomotive rests between magnets is a table change, no logic, and it is the
smallest thing that works. It does not generalise: any platform whose landing
drifts onto a magnet has the same exposure.

**Judge a dwell passage on its arrival.** The information is in the recording --
the first second is a clean arc reaching 225 before the plateau begins. A
passage the locomotive was stationary through could be judged on its leading
edge rather than its whole length. This is the general fix and it is a change to
the recognizer's contract, so it wants a decision and a specification, not a
patch. It also touches decision 0057, which deliberately removed the duration
ceiling.

---

## Carried forward

- CCW landings are otherwise unmeasured on 0.7. This stop is the first CCW data
  point and it is already two markers from its CW twin.
- Findings 09 and 10 remain fixed; this record is evidence the fix works, not
  evidence against it.
- Findings 02/03 remain open.

## Artefacts

- `field-records/logs/20260901_navi_one_ccw_dwell_on_magnet/`
