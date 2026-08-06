# 0004 — Speed filters admit every measurement; robustness lives in the output

Status: Accepted  (2026-08-05; recorded 2026-08-06)

## Decision
No admission gate on any speed filter. Every genuinely measured interval
enters the FIFO; the output is a median over the window. The only
non-insertions are absences of measurement (no prior edge, or a gap
spanning a known blind period), flagged by the sampler — never judgements
against the buffer's own contents.

## Context
The 3×-median admission gate wedged permanently in the field: a spurious
41 ms interval seeded the buffer, the gate locked to 3× that, every genuine
interval was refused, and refused intervals can never move the median.
Speed froze at 1402.44 mm/s publishing quality "OK". Any filter whose
admission rule reads its own contents has this failure mode; widening the
multiplier relocates the wedge.

## Alternatives considered
- Wider multiplier — rejected: relocates, does not remove.
- Plausibility gates against external references — rejected: reintroduces
  conditional admission, the property under indictment.
- Mean instead of median — rejected: a mean cannot outvote a doubled or
  halved interval.

## Consequences
No state outlives the buffer; every sample ages out within the window.
Field-proven 2026-08-05: a 261,632 ms interval entered, corrupted briefly,
aged out. Revolution-time summing was later dropped for the same family of
reason (one bad interval contaminated `SPOKES + N − 1` outputs); the
contamination window is now exactly one interval.

## References
Commits `24a55e3`, `e0f7903`; `IR_TEST_STATE_AND_REACQUISITION.md`;
IR_SENSOR_NOTES "What held up".
