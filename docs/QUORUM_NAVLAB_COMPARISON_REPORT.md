# Artifact 5 — current QUORUM vs the reachability navigator, iteration 1

Date: 2026-08-22. Status: **failed experiment by the plan's own rule — and a
well-instrumented one.** Per `QUORUM_REACHABILITY_RECOVERY_PLAN.md`: "If any
condition fails, the result is a failed experiment, not a partially completed
production feature." Scoreboard below: 0 demonstrated / 6 partial / 3 not
demonstrated. This report exists so the operator can exercise the plan's
decision — outcome 2 (revise off-locomotive against the identified failed
conditions) is the recommendation.

Evidence: `tools/navlab/results/artifact5_evidence.json` (all five analysis
agents' full output: 45 classifications, the incident table, the invariant
audit, the acceptance audit). All numbers derive from committed files;
record-line citations refer to `db/records_v1.jsonl`, which is not committed
but regenerates deterministically via `rebuild_db.sh` from committed inputs.

## The dominant finding: one navigator defect explains almost everything

**Reversal blindness.** `reachability_nav.py` takes direction from
`session_dir`, which stays `CW` for the entire Otto session, while the
locomotive's real direction is in `motor_dir` — which flipped to REV **15
times** (the operator's reverse experiments), announced by firmware DIRECTION
events in every window. The navigator tracked forward through three reverse
excursions.

Classification of all 31 Otto contradictions + 2 full-circle windows + Toby's
one: **~27 of 31 NAVIGATOR_MODEL_GAP with reversal blindness as the root**,
2 ENVELOPE_GAP, 0 corrupted evidence, 0 duplicates. Every trigger event was a
genuine marker (peaks 146–216). The contradiction cascades (up to 13 in 23 s)
are one defect repeating: each external reseed onto a reverse-decrementing
firmware label re-contradicts within 1–3 events.

**The worse half of the defect, stated bluntly:** during reverse excursions
the matcher did not just stop — it sometimes **falsely confirmed** (DNA
aliasing under reversal: confirmed 36/49/58/107/126/131 while the locomotive
was elsewhere). At least 4 of strict-Otto's 145 confirmations are provably
false. A contradiction is honest; minutes of confident false tracking before
it are the damning part, and no automated check yet validates confirmations
against ground truth.

## What the concept nonetheless demonstrated, in strict mode, on held-out data

| incident | firmware | navigator (STRICT) |
|---|---|---|
| (a) Toby mm 101 five-straight | five genuine markers rejected, label frozen, wrong-station stop, NO_QUORUM | **accepted all five in real-time lockstep** (102…106 confirmed as each rejection fired); no false relocation through the window |
| (b) Otto 11:21 quarantine discards | two doubtful events deleted, NO_QUORUM mm 35 | both events **held pending**, hypothesis set widened to 8, narrowed back, confirmed — end position exactly matches the operator's later declare (mm 46) |
| (c) Otto 11:50 dwell/−2 slip | label ran 2 ahead; the offset fence cannot express −2; NO_QUORUM mm 37 | matched the 5,641 ms dwell event, held the doubtful follower, **resolved the −2 slip the firmware cannot represent**; final position = the operator's declare 8 min later |
| (f) fifteen-straight mm 130 | fifteen genuine markers rejected, label frozen | all fifteen accepted in lockstep *(continue mode — strict had already stopped at the reversal defect upstream)* |

Invariant audit across both strict runs: **2,002 occupied positions checked,
zero ever behind the anchor.**

## Acceptance conditions (independent skeptic audit)

| condition | status |
|---|---|
| never selects outside the physically reachable set | NOT DEMONSTRATED (no automated corridor-membership check on every step) |
| never relocates behind last confirmation without reversal | PARTIAL (2,002-point audit clean; not an automated committed check) |
| no route-wide search before a full circuit is reachable | NOT DEMONSTRATED (behavioural test exists; no whole-log audit) |
| genuine acceleration stays genuine | PARTIAL |
| known phantoms do not advance position | PARTIAL |
| frozen genuine-rejection runs disappear | PARTIAL (a: strict; f: continue only) |
| known incidents recover or stop without false relocation | PARTIAL (a, b, c yes in strict; d/e blocked by reversal blindness) |
| every changed regression expectation event-justified | NOT DEMONSTRATED |
| results hold on excluded sessions | PARTIAL (all of the above IS held-out data; contaminated by the false confirmations) |

## Revision list for iteration 2 (plan outcome 2), in order

1. **Model reversal natively**: consume `motor_dir` and firmware DIRECTION
   events; on reversal the corridor flips and positions behind the old anchor
   become legal — exactly the "without a reversal" clause of the plan.
2. **Automated ground-truth validation of confirmations** (operator declares +
   landmark agreements) — false confirmations must be counted by a machine,
   not found by an auditor.
3. **Automated whole-log invariant checkers** for corridor membership and
   route-wide gating (turns three NOT DEMONSTRATED into checkable).
4. Close the two ENVELOPE_GAP cases (tier fallback when a bucket is thin).
5. Re-run both held-out sessions strict; classify every remaining
   contradiction; only then rescore the nine conditions.

## What this is not

Not a firmware proposal. T remains rejected; O remains archived; the rollback
firmware remains what Otto runs. Per the plan, only the operator's decision
follows a completed comparison — and this comparison's honest completion is:
iteration 1 failed its acceptance gate, with one precisely-located, fixable
cause and four incident-level demonstrations that the underlying model does
what QUORUM cannot.
