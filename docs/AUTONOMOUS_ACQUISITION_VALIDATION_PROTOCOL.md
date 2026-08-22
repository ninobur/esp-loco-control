# Frozen future-validation protocol

Status: Frozen by decision 0042 on 2026-08-22. Corrected the same day in the
specification correction pass; the correction concerns replay discipline (§3)
and the reconciliation of "untouched" with the required evidence (§2.2), and
changes nothing about what counts as development data. Changing this protocol
after a capture has been taken invalidates the evaluation.

This protocol governs what may be called **validation**. It is separate from
`docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`, which governs host-model
acceptance on generated cases. Acceptance comes first; validation is spent on a
candidate that has already passed acceptance, including its usefulness gates.

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

### 2.1 The four conditions

**Untouched.** Kept out of normalization, envelope building, replay, probing
and inspection until the scored replay. Its hash is recorded and committed
before anyone looks at it.

**Anchored.** Roughly a dozen operator declarations scattered through the
session, each made with the locomotive **stationary** under §7.5 of the
specification — the only condition under which an operator declaration is
authoritative, since a moving locomotive's exact MM cannot be confirmed from a
delayed console, with no marker events between the declaration and the check, so
each independently anchors a confirmation. Without these, a session produces
direction-consistent and position-unvalidated confirmations — what 559 of the
iteration-3 confirmations were.

**Ghost-bearing.** At least one slow or low-PWM stretch of the sort that
produces phantom events, so the phantom population is real rather than
anecdotal. The iteration-3 record credited 51 ghosts to a replay that processed
2.

**Launch-representative.** At least one startup from the normal launch region
(MM036–MM045) under mode 2, so launch-region acquisition — the design's first
operational target — is measured on real track rather than only on generated
cases. A session that never uses mode 2 scores the launch-region usefulness
conditions NOT_DEMONSTRATED.

**Discontinuity-bearing.** At least one genuine elapsed-time discontinuity
(§3.7 Case I) with an independently known elapsed time, and at least one
internal redeclaration (Case R), so both are measured rather than probed
synthetically. Case R is included specifically because the corrected timing
rule claims it is *not* a timing event; that claim needs a real observation.

### 2.2 Reconciling "untouched" with those conditions

These are not in tension, and the distinction is about **when** and **on what
basis** the conditions are arranged:

- **Permitted:** the operator deliberately arranges the conditions during
  collection — making stationary declarations, running a slow stretch,
  power-cycling once — and records what was done and when, as a collection
  note committed with the hash. Arranging the session is part of collecting it.
- **Permitted:** the operator confirms from their own collection notes, before
  any replay, that the required conditions are present, and if they are not,
  collects another session.
- **Prohibited:** selecting a capture, a window inside a capture, or a subset
  of declarations **after** seeing how the navigator performed on it. That is
  choosing the test after seeing the answer, and it is what the untouched rule
  exists to prevent.
- **If a capture lacks a required condition**, the conditions it cannot support
  are scored **NOT_DEMONSTRATED**. The capture is **not** searched for a
  favourable episode to substitute, and the missing condition is not
  reclassified as satisfied by something else.

No train run is requested to obtain any of this. It is whatever the railway
next records, under whatever conditions the operator chooses to run it.

## 3. Replay discipline

The previous "replayed exactly once" rule was wrong: it made reproducibility —
which is a requirement of the recovery plan's definition of done — into a
violation. Corrected:

**Before the scored replay**, all four are frozen and committed: the candidate
at a named commit, the checker, the scoring criteria, and the capture hash. No
one inspects the capture contents for the purpose of tuning the design.

**The first scored replay is the validation result.** It is the result,
whatever it says.

**Deterministic reruns are permitted and required.** Re-running the identical
candidate, checker and input must reproduce the identical result, and doing so
is part of the deliverable — another reviewer must be able to reproduce it from
committed files. A rerun does not invalidate the original result and does not
make the capture development data.

**What does consume the capture** is using it to redesign. After the scored
result, the capture may not be used to redesign the candidate and then validate
the redesign. If a redesign occurs, that capture becomes development data and a
**new** untouched capture is required for the next scored evaluation. This is
the rule the old "exactly once" was reaching for.

**Boundary cases, stated so they are not argued later:**

- Fixing a crash in the *harness* (not the navigator) and re-running is a
  rerun, not a redesign — provided the navigator commit is unchanged.
- Changing any navigator parameter, including an engineering parameter from
  §10 of the specification, is a redesign.
- Recomputing envelopes from different source sessions is a redesign.
- Reading the scored report, the classifications and the diagnostics is not a
  redesign; acting on them by changing the candidate is.

## 4. Pre-registration

Before the capture is replayed:

1. The candidate is frozen at a named commit, with its acceptance-test results
   committed — **both** the safety gates and the usefulness gates.
2. The engineering parameters of specification §10 are frozen with their
   calibration evidence. A parameter with no supporting evidence blocks the
   freeze.
3. The conditions to be scored, and the evidence `basis` each may claim, are
   committed.
4. The capture's hash and the operator's collection note are committed.

The checker is `tools/navlab/acceptance_checks.py` with its evidence-`basis`
machinery, retained from 75fa0ee.

## 5. Scoring rules, carried over unchanged

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
- **Stopping is not a pass.** A validation run in which the navigator stopped
  rather than acquiring or recovering scores the usefulness conditions as
  failures, not as safe outcomes. Safety and usefulness are scored separately,
  exactly as in acceptance.
- **Unscheduled navigation stops are counted and reported individually**, each
  with its §7.6 snapshot and its stop classification. An unscheduled stop is a
  safe outcome and an operational failure requiring diagnosis; a run containing
  them may still pass the safety conditions and must not be described as a
  successful operational result.

## 6. References

- Decision 0042 (this protocol is frozen there; supersedes 0041)
- `docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md` §7.5, §8, §10
- `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md`
- `docs/QUORUM_NAVLAB_ITER3_REPORT.md` §6 (the protocol this extends)
- `docs/QUORUM_REACHABILITY_RECOVERY_PLAN.md` (definition of done)
