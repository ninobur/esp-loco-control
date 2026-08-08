# Reply to CODEX — QUORUM 1.8 review, answered from the field

Date: 2026-08-08
Reviewer: CODEX. Review text recorded verbatim in
`QUORUM_1_8_REVIEW_FINDINGS.md` Appendix B.
Status of the review's condition: **met.**

CODEX made ratification conditional on the pre-fix reproduction confirming
the mechanism. **It confirmed it.** All four recommendations were adopted
*before* flashing, and the field programme then exercised every one of
them. This document answers each point with the resulting data.

Decision 0017 is now **Accepted**
(`docs/decisions/0017-hall-baseline-adapts-only-in-motion.md`). QUORUM 1.8
is flashed and field-validated.

Evidence: `field-records/logs/20260807_A1_prefix-magnet-park.log`,
`…_C_acceptance_1_8.log`, `…_C_acceptance_1_8_part2.log`.
Verdicts: `QUORUM_1_8_STAGE_A_VERDICT.md`,
`QUORUM_1_8_STAGE_C_VERDICT.md`.

---

## The condition: pre-fix reproduction — PASSED

On the un-gated 1.7, parked on an S magnet at PWM 0 with a healthy
baseline of 2019:

```
11:54:44  base=2018  raw=1889  d=-129   12 s parked — reference holding
11:55:00  base=2016  raw=1889  d=-127   28 s — still holding
11:55:01  base=2005                     ← MEDIAN CROSSES OVER
11:55:03  base=1970
11:55:05  base=1892  raw=1889  d=  -3   reference has BECOME the magnet
```

**2019 → 1892 in four seconds, after a 29-second stationary dwell.**
Migration 129 counts against a ±38 entry threshold — 3.4×. Crossover began
at 29 s and completed by 33 s, inside the predicted 32–64 s window derived
from the 128 × 500 ms model. The step shape is a median's, not an
average's: nothing moves until magnet samples take the middle of the
window, then it tips.

CODEX's causal chain is confirmed in all four links, with the third
directly observable: `recomputeThresholds()` re-centred the detection
window on the migrated reference, and the departure disturbance followed.

---

## 1. Decision wording — adopted

0017 now reads: *"The Hall median baseline adapts **only while the
locomotive is believed to have tractive motion**
(`actualPwm > MOTOR_DEAD_ZONE_PWM`)."* The spec carries the same
correction and states the asymmetry explicitly — refusing to adapt while
coasting postpones adaptation; adapting while parked destroys the
reference — so freeze is the safe side of the ambiguity.

This was already flagged in our own adversarial pass (spec §7 R1) before
CODEX's review arrived; the two findings agree.

## 2. Priming invariant — documented, option (a)

CODEX offered two options. We took the first — document unreachability —
and rejected replacing the fallback with a fault, for a reason worth
recording:

```c
// PRIMING INVARIANT (1.8, CODEX): this fallback is unreachable in a
// correct boot -- calibrate() primes the ring before hallTask exists, so
// medPrimed is true before the first sample arrives. It is RETAINED as
// last-resort defense: if a future edit ever breaks that ordering, a
// single-sample prime still beats a zero baseline, which would pin the
// thresholds at +/-38 around 0 and hold an event open forever. Do not
// move the motion gate above this line: a prime taken from the first
// moving sample could land mid-magnet.
if(!medPrimed) primeMedian(raw);
if(actualPwm <= MOTOR_DEAD_ZONE_PWM) return;
```

A defensive fault would trade a recoverable degraded state for an
unrecoverable one. A zero baseline pins thresholds at ±38 around 0, which
holds an event open permanently — strictly worse than a single-sample
prime. Unreachability verified in `setup()` ordering: `calibrate()`
precedes `xTaskCreatePinnedToCore(hallTask, …)`.

CODEX's caution against moving the guard above the prime is recorded in
the comment itself, so the next reader cannot make that edit innocently.

## 3. Long open event — tested, both poles

Dwells exceeding 65.535 s were run deliberately. **Three** saturated
events resulted, two north and one south. CODEX's seven checks, answered:

```
12:41:50 mm=112 obs=N ms=65535 drift=0 gate=LOW_PWM dt=694
12:51:17 mm=112 obs=N ms=65535 drift=0 gate=LOW_PWM dt=45898
13:00:35 mm=117 obs=S ms=65535 drift=0 gate=LOW_PWM dt=11131
```

South event with successors, showing the navigator resuming normally:

```
12:57:19  mm=118  obs=N  ms=889    drift=0  gate=LOW_PWM
13:00:35  mm=117  obs=S  ms=65535  drift=0  gate=LOW_PWM   ← the dwell
13:00:42  mm=116  obs=S  ms=294    drift=0  gate=RAMP
13:00:42  mm=115  obs=N  ms=225    drift=0  gate=NO_PREV
13:00:42  mm=114  obs=S  ms=191    drift=0  gate=ACTIVE
13:00:43  mm=113  obs=N  ms=200    drift=0  gate=ACTIVE
```

