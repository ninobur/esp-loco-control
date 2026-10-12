# MM045 STOP — evening telemetry analysis, October 9, 2026

**Analyst:** Claude (Claude Code), with David and Sam. Analysis performed
2026-10-10; read-only.
**Source:** `field-records/logs/20261009_MM045_EVENING/all_20261009.log.gz`
(commit `bf95cc4`; original SHA-256
`56292fad9aa28498f93134e20b221b22bb18ac13f91e202340229dcb7c491978`), Otto
`9950011` topics only. Times are the log's own labels.
**Firmware read:** `NAVI_EWO_0_1_MM045_STOP` at `6bf81a1` and branch head of
`codex/navi-cruise-stop-20261009`.
**Companion records:** [field record](../field-records/20261009_MM045_CW_FIRST_TWO_STOPS.md)
(operator observations) and CODEX's
[run 1 investigation](NAVI_MM045_NAVIGATION_INVESTIGATION_20261009.md).

**Principles applied:** observation, interpretation and decision are kept
separate (`AGENTS.md` §5); each claim below is labelled as log evidence,
calculation or hypothesis. Nothing here changes firmware, thresholds, targets
or navigation authority; the questions in the last section are David's to
decide.

## 1. Sessions

| Run | Boot | Declared / GO | Ended | Hall accepted / missed |
|---|---|---|---|---|
| A | `FD2F3506…` | 055–056 / 18:56:49 | 18:58:56 at MM149: obstruction stop and power cycle (operator-confirmed) | 94 / 0 |
| 1 | `E71CF024…` | 025–026 / 19:03:34 | dispatcher release 19:13:51 | 142 / 218 |
| 2 | `E71CF024…` (no reboot after run 1) | 040–041 / 19:31:33 | dispatcher release 19:40:25 | 384 / 0 |
| 3 | `2A0C8220…` | 040–041 / 19:49:11 | E-stop 19:54:39 | 71 / 104 |

- **Run A** ends with Otto's messages stopping abruptly while cruising and the
  IR span falling to 31 (a stationary wheel); the broker marks it offline 23 s
  later. Dispatcher STOPs at 18:59:59–19:00:35 were sent while it was off.
- **IR car dislodged and reseated** between runs A and 1: at 19:02:04–19:02:10
  the span falls 399 → 287 → 15 and pulses stop while PWM is 70; by 19:03:00 the
  span is 319–351 at PWM 20. The IR transmitter keeps the same boot ID.
- **Between runs 2 and 3** the car was moved to the table and Otto was
  reflashed with edited ramp values (reboots at 19:43, 19:47, 19:48). Run 3's
  braking (about one PWM count per 300 ms) matches `93d5226` (300 ms final
  step, 150 ms AUTO acceleration). The boot record carries no Git revision, so
  this is a match, not an identification. `NaviIntegratedCore.h` is unchanged
  between `6bf81a1` and the branch head.

## 2. Stopping behaviour

Target: MM045 + 150 mm (midway to MM046), referenced to the Hall sensor.

| Stop | Final-ramp step | Entered FINAL (short of target) | Motion ended | Moved while braking | Final position (short of target) |
|---|---|---|---|---|---|
| Run 1, stop 1 | 31 ms | 296 mm, PWM 31, 64 mm/s | coasting; PWM 0 within 0.83 s | ~20 mm | 276 mm |
| Run 1, stop 2 | 31 ms | 296 mm, PWM 31, 54 mm/s | coasting; PWM 0 within 0.86 s | ~10 mm | 287 mm |
| Run 2 (three stops) | 31 ms | 296 mm, PWM 31 | coasting; PWM 0 within ~1.7 s | 10–20 mm | 276–286 mm |
| Run 3, stop 1 | 300 ms | 296 mm, PWM 31, 32 mm/s | at PWM ~22–26 | 29 mm | 267 mm |
| Run 3, stop 2 | 300 ms | 296 mm, PWM 31, 21 mm/s | at PWM ≤27, immediately | 0 mm | 296 mm |

