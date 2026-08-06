# 0006 — Envelope adaptation gates on signal activity, not wall clock or completed pulses

Status: Accepted  (2026-08-05/06; recorded 2026-08-06)

## Decision
Adaptive-envelope decay runs only while a short rolling range of raw
samples shows the signal varying. Flat trace: hold, including phase.
Varying trace: decay — at the normal cadence while edges flow, faster and
bounded while blind (active but edgeless), because decay is then the only
path back to sight. Expansion stays unconditional. IR_DIAG goes further:
its bounds are rolling-window percentiles, robust by construction.

## Context
Two predecessors failed in the field. Wall-clock decay collapsed the span
to 11 counts in four minutes of station dwell (a stationary wheel presents
a constant — nothing to adapt to). Gating on completed pulses fixed that
and created a deadlock: a stale wide band produces no completed pulses, so
the gate disables the only mechanism that could narrow it — 479 s blind.

## Alternatives considered
- Tuning `DECAY_STEP` — rejected: no value fixes an input with no
  information (wall clock) or a gate wired to its own failure (pulses).
- Max-span quality ceiling — rejected (CODEX correction): wide span with
  edges flowing is excellent contrast; the fault is width the current
  signal cannot cross, which is what active-but-edgeless measures.

## Consequences
Stationary hold and blind recovery coexist. Quality is judged from what
the signal is doing (span, activity, time since edge, latch duration),
not span alone. Guards must never take their input from the failed thing
itself — the same reasoning killed the 20×median latch timeout proposal.

## References
Commits `dcbada0` (superseded), `e1ddd53`, `8c78297`; IR_SENSOR_NOTES
"Reacquisition deadlock".
