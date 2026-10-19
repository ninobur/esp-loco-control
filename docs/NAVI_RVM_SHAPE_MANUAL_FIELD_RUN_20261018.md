# NAVI_RVM_SHAPE_MANUAL: first field run, Otto, 2026-10-18 evening

Analysis by Claude (Opus 5.5) of David's Pi run log. Nothing was changed on
Otto or on the Pi.

## Source

| | |
|---|---|
| Log | `/Users/davidbrown/Downloads/9950011_20261018_210522.log` (David), 10,369 lines, sha256 `b8655d2809a5d89901f264be871988d588ec68560c4233d3a8a4277f03335882` |
| Window | 21:05:22 – 21:23:05 (18 min) |
| Firmware | NAVI_RVM_SHAPE_MANUAL `30271a6`, boot `326C357476B624FC`, model `330cdc007da3445b` |
| Analysis | `analysis/navi_rvm_shape_20261018/field_20261018/analyze_field.py` → `field_summary.json` and CSVs |

The log contains **one run only**, declared **CCW** (every `state/nav`:
`session_dir CCW`, `dir -1`). David reports a CW run as well; it is not in this
file. The declaration itself (and `session_direction`) happened before the
log starts. The first record is `state/nav … "event":"DECLARED","mm":42,"target":41`.

## Observations

1. **One CONTRADICTION, then no claims.**
   - The first recognized ensemble came 1 s into the log (21:05:23): principal
     polarity **N**, where the navigator expected **MM041 (S)**. The navigator
     reported CONTRADICTION and made no geographic claim after it, as designed.
   - All 520 later recognitions are `RECOGNIZED_NO_CLAIM`.
2. **521 ensembles were recognized** in 18 min. Waveform windows ranged
   0.32–2.92 s, much of the run at PWM about 32, and LLR 35.7–43.9.
3. **Polarity sequence against the route map** (longest exact run of matching
   principal polarity):

   | Reading | Longest exact run |
   |---|---|
   | CCW (as declared) | 11 |
   | **CW, starting MM042** | **521 of 521** |
   | CCW, polarity inverted | 18 |
   | CW, polarity inverted | 11 |

   Every 10-magnet window of the route's polarity is unique, so a chance match
   of 521 is not credible.
4. **STOP/GO.** There were three stops (21:17:53, 21:20:38, 21:21:17), plus a
   final stop at 21:21:49 with no restart in the log. All were on the
   **baseline**, after MM110, MM047 and MM049 in the CW numbering. None lay
   inside an ensemble.
   - Arrival trims at GO: 379, 240 and 203 blocks.
   - Stationary ranges: 1896.3–1901.4, 1899.6–1903.4 and 1900.4–1905.1.
   - Recognition continued with the next magnet each time, with no miss and no
     duplicate (observation 3).
5. **Health.**

   | Counter | Value |
   |---|---|
   | `heap_min` | 8,944 B |
   | lowest `heap_free` | 15,996 B |
   | `nav_event_drop`, `pub_drop`, `cmd_drop`, `rvm_out_drop` | 0 |
   | RVM stack free, minimum | 5,648 B |
   | INA219 | present (16.39 V) |
   | **`block_q_drop`** | **44** |
   | **`rvm_push_max_us`** | **3,317,470** |

   The 44 dropped blocks were lost in two bursts: 7 at 21:21:02 and 37 at
   21:21:38. Each was the GO after a stop, when the held arrival blocks were
   released into the recognizer, and the single longest RVM step was 2.7 s and
   3.3 s.
   - **Cause.** The ESP32 needs several milliseconds per block, and the release
     plus re-scoring exceeded the 128-block (2.56 s) queue.
   - **Effect tonight:** the drops fell in the stationary departure phase and
     no magnet was affected.
   - Before that, `rvm_push_max_us` rose to 0.16–0.49 s during slow running at
     PWM 32 (wide windows, re-scoring after acceptance), and to 1.65 s at the
     first GO. No drops then.

## Interpretation (separate from the observations)

