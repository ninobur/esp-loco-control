# The PWM sampling gate, tested against the marker-cadence rule

**Proposal.** Keep the current rule unchanged and add one condition: collect
baseline samples only while actual PWM > 30. Skip the collection if PWM ≤ 30
when it would begin; discard it and retain the previous baseline if PWM falls to
≤ 30 during it. **No** clearing of marker history or cadence qualification, and
**no** PWM check at the close.

**Verdict.** On the recorded waveform the gate is very nearly a no-op: it changes
**one** collection in an hour, and that collection was harmless either way.
Under injected drift it is strictly harmful — it removes the only mechanism by
which the rule recovers from a long stop, and reproduces the lockout previously
attributed to the clearing mechanism.

**Source.** `xhr_20260916_191904.xhr`, session **`C3B93D0B`**, Otto (9950011),
md5 `6f78f5de8cb9116a26e62362baf1e09e`. Same held reference line and same
scenarios as [NAVI_BASELINE_DRIFT](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md).

**Reproduce.**

```bash
python3 tools/xhr_baseline_eligibility.py xhr_20260916_191904.xhr --session C3B93D0B --all
```

`R5` in `tools/xhr_baseline_eligibility.py` is the proposal, implemented as
specified: `cadence_gate=True, pwm_gate=True, pwm_at_close=False, clearing=False`.

---

## 1. Recorded waveform: one interval differs

| rule | locks | rej | p50 | p95 | p99 | max | <20 mm | min mm | stale p95 | stale max | missed | extra | min margin | 1st | 2nd |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **R1** current | 1511 | 39 | 4.0 | 9.0 | 10.0 | 13.0 | 0 | 21 | 1927 | 4662 | 1 | 4 | 126 | 0 | 0 |
| **R5** + sampling gate | 1510 | 40 | 4.0 | 9.0 | 10.0 | 13.0 | 0 | 21 | 1927 | 4662 | 1 | 4 | 126 | 0 | 0 |

Rejections: R1 `{cadence 34, spread 5}`; R5 `{cadence 34, pwm 1, spread 5}`.

**Detection is bit-identical.** Both rules produce the same 1,550 events in the
same order, with the same peak margins — minimum 126 counts against a 70-count
threshold. The gate changes nothing a magnet sees.

### The one differing interval

```
t = 2503.2 s
  R1  LOCKED   baseline 1904   error vs held line  +0.0 counts   spread 22
              prior marker interval 1929 ms, magnet span 78228 ms, mm unknown
              PWM over the 200-sample window: 0 to 0
  R5  SKIPPED  "pwm 0 at collection start"
              carried 1904 (locked at t=2423.2 s, error +1.0) for 61.3 s longer,
              until t=2564.5 s
              the line there was 1904, so the carried baseline was +0.0 counts off
```

This is the moment the locomotive came off the magnet it had stalled on — the
78-second span. R1 collected while the throttle was still at zero and got the
line exactly right. R5 skipped it and carried a baseline that was **also exactly
right**. The skip cost nothing: same detections, same margins, zero error either
way.

### Recovery after stops is identical

All 21 stops (derived causally from PWM ≤ 30 for ≥ 2 s):

| stop ends | R1 first lock | R5 first lock | R1 err | R5 err |
|---|---|---|---|---|
| 30.9 s | 36.4 s | 36.4 s | +2.0 | +2.0 |
| 1107.6 s | 1111.4 s | 1111.4 s | 0.0 | 0.0 |
| 1173.9 s | 1179.7 s | 1179.7 s | −7.5 | −7.5 |
| … 17 more … | *identical* | *identical* | | |
| 3259.1 s | 3263.2 s | 3263.2 s | +4.0 | +4.0 |

Every stop: same first lock, same instant, same error. Neither rule missed a
first or second magnet after any stop, with minimum margins of 148 and 126.

**On measured evidence the gate is neutral.** It costs one harmless lock and
changes nothing else.

---

