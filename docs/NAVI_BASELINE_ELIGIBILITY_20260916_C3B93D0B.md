# Movement eligibility for baseline acquisition: four rules compared

**Question.** Should the baseline rule gate on actual PWM as well as marker
cadence, with a loss-of-eligibility mechanism that freezes the baseline and
requires two magnets to requalify?

**Answer.** No. On this session the PWM clause changes exactly one lock — it
rejects a good one — and the loss-of-eligibility mechanism introduces a lockout
failure mode from which the current rule recovers. The simplest rule supported
by the results is **the marker-cadence gate alone**.

**Source.** `xhr_20260916_191904.xhr`, session **`C3B93D0B`**, Otto (9950011),
2026-09-16, md5 `6f78f5de8cb9116a26e62362baf1e09e`. 3,537,200 samples,
1,547 magnets, 21 stops. Scored against the held reference line of
[NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md).

**Reproduce.**

```bash
python3 tools/xhr_baseline_eligibility.py xhr_20260916_191904.xhr --session C3B93D0B --all
```

---

## 1. The four rules

Raw-signal timing is **identical in all four** and unchanged: departure at 70
counts sustained 5 samples, close after 30 ms below 70, 80 ms guard, 200 raw
samples, accept the median only if max − min ≤ 32.

| | cadence gate ≤3000 ms | PWM > 30 at start and throughout | loss-of-eligibility clearing |
|---|---|---|---|
| **R1** current | ✓ | | |
| **R2** PWM alone | | ✓ | ✓ |
| **R3** cadence + clearing | ✓ | | ✓ |
| **R4** combined | ✓ | ✓ | ✓ |

*Clearing* is implemented as specified: PWM ≤ 30 or a direction change aborts
any collection, freezes the last valid baseline, and clears the cadence
qualification, so that after a resume the first magnet only re-arms the clock
and a **second** magnet is needed to establish a qualifying interval.

The replay sees the raw Hall value, actual PWM and the motor direction flag —
nothing else, and nothing from the future. X18's `st_phase` and `nav_mm` appear
only as labels in the breakout tables, never in a decision.

---

## 2. Measured outcome on the real waveform

No injected offset. Errors in counts, distances in mm, times in ms.

| rule | locks | rej | p50 | p95 | p99 | **max** | **<20 mm** | min mm | stale p95 | stale max | missed | extra | min margin | 1st | 2nd |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **R1** current | 1511 | 39 | 4.0 | 9.0 | 10.0 | **13.0** | **0** | 21 | 1927 | 4662 | 1 | 4 | **126** | 0 | 0 |
| **R2** PWM alone | 1530 | 20 | 4.0 | 9.0 | 10.0 | 13.0 | **20** | **0** | 1971 | 4662 | 1 | 4 | 127 | 0 | 0 |
| **R3** cadence+clearing | 1510 | 40 | 4.0 | 9.0 | 10.0 | 13.0 | 0 | 21 | 1927 | 4662 | 1 | 4 | 126 | 0 | 0 |
| **R4** combined | 1510 | 40 | 4.0 | 9.0 | 10.0 | 13.0 | 0 | 21 | 1927 | 4662 | 1 | 4 | 126 | 0 | 0 |

Rejection reasons:

| rule | cadence | pwm | eligibility | spread |
|---|---|---|---|---|
| R1 | 34 | 0 | 0 | 5 |
| R2 | 0 | 6 | 0 | 14 |
| R3 | 34 | 0 | 1 | 5 |
| R4 | 34 | 1 | 0 | 5 |

**All four are identical on baseline accuracy** (p50 4, p95 9, p99 10, max 13)
and on detection (1 unmatched, 4 extra, minimum margin 126 counts against a
70-count threshold). The 1 unmatched and 4 extra are the same ones identified
previously: a boundary shift at the stall onset and three re-triggers while
creeping over a magnet at Grillers and Bamboo.

### The entire difference between R1 and R4 is one lock

```
R1 locked at t=2503.2 s   error +0.0 counts   spread 22   prior interval 1929 ms
R4 rejected it:           "pwm 0 at close"
```

