# NAVI_ONE — replay of a motion-aware, no-closure state model

**Date:** 2026-09-15
**Build:** `NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST` (working tree), Otto 9950011,
entry 70 / exit 25 / floor 82, `fixedAfterPrime = true`, `NAVI_BASELINE_ADAPT_PWM = 24`.
**Data:** complete 2026-09-15 telemetry including the Pi tail to 13:50:10.
52 passages with complete raw samples, 4,367 nav events, 155 departure-diagnostic
records at 10 Hz, 27 crossings of the STOPPED boundary.
**Status:** Replay only. X18 unmodified, no X19, no decision record proposed.

---

## Headline

**The motion premise is sound and the motion gate is *not* what rescues the
architecture.** Of the nine +500 ms re-triggers, six are removed by motion
gating — but **all nine** are removed by the Hall-transition requirement alone
(observe `|raw − ref| < ENTRY` once before accepting another upward crossing).
Motion gating is a strict subset of what the transition rule already does on
every case this dataset represents.

That does not make it worthless: it covers a hazard the transition rule cannot
(parked at a field's fringe with the level oscillating across the entry
threshold), and the transition rule covers one motion cannot (still deep in the
field while moving slowly). Neither hazard is represented on September 15.

**One correction to the brief before the results:** the 22-second event is *not*
a station dwell. It is `11:50:57.201`, boot 5, the boot that primed ~112 counts
wrong. The actual station dwells were ~30 s at Patio and ~30 s at Bamboo, and
both are clean — see §3.

---

## 1. Of the 9 re-triggers, how many disappear because Otto was STOPPED?

PWM evaluated **at the re-arm instant** (detection + 500 ms), not at close.

```
  11:29:24.399  boot3  dur    656  |v|@+500=129  pwm= 0   STOPPED
  11:29:24.387  boot3  dur   2775  |v|@+500= 90  pwm= 0   STOPPED
  11:29:43.146  boot3  dur  16004  |v|@+500=100  pwm= 0   STOPPED
  11:29:57.084  boot3  dur   5478  |v|@+500= 75  pwm= 9   STOPPED
  11:29:59.317  boot3  dur   1945  |v|@+500= 76  pwm= 0   STOPPED
  11:30:09.202  boot3  dur   2107  |v|@+500= 84  pwm= 0   STOPPED
  ------------------------------------------------------------------ 6 removed by motion gate
  11:18:10.395  boot3  dur    672  |v|@+500= 93  pwm=42   MOVING, low speed
  ------------------------------------------------------------------ 1 survives motion gating
  11:50:57.201  boot5  dur  22053  |v|@+500= 83  pwm= 0   STOPPED  } invalid reference
  11:50:57.196  boot5  dur   4996  |v|@+500= 83  pwm=88   MOVING   } (§10)
```

**DEMONSTRATED: 6 of 9 removed by motion gating.** All six are the 11:29–11:30
hand-push experiment, not station dwells.

**The surviving moving case is a reference-validity problem, not a motion
problem.** At 11:18:10 the clean shadow read **+38 to +49 counts** for twenty
consecutive seconds. With `|E| ≈ 45`, ordinary track already sits at 45, so
re-arming below the entry margin of 70 requires the residual field to fall under
**25** rather than 70. The offset ate two-thirds of the re-arm budget. This
matters generally:

```
  |E| = 0    re-arm needs residual field < 70
  |E| = 45   re-arm needs residual field < 25      <- 11:18:10
  |E| >= 70  never re-arms
```

**The entry-gated re-arm degrades as the reference ages — just far more slowly
than the exit-gated one (70 counts of budget instead of 25).**

## 2. The station dwells — **DEMONSTRATED: zero duplicates, for a simpler reason than expected**

```
  Patio   ZONE 11:02:28 (mm20) -> ZERO_RAMP 11:02:44 (mm15) -> DWELL 11:02:56 -> DEPART 11:03:26
  Bamboo  ZONE 11:04:11 (mm162)-> ZERO_RAMP 11:04:24 (mm158)-> DWELL 11:04:36 -> DEPART 11:05:06
```

The 10 Hz departure diagnostic covers both. Throughout each dwell and the entire
acceleration ramp:

