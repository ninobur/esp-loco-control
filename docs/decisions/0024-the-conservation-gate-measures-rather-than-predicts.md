# 0024 — The phantom CONTAINMENT defect is the gate's PWM-predicted expectation

Status: Analysis retained; **proposed change NOT to be implemented** — superseded
in part by decision 0025 (2026-08-12)

> **Outcome, 2026-08-12.** The phantom's *source* was found and corrected
> physically: stacked double magnets installed as a weak-marker remedy. Replacing
> them with single disks took weak events from 12 in 235 markers to 0 in 214 on
> the same firmware image. Under the governing rule the
> `expectedDt = previousAcceptedDt` change proposed here **loses its
> justification and is not to be implemented on phantom grounds**. The analysis
> below of *why containment failed* remains correct and is worth keeping.
> See `docs/decisions/0025-*` and
> `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md`.

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

**Converse test, 2026-08-11 (Otto, 118→050 CCW, constant 91 pwm, no station
armed).** The diagnosis above says containment fails when the PWM model
disagrees with reality. The converse — that containment succeeds when the model
is right — is now measured on the same locomotive, track and direction:

| condition | prev dt | model | model error | weak dt | ratio | outcome |
|---|---|---|---|---|---|---|
| ramped, decelerating into Arches (mm 102→101) | 1862 | 1191 | **+56%** | 335 | 1.84 | **ACCEPTED** — offset −1, 9/9 laps |
| constant 91 pwm (mm 80) | 1136 | 1173 | **−3%** | 195 | 1.13 | **REJECTED** — 0 disagreements |

Same weak-event signature both times (peak 41 and 39 against a session median of
145). The gate rejects on a ratio inside [0.7, 1.3]. At constant speed the model
error is −3% and the pair lands at 1.13, inside the band, contained. Under the
ramp the error is +56% and the pair lands at 1.84, outside, admitted. That is a
controlled confirmation, not an inference from correlation.

**A second, separable finding from the same run: speed affects GENERATION, not
only containment.** At constant 91 pwm, mm 101 was crossed with an entirely
normal read (peak 144, no weak event at all) where the ramped approach produced
one on 9 of 9 crossings. The weak event there was not merely contained — it was
not produced. mm 84, Toby's Event B location, was likewise clean on Otto
(peak 180, width 197). Caveat: one lap, so each location was crossed once.

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
- **The source is now partly identified, and it is NOT Otto's sensor.** The
  discriminating experiment was run on 2026-08-11: Toby, on `QUORUM_1_6`, with a
  different Hall installation, **reproduced the Grillers extra event** — genuine
  strong read at believed mm 63 (peak 170) followed 303 ms later by a weak one
  (peak 41, 42 ms wide) which the gate accepted, advancing the odometer and
  triggering `ZERO_RAMP` a marker early. Evidence: PR #4,
  `field-records/20260811_TOBY_QUORUM_1_6_CROSS_LOCO_FINDINGS.md`. The source is
  therefore associated with the **railway** at that location, not with Otto's
  recently re-glued sensor. Physical inspection at Grillers, and near believed
  mm 84, is now the first action — ahead of any firmware containment.
- **Two distinct failure classes, and this record only addresses one.**
  *Count* errors (an extra event advances the odometer) produce a uniform offset,
  which the fence exists to express and which QUORUM recovers from — Toby's
  Grillers event became offset -1, was adopted and closed. *Identity* errors (the
  wrong member of a close pair is retained) leave the count correct and poison
  the evidence instead: a wrong-polarity reading enters the ring, no offset
  explains it, and the exact-window advisory of decision 0023 is silenced by it.
  Identity errors are the less recoverable of the two.
- Anything touching acceptance now owes: a decision record, the full replay
  suite, and an explicit enumerated diff from `verify_inert.py` — which for this
  change must show it is deliberately **not** inert.
- **The proposed rule does NOT resolve identity, and this is now measured, not
  assumed.** Toby's Event B near believed mm 84: a weak, map-inconsistent read
  arrived first at an ordinary 1166 ms interval and was accepted; the strong,
  map-consistent read arrived 115 ms later and was rejected. Under the proposed
  rule the same inversion holds — `1166 > 0.30*1338` accepts the weak one, then
  `115 <= 0.30*1166` rejects the strong one. The rule is **order-preserving**: it
  always keeps the FIRST of a close pair, so it can fix a count error and can
  never fix an identity error. It is a partial containment, not a phantom
  discriminator, and the record should not be read as claiming otherwise.
  Resolving identity would require comparing the two members of a pair — a
  relative comparison needing no absolute calibration, but also deferral or
  amendment machinery. Not proposed here.
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
