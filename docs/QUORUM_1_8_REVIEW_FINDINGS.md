# QUORUM 1.8 baseline motion gate — Sam and CODEX review findings

Date received: 2026-08-07 (reviews of spec/problem record/decision 0017 as
of commit `2dc4223`)
Verdict, both reviewers: **diagnosis confirmed in source; one-line motion
gate approved as proportionate; ratification and implementation conditional
on the pre-fix reproduction test.** Neither found a flaw in the mechanism;
all requested changes are wording, documentation, and test-coverage.

Final disposition, 2026-08-08: **condition satisfied; PASS.** Decision 0017
is accepted without qualification for the baseline-motion-gate change.
Dedicated Stage D regression and Toby validation remain outstanding
operational work; neither reopens the accepted design decision. See
Appendix C.

Milestone noted by Sam: the connector now fetches repository files
directly — reviews no longer depend on pasted copies.

---

## Disposition of requested changes

| # | Reviewer | Request | Disposition |
|---|---|---|---|
| 1 | Sam + CODEX 1 | Decision 0017's first sentence ("cannot be moving") overstates what PWM ≤ 20 proves — coasting, hand-pushing, stalls | **Applied.** Decision now reads "believed to have tractive motion (`actualPwm > MOTOR_DEAD_ZONE_PWM`)", matching the spec's corrected R1 language |
| 2 | CODEX 2 | Document the priming invariant: `primeMedian(raw)` fallback precedes the gate and could establish a stationary baseline | **Applied (document, not change).** Verified in source: `calibrate()` primes before `hallTask` is created, so `medPrimed==false` is unreachable once samples flow. The fallback stays as last-resort defense — if the invariant is ever broken by a future edit, a single-sample prime still beats a zero baseline (which would pin thresholds ±38 around 0 and open a permanent event). 1.8 must carry a comment stating both the invariant and this reasoning. CODEX's caution against moving the gate above the prime is agreed and recorded |
| 3 | CODEX 3 | Long open event (dwell > 65.535 s) is a compatibility *requirement*, not an assumption — test the u16 saturation path | **Applied.** Added to the spec's acceptance tests: one event only, `ms == 65535`, `drift` ≈ 0, polarity = arrival pole, navigation advances exactly once, successor marker not phantom-rejected, stale arrival timestamp benign |
| 4 | CODEX 4 | Post-fix acceptance matrix: five stop conditions including fringe-field and deliberate-stall rows | **Applied** verbatim as the spec's §5 acceptance matrix. The fringe row is the one that demonstrates superiority over the excursion gate; the stall row documents the accepted residual rather than hiding it |
| 5 | CODEX (wording) | The 19-count session range is an observed baseline-variation bound, not evidence of thermal drift | **Applied** in spec §3.2 and decision 0017. The self-review had already decomposed the figure as position dependence; the label now matches |

## Points of agreement worth preserving

- Both reviewers independently endorsed the **random-error vs.
  systematic-reference-corruption** distinction as the conceptual core: the
  navigator's tolerance for noisy readings is not tolerance for a
  displaced reference frame.
- Both endorsed rejecting the excursion gate as self-referential, and Sam
  specifically endorsed rejecting `!evActive` for the reason found in
  self-review R6 (it would disable the baseline-migration path that
  eventually closes a stuck-open event while moving).
- Both endorsed the falsifiability discipline: **if the parked-on-magnet
  baseline holds steady, the diagnosis is wrong and 1.8 is not built.**
- CODEX confirmed the residual stall-over-magnet case is correctly left to
  a future genuine motion witness (decision 0005 seam) rather than
  patched now.

## Gate to implementation

1. Operator runs the pre-fix reproduction (spec §5) — capture continuous
   from before the stop to ≥ 60 s after departure.
2. If baseline migration is reproduced: implement QUORUM 1.8 per the spec,
   including the priming-invariant comment; run the five-row acceptance
   matrix and the long-dwell saturation checks; promote 0017 to Accepted.
3. If not reproduced: 0017 stays Proposed, diagnosis reopens, no firmware.

## Source documents

- `docs/QUORUM_BASELINE_MOTION_GATE_SPEC.md` (amended per this disposition)
- `docs/QUORUM_STATIONARY_BASELINE_POISONING.md`
- `docs/decisions/0017-hall-baseline-adapts-only-in-motion.md` (amended)

---

## Appendix A — Sam's review (verbatim)

