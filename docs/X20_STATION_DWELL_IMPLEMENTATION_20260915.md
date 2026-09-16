# X20 — the station dwell. Implementation report.

**2026-09-15.** `NAVI_ONE_1_0X20_DWELL_FIELDTEST`. Built from X19 at HEAD, not
from `d99419c`.

## 0. The base commit, and why it is not the one asked for

The brief said to work from `d99419c` (18:34). That is the build whose first
flash **halted on the boot record** and never flew — `16f3e7e` (18:45) is the
fix, and the Arches run was 18:55–18:59. `37af220` (18:56) then corrected the
stack high-water units. Building from `d99419c` would have shipped a
locomotive that goes online and immediately goes stale. X20 is X19 at HEAD.

## 1. What changed

X19 is copied to `firmware/test-programs/NAVI_ONE_X20/` so X19 stays flashable
as the build that produced the field record, exactly as X19 left X18 in
`NAVI_ONE/`. The diff against X19 is:

**`ExcursionDetector.h`** — `sample()` takes a fourth argument, `stopped`,
defaulted false. When it goes true:

| principle | mechanism |
| --- | --- |
| 4, 8 — do not re-base the zero onto the parked field | `noteRest()` returns early while `restFrozen_`. **This one line is the Arches fix.** |
| 4 — no new event | declaration refused; ring, histogram, `localRef` and every diagnostic keep running; anything that would have been a candidate is counted into `suppressed_` and into a per-phase counter |
| 5, 6 — do not stitch | an in-flight window is finished **at the stop sample**, with `stoppedShort` and `measuredMs` on the record. Peak, polarity and both widths come from the moving samples alone |
| 8 — know which condition | at PWM 0, `dwellDisplaced_ = |raw − frozen rest| ≥ departCounts` → REST or IN_OLD_FIELD |
| 9A | REST: `arm()` on the departure sample — refractory cleared, persistence cleared |
| 9B | IN_OLD_FIELD: declaration stays refused and rest stays frozen until the signal is within `departCounts` of it for `persistSamples`; then OLD_FIELD_CLEAR and `arm()` |

No constant is added. `departCounts` and `persistSamples` do all of it.

**`NAVI_ONE_X20.ino`** — `stationHolding` (ZERO_RAMP or DWELL) is raised on the
loop thread; the Hall task computes `stopped = stationHolding && actualPwm == 0`
itself, so the PWM-0 edge is seen on the sample it happens on rather than a
loop pass later. Holding covers the ramp, so there is no gap at Ramp → Dwell —
and the whole ramp still has `actualPwm > 0` and therefore full detection
(principle 2 and 3). MANUAL never raises it.

**`Stations.h` is unchanged.** `StPhase::Ramp → Dwell` already fired on exactly
`actualPwm == 0`; principle 3 was already the existing semantics.

**Telemetry** — new topic `diag/dwell`, at most three records per stop:
`DWELL_BEGIN` (with `hall`: REST or IN_OLD_FIELD, and the numbers that decided
it), `DWELL_DEPART` (with `armed`), `OLD_FIELD_CLEAR`. Each carries
`suppressed`, the count of departures refused, so suppression can never be
silent. `diag/excursion` gains `stopped_short` and `measured_ms`.
`state/loopstat` gains `dwell`, `armed` and `old_field_hold_ms`.

## 2. Why polarity survives truncation

The documented way a pole inverts (`localRef()`'s own header) is a wrong zero
making the detector fire on an arc's **trailing** edge, so the recovery is read
as the arrival. A locomotive decelerating into a magnet has no trailing edge:
the signal rises toward the apex and then holds. Every moving sample is on one
side of the local zero, so the sign of the excursion sum is the true pole. In
the limit — PWM 0 one sample after detection — the sum is the detection sample
alone, which already carries ≥70 counts of **signed** departure, because that
is what declared it. There is no case with no moving evidence. Gate 15 asserts
it for both poles.

## 3. Gates

All 18 pass. Twelve are X19's, unchanged. Six are new:

- **13** reproduces Arches: parked ~100 counts into a fringe field for 30 s with
  the wander back toward the line. X19 manufactures ≥3 candidates; X20 declares
  nothing.
- **14** the zero ramp is not a dwell — a magnet crossed during deceleration is
  still one candidate with its pole.
- **15** PWM 0 mid-window, both poles: one candidate, `stopped_short`,
  `measured_ms < 400`, correct pole, and `windowEnd − detected == measured_ms`
  so no stationary sample was appended.
- **16** stopped at REST: armed on the departure sample, no refractory carried,
  next magnet counted.
- **17** stopped IN_OLD_FIELD: leaving the field declares nothing,
  OLD_FIELD_CLEAR arms as the line returns, next magnet counted.
- **18** the counterexample to 17 — the same departure under X19 produces a
  fresh candidate of the **opposite pole**. That is the Arches strike.

The 2026-09-15 replay is **byte-identical to X19's**: 11/11 next-event targets,
33/33 normal passages, 64 candidates, 0 persistent-field extras. The corpus has
no throttle in it, so `stopped` is false throughout and the moving path is
provably untouched.

`check_payload_bounds.py` — 33 records, 0 over limit. It caught the health
buffer at 332 vs 320 during this work, which is what it is for.

## 4. Size

| | flash | static RAM |
| --- | --- | --- |
| X19 | 968275 | 71516 |
| X20 | 969871 | 71644 |
| | +1596 | +128 |

Compiled clean under `--warnings all` on esp32 core 3.3.11. The new 704-byte
`diag/dwell` buffer lives on the Hall task stack, in its own frame, and is
never live at the same time as `publishExcursion`'s — the two are sequential
calls, and the stack is 8 kB with headroom published at 1 Hz.

## 5. What this does not settle

The old-field hold has no timer, per principle 9. `old_field_hold_ms` on
`state/loopstat` is the instrument that makes that safe to fly; it is the first
number to check if a departure ever fails to produce its next magnet. And
whether Otto should be stopping in a field at Arches at all remains an open
question for the operator — decision 0086 does not touch the geography.
