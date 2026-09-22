# NAVI_COHERENCE_0_5_AUTO_ENABLED

Toby (9950012). **AUTO is enabled.** This is a field test, not an accepted
build. 0.4 is preserved unchanged as its predecessor.

Built from the operator's rulings of 2026-09-22, recorded in
`docs/decisions/0090-sequence-overrules-declaration-15pct-window-auto-enabled.md`.
That record is proposed until the operator ratifies it.

## Changes from 0.4

1. **AUTO enabled.** The sketch sets `NGR_ENABLE_EXPERIMENTAL_AUTO 1`.
   Admission is unchanged: declared position, no e-stop or low voltage,
   forward only.
2. **IR eligibility window ±15%** (was ±10%).
3. **A 10-magnet sequence overrules the position.** The last 10 observed
   polarities at consecutive mapped points are compared with the map.
   - Position moves only when exactly one route offset matches 10/10 and the
     current position disagrees on at least 2. A single misread never moves it.
   - The window restarts on declaration, on reversal, and after a multi-step
     advance.
   - On correction the sketch publishes a retained
     `state/nav {"event":"SEQUENCE_CORRECTED",...}`, sets the sticky warning
     `POSITION CORRECTED by 10-magnet sequence: MMaaa -> MMbbb`, and sets trust to
     `SEQUENCE_RECOVERED`.
   - Corrections apply at any time. Station logic relies on the corrected
     position: no stop and no reset. An approach the correction jumped into is
     armed where it lands.
4. **Direction before declaration is not a position.** 0.4 reported
   `TRACKING mm 0` when a session direction arrived before a declaration.
   `admitAuto()` would have accepted that as a known position.
5. **Telemetry:**
   - `seq_matches` now reports agreement at the current position (always 0 in
     0.4).
   - `seq_len` is the observed window, 0–10.
   - `mm/marker` gains `trust` and `corrections`.
   - `state/bootid` gains `ir_window_pct` and `sequence_authority`.

The Hall acquisition, recognizer, IR adapter, station tables, cruise profile,
and stop/reversal rules are unchanged.

## Verification

- Host suite: `g++ -std=c++17 -fsanitize=address,undefined -Wall -Wextra -I.
  tests/test_coherence.cpp`. It passes.
  - Offset corrections: every start × both directions × ±1..3 lands on the
    truth at the 10th magnet (2,052 cases).
  - Single-misread holds: every start × both directions × any one flip in 20
    magnets (6,840 cases).
- `tests/test_station_correction.cpp` (same flags): arming after a correction
  at every station and direction, offsets −9..−6 (32 cases). Ordinary running
  still arms only at −10.
- ESP32: `arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all --clean`.
  Zero sketch warnings, 1,002,391 B (76%).

## First AUTO run — suggested

- Declare **after** Toby shows online. The console does not resend a
  declaration made while the loco is offline.
- Supervise with a hand on stop. Station behaviour has never run under this
  navigator.
- If a `SEQUENCE_CORRECTED` event appears, note where Toby physically was.
