# NAVI_EYES_WIDE_OPEN

Toby (9950012) development control candidate, 2026-09-28. **Not flashed, not
field tested, not field accepted; ESP32 compile pending.** Do not flash Toby
or Otto until David and Sam have reviewed it.

Built from NAVI_COHERENCE 0.6 POSITION_STATIONS_R1_20Q3 + NSR1 (`3d6a1d3`).
It removes X22R from the Hall → NAVI path:

- the Hall task only acquires (median of five conversions per 1 ms reading,
  into a sample ring and the NSR1 recorder);
- NAVI reads every native sample, finds MM openings (≥70 counts from NAVI's
  own reference on two consecutive same-sign samples; polarity fixed at the
  second) and judges every one;
- NAVI's Hall reference is spatial: after each accepted opening, 0–100 mm of
  IR route travel is clearance, 100–200 mm is collected, and at 200 mm the
  median becomes the reference for the next MM; stationary spans add nothing;
- openings at motive PWM 0 carry no navigation authority, and IR pulses at
  PWM 0 are not route displacement (0106);
- with valid IR distance, a wrong-polarity opening is held as contradictory
  evidence, not accepted as the expected MM.

Everything else (IR health and Epochs, map, stations, manual control, AUTO,
motor control, declarations, telemetry, NSR1) is carried over from 20Q3.

Full change record, reconciliation table, audits, test results and open
review items: [`CHANGES_EYES_WIDE_OPEN.md`](CHANGES_EYES_WIDE_OPEN.md).

Build like the other NAVI_COHERENCE variants (Arduino sketchbook =
`<repo>/firmware`, `credentials.h` in `firmware/programs/QUORUM/`). Host tests:
see §14 of the change record.
