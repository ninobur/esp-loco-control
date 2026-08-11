# Low-PWM phantom acceptance — narrow design proposal

**Date:** 2026-08-11
**Status:** proposal only. Nothing implemented, nothing flashed.
**Observed failure:** two phantom marker events accepted during the Bamboo
departure of 2026-08-10, capture lines **24662** and **24669**.
**Evidence:** `field-records/logs/20260810_IR_SPEED_LOCAL_1_2_otto.log`
**Replay:** `firmware/QUORUM/tests/` (real firmware, host-compiled)

**Recommendation: specify now, implement after a targeted sample.** The need is
demonstrated beyond argument. The discriminator separates cleanly but rests on
**nine** low-PWM events in one run, only two of them faults. Setting a
production threshold on seven genuine points is not a standard this project has
accepted elsewhere. §7 states the sample that would settle it — roughly one
evening's running.

---

## 1. The failure being addressed

During the Bamboo departure on the final lap, two events were accepted that the
locomotive did not physically traverse. The odometer went 2 markers ahead of
truth. Offset −2 is outside `QUORUM_OFFSETS {-1..+4}`, so QUORUM could not
express the correction; it adopted +3 on a four-entry ring where −2 also scored
4/4, compounding the error to −5, and stopped at HARD_BOUND fifteen markers
later. Established in
[QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md](QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md)
and localised by a change-point fit with 0 mismatches over 39 events.

**10.2%** of accepted markers in the run were never conservation-tested.

---

## 2. Why the conservation gate is disabled below PWM 40

`QUORUM.ino:1492`. The conservation test asks whether two adjacent events split
one physical interval, which requires an expected interval:

```
velocityMmPerSec = VEL_MODEL_SLOPE*pwmActual + VEL_MODEL_INTERCEPT   // 3.90*pwm - 99.2
expectedDtMs     = 1000 * spacingMm[interval] / velocityMmPerSec
```

The model crosses zero at **pwm 25.4**. Below that `velocityMmPerSec` is
negative and `expectedDtMs` is negative — not a large number, not a small one,
but meaningless. `GATE_LOW_PWM_FLOOR = 40` sits above that crossing with margin;
at pwm 40 the model yields 56.8 mm/s, the lowest value it can state at all.

The floor is therefore **correct as a statement about the model**. Its defect is
what it does next: `LOW_PWM` calls `invalidatePreviousAcceptedDt()` and then
`acceptEvent(e)` **unconditionally**. Having concluded it cannot judge, the gate
admits the event rather than withholding it. That is the hole the phantoms came
through.

---

## 3. Why forcing the model at low PWM is invalid

Not a theoretical objection — the capture contains the counterexample.

**Capture line 24681, marker 161, pwm 35, gate LOW_PWM.** A *genuine* marker:
the change-point fit puts the offset at −2 from line 24669 onward, and 24681 is
consistent with it; its peak (68) sits inside the 67–78 range of the ten other
crossings of marker 161.

Force the model there:

```
v            = 3.90*35 - 99.2 = 37.3 mm/s
spacingMm[160] = 300 mm
expectedDt   = 1000 * 300 / 37.3 = 8043 ms
observed dt  = 2272 ms
```

The conservation test would compare `dt + previousAcceptedDt` against 8043 ms
and find the pair far short of one interval — the phantom signature — and
**reject a real marker**. The model underestimates travel at pwm 35 by roughly
3.5×, because PWM is a request and the locomotive coasts, is pushed by grade,
and carries momentum out of a stop.

Below pwm 25.4 there is not even a wrong answer to force: the two phantoms were
detected at **pwm 12 and 19**, where the model reports −52.4 and −25.1 mm/s.

**Conclusion: the model cannot be extended downward. Any low-PWM rule must not
use it.**

---

## 4. What actually distinguishes the two recorded phantoms

The run crosses the Bamboo departure markers **eleven times**, so each phantom
has ten controls at the same marker, same manoeuvre, same physical distance.

### 4.1 Peak amplitude — the discriminator that holds

```
marker 159, 11 crossings   genuine peak 66-88   phantom (line 24662) peak 39
marker 160, 11 crossings   genuine peak 59-73   phantom (line 24669) peak 40
```

Across the whole run, only **9 markers of 1890** were detected below pwm 40:

```
genuine (7):  peak 67, 68, 71, 73, 96, 99, 125
phantom (2):  peak 39, 40
```

A 27-count gap with nothing in between. Physically coherent: a genuine
traversal carries the sensor through the full field of the magnet, while a
locomotive rocking or creeping over a magnet it is already sitting on
re-triggers on a partial field.

Both phantoms cleared the detector's own floor (`HALL_MIN_PEAK_DELTA = 35`) by
4 and 5 counts.

### 4.2 Departure geometry — corroboration, model-free

`DWELL_COMPLETE` → `DEPARTURE_COMPLETE` at Bamboo, all eleven laps:

| lap | elapsed | actual_pwm at clearance |
|---|---|---|
| 2–10 | 9.20 – 11.02 s | 61 – 74 |
| **11 (incident)** | **4.32 s** | **29** |