## 2. Drift scenarios: the gate is harmful

| scenario | kind | R1 locks / max\|e\| / miss / extra | R5 locks / max\|e\| / miss / extra |
|---|---|---|---|
| flat (control) | **real** | 1511 / 13.0 / 1 / 4 | 1510 / 13.0 / 1 / 4 |
| measured shape, tiled | measured | 1511 / 13.0 / 1 / 4 | 1510 / 13.0 / 1 / 4 |
| ramp +0.08/min | measured | 1511 / 13.0 / 1 / 4 | 1510 / 13.0 / 1 / 4 |
| ramp ±1.19/min (**measured max**) | measured | 1511 / 13.0 / 1 / 4 | 1510 / 13.0 / 1 / 4 |
| step ±20 every 120 s | measured | 1506 / 13.0 / 2 / 5 | 1505 / 13.0 / 2 / 5 |
| step ±32 every 120 s | measured | 1506 / 13.0 / 2 / 64 | 1505 / 13.0 / 2 / 64 |
| ramp +3, +6/min | *stress* | 1511 / 13.0 / 1 / 4 | 1510 / 13.0 / 1 / 4 |
| ramp-hold +30, +120/min | *stress* | 1511 / 13.0 / 1 / 4 | 1510 / 13.0 / 1 / 4 |
| sine ±25 | *stress* | 1510 / 13.0 / 1 / 4 | 1509 / 13.0 / 1 / 4 |
| **measured shape ×4** | *stress* | 1325 / 13.0 / 104 / **241** | 1323 / 13.0 / 104 / **366** |
| **ramp-hold +60/min** | *stress* | 1512 / 32.0 / 1 / **42** | 1512 / 33.0 / 1 / **138** |
| **ramp +12/min** | *stress* | **1512 / 13.0 / 1 / 6** | **1431 / 13.0 / 45 / 341** |
| **step ±60 every 120 s** | *stress* | **1461 / 13.0 / 25 / 324** | **1421 / 13.0 / 47 / 1275** |
| step ±120 every 120 s | *stress* | 740 / 33.0 / 441 / 152 | 735 / 11.0 / 442 / 148 |
| step ±32 every 20 s | *stress* | 1487 / 33.0 / 2 / 87 | 1485 / 13.0 / 2 / 87 |

Across every **measured** scenario the difference is the same single lock.
Across the fast **stress** scenarios the gate costs up to 81 locks, 22 extra
missed magnets and 951 extra phantom detections.

---

## 3. Why — and a correction to the previous report

[NAVI_BASELINE_ELIGIBILITY](NAVI_BASELINE_ELIGIBILITY_20260916_C3B93D0B.md) §4
attributed the `ramp +12/min` lockout to the clearing mechanism. **That was
wrong.** Isolating the two:

| variant | locks | missed | extra | locks after t = 3100 s | magnets detected there |
|---|---|---|---|---|---|
| cadence only (R1) | 1512 | 1 | 6 | **79** | **82 / 82** |
| cadence + clearing only | 1431 | 45 | 344 | **0** | 38 / 82 |
| cadence + **PWM gate only** | 1431 | 45 | 341 | **0** | 38 / 82 |
| both | 1431 | 45 | 344 | **0** | 38 / 82 |

Either mechanism alone produces the identical lockout. Neither is uniquely to
blame: **any additional rejection condition on top of the cadence gate is enough
to trigger it**, because they all block the same rescue.

### The rescue the gate removes

During the 403-second park at t = 2759–3164 s, the injected line moves **+81
counts** while the baseline is frozen.

**R1 rescues itself.** As staleness approaches 70 counts the clean line crosses
the threshold and produces a phantom detection — three of them inside the park.
The phantom's span closes, two phantoms inside 3000 ms qualify the cadence gate,
and R1 **re-baselines while parked**, at t = 2997.3 s, to within +5.0 counts. It
enters the resume only **28 counts stale**, and detects all 82 magnets.