That is the lock taken after the 78-second magnet span, when the locomotive had
come off the magnet but the throttle was still at zero. It was **exactly
correct** — zero error against the held line. The PWM clause's only effect in
the entire hour is to throw it away.

### Breakout by phase

Labels from X18's `st_phase`, never used in a decision.

| rule | steady | approach | zone | zero ramp | dwell | departure |
|---|---|---|---|---|---|---|
| R1 locks | 1338 | 80 | 74 | 17 | 0 | 2 |
| R2 locks | 1346 | 80 | 74 | 17 | 0 | **13** |
| R3 / R4 locks | 1337 | 80 | 74 | 17 | 0 | 2 |
| max \|err\| (all rules) | 13.0 | 11.0 | 10.0 | 8.0 | — | 6.0 |
| **< 20 mm, R2 only** | **9** | 0 | 0 | 0 | — | **11** |

No rule ever locks during dwell. **R2 is the only rule that accepts collections
covering no track at all.**

### First and second magnet after each of the 21 stops

Stops derived from PWM ≤ 30 for ≥ 2 s — causally, not from X18.

| rule | 1st missed | min margin | 2nd missed | min margin |
|---|---|---|---|---|
| R1 | **0** | 148 | **0** | 126 |
| R2 | 0 | 148 | 0 | 137 |
| R3 | 0 | 148 | 0 | 126 |
| R4 | 0 | 148 | 0 | 126 |

**Carrying the baseline through a stop costs nothing here.** Every first magnet
after every stop was detected, arriving 1.0 to 4.1 s after the stop ended, with
margins of 148 to 240 counts. No rule is better or worse.

---

## 3. PWM is evidence of drive, not proof of displacement

### It does persist while Otto is not advancing

| threshold | stretches with no magnet for >3 s | total | longest |
|---|---|---|---|
| PWM > 24 | 35 | 85.4 s | 5.0 s |
| **PWM > 30** | **28** | **60.7 s** | **4.1 s** |
| PWM > 40 | 23 | 38.5 s | 3.2 s |

So yes — the answer to the question as posed is that PWM > 30 persists for up
to 4.1 s at a time while the locomotive produces no markers.

### But it does not admit a contaminated baseline here

At the places where this happens, the locomotive is resting **on** a magnet:

| t | mm | PWM | offset from line | spread | can a collection start? |
|---|---|---|---|---|---|
| 1173.9 s | 62 | 91 | **+203** | 13 | no — offset ≥70 keeps the span open |
| 1539.8 s | 62 | 47 | **+200** | 19 | no — span stays open |
| 1581.0 s | 62 | 92 | **+204** | 24 | no — span stays open |
| 1793.3 s | 159 | 81 | **+160** | 17 | no — span stays open |
| 1990.7 s | 62 | 92 | **+130** | 4 | no — span stays open |
| 2188.4 s | 160 | 58 | −11 | 15 | yes, and harmless |

The protection is structural, not lucky: an offset of 70 counts or more never
closes the ≥70 span, so no collection can begin there. What can get through is
bounded by the threshold, and everything under 70 in this session is within 11
counts of the line.

### The threshold is a dead knob

PWM at each collection start, across 1,511 collections:

| 0–20 | 21–24 | 25–30 | 31–40 | 41–50 | >50 |
|---|---|---|---|---|---|
| 1 | **0** | **0** | **0** | 7 | 1503 |

It is bimodal — either zero or well above 50. Nothing sits in the band the
threshold would discriminate, so moving it changes nothing:

| PWM > | R4 locks | rej | max \|err\| | <20 mm | | R2 locks | rej | <20 mm | min mm |
|---|---|---|---|---|---|---|---|---|---|
| 20 | 1510 | 40 | 13.0 | 0 | | 1530 | 20 | 20 | 0 |
| 24 | 1510 | 40 | 13.0 | 0 | | 1530 | 20 | 20 | 0 |
| **30** | 1510 | 40 | 13.0 | 0 | | 1530 | 20 | 20 | 0 |
| 40 | 1510 | 40 | 13.0 | 0 | | 1530 | 20 | 20 | 0 |
| 50 | 1499 | 51 | 13.0 | 0 | | 1519 | 31 | 20 | 0 |

