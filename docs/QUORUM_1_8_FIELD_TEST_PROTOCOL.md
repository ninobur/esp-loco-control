# QUORUM 1.8 field test protocol — baseline motion gate

Date: 2026-08-07
Governing spec: `QUORUM_BASELINE_MOTION_GATE_SPEC.md` §5 (Sam + CODEX reviewed)
Decision at stake: 0017 (Proposed → Accepted only if this protocol passes)
Locomotive: Otto (9950011). Total track time: roughly 45–60 minutes.

**The order is load-bearing. Stage A runs on the 1.7 currently aboard
Otto. Do not flash 1.8 first — the gate makes the bug undemonstrable, and
Stage A is the falsifier the whole decision rests on.**

---

## 0. Safety and operating frame

- All stages run in **MANUAL** — your throttle, your direction. AUTO is
  never enabled in this protocol.
- Track clear: no other locomotives powered on the Lowline.
- E-stop available on the console throughout.
- Stage A **deliberately corrupts navigation**. Expect DISAGREEs,
  EVALUATING, possibly NO_QUORUM. This is the point. Recovery is the
  normal one: stop, re-declare the start interval, continue.
- Stage C5 (deliberate stall) energizes a held locomotive. Keep PWM ≤ 30,
  watch `telem/current` (live on the dashboard since 1.7), and abort the
  row if current climbs above ~1 A or anything warms.

## 1. Pre-flight checklist (5 min)

- [ ] Otto powered, on the main, connected to the broker
      (dashboard tiles updating).
- [ ] Confirm Stage-A firmware is 1.7: `state/bootid` shows
      `"sketch":"QUORUM_1_7"` **and** a passing marker line carries
      `"pwm":` and `"v":`.
- [ ] Console ready to re-declare a start interval (you will need it
      after A1).
- [ ] Pick your two park spots in advance:
      - **MAGNET spot** — sensor directly over a marker magnet. Confirm
        by parking and reading `delta` in `state/loopstat` (dashboard or
        capture): **|delta| ≥ 100** and steady. Note the sign:
        **positive = N pole, negative = S pole.** Note the MM number.
      - **CLEAR spot** — sensor well away from any magnet: **|delta| ≤ 5**
        and steady.
- [ ] Terminal ready for captures. Every stage uses the same command, only
      the filename changes:

```bash
cd ~/esp-loco-control
mosquitto_sub -h 192.168.68.142 \
  -t 'ngr/loco/9950011/state/loopstat' -t 'ngr/loco/9950011/mm/marker' \
  -t 'ngr/loco/9950011/state/nav' -t 'ngr/loco/9950011/alert' \
  -t 'ngr/loco/9950011/telem/voltage' -t 'ngr/loco/9950011/telem/current' \
  -v -W 600 > field-records/logs/20260807_<STAGE>.log
```

`-W 600` self-terminates after 10 minutes; re-run it fresh per stage.
**Start the capture BEFORE the locomotive stops moving, every time.** The
corruption self-erases in ~30–60 s of driving; a capture that starts after
the dwell proves nothing.

Quick trackside baseline check (run against any capture, live or after):

```bash
grep -o '"baseline":[0-9]*' field-records/logs/20260807_<STAGE>.log | sort | uniq -c
```

One dominant value = stable. A ladder of values = migration.

---

## 2. Stage A — pre-fix reproduction, on 1.7 (the falsifier)

### A1 — magnet park (~6 min)

1. Start capture → `20260807_A1_prefix-magnet-park.log`.
2. Drive Otto normally for ≥ 60 s (establishes the healthy baseline in
   the log).
3. Stop with the sensor **on the MAGNET spot**. Throttle to 0. Confirm
   |delta| ≥ 100.
4. Sit **≥ 120 s**. Watch `baseline` in loopstat once a minute.
5. Drive off at normal cruise. Keep driving **≥ 60 s** past ~10 markers.
6. When navigation degrades, let it: do not rescue it early. After the
   minute, stop, note the nav state, and recover (re-declare interval).
7. Stop the capture (Ctrl-C if before the 10-min timeout).

**Predictions (1.7):**
- During the dwell, `baseline` in loopstat **migrates toward the magnet
  level** — tens to ~200+ counts within ~60 s — while Otto sits still.
- An `mm/marker` line appears mid-dwell or at departure with a huge `ms`
  and/or extreme `drift`.
- After departure: wrong-polarity readings, DISAGREE events, miss streak,
  EVALUATING with a flat score vector; possibly NO_QUORUM.

**PASS (diagnosis confirmed):** baseline migration ≥ 40 counts during the
dwell (i.e., beyond the ±38 window) with the departure disturbance.
**FAIL (diagnosis wrong):** baseline holds within ±5 for the whole dwell.
→ **STOP THE PROTOCOL.** 1.8 is not flashed; bring me the log.

