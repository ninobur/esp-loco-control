# The baseline rule under drift, steps and low speed

**Question.** The rule in [NAVI_BASELINE_TIMING_20260916_C3B93D0B.md](NAVI_BASELINE_TIMING_20260916_C3B93D0B.md)
was validated against line noise and magnet tails on an evening when the line
barely moved. Does it hold when the line moves, and when the locomotive is
barely moving?

**Source waveform.** `xhr_20260916_191904.xhr`, session **`C3B93D0B`**, Otto
(9950011), 2026-09-16. md5 `6f78f5de8cb9116a26e62362baf1e09e`. The waveform and
its event timing are **never altered**: a scenario adds `o(t)` to raw and the
same `o(t)` to the reference line, so every magnet keeps its exact shape at its
exact instant, and only the line the navigator must track moves.

**Reproduce.**

```bash
python3 tools/xhr_baseline_drift.py xhr_20260916_191904.xhr --session C3B93D0B --all
```

---

## 0. A correction to the September 16 report

That report scored locks against a plain rolling median of the raw trace. That
estimator **follows the locomotive down when it stands still on a magnet**, so
it scored the rule's worst blind spot as zero error. Re-scored against a line
that is held constant across stops, the flat control changes:

| | old yardstick | held yardstick |
|---|---|---|
| locks | 1455 | 1455 |
| p99 \|error\| | 11.5 | 11.8 |
| **max \|error\|** | **12.0** | **41.0** |
| locks worse than 20 counts | 0 | **1** |

The claim "worst error 12 counts, no lock worse than 15" was an artefact of the
yardstick. The honest figure for the September rule is **one lock 41 counts off
the line**, at t = 2271.3 s. Everything below uses the held reference. The
other conclusions of that report — where in the interval the window belongs,
and why the magnet's own ≥70 span is the right anchor — are unaffected.

---

## 1. Measured baseline histories in the repo

Searched: `tools/baseline_laps/out/`, `firmware/.../Hall_Baseline_Laps.txt`,
`field-records/logs/`.

**Used — `tools/baseline_laps/out/observations.csv`** (Otto 9950011,
2026-09-14, X16 FLOOR82 field-test sessions). `shadow_baseline` is the
locomotive's own rolling median of the line, with a `moving` flag:

| session | start | minutes | shadow range | trend (counts/min) |
|---|---|---|---|---|
| `9950011-B20260914T103024` | 10:30:24 | 9 | 202 | −0.223 |
| `9950011-B20260914T115155` | 11:51:55 | 12 | 12 | +0.724 |
| **`9950011-B20260914T120419`** | **12:04:19** | **46** | **69** | **+1.195** |
| `9950011-B20260914T125025` | 12:50:25 | 146 | 34 | +0.057 |

Most of those ranges mix *time* with *place* — the line differs by up to 47
counts around the loop, so a session that sat in one spot and then drove
elsewhere records that as "drift". **One stretch is unambiguously temporal:**
`9950011-B20260914T120419`, 12:06–12:28, where the locomotive was moving 100 %
of the time and covered the whole loop in every two-minute bin, so position
averages out.

- **+28 counts over 22 minutes = +1.30 counts/min**, linear fit R² = 0.88
- per-5 s increments: p50 0, p95 +3, max +7 counts

That is the fastest clean drift this railway has been measured doing. For
comparison, the Sept 16 evening recording moved under 6 counts in 36 minutes
(< 0.17 counts/min).

**Measured steps**, both from stops rather than from drift: −32 counts during a
79 s stationary period on Sept 16 (t ≈ 2273 s, a handling event), and −20 counts
at ≈ 14:30 in the 12:50 session.

**Found and NOT used**, with reasons:

- `field-records/logs/20260820_morning_session.log` — the `env` field spans
  **182 hours** (a broker log, not a morning) with excursions to 16275 counts.
  It is not a clean Hall line; injecting it would inject nonsense.
- `firmware/test-programs/NAVI_ONE_X18_RECORDER/Hall_Baseline_Laps.txt` — one
  session, 14:35–15:16, parked at MM40 with PWM 0, `fixed` 1949 and `shadow`
  1936 unchanged for the whole 41 minutes. No motion, so no drift rate.

**No morning history with motion was found.** Every usable record is midday or
later. Morning warm-up remains untested.

---

## 2. Scenarios

All stay inside the 12-bit ADC range on this waveform.

