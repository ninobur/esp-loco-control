# NAVI_ONE 0.6 — Field finding 10

## The baseline is captured by the magnet it parks on; and the Grillers departure cannot reach its own grade setting

**Date:** 2026-09-01
**Locomotive:** Toby (9950012), `NAVI_ONE_0_6`, boot 16:41:37
**Status:** Observed and decoded. Not a decision. Nothing ratified. No code changed.

---

## 1. What 0.6 got right

First field run of the station machine (decision 0068). All four stations
executed the full sequence ARMED → APPROACH → ZONE → ZERO_RAMP → DWELL →
DEPART → DEPARTED.

| station | dir | ramp starts | comes to rest | dwell |
|---------|-----|------------:|--------------:|------:|
| Patio    | CW | MM16 (+1) | MM17 (+2) | 30.000 s |
| Grillers | CW | MM64 (+1) | MM64 (+1) | 30.036 s |
| Arches   | CW | MM109 (+1) | MM110 (+2) | 30.001 s |
| Bamboo   | CW | MM158 (+1) | MM159 (+2) | 30.009 s |

Approach pacing widened 231 / 256 / 287 / 326 / 378 ms per marker as designed.
**Grillers CW rests a marker earlier than the rest** — the climb brakes it — which
is the per-station, per-direction difference the tunable columns exist for.

271 advances, 1 disagreement, baseline healthy at 1853 for the first two laps.

---

## 2. The Grillers CW departure cannot climb

**Observed:** after the first completed Grillers stop, the wheels spun and the
operator had to help the locomotive away.

```
16:50:47.049  DEPART, target 90 PWM
16:50:52.995  throttle reaches 90
              ... 31 s at 90 PWM, no marker advances ...
16:51:23.499  first advance (operator assistance)
16:51:26.926  DEPARTED at MM67, machine goes Idle
16:51:31      throttle 110   <- the grade cruise finally arrives
```

For comparison, on the lap where the stop was interrupted and Toby ran through,
MM63→67 took about 7 s. The grade is climbable at speed. The standing start is
not.

**Cause, in this repository's own code.** Two things combine:

1. `StationMachine` departs to `cruisePwm` as handed to it — `AUTO_CRUISE_PWM`,
   90 — not to the section cruise.
2. `cruisePwmAt()` is only consulted when `stationMachine.phase() == StPhase::Idle`
   (`NAVI_ONE.ino`, `case Ruling::Advanced`), and the machine does not reach Idle
   until `off == +4`, i.e. MM67.

So the locomotive needs 110 to leave Grillers and cannot be given 110 until it
has already climbed three markers. MM64→67 is exactly the stretch decision 0066
raised to 110 for. Grillers CW is the only station whose departure runs into a
grade, which is why the other three depart in 6–8 s.

The operator's specification was correct and complete; the gating in the
`Advanced` branch is what dropped it. **Operator's ruling, 2026-09-01: "After
Grillers CW, the target should be 110 out of the station."**

---

## 3. The Bamboo strike: the baseline was captured, not frozen

**Predicted in decision 0068:** that the 30 s dwell would sit the locomotive
still with the baseline frozen — the latch condition of finding 08.

**What actually happened is the mirror image.** The baseline was never frozen. It
was *captured*.

```
16:54:19  1854            rolling in, throttle 52
16:54:24  1851            throttle 27
16:54:25  1859            throttle 22
16:54:26  1895            throttle 17     <- +36 in one second
16:54:27  1899            throttle 12
16:54:29  DWELL_BEGIN     1897-1900, still updating, for the whole 30 s
16:54:59  DEPART
16:55:02  ADVANCE MM160   gap_ms 41306, peak 144, resid 0.1112
16:55:05  TOO_SOON        gap_ms 57, peak 236
16:55:05  DISAGREE        obs N, expected S -> STRUCK
```

Toby comes to rest at MM159 — centre + 2 — with the sensor in a magnet's steady
field. Because the approach is slow the raw level rises gradually, and the
41-deep median tracks it without any single step exceeding the 38-count entry
margin. **The reference becomes the magnet:** 1853 → 1899, a shift of +46.

On departure the field falls away and the true idle level now reads 46 counts
low. The decoded dump shows exactly that:

| slot | opened | open for | pol | outcome | peak | resid | outer thirds |
|-----:|--------|---------:|:---:|---------|-----:|------:|--------------|
| 5 | 16:54:15 | 286 ms | N | MAGNET | 201 | 0.0834 | clean arc |
| 4 | 16:54:17 | 282 ms | S | MAGNET | 204 | 0.0572 | clean arc |
| 3 | 16:54:20 | 360 ms | S | MAGNET | 196 | 0.0773 | clean arc |
| 2 | 16:55:02 | 554 ms | N | MAGNET | 144 | 0.1112 | broad, asymmetric |
| 1 | 16:55:05 | **3628 ms** | S | **TOO_SOON** | 236 | — | mean 50 sd 6 / mean 46 sd 6 |
| 0 | 16:55:05 | 128 ms | N | MAGNET | 132 | 0.1087 | clean arc |

Slot 1 is the signature: 3.6 seconds held open, flat to within 6 counts on both
outer thirds, polarity S — the sign of a baseline that is too *high* — with the
real MM161 magnet plainly visible inside it at peak 236. It opened 57 ms after
the previous passage closed, so the 200 ms guard rejected the whole thing and a
good marker was consumed. The next real magnet then disagreed.

### This is the failure QUORUM decision 0017 was written to prevent

`QUORUM.ino` v1.8, in its own words: the median's robustness "rests on an
assumption that only holds while MOVING: a traversed magnet contributes <=1
sample in 128; a PARKED-ON magnet contributes all of them, and within ~32-64 s
the reference BECOMES the magnet."

NAVI_ONE's bootid reads `motion_gate: 0`.

---

## 4. Two corrections to the analysis in circulation

**The dwell is 30 s, not 20.** `STATION_DWELL_MS` is 30000 and the field
measurement is 30.009 s. This matters because it is the number the migration
budget is reasoned against.

**The capture did not happen during the dwell.** It took roughly one second,
between 16:54:25 and 16:54:26, while the locomotive was still *decelerating* at
throttle 22 → 17. By DWELL_BEGIN it was already complete. **Shortening the dwell
is therefore not a mitigation**, and any gate must be closed during the approach
ramp, not merely during the stop.

---

## 5. What this says about the proposed motion gate

Restoring motion-gated baseline maintenance (CODEX's proposal, QUORUM 0017)
addresses both observed failures: it stops adaptation onto a parked magnet, and
it allows a stale offset to be adapted away while moving instead of latching.

Two things must be settled before it is written.

**The gate constant should come from NAVI_ONE's own tractive floor, not
QUORUM's.** QUORUM uses `MOTOR_DEAD_ZONE_PWM` 20. NAVI_ONE's measured floor is
PWM 25.1 (`speed_mm_s = 3.990 x (PWM - 25.1)`). The capture here began at
throttle 22: a gate at 25 closes just before it, a gate at 20 does not.

**The time constants differ by 60x and that is the whole difference.** QUORUM's
median is 128 samples at 500 ms — 64 pushes, about 32 s to migrate. NAVI_ONE's
is 41 at 25 ms — 21 pushes, about **525 ms**. QUORUM's reference is simply
immune to a two-second slow roll; NAVI_ONE's is captured by it. The same is true
in the other direction: a genuine passage longer than 525 ms would have its
reference move underneath it while being recorded. From today's magnets the
effective width is ~30 mm, so 525 ms corresponds to about **PWM 39** — and the
band PWM 25–39 is precisely the station approach and departure ramps.

The remedy both analyses have independently arrived at: **let the live baseline
migrate for the open/close threshold test only, and reference the recording to
the baseline captured at passage open.** Migration can then end a latch without
deforming a waveform at any speed.

---

## Artefacts

- `field-records/logs/20260901_navi_one_bamboo_capture/` — dump line and six
  decoded CSVs.

## Carried forward

- Finding 09's mechanism (latch on a stale baseline) and this one (capture onto
  a parked magnet) are two routes into the same state. Both are open.
- The +2 rest point is what puts the sensor in a magnet's field at Bamboo,
  Arches and Patio. Whether to move it is a separate question from the baseline.
- Grillers CW departure remains unable to reach 110.