**Identical from 20 to 40.** And no threshold fixes R2's 20 zero-millimetre
collections, because at those moments PWM was **49 to 99** — the throttle was
already well up while the train had barely moved:

| t | phase | coverage | PWM during collection |
|---|---|---|---|
| 1178.2 s | departure | 0.0 mm | 95–98 |
| 1585.3 s | departure | 0.0 mm | 96–99 |
| 1995.0 s | departure | 0.0 mm | 95–99 |
| 1796.9 s | steady | 0.0 mm | 85–88 |
| … 16 more | | 0.0 mm | 49–99 |

This is the clearest available demonstration of the premise in the request. The
marker-cadence gate rejects every one of these, because it measures the train
having moved rather than the driver having asked.

---

## 4. Where the combined rule is worse

### Measured scenarios: no difference

| rule | locks | rej | max \|err\| | >20 | <20 mm | missed | extra | min margin | 1st/2nd |
|---|---|---|---|---|---|---|---|---|---|
| R1 | 1506 | 103 | 13.0 | 0 | **0** | 2 | 64 | 73 | 0/0 |
| R2 | 1524 | 85 | 13.0 | 0 | **21** | 2 | 64 | 73 | 0/0 |
| R3 | 1505 | 104 | 13.0 | 0 | 0 | 2 | 64 | 73 | 0/0 |
| R4 | 1505 | 104 | 13.0 | 0 | 0 | 2 | 64 | 73 | 0/0 |

### Stress scenarios: the clearing mechanism can lock itself out

| rule | locks | rej | max \|err\| | >20 | missed | extra |
|---|---|---|---|---|---|---|
| R1 | 1325 | 384 | 33.0 | 1 | 104 | 324 |
| R2 | 1337 | 1337 | 33.0 | 1 | 104 | 1272 |
| R3 | 1323 | **1356** | **13.0** | **0** | 104 | **1276** |
| R4 | 1323 | **1356** | **13.0** | **0** | 104 | **1276** |

The clearing rules buy a better worst-case baseline error (33 → 13) and pay for
it with 3.5× the rejections and 4× the phantom detections. Scenario by scenario,
R4 is worse than R1 in:

| scenario | kind | R1 locks / missed / extra | R4 locks / missed / extra |
|---|---|---|---|
| measured shape ×4 | *stress* | 1325 / 104 / 241 | 1323 / 104 / **366** |
| **ramp +12/min** | *stress* | 1512 / **1** / **6** | **1431 / 45 / 344** |
| ramp-hold +60/min | *stress* | 1512 / 1 / 42 | 1512 / 1 / **140** |
| **step ±60 every 120 s** | *stress* | 1461 / **25** / **324** | **1420 / 47 / 1276** |
| all measured scenarios | measured | — | 1 fewer lock, nothing else |

### The lockout, in detail

Under `ramp +12/min`, after the 403-second park that ends at t = 3164 s:

| | R1 | R4 |
|---|---|---|
| locks after t = 3100 s | **79** | **0** |
| magnets detected there | **82 / 82** | **38 / 82** |
| longest gap with no lock | 240 s | **780 s — to the end of the session** |

The mechanism:

1. During the 403 s park the injected line moves **80 counts**. Both rules
   correctly refuse to re-baseline, so both carry a baseline 80 counts stale.
2. A baseline 80 counts stale makes magnets of the opposing polarity read
   125 − 80 = 45 counts. They fall under the 70-count threshold and vanish.
   Both rules now see only about half the magnets.
3. **R1 recovers.** Its cadence clock survives the stop. The first magnet after
   the park carries a 403 s interval and is rejected; the second carries a
   normal interval, qualifies, and re-locks.
4. **R3 and R4 do not.** Clearing fires on *every* sample with PWM ≤ 30, and in
   that final station-heavy stretch PWM is ≤ 30 for **49.8 %** of samples. The
   qualification is wiped again before two qualifying magnets can accumulate. The
   baseline never refreshes, more magnets vanish, and the rule never recovers —
   `collision` rejections (49) and `cadence` rejections (16) for the rest of the
   run.

