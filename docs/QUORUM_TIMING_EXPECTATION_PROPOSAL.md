# The conservation gate should measure, not predict — design proposal

**Date:** 2026-08-11 · **Status:** proposal only. Nothing implemented.
**Supersedes:** `QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md` (scope was wrong)
**Evidence:** `field-records/logs/20260811_QUORUM_1_13_beta_otto.log`,
`field-records/logs/20260810_IR_SPEED_LOCAL_1_2_otto.log`

**Recommendation: implement, behind the replay suite.** The need is demonstrated
across two sessions, two directions and two failure modes; the change removes a
constant rather than adding one, and an offline replay of the whole 2026-08-11
session catches 44/44 phantoms at a 0.13% false-reject cost.

---

## 1. The observed failure

Phantom marker events are accepted once per lap, at a fixed place, in both
directions. Each costs an odometer offset of −1 and a QUORUM incident to repair.
On 2026-08-11 that produced **16 of 18 adoptions**; on 2026-08-10 a *pair* of
them produced offset −2, which the fence cannot express, and the whole
incident-C cascade followed.

Two instances, same signature:

```
CCW, accelerating out of the Arches dwell        CW, climbing into Grillers
  mm 102  dt 1862  ratio -1.0  peak 234            mm 62  dt 2936  ratio 3.10  peak 258
  mm 101  dt  335  ratio 1.88  peak  41  ACCEPTED  mm 63  dt  617  ratio 2.02  peak  51  ACCEPTED
  mm 100  dt 1374  ratio 1.43  peak 144            mm 64  dt 2747  ratio -1.0  peak 140
```

A 335 ms event at pwm 90, and a 617 ms event at pwm 72, where neighbouring
intervals are 1 400–2 900 ms. Both admitted.

---

## 2. Why the gate cannot see them

`QUORUM.ino:1459-1492`. The test asks whether two adjacent events split one
physical interval:

```
velocity   = VEL_MODEL_SLOPE*pwmActual + VEL_MODEL_INTERCEPT     // 3.90*pwm - 99.2
expectedDt = 1000 * spacingMm[i] / velocity
reject if  |dt + previousAcceptedDt - expectedDt| <= 0.30 * expectedDt
```

The expectation comes from **PWM**. Measured against reconstructed truth, that
model was wrong by:

| condition | model | actual | error |
|---|---|---|---|
| Grillers climb, pwm 72 (mm 62) | 182 mm/s | 102 mm/s | **1.78×** |
| accelerating out of Arches, pwm 90 (mm 102) | 252 mm/s | 161 mm/s | **1.56×** |
| whole 2026-08-10 run, ACTIVE samples | — | — | +12.9% median |

When the true interval is long and the model says it should be short, a long
interval plus a short phantom sums to ~1.9 expected intervals — outside the
reject band — and the phantom is admitted. The source already states the reason:
*PWM is a request, not a result — grade, load, battery and railhead all move
what it produces.*

Low PWM is the same bug at its extreme: below pwm 25.4 the model returns a
*negative* velocity, so the gate is switched off entirely (`GATE_LOW_PWM_FLOOR`),
which is how the 2026-08-10 Bamboo pair got in.

---

## 3. The proposed change

Use the interval the locomotive **just measured** instead of the one the model
predicts:

```
expectedDt = previousAcceptedDt
```

Everything else — `DT_CONSERVE_TOL`, the sum, the tolerance — is unchanged. The
condition then reduces to:

> **reject an event that arrives within 30% of the previous accepted interval.**

`|dt + prev − prev| ≤ 0.30·prev` → `dt ≤ 0.30·prev`.

That is the whole rule. A real marker cannot arrive in a third of the time the
last one took.

### What this removes

- **the velocity model** from the acceptance path — no grade, load, battery or
  railhead sensitivity, and `VEL_MODEL_SLOPE`/`INTERCEPT` stop being safety-
  relevant (they remain for telemetry);
- **`spacingMm[]`** from the acceptance path, and with it the off-by-one on
  `conserveIntervalIndex` the source warns is worth up to 31%;
- **the low-PWM hole** — the rule needs no velocity, so `GATE_LOW_PWM_FLOOR`
  no longer has to disable the test. That closes the 10.2% of markers that
  were never conservation-tested, without a peak threshold.