- **Log evidence:** entering the final MM switches the overlay to BRAKING and
  commands PWM 0 (`NaviStopOverlay.h:73`, `:176–180`), as the implementation
  record specifies (`NAVI_MM045_STOP_IMPLEMENTATION_20261009.md:99–104`). The
  overlay computes 30 → 20 → 0 aims but does not use them.
- **Finding:** every stop ends about 0.9 MM before the target, at about
  MM044.6, matching the operator's 044.5. **The stop position is set by where
  braking starts, not by the ramp rate.** With the slow ramp Otto stopped
  moving at PWM ~22–27; with the fast ramp it coasted on momentum.
- **Correction to earlier advice:** during the session Claude suggested a
  slower time-based final ramp (about 300 ms/count) would carry Otto through
  the final MM. Run 3 shows it does not. Commit `0fdc6c9` (hold PWM to MM045,
  then ramp) addresses the right variable, but see §5 for its stall risk.
- Every completed stop dwelled at PWM 0 until the firmware's 5 s timer
  released it, and restarted, except run 3's second stop, which was E-stopped
  0.4 s after release. The PWM-0 movement warnings in the log (19:26, 19:31,
  19:42–19:43) all occurred outside AUTO, during manual handling.

## 3. Run 1, third approach — stall, not a failed restart

At 19:12:15 Otto stopped in the PENULTIMATE section at applied PWM 31: IR speed
0, NAVI position frozen 308 mm before the final section, AUTO running, no
warning, until the dispatcher release at 19:13:51 (1 min 36 s). Physical
location was about MM062.5 (operator). The PENULTIMATE aim is a function of
position only, so with position frozen PWM stayed at 31 indefinitely. Otto
never reached BRAKING, DWELL or RELEASED, which is why it did not restart. On
the two earlier approaches (physically about 044 and 054) PWM 31 still moved
Otto at 53–64 mm/s.

## 4. Location drift

### 4.1 IR acquisition irregularities (log evidence)

- **Run 1, from 19:05:22 near MM095:** IR span spiked to 1327 and settled at
  about 255–303 (previously 383–431). From then until the end, every 1 Hz
  sample reported `InadequateContrast` (fault 6) and readiness `Unavailable`,
  and aborts rose one-for-one with completed pulses. Pulse counting continued.
  In the onset second only 6 pulses arrived instead of about 26. The per-pulse
  event stream reported almost nothing after the onset, so the test in §4.2
  cannot be applied to run 1. Aborts are diagnostic; they are not a count of
  lost spokes.
- **Run 3:** contrast healthy (fault 0) and aborts 0 throughout motion, yet the
  same drift occurred.

### 4.2 Periodic long pulse intervals

Method: per-pulse `telem/ir_pulse` events with `fresh=1`, restricted to steady
running (21-pulse local median interval below 60 ms); a "long" interval is more
than 2× the local median. "Uncounted time" sums each interval's excess over the
local median, as a fraction of total time.

| Run | Steady pulses | Long intervals per pulse | Typical long / median | Most common spacing (counted pulses) | Uncounted time | Observed drift |
|---|---|---|---|---|---|---|
| A (before foot incident) | 1,646 | 0 | — | — | 0% | none |
| 2 | 7,123 | 1.6% | ~2.1–2.8× | 8, 9 | ≈2.4% | none |
| 3 | 3,669 | 5.6% | ~2.5–3.2× | **8** (85), 18, 9, 17 | ≈8.6% | ≈15.5 of ≈166 markers ≈9.3% |

- **Calculation:** run 3's uncounted fraction matches its drift. The excess
  time is distance the IR count did not register, whatever the mechanism.
