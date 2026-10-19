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

## Run 3 ending: the final stop (added after David's observation)

**Observation (David):** run 3 ended at the final stop and did not continue.
The Hall sensor came to rest **between MM126 and MM127**.

**Log (full file, refetched after it closed):**

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_094533.log` | Run 3, complete, 5,347 lines, to 09:55:00 | `843a8d2720304a00448dbc423aa113ccfb9db2bc57ebe1307014ac631eee8770` |
| `9950011_20261019_095501.log` | 09:55:01–09:56:39, PWM 0 throughout, Otto offline at 09:56:39 | `ef5fb4dd494ef466603e0221b0e0e0fc52601fc58b97809297ca5cf7b1d1348a` |

- The commanded target was **0 from 09:52:40**; applied PWM ramped down from
  116 to 0 by 09:53:03 (about 22 s).
- The last claim was **MM120**, published 09:53:00.674. Its waveform midpoint is
  block time 480.07 s and it was decided at 480.47 s.
- STOP (`STOPPED_WAVEFORM_PAUSED`) at block time **491.31 s**, published
  09:53:01.260: last MM120, expected MM121.
- No GO and no declaration followed. `state/nav` still reported MM120 → 121,
  `waveform_paused 1`, at 09:56:16.

**Reconciliation.** MM120 was decided at block time 480.47 s but published at
wall time 09:53:00.67, about **10.2 s after** the STOP block's own wall time
would place it. That lag is the arrival hold-back: from the moment the stop
was commanded, blocks were held in the 512-block ring (10.24 s), and only the
oldest were released to the recognizer as the ring filled.

- The 11.1 s of travel between MM120 (block 480.2 s) and the STOP (block
  491.3 s) was therefore **still held in the ring** when Otto stopped.
- Held blocks are released only at GO (after the arrival trim) or at a
  declaration. Neither happened, so **MM121–MM126 were passed but never fed to
  the recognizer, and never claimed.**
- This is the designed retrospective behaviour (as with MM039 on the CW run),
  not a recognition failure. The recognizer never saw those blocks, so whether
  it would have recognized them is untested; the raw Hall for them is not in
  the MQTT log.

**What the field shows that was not anticipated.** With a long ramp-down
(commanded 0 for 22 s before PWM fell below 15), the hold-back covers the
whole final 10 s of travel. At a final stop the navigator's position is then up
to about 10 s of travel (here six magnets) behind the real position, until the
next GO or declaration. Recorded as evidence; no change made or proposed here.

---

# Run 4, 2026-10-19 10:00: CW, cruise at PWM 90, one final stop (firmware `3a8953c`)

**David's plan:** CW at 90 with no stops, starting between MM126 and MM127;
the stopping point is MM029–MM030.

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_100013.log` | 10:00:13–10:02:59, 1,687 lines (the declaration was in the one-second churn files just before) | `857c7fedbcf683e05c6aeec1ef43fcee68d0fb9ce9dc9e1252505b8cf5099833` |

Output: `analysis/navi_rvm_shape_20261018/field_20261019/cw_nostop_100013/`.

## Observations

- **Boot** `8F9B71ED5DAF40BB` (a new boot after run 3), declared CW, last
  MM126, next MM127.
- **PWM:** commanded 90 from 10:00:14 to 10:01:45, then 55, then 43, then 0 at
  10:02:06. Applied PWM fell below 15 at about 10:02:16.
- **Navigation:** 73 recognitions, **all 73 MM claims**, MM127 → MM171 → MM028,
  through the lap wrap.
  - The polarity sequence matches the route CW from MM127 for all 73 (the best
    CCW match is 11).
  - 73/73 claims agree with the route-derived identity, with **no
    CONTRADICTION, no miss and no extra**.
- **Final stop** (10:02:16.5): last MM028, expected MM029, **candidate held**.
  - MM028 was published at 10:02:02.6, before the stop was commanded, so there
    was no hold-back lag on it.
  - The held candidate is an ensemble already fed to the recognizer and awaiting
    its acceptance (half its window beyond its end). That agrees with **MM029
    passed and the sensor at rest between MM029 and MM030**, David's stopping
    point.
  - There was no GO, so MM029 is not yet confirmed (retrospective by design).