Identical markers, identical distance. The incident lap covered it in **less
than half** the fastest control time at **less than half** the power. Faster
travel at lower power is not physical; two of the three "crossings" were not
crossings. This is independent of any velocity model — it compares the
locomotive against itself.

It is corroboration, not a runtime rule: it needs per-station history the
firmware does not keep, and adding that store is exactly the kind of machinery
the governing rule forbids without demonstrated need.

### 4.3 What does NOT distinguish them — and why it matters

- **Pulse width.** Phantom 24669 has `ms` 1437 against 149–234 for genuine
  marker-160 crossings — a 6× outlier. But phantom 24662 has `ms` 364, sitting
  inside the genuine 342–600 range for marker 159. Width catches one of two.
  **A debounce or width filter would not have prevented this incident.**
- **Peak, run-wide.** Genuine markers reach peak 38, 39, 39 at pwm 90–120
  (capture lines 19604, 19557, 19597) — all inside the defective mm 66–82
  stretch. A *global* peak floor at any level that catches the phantoms would
  discard genuine readings there. **The rule must be scoped to the window where
  the gate is already disabled.**
- **PWM alone.** Capture line 20040 is a marker at **pwm 0 with peak 125**,
  during operator handling around the reboot at lines 20053–20073. A rule that
  rejected on low PWM would discard it. Peak survives this case; a PWM floor
  does not.
- **dt.** Phantom 24662 has dt 26522 (a dwell) and phantom 24669 has dt 1170.
  Genuine low-PWM dt ranges 2272–65535. No usable separation.

---

## 5. The proposed rule

**One clause, inside the existing `LOW_PWM` branch. No new state, no timer, no
debounce, no baseline manipulation, no new detector authority.**

Today (`QUORUM.ino:1492`):

```
if (pwmActualAtDetect < GATE_LOW_PWM_FLOOR) {
    lastTimingGate = "LOW_PWM";
    invalidatePreviousAcceptedDt();
    acceptEvent(e);            // <- admits without judging
    return;
}
```

Proposed:

```
if (pwmActualAtDetect < GATE_LOW_PWM_FLOOR) {
    invalidatePreviousAcceptedDt();
    if (e.peak < LOW_PWM_PEAK_FLOOR) {        // new constant, §7 sets it
        lastTimingGate = "LOW_PWM_WEAK";
        publishQuorumDecision("WEAK_REJECTED", ...);   // peak, pwm, dt, floor
        return;                                        // navMm does NOT advance
    }
    lastTimingGate = "LOW_PWM";
    acceptEvent(e);
    return;
}
```

### Reject / defer / accept, exactly

| condition | verdict |
|---|---|
| `pwm >= GATE_LOW_PWM_FLOOR` | unchanged — RAMP / NO_PREV / ACTIVE as today |
| `pwm < floor` and `peak >= LOW_PWM_PEAK_FLOOR` | **accept**, exactly as today |
| `pwm < floor` and `peak < LOW_PWM_PEAK_FLOOR` | **reject**: publish, do not advance `navMm`, do not push the ring, do not touch `missStreak` |

**There is no defer state, deliberately.** Deferral needs a holding buffer and a
release condition, and a released event carries a stale `detectedAtMs` into a
timing gate that reasons about intervals. That is new machinery for a failure
mode nothing in the capture demonstrates. Rejection matches how
`PHANTOM_REJECTED` already behaves (`QUORUM.ino:1512`): decline to advance,
leave `previousAcceptedDt` alone, publish the fact.

**Why rejection is safe here specifically.** A rejected genuine marker leaves
the odometer one *behind* — a positive offset, which the fence expresses up to
+4 and which QUORUM recovers from unaided (`syn_ordinary_recovery` proves it
adopts +1). An accepted phantom leaves it *ahead* — a negative offset, which the
fence expresses only to −1, and which this incident proves it cannot recover
from. **The asymmetry of the fence makes a false reject far cheaper than a false
accept**, and it is why this is worth doing at all.

### Scope

- Applies only where `pwm < GATE_LOW_PWM_FLOOR` — 9 of 1890 markers (0.5%) in
  this run. It cannot touch the defective mm 66–82 stretch, where genuine
  readings run at pwm 90–120.
- The navigator does the rejecting, with context (it knows the gate is invalid),
  not the detector. The doctrine at `QUORUM.ino:285` — *"the detector no longer
  makes decisions … rejection needs context"* — is preserved. `HALL_MIN_PEAK_DELTA`
  is untouched.

---

## 6. How the rule could be defeated

