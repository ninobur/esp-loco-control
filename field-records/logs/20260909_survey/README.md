# 2026-09-09 — Otto's bidirectional Hall survey (QUORUM_1_13X)

Otto (9950011). Curated off the Pi; the originals live in
`~/NGR/telemetry/all_20260909.log` on 192.168.68.142.

Collected on `firmware/test-programs/QUORUM_1_13X`, PWM 90, manual drive,
new ESP32 with external antenna. See [docs/OTTO_SURVEY_BUILD_20260905.md](../../../docs/OTTO_SURVEY_BUILD_20260905.md)
for why this sketch and not an IR/ESP-NOW build.

## What's here

| file | what it is |
|---|---|
| `otto_1_13X_cw_waveforms.log.gz` | every `mm/marker` + `mm/wave` record, CW, post-INA219-fix only |
| `otto_1_13X_ccw_waveforms.log.gz` | every `mm/marker` + `mm/wave` record, CCW |

## Time windows and the INA219 cut

The board's `telem/current`/`telem/power` were dead (flat 0.17 A) on a bad
INA219 sensor for the first part of the session. The operator swapped the unit
mid-run; nothing else about the recognizer or wiring changed. Because it's not
clear whether the same power-supply fault that killed the current reading
also touched anything upstream of the Hall path, the pre-fix CW stretch is
excluded here rather than assumed clean.

- **Boot at 21:04:17** is the first boot after the INA219 swap (`state/bootid`
  timestamp; matches the operator's own "much better run" report at the time).
- **CW window kept:** `21:04:17` – `21:29:14` (session_direction flips to CCW).
  Everything CW before `21:04:17` — three earlier boots, the stall, the
  NO_QUORUM stretch — is **discarded**, not included in this directory.
- **CCW window kept:** `21:29:14` onward, no discard (whole CCW run is
  post-fix).

## Coverage

| | CW (kept) | CCW |
|---|---|---|
| `mm/wave` | 1,481 | 2,609 |
| `mm/marker` | 1,084 | 2,083 |
| Distinct mm positions | 171/171 | 171/171 |
| Admitted (`rej:0`) | 1,083 | 2,081 |
| Floor-rejected (`rej:1`) | 2 | 1 |
| Sub-threshold (`rej:2`) | 396 | 527 |
| `clip:1` (quality flag) | 1 | 5 |
| `tr:1` (truncated capture) | 24 | 24 |

Combined 4,090 waveforms — full 171-position circuit coverage independently
confirmed in both directions. For comparison, Toby's 2026-08-28 combined
twelve-lap dataset was 2,092 records.

## Known artifacts

- The `clip:1` and `tr:1` records above are real and left in, not filtered.
  Per the 2026-08-28 survey's own convention: `clip` should be 0, and a `tr:1`
  (truncated) capture "may be judged on jitter, amplitude and duration but
  never on shape, because a cropped arch reads as flat-topped."
- The very first CCW record (`mm:46`, `dur:33762` ms, `pwm:0`, `timing_gate:
  "LOW_PWM"`, `clip:1`) is the direction-switch dwell, not a real magnet
  pass — the loco sitting stopped while `session_direction` flipped, not a
  36-second traversal. Exclude it from any duration-based analysis.
- The first several markers after each boot carry `timing_gate:"RAMP"` or
  `"LOW_PWM"` and `dt_conserve_ratio:-1.00` while PWM ramps up from a stop —
  expected, not a fault, but not representative steady-state timing either.

## Reading a waveform record

Same format as the 2026-08-28 survey — see
[`field-records/logs/20260828_survey/README.md`](../20260828_survey/README.md#reading-a-waveform-record).