> I got it. The connector is working now, and I reviewed the three
> documents plus the relevant QUORUM source.
>
> My conclusion: the diagnosis is strong, the proposed fix is appropriately
> narrow, and Decision 0017 is sound as a proposed design decision. I would
> approve moving to the decisive pre-fix field test exactly as specified
> before firmware is changed.
>
> The source confirms the central mechanism. `updateBaseline()` pushes one
> raw Hall sample into the 128-entry median every 500 ms with no motion
> gate at all, and every median change immediately recenters the Hall
> thresholds. That is exactly the behavior the documents describe. The
> source also confirms `actualPwm` is already a volatile shared state read
> across the loop/Hall-task boundary, so using it for the proposed guard
> does not introduce a new threading pattern.
>
> The problem record makes a persuasive distinction between random detector
> error, which QUORUM was designed to tolerate, and systematic reference
> corruption, which it cannot tolerate. That is the most important
> conceptual point in the whole write-up. The existing philosophy—let the
> navigator absorb imperfect individual readings—remains valid. But once
> the baseline itself migrates onto a stationary magnet, subsequent
> measurements become coherently wrong rather than occasionally noisy. The
> record quantifies why this matters: ±38-count entry thresholds against
> observed magnet excursions as large as −254/+182 counts.
>
> I also think the self-review materially improved the proposal. In
> particular, correcting the claim that PWM ≤20 means "wheels are stopped"
> was important. The revised claim is much more defensible: below the
> tractive floor there is no positive evidence of motion, and the two
> possible mistakes are asymmetric. Refusing baseline adaptation while a
> locomotive happens to be coasting merely delays adaptation; adapting
> while a locomotive is actually parked over a magnet can destroy the
> reference frame. That is a good engineering reason for choosing the
> conservative side of that ambiguity.
>
> The proposed change itself is excellent in scope:
>
> ```cpp
> if(actualPwm <= MOTOR_DEAD_ZONE_PWM) return;
> ```
>
> One comparison, no new state machine, no new detector qualification, no
> navigator changes, no communications changes. Most importantly, it fixes
> the assumption that the median algorithm was already relying on
> implicitly: magnets are outliers only while the sensor is traversing the
> railway. At rest, repeated samples from the same physical point are not
> independent environmental samples.
>
> I agree with rejecting the alternatives. The excursion gate is attractive
> at first glance but genuinely self-referential; once the baseline is
> wrong, "too far from baseline" stops meaning what you need it to mean. A
> larger median merely stretches the failure time. Re-priming on departure
> discards known-good history and creates more machinery. And an
> operational prohibition against stopping near magnets would be
> particularly bad now because CTO3 station stopping deliberately creates
> repeated dwells at fixed physical locations.
>
> There is one thing I would change before promoting Decision 0017 from
> Proposed to accepted. Its first sentence says:
>
> "stops adapting while the locomotive cannot be moving"
>
> That is slightly stronger than the carefully corrected language in the
> full spec. PWM ≤ dead-zone does not prove it cannot be moving, because
> coasting is explicitly acknowledged. I would make the decision record
> say:
>
> The Hall median baseline stops adapting while the locomotive lacks
> positive evidence of powered motion (`actualPwm <= MOTOR_DEAD_ZONE_PWM`).
>
> or simply:
>
> The Hall median baseline adapts only while the locomotive is believed
> moving.
>
> That matches the title and avoids reintroducing the very factual
> overstatement that R1 corrected.
>
> The residual stall case is also handled correctly. PWM >20 while
> physically stalled over a magnet can still poison the median. I would not
> complicate this fix to solve that today. The spec explicitly identifies
> the proper future seam: a genuine motion witness. That is much cleaner
> than trying to infer every possible stall case from Hall behavior now.
>
> And I strongly agree with rejecting `!evActive` as an additional gate.
> That would look like an easy way to cover the stall-over-magnet case, but
> the self-review caught the important unintended consequence: an event
> that legitimately gets stuck open while moving would lose the baseline
> migration mechanism that can eventually close it. That is exactly the
> sort of "small extra safeguard" that creates a second failure elsewhere.
>
> The pre-fix experiment is especially well designed because it is
> falsifiable. Park directly over a magnet, record continuously through the
> dwell and departure, and require the predicted baseline migration. Then
> repeat clear of magnets as the control. The line I particularly approve
> of is essentially: if the baseline does not migrate while parked on the
> magnet, the diagnosis is wrong and 1.8 is not built. That is the right
> discipline.
>
> So my review verdict is:
> Problem mechanism: confirmed in source.
> Connection to the specific August 6 NO_QUORUM incident: strong
> hypothesis, not yet proven, correctly labeled that way.
> Decision 0017 architecture: approve, with the wording correction above.
> Proposed one-line implementation: technically appropriate pending the
> pre-fix test.
> Development sequencing: yes, resolve this before CTO3 Station Stop v1.
>
> And one other important result from tonight: I can now fetch your
> repository files directly. The clerical problem we were trying to
> eliminate is effectively solved.

## Appendix B — CODEX review (verbatim)

