# NGR NAVI — Hall Encounter Lifecycle, Timing, and Staged Implementation
**Revision:** 2026-10-25 · **Status:** architectural proposal for adversarial review; **no firmware changes authorized**.
**Evidence:** Claude, `docs/navi/NAVI_DESIGN_TO_IMPLEMENTATION_COMPLIANCE_AUDIT_20261025.md`, commit `44be78a`; firmware audit target `05393117`. Companion expanded DOCX was prepared in ChatGPT.

## 1. How the idea developed
Otto completed a clockwise manual-only circuit from MM50/51 to a stop between MM55 and MM56, with 176 confirmed encounters and MM55 correctly last confirmed. Its final encounter remained open at rest. The operator suggested that stopping in a magnet's field naturally prevents return to baseline, and that a stationary encounter should not be forced closed. A future manual reverse may naturally carry the Hall sensor back out of the field.

The operator challenged an earlier description that PWM-90 magnet-to-magnet duration determines when an encounter should close. His intended clock starts at the qualifying 70×2 Hall departure and predicts the next qualifying departure. Encounter completion and magnet-to-magnet arrival prediction are different questions. **Audit correction:** the actual October 25 firmware did NOT use MM-to-MM timing to close encounters. Its timing profiles were empty and timing was inert in those tests. Independent arrival protection would be a new design decision, not a fix to an existing timer.

The operator then reasoned that after the principal magnetic excursion recedes substantially, the encounter may be released without waiting for perfect baseline return. The magnet's principal recognition task is already underway; its trailing lobes need not keep the encounter open. The predicted next arrival and a separate conservative earliest physically plausible arrival can prevent a trailing lobe from impersonating the next magnet. At very slow speeds or after a stop, elapsed time alone does NOT establish displacement; polarity and width are supporting evidence, not guaranteed safeguards. IR is currently unreliable and is not a dependency.

**Important design-status correction:** existing governing records require retrospective qualification after observed return to baseline. They do not approve immediate geographic identification at 70×2 onset. Early recession-based closure and earlier target confirmation are **proposals requiring a new decision**, not already-established rules. A 70×2 event is a candidate observation; geographic acceptance is a separate event.

## 2. Why the October 25 failures matter
The compliance audit distinguishes faithful high-level navigation architecture (one target, continuous Hall recording, no search/recovery, no shape veto) from unapproved low-level choices (frozen closure reference, 10 × 20 ms within ±20 ADC, discarding new onsets while one encounter remains open).

MM157 replay: at Otto-clock 215.580 s, an onset opened against a frozen reference of 1957.0. Raw Hall background subsequently rose about +116 ADC and was still about +81 high five minutes later. The required return to the frozen reference never occurred. **323 later onsets were silently discarded**; 233 onset clusters matched the mapped MM159–MM49 polarity sequence with zero mismatches. The raw shift's physical cause is unproven. This is evidence that navigation rejected available observations, not that Hall stopped observing magnets.

The audit also demonstrates **BASE-03 noncompliance**: the baseline estimator admitted magnetic evidence after a single ordinary-looking block, including a 2182 block peak; a subsequent baseline estimate of 2126.9 exceeded an approximately 2090–2115 background. Fixing magnetic-tail admission is an independent obligation, not a side effect to assume early closure resolves.

The October 24 onset investigation had already identified the frozen-reference/single-open-encounter lockout. Removing a 10-second abandonment path left an indefinite lockout. **This document does not propose restoring a universal timeout.** The governing conference Proposition 6B (arrival-boundary protective stop) conflicts with its later withdrawal recorded only in the firmware README; operator ruling is required.

## 3. Separate responsibilities
1. **Observe:** continuous raw Hall and candidate onsets, including while an encounter is open. Do not silently discard evidence.
2. **Detect:** 70 ADC departure on two consecutive samples, timestamp on second sample, opening polarity captured. Preserve governing sentinel.
3. **Lifecycle:** opening, peak, recession, closure are signal observations. Current full-RTB closure is implemented but its numerical reference/band/persistence were never approved. Early recession closure is proposed and requires evidence and authorization.
4. **Eligibility:** closed encounter does not automatically make the next magnet eligible. Separate conservative physical-arrival boundary from expected-arrival prediction.
5. **Match:** eligibility is not geographic confirmation. Compare expected map target, polarity, time, and optionally width. No unapproved hard width veto or search/recovery.
6. **Baseline:** median of ordinary inter-magnet space, explicitly excluding magnetic tails (BASE-03). The collection rule must be reconciled with encounter semantics.
7. **Stop/reverse:** stopping within a field need not close the encounter or imply failure. Manual reverse must be tested; automatic-mode no-reverse assumptions cannot be imported.

