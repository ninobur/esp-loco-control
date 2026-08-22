# 0042 — Iteration 3 is frozen as evidence; the navigator is not frozen as the candidate

Status: Accepted  (2026-08-22)

Supersedes 0041. This is the revision of 0041 the operator directed on
2026-08-22. It keeps everything 0041 froze about *evidence* and changes what
0041 froze about the *artefact*.

## Decision

Three things are frozen and are not reopened:

1. **The iteration-3 evidence.** The classifications committed at 75fa0ee —
   the eight route-wide recoveries, the Otto boot1 contradiction, the 48
   frozen-run events, the dt=0 containment probe — stand as the record of what
   was and was not shown. They are not re-derived to obtain a better number.
2. **The corrected verdict: FAIL — 2 PASS, 1 FAIL, 6 NOT_DEMONSTRATED.** It is
   not rescored. C2 and C3 pass as internal safety properties only.
3. **The future evaluation protocol**, fixed before the data exists so it
   cannot be shaped to a result: untouched, anchored, ghost-bearing, replayed
   exactly once. It is restated in full and extended in
   `docs/AUTONOMOUS_ACQUISITION_VALIDATION_PROTOCOL.md`.

One thing is **not** frozen, reversing 0041:

4. **The iteration-3 navigator is not the candidate for that future
   validation.** 0041 froze it as the artefact to be judged. That was wrong.
   The navigator carries a *demonstrated* design defect — the dt=0
   one-interval grant, which a probe on the committed map refutes by producing
   confident wrong positions at 7–25 of 171 start positions — plus two
   recorded and unfixed defects: no amplitude or duration criterion anywhere,
   and a corridor that grows 4–6× faster than the locomotive moves. Freezing a
   navigator with a known false-confirmation pathway means the untouched
   capture, when it arrives, would be spent measuring an artefact we already
   know is unsound, and the capture is single-use by construction.

**Known design defects are corrected off-locomotive first. Only then is a
candidate frozen and untouched validation data collected against it.** The
correction is a design replacement, not another patch: it is specified in
`docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`, and no untouched capture is
consumed until an implementation of that specification passes the host-model
acceptance tests written before it.

## Context

0041 was written to stop score-chasing, and that part of it was right: three
iterations against three development logs improved the number and not the
evidence. But it drew the freeze line in the wrong place. "Freeze the
candidate" and "freeze the evidence and the protocol" are separable, and only
the second is what stops fitting to the test set.

The distinction that makes the difference is between defects *discovered by
tuning against a replay* and defects *demonstrated by counterexample
independent of any replay*. Correcting the first kind is fitting. The dt=0
grant is the second kind: `probe_dt0_unknown_time.py` refutes it on synthetic
sweeps over the committed map, and the refutation would hold if every Otto and
Toby log were deleted. Correcting it is not tuning. 0041's own reasoning —
"the counterexample tells us the one-interval grant is unsound, not which
replacement is right" — is an argument for specifying the replacement
carefully, not for shipping the unsound rule into the one evaluation the
programme gets.

The same applies to the other two recorded defects. That the navigator has no
amplitude criterion at all is a structural fact about its code, not a replay
result. That `fast_bound = min × (1 − margin)` lets one contaminated sample set
the corridor's growth rate is a property of the formula.

## Alternatives considered

- **Leave 0041 standing and validate the iteration-3 navigator as-is.**
  Rejected. The capture is single-use. Measuring a navigator with a known
  false-confirmation pathway answers a question we already know the answer to,
  and burns the data doing it.
- **Patch the three defects inside the iteration-3 navigator and freeze that.**
  Rejected. Fixed-offset QUORUM recovery and the monotonic corridor are the
  things being replaced; three more patches on top of them repeats the
  programme's characteristic failure. The recovery plan is explicit that
  widening offsets cannot repair unsound upstream classification.
- **Unfreeze everything and iterate again.** Rejected for 0041's original
  reason, which stands: iterating against used logs improves the score and not
  the evidence.
- **Collect the untouched capture now, before any design work.** Rejected. A
  capture is only untouched once. It is collected when there is an artefact
  worth spending it on.

## Consequences

- No firmware implementation is authorised by this record. T remains rejected,
  O remains archived, Otto remains on rollback commit `6d35bb7`. No train run
  is requested.
- The iteration-3 navigator becomes **reference material**: its internal safety
  properties (forward-only motion, gated route-wide search, held pending
  events, native reversal handling) are inputs to the new specification, and
  its confirmations are not evidence about the railway.
- Development data is permanently labelled. Every existing Toby and Otto
  session — every session used in normalization, envelope building, replay or
  inspection — is development data and can never be called validation, for the
  iteration-3 navigator or for its replacement.
- The acceptance checker's evidence-`basis` machinery, added at 75fa0ee, is
  retained and is the checker for the replacement as well.
- The next artefact to be frozen is an implementation of
  `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md` that passes
  `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md` on generated cases with
  known ground truth. Until then there is no candidate.

## References

- Decision 0041 (superseded) — the freeze this record revises
- `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md` — the replacement design
- `docs/AUTONOMOUS_ACQUISITION_IMPLEMENTATION_MAP.md`
- `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`
- `docs/AUTONOMOUS_ACQUISITION_VALIDATION_PROTOCOL.md`
- `docs/QUORUM_NAVLAB_ITER3_REPORT.md` (corrected record, frozen)
- `docs/NAVLAB_DT0_SEMANTICS.md` (the rule and its refutation)
- `docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (the operator's plan)
- commit 75fa0ee
