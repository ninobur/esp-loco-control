# Otto X21 — 95 advances CCW, then a 30 ms conversion burst opened an event 330 ms before the magnet

**2026-09-16, 11:25–11:30.** Otto 9950011,
`NAVI_ONE_1_0X21_HALL_ONLY_FIELDTEST`, `refract_ms 645`. Declared CCW.
Struck at MM117 → 116 after **95 advances, 0 refusals, 0 non-magnets**,
`trust PROVEN`. Stopped near MM115 on `withdraw()`.

Provenance: `~/NGR/telemetry/runs/9950011_20260916_112539.log` on
192.168.68.142. X21 booted 11:22:27 and again 11:25:22; the run log opened
11:25:39, which is why it carries no `bootid` line of its own.

---

## 1. The run up to the strike

97 `diag/excursion` records, 95 advances, one strike. The ten-magnet witness
reached `PROVEN`, so the map and the track agreed for half a lap.

Two numbers settle the risks flagged when X21 was written:

* **Records below the 0.34 amplitude floor: 0.** The recognizer screen X21
  stripped of authority would have refused *nothing* in this run. Limitation 1
  of the implementation report did not bite here.
* **Records where the window argmax disagrees with the opening sign: 1.** It is
  the strike. `w_caliper_ms` median 145 across the run; this record 348.

## 2. The strike record

```
det=254581  depart +85 -> N     expected S at MM116
peak 138  peak_signed -138      <-- the window's argmax is SOUTH
exc_first 826 (= detection +314 ms)   exc_n 87   exc_sum -9750
w_caliper_ms 348  (clean magnets this stretch: 140-158)
ratio 0.826  gain 167           <-- nowhere near the 0.34 floor
```

The raw record, L-relative, ms from the detection sample:

```
  ms   -24  -20  -16  -12   -8   -4   +0   +4   +8  +12  +16  +20  +24  +28
  val  +16  +16  +15  +16  +16  +18  +85 +116  +72  +79 +102  +72  +88  +43

  ms  +300 +310 +320 +330 +340 +350 +360 +368 +380 +390 +400
  val  -39  -39  -80 -106 -114 -125 -134 -138 -121 -110 -102
```

**The opening is not a magnet.** The line sits flat at +16 for 24 ms, then
throws a ~30 ms burst of jittering conversions — 85, 116, 72, 79, 102, 72, 88 —
swinging 30–45 counts sample to sample, before settling to a +40 step that
decays away. Every clean magnet on this stretch is a smooth half-sine peaking
at 158–193 with `exc_first 512`, i.e. the excursion starts *at* detection.

**The magnet is the late lobe.** The signal crosses zero at +280 ms and dives
to −138 at +368 ms, still at −102 when the window ends. That is South, it is
the right amplitude, and its own ≥70 crossing would land at ≈254911.

## 3. The arithmetic that identifies it

Spans through this stretch are all 300 mm. Detect-to-detect for the four
preceding accepted markers:

```
  1213 ms   1252   1156   1140      ->  247, 240, 260, 263 mm/s
```

The strike event sits **919 ms** after MM117 — 326 mm/s, a 25 % jump in one
300 mm span at unchanged throttle (`pwm_detect 80`). Not physically credible.

The late lobe's crossing at ≈254911 is **1249 ms** after MM117 — 240 mm/s,
squarely inside the run's own cadence.

**So det=254581 is not MM116. It is a burst 330 ms ahead of MM116, and MM116
itself then fell inside the 645 ms guard and was never counted.**

## 4. What X20 would have done, and why that matters

Under X20 the *same* detection fires — the detector, the ≥70/2 test and the
guard origin are unchanged by X21 — and MM116 is swallowed by the guard either
way. The difference is only the pole:

| | polarity from | result |
| --- | --- | --- |
| X20 | excursion sum over the window, −9750 | **S** — matches MM116, advances |
| X21 | opening sign, +85 | **N** — strike |

X20 would have advanced, and would have been **right by accident**: the event
was framed on a burst, but the window it then measured contained the real
magnet, so the pole came out correct. **The 400 ms window was quietly
supplying burst immunity**, and nobody had named it as a function.

State the limit of this claim plainly: X20's outcome here is inferred from
X21's own record, not measured on an X20 run. What is measured is that X21
struck.

## 5. What this is and is not

**It is not MM136 recurring.** MM136 was the window overruling a correct
opening. This is the opening being wrong and the window being right. Both are
the same underlying fact — *the detector can frame an event on something that
is not the magnet* — seen from opposite sides.

**It is not the amplitude screen.** `ratio 0.826`. Restoring it changes
nothing here.

**It is not a single conversion.** Decision 0073's 2-sample persistence is
sized for the 1.1-in-1000 single-sample population (bench 2026-09-03). A 30 ms
burst of bad conversions clears two samples trivially. This is the MM157 burst
population (decision 0071), which the persistence test was never built to
refuse, meeting a polarity rule that now has no second opinion.

**It is not the 645 ms guard being too long.** MM116 was swallowed because the
event opened 330 ms early, not because the guard is wide. A 500 ms guard from
this same detection expires at 255081, still ahead of MM116's ≈254911 crossing.
X20 would have swallowed it too.

## 6. Not proposed here

No width screen, no burst detector, no median-of-five, no second threshold, no
morphology, no re-referencing of the guard. One event in 97 is not a mandate,
and requirement 8 of the X21 brief forbids most of it outright. What this run
establishes is the **question**: how often does an opening frame an event on
something that is not the magnet, and is that rate tolerable without the
window's second opinion. One in 97 here, with the disagreement already
instrumented on `diag/excursion` as `sign(depart)` vs `peak_signed`.

## 7. Defect found in X21's own telemetry

`nav/marker` and `state/nav` publish `"win_ms":4294712715`. `publishNav`
computes `windowEndMs - detectedAtMs`, and X21 sets `windowEndMs = 0` in the
detection-time record because the window has not run yet. Unsigned underflow.
Cosmetic — nothing reads it — but wrong, and it is X21's, introduced with the
detection-time queueing. Not fixed in this commit; the locomotive is fielded.
