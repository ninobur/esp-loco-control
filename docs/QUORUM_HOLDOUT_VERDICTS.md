# Hold-out verdicts: T and O, ruled separately — 2026-08-21/22

Data: 23 untouched sessions from two captures (2026-08-11 beta log and the
2026-08-13→20 watch log), ~40,000 markers replayed per variant, none used in
development. Raw results committed:
`tests/results_20260821_holdout_matrix.json`,
`tests/results_20260821_holdout_adjudication.json`. All further tuning against
ANY of these captures — including the 08-20/21 development five — is
prohibited: they are development data now.

## O (symmetric offsets {−3…+4}) — CANDIDATE for focused production review

- Five −2/−3 adoptions on data it never saw: (167,−3), (46,−2)+(44,−3),
  (44,−3)+(46,−2), (91,−2), (167,−2). Cut hold-out NO_QUORUMs 10 → 6.
  Converted the 08-11 beta log's terminal `SECOND_ADOPTION_FAILED` cascade
  into a (23,−3) adoption. Introduced zero new NO_QUORUMs.
- Its two "new" rejections are dt=0 duplicates at ratio exactly 1.00 —
  correct catches, not regressions.
- Costs: a handful of extra DISAGREEs exploring the wider fence; two
  NO_QUORUMs relocated rather than removed; and the mandatory companion
  buffer fix (`sc/ex` sized from QUORUM_CANDIDATES) without which every nav
  JSON truncates.
- Known limit, not fence-related: with the evidence available, QUORUM_MAX=12
  and MARGIN=2 can still terminate with the true offset viable.

**Status: worth a focused production review as a bounded, self-contained
change (fence + buffer). NOT approved; and under the reachability plan's
exclusions it is not promoted as "the solution" — it repairs the fence's
expressiveness, nothing upstream.**

## T (measured-expectation conservation gate) — REJECTED in current form

The hold-outs flatter it: rejections 78→5, longest run 18→1, no lock ever
formed, and on the beta log the one event where BASE and T split, the real
fielded firmware sided with T. But:

- It took FOUR design iterations against the same five development sessions
  (literal proposal → median → sanity cap → rhythm escape). Each fix was
  falsified by the next replay. That is threshold-tuning by another name.
- The 0.30–0.50×prev wedge is unresolved and now has a clean synthetic
  witness: `syn_adv_missed_then_accel` encodes PHYSICAL TRUTH (one marker
  passes unseen, then genuine acceleration) and T fails it three ways where
  BASE passes.
- The skeptic audit refuted 6 of the first 10 "fixture merely encodes the old
  gate" classifications — T's suite damage is larger than the adjudication
  first claimed.
- The rhythm-escape cap is empirical, not structural (>30% cadence drift
  defeats it).

**Status: rejected in current form; returned to design. Per the reachability
plan, the correct successor is not a fifth threshold variant but the timing
database + reachability corridor, in which a PWM-specific dt envelope makes
this entire single-predecessor heuristic unnecessary.**

## Continuity note

The T experiment was not wasted: it proved the PWM-model gate is the dominant
mechanical cause of frozen rejection runs (out-of-sample: 18-long on
otto_s09, three 4-runs on toby_s11, all genuine markers), quantified the
recoverable margin, and surfaced two quarantine defects recorded separately
in `docs/QUORUM_QUARANTINE_DEFECTS.md`. Its guarded code stays in the tree as
the harness apparatus that produced these numbers, disabled in production.