```
  Patio  mm=15:  delta = +1,+2,−6,−4,+1,+1,+2,+2,+3,... never beyond ±9,  open=0
  Bamboo mm=157: delta =  0,−10, 0,+1,+1,−10,−12, 0,...  never beyond ±12, open=0
```

**Otto was not sitting in a magnetic field at either station.** Zero duplicate
advances — but under *any* rule, including the unconditional one. The dwell case
is therefore **not exercised by this dataset**; motion gating is untested where it
would matter most. (Finding 10's parked-in-fringe-field event is 2026-09-01, a
different locomotive and day.)

## 3. Departure: can an existing level be mistaken for a new magnet? — **DEMONSTRATED: not here**

First crossing of the entry margin after each departure:

```
  Patio   motion resumes 11:03:26 (pwm>=25 at t+1.6 s)  first |delta|>=70 at t+4.7 s, pwm 76, delta +78
  Bamboo  motion resumes 11:05:06 (pwm>=25 at t+1.6 s)  first |delta|>=70 at t+5.4 s, pwm 87, delta −158
```

Across the whole day: **27 crossings of pwm = 25, and zero accepted magnets whose
detection fell within 2 s after a STOP→MOVE crossing.** The first real magnet
after a departure arrived 4.7 s and 5.4 s later. No false event, no missed magnet
at the boundary.

**UNTESTED:** because both departures began at delta ≈ 0, this says nothing about
a departure that begins *inside* a field — which is exactly the case the re-arm
rule exists for.

## 4. The minimal definition of a new Hall encounter after motion resumes

Four candidate rules, replayed over all 52 passages (window 150 ms, refractory
500 ms):

```
  rule                                                     events   re-triggers left
  (a) unconditional re-arm at +500 ms                        183          9
  (b) + require pwm >= 25 at re-arm (motion gate only)        ~            3
  (c) + require |v| < ENTRY seen once (transition only)        71          0
  (d) motion gate AND transition                               71          0
```

**DEMONSTRATED: (c) alone removes every re-trigger in the dataset. (d) is
identical to (c) on all 52 passages.** The transition requirement subsumes motion
gating on everything September 15 contains.

**The smallest rule justified by represented evidence is (c):**

> Re-arm when `now ≥ detectionMs + 500` **and** at least one sample since
> detection satisfied `|raw − ref| < ENTRY`.

That is one bit of state, no exit margin, no local baseline, no
return-to-baseline question, and no new threshold — it reuses the entry margin,
so it tolerates 70 counts of reference error instead of 25. It is the definition
of an edge-triggered detector: a new rising edge cannot be recognised until the
signal has come down.

**I tested no alternative observable that works.** Distance travelled since
motion resumed would need a PWM→speed model, and the day's data shows speed at a
given PWM varies with grade (cruise PWM is position-dependent by design,
decisions 0066/0067); a falling-edge-of-size-N test is the transition rule with
an extra constant and no evidence to set it.

**Why keep the motion gate anyway, despite being redundant here.** The two rules
fail on disjoint cases:

| | transition rule (c) | motion gate |
|---|---|---|
| still deep in field, moving slowly | **catches** (11:18:10) | misses |
| parked at a fringe, level oscillating across entry | misses — each crossing 500 ms apart is a new event | **catches** |

The second case did not occur on September 15 and cannot be ruled out. But
adding the motion gate introduces a hazard of its own — see §5.

## 5. FAILURE MODE INTRODUCED BY MOTION GATING: coasting

