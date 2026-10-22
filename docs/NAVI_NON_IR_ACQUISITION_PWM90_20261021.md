# NAVI — non-IR target acquisition at PWM 90

**Date:** 2026-10-21
**Status:** Architectural correction and consolidation. Not a decision
record. No firmware, research code, or specification has been modified to
produce this document.
**Assignment:** Two sequential corrections from David to the governing
assumptions behind the four prior documents in this series:
(1) IR is excluded entirely from the current development stage, and the
historical ±15% distance tolerance is vestigial for this model; (2) the
automatic distance/time-based transition to LOST proposed in
[`NAVI_ACQUISITION_EXCEPTION_DESIGN_REVIEW_20261021.md`](NAVI_ACQUISITION_EXCEPTION_DESIGN_REVIEW_20261021.md)
is itself withdrawn — "NAVI is not playing roulette." This document
supersedes the IR-dependent and bound-dependent portions of all four
prior documents, which are marked accordingly and left otherwise intact
as the durable record of what was proposed, corrected, and why.
**Authorization:** Research and design only (`AGENTS.md` §2).

## The two corrections, stated precisely

1. **No IR, for now.** IR has not been validated sufficiently to
   participate in recognition or geographic decisions at this stage. The
   historical ±15% distance tolerance, which was built around IR-era
   practice, does not apply to this model and is not replaced by another
   percentage. Available evidence is: the Hall waveform, the established
   baseline, the target's known polarity, the mapped magnet sequence and
   spacing, and PWM/elapsed-time as approximate, not-yet-validated
   movement information.
2. **No automatic geographic contradiction from distance or time
   imperfection.** The map represents physical reality, not a probability
   distribution over candidates. When NAVI closes MM045, its target is
   MM046 — not a hypothesis competing with MM047 that a slow curve or an
   imprecise distance estimate can defeat. PWM and elapsed moving time may
   provide approximate context; neither may be used to manufacture a
   geographic contradiction, trigger LOST, or abandon the target, without
   supporting evidence that this has actually occurred. The automatic
   bound proposed in the acquisition-exception review (§7–§8 there) is
   withdrawn, not re-derived with a different number.

## The architecture, restated under both corrections

```
CLOSE  -> confirmed magnet M.
       -> TARGET := the next mapped magnet after M (deterministic — the
          known sequence, not a hypothesis). Its polarity and approximate
          mapped spacing are known from the map.
BASELINE -> fit level/slope/noise from quiet Hall samples since M (0107).
ACQUIRE  -> watch the Hall trace relative to that baseline for a
            departure of TARGET's known polarity.
            - No qualifying departure yet, however much elapsed time or
              PWM-implied distance has passed: continue. This is not a
              failure state and triggers nothing.
            - A departure that clears the existence floor (sustained,
              ΔBIC > 0 over baseline noise) and whose resolved polarity
              matches TARGET, confirmed by a genuine return to baseline:
              -> CLOSE AND ADVANCE. M := TARGET's magnet; compute the new
              TARGET; go to BASELINE.
            - A departure that clears the existence floor but whose
              resolved polarity contradicts TARGET: dispose of this
              candidate (log it for diagnosis); TARGET is unchanged;
              return to watching for it. This is not a geographic
              contradiction — it is evidence that *this candidate* was
              not the target, nothing more.
            - A departure that never clears the existence floor: no
              candidate ever existed; continue.
```

There is no third branch and no automatic exit from ACQUIRE other than
successful closure. PWM/elapsed time may still be used, non-decisively,
to set expectations about *when* to look most attentively, or to inform
how much benefit of the doubt a marginal candidate deserves (this was
already the non-gating role spacing played even before IR was withdrawn)
— but it has no authority to reject the target, declare it lost, or
invoke recovery.

## What this removes from the prior documents, and why

- **The reachability-constraint/candidate-set-narrowing machinery**
  (`NAVI_TWENTY_QUESTIONS_DECISION_MACHINERY_20261021.md`) computed a set
  of physically reachable mapped magnets from IR distance and a ±15%
  tolerance. With IR withdrawn and the tolerance declared vestigial, this
  computation has no inputs it can trust. It is not replaced by an
  equivalent PWM-based version — per correction 2, PWM/time is not to be
  used that way at all. TARGET is simply the next mapped magnet; there is
  no set to narrow.
- **The automatic LOST-transition bound**
  (`NAVI_ACQUISITION_EXCEPTION_DESIGN_REVIEW_20261021.md`, §7–§8 and its
  addendum) is withdrawn outright, not re-derived from PWM/time. The
  addendum's own reasoning — distance evidence should inform confidence,
  never gate a decision — turns out, under this stronger correction, to
  argue against building *any* automatic distance/time-triggered
  transition, not merely against building one from an arbitrary
  percentage. The MM008 migrated-magnet case used to justify the bound's
  necessity was a real documented event, but it was discovered and
  diagnosed by the operator and QUORUM's own quarantine mechanism at the
  time, not by a pre-computed geographic timeout — it is evidence that
  recovery machinery (0098) should exist for when a genuine failure is
  observed, not evidence that ordinary ACQUIRE needs to compute, on its
  own, when to give up.
