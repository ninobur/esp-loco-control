# NAVI_COHERENCE 0.4 — second field run, 2026-09-22

Toby (9950012), Manual, CW, boot `B1167E20B61E6365`. Declared 143–144; the
operator reports it **stopped just past MM050**.

Source: `field-records/logs/20260922_navi_coherence_0_4_run2.log` (all Pi
traffic from 14:15:00 to 14:23:30). Companion to
`NAVI_COHERENCE_0_4_FIRST_LAP_20260922.md`.

## Verdict

**Correct from start to finish.** Toby passed 78 magnets, MM144 → 170 → 0 →
MM50. The navigator published MM50 and the train rested about 30 mm past it,
which matches the operator's report. IR was usable on every decision, and its
distances agreed with the survey to a median of 0.997. The ±10% IR window
refused 4 real magnets, 5% of those passed. Each time the next magnet
recovered position through a two-step advance, but the refused magnet is
recorded as "missed" when the Hall sensor actually saw it.

## Findings

### 1. Position

- The declaration went in with the motor in reverse: `DECLARED mm 144, tgt 143,
  CCW`. Then `cmd/direction 2` arrived. `setDirection` made MM144 the first
  expected point CW, which is correct for a train sitting in 143–144.
- The Hall sequence was MM144 first (14:20:48) and MM50 last (14:22:37.4).
  That is 74 accepted events covering 78 magnets (4 two-step advances).
- Observed polarity matches `ROUTE_POLARITY[mm]` on **74/74** advancing events
  at offset 0. At ±1 the match is 38–39/74, chance level.
- After MM50 the IR count rose 3 pulses (about 29 mm), then PWM went to 0.
  That fits "just past 050".

### 2. IR

- Link: movement car restarted as boot `DD7EEC2E52B071AD`, no rejects, no
  drops. The optical detector was TRACKING through the run and dropped to
  reason 1/5 only once the train was at rest.
- Decision intervals: `NOMINAL_ONLY` (usable) on **79/79**, against 0/184 on
  the first lap.
- IR ÷ mapped distance on the 69 single-step advances: min 0.901, p10 0.933,
  median **0.997**, p90 1.042, max 1.076. Six were over 1.05 and twelve under
  0.95, so several accepted spans sat near the window edge.

### 3. Real magnets refused by the ±10% window

| Serial | Magnet refused | Polarity (obs / map) | IR / map | Next event |
|---|---|---|---|---|
| 104 | MM152 | N / N | 347 / 300 = **1.158** | 105: two steps to 153, 627 / 605 |
| 113 | MM161 | S / S | 347 / 300 = **1.158** | 114: two steps to 162, 627 / 605 |
| 143 | MM20 | S / S | 270 / 320 = **0.845** | 144: two steps to 21, 589 / 635 |
| 145 | MM22 | S / S | 261 / 315 = **0.827** | 146: two steps to 23, 560 / 620 = 0.903 |

- In all four cases the refused Hall event had the polarity of the magnet it
  was refused as, so these were real landmarks, not spurious openings.
- 152 and 161 both read exactly 36 pulses (347.472 mm) over a surveyed 300 mm,
  and their following spans came out short (about 280 mm). That pattern points
  to where the magnet sits within the span, or to Hall opening position, not to
  IR scale.
- MM19→20→22 reads short for two spans in a row. MM19–21 are surveyed at 320 and
  315 mm, above the route's typical 300. The survey there could be long. That
  can't be decided from one pass.
- Serial 146 was accepted at 0.903, just inside the window. One more percent and
  the navigator would have gone three steps without a landmark.
- No position error resulted. The cost is labelling: `MISSED_OBSERVATION` is
  recorded for magnets that were detected and then refused.

*Implication to weigh:* the ±10% window is a deliberate stress-test setting,
and the README calls it experimental. At 10%, per-span IR spread plus survey
error already refuses 1 real magnet in 20. Widening it, or basing it on
cumulative distance instead of a single span, is a navigation-behaviour change
and needs the operator's ruling.

### 4. Correct refusal

- Serial 173 came 402 ms and 19 mm (IR) after MM49, at PWM 24. It opened at
  raw 2059 against baseline 1850, a re-crossing of MM49 as the train slowed.
  It was ruled `NON_LANDMARK_HALL`. IR agreed: 19 mm.

### 5. Console: a declaration sent to an offline loco is silently lost

- Toby was off from 13:55:05. The console sent `cmd/start_interval 052-053` at
  14:15:56 and 14:16:00, and Toby came online at 14:16:18.
- Toby never received it and ran with `NO_POSITION` (63 Hall events ignored)
  until the operator stopped and declared again.
- The console gave no warning and did not resend the declaration on
  reconnect. One more declaration was correctly refused by the firmware:
  14:20:04 `REFUSED: set session_direction first`.

### 6. Carried over from the first lap, still present

- `seq_matches` is 0 on every event, so the sequence comparison is still not
  computed.
- `gap_ms` is 0 on every event.
- `DIRECTION` before declaration reported `nav_state TRACKING, mm 0` again
  (14:20:10).

## Summary

| Aspect | First lap | Second run |
|---|---|---|
| Declaration | 041–042 (train was in 040–041) | 143–144, correct |
| Position at rest | Published MM53, train in 052–053 (+1) | Published MM50, train just past 50 |
| Polarity match | 184/184 at offset −1 | 74/74 at offset 0 |
| IR usable | 0/184 | 79/79 |
| IR ÷ map (accepted) | — | median 0.997 |
| Real magnets refused | 0 | 4 of 78 (±10% window) |
| Spurious openings refused | 1 | 1 |
