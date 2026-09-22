# NAVI_COHERENCE 0.4 — first field lap, 2026-09-22

Toby (9950012), Manual, CW. Operator account: started MM040–041, stopped by a
power issue near Grillers, restarted, ran the lap, coasted to a stop at ~MM055.

Source: `field-records/logs/20260922_navi_coherence_0_4_lap.log` (all Pi traffic
13:28:59–13:54:42) and `field-records/logs/20260922_navi_coherence_0_4_runs/`
(Pi run `9950012_20260922_133918` is the lap). The logs were not committed.

## Verdict

The sketch ran stably and never lost continuity. **But it was one marker ahead
of the train for the whole lap, and it never noticed.** The error came from the
position declaration, not from navigation. The sketch had the evidence to catch
it: about half of all Hall events disagreed on polarity. It treated every
disagreement as a local diagnostic.

## Findings

### 1. The declaration was one marker ahead; the Hall evidence says so without ambiguity

- The console sent `cmd/start_interval 041-042` at 13:39:18. The sketch declared
  MM41 with target MM42. The operator says the train started in **040–041**.
- The polarities observed at the 183 advancing Hall events match
  `ROUTE_POLARITY` **183/183 when the first event is MM41**. With the first event
  as MM42 (what the navigator used) the match is 85/183. That is chance level;
  every other offset scores 85–92. Every 10-marker window on the route is
  unique, so a 183-marker match has only one placement.
- The operator's start (040–041) and the Hall data therefore agree. The declared
  interval (041–042) is the outlier. **Open question:** was 041–042 chosen by
  the operator, or produced by the console? This log cannot say.

Consequence: every MM the navigator published was true MM + 1.

### 2. Polarity discrepancy never escalated

- 98 of 183 advances were `ADVANCED_WITH_DISCREPANCY`, 85 were `ADVANCED`
  (CONFIRMED). The CONFIRMED ones are the markers where the true and the
  claimed polarity happen to coincide.
