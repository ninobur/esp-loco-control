# NAVI_COHERENCE 0.5 AUTO_ENABLED — first AUTO run, 2026-09-22

Toby (9950012), boot `F37B52F0930A43B6`, 14:53–15:21. The sequence was:

1. Manual demonstration.
2. AUTO CW.
3. AUTO CCW, during which the IR car came uncoupled at Patio. The operator
   stopped, recoupled, and resumed AUTO.
4. A slow Manual run with a reverse segment.

The operator reports the train **physically stopped in 041–042**.

Source: `field-records/logs/20260922_navi_coherence_0_5_auto_run.log`
(14:48:50–15:21:43). Governing record:
`docs/decisions/0090-…` (proposed).

## Verdict

**Navigation was correct at the end and at every station.** The navigator
finished at MM41, next expected MM42, CW, which matches the operator's
041–042.

- 723 Hall events: 710 advances and 13 refusals.
- One false advance occurred, departing Arches CW. The new sequence rule
  corrected it on its first field use (MM117 → MM116), with no station in the
  affected stretch.
- Every advance after that correction matches the map's polarity.
- AUTO served 15 station stops across Arches, Bamboo, Patio and Grillers, in
  both directions.
- When the IR car was left behind, Hall + map + sequence carried the
  navigation through three stations with no error.

## Findings

### 1. The one false advance, and the sequence correction (CW, departing Arches)

| Time | Serial | Ruling | Detail |
|---|---|---|---|
| 15:02:07.587 | 243 | accepted as MM110 | 20.8 s after the last accept (dwell), peak 101, PWM 30. IR `OPTICAL_INVALID` |
| 15:02:08.006 | 244 | refused | 402 ms later (timing gate) |
| 15:02:08.943 | 245 | **accepted as MM111** | 1.36 s later, **peak 38**, which is right at the detection threshold. IR `OPTICAL_INVALID` |
| 15:02:16.6–19.1 | 248–250 | `ADVANCED_WITH_DISCREPANCY` ×3 | MM114–116: the first markers after the all-North run 107–113 |
| 15:02:20.283 | 251 | **SEQUENCE_CORRECTED** | MM117 → MM116, offset −1, 7/10 agreed before. AUTO continued, station machine Idle |

- MM107–113 are all North, so polarity could not expose the extra advance
  until MM114.
- The most likely spurious event is serial 245: a threshold-level peak
  1.36 s after a genuine opening.
- **IR saw it but wasn't admitted.** Between serials 243 and 245 the IR car
  counted about 5 pulses (about 48 mm) against a 300 mm span. The interval was
  ruled `OPTICAL_INVALID` only because its anchor (serial 243, 07.587) was
  captured about 0.2 s before the detector reached TRACKING (07.847). From
  the anchor onward the car was counting.
  **Candidate rule (for Sam):** measure from the first TRACKING sample when
  the anchor predates it.

### 2. The IR car uncoupled at Patio (CCW)

- The car's pulse count froze at 17,800 at 15:07:32, during the Patio dwell.
  The radio link stayed up throughout (one 1 s gap at 15:09:55), so the car
  was **left standing**, not disconnected.
- A stationary car stops reporting TRACKING, so its intervals self-invalidated
  (`OPTICAL_INVALID`). The navigator fell back to the 500 ms timing gate
  without any intervention.
- Toby then served Bamboo, Arches and Grillers CCW. Every advance matched the
  map's polarity, and `seq_matches` stayed at 10/10.
- The operator released AUTO at 15:12:07, recoupled, and resumed at 15:13:29.
  IR was counting again by 15:13:53 and Patio was served normally.

### 3. IR refused a spurious event that timing alone would have accepted

Slow Manual CW, 15:21:04.670, serial 721:
- 700 ms after an accept, so it passed the 500 ms gate.
- Peak −40.
- IR said **38.6 mm**.
- Refused as `NON_LANDMARK_HALL`.

This is the same pattern as serial 245 in §1 (weak peak, more than 500 ms
after a genuine opening), with the opposite outcome because IR was admitted.
The day's two cases of this pattern differ only in whether IR was available.

### 4. The ±15% window still refused a real magnet

- 15:02:26.800, serial 257, CW, PWM 90: a strong S opening (peak −212) at the
  correct polarity for MM122. IR measured 251 mm against 300 (0.837).
  **Refused.**
- The next event was accepted as a two-step advance to MM123 (531 mm against
  600, 0.885).
- Position was unaffected. As in run 2, a magnet the Hall sensor detected is
  recorded as missed. IR read short through MM119–123 (280, 251, 270 mm).

### 5. The other refusals were magnets read twice

- The 13 refusals were: serial 257 above, serial 721 above, and eleven
  re-crossings or near-threshold openings at crawl or stop speeds.
- Most came about 402 ms after a genuine opening at PWM 17–42. Some were weak
  (peaks −38 to −63) at the Arches dwells.
- Refused by IR: serials 48, 72, 325, 396, 571, 650, 717, 723. Refused by
  timing (IR invalid): 244, 476, 478.

### 6. Reversal

- At 15:20:24 the motor direction was reversed. Travel went from CCW to CW,
  with the train last past MM39.
- The first CW event was accepted as MM39, the point just passed, which is
  correct. MM40 and MM41 followed, and the train stopped short of MM42.
- The sequence window restarted as designed (`seq_len` 1 → 3).

### 7. Station stops (navigator offsets; physical check not made)

`off` is in markers from the station centre, direction of travel.

| Station | Dir | ZERO_RAMP off | DWELL_BEGIN off |
|---|---|---|---|
| Arches | CW | 0, 0 | 2, 1 |
| Arches | CCW | 0, 0 | 2, 2 |
| Bamboo | CW | 1, 1 | 3, 3 |
| Bamboo | CCW | −1, −1 | 1, 1 |
| Patio | CW | 1, 1 | 3, 3 |
| Patio | CCW | 0, 0 | 1, 1 |
| Grillers | CW | −1 | 0 |
| Grillers | CCW | −1, −1 | 2, 2 |

- The ramp to zero consistently carries the train 1–2 markers past where it
  begins. That is 2 markers at Bamboo and Patio CW, and up to 3 at Grillers
  CCW. The stops repeat to within one marker.
- Whether these are the intended platform positions is a physical question
  for the operator. They come from the station tables, unchanged in 0.5.

### 8. Declarations and AUTO sequencing

- CW: declared 040–041 at 14:54:02, after the loco was online. `DIRECTION`
  before the declaration now reports `UNSET` (the 0.5 fix, observed at
  14:53:54).
- CCW: declared 050–051 at 15:06:28, which gave MM51 with target MM50. That
  is correct for CCW.
- AUTO and GO were admitted normally. `RELEASED by dispatcher` stopped the
  train each time.

## Summary

| Aspect | Result |
|---|---|
| Final position | MM41 → 42 CW; operator: 041–042 ✓ |
| Advances / refusals | 710 / 13 |
| Polarity agreement | 707/710; the 3 disagreements are the pre-correction MM114–116 |
| Sequence corrections | 1, correct (−1, after a false advance on departure) |
| Stations served | 15 stops, 4 stations, both directions |
| IR usable at decisions | 570/723 (`NOMINAL_ONLY`) |
| IR car left behind | Navigation continued correctly on Hall + map + sequence |
| Real magnets refused by the ±15% window | 1 (0.837), recovered two-step |
| Spurious event refused by IR that timing would accept | 1 (serial 721, 38.6 mm) |
| Spurious event accepted when IR was invalid | 1 (serial 245, corrected by sequence) |