## 4. Timing distinctions and limitations
For accepted consecutive magnet arrivals, `delta_t = t_next - t_current`. Historical intervals at PWM 90 and mapped spacing can predict the expected next arrival. A defensible conservative maximum speed can define a *minimum plausible elapsed time* before the next mapped magnet could be reached. **Prediction is not permission; minimum elapsed time is not proof of displacement.** If the locomotive stops or creeps and IR is invalid, a time-only gate may eventually expire while the same magnet's trailing lobe remains nearby. Report the uncertainty; do not invent a motion sensor.

## 5. Established, proposed, unresolved
**Established:** continuous Hall; 70×2 candidate onset; one expected target; no search/recovery; shape diagnostic only; nonqualifying evidence leaves target pending; ordinary-track baseline median without magnetic tails; no timer-driven waveform completion.

**Proposed, not approved:** significant-recession closure; independent next-onset reachability gate; fresh-departure rearm; overlap handling; whether/when candidate onset can confirm map position before RTB; handling persistent raw background shifts.

**Unresolved:** recession threshold/persistence/reference; robust stationary/motion evidence without IR; polarity/width veto authority; how to reconcile Prop 6B removal; treatment of a never-completing encounter; actual flashed binary identity (no commit hash in telemetry); absent MM56/MM23 raw analyses.

## 6. Strict implementation rollout — one step per review gate
**Step 0 — READ-ONLY reconciliation.** Read full audit `44be78a`, governing conference and agreements, current sketch/headers, October 24 investigation, and raw evidence. Produce file/function/line traceability, conflict ledger, and proposed minimal diffs. Verify no MM timer currently closes encounters. **STOP; no edits.**

**Step 1 — Observability only.** Instrument every 70×2 onset, including suppressed/overlapping ones; live and frozen baseline/reference, encounter open/close reason, PWM, target, timestamps, suppression counters. Preserve NSR raw collection. Regression must show unchanged navigation counts. **STOP.**

**Step 2 — BASE-03 baseline admission.** Using MM157 and clean Otto/Toby replay, propose and obtain approval for a collection rule excluding tails and full magnets. Implement behind a switch; compare baseline bias and onset sequence. No other lifecycle changes. **STOP.**

**Step 3 — Experimental early closure.** Derive signed recession/peak/persistence/hysteresis from real traces; do not invent constants. Implement behind a feature flag, *without changing target confirmation*. Replay multi-lobe, stop-on-field, and +80–160 ADC raw shift cases; quantify premature closure, never-closure, onset visibility. If data insufficient, **STOP and request traces**.

**Step 4 — Independent arrival guard.** Derive mapped spacing and defensible conservative Vmax; implement/log earliest-plausible-arrival separately from predicted MM-to-MM interval. Guard may reject impossibly early subsequent lobes; must not assert travel merely because time elapsed. Test stationary and creeping cases. **STOP.**

**Step 5 — Rearming/overlap.** Define fresh departure after early close, same-field lobes, background shifts, and concurrent candidates. Do not silently discard observations. Test stop, restart, slow creep, and manual reverse. **STOP.**

**Step 6 — Geographic qualification.** Define exact candidate→confirmed transition using map polarity, time, and width diagnostics, preserving one-target/no-search behavior. Explicitly reconcile governing retrospective-RTB qualification before any change in acceptance semantics; no automatic width or polarity veto without approval. **STOP.**

**Step 7 — Field authorization.** Compare deterministic offline replays of MM157, recovered MM56/MM23 if available, successful 176-encounter stopped lap, Toby clean laps, multi-lobes, stops/reverses. Report correct MM advances, false advances, suppressed lobes, missed arrivals, baseline bias, stuck-open time, ambiguities, and rollback. Seek authorization before flashing a single manual-only locomotive.

For every authorized step: one scoped commit; exact files/functions; tests; before/after telemetry; explicit rollback; no CTO, dispatcher, IR, or unrelated changes; **STOP for David's approval before next step**.

## 7. Questions for independent adversarial review (Claude)
- Which claims above conflict with governing conference/decision records? Quote source and status; do not treat a proposal as approved.
- Is early recession closure safe against multi-lobe Hall signatures and sustained ADC offsets? What trace evidence would falsify it?
- Can a time-only physical reachability guard actually prevent trailing-lobe false positives after stops/creep? State its exact limitation.
- Does baseline BASE-03 need correction before early closure can be meaningfully tested?
- Does proposed Step 6 change the accepted qualification cycle? What explicit operator decisions are needed?
- Are the rollout steps minimal, ordered, reversible, and independently testable? Identify hidden coupling and missing tests.
- Distinguish confirmed audit facts, operator proposals, unresolved choices, and firmware implementation assumptions. **Do not edit firmware or governing records.**

**Review only. No implementation authorization is implied.**