- This is the designed behaviour ("wrong polarity at the expected physical
  location is diagnostic rather than a position crisis"). The design has no
  aggregate check, so a sustained ~50% discrepancy rate, the signature of a
  constant offset, produced no warning at all.
- `seq_len` reached 10 and stayed there. `seq_matches` was **0 on every event**:
  `Navigator.h` declares `sequenceMatches` but never writes it. The rolling
  mapped history (the "DNA") is recorded but never compared with the map. That
  comparison is the mechanism that would have caught finding 1 within 10
  markers.

*Implication to weigh:* when an absolute check (polarity, sequence) finds a
discrepancy and the sketch treats it as local and non-escalating, a wrong
declaration is invisible no matter how much evidence builds up. Any fix that
lets sequence evidence *move* the position is a change of authority, not a
diagnostic. It needs the operator's ruling, not an engineering default.

### 3. IR contributed nothing: the optical detector was not tracking

- The ESP-NOW link was clean. Movement car `38:18:2B:30:8C:2C`, one boot
  (`F91EA4D2DF733CF5`), 3,225+ frames accepted, 0 rejected, 0 duplicates,
  0 queue drops.
- The detector reported `INADEQUATE_CONTRAST` (reason 1) in 345 of 351 link
  samples during the lap. `TRACKING` appeared 5 times, for about 1 s each.
- As a result every decision interval was `OPTICAL_INVALID`
  (`distance_assessable` 0 on 184/184), and all 2,024 `nav/ir_compare` rows were
  `OPTICAL_INVALID` or `NO_SOURCE`.
- Pulses completed over the lap: ~3,700 × 9.652 mm ≈ 36 m, against ≈ 55.5 m
  actually travelled (182 surveyed spans, MM41→MM52). The detector undercounted by about
  a third.
- So the ±10% IR window never ran. The whole lap was navigated by the 500 ms
  timing fallback.

### 4. Hall acquisition and gating: clean

- 184 Hall events: 183 accepted and 1 refused. The 500 ms fallback gate never
  rejected a real marker. The shortest marker-to-marker interval was 894 ms.
- Opening and window polarity agreed on 184/184. Peak |signed| was 71 / 205 /
  330 (min / median / max). Raw values stayed inside Otto-class bounds, and
  nothing railed.
- The one refusal was serial 184, 402 ms after serial 183, at PWM 29 while the
  train was coming to rest. It opened already 183 counts below baseline, which
  is a re-crossing of the same magnet. It was correctly ruled
  `NON_LANDMARK_HALL`.
- `loopstat` at the end: `hall_drop 0, ir_drop 0, stale_frame 0, pub_drop 0,
  cmd_drop 0`.

### 5. Stall near Grillers: position was held

- The train stopped between Hall serial 20 and 21: navigator MM61→62, true
  MM60→61, 13:40:44 → 13:41:34, about 50 s.
- Bus voltage stayed 15.42–15.47 V at the 5 s sample rate. Current dropped to
  0.21–0.29 A. The power issue does not show on the loco's INA bus reading.
- The operator raised the throttle to 86, cut it at 13:40:55, then restarted at
  13:41:26. `THROTTLE CAPPED at experimental profile ceiling` fired 14 times
  during the climb (cmd peaked at 140, applied PWM 97).
- The first Hall after the restart was accepted as the next marker. Stop
  preserved position as designed, relative to the offset reference.

### 6. Where the train stopped

- The throttle was cut at 13:44:31 (navigator MM35, true MM34). The firmware
  ramped PWM 100→0 over about 30 s, and the train passed 18 more magnets.
- The last advancing Hall was serial 183 at 13:44:59.5, **true MM52**, which the
  navigator published as MM53. No further landmark crossings occurred.
- The operator estimated ~MM055, and later corrected this: **the last magnet
  crossed was MM53** (field observation). See the correction below.

### 7. Smaller defects seen in the telemetry

- `gap_ms` is 0 on all 184 `mm/marker` rows. `priorGapMs` is not populated.
- At 13:39:12 `cmd/session_direction CW` arrived before any declaration. The
  navigator published `DIRECTION … nav_state TRACKING, mm 0`.
  `Navigator::setDirection()` sets `Tracking` unconditionally, even from
  `Unset`. For 6 s the sketch claimed tracking at MM0 with no declared
  position. A Hall event in that window would have "advanced" from MM0.
- `cmd/auto 1` at 13:39:21 was refused as designed ("AUTO disabled pending
  supervised NAVI_COHERENCE station acceptance").

### 8. Before the lap

- Boots at 13:28:59 and 13:33:50. At both, the loco bus read 0.6–0.7 V, so the
  motor supply was off.
- The loco went offline 13:35:42 → 13:36:48 and came back at 15.46 V.
- The lap ran on boot `B072C1297E3C6F1E` with no resets.

## Correction after operator report: the last magnet crossed was MM53

The operator reports that the train crossed MM53 before coming to rest. The
navigator published MM53 at rest. That does **not** confirm the navigator's
count, for three reasons:

- In earlier runs, observed polarity matched `ROUTE_POLARITY[mm]` at offset 0:
  09-16 Otto 2373/2390, 09-18 Otto 1030/1030, 09-19 Toby 1988/2097, 09-20 Toby
  513/513. Magnets and table agree. In this run, the observed polarity matches
  `ROUTE_POLARITY[mm-1]` 184/184, with no break anywhere in the lap. That fits
  only one reading: the first detected magnet was MM41, which is also the
  operator's stated start of 040–041.
- A constant alignment from first to last event means no magnet went
  undetected mid-lap. Detected magnets were MM41 → 170 → 0 → MM52.
- The train kept moving after the last Hall event (serial 183, 13:44:59.5).
  IR pulses rose 3722 → 3737 until 13:45:04, then stopped. In the MM51→52 span
  just before, 17 pulses covered 300 mm, so 15 pulses is about 265 mm of creep.
  The MM52→53 span is 300 mm. That estimate is rough given the detector's
  state, but it puts the train at, or within a few cm of, MM53 without a Hall
  opening.

If MM53 was crossed, 184 magnets were passed and 183 were detected. The
undetected one is MM53 itself, at crawl speed and at the very end. The
navigator's MM53 at rest is then a coincidence of two errors: +1 from the
041–042 declaration and −1 from the undetected final magnet.

Still open: did the train stop on top of MM53, or clearly past it? If it was
clearly past, a Hall opening at crawl speed was missed. That is a new
acquisition finding, and it has a findable cause.

## Summary table

| Aspect | Result |
|---|---|
| Stability (resets, drops) | Clean |
| Hall acquisition / 500 ms gate | Clean, 1 correct refusal |
| Stop/restart continuity | Held |
| Position correctness | **Off by +1 all lap** (declaration); final MM53 matches only because MM53 went undetected |
| Self-detection of offset | **None** (seq_matches never computed) |
| IR distance evidence | **Absent** (INADEQUATE_CONTRAST ~98%) |