- **Health:**
  - `block_q_drop 0`, `rvm_backlog_max 0`, worst RVM step 164 ms.
  - `heap_min 8,744` B, RVM stack at least 5,680 B free, `pub_drop 0`.
  - NSR native drops 215, UDP failures 16.
  - 16.0–16.3 V.

## Interpretation

- **Second clean cruise.** At PWM 90 there were 73 consecutive correct claims
  across the lap boundary.
- **The final position agrees with the field this time:** one magnet pending,
  versus six on run 3.
  - The difference matches the run-3 account. MM029 lay in blocks fed before
    the hold-back began, which started at the stop command (10:02:06).
  - On run 3 the stop was commanded about 22 s before standstill, so the last
    10 s of travel stayed held.

---

# Run 5, 2026-10-19 10:05: CW continuation with stops; **Otto rebooted mid-run**

**David's account:** this run continued from run 4 with no new declaration, with
a few stops on magnets. The run ended with the Hall sensor **on MM128**.

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_100013.log` | 10:00:13–10:04:02 (run 4, then rest) | `136b42beae9eee008b78a89503a60c420351e94d1a34c69f67ae02bb2d1da9fe` |
| `9950011_20261019_100403.log` | 10:04:03–10:07:43 | `45e9876c79ac3c394c17a15ec2eaadbc3383dafa7aff4f5497e5a266a61a0cba` |
| `9950011_20261019_100744.log` | 10:07:44–10:10:48 | `67bf6ab18f9ca623901af5f57f2c32fe009bd8bb0a982d8d86b3ce15af71622b` |

Output: `analysis/navi_rvm_shape_20261018/field_20261019/stops_100403/`
(analysed from the three files concatenated).

## Observations

1. **GO 1** (resumed 10:05:19, boot `8F9B71ED5DAF40BB`).
   - Stationary range 1888–1893, which is baseline.
   - 507 blocks trimmed, 245 withheld.
   - Claims **MM029–MM033** followed: MM029 was the candidate held at the end of
     run 4. The total for the boot is 78/78 claims agreeing with the route,
     MM127 → MM033.
2. **Stop 2** (10:05:57): last MM033, expected MM034.
   - Stationary range **2054–2059** (baseline about 1,890), so **the sensor was
     over MM034 (N)**.
   - GO, pattern resumed at 10:06:17.3: 187 trimmed, 413 withheld in total.
   - The released backlog peaked at **325 blocks** and was draining (289, 214,
     144, 82, 66) with no block drops.
3. **Reboot.**
   - The last loopstat of the old boot was at 10:06:18.41 (PWM 38, backlog 66,
     heap_free 27,664, heap_min 8,744, RVM stack at least 5,680 B free,
     `block_q_drop 0`).
   - The loopstats due at 10:06:19 and 10:06:20 are missing.
   - `online 0` (LWT) then `online 1` at 10:06:21.16, with **new boot
     `DAFB0CF10DD83178`**.
   - Track voltage was 16.26 V and current 0.26 A just before; the INA219 does
     not see the ESP32's 3.3 V rail.
   - **MM034 was never claimed**: the old boot rebooted while its ensemble was
     still in the backlog.
4. **After the reboot:** the navigator was **UNSET** (no declaration), the
   recognizer was inactive and there were no claims. David ran further stops and
   a PWM 101 section to MM128 on the new boot, with no navigation.
   - Its health: `block_q_drop 0`, `heap_min` at least 15,112.

## What is not known

- **The reset cause.** This build does not publish `esp_reset_reason()`, and
  there is no serial capture, so watchdog, panic and brownout cannot be told
  apart from MQTT.
- **The timing.** The only established timing is that the old boot stopped
  publishing about 1–2 s before it reset, about 3 s after a GO with a 325-block
  backlog draining.
  - That is the first GO in these runs with a backlog above 80.
  - It is a correlation from **one** event, not a cause.

## Consequence

- A reboot silently ends navigation: the session returns to UNSET, and there is
  no claim and no CONTRADICTION.
- For about 95 magnets (MM034 → MM128) Otto ran with no geographic claims, and
  nothing in navigation reported why except `state/bootid` and the `online`
  flap.

**David, afterwards:**
- Otto runs on **battery**, so the 16.26 V reading is the battery, and there is
  no track-power interruption to consider.
- David doesn't believe he rebooted Otto, but **could have done it
  absent-mindedly**.
- **Status: cause undetermined. It is not established as a firmware fault.**
  - If it is real, it will repeat. Watch \`state/bootid\` and the \`online\`
    flap in later runs.
  - No action has been taken or proposed for now (David, 2026-10-19).

---

# Run 6, 2026-10-19 10:14: CW from MM127–128, final stop (firmware `3a8953c`)

**David's account:** started from between MM127 and MM128, and stopped **just
past MM033**; Otto is sitting still.

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_101407.log` | 10:14:07–10:14:51: boot, declaration | `a047f0869501d34ba4cedcd322826488fc64b0cf9f8167583bfdc2cc80ef8d19` |
| `9950011_20261019_101452.log` | 10:14:52–10:18:10 | `5ba5d96443056066dfa629ed97b7b6b953df3091b598e8f8d60983080c72ed11` |