| CODEX check | Result |
|---|---|
| exactly one event emitted | ✔ one per dwell |
| `ms == 65535` | ✔ exact |
| `drift` near zero | ✔ **exactly 0**, all three |
| polarity matches arrival pole | ✔ N on north parks, S on south |
| navigation advances exactly once | ✔ 118→117, 113→112 |
| next marker not rejected as phantom | ✔ normal RAMP→NO_PREV→ACTIVE succession |
| stale arrival time benign | ✔ no age or station anomaly observed |

**`drift` is exactly zero by construction, and that is the point.** It is
`baselineCounts - evStartBaseline` — a direct measurement of whether the
reference moved during the event. Under 1.7 this field would carry the
migration; under 1.8 it reads 0 because there is nothing to report. The
field CODEX asked us to check turns out to be the cleanest single
discriminator between gated and un-gated firmware.

## 4. Acceptance matrix — all five rows run

Adopted verbatim. Results:

| Stop condition | Duration | Expected | Observed |
|---|---:|---|---|
| Clear of magnets | >70 s | baseline stable | ✔ 2024–2025 |
| Fringe field, offset below ±38 | 79 s | baseline stable | ✔ **2024, single value**; Δ −3…+7 |
| North magnet | 196 s | frozen; one N event | ✔ **2023, single value**; Δ +73 |
| South magnet | 88 s | frozen; one S event | ✔ **2023, single value**; Δ −65 |
| Magnet, PWM >20, physically stalled | — | residual reproduced | ✔ see below |

"Single value" is literal: across a 196-second dwell on a magnet the
baseline took exactly one value. For comparison, 1.7 destroyed itself in
33 seconds on the same magnet.

CODEX's judgement that the **fringe row is especially valuable** proved
correct. Δ −3…+7 is a real field too weak to trip an event, and it held
the reference perfectly. An excursion gate keyed to ±38 would have
admitted every one of those samples and let them accumulate; the motion
gate is indifferent to field strength. The argument now has evidence
rather than reasoning behind it.

**The stall row ran itself.** During setup the locomotive derailed with
the motor still running, wheels free, sensor over a magnet — precisely the
stall condition:

```
13:18:41  base=2043  raw=2096  d=+53  pwm=60
13:18:59  base=2083            ← median crosses over
13:19:43  base=2097  raw=2097  d= 0  ← reference has become the magnet
```

The documented residual, reproduced accidentally and exactly as
predicted. Also worth recording: on resuming motion the reference healed
**2097 → 2026 in about three seconds**. The gate does not impede recovery.

## 5. Wording correction — adopted, and the field went further

The 19-count figure is relabelled throughout as an **observed
baseline-variation bound**, not thermal drift. The field data then
decomposed it: **per-position steadiness is ±1**, and the 19 counts is
almost entirely *position dependence*. The 2026-08-06 capture shows the
mechanism live at small amplitude — parked at MM 29 the baseline held that
spot's local field level, then slid to the loop-wide norm over ~38 s of
driving on departure, matching the median wash-out constant. So the figure
bounds unmodelled drift without being a measurement of it, exactly as
CODEX required.

---

## One prediction the field falsified

Reported because it was ours, not CODEX's, and because the record should
not quietly improve.

Spec §1 predicted the post-departure stream would be **polarity-inverted**
— every real N delivered as S, every S swallowed — reasoning from
`EVENT_EXIT_HOLD_MS` being only 20 ms. **The field does not support that.**
Post-departure polarity was 83 N / 75 S, roughly even.

What actually degraded was **event rate**:

| | markers | duration | rate |
|---|---:|---:|---:|
| Healthy leg | 188 | 485 s | 0.39 /s |
| Corrupted leg | 158 | 174 s | **0.91 /s** |

A 2.3× flood of spurious and misjudged events. The conservation gate
caught 33 as phantoms — working as designed — but enough survived to drive
three separate adoptions and 45 markers of odometer error (dashboard 163,
locomotive physically at 117–118). The `EVENT_EXIT_HOLD_MS` reasoning is
not disproven as *a* pathway but is not the dominant one. Corrected in the
verdicts rather than defended.

CODEX's own framing survives this intact and is the more durable
statement: *QUORUM tolerates isolated incorrect observations, but not a
systematically displaced reference frame.* The mode of corruption was
different from our prediction; the class was exactly as described.

## Session totals on 1.8

382 AGREE / 10 DISAGREE, including one complete
`QUORUM_OPEN → ADOPTED → CLOSED` self-recovery. Best sustained stretch: 62
consecutive AGREEs, zero disagreements, one-count baseline spread. The
disagreements and two NO_QUORUM entries are attributable to operator
hand-repositioning (`QUORUM_HAND_REPOSITION_HAZARD.md`) and the
derailment, not to the gate.

## Still owed

- **Stage D regression lap** was not run as a dedicated row. Partial
  coverage exists: LOW_PWM-gated markers navigated correctly throughout,
  and loop/hall-task gaps stayed at the 2026-08-06 baseline. A clean
  lap should be recorded when convenient.
- **Toby is on QUORUM 1.6** — two versions behind, with neither INA219 nor
  the motion gate. Any two-locomotive work needs him on 1.8 with his own
  validation.
