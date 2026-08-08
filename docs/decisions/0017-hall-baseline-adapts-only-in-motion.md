# 0017 — The Hall baseline adapts only while the locomotive is believed moving

Status: **Accepted (2026-08-07).** Reviewed by Sam and CODEX 2026-08-07,
both approving conditional on the pre-fix reproduction
(`docs/QUORUM_1_8_REVIEW_FINDINGS.md`); wording below amended per that
review. **The reproduction PASSED in the field the same day** — baseline
2019 -> 1892 during a 29 s stationary dwell on a magnet, a 129-count
migration against a +/-38 threshold, with the predicted long open event
and departure phantom. Evidence:
`field-records/logs/20260807_A1_prefix-magnet-park.log`, verdict in
`docs/QUORUM_1_8_STAGE_A_VERDICT.md`. Implemented in QUORUM 1.8.

## Decision

The Hall median baseline adapts **only while the locomotive is believed to
have tractive motion** (`actualPwm > MOTOR_DEAD_ZONE_PWM`); below that
there is no positive evidence of powered motion, and adaptation stops.
(Wording per Sam/CODEX review — the gate does not prove the locomotive
*cannot* be moving: coasting, hand-pushing, and stalls above the dead zone
are acknowledged, and the asymmetry of the two error cases is why freeze
is the safe side.) A parked locomotive's reference frame is whatever
motion last proved; rest preserves it, motion maintains it, and only the
deliberate boot calibration establishes it from scratch. Target: QUORUM
1.8, before CTO3 Station Stop v1.

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
- No drift correction during dwells. The observed baseline-variation bound
  is 19 counts across a full session — almost entirely position
  dependence, not measured thermal drift (CODEX wording) — and the median
  washes it out within ~30 s of motion.
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

- `docs/QUORUM_BASELINE_MOTION_GATE_SPEC.md` (implemented and field-validated)
- `docs/QUORUM_STATIONARY_BASELINE_POISONING.md` (problem record, evidence)
- `field-records/logs/20260806_quorum17_otto_run.log`
- Decisions 0004, 0005, 0006, 0007