- **Travel direction.** The recognized magnets follow the route **CW (ascending
  MM numbers) from MM042**, for about 3 laps, without a single missed or extra
  ensemble. The session was declared **CCW**, with last MM042 and next MM041.
  - The first magnet passed was MM042, which is North; MM041 is South.
  - So the CONTRADICTION was a **correct** detection that the declared direction
    disagreed with the travel.
  - Why they disagreed is not in the log: session direction, motor direction or
    the declaration. Only David can say.
- **Counterfactual.** Had the run been declared CW at 041–042, the navigator's
  rule (advance when the polarity equals `ROUTE_POLARITY[next]`) would have
  claimed 521 MMs, all consistent with the route.
- **What this run does not test.** It does not exercise a stop with the sensor
  over a magnet, nor the CW-declared case.

## Open items (for David)

1. **The direction mismatch.** Which direction did you intend, and what were
   `session_direction` and the dashboard direction?
2. **The CW run log.** Please point me to it; it is not in this file.
3. **The block-queue overflow at GO.** This is a defect in the arrival
   hold-back implementation (resource timing on the ESP32). Possible fixes, none
   applied:
   - feed the released blocks to the recognizer in bounded portions between
     incoming blocks;
   - enlarge the block queue (heap is tight);
   - reduce the per-window cost, for example by doing the resampling in float.

   Your decision is needed.
4. **A stop over a magnet** remains to be tested on the track.

---

# CW run, 2026-10-19 morning (firmware `3a8953c`)

Analysis by Claude (Opus 5.5). Logs were copied read-only from the Pi run
logger (`/home/david/NGR/telemetry/runs/`).

## Sources

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_092846.log` | **The CW run**, 09:28:46–09:33:00 (Otto offline at 09:33:00) | `0ddc40bdc71b1c292ff0ccef3235571af2ddda64483f18f34f6abc2aa72a1add` |
| `9950011_20261019_085730.log` | 08:57–09:28: before the declaration (no recognitions; session UNSET, then CW) | `e2996fcb6d1f1b13c4c2b548b07d5e96d847b8c2f3359569e215070a3063abe2` |

About 20 one-second run files at 09:28:27–45 are logger churn at the run
boundary.

The analysis script is
`analysis/navi_rvm_shape_20261018/field_20261018/analyze_run.py`; its outputs
are in `analysis/navi_rvm_shape_20261018/field_20261019/`.

## Observations

1. **Boot and power.** Boot `05D9583227F92717` (a reboot on track power after
   the USB flash).
   - The INA219 is present (16.30–16.38 V).
   - The retained "INA219 NOT FOUND" warning from the USB boot was cleared at
     the declaration.
2. **Declaration.** CW, last MM040, next MM041 (09:28:46).
3. **Recognition.** 169 ensembles, 09:28:46–09:32:20. **All 169 were MM
   claims**, with no CONTRADICTION.
   - The recognized polarity sequence matches the route **CW from MM041 for
     all 169**. The longest CCW match is 11.
   - Every MM claim agrees with that route-derived identity (169/169), MM041
     through to MM038: **one full lap with no miss and no extra.**
4. **STOP.** At 09:32:31: last MM038, expected MM039, **candidate held**
   (an ensemble awaiting completion).
   - No GO followed; Otto went offline at 09:33:00.
   - MM039 was therefore not confirmed. This is the designed retrospective
     behaviour: a magnet traversed while stopping is reported after the next
     movement or declaration.
   - Whether the sensor stood over MM039 cannot be decided from MQTT alone.
5. **Health.**
   - Block drops, backlog (`rvm_backlog_max`), event, publish and command
     drops: all 0.
   - Worst RVM step 66.6 ms; RVM stack at least 5,680 B free.
   - Heap minimum 10,192 B.
   - NSR native drops 102 (boot), UDP failures 3.

## Interpretation

- **First full-lap evidence.** This is the first full-lap field evidence of the
  shape recognizer driving navigation. With the declared direction matching the
  travel, it advanced 169 MMs, all consistent with the route, with no
  CONTRADICTION.
- **Bounded feeding not exercised.** It could not be confirmed because there
  was no GO after a hold-back in this run.
- **Still not tested on the track:**
  - a stop with the sensor over a magnet followed by a restart (arrival mirror
    and resumption);
  - a GO after a long hold-back (bounded feeding).

---

# Runs 2 and 3, 2026-10-19 morning: stops over magnets (firmware `3a8953c`)

Logs were copied read-only from the Pi run logger. The outputs are in
`analysis/navi_rvm_shape_20261018/field_20261019/{undeclared_093641,cw_stops_094533}/`.

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_093641.log` | Run 2, 09:36:41–09:45:11, **undeclared** (session UNSET) | `04f3849936202b414d1e4e3bbf9695cfe02153f0e8dc4d5eb076dbf4f507992e` |
| `9950011_20261019_094533.log` | Run 3, 09:45:33–09:53:59 (    4855 lines; copied while Otto was still online) | `ebaab2ac08b2552b917067a9613c2baedf8e06fddca22d83f1f312fd54f944cf` |

