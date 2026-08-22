# Frozen future-validation protocol

Status: Frozen by decision 0042 on 2026-08-22, before the data exists.
Changing it after a capture has been taken invalidates the evaluation.

This protocol governs what may be called **validation**. It is separate from
`docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`, which governs host-model
acceptance on generated cases. Acceptance comes first; validation is spent on a
candidate that has already passed acceptance.

## 1. Development data is permanently labelled

Every existing Toby and Otto session — every capture used in normalization,
envelope building, replay, probing or inspection up to and including 75fa0ee —
is **development data**. This is not reversible by later disuse. Development
data can support:

- regression (the navigator runs, and its behaviour is explicable);
- realism (generated cases resemble what the railway produces);
- counterexamples (a defect shown on development data is still a defect).

It can never support a PASS on any acceptance condition, and no report may
describe a development-data result as validation. The four sessions listed as
`sessions_held_out` in `tools/navlab/db/timing_db_v1.json` are held out of
envelope generation only; they have been replayed and inspected, so they are
development data too.

## 2. What a validation capture must be

**Untouched.** Kept out of normalization, envelope building, replay, probing
and inspection until the evaluation run. Its hash is recorded and committed
before anyone looks at it. It is then replayed **exactly once**. A second
replay makes it development data.

**Anchored.** Roughly a dozen operator declarations scattered through the
session, each made with the locomotive near-stationary and with no marker
events between the declaration and the check, so that each one independently
anchors a confirmation. Without these, a session produces direction-consistent
and position-unvalidated confirmations, which is what 559 of the iteration-3
confirmations were.

**Ghost-bearing.** At least one slow or low-PWM stretch of the sort that
produces phantom events, so the phantom population is real rather than
anecdotal. The iteration-3 record credited 51 ghosts to a replay that processed
2.

**Reset-bearing.** At least one internal dt-chain reset with a known elapsed
time — obtainable from an independent clock or from the operator's own record —
so Case I of the specification is measured rather than probed synthetically.
This requirement is new; it exists because the one demonstrated defect in the
frozen navigator lives exactly there.

No train run is requested to obtain any of this. It is whatever the railway
next records, under whatever conditions the operator chooses to run it.

## 3. Pre-registration

Before the capture is replayed:

1. The candidate is frozen at a named commit.
2. Its acceptance-test results (the T-families and the G invariants) are
   committed.
3. The conditions to be scored, and the evidence `basis` each is allowed to
   claim, are committed.
4. The capture's hash is committed.

The checker is `tools/navlab/acceptance_checks.py` with its evidence-`basis`
machinery, retained from 75fa0ee.

## 4. Scoring rules, carried over unchanged

- A condition scores PASS only on **independent** evidence. Internal
  consistency, development regression and heuristic labels each score
  NOT_DEMONSTRATED, and are reported with the basis named.
- Direction consistency never carries a position claim.
- Firmware labels and firmware verdicts are not cross-checks; the firmware is
  the subject of the study.
- Ghost events are counted only if the replay actually processed them.
- The permitted wording is **"zero false confirmations detected"**, never
  "zero false confirmations".
- A failure on any condition makes the whole evaluation a failed experiment,
  not a partially completed feature.

## 5. What a validation run cannot do

It cannot be used to tune the candidate. If the run fails, the failure is
recorded, the candidate is revised off-locomotive against the specifically
identified failed condition, and a **new** untouched capture is required for
the next evaluation. The failed capture becomes development data.

## 6. References

- Decision 0042 (this protocol is frozen there; supersedes 0041)
- `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`
- `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`
- `docs/QUORUM_NAVLAB_ITER3_REPORT.md` §6 (the protocol this extends)
- `docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (definition of done)
