# QUORUM_1_13 beta — session record and verdict

**Date:** 2026-08-11 · **Locomotive:** Otto 9950011 · **Build:** `QUORUM_1_13`
**Capture:** `field-records/logs/20260811_QUORUM_1_13_beta_otto.log` (36 817 records, 135.8 min)
**Protocol:** `field-records/QUORUM_HARD_BOUND_ADVISORY_BETA_PROTOCOL.md`

**Verdict: the advisory PASSES. Ship it. It is also, on this evidence, of
limited value — and the session found something far more important.**

---

## 1. Session at a glance

| | |
|---|---|
| markers | 2 667 |
| station departures | 59 |
| AGREE / DISAGREE | 2 418 / 101 (4.0%) |
| QUORUM incidents opened | 19 |
| adoptions | 18 (16 closed, 1 reopened) |
| phantom rejections | 38 |
| terminal events | 3 |
| reboots | 2 (one operator power-cycle) |

Conditions: CW then CCW, towing the IR test car and later a heavyweight car;
Hall sensor re-glued and re-centred that morning after being found dislodged.

---

## 2. Advisory results — the thing under test

Three terminal events. **The advisory returned `null` every time, and was
correct every time.**

| t+ | mm | reason | `adv` | `advn` | correct? |
|---|---|---|---|---|---|
| 2808 s | 90 | HARD_BOUND | null | 12 | yes — no exact match anywhere on the route (best 11/12) |
| 3106 s | 33 | HARD_BOUND | null | 12 | yes — no exact match anywhere (best 10/12) |
| 3398 s | 0 | SECOND_ADOPTION_FAILED | null | 3 | yes — ring rebased to 3 entries |

Both HARD_BOUND rings were checked against all 171 alignments. Neither has an
exact 12-window match **anywhere on the railway** — so the nulls are not an
artefact of the ±5 bound. Incident A came within **one bit** of a perfect match;
a nearest-match design would have confidently named that marker. Exact-or-silent
refused. That is the first field test of the refusal behaviour and it held.

The third event is a genuine field `SECOND_ADOPTION_FAILED` — the exact scope
case the reviewer required gating in `723f0b4`. Honest limit: `advn` 3 means the
short rebased ring would have silenced it anyway, so it confirms the path occurs
in service and that the synthetic models it faithfully, but it does not isolate
the reason gate.

**What was NOT demonstrated: a non-null advisory in the field.** In 135 minutes
the advisory never had an opportunity to name a marker. Its positive case rests
entirely on the harness — 1 535 provocations (1 197 CW, 338 CCW), zero wrong
advisories. Useful, but it should be said plainly: the feature has never yet
told the operator anything they did not already know.

Protocol §6 pass criteria: Test 1 `adv:null` ✓; normal running unchanged ✓;
no wrong non-null advisory ✓; declaration remained the only recovery ✓.
Test 3 (provoked positive) was not run.

---

## 3. The finding that matters more

**Phantom marker events occur once per lap, at a fixed place, and the
conservation gate cannot see them.**

16 of the 18 adoptions were offset −1 — a single phantom correction. Eight were
the *identical* correction `95 → 96` with identical score vectors, once per lap.
The cause, visible every lap:

```
mm 102   dt 1862   ratio -1.0   peak 234
mm 101   dt  335   ratio 1.88   peak  41    <-- phantom, ACCEPTED
mm 100   dt 1374   ratio 1.43   peak 144
```

A 335 ms event at pwm 90 with peak 41 against a session median of 144. It is
accepted because `dt_conserve_ratio` is 1.88, just outside the reject band
[0.7, 1.3] — and it is 1.88 only because the preceding interval (1 870 ms) is
long, the locomotive still accelerating out of the Arches dwell.

The same signature appeared at **Grillers (mm 63) under CW**, twice, where the
long preceding interval came from the climb instead:

```
mm 62   dt 2936   ratio 3.10   peak 258
mm 63   dt  617   ratio 2.02   peak  51    <-- phantom, ACCEPTED
```