### A2 — control park (~4 min)

Same steps at the **CLEAR spot** (120 s dwell) →
`20260807_A2_prefix-clear-park.log`.
**Expected:** baseline steady (±3), clean departure, zero DISAGREEs.
This row proves the effect is the magnet, not the stopping.

---

## 3. Gate decision

A1 PASS + A2 clean → proceed to Stage B. Anything else → stop, captures
to me, no flash.

---

## 4. Stage B — flash 1.8 and verify (~5 min)

1. Flash from the repo (working tree = committed 1.8), or use the
   verified binary (SHA-256 `f92a45ef113ef875…`):

```bash
arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 --input-dir /private/tmp/claude-501/-Users-davidbrown-esp-loco-control/f0b2ef2a-25fc-4603-a10b-9453e86a1324/scratchpad/v18/out
```

2. Boot-serial verification, in order — all three lines, not just the
   first (the bootid lesson):

```
[BOOT] QUORUM_1_8 — 9950011
[INA] ready
[CAL] 2 s baseline — keep clear of magnets
```

3. Boot **clear of magnets** (the `[CAL]` instruction is now the only
   unconditional baseline authority — that's the design).
4. Set session direction and start interval; drive a few markers; confirm
   AGREEs and `pwm`/`v` still on the marker line.

The *behavioural* proof of genuine 1.8 is Stage C row 3: a frozen
baseline under a big steady delta. Version strings alone don't count.

---

## 5. Stage C — acceptance matrix on 1.8 (~25 min)

Five parks. Same choreography as A1 for each: capture first → drive ≥ 60 s
→ park **> 70 s** (the u16 saturation threshold is 65.535 s — 70+ makes it
unambiguous) → drive off ≥ 60 s → stop capture. One log per row:

| Row | Park condition | File suffix | PASS criteria |
|---|---|---|---|
| C1 | CLEAR spot | `C1_clear` | baseline ±3 throughout; clean restart; no DISAGREE |
| C2 | **Fringe**: edge of a magnet's field, steady 10 ≤ \|delta\| ≤ 35 | `C2_fringe` | baseline ±3 throughout (**this row is why the motion gate beats an excursion gate** — 1.7 would have crept) |
| C3 | **N magnet** (delta ≥ +100) | `C3_north` | baseline **frozen**; saturation checklist below; polarity **N** |
| C4 | **S magnet** (delta ≤ −100) | `C4_south` | same, polarity **S** |
| C5 | Magnet, **PWM ~25–30, physically restrained** (chock/hand, ≤ 30 PWM, watch current) | `C5_stall` | baseline **migrates** — the documented residual, reproduced on purpose. Not a failure; a boundary made visible |

**Saturation checklist for C3 and C4** (CODEX #3 — check in the log):

- [ ] Exactly **one** `mm/marker` event for the parked magnet
- [ ] `"ms":65535` (the capped stopwatch)
- [ ] `"drift"` near 0 (frozen baseline = nothing drifted during the event)
- [ ] Polarity = the pole you parked on (N for C3, S for C4)
- [ ] MM advanced by exactly **one** for that magnet
- [ ] The **next** magnet after departure is a normal AGREE, not rejected
      as a phantom
- [ ] No weird station/age behaviour from the old arrival timestamp

Note for C3/C4: the marker event may arrive at *departure* (the event
closes when the magnet clears). During the dwell itself the marker line
stays silent while an event sits open — that silence is expected.

---

## 6. Stage D — regression lap (~5 min)

Capture → `20260807_D_regression-lap.log`. One full lap at normal cruise,
plus a short **low-throttle crawl segment** (PWM < 40 over a few markers,
to confirm LOW_PWM-gated markers still navigate).

**PASS:** `miss_streak` 0 for the lap; `loop_max_gap_ms` 33–34-ish and
`hall_task_max_gap_ms` 2–3 (the 2026-08-06 baseline); crawl markers
accepted with `timing_gate":"LOW_PWM"`; `pwm`/`v` present throughout.

---

## 7. Afterwards

Commit all logs to `field-records/logs/` (or hand them to me — I'll
analyze, commit, and write the field verdict). Outcomes:

- **A PASS + C/D PASS** → 0017 promoted to Accepted; field verdict
  committed; Station Stop v1 unblocked.
- **A PASS, any C/D row FAIL** → 1.8 stays flashed only if the failure is
  benign; otherwise reflash 1.7 (archive tag `daf468d~` builds it) and we
  regroup on the spec.
- **A FAIL** → protocol stopped at §3; diagnosis reopened from the A1 log.

## Abort criteria (any stage, any time)

- Current above ~1 A sustained, or anything warm → throttle 0, abort row.
- Runaway or unresponsive console → E-stop.
- Weather/track hazard → stop; captures are cheap, redo the row later.