Output: `analysis/navi_rvm_shape_20261018/field_20261019/run6_101407/`.

## Observations

- **New boot** `EFD0225ADC42E524` at 10:14:07, while stationary and before the
  declaration (CW at 10:14:30).
- **Navigation:** **62 claims, 62/62 agree with the route**, MM128 → MM170 →
  MM000 → MM018. No CONTRADICTION, `block_q_drop 0`.
- **PWM:** about 100. The stop was commanded at 10:16:03 (throttle to 0); PWM
  coasted 98 → 74 by 10:16:12, with brake commands from 10:16:08, and fell
  below 15 at 10:16:20.5.
- **Hold-back:** from 10:16:03 the `blocks` counter stood still for about 10 s
  (the 512-block ring filling). From 10:16:13 it rose at 50/s, with the oldest
  held blocks released as newer ones arrived.
  - So the claims for MM014–MM018, published 10:16:13–10:16:19, are passages
    from about 10 s earlier.
- **STOP** at 10:16:20.5: last MM018, expected MM019, no candidate, no GO since.
  The held ring covers about the final 10.2 s of travel (Otto's block time
  about 124.7–134.9 s; MM018's midpoint is at 123.15 s).

## Discrepancy (unresolved)

**David reports the rest position just past MM033. That is 15 magnets beyond
the last claim (MM018).** The log's own timing makes 15 magnets in that interval
hard to reconcile:

- In this run, cruise spacing at PWM about 100 was **1.2–1.5 s per magnet**
  (MM001–MM013).
- From MM018 to standstill is **about 11.7 s** of travel, while PWM falls from
  about 75 to 4. At cruise spacing that is at most about 8 magnets; slowing,
  fewer.

The observation is preserved as reported and is not reinterpreted. Possible
explanations, none established:

- a different rest point;
- hold-back timing other than designed;
- block timing not matching wall time.

A GO from this stop would release the held blocks and show which magnets they
contain.

**Resolution (David, afterwards):** the rest position was **just past MM023**,
not MM033 (he misread it the first time).

- MM019–MM023 are therefore **5 magnets** passed after the last claim. That is
  consistent with the timing above (at most about 8 in about 11.7 s while
  slowing).
- Those passages lie in the final 10.2 s of travel, which is still in the
  hold-back ring with no GO since. **It is the same mechanism as the end of run
  3**, not a recognition fault.
- **Discrepancy closed.** As at run 3, the navigator's position at a final stop
  after a long ramp-down lags the real one until the next GO or declaration.

---

# Run 7, 2026-10-19 10:22: one exact lap, MM023–024 to MM023–024 (firmware `3a8953c`)

**David's account:** a run of exactly 171 MMs, from between MM023 and MM024
back to the same place.

| Log | Content | sha256 |
|---|---|---|
| `9950011_20261019_101819.log` | 10:18:19–10:22:21: rest, boot, declaration | `e36efc6e5cf66dbc8727d62e70b068a31355baa0f96e165600589b0b73b5d823` |
| `9950011_20261019_102237.log` | 10:22:37–10:25:52 (Otto offline at 10:25:52) | `6027475cbb68bc21cf452d2082ece4c32d4f2633cef43ed4186f3f7dd6f89708` |

Output: `analysis/navi_rvm_shape_20261018/field_20261019/run7_101819/`.

## Observations

- **Boot** `FA9D779D889CC1F9` at 10:21:14 (an `online` flap), while
  stationary and before the declaration (CW at 10:21:42).
- **Navigation:** **171 recognitions, all 171 MM claims**, from MM024 through
  to MM023. They are **171 unique magnets, every one on the route exactly once**.
  - The polarity sequence matches the route CW from MM024 for all 171 (the best
    CCW match is 11).
  - 171/171 claims agree with the route-derived identity, with **no
    CONTRADICTION, no miss and no extra**.
- **Final stop** (10:25:26.8): last MM023, expected MM024, no candidate held.
  This **agrees with the rest position between MM023 and MM024**, with no
  hold-back lag at this stop.
- **Margins:** minimum LLR **30.6**; waveform windows 0.24–1.50 s.
- **Health:**
  - `block_q_drop 0`, `rvm_backlog_max 0`, worst RVM step 101 ms.
  - `heap_min 9,608` B, RVM stack at least 5,680 B free, `pub_drop 0`.
  - NSR native drops 244, UDP failures 41.
  - Battery 16.06–16.27 V.

## Interpretation

- **First exact closed lap.** NAVI_RVM_SHAPE_MANUAL navigated one complete lap
  with every magnet claimed once, in order, and ended at the position David
  observed.
- **Not shown in this run:** stops over magnets (runs 3 and 5 cover those), CCW
  travel, and Toby.

---

# Run 8, 2026-10-19 10:29: slow full lap with stalls; **6 magnets missed, CONTRADICTION** (firmware `3a8953c`)

**David's account:** one complete lap, CW, from MM023–024 back to MM023–024,
**exactly 171 magnets**. There was lots of slow running, and "a few stops due to
lack of motive power".

| Source | Content | sha256 |
|---|---|---|
| `9950011_20261019_102850.log` | 10:28:50–10:29:16: boot, declaration (CW) | `2fffe179a11b27dfe63fcd585c56bdb08e3e66a74b4030a056658611468fcbf2` |
| `9950011_20261019_102933.log` | 10:29:33–10:40:44 | `006f7c65b9f1bcbf2e869ad38b0239ca66915918abf5d820bb97f50f81a551b9` |
| `9950011_20261019_104044.log` | 10:40:44–10:44:33 (e-stop 10:44:06, offline 10:44:33) | `58baa29fb9e3876e92edbaa902f47b4967230f26492a49cd8b92e91ad8fbd551` |
| Last 80,000,000 bytes of Pi `/home/david/NGR/navi_sync/navi_sync_20261015_100403.nsr` (read-only `tail -c`) | Otto's **native 5 kHz Hall and applied PWM** for boot `B29BDF0E37C0CDA1`, Otto time 170–921 s; 3,754,416 reads, 0 bad frames | `7430ec7517a48500763a12b7ddfdf5a2e38c06063b129ca986c25c6f7ace9e37` |

- MQTT analysis: `analysis/navi_rvm_shape_20261018/field_20261019/run8_102850/`.
- NSR tail decoder:
  `analysis/navi_rvm_shape_20261018/field_20261018/decode_nsr_tail.py`.

## Observations (MQTT)

- Boot `B29BDF0E37C0CDA1`, declared CW, last MM023, next MM024.
- **165 recognitions:** 85 MM claims (**MM024 → MM108, 85/85 agree with the
  route**), then 1 CONTRADICTION, then 79 `RECOGNIZED_NO_CLAIM`.
- **CONTRADICTION** at 10:40:11.9: expected MM109 (N), recognized **S**. LLR
  18.1, baseline level **1988.2** (the line is normally about 1,890), baseline
  start 593.29 s (Otto time).
- **Alignment.** With the first 85 fixed as MM024–MM108, the 80 later
  recognitions match the route **80/80 only if they start at MM114**, running
  MM114 → MM022. Every other offset from 0 to 11 matches 34–42/80.
  **MM109–MM113 were not recognized.**
- Only one STOP/GO transition, at the e-stop (10:44:06). **The stalls produced
  no STOP:** the applied PWM stayed at or above 15.
- **Health:** `block_q_drop 0`, worst RVM step **0.95 s**, `heap_min 8,996`.

## Observations (native Hall, NSR)

| Otto time (s) | Applied PWM | Hall | Reading |
|---|---|---|---|
| 590–592 | 32 | peak 2023 | MM108 passage |
| 593–603 | 32 | 1887–1895, flat | baseline |
| 604–606 | 32 | rises to about 2020 | approach to MM109 (N) |
| **606–665** | **32–35** | **2020–2034, flat (sd ≤ 1)** | **stationary over MM109 for about 60 s with power applied: a stall** |
| 665–667 | 35 → 48 | 2069, then back to 1886 | David raises PWM; Otto leaves MM109 |
| 670, 673, 676, 679 | 61 | N peaks 2060–2082, clean | **MM110–MM113 passed at speed** |
| 681–682 | 61 | S trough 1728 | MM114, the CONTRADICTION ensemble |
| 902–903 | 29 | peak 2066 | approach to MM023 |
| **903.5–911.5** | **29–38** | **about 2018, flat** | **second stall, over MM023, about 8.5 s** |
| 912–917 | 41–48 | about 1894 | off MM023, on baseline |
| 917.2 | → 0 | about 1894 | e-stop; rest between MM023 and MM024 |

The 6 unrecognized magnets are MM109–MM113 and MM023:
**165 recognized + 6 = 171**, which agrees with David's count.

## Interpretation (separate from the observations)

- **The stall is a case the STOP rule does not cover.** STOPPED is defined as
  applied PWM below 15 (David's ruling). Here Otto stood still at **PWM 29–35**,
  so the waveform was not paused and the stationary blocks were appended as
  travel.
- **MM109.** Its passage, including a 60 s plateau, is far wider than the widest
  window (444 blocks, 8.88 s), so it could not be recognized as one ensemble.
  The same holds for MM023 (passage about 11 s).
- **MM110–MM113.** These passed cleanly at PWM 61, yet were not recognized.
  - Inference, not yet replayed: no ensemble had been accepted since MM108, so
    the local baseline line ran from 593.3 s **through the 60 s plateau at about
    2,027**.
  - The reported baseline level for the next recognition was **1988**, against
    about 1,890 for the true line.
  - Judged against that line, the N excursions of MM110–MM113 were not credible
    ensembles. The large S trough of MM114 was, and its acceptance restarted the
    baseline; from MM115 on, everything was normal.
  - An offline replay of the native Hall through the Python reference (or the
    C++ replay) would test this inference.
- **The CONTRADICTION was correct behaviour.** The navigator expected N and saw
  S, and it then made no further claims.

## Open (for David)

- **The stall case.** Applied PWM does not say whether Otto is moving: these
  stalls happened at PWM 29–35, and the pause rule saw nothing. How a stall
  should be treated, and what defines STOPPED, are your decisions. Nothing has
  been changed.
- **Replay.** Whether to run the offline replay of this native Hall to confirm
  the baseline-contamination inference.

## Run 8 offline replay (analysis only, David's approval 2026-10-19)

**Source:** the last 115,000,000 bytes of the Pi NAVI sync recording, read only
(Pi file size 8,652,695,604 B; Otto offline, so the file was static), sha256
`03a67e20b05f491632c6e32e6281917158c1a62fa1d67a23cd1e4d239d567d3d`.

- Boot `B29BDF0E37C0CDA1`: 4,596,912 native reads (Otto time 1.36–921.27 s)
  and 920,256 legacy samples.
- The legacy samples carry the firmware's own **commanded** PWM, so the replay
  uses the real commanded target, not a proxy.
- The declaration (CW, 23 → 24) is at Otto time 26.99 s, from `state/nav`
  DECLARED `baseline_start_us`.

**Script:**
`analysis/navi_rvm_shape_20261018/field_20261019/run8_replay.py`.
- Firmware path: `tests/navi_rvm_shape/replay.cpp` with the firmware headers,
  `--drain 1` (bounded feeding).
- Python reference: `stopgo.firmware_mask` + `rvm_local.run`, `model_A.json`.
- Outputs: `field_20261019/run8_replay/`.

**Recording gaps** (serial jumps in the NSR copy; reads that Otto had but the
UDP recording lost): eight, the largest 912 reads at 410.4 s.
**144 reads at 700.575 s** matter here.

## Results

**1. Firmware parity** (as run, stalls included):

| Check | Result |
|---|---|
| C++ replay vs field recognitions | **164/164 matched** within 50 ms, same polarity |
| Field recognitions not in the replay | 1: **700.37 s (S, MM123)** |
| C++ vs Python | **164/164** matched both ways |
| Navigator events | **identical to the field:** 85 MM (MM024–MM108), CONTRADICTION at 682.01 s expecting MM109 (LLR 18.094, baseline level 1988.241, as in the field), then NO_CLAIM |
| Replay timing | `output_drops 0` |

The one difference, at 700.37 s, falls exactly on the 144-read recording gap at
700.575 s, inside MM123's passage. It is a recording artifact, not a firmware
difference.

**2. Counterfactual** (diagnostic only, not a proposal): only the two stall
plateaus (606.0–665.0 s and 903.5–911.5 s, applied PWM 29–35) were set to
applied PWM 0, so the existing pause rule treats them as STOPPED. Everything
else is unchanged.

| Check | Result |
|---|---|
| Recognitions | **169** (C++ = Python, 169/169) |
| **New** | **MM109** (666.85 s, LLR 18.9), **MM110** (670.63), **MM111** (673.79), **MM112** (676.81), **MM113** (679.61), all N, LLR 42–43 |
| Lost versus as-run | none |
| Baseline level at the first recognition after MM108 | **1896.8** (as run: **1988.2**) |
| Navigator | **99 MM claims**, MM024 → MM122, every one as expected; then a CONTRADICTION at 702.29 s expecting MM123 |

The CONTRADICTION at MM123 comes from the **recording gap at 700.575 s**, which
removed MM123 from the replay (Otto recognized it at 700.37 s in the field). It
is not a result of the counterfactual.

**MM023 is not recovered** by this counterfactual. Its passage (approach peak
about 2,066–2,139 at 902–903 s, a plateau at about 2,018, exit at 911.5–912.5
s, e-stop at 917.2 s) gives no recognition in either replay. **Not explained;
not investigated further.**

## What the replay establishes

- **The firmware path reproduces the field exactly,** wherever the recording is
  complete.
- **The inference is supported.** The 60 s stall plateau over MM109 entered the
  local baseline and raised it from about 1,897 to 1,988. MM110–MM113 then went
  unrecognized, and the CONTRADICTION followed. With the plateau treated as
  stopped, the same Hall data gives MM109–MM113 at normal margins (MM109 at the
  low LLR of 18.9) and correct claims.
- **Not established:**
  - how a stall should be detected on the locomotive (here the stall intervals
    were set by hand from the Hall record);
  - the MM023 miss;
  - any behaviour beyond this one recording.

**David's ruling on MM023 (2026-10-19):**
- The run-8 MM023 miss (a stall over the magnet at the end of a manual lap,
  followed by an e-stop) **does not need to be resolved**. It is not a normal
  operating condition under automatic operation.
- It stays recorded as an unexplained observation, closed and not
  investigated.