**R5 cannot.** PWM is 0 throughout the park, so the sampling gate skips every
one of those collections. The baseline stays frozen, staleness reaches **89
counts**, and the park fills with **313 phantom detections** instead of 3.

### Why staleness past 70 counts is fatal rather than merely inaccurate

With a frozen baseline, clean track reads `|dev| = staleness`.

| staleness | clean line reads | against a 70-count threshold |
|---|---|---|
| 40 | 40 | recoverable |
| 60 | 60 | recoverable |
| **70** | **70** | **the line itself is a departure** |
| 89 (R5 at the resume) | 89 | detector permanently open |

Once staleness reaches the threshold, the span never closes on clean track, no
collection can start, and the error can only grow. R5's 0 locks and 38-of-82
magnets for the final 780 seconds are that trap. R1 escapes because it
re-baselined before staleness reached 70.

### How far this is from measured conditions

The rescue only matters when a stop is long enough for drift to reach 70 counts.
At the **measured maximum of 1.19 counts/min**, the 403 s park accumulates
**8 counts** — nothing. Reaching 70 counts would need a park of about **59
minutes**. The `+12/min` rate that triggers this is ten times anything this
railway has been measured doing.

So the lockout is **not reachable at measured rates in this recording**. It is a
statement about the rule's failure mode, not about the field.

---

## 4. What this exposes about the current rule

The finding cuts both ways and is worth stating plainly.

R1's recovery from long-stop drift works by **re-baselining while the locomotive
is stationary, off a phantom detection caused by its own staleness**. That is
accidental, not designed. It is also in direct tension with why the cadence gate
exists: to stop the rule locking onto a magnet while stationary (the −41 count
lock, and the +130 to +243 count hazards at the Grillers and Bamboo dwells).

The cadence gate happens to permit the rescue because two phantoms inside 3000 ms
look like normal marker cadence. That is luck. The PWM sampling gate closes the
escape hatch without closing the original hazard any better than the cadence gate
already does — it prevents 1 harmless lock and 0 hazardous ones.

The principled fix for both is the one already flagged as out of scope: an
explicit **re-prime on leaving a stop**, which would make the rescue deliberate
and let the stationary hazard stay closed. Neither the sampling gate nor the
clearing mechanism is a substitute for it.

---

## 5. Recommendation

**Do not add the PWM sampling gate.**

- On the recorded waveform it changes one collection, and that collection was
  correct to 0.0 counts under both rules. Detection, margins, stale times and all
  21 stop recoveries are identical.
- Under injected drift it reproduces the lockout in full, by blocking the
  stationary re-baseline that is currently the rule's only escape from runaway
  staleness during a long stop.
- It therefore has no measured benefit and a real, if remote, failure mode.

If a movement condition on the collection is wanted for reasons outside this
data, the cadence gate already supplies one that is strictly better evidenced:
it measures the train having moved rather than the throttle having been asked,
it rejects all 20 of the zero-millimetre collections the PWM proxy admits, and
it does not close the drift escape hatch.

---

## 6. Measured versus injected

**Measured:**

- R1 and R5 differ by exactly one collection in 3,537 seconds, at t = 2503.2 s,
  with zero error either way
- identical detections (1,550 events, same order, same margins, minimum 126)
- identical recovery after all 21 stops, to the millisecond and the count
- identical across every measured-drift scenario except that same single lock

**Injected (stress rates only, five to ten times the measured maximum):**

- the lockout at `ramp +12/min` and `step ±60`
- the finding that either clearing or the PWM gate produces it independently
- the 70-count staleness cliff, which is arithmetic rather than observation

**Not established:** whether a real park long enough to reach 70 counts of
staleness ever happens. At measured rates it needs about an hour standing still,
and this session's longest park was 6.7 minutes.

---

*Analysis: `tools/xhr_baseline_eligibility.py` (rule `R5`). No firmware was
modified. X18's navigation rulings were not examined.*
