# navlab iteration 3 — strict report

Date: 2026-08-22. **Overall: FAIL — 8 PASS, 0 FAIL, 1 NOT_DEMONSTRATED.**
A NOT_DEMONSTRATED is not a pass, and the missing condition (C9) cannot be
demonstrated with existing data at all. Machine verdict:
`tools/navlab/results/iter3_acceptance.json`.

## The data-discipline statement, first

Toby boot2 and Otto boot16 are now **development/regression data** — they
have been inspected event-by-event across three iterations. All replays in
this report are labeled DEV. And after inventory: **no genuinely untouched
committed session exists.** Every committed session either trained the
envelopes (32 sessions), was a development session, or was event-level
inspected during the T/O hold-out study. That is stated, not papered over:
iteration 3 demonstrates mechanisms on development data; **unbiased
validation requires data that does not exist yet.** No train run is
requested.

## Corrected checker (before any navigator change)

1. Ground truth is now three-class: validated / direction-consistent-only /
   unvalidated, and the only permitted claim is "zero false confirmations
   DETECTED". Current totals: 1 validated, 558 unvalidated across the three
   dev replays. Unvalidated confirmations cannot satisfy C7.
2. C5 cannot pass vacuously: it requires a non-empty, independently
   justified phantom population (physical ghost signature: peak < 90 AND
   duration < 80 ms, per the committed amplitude/duration analysis).
3. C8 requires event-level justification of expectations: all exemplar
   windows are now DERIVED from committed records at runtime; the
   hand-pinned epochs are gone.
4. C9 split: exclusion hygiene is reported separately from held-out
   success, and dev-only evaluation forces NOT_DEMONSTRATED with the data
   statement above.

Two further checker-fidelity defects were found by their own output and
fixed with the navigator untouched: monotonicity comparisons spanning a
declaration boundary (flagged a physically correct confirmation after a
legitimately gated route-wide re-acquisition), and the earlier
exemplar-selection and anchoring fixes carried forward.

**Corrected iteration-2 verdict: FAIL — 4 PASS / 2 FAIL / 3 NOT_DEMONSTRATED**
(was reported as "7 of 9, zero false confirmations"; both claims withdrawn in
the iteration-2 report).

## dt=0: rule specified, then implemented, then tested

`docs/NAVLAB_DT0_SEMANTICS.md` defines the protocol meaning (chain reset =
unknown traversal time, never zero travel), the bounded one-interval
corridor grant, the dual stay/advance hypothesis, the fail-safe, and what
the rule must never do. Implementation followed the document; seven
synthetic boundary tests cover every clause (17/17 navigator tests green):
genuine traversal across a reset, same-magnet reread, phantom after reset,
reversal adjacent to a reset, repeated resets (linear, bounded), exact
one-interval grant, and reset events never counting toward confirmation.

## DEV replay results (strict; zero external seeds anywhere)

| | Toby boot2 | Otto boot16 | Otto boot1 (ghost exerciser) |
|---|---|---|---|
| events | 1,506 | 1,455 | 4,726 |
| confirmations | 332 | 219 | 8 |
| reversals (native) | 0 | **14 — the full verified flip count** | 0 |
| contradictions | 0 | **0** | 1 (stop; weak-sensor-era stream) |
| final | NORMAL | **NORMAL, end-to-end** | STOPPED_AT_FIRST_CONTRADICTION |

Otto's cascade day now runs END-TO-END in strict mode: every reversal
excursion tracked natively (including backward marker-by-marker tracking
after a mid-flip dt reset), all four derived frozen-run windows eliminated
in full (5/5, 13/13, 16/16, 14/14), all eight full-circle windows resolved
by gated re-acquisition. The C5 population is real: 51 physical-signature
ghosts exercised in Otto boot1, none advanced position.

## Condition scoreboard

C1–C6, C8: **PASS** (on development data, as labeled).
C7: **PASS** as "no false confirmation detected + stop-honest" — but note
only 1 of 559 confirmations is independently validated; the checker records
this and it is why C7's evidentiary weight is thin.
C9: **NOT_DEMONSTRATED** — see the data statement. This alone makes the
overall verdict FAIL under the rule that a partial is never a pass.

## What iteration 4 needs (not begun)

1. Independent ground truth at scale: the current record supports exactly
   one anchored confirmation check. Options that do not require a train
   run: none identified. Options requiring one (NOT requested now): scripted
   declare checkpoints during a future ordinary session, which would turn
   every declare into an anchor.
2. Genuinely untouched evaluation data: the next captured session, kept out
   of every tool until evaluation.
3. Otto boot1's contradiction (weak-sensor era) is unclassified.

Per the standing rule: T rejected, O archived, Otto on rollback `6d35bb7`.
No firmware implementation is proposed — C9 is not demonstrated, and the
rule forbids proposing it until all nine are, on suitable untouched data.