**One root cause.** `expectedDt` is derived from `3.90·pwm − 99.2`, a model that
knows nothing about grade or acceleration. Whenever real speed differs from the
model — on a climb, or accelerating out of a stop — a long true interval plus a
short phantom sums to look like two legitimate intervals, and the phantom is
admitted. The source comment already says why: *PWM is a request, not a result.*

This supersedes `docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md`. That proposal
was scoped to pwm < 40; **both recurring phantoms occurred at pwm 72 and 90 with
the gate fully ACTIVE.** Low PWM is only the extreme case where the model is so
wrong it is switched off. See `docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md`.

---

## 4. The poisoning trap (derailment, t+3090)

A car derailed and left the locomotive spinning. The gate then **inverted**:

```
dt  173  ratio 0.30  peak  41  -> ACCEPTED   (phantom)
dt 1010  ratio 0.99  peak 151  -> REJECTED   (genuine)
dt 1266  ratio 1.21  peak 118  -> REJECTED   (genuine)
```

Once a short phantom interval becomes `previousAcceptedDt`, every genuine marker
sums to ≈ one expected interval and is rejected, while every further phantom
sums to ≈ 0.3 and is accepted. Rejection deliberately does not update the
predecessor, so **there is no escape path**. 18 consecutive genuine markers were
discarded; the ring filled with nine identical N readings; HARD_BOUND followed.
An operator power-cycle cleared it.

Proportion, honestly: outside the derailment the gate behaved well — one genuine
marker wrongly rejected in 1 139 ACTIVE markers of normal running (~0.1%), and
the ten other in-band rejections were the weak echoes at mm 149/150 being killed
correctly. This is a latent trap with a specific trigger, not an everyday fault.

---

## 5. Physical findings

- **mm 149 and 150 throw a weak echo every lap** — a second trigger ~250 ms
  after the real read, ~50 mm past the magnet, peak 42–50 against a median 144.
  The gate kills them correctly (15 of the 38 rejections), but they are a
  standing supply of phantoms.
- **The detector scale moved ~1.8×** after the sensor was re-glued: peak median
  80 → 144, p5 57 → 123. At mm 159 — the marker that produced the 10 Aug phantom
  at peak 39 — today's read is 133. Any fixed absolute peak threshold calibrated
  on one session would have been wrong for the other.
- **The mm 66–82 "defective stretch" from the 10 Aug analysis is withdrawn.**
  With the sensor re-glued, signal there is normal (peak median 144, same as the
  route). Today's disagreements in that range were an odometer offset, not
  misreads: tested against the map, offset 0 fits 2/8 and offset −1 fits **8/8**.
  The 10 Aug behaviour is better explained by the dislodged sensor plus the
  stall. No track repair is indicated.

---

## 6. Operator findings (deferred)

- **Console E-STOP ALL can only assert.** `pub_dispatcher()` always publishes
  `"1"`. The only working clear is the Dashboard's toggle; the console's per-loco
  CLEAR button is `display:none` unless the loco reports E-stopped. Recorded, not
  fixed.
- Governing principle, operator-stated: **Dashboard is MANUAL, Console is AUTO** —
  the two chambers of the bicameral rule. The AUTO chamber must be self-sufficient
  for AUTO operations; today clearing E-stop and declaring position after
  NO_QUORUM both live only in the manual chamber.
- A re-ordering of both surfaces is the operator's to specify. Not started.

---

## 7. Recommendation

1. **Keep `QUORUM_1_13`.** The advisory is correct, inert, and costs nothing.
   Do not expect it to earn its keep often.
2. **Take up `docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md` next.** It addresses
   the fault that actually cost this session 18 adoptions and three stops.
3. **Do not implement the low-PWM peak-threshold proposal.** Superseded; its
   scope was wrong and its threshold does not survive a sensor re-glue.
4. Leave the fence and adoption floor alone until the timing work lands — every
   one of the 18 adoptions was inside the fence and 16 closed successfully, so
   the fence is not the binding constraint today.