| scenario | kind | excursion |
|---|---|---|
| flat (control) | **real** | 0 |
| measured shape, tiled | measured | +29 |
| measured shape ×2, ×4 | *stress* | +58, +116 |
| ramp +0.08/min | measured | +5 |
| ramp ±1.19/min (**measured max**) | measured | ±70 |
| ramp +3, +6, +12/min | *stress* | +177, +354, +707 |
| ramp-hold +30, +60, +120/min | *stress* | ±30 each, repeated |
| sine ±25, 900 s | *stress* | 50 |
| step ±20, ±32 every 120 s | measured | 20, 32 |
| step ±60, ±120 every 120 s | *stress* | 60, 120 |
| step ±32 every 20 s | *stress* | 32 |

A stress pass is a stress test, not field validation.

---

## 3. Ramps are not the problem

The September rule, every ramp scenario, scored on the held reference:

| scenario | locks | max \|err\| | missed | extra |
|---|---|---|---|---|
| ramp +0.08/min (measured) | 1445 | 41.0 | 1 | 4 |
| ramp ±1.19/min (**measured max**) | 1445 | 41.0 | 1 | 4 |
| ramp +3/min (*stress*) | 1444 | 41.0 | 1 | 4 |
| ramp +6/min (*stress*) | 1443 | 41.0 | 1 | 4 |
| ramp +12/min (*stress*) | 1445 | 41.0 | 1 | 7 |
| ramp-hold +120/min (*stress*) | 1455 | 41.0 | 1 | 4 |

The 41-count maximum is **the same lock in every row** — the stationary one of
§0, which has nothing to do with drift. The ramps themselves contribute nothing
measurable.

The reason is arithmetic, not luck: the rule re-locks about **every 1.2 s**, so
even at +12 counts/min — ten times the fastest rate ever measured here — the
line moves **0.24 counts between locks**. A rule that re-baselines every marker
cannot be outrun by a ramp; it would take roughly **3,500 counts/min** before
the line moved 70 counts inside one marker interval.

**This result depends on the injected drift being smooth.** It is not evidence
about morning warm-up, only about what a smooth ramp of that rate would do.

---

## 4. Steps are the problem, and the limit is structural

| scenario | kind | locks | missed | extra |
|---|---|---|---|---|
| step ±20 every 120 s | measured | 1456 | 2 | 5 |
| step ±32 every 120 s | measured | 1453 | 2 | 5 |
| step ±60 every 120 s | *stress* | 1458 | 2 | **163** |
| step ±120 every 120 s | *stress* | **717** | **437** | **154** |
| measured shape ×4 | *stress* | 1332 | **88** | **85** |

A step of 70 counts or more **is** a 70-count departure. No baseline rule can
tell one from a magnet at the instant it arrives, because they are the same
event in the only signal the navigator has. At ±120 the rule loses 437 of 1,547
magnets. At ±60 the step itself plus the ±17-count trailing shelf crosses 70 and
produces 163 phantom detections.

Both measured step magnitudes (±20, ±32) are handled. The breaking point lies
between 32 and 60 counts — **below** the 70-count threshold, because the step
adds to the tail shelf.

The extra detections are **not at the step edges** (median distance from the
nearest edge 5.4–7.7 s). They cluster in stationary stretches, where the
navigator is correctly refusing to re-baseline and therefore carries an old
baseline across the step. That is the honest cost of refusing to lock while
stopped, and it argues for a re-prime on leaving a stop — out of scope here.

---

## 5. Spatial coverage, and the real hazard

### One collection spans about 51 mm — except when it doesn't

| phase | n | p50 (mm) | p5 | min | < 20 mm | \|err\| max |
|---|---|---|---|---|---|---|
| IDLE | 1326 | 51 | 41 | 1 | 5 | 41.0 |
| APPROACH | 78 | 47 | 33 | 28 | 0 | 12.0 |
| ZONE | 37 | 28 | 22 | 21 | 0 | 10.0 |
| ZERO_RAMP | 1 | 32 | 32 | 32 | 0 | 0.0 |
| **DEPART** | **13** | **1** | **1** | **1** | **9** | 5.0 |
| ALL | 1455 | 51 | 37 | 1 | 14 | 41.0 |

**Thirteen collections in the DEPART phase span one millimetre of track.** The
locomotive is pulling away from a station stop at a few mm/s, so 200 ms of
samples is 200 ms of *the same point on the railway*. Those windows are
perfectly steady and pass the spread test trivially. In this recording they
happened to sit on clean line, so their worst error is 5 counts. That is luck,
not the rule.

### The hazard is real, in real data

Of 21 stationary stretches longer than 2 s, **six** contain a 200 ms window with
spread ≤ 22 counts that sits far off the line:

| t start | duration | mm | held line | worst deviation | spread |
|---|---|---|---|---|---|
| 1142.0 s | 30.0 s | 62 | 1934 | **+204** | 20 |
| 1549.1 s | 30.0 s | 62 | 1935 | **+200** | 22 |
| 1761.4 s | 30.0 s | 159 | 1936 | **+160** | 17 |
| 1958.8 s | 30.0 s | 62 | 1934 | **+130** | 22 |
| 2243.8 s | 78.7 s | 39 | 1936 | **−41** | 22 |
| 2426.0 s | 132.5 s | — | 1904 | **−243** | 22 |

MM62 is Grillers and MM159 is Bamboo: the locomotive **stops on a magnet** at
those station dwells, and rests on its flat top. A spread test cannot see this
— the signal is steadier there than on open line.

Two things limit the damage:

1. **Offsets above 70 counts never close the ≥70 span**, so no collection ever
   starts. The +204, +200, +160, +130 and −243 cases are all self-protecting.
2. What gets through is therefore **bounded by the 70-count threshold**, and the
   one that did get through is the −41 at t = 2243.8 s, which produced the
   41-count lock of §0 at t = 2271.3 s, with prior marker interval **30,269 ms**
   and a window covering **2 mm**.

So the September rule's worst realistic stationary error is bounded at about
70 counts — enough to halve the detection margin on the weakest magnet (125
counts), and worth removing.

---

## 6. A motion requirement removes it

The evidence was already on the table at the moment of the bad lock: the
**previous marker interval was 30 seconds**. Rejecting a collection whose prior
marker interval exceeds 3,000 ms costs almost nothing and removes the hazard.

| gate | measured scenarios: locks | max \|err\| | locks > 20 |
|---|---|---|---|
| none (September) | 1445 | **41.0** | 2 |
| prior marker interval ≤ 3000 ms | 1431 | 28.0 | 1 |
| window covers ≥ 20 mm | 1431 | 28.0 | 1 |
| `pwm_actual` > 0 throughout | 1445 | 28.0 | 1 |

All three work on this recording. The **prior marker interval** is preferred
because it needs neither `RouteMap.h` (as the distance gate does) nor a
throttle reading, and because it measures *the train advancing* rather than
*the driver commanding*. The gate value is flat between 2,500 ms and 6,000 ms;
3,000 ms sits in the middle.

The distance gate is equivalent here and is the same quantity in different
clothes — but it needs the spacing table, so it costs more for nothing.

---

## 7. Retuning the guard and the spread limit

With the gate in place, the September constants are no longer the best choice.

**Guard 40 ms leaves residual shelf in the window**, which inflates the spread
and causes rejections. Tightening the spread limit at a 40 ms guard starves
part of the route:

| rule | locks | markers never locked | MM65–85 locks/marker |
|---|---|---|---|
| spread 40, guard 40 | 1441 | 0 | 8.5 |
| **spread 24, guard 40** | **1048** | **6** (66, 67, 105, 138, 140, 157) | **4.5** |
| spread 24, guard 80 | 1436 | 0 | 8.5 |
| spread 32, guard 80 | 1512 | 0 | 9.0 |

Moving the guard to 80 ms buys back everything the tighter spread test costs —
and MM65–85 is the Grillers grade, where the locomotive is slowest and the tail
longest, so starving it would be starving the worst place on the railway.

Measured scenarios, worst case across all of them:

| variant | locks | rejected | p99 \|err\| | max \|err\| | > 20 |
|---|---|---|---|---|---|
| September: guard 40, N 200, spread 40 | 1445 | 105 | 11.8 | **41.0** | 2 |
| **guard 80, N 200, spread 32, gate** | **1507** | **102** | **11.8** | **13.0** | **0** |
| drop the spread test | 1517 | 91 | — | 40.0 | 1 |
| drop the motion gate | 1528 | 22 | — | 41.0 | 1 |
| N 200 → 150 | 1472 | 367 | — | 14.0 | 0 |
| N 200 → 100 | 1489 | 168 | — | 15.0 | 0 |
| guard 80 → 40 | 1299 | 289 | — | 12.1 | 0 |
| guard 80 → 120 | 1494 | 178 | — | 12.0 | 0 |
| spread 32 → 24 | 1405 | 176 | — | 13.0 | 0 |

**Every mechanism earns its place.** Dropping the spread test costs 40 counts of
worst-case error; dropping the motion gate costs 41; shortening the collection
costs accuracy and rejections both. The recommended rule is the smallest one in
which nothing can be removed without a measurable loss.

---

## 8. Recommended rule

The September rule, **plus one motion requirement**, with two constants moved.

