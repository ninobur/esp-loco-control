# Otto X21 — Grillers parks him on a magnet, and rest migrates onto it before the dwell can freeze it

**2026-09-16, 11:25–11:51.** Otto 9950011,
`NAVI_ONE_1_0X21_HALL_ONLY_FIELDTEST`. Declared CCW. **Four stops.**

They are **two distinct faults**, and neither is X21's polarity rule.

| # | time | at | obs / exp | gap_ms | cause |
| --- | --- | --- | --- | --- | --- |
| 1 | 11:29:35 | MM117 → 116 | N / S | 919 | conversion burst opened 330 ms early — separate record |
| 2 | 11:37:57 | MM60 → 59 | S / N | 2747 | Grillers departure counted as a magnet |
| 3 | 11:39:17 | MM60 → 59 | S / N | 80121 | same, on the re-declared restart |
| 4 | 11:46:56 | MM60 → 59 | S / N | 2635 | same |

Stop 1 is `20260916_OTTO_X21_MM117_BURST_OPENING_STRIKE.md`. This record is
2, 3 and 4.

---

## 1. Grillers parks Otto inside a field. No other station does.

Every `DWELL_BEGIN` today, raw against the rest the detector had measured:

```
  Patio    1885 / 1893      Bamboo   1904 / 1903      Arches   1951 / 1951
  Patio    1947 / 1945      Bamboo   1949 / 1945
  GRILLERS 2163 / 2176      GRILLERS 2141 / 2138
```

Grillers rests **190–230 counts above every other stopping place** on the
route. The quiet line on this locomotive is ~1950; Otto comes to rest at
~2150. He is standing on, or inside, a magnet's field. This is not new
behaviour and it is not X21's — Grillers is the one asymmetric platform, and
`Stations.h` has always put its ramp at MM62 and its rest at MM60 against a
single marker.

## 2. Why `IN_OLD_FIELD` did not catch it

Both Grillers stops classified **`hall=REST`**, `depart −13` and `+3`, and
armed detection immediately on departure.

The classification is `|raw − rest| >= departCounts`, and `rest` is
`restRef_`, which **had already migrated onto the field**: 2176 and 2138.
Measured against a rest that is itself sitting on the magnet, the locomotive
looks like it is at rest. **The test cannot see the problem because its
reference is the problem.**

`noteRest` can only have moved it before PWM reached 0, because `restFrozen_`
blocks it from the first stationary sample onward. So the migration happens
during the **zero ramp** — about twelve seconds at 200 ms a count, creeping
into the Grillers field at walking pace, far longer than the
`localWindowMs + refractoryMs` = 945 ms a level must hold to be believed.

**X20's Arches fix is therefore incomplete, and Grillers is where it shows.**
X20 freezes rest at PWM 0; at Grillers the damage is already done by then.
`ExcursionDetector.h` states the choice plainly — *"STATION DECELERATION
REMAINS NORMAL HALL OPERATION... The dwell begins at PWM 0 and not one sample
earlier"* — and that is exactly the window the migration uses.

## 3. The extra count, stop 4 in full

```
DWELL_BEGIN  Grillers  mm=61  raw 2141  rest 2138  hall=REST
DWELL_DEPART Grillers  mm=61  raw 2142  rest 2138  armed=1

det=1293008  raw 2047  local_ref 2121  depart -74  -> S
             peak 190  peak_signed -190   exc_n 401  w_caliper_ms 401
             (the WHOLE window displaced: not an arc, the line coming home)
             -> matches map MM60 = S -> ADVANCE 61 -> 60

det=1295643  raw 1874  local_ref 1944  depart -70  -> S
             peak 174  peak_signed -174  exc_n 158  w_caliper_ms 177
             (a clean South arc: THIS is the real magnet)
             -> judged against MM59 = N -> STRIKE
```

Otto pulls away from Grillers, the signal falls ~190 counts from the field
back to the true line, and that fall is a −74 departure from a displaced rest.
It is counted as MM60. The real MM60 then arrives 2635 ms later and is judged
against MM59.

**The departure from the field he stopped in was counted as a magnet.** That is
the Arches failure of 2026-09-15, with the navigation consequence intact,
reached through the ramp instead of through the dwell.

## 4. This is not X21's polarity rule

On the false event, `depart −74` and `peak_signed −190` **agree**. The window
and the opening say the same thing. X20's excursion sum would have returned
South as well and made the identical extra count. X21 changed nothing that
bears on this.

If anything X21 makes it marginally harder: `noteRest` requires
`localWindowMs + refractoryMs`, which went from 800 ms to **945 ms** when the
guard widened to 645.

## 5. Stop 3 was probably not a positioning error

The operator put stop 3 down to incorrect positioning. It reads MM60 → 59,
`obs S`, `expected N` — identical to 2 and 4 — with `gap_ms 80121`, i.e. the
first detection after 80 s standing still. If Otto was re-declared while still
parked in the Grillers field, driving off reproduces the same spurious South.
Same mechanism, not a declaration error.

## 6. `IN_OLD_FIELD` has now executed on a locomotive

Twice, in the 11:31–11:39 window: `mm=60 raw 2070 rest 1981 dep 89` and
`mm=60 raw 2097 rest 1981 dep 116`. X20 shipped with that branch never having
run outside a host gate. It ran, held, and released. Those two stops produced
no strike.

The difference between those and stops 2/4 is **which rest was frozen**. At
1981 the rest was still near the true line, so the displacement was visible as
+89 / +116 and the hold armed. At 2138/2176 the rest had already gone over to
the magnet, the displacement read as 3 counts, and nothing armed.

## 7. Nothing proposed

The obvious moves — freeze rest at the start of the ramp, gate `noteRest` on
speed, bound rest migration against the prime — are all changes to the rest
estimator, and three of the four are new mechanisms. Requirement 8 of the X21
brief forbids a new baseline algorithm, and this record exists to establish the
mechanism, not to pick the fix.

What can be said without proposing anything: **the fault is in what rest is
allowed to become during a slow approach into a field, not in how a magnet is
identified once it is.** X21's subtraction is not implicated, and neither is
the 645 ms guard.