**David's account of run 3:** CW, "a few stops over magnets, a slow section
approaching GRILLERS", not a full lap.

## Run 2 (undeclared)

There were no navigation claims; the recognizer is idle until a declaration.

- **Warnings:** "DIRECTION REFUSED: too fast to reverse" and "stop fully first",
  the admissibility rules acting as designed.
- **Health:** `block_q_drop 0`, `heap_min 9,528`, `rvm_backlog_max 256`.

## Run 3 (CW, stops over magnets)

**Setup.** Boot `B05694E24BFC6579`, declared CW (last MM040, next MM041) at
09:45:33.

**Navigation.** 80 ensembles, **all 80 MM claims**, MM041 → MM120, unique and
consecutive.
- The polarity sequence matches the route CW from MM041 for all 80 (the best
  CCW match is 10).
- Every claim agrees with the route-derived identity (80/80).
- **No CONTRADICTION, no miss, no extra.**

**Stops.** 10 STOP/GO stops. The stationary level during the stop shows where
the Hall sensor stood:

| Stop | Expected next | Stationary range (baseline ≈ 1,900) | Where | Outcome |
|---|---|---|---|---|
| 09:46:36 (re-stop 09:46:42) | MM044 (N) | **1985–2001** | **over MM044** | MM044 claimed once at 09:47:10, 1.3 s after the final GO; real span 6.3 s, waveform window 2.3 s, LLR 28.2 |
| 09:47:49 | MM048 (N) | **1999–2003** | **over MM048** | claimed once at 09:48:14; real span 22.5 s, window 4.5 s, LLR 17.5 |
| 09:48:28 (60 s) | MM050 (S) | **1721–1725** | **over MM050**, deep in the field | claimed once at 09:49:31; real span 61.4 s, window 3.6 s, LLR 37.5 |
| The 7 others | — | 1892–1906 | baseline | sequence continued |

- **Arrival trims:** 2–16 blocks per stop.
- **Blocks withheld after GO:** 972 in total, all stationary repetitions; no
  magnet was lost.

**Health.**
- **`block_q_drop 0`:** the bounded-feeding fix held; backlog peaked at 80
  blocks.
- **The longest single RVM step was 1.73 s** (slow section). The queue absorbed
  it.
- `heap_min 9,648`, RVM stack at least 5,680 B free, `pub_drop 0`.

**Margins.** Waveform windows ranged 0.24–8.88 s. The lowest LLR was **8.08**
(slow section), against about 35–44 on the open line.

## Interpretation

- **The decisive track evidence.** This is the first track evidence for the
  arrival mirror and the pattern-based resumption: **three stops with the Hall
  sensor over a magnet**, including a repeated stop and a 60 s stop deep in a
  South field. Each magnet was recognized once, with the right polarity, and the
  geographic sequence continued without error.
- **Not shown:**
  - CCW stops over magnets;
  - Toby;
  - a full lap with stops.
- **Watch item: margin.** The lower LLR margins in the slow section, minimum
  8.1, say a slow passage is less clearly an ensemble to the model than a cruise
  passage. It is observed, not acted on.
