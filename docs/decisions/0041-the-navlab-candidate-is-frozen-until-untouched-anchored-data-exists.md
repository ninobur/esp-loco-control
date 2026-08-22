# 0041 — The navlab candidate is frozen until untouched, anchored data exists

Status: Superseded by 0042  (2026-08-22)

Superseded the same day it was accepted. 0042 keeps the freeze on the
iteration-3 evidence, the corrected verdict and the evaluation protocol, and
lifts the freeze on the navigator as the candidate: it has a demonstrated
dt=0 false-confirmation defect, and known design defects are corrected
off-locomotive before a candidate is frozen and untouched data is spent on it.

## Decision

The navlab reachability navigator is frozen as a candidate at its iteration-3
state. No further model iteration is run for the purpose of improving the
acceptance score. The corrected iteration-3 verdict is **FAIL — 2 PASS, 1 FAIL,
6 NOT_DEMONSTRATED**, and the next work on it is evaluation against data that
does not exist yet, not more development against data that has been used.

The evaluation protocol is fixed now, before that data is captured, so it
cannot be shaped to fit a result: a future ordinary session's capture kept out
of every tool until a single replay; roughly a dozen operator declarations made
with the locomotive near-stationary and no marker events between declaration
and check, so each anchors a confirmation; and at least one slow or low-PWM
stretch so the phantom population is real. No train run is requested to obtain
it — it is whatever the railway next records.

## Context

Three iterations produced a navigator whose bookkeeping is internally sound and
whose evidence about the railway is almost entirely absent. The iteration-3
report initially scored 8 of 9 conditions as passing. Under review, four of
those passes rested on evidence that was internally consistent, heuristic, or
never exercised: 51 ghost events credited to a replay that processed 2; a C7
pass resting on 558 unvalidated confirmations and 0 of 10 independently
supported incident outcomes; a C8 pass that proved reproducible window
selection rather than justified expectations; and a "fail-safe" dt=0 rule that
a development probe refutes by producing confident wrong positions.

Continuing to iterate against the same three development logs can only improve
the score, not the evidence. The scoring rules were tightened instead, and the
candidate frozen.

## Alternatives considered

- **Iterate again to recover the score.** Rejected: every remaining condition
  fails for want of independent evidence, and no navigator change can supply
  it. Tuning against these logs would be fitting to the test set.
- **Fix the dt=0 rule now.** Rejected: the counterexample tells us the
  one-interval grant is unsound, not which replacement is right. Changing it
  now would be tuning to preserve the one replay that motivated it. Two sound
  representations are recorded in `docs/NAVLAB_DT0_SEMANTICS.md` for
  iteration 4.
- **Fix the contaminated fast bounds now.** Rejected for this round for the
  same reason, and recorded: `fast_bound = min × (1 − margin)` lets one
  contaminated sample set the corridor's growth rate, and in Otto's PWM-90
  bucket all twenty fastest samples carry ghost or dwell signatures, giving a
  corridor six times faster than the median speed.
- **Declare the candidate dead.** Rejected: the internal safety properties and
  the reversal handling are real results worth evaluating properly.

## Consequences

- No firmware implementation is proposed. T remains rejected, O remains
  archived, Otto remains on rollback commit `6d35bb7`.
- The acceptance checker now records an evidence `basis` per condition
  (independent / internal-consistency / development-regression / none), counts
  only ghost events a replay actually processed, refuses to let direction
  consistency carry C7, and consumes the dt=0 containment counterexample.
  Earlier verdicts rescored under it drop, including iteration 2 to 2/3/4.
- Two navigator defects are recorded and deliberately left unfixed: no
  amplitude or duration criterion anywhere in the navigator (the Otto boot1
  stop was triggered by a peak-44, 42 ms event), and corridor growth that
  outruns the locomotive by 4–6×, which is what produced eight route-wide
  recoveries in a single session.
- When the untouched capture exists, the protocol above is what it is judged
  by; changing the protocol afterwards invalidates the evaluation.

## References

- `docs/QUORUM_NAVLAB_ITER3_REPORT.md` (corrected record)
- `docs/NAVLAB_DT0_SEMANTICS.md` (rule plus its refutation)
- `docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (the operator's plan)
- `tools/navlab/results/iter3_acceptance_corrected.json`,
  `iter3_evidence_classification.json`, `iter3_probe_dt0_unknown_time.json`
- Decision 0040 (the entry threshold is the only amplitude gate)
