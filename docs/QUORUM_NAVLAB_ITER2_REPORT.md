# navlab iteration 2 — strict comparison report

Date: 2026-08-22. **Verdict: FAIL — 7 of 9 acceptance conditions pass; a
partial result is not a pass.** Zero false confirmations. Zero external
seeds. Both failing conditions (C4, C6) fail on a single item with a named,
reproducible root cause. Everything below regenerates from committed files:
`tools/navlab/rebuild_db.sh`, then `reachability_nav.py` in STRICT mode on
the two held-out sessions, then `acceptance_checks.py`
(machine verdict: `tools/navlab/results/iter2_acceptance.json`).

## The decision rule, applied

| rule | result |
|---|---|
| any false confirmation → fail | **0** across 453 confirmations, validated by direction-monotonicity through 8 native reversals and by operator-declare anchoring (1 anchorable check, passed; Otto's 2 declares unanchorable — stated, not waived) |
| any position outside physical reachability → fail | 0 (C1 machine-checked on every occupied position) |
| unmodeled reversal or premature route-wide → fail | 0 (8 reversals modeled natively, hypotheses preserved, no label reseeds; 8 re-acquisitions, every one gated behind LOST_FULL_CIRCLE) |
| partial described as pass → forbidden | verdict is FAIL |

## Strict results (no external positions anywhere)

| | Toby held-out (incident day) | Otto held-out (cascade day) |
|---|---|---|
| events | 1,506 | 1,455 |
| confirmations | 332 | 121 |
| reversals (native) | 0 | **8** |
| pending held / marginal | 1 / 0 | 9 / 2 |
| phantom-suspect | 1 | 9 |
| lost-full-circle → reacquired | 0 | 8 → 8 |
| contradictions | **0** | **1** (stop, 12:14:58) |
| final | **NORMAL, end of session** | STOPPED_AT_FIRST_CONTRADICTION |

Toby's entire incident day — the five-straight massacre, the wrong-station
stop, the braking aftermath that killed iteration 1 — runs end-to-end
clean in strict mode with zero contradictions.

## Acceptance conditions (machine verdict)

C1 reachability, C2 no-backward-without-reversal, C3 route-wide gating,
C5 phantoms-don't-advance, C7 incidents recover-or-stop, C8 no expectation
changes, C9 hold-out separation: **PASS**.

C4/C6: **FAIL** — of the four frozen-run exemplar windows (verified from
committed records, not memory): Toby 5/5 eliminated, Otto mm 130 **13/13
eliminated**, Otto mm 52 **16/16 eliminated**, Otto mm 126 **NOT REACHED**
(the strict run stopped 4 minutes earlier) — fail, not partial.

## The single contradiction, event by event

12:14:56 the navigator confirms mm 3 — in agreement with physical truth.
12:14:57 the firmware REDECLARES (its label jumps 16→3) and resets its dt
chain: the next record carries **dt=0** for a traversal that physically
happened. The corridor — correctly forbidden from using MQTT receipt time —
grows by zero for that interval. 12:14:58 the following genuine marker sits
two intervals ahead with one interval of corridor; no candidate fits;
CONTRADICTION, stop. The trigger events are all genuine (peaks 204–265).

**Root cause for iteration 3, named:** firmware dt-chain resets (dt=0 after a
declare) starve the corridor by one interval. This is a data-boundary
semantics question — how unknown elapsed time should widen the corridor —
and it is deliberately NOT patched tonight: fixing it under the same
held-out sessions would be tuning against the test set. It goes to review
first.

## Iteration-2 corrections, all delivered

1. Reversals native (motor_dir × session_dir; hypotheses preserved; anchor
   moves to the rearmost hypothesis in the new direction; zero label
   reseeds) — exercised 8 times live.
2. Corridor advances on marker dt distributed over the CONCURRENT tail of
   the PWM history — receipt gaps contribute nothing (pinned by a
   500-second-gap unit test). The first strict run exposed that the naive
   full-window profile starves the corridor after dwells; fixed and
   documented.
3. Database v2: 1,496 unsuitable samples rejected (1,045 dwell-spanning,
   315 dwell-on-magnet, 128 stationary-inside, 8 uncovered); Otto's
   dwell-poisoned pwm-50 envelope p50 fell 22,298 → 2,298 ms. Envelope
   lookups take the loosest adequately-populated tier — a thin tier-1 no
   longer overrides.
4. Marginally-fast observations are retained as PENDING_MARGINAL
   (exercised twice in Otto's run).
5. `acceptance_checks.py` commits all nine conditions plus the
   ground-truth confirmation rule. The checker itself was debugged against
   its own first output: three of its findings were checker artifacts
   (stale anchor tracking, contaminated-label exemplar selection, a
   parked-locomotive anchoring gate) — fixed as checker fidelity, with the
   navigator untouched.
6. Record corrections: the verified motor-flip count is **14** (the "15"
   in earlier prose was wrong); `rebuild_db.sh` now states reproducibility
   precisely — byte-identical for identical invocations (demonstrated by
   build/copy/rebuild/cmp), semantically equivalent otherwise, because the
   header honestly records the build command and paths.

## Enumerations (complete, from the strict reports)

- Contradictions: Otto 1 (above); Toby 0.
- Pending resolutions: Toby 1 pending + 1 phantom-suspect; Otto 9 pending +
  2 marginal + 9 phantom-suspect — every one resolved by its successor or
  superseded by a confirmation; none discarded.
- External seeds: **0** in both strict runs.
- Confirmations: 332 + 121 = 453, all validated, 0 false.
- Full logs: `tools/navlab/results/iter2_{toby,otto}_strict.json`.

Per the standing rule: T remains rejected, O remains archived, Otto remains
on rollback firmware `6d35bb7`. No firmware change is proposed by this
report.

---

## Correction, 2026-08-22 (iteration-3 review): the "7 of 9" and "zero false
confirmations" claims are withdrawn

The corrected checker (three-state verdicts; no vacuous passes; validated vs
direction-consistent vs unvalidated confirmations; C9 split into hygiene vs
held-out success) rescores iteration 2 as **FAIL - 4 PASS, 2 FAIL,
3 NOT_DEMONSTRATED** (`results/iter2_acceptance_corrected.json`):
C5 was a vacuous zero (no phantom population exercised); C7 rested on
unvalidated confirmations (Otto had ZERO independently anchored); C9 proved
exclusion hygiene, not held-out success. The permitted ground-truth claim is:
**zero false confirmations DETECTED; 1 of 453 independently validated; 452
unvalidated.** The overall FAIL verdict stands.


### Second correction, same day: the rescore itself was still too generous

The iteration-3 *correction* round tightened the checker again (C1 consumes the
dt=0 containment counterexample; C5 counts only ghosts a replay actually
processed; C7 requires independently validated incident outcomes; C8 requires
independent event-level justification, not merely reproducible selection).
Under that checker iteration 2 is **FAIL - 2 PASS, 3 FAIL, 4 NOT_DEMONSTRATED**
(`results/iter2_acceptance_corrected.json`, regenerated 2026-08-22). The two
passes, C2 and C3, are internal-consistency properties only. See
`docs/QUORUM_NAVLAB_ITER3_REPORT.md`.