| condition | effect | assessment |
|---|---|---|
| **Station dwell** | `pwm` 0; any event is in scope | Where phantoms are most likely. Correct scope. |
| **Departure ramp** | `pwm` climbs through the window | The case at hand. Line 24681 (pwm 35, peak 68) is the genuine marker that must survive, and does at any floor ≤ 67. |
| **Wheel slip / wet rail** | wheels turn, locomotive doesn't | **Does not defeat it.** Markers are track magnets read by position, not wheel odometry. Slip cannot create a marker. It can make the loco *rock* over a magnet — which is the phantom mechanism, and produces the weak partial-field read the rule targets. |
| **Towing load** | more power for the same speed, so `pwm` higher at a given speed | Pushes events *out* of scope. Fewer low-PWM events, not more. Safe direction. |
| **Grade** | Grillers climbs; PWM boosted to 120 there | Out of scope entirely. The incident A/B stretch is unaffected. |
| **Genuine very slow motion** | real traversal at pwm < 40 | **The real risk.** A genuinely slow crossing still passes the full magnet field, so peak should hold; all 7 genuine low-PWM events in the run have peak ≥ 67. But sensor clearance varies with track, and a weak *magnet* at low speed could read low. This is what §7's sample must bound. |
| **Operator handling** | loco pushed by hand, `pwm` 0 | Line 20040: pwm 0, peak 125 — accepted by the rule. A PWM-based rule would have discarded it. |
| **Weak or misaligned magnet** | genuinely low peak at low speed | **The failure mode that would bite.** Route-wide, genuine peaks reach 38 in the mm 66–82 stretch. If a similarly weak magnet sits where the locomotive also runs slowly — a station approach — the rule discards a real marker. Mitigated by direction (a false reject is recoverable, §5) but it is the reason for §7. |

---

## 7. Evidence required before implementing

The separation is clean and the direction of error is safe, but the genuine
low-PWM population in this run is **seven events**. What is missing:

1. **20–30 station departures logged with markers** — Bamboo and Grillers, laden
   and light, wet and dry. Yields the genuine low-PWM peak distribution at the
   places the rule fires. `LOW_PWM_PEAK_FLOOR` should be set from that
   distribution's lower tail, not from seven points. Roughly one evening.
2. **A slow deliberate crossing.** Drive a marker at the lowest PWM that moves
   the train and record peaks. Bounds the genuine-slow case directly.
3. **Peak at the mm 66–82 markers, at low speed.** The one stretch where genuine
   peaks are known to fall to 38. If a station stop ever lands there, the rule's
   scope and that stretch's fault interact. Run it slowly and measure.

Should any of these show genuine low-PWM peaks below ~55, the rule is not viable
as stated and the alternative is to **withhold rather than reject** — hold the
low-PWM run out of the evidence ring while still advancing `navMm`, so a weak
read cannot corrupt adjudication even if it did happen. That variant is cheaper
to justify but does not fix the odometer error, and is not proposed here.

---

## 8. How the replay proves it

The suite in `firmware/QUORUM/tests/` compiles QUORUM.ino for the host and
already reproduces the run exactly: 1890/1890 odometer values, 40/40
adjudication decisions. Acceptance for this change:

1. **Both phantoms rejected.** `full_run` must publish `WEAK_REJECTED` at
   capture lines 24662 and 24669, and at no other line.
2. **The genuine low-PWM marker survives.** Line 24681 (pwm 35, peak 68) must
   still be accepted. This is the discriminating test — a rule that merely
   rejects on low PWM fails it.
3. **No other marker changes verdict.** The other 6 genuine low-PWM events —
   capture lines 11910, 15544, 19662, 20040, 20888, 22771 — must be accepted,
   including 20040 at pwm 0.
4. **The incident does not happen — already demonstrated.** This is not a
   prediction. `run_suite.py` section 5 replays the real event stream with
   exactly those two events removed and the whole chain disappears:

   ```
   dropped the 2 phantom events at capture lines [24662, 24669]
   adoptions: 0 (was 1 — the wrong +3 at line 24800)
   NO_QUORUM at mm [87, 100] (was [23, 87, 100]) — incident C gone
   ```

   Incidents A and B are untouched, as they must be — they occur elsewhere for
   other reasons. **This establishes the two events as the sole cause of
   incident C, rather than merely correlated with it**, and it fixes the
   acceptance target: any rule that rejects exactly these two inherits this
   outcome. It also bounds the prize honestly — the rule buys one incident of
   three.
5. **Nothing upstream changes.** `verify_inert.py --base <pre-change>` must show
   every fixture identical except the events at 24662/24669 and everything
   causally downstream of them.
6. **Legitimate recovery still works.** `syn_ordinary_recovery` must still adopt
   +1. A navigator that rejects too freely would drift toward never adopting.
7. **New synthetics.** A weak genuine low-PWM marker at each candidate threshold,
   to make the false-reject boundary explicit rather than implied.

Note the replay's limit, stated in the suite README: it begins at
`navOnMarker()`, so it can prove the *navigator* rejects these events. It cannot
prove the detector reports `peak` consistently across temperature, battery state
and sensor clearance. That is a bench and field question, and §7 is how it is
answered.

---

## 9. Recommendation

**Do not implement yet.** Adopt the rule as specified, and gather §7's sample
first — it is one evening of running and it converts a seven-point inference
into a distribution.

If the phantom recurs before that sample exists, §5 is ready to implement as
written, and the fence asymmetry means the failure direction of a wrong
threshold is the recoverable one.

Two things should proceed independently of this proposal, both already
identified: the **mm 66–82 stretch** needs a slow-speed pass to separate a
marker fault from a detection-speed limit, and **fence width and adoption
evidence floor** remain a coupled defect awaiting their own joint redesign and
decision record.