> My verdict: the diagnosis is technically sound, and the motion gate is
> the right minimal fix. I would approve Decision 0017 after the proposed
> pre-fix reproduction confirms the mechanism.
>
> The key causal chain is valid:
> - The 128-sample median assumes magnet occupancy remains below half the
>   window.
> - That assumption holds while traversing magnets, but fails indefinitely
>   when parked.
> - Once the median crosses over, all thresholds move with it.
> - QUORUM tolerates isolated incorrect observations, but not a
>   systematically displaced reference frame.
>
> The proposed guard is appropriately narrow:
>
> ```cpp
> if (actualPwm <= MOTOR_DEAD_ZONE_PWM) return;
> ```
>
> It preserves the existing detector and navigator contracts, introduces
> no new state machine, and provides a clean seam for a future physical
> motion witness.
>
> I recommend four changes before ratification:
>
> 1. Tighten the decision wording
> "Cannot be moving" is stronger than the implementation proves. The
> locomotive can coast, be hand-pushed, or stall above PWM 20.
> Use:
> > The Hall baseline adapts only while the locomotive is believed to have
> > tractive motion (`actualPwm > MOTOR_DEAD_ZONE_PWM`).
> That accurately describes both false-negative and residual-risk cases.
>
> 2. Document the priming invariant
> `updateBaseline()` currently calls `primeMedian(raw)` before the
> proposed gate. Normal startup is safe because `calibrate()` primes the
> median before the Hall task starts. Nevertheless, the code technically
> retains a path that can establish a stationary baseline.
> For QUORUM 1.8, I would either:
> - explicitly document that `medPrimed == false` is unreachable after the
>   Hall task starts; or
> - replace the fallback with a defensive return/fault, leaving
>   `calibrate()` as the only authority permitted to establish a baseline.
> I would not simply move the guard above `primeMedian()`: that could
> prime from a single sample immediately after motion begins, potentially
> while crossing a magnet.
>
> 3. Treat the long open event as a tested compatibility requirement
> The proposed behavior appears compatible with the current navigator.
> Still, explicitly test a dwell longer than 65.535 seconds. Confirm:
> - exactly one event is emitted;
> - `ms == 65535`;
> - `drift` remains near zero;
> - polarity matches the arrival pole;
> - navigation advances exactly once;
> - the next marker is not rejected as a phantom;
> - stale arrival time does not cause unexpected age or station-state
>   behavior.
>
> 4. Strengthen the field matrix
> The decisive pre-fix test is good. I would make the post-fix acceptance
> matrix:
>
> | Stop condition | Duration | Expected result |
> |---|---:|---|
> | Clear of magnets | >70 s | Baseline stable; clean restart |
> | Fringe field, offset below ±38 | >70 s | Baseline stable |
> | North magnet | >70 s | Baseline frozen; one N event |
> | South magnet | >70 s | Baseline frozen; one S event |
> | Magnet, PWM deliberately above 20 but physically stalled | >70 s | Known residual poisoning reproduced/documented |
>
> The fringe-field case is especially valuable because it demonstrates why
> the motion gate is superior to an excursion gate.
>
> One wording correction: the observed 19-count session-wide baseline
> range does not by itself prove thermal drift during a stationary dwell.
> Describe it as the observed baseline-variation bound unless the log
> isolates temperature/time from position.
>
> Subject to the reproduction test, I recommend:
> > Accept Decision 0017 and implement QUORUM 1.8, with the priming
> > invariant documented and the long-event compatibility checks added to
> > acceptance testing.
> The proposed fix is proportionate, reversible, and should land before
> Station Stop v1.

---

## Appendix C — CODEX final disposition after field validation

Received 2026-08-08 after review of the Stage A reproduction, Stage C
acceptance verdict, implemented guard, priming invariant, saturation tests,
fringe-field row, and above-dead-zone residual.

> The condition is met. I withdraw the conditional qualification and endorse
> Decision 0017 as Accepted. QUORUM 1.8 is field-validated for
> stationary-baseline poisoning.
>
> The evidence is unusually strong:
>
> - 1.7 reproduced the predicted median crossover.
> - 1.8 held the baseline across clear, fringe, north, and south dwells.
> - Long-event saturation behaved correctly.
> - The known PWM-above-dead-zone residual reproduced.
> - Motion restored the poisoned reference.
> - The falsified polarity-stream prediction was reported honestly.
>
> Stage D does not block acceptance of Decision 0017 -- the targeted defect
> and compatibility requirements passed -- but it remains required to close
> the complete 1.8 regression program. Toby's upgrade is a separate
> fleet-readiness requirement.
>
> Final disposition: **PASS. Condition satisfied. Decision 0017 accepted
> without qualification for the baseline-motion-gate change. Dedicated Stage
> D regression and Toby validation remain outstanding operational work.**