**It adds no constant and no state.** `previousAcceptedDt` is already tracked.

### It also dissolves the poisoning trap

The 2026-08-11 derailment put a ~184 ms phantom interval into
`previousAcceptedDt`, after which the model-based test rejected 18 consecutive
genuine markers with no escape (§4 of the beta verdict). Under the proposed
rule a poisoned short predecessor makes the threshold *smaller*
(`0.30 × 184 = 55 ms`), so genuine markers are accepted and the predecessor
self-heals on the next acceptance. **The trap cannot form.**

---

## 4. Evidence

Offline replay of the gate over the whole 2026-08-11 session, ACTIVE-gate events
only, classifying a phantom as `dt < 700 ms and peak < 80`:

```
phantoms caught : 44 / 44
phantoms missed :  0
genuine kept    : 2278 / 2281
genuine LOST    :  3   (0.13%)
```

The three "losses" carry peak 40, 43 and 46 — below the session's 5th percentile
of 123 — so they are very likely phantoms the crude `dt < 700` classifier missed,
and the true false-reject rate may be zero.

Both recurring phantoms are caught:

```
Grillers  prev 2936, dt 617  ->  617 <= 881  REJECT
mm 101    prev 1870, dt 335  ->  335 <= 561  REJECT
mm 149/150 echoes  prev ~1200, dt ~250       REJECT
```

**Limit of this evidence, stated plainly:** this is an open-loop replay of the
*decision*, not of the stateful interaction. Changing acceptance changes every
subsequent `previousAcceptedDt`, so real behaviour will diverge from these
numbers. The figure is an indication, not a result. The result must come from
the harness with the change implemented.

---

## 5. How it could be defeated

| condition | effect | assessment |
|---|---|---|
| **genuine acceleration** | intervals shorten legitimately | The binding case. From 1 191 ms, the next interval must exceed 357 ms — a 3.3× speed increase between adjacent markers. Not reachable on this railway; the sharpest observed step is ~1.4×. Must be checked against the ramp profiles. |
| **a missed marker** | interval doubles | Makes the threshold *larger*, so more permissive — safe direction, but a phantom is then likelier to pass. Bounded: it takes two consecutive faults. |
| **first event after RAMP/LOW_PWM/dwell** | no predecessor | `NO_PREV` bootstrap already covers this and is unchanged. Note this is the remaining unguarded entry point, and it is where the 10 Aug Bamboo phantoms arrived. |
| **reversal** | direction flips mid-interval | `applyDirection()` already steps the odometer; the predecessor should be invalidated on reversal. Verify. |
| **a phantom pair arriving together** | second phantom measured against the first | Both short, so `dt ≈ prev` and the ratio is ~1.0 → rejected. Better than today. |
| **very slow running** | long intervals | Threshold scales with the interval, so it adapts. This is the whole point. |

---

## 6. Replay tests required before implementing

Beyond the existing suite passing unchanged:

1. **Both recurring phantoms rejected** — 2026-08-11 mm 101 (every lap) and
   mm 63 (two laps). Assert the adoption count drops from 18 towards 2.
2. **The 10 Aug Bamboo pair rejected**, and `syn_phantom_pair_outside_fence`
   with it — the counterfactual already shows this removes incident C entirely.
3. **No new false rejects** across both full captures; any genuine marker lost
   must be listed and justified individually.
4. **The derailment window replayed** — assert the gate does not invert and
   `previousAcceptedDt` recovers without a power cycle.
5. **`syn_ordinary_recovery` still adopts +1.** A gate that rejects too freely
   drifts toward never adopting, and this is the control that catches it.
6. **New synthetic: hard acceleration** — the fastest ramp in the station
   profiles, asserting no genuine marker is rejected.
7. **`verify_inert.py` must show the change is NOT inert** — this alters
   acceptance deliberately, so the expectation is a specific, enumerated set of
   differences, not none.

---

## 7. Recommendation

**Implement, behind the suite.** Unlike the superseded proposal this adds no
threshold to calibrate, no new state, and no dependence on a sensor scale that
moved 1.8× when a glue joint was repaired. It deletes a model from the safety
path rather than adding a compensator to it — which is the direction the
governing rule points.

It is nonetheless a change to the **acceptance** path, far more consequential
than the diagnostic advisory, and it needs its own decision record before code
is written.
