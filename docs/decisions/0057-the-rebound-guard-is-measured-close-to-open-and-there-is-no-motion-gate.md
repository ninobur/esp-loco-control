# 0057 — The rebound guard is measured close-to-open, and there is no motion gate

Status: Accepted (operator, 2026-08-29)

## Decision

The 500 ms rebound guard is measured from the **close of the previous accepted
passage to the open of the next candidate** — a real gap between passages.

There is **no motion gate**. No PWM threshold, no motion clock, no per-locomotive
movement table. Nothing in the accept path consults throttle to decide whether
the locomotive is moving.

## Context

`NAVI_2` gated its debounce clock on `actualPwm > 35`, derived from the
operator's observation that above PWM 35 there should be movement. On
2026-08-29 Toby ran at exactly PWM 35 for 7 minutes 21 seconds, moving at about
104 mm/s. `35 > 35` is false, so the clock never accrued, every arriving magnet
was refused as `REBOUND` with `moved_ms = 0`, and position froze at MM042 while
the locomotive covered roughly 46 metres. 110 events carry `moved_ms = 0`.

The operator then supplied the fact that removes the whole approach:

> *"With a heavy coach going downhill by Westpoint, 14 PWM will move him. Uphill
> at the same point and load, 35 might."*

**There is no PWM value that means "stopped."** Any threshold is simultaneously
too high somewhere on the route and too low somewhere else, and it varies with
grade, load and railhead. The original observation was correctly stated as a
weak sufficient condition for movement — *above 35 he is moving most of the
time* — and was wrongly inverted into a necessary one.

## Why no gate is needed

The motion clock existed to cover one case the operator identified: stop just
past a magnet, dwell, restart, and a wall-clock guard has already expired when
the rebound arrives.

Measuring close-to-open dissolves it. A locomotive that stops with the sensor
inside a magnet's field leaves the Hall reading deviated, so **the passage never
closes** — there is no guard window running, and no rebound can be admitted
during the dwell. When the locomotive moves off, the passage closes and the
500 ms starts from that instant. If instead it stops *past* the magnet, the
field cleared and the rebound already happened before the stop.

Both halves are handled by the geometry of the measurement.

Worked example, 2026-08-29 18:19:06.9–18:19:25.6: throttle cut to 0, Toby
coasted to a stop over a magnet, and the passage stayed open for 18,707 ms
across the dwell, closing when he moved off. `peak = 144`, `drift = -1`,
polarity correct. It was accepted, and accepting it was right.

## Alternatives considered

- **A better PWM constant, or a per-locomotive table.** Rejected: the Westpoint
  observation shows the threshold is per-grade and per-load, not per-locomotive.
- **IR pulses as the motion clock.** Rejected: it gives the observer authority
  over refusals, and IR has now failed silently in both directions —
  undercounting up to 39% (2026-08-26) and overcounting 3–8× (2026-08-29).
- **A maximum passage duration.** Rejected on the evidence above: the 18.7 s
  event was a real magnet, and a ceiling would have refused it. This was the
  second threshold proposed in one day that would have discarded something real;
  the first was the 140-count amplitude floor.

## Consequences

- The recognizer needs no knowledge of throttle, speed, or motion. It sees a
  waveform and the time since the last accepted passage closed.
- A dwell inside a passage produces one long passage, not several. Durations of
  seconds are legitimate and must not be capped.
- Detection becomes independent of grade, load, consist and locomotive, which is
  what makes the same sketch honest on Otto as on Toby.

## References

- decisions 0053, 0055, 0056
- `NAVI_FRESH_0_2` `MagnetRecognizer.h` — the close-to-open guard this adopts
- `docs/reviews/NAVI_FRESH_0_2_REVIEW_20260829.md`
- field evidence: `~/NGR/telemetry/all_20260829.log`, 18:19:06–18:19:25 (the
  worked example) and 18:48:05–18:55:26 (the PWM-35 freeze)
