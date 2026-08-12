# 0024 — The phantom CONTAINMENT defect is the gate's PWM-predicted expectation

Status: Proposed (2026-08-11)

## Decision

The low-PWM peak-threshold approach in
`docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md` is **abandoned**. That document
is marked superseded and must not be implemented.

**Scope correction (CODEX review, 2026-08-11):** this record concerns
CONTAINMENT, not the source. The extra event is produced by the Hall detector or
by the physical magnetic field; the PWM-derived expectation is why the gate fails
to contain it. Nothing here identifies or addresses the source, and that
question remains open — see Consequences.

The containment failure is attributed to the conservation gate deriving its
expected interval from a PWM velocity model. The replacement direction —
`expectedDt = previousAcceptedDt`, reducing the test to *"reject an event
arriving within 30% of the previous accepted interval"* — is specified in
`docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md`. It is **not** implemented here;
it changes the acceptance path and owes its own record before code is written.

## Context

The 2026-08-11 QUORUM_1_13 beta (135.8 min, 2667 markers) showed the earlier
proposal was scoped wrongly on two counts.

**The scope was wrong.** It targeted `pwm < 40`, where the gate is disabled. Both
recurring phantoms in the beta occurred with the gate fully ACTIVE — at pwm 90
(mm 101, every lap, accelerating out of the Arches dwell) and pwm 72 (mm 63,
Grillers, climbing). Low PWM is only the extreme case, where the model returns a
negative velocity and the gate is switched off entirely.

**The discriminator was wrong.** The proposal set an absolute peak floor from
nine events. When the operator re-glued a dislodged Hall sensor on 2026-08-11 the
detector's peak scale moved ~1.8× (median 80 → 144, p5 57 → 123). At marker 159,
which produced a peak-39 phantom on 10 Aug, the same crossing now reads 133. No
fixed absolute floor survives a glue joint.

The common root is visible in the telemetry: `expectedDt` comes from
`3.90·pwm − 99.2`, which was measured wrong by 1.56–1.78× on grades and on
acceleration out of stops. A long true interval plus a short phantom then sums to
~1.9 expected intervals — outside the reject band — and the phantom is admitted.
The source comment already states the premise: *PWM is a request, not a result.*

Cost of the defect in one session: 16 of 18 adoptions were offset −1 phantom
repairs; on 2026-08-10 a *pair* produced offset −2, which the fence cannot
express, and the whole incident-C cascade followed.

## Alternatives considered

**Peak threshold at low PWM** (the superseded proposal). Rejected: wrong scope,
and the threshold does not survive a sensor repair.

**Relative peak threshold**, scaled to a running median. Rejected as the primary
mechanism: it needs new state and a new constant, and it treats a symptom. Peak
remains useful as corroboration.

**Extend the velocity model with grade or load.** Rejected: more model, more
calibration, and the railway has no independent speed reference. The measured
interval already is the measurement.

**Do nothing.** Rejected: QUORUM currently repairs a phantom roughly once per
lap, and 2026-08-10 showed two in one place exceeds the fence and stops the
railway.

## Consequences

- The proposed direction **removes** the velocity model and `spacingMm[]` from
  the acceptance path rather than adding a compensator — the direction the
  governing rule points. `VEL_MODEL_*` stay for telemetry only.
- It closes the low-PWM hole (10.2% of markers never conservation-tested) without
  any peak threshold, because the rule needs no velocity.
- It dissolves the poisoning trap observed at the 2026-08-11 derailment, where a
  short phantom predecessor made the gate reject 18 consecutive genuine markers
  with no escape short of a power cycle.
- The "44/44 phantoms caught at 0.13% false rejects" figure is **preliminary and
  partly circular** (CODEX): the classifier labels a phantom as `dt < 700 and
  peak < 80`, which is close to the condition under test, and the three purported
  false rejects (peak 40-46) may themselves be phantoms. It is a sanity check,
  not evidence. Binding evidence requires independently labelled events and a
  STATEFUL harness replay.
- **The source of the extra events is not identified.** Containment reduces the
  cost of a phantom; it does not stop one being generated. The discriminating
  experiment is a second locomotive over the same markers: if the mm 101 event
  recurs for Toby, the source is at the track; if it does not, it is Otto's
  detector or its mounting. That test needs no firmware change (QUORUM_1_6
  already publishes `peak`, `ms`, `drift` and `dt` on AGREE/DISAGREE).
- Anything touching acceptance now owes: a decision record, the full replay
  suite, and an explicit enumerated diff from `verify_inert.py` — which for this
  change must show it is deliberately **not** inert.
- Adversarial cases required before implementation (CODEX): maximum genuine
  acceleration combined with route-spacing variation; a missed marker followed by
  acceleration; a correctly rejected phantom followed by the genuine remainder of
  that interval; stops, dwell, reversal and declaration; consecutive phantoms;
  and recovery from a short poisoned predecessor.
- **Approval boundary (operator/CODEX, 2026-08-11):** this record as a proposal
  is acceptable; implementing the timing change is NOT approved. The next step is
  implementation in the host replay harness only, run statefully over both
  complete captures, enumerating every changed acceptance, rejection, adoption
  and terminal outcome, before any flashable firmware is touched.
- The fence and adoption floor stay untouched. All 18 beta adoptions were inside
  the fence and 16 closed successfully, so the fence is not today's constraint.

## References

- `docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md` — the replacement direction
- `docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md` — superseded by this record
- `field-records/20260811_QUORUM_1_13_beta_verdict.md` — session evidence
- `field-records/logs/20260811_QUORUM_1_13_beta_otto.log`
- Decision 0023 — the advisory, unaffected by this record
