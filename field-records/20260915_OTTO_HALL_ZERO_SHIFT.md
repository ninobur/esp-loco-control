# Otto — three strikes, one mechanism: the Hall zero moves

**2026-09-15.** Otto, 9950011, `NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST`
(`baseline_mode: set_location_lap_cap2`). Written live from telemetry; the
locomotive is stopped at MM031 as this is filed. Nothing here is committed
firmware policy.

---

## 1. What happened

Three in-motion strikes, all `POLARITY_MISMATCH`, all the same chain.

| | offset at the time | what was lost | slipped inside |
|---|---|---|---|
| 11:05:20 MM150 | **−33** (reference low) | 1,852 ms **merge** | MM151–153, three N |
| 12:06:24 MM013 | **−33** (reference low) | 1,443 ms **merge** | MM011–014, four N |
| 13:21:03 MM031 | **+18** (reference high) | 76 ms **floor rejection** | MM030–032, three N |

Two further strikes at 11:29 and 11:30 were the operator hand-pushing the car
after a declaration; the navigator judged them correctly and they are not
faults.

**The chain, every time:**

1. the operative reference and the resting Hall level part company by ~20–35 counts;
2. inside a run of same-pole magnets, one advance is lost —
   * reference **low**: the field between two like poles never returns inside the
     25-count exit window, so two magnets merge into one passage;
   * reference **high**: the passage is starved, falls under the 82 ms floor, and
     is discarded;
3. the count is now one short, and stays invisible for as long as the same-pole
   run lasts, because every remaining marker in it still matches;
4. at the first pole change the observed polarity contradicts the expected one
   and the navigator strikes.

The 13:21 strike was **predicted eight seconds before it happened** from the
floor rejection at 13:20:55, and the route map confirmed it afterwards: MM030,
MM031, MM032 are North and MM033 is South.

The stopped locomotive gives an independent check. The firmware believes MM031
(North); the frozen shadow reads 51 counts **negative**, which is a South magnet
under the sensor. MM031 + 2 = MM033, South. The slip is real and it is one marker.

---

## 2. It is the sensor's zero, not the railway

Accepted passages this afternoon, split by pole and by
`E = level − operative reference`:

```
pole  offset band    n    peak med   dur med   dur p90
 N    E <= +4       744      173       146       169
 N    E >= +12      119      187  +14  168  +22  196
 S    E <= +4       714      174       141       165
 S    E >= +12      128      158  -16  125  -16  147
```

North gains 14 counts, South loses 16, **around an unchanged true amplitude of
~173**. Equal and opposite about a constant `A` is the signature of a DC shift of
the whole signal — `peak = A + E` for North, `A − E` for South. Had the field or
the sensor gain changed, both poles would have moved the same way.

**The magnets are constant. The zero moves.**

### Ruled out, with the measurement that ruled it out

| candidate | evidence against |
|---|---|
| track geography | the high state appears in all twelve MM buckets; MM068 and MM151–156, "hot" at 11:05, were ordinary two hours later |
| motor load | 0.29 A at `E ≤ +5`, 0.30 A at `E ≥ +25` — flat |
| supply voltage | 15.38–15.49 V, median 15.44 at every offset band |
| uptime | run 1 ramped at minute 11; run 2 sat flat through minute 28 and ramped later |
| distance from prime to equilibrium | run 2 primed 1942, held 1942–1952 for half an hour, then went to 1968 anyway |
| a thermal soak | soaks decelerate; run 1 went 0, +3, +11, +22 counts per lap — accelerating |

### What it looks like instead

Two loosely-held states, ~1946 and ~1968, with transitions inside a single 1 Hz
sample:

```
12:49:13 MM022  1946
12:49:17 MM026  1946
12:49:21 MM029  1968     <- one sample
12:49:25 MM032  1969
```

and back down again 24 minutes later, 1972 → 1942 in about two minutes. Over the
whole run the high state has sd 3.5 (range 1964–1987) and the low state sd 5.1
(range 1936–1959), so the two are not cleanly separated — "bistable" overstates
it. What is solid is that the level makes 20–30 count excursions on a timescale
of seconds to minutes, reverses, and is not locked to position.

**1968 recurred as the high-state mode on three separate boots with three
different primes.** That is the only repeated absolute number of the day and it
may be worth a scope.

---

## 3. Why no lap-rate controller can answer this

X18 moves the reference at most 2 counts per completed lap. A lap is ~229 s, so
its ceiling is **0.52 counts/minute**. Measured excursion rates today: **3.6 to
7.2 counts/minute**, sustained for minutes.

The arithmetic from the run's own telemetry:

```
lap  9  requested  +7   applied +2
lap 10  requested +19   applied +2     gap opened in ~5 min; 10 laps (35 min) to close
lap 12  requested +13   applied +2
lap 19  requested -29   applied -2     gap opened in ~2 min; 15 laps (52 min) to unwind
```

Twenty laps completed, every one `valid`, `coverage 171/171`. The estimator is
not the problem — lap 10's estimate came back at 1969 against an instantaneous
1969. **The authority is.**