**`PWM < 25` does not mean stationary.** It means below the tractive floor, and
`HallCapture.h:312-329` says so explicitly ("Not proof of rest — a locomotive can
coast at PWM 0"). A station approach sets ZERO_RAMP while the locomotive is still
rolling, and markers are crossed during that roll.

```
  accepted magnets by PWM at close:   >=80: 4204    60-79: 21    25-59: 4    <25: 0
  Patio approach  mm20..mm15 : all closed at pwm 60
  Bamboo approach mm162..158 : pwm 60;  mm157: closed at pwm 40   <- tightest case of the day
```

**DEMONSTRATED: zero accepted magnets at pwm < 25 all day.** So motion gating
would not have suppressed a single genuine magnet.

**But the margin is 15 PWM counts on a sample of two station stops.** Bamboo's
last marker closed at pwm 40 with the throttle ramping toward 0; a slightly
faster ramp, or a marker a metre further on, puts a genuine magnet's *detection*
below 25 while the locomotive is still rolling. This dataset cannot bound that
risk.

## 6. The 3652 ms low-shelf event — **DEMONSTRATED: unchanged**

PWM was 89–91 throughout, so the locomotive was MOVING and the motion gate is
inactive. The prior result stands in full:

```
  shelf detected at |v|=75, measured ratio 0.43 -> falsely ACCEPTED (amplitude cannot refuse it)
  |v| = 34 at detection+500 ms  -> below entry, re-arms cleanly under both (c) and (d)
  real magnet crosses entry at +992 ms, a 150-300 ms window measures ratio 1.16-1.25 -> ACCEPTED
```

**Motion gating neither helps nor hinders here.** The shelf is rejected-or-not by
amplitude, which fails; the real magnet is recovered either way.

## 7. The two lost-magnet/strike cases — **DEMONSTRATED: unchanged**

Both at PWM 89–91, MOVING, motion gate inactive. Under (c) or (d), window 150 ms:

```
  12:06 merge (MM013 strike)   -> 2 accepted (peaks 159, 222)   CORRECT -- strike prevented
  13:20:55 passage             -> 1 accepted (peak 227)         CORRECT
  13:21:03 passage             -> 1 accepted (peak 220)         CORRECT
  11:05 merge (MM150 strike)   -> 3 accepted (204, 205, 83)     OVER-COUNTS BY ONE
```

The 11:05 extra is a tail fragment at ratio 0.47 — above the amplitude floor, so
nothing downstream refuses it. Three of four correct; one trades an under-count
for an over-count.

## 8. STOPPED-boundary transitions — **DEMONSTRATED: clean**

27 crossings of pwm = 25 (boot3 20, boot4 1, boot5 2, boot6 2, boot7 2). Zero
accepted magnets detected within 2 s after a STOP→MOVE crossing; zero false
events; no interval anomaly at any boundary (the independent interval test found
only the two known losses, neither at a boundary).

## 9. Low-speed MOVING versus STOPPED, kept separate

**These are different populations and only one of them contains navigation.**

```
  LOW-SPEED MOVING (25 <= pwm < 60), accepted magnets: 4 of 4229
     dur 650 peak 171 pwm 40     dur 575 peak 178 pwm 46
     dur 453 peak 193 pwm 47     dur 380 peak 162 pwm 58

  STOPPED (pwm < 25), accepted magnets: 0 of 4229
     the 29 passages at pwm<25 are all NO_POSITION -- boot 3's struck period and
     the hand-push experiment. None is a navigation event.
```

**The entire low-speed moving population is four passages**, none below pwm 40,
and the raw samples I have for pwm 42 come from boot 3's offset regime. Nothing
between the tractive floor (24) and 40 was recorded at all.

## 10. Boot 5, analysed separately — **reference validity, not motion**

Boot 5 primed at 1987 while the clean shadow ran to 1875: **the reference was up
to 112 counts wrong**, which exceeds the entry margin of 70. Ordinary track
therefore reads above the entry threshold permanently.

```
  boot5: 1 agree, 2 NOT_A_MAGNET, 1 disagree, then abandoned after ~3 minutes
  11:50:57.196  pwm 88, |v|@+500 = 83   re-triggers
  11:50:57.201  pwm  0, |v|@+500 = 83   re-triggers
```

**No re-arm rule can fix this**, motion-aware or otherwise: when `|E| > ENTRY`,
the detector is triggered on bare track and there is no transition to observe.
**Labelled a reference-validity failure.** It argues for a prime sanity-check —
finding (d) in the field record already notes `[CAL] 2 s baseline — keep clear of
magnets` is a serial print, not a check — and not against motion gating.

---

## Label summary

**DEMONSTRATED**
- 6 of 9 re-triggers removed by motion gating; **all 9** removed by the
  transition rule alone (§1, §4).
- Motion gate + transition is identical to transition alone on all 52 passages (§4).
- Both recorded station dwells sat at delta ≈ 0; zero duplicates, but the
  in-field dwell case was never exercised (§2).
- 27 STOPPED-boundary crossings, zero false events, zero magnets detected within
  2 s of a departure (§3, §8).
- Zero accepted magnets below pwm 25 all day; tightest genuine case pwm 40 (§5).
- No legitimate interval below 500 ms: 4,224 intervals, minimum **719 ms** (re-verified).
- 3652 ms shelf and both strike cases are unaffected by motion state (§6, §7).
- The entry-gated re-arm budget shrinks as `|E|` grows: 70 − |E| (§1).

**UNTESTED**
- A dwell that begins *inside* a magnetic field — the case motion gating exists
  for. Neither September 15 dwell did this.
- A departure beginning inside a field.
- Coasting below pwm 25 through a magnet (§5). Margin is 15 PWM counts, n = 2 stops.
- Anything between pwm 24 and pwm 40 — no records at all.
- Whether the level oscillates across the entry threshold while parked at a fringe.
- What the 82 ms floor screens out: floor rejections publish no waveform, so the
  population cannot be replayed and decision 0085's number cannot be re-derived here.

**FAILURE**
- **Unconditional re-arm** (the rule motion gating was proposed to rescue) — 183
  events vs 71, up to 50 from one passage.
- **Motion gating alone** — leaves 3 of 9 re-triggers, including 11:18:10 at pwm 42.
- **Motion gating introduces a coasting hazard** the data cannot bound (§5).
- **11:05 merge over-counts by one** under every rule tested (§7).
- **Transient flux is not addressed** — the 3652 ms shelf reads ratio 0.43,
  inside the real-magnet band, at every window length (§6).
- **Boot 5 is unrecoverable** by any re-arm rule; it is a prime-validity fault (§10).

---

## The smallest motion-aware re-arm rule that survives every represented case

```
  ARMED  ->  candidate when |raw - ref| >= ENTRY
         ->  capture fixed window W (400 ms; 300 ms was 10 ms short for a pwm-42 passage)
         ->  classify: peak and polarity from the window
         ->  REFRACTORY

  REFRACTORY -> ARMED when BOTH:
         (1)  now >= detectionMs + 500          [existing guard, re-anchored at detection]
         (2)  at least one sample since detection had |raw - ref| < ENTRY
```

That is the whole state machine. No exit margin, no local closure baseline, no
waveform-return test, no new constant. **Condition (2) is not a closure test** —
it reuses the detection threshold and asks only "am I currently triggered?".

The PWM gate is **not** in the minimum, because condition (2) already removes
every re-trigger in the dataset and the gate adds an unbounded coasting risk. If
it is added later as defence-in-depth for the in-field dwell, it should be
conditioned on genuine rest rather than on `pwm < 25` alone — and that needs the
field test below.

## What still requires a field test

One session, instrumented but behaviourally identical to X18, that deliberately
includes:

1. **A station stop positioned so the sensor rests inside a magnet's fringe
   field.** This is the only way to exercise the case motion gating exists for,
   and the only way to decide whether the PWM gate earns its place. Park Otto
   deliberately; do not wait for it to happen.
2. **A slow section at pwm 25–40**, the band with no records at all, to test
   whether the 400 ms window still reaches the peak and whether condition (2)
   re-arms before the next magnet.
3. **A coasting approach** with the throttle cut earlier than the station machine
   normally cuts it, to find whether a genuine magnet's detection can fall below
   pwm 25.

Publish per passage: where the window would have put peak and polarity; `|raw −
ref|` at detection + 500 ms; the time from detection until the first sample below
entry; and the time above entry inside the window. Nothing changes behaviour, so
this is a measurement aperture.

**Falsified if:** any genuine magnet's detection occurs below pwm 25 while
rolling (kills the PWM gate); or condition (2) fails to re-arm before the next
magnet at pwm 25–40 (kills the transition rule at low speed); or a parked
in-field dwell produces entry crossings under condition (2) (shows the PWM gate
is required, not optional).

## Provenance

`david@192.168.68.142:~/NGR/telemetry/runs/9950011_20260915_*.log`, read-only;
the serial port was not touched. PWM at the re-arm instant interpolated from the
1 Hz alert stream (±3 s tolerance); departure behaviour from the 10 Hz
`diag/departure` records, the only raw Hall spanning a STOPPED→MOVING boundary.
Shadow readings classified clean/stale/contaminated before use; the frozen −51 of
13:21 onward is treated as stale throughout.
