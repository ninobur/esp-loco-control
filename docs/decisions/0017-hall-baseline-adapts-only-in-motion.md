# 0017 — The Hall baseline adapts only while the locomotive is believed moving

Status: Proposed (2026-08-06). Problem accepted by operator; gate mechanism
awaiting Sam/CODEX review of `docs/QUORUM_BASELINE_MOTION_GATE_SPEC.md`.

## Decision

The Hall median baseline stops adapting while the locomotive cannot be
moving (`actualPwm <= MOTOR_DEAD_ZONE_PWM`). A parked locomotive's
reference frame is whatever motion last proved; rest preserves it, motion
maintains it, and only the deliberate boot calibration establishes it from
scratch. Target: QUORUM 1.8, before CTO3 Station Stop v1.

## Context

Twice on 2026-08-06 the operator observed that a powered, stationary
locomotive loses navigation on restart. Source audit found the mechanism:
`updateBaseline()` pushes samples into the 128×500 ms median
unconditionally, so the magnets-are-outliers assumption that makes a
median robust holds only in motion. Parked over a magnet, the reference
migrates to the magnet's level in ~32–64 s and `recomputeThresholds()`
re-centres detection on it. Measured: entry threshold ±38 counts, magnet
excursions to −254/+182. The boot sequence already prints "keep clear of
magnets" — the firmware knew a baseline taken over a magnet is wrong, then
silently redid that calibration continuously wherever the locomotive
stopped. Station Stop v1 institutionalizes parking at fixed positions, so
the flaw would fire by design, four stations a lap.

## Alternatives considered

- **Excursion gate** (refuse magnet-sized samples): self-referential — a
  poisoned baseline refuses its own correction — and blind to fringe-field
  parking. Rejected as primary.
- **Re-prime on departure**: discards a good baseline in the common case,
  trusts an instant over a median, adds a state machine. Rejected.
- **Larger median window**: changes the time constant, not the fact.
  Rejected.
- **Operational rule only** ("don't park on magnets"): invisible
  constraint, defeated by station stops. Rejected as sole remedy; station
  maps may still avoid magnet-adjacent dwell points where free.

## Consequences

- Parking on a magnet becomes reference-safe and produces one legitimate
  arrival-stamped, long-duration event; the header's stuck-open-event
  warning becomes an expected dwell signature.
- No drift correction during dwells (bounded: 19 counts observed across a
  full session; median washes it out within ~30 s of motion).
- Residual risk: a stall above the dead zone parked exactly on a magnet
  still poisons. The clean fix is a real motion witness; the gate's
  "believed moving" condition is the seam where decision 0005's
  `motionWitnessSaysStopped()` hook (IR/Hall evidence) later upgrades
  belief to measurement.
- Restates the house doctrine on the Hall side: 0006 (adaptive references
  gate on signal activity) and 0007 (persist only the proven state) now
  have their Hall-baseline counterpart. 0004 is untouched — it governs
  measurement admission, not reference maintenance.

## References

- `docs/QUORUM_BASELINE_MOTION_GATE_SPEC.md` (proposal under review)
- `docs/QUORUM_STATIONARY_BASELINE_POISONING.md` (problem record, evidence)
- `field-records/logs/20260806_quorum17_otto_run.log`
- Decisions 0004, 0005, 0006, 0007