```
STATE:  baseline           int16, primed at boot from a 2 s median
        lastOpenMs         when the previous magnet opened (0 = none yet)

EVERY 1 ms SAMPLE:
  dev = raw - baseline

  IDLE / GUARD / COLLECT:
      if |dev| >= 70 for 5 consecutive samples:
          priorInterval = now - lastOpenMs ;  lastOpenMs = now
          -> OPEN   (abandon any collection; KEEP the existing baseline)

  OPEN:
      when |dev| < 70 has held for 30 ms:
          span closed at the last over-threshold sample
          if priorInterval > 3000 ms:  -> IDLE        <-- NEW: motion required
          else:                        -> GUARD until close + 80 ms

  GUARD:   at close + 80 ms -> COLLECT

  COLLECT:
      append raw
      at 200 samples:
          if max(buf) - min(buf) <= 32:  baseline = median(buf)   <-- valid here
          else:                          keep the old baseline
          -> IDLE
```

| constant | September | now | why it moved |
|---|---|---|---|
| guard after close | 40 ms | **80 ms** | 40 ms leaves shelf in the window; 80 ms restores full route coverage under a tighter spread test |
| samples | 200 | 200 | unchanged; 150 and 100 both lose accuracy |
| spread limit | 40 | **32** | with an 80 ms guard it costs no locks and catches step contamination |
| motion requirement | — | **prior marker interval ≤ 3000 ms** | removes the 41-count stationary lock and the 1 mm collections |

### What it does on the real waveform

| | September | recommended |
|---|---|---|
| locks | 1455 | **1512** |
| rejected collections | 95 | **38** |
| max \|baseline error\| | 41.0 | **13.0** |
| locks worse than 20 counts | 1 | **0** |
| collections spanning < 20 mm | 14 | **1** |
| DEPART-phase collections at 1 mm | 9 | **0** |
| carried-baseline time, p50 / max | 1188 / 4893 ms | 1197 / 4662 ms |
| detection timing vs offline, p1 / p50 / p99 | −11 / 0 / +9 ms | −11 / 0 / +8 ms |
| earliest detection | −55 ms | −77 ms |
| **smallest detection margin** | 124 counts | **126 counts** |
| magnets detected of 1,547 | 1,546 | 1,546 |

Worst individual locks under the recommended rule, flat control:

| t | phase | error | spread | span | prior interval | pwm min |
|---|---|---|---|---|---|---|
| 68.9 s | IDLE | −13.0 | 19 | 153 ms | 1571 ms | 90 |
| 2673.2 s | IDLE | −12.0 | 12 | 131 ms | 1588 ms | 90 |
| 279.5 s | IDLE | −11.5 | 15 | 156 ms | 1501 ms | 90 |
| 1212.4 s | IDLE | −11.0 | 23 | 111 ms | 1212 ms | 90 |
| 693.2 s | IDLE | −11.0 | 22 | 98 ms | 1298 ms | 90 |

All five are ordinary running at full throttle, all are the trailing shelf, and
none is a motion or contamination failure. The rule's residual error is now the
physics of the magnet, which is where it should be.

---

## 9. What is demonstrated and what is injected

**Demonstrated by real measurement** (the Sept 16 waveform, unmodified):

- the stationary tail-lock hazard: 6 of 21 stationary stretches carry a
  low-spread 200 ms window 41 to 243 counts off the line, four of them at
  station dwells on Grillers and Bamboo
- the September rule takes one such lock, 41 counts off, with 30 s since the
  last marker and a 2 mm window
- 13 DEPART-phase collections spanning 1 mm of track
- the guard/spread retuning, and the route coverage it protects
- that the prior marker interval was sufficient evidence at the moment of every
  bad lock
- that no magnet is lost and the smallest detection margin is 126 counts

**Depends entirely on the injected offsets** (proves nothing about the field):

- everything in §3 and §4 — ramps, steps, and the breaking point between 32 and
  60 counts
- the claim that the rule cannot be outrun by a ramp below ~3,500 counts/min

**Still not established by anything here:**

- **morning warm-up.** No repo record combines morning hours with motion. The
  fastest measured drift is a midday one, and it is smooth; a warm-up transient
  may not be.
- **stops and direction changes.** The rule now refuses to re-baseline across
  them, which is correct but leaves the baseline stale on departure. §4 shows
  that a line step during a stop then produces phantom detections. A re-prime
  on leaving a stop is the obvious next question and is not answered here.
- **whether real drift is smooth.** Every ramp scenario assumes it is. The one
  measured trajectory has 5 s increments up to 7 counts, which the tiled
  scenario reproduces; nothing says a warm-up looks like that.
- **CCW at CW's confidence**, and **other locomotives**, unchanged from the
  September report.

---

*Analysis: `tools/xhr_baseline_drift.py`, building on `tools/xhr_baseline_timing.py`.
No firmware was modified. X18's navigation rulings were not examined.*