Worse, a bounded tracker chasing a signal that reverses is *guaranteed* to be
stranded by whichever transition it is part-way through. At 13:13 the controller
had spent six laps faithfully climbing to the high state; the level then dropped
to the low one, leaving the reference 26 counts **above** it — past the exit
margin, on the opposite sign, with no fault of its own.

This retires the question yesterday's sweep was built to answer. Choosing
between caps of 1, 2, 3 and 4, or between persistence thresholds, is choosing
between rates that are all one to two orders of magnitude too slow. See
`docs/NAVI_BASELINE_CONTROLLER_REPLAY_20260914.md` §7, whose recommendation
rested on an assumption — that the drift is slow and monotonic — which today
falsified.

---

## 4. X18 code review

CODEX is unavailable until 2026-09-17; this stands in.

**a. `openMigrateMs` expiry now feeds the reference.** `HallCapture.h:353-358`
stops updating the rolling median while a passage is open — but only for
`openMigrateMs` (2000 ms). Past that it resumes, taking samples from *inside* the
open magnet. Measured three times today:

| time | delta reached | passage open |
|---|---:|---:|
| 11:12:02 | +65 | 3,652 ms |
| 11:29:42 | +97 | long |
| 11:32:05 | **−134** | 13,126 ms (hand-pushed) |

Under X17 this was harmless — `fixedAfterPrime` meant the corrupted value was
shadow telemetry with no authority. **X18 gave that median a path to the
operative reference** (`NAVI_ONE.ino:1283`, `lapBaseline.advance(ns.navMm,
j.shadowBaseline, j.closeBaseline)`) and left the expiry in place. It is now the
only route by which a magnet can become the baseline. The expiry existed because
the rolling median used to be the only way to recover from drift; X18 built a
lawful path for that, so under `fixedAfterPrime` the guard can be
unconditional.

**b. `adjustBaseline()` has the pre-roll backwards.** `HallCapture.h:102`
subtracts `delta` from `pre_[]`, commented "already expressed relative to the old
baseline". `HallCapture.h:154`, the only other write site, says the opposite in
its own comment: `pre_[preHead_] = raw;` — **RAW, not delta**. After a `+2`
correction the pre-roll replays at `raw − B − 4`. It also indexes `pre_[0..preLen_)`
instead of the ring order used at line 178.

*Downgraded from my first assessment.* At `delta = 1` this puts 12 samples one
count low against a `sum_` of ~6000, and the ±2 cap means it can never receive a
delta large enough to matter. Observed live: the eight passages after the first
real adjustment were clean (peaks 171–192, ratios 0.94–1.06, 139–157 ms). It is a
correctness defect and the comment is wrong, but it is not a hazard.

**c. Gate 13 cannot see either.** `tests/gate_lap_baseline.cpp:8` passes the same
`value` for all 171 positions, so `u.estimate == 1960` would pass for a median, a
mean, first-value or last-value-wins. It cannot detect one poisoned vote, let
alone a run of them. All 13 gates pass.

**d. Priming is unguarded.** The exemption at `HallCapture.h:341` is correct and
necessary — the locomotive is stationary at boot — but nothing sanity-checks the
result. Today's primes: 1935, 2006, 1903, 1942. The 2006 boot failed on its first
magnet after each of two declarations; it had primed on a magnet. `[CAL] 2 s
baseline — keep clear of magnets` is a serial print, not a check.

---

## 5. What I would do next, and what I would not

**Not** tune the cap. Every candidate in the 755-candidate sweep is too slow by
more than an order of magnitude, and the sweep's premise is falsified.

**Find the zero shift on the bench.** It is inside the power car: not position,
not load, not supply, and it leaves magnet amplitude untouched. A scope on the
Hall output and on its supply/reference rail, with the car stationary and the
controller idle, for long enough to catch one 20-count step. If the step appears
with the car sitting still and the motor disconnected, the railway is exonerated
entirely.

**Then** decide the reference policy, because the answer depends on what is
found. If the shift is a fault that can be fixed, a slow bounded controller is
appropriate and X18 is close to right. If it is intrinsic, no lap-rate mechanism
suffices and the architecture has to change.

**Interim, if Otto must run:** it managed 20 valid laps and 90 minutes today,
which is the best X18 has done — but it ended the same way as every other run.
Nothing measured today suggests a parameter that would have prevented it.

---

## 6. Provenance

Source: `~/ngr-telemetry/pi/NGR/telemetry/runs/9950011_20260915_*.log`, live off
192.168.68.142. Route polarity from `RouteMap.h`. Passage statistics computed
over moving samples with `pwm > NAVI_BASELINE_ADAPT_PWM` and a known position;
the exclusion rules and their justification are in
`docs/NAVI_BASELINE_LAP_DATASET_20260914.md` §4.

Corrections made in the course of the day, recorded so they are not repeated:
"something physical changed at MM151–156" (falsified two hours later at the same
markers); "this is yesterday's warm-up curve" (yesterday's was linear, today's
accelerated); "the level gets quieter in the high state" (a 50-second artefact,
sd 3.5 vs 5.1 over the full sample); "spurious openings are offset-driven" (all
four fragments were North regardless of the offset's sign).