This is a **positive-feedback failure**: the mechanism meant to protect the
baseline prevents the recovery that would fix it. R1's single retained piece of
state — the cadence clock kept across the stop — is exactly what breaks the loop.

The threshold is quantifiable: at `ramp +6/min` (40 counts accumulated over the
same park) both rules are fine; at `+12/min` (80 counts) only R1 survives. Both
are stress rates — ten times and five times the fastest measured drift — so this
is a fragility, not an observed failure.

---

## 5. Recommendation

**Keep R1: the marker-cadence gate alone.** Do not add the PWM clause, and do
not add PWM-triggered clearing.

```
STATE:  baseline           int16, primed at boot from a 2 s median
        lastOpenMs         when the previous magnet opened

EVERY 1 ms SAMPLE:
  dev = raw - baseline
  IDLE / GUARD / COLLECT:
      if |dev| >= 70 for 5 consecutive samples:
          priorInterval = now - lastOpenMs ;  lastOpenMs = now
          -> OPEN   (abandon any collection; KEEP the existing baseline)
  OPEN:
      when |dev| < 70 has held for 30 ms:
          if priorInterval > 3000 ms:  -> IDLE
          else:                        -> GUARD until close + 80 ms
  GUARD:   at close + 80 ms -> COLLECT
  COLLECT: append raw; at 200 samples:
               if max(buf) - min(buf) <= 32:  baseline = median(buf)
               -> IDLE
```

Grounds, in order of weight:

1. **The PWM clause changes one lock in an hour, and that lock was correct**
   (error +0.0). It cannot be justified on measured evidence.
2. **The PWM threshold is not a tuning parameter.** Identical results from 20 to
   40, because PWM at a collection instant is bimodal.
3. **The PWM proxy alone is strictly worse** — 20 collections covering 0 mm,
   taken at PWM 49–99. Drive is not displacement, exactly as suspected.
4. **The clearing mechanism can lock itself out** under fast drift, where the
   cadence gate alone recovers within two markers.
5. R1 is the smallest: one comparison, one constant, one piece of retained state.

**One qualification.** The direction-change half of the clearing rule is
**untested**. The motor direction flag is FWD for 100 % of this session, with
zero transitions; X18's `nav_dir` flips once at t ≈ 2307 s, but that happened
inside a 79 s stop when the locomotive was physically turned, not commanded to
reverse. If commanded reversals occur in service, clearing the cadence
qualification on a direction change is cheap and principled — a reversal
invalidates the interval's meaning — and it carries none of the lockout risk,
which comes entirely from the PWM-triggered clearing firing hundreds of times per
session. Adding *that* clause alone would be free on this recording (it never
fires) and is the one part of the proposal worth keeping.

---

## 6. Measured versus injected

**Measured on the unmodified waveform:**

- all four rules identical on baseline error (p50 4, p95 9, p99 10, max 13) and
  on detection (min margin 126 counts)
- the PWM clause's only effect: one rejected good lock at t = 2503.2 s
- PWM > 30 persists up to 4.1 s while no markers arrive, in 28 stretches
- at every such place the offset is ≥70 counts, so the span protection holds and
  no contaminated baseline is admitted
- PWM at a collection start is bimodal; thresholds 20–40 are indistinguishable
- PWM alone accepts 20 collections covering 0.0 mm at PWM 49–99
- no rule misses the first or second magnet after any of the 21 stops

**Depends entirely on the injected offsets:**

- everything in §4 — including the lockout, which appears only at stress rates
  (+12/min, ±60-count steps) five to ten times beyond anything measured
- the 40-vs-80-count threshold for recovery after a long stop

**Still not established:**

- **commanded reversals** — none in this session, so the direction clause is
  untested
- **morning drift**, unchanged from the previous report
- whether a lockout like §4's can occur at measured drift rates during a park
  longer than the 403 s seen here; the arithmetic says a park would need roughly
  **70 minutes** at the measured maximum of 1.19 counts/min to accumulate the
  80 counts that triggered it

---

*Analysis: `tools/xhr_baseline_eligibility.py`, building on
`tools/xhr_baseline_timing.py` and `tools/xhr_baseline_drift.py`.
No firmware was modified. X18's navigation rulings were not examined.*