- **What is not removed:** 0098's recovery search remains available,
  unchanged, for genuinely triggered circumstances — session startup
  before any magnet is confirmed, an explicit operator redeclaration, or
  a failure pattern actually demonstrated in real recordings and
  investigated afterward. What is removed is any automatic path from
  ordinary ACQUIRE into that machinery based on a precomputed threshold.
  This is the direct application of principle 13 (general model first;
  exceptions from evidence) and `AGENTS.md` §8 (do not add thresholds or
  control complexity to handle a hypothetical condition without evidence
  that it is physically plausible or has occurred) to this specific
  design choice — the bound I had proposed was exactly such an
  unevidenced addition, however carefully its number was derived.
- **Scope note, not a gap:** this document covers geographic recognition
  only. It does not alter E-stop, manual override, or any other motor/
  safety authority, which already takes precedence over automatic
  navigation by standing project constraint and is architecturally
  separate from this recognizer (RvmNavigator.h's own precedent:
  "observation only... nothing in this sketch reads its state to drive
  the locomotive"). A target that is slow to acquire does not, by itself,
  stop the locomotive or withdraw any other authority; it simply means
  geographic position has not yet advanced.

## Evaluating this architecture, non-IR, at constant PWM 90

Per David's explicit request, the question is whether NAVI can recognize
each successive mapped magnet from Hall waveform shape, baseline,
expected polarity, and the known sequence alone — not whether a
particular reliability percentage is reached, and not whether a
precomputed bound value is well-chosen (there is none to choose).

1. Freeze an independently bounded PWM-90 Otto replay set, as both prior
   evaluation plans already required — the existing historical ADC-
   derived annotations remain disqualified as ground truth by the
   recognizer's own `REPORT.md`, for the same reason as before.
2. Replay CLOSE → BASELINE → TARGET → ACQUIRE → CLOSE exactly as restated
   above: TARGET from the real map (`RouteMap.h`) alone, ACQUIRE using
   only Hall evidence and the target's known polarity, with PWM/elapsed
   time recorded but given no decision authority.
3. Report, for each successive target in sequence: acquired correctly;
   acquired after disposing one or more contradicting candidates first
   (record how many, and how long each took, purely as telemetry); or
   **not yet acquired by the end of the available recording** — which is
   not scored as a failure of the architecture, but flagged explicitly
   for inspection, exactly as David's closing instruction asks: "if actual
   recordings demonstrate a circumstance in which that assumption fails,
   we can investigate it."
4. Specifically look for, and report separately, any recorded stretch
   where the target's expected curve simply never appears at all before
   the recording ends or before a different, later target's curve
   appears unambiguously — that is the observable signature an automatic
   bound would have been trying to anticipate, and it is the only kind of
   evidence that should ever motivate adding one back.
5. Compare against the standalone recognizer's own 75-declaration result
   (`98db802`) as before: does removing IR and the tolerance change which
   of those 75 the target-acquisition process would have accepted,
   disposed, or left pending, relative to the recognizer's unanchored
   declarations?
6. Do not proceed to PWM 60, acceleration, stop/restart, or Toby until
   this review is done and authorized, as in every prior document in this
   series.

## What is established evidence vs. proposed here

| Status | Item |
|---|---|
| Established | Forward-only deterministic map traversal; the real MM44–48 map values; 0104/0106/0107/0108 as cited in the prior documents (none of which depended on IR or the tolerance); 0098's recovery scope, now explicitly reserved for genuinely triggered circumstances only. |
| Corrected by this document | IR and the ±15% tolerance are removed as inputs, not replaced by a PWM-based equivalent; the automatic LOST bound is removed outright, not re-derived; TARGET is stated as deterministic, not a hypothesis. |
| Proposed here, not yet evidenced | That ACQUIRE with no automatic exit is sufficient for the full PWM-90 Otto corpus — this is exactly what the evaluation in the prior section is for. |
| Not established | Any reliability number; whether a genuine stall (MM008-class event) actually occurs in available PWM-90 recordings; PWM-to-speed/distance validity at any level of precision; anything beyond PWM 90/Otto. |

## References

- `docs/NAVI_INTEGRATED_RECOGNITION_ARCHITECTURE_PROPOSAL_20261021.md`,
  `docs/NAVI_TWENTY_QUESTIONS_DECISION_MACHINERY_20261021.md`,
  `docs/NAVI_TARGET_ACQUISITION_EVALUATION_20261021.md`,
  `docs/NAVI_ACQUISITION_EXCEPTION_DESIGN_REVIEW_20261021.md` — the four
  documents this one partially supersedes, each marked accordingly.
- `docs/decisions/0098`, `0104`, `0106`, `0107`, `0108`.
- `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/RouteMap.h`,
  `RvmNavigator.h` (commit `3a8953c`).
- `research/navi_wolf_pack/normal_curve_recognizer.py` (commit `98db802`).
- `AGENTS.md` §8 (track evidence has priority; do not add control
  complexity for a hypothetical condition without evidence it has
  occurred) — the principle this correction applies most directly.