- **Hypothesis (unconfirmed):** the IR car or its measuring wheel has been
  defective since the foot incident and worsened after later handling: a
  missing or damaged spoke, a binding or slipping wheel, or an optical counting
  fault. The regularity favours a recurring wheel or optical cause over random
  vibration. Eight counted pulses cannot be assumed to be one revolution.
- In run 2 the error was about 7 mm per magnet interval, inside NAVI's ±45 mm
  acceptance window, so every magnet was accepted and corrected the count.
  This is why run 2 did not drift.

### 4.3 How the IR error became location loss

- **Hall kept working.** Run 1 before its fault: 69 detections, 69 accepted.
  After: 235 detections, 73 accepted. (Detections can repeat per magnet and
  only the expected polarity is registered, so these are not magnet counts.)
- **Acceptance gate:** `NaviIntegratedCore.h::observeHall` accepts a magnet
  only if IR distance since the last accepted magnet matches the map within
  ±15% of *one* interval (≈±45 mm). The tolerance does not widen across
  consecutive misses.
- **Rejections:** in run 1, 181 of 303 detections were rejected; 159 of those
  were within ±15% of the *cumulative* mapped distance but outside the
  one-interval window, with IR 5–10% short and the deficit growing steadily
  along each stretch (e.g. from MM101: −65, −135, −238, −249 mm by MM111). In
  run 3 rejected detections were 74–230 mm short (interquartile range).
- **Fallback:** each rejection became a MISSED_MAGNET; NAVI advanced by IR
  distance alone (`evaluateMissing`) and, with IR short, fell behind.
- **Wrong-number acceptances:** in run 3, for each later group of three or more
  consecutive accepted magnets, the lags consistent with the magnet polarities
  (`ROUTE_POLARITY`) progress 3 → 6–8 → 9 → 9 → 11 → 14 → 15 (k = 0 is always
  trivially consistent and is excluded once the frame has drifted). The final
  15 matches the physical 059–060 against dashboard 044. NAVI re-anchored to
  real magnets under the wrong numbers, locking the offset in, while
  `position_reliable` stayed 1.
- Where adjacent magnets were both accepted, the IR/map ratio has median 0.997
  (101 pairs, run 1): IR scale is correct when counting is clean.

### 4.4 Central finding (David, Sam and Claude)

NAVI detected each Hall/IR disagreement but interpreted every one as a missed
magnet, never as IR error, and had no response to persistent disagreement.
Optical faults and aborts were diagnostic only and did not withdraw IR distance
authority, and periodic pulse irregularities were not assessed as counting
faults. A possible IR measurement defect therefore exposed a separate
navigation weakness: NAVI did not recognize or reconcile the growing
disagreement between its location and the railway.

## 5. Questions for David (nothing changed)

1. Should NAVI keep IR distance authority while the IR instrument reports a
   fault such as `InadequateContrast`?
2. How should persistent Hall/IR disagreement be handled under Decision 0116 —
   in particular, a run of rejected Hall detections that are consistently
   short against the map, and acceptances that fit only a shifted frame?
3. The stop overlay sets PWM from position only, so a stall freezes it at a PWM
   that cannot move the train (§3). How should a stall in the approach be
   handled?
4. Final-MM braking: stops land about 0.9 MM short (§2). `0fdc6c9` holds PWM 35
   to MM045 before ramping. Risk: run 3's second approach had slowed to 21 mm/s
   at PWM 31, and run 1 stalled at PWM 31 near Grillers; if Otto stalls before
   MM045 the overlay would hold 35 indefinitely.

## 6. Recommended next steps

- **Bench check of the IR car:** mark the measuring wheel and compare pulse
  counts and the positions of long intervals with its angle over several
  revolutions; check that the wheel spins freely and stays in rail contact.
- During the next run, watch `ir_health_fault`, `ir_abort` and the long-interval
  rate; a healthy run here showed fault 0, span about 380–430 and an abort count
  that barely moved.
- Record each manual action (stops, reseating, redeclaration, reflash) with time
  and place; matching these against telemetry resolved most questions here.
