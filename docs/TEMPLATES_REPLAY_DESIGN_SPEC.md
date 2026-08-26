# TEMPLATES Replay Design Spec

Status: **design-only, investigatory**. Nothing in this document approves a firmware implementation, a flash, or a field operation. It exists to be tested against real capture data at a later replay stage; every constant below is either *validated by direct replay against the real captures* (stated explicitly where true) or *provisional pending replay evidence* (stated explicitly where true). None is asserted as production-ready.

**Tool identity, stated explicitly:** this document specifies `tools/templates_replay_spec.py`, the raw-SAMPLE experimental candidate — it reads only `phys_*`/`ctl_*` measurement fields, never QUORUM's own `dec_*` decisions. `tools/templates_replay.py` (committed separately at `d05a8b4`) is Sam's own, independent QUORUM-event-summary baseline — it reads QUORUM's `dec_*` decision output as input. The two are not variants of the same tool; they are different designs under confusingly similar names, and **neither is firmware-approved or canonical**. See `docs/TEMPLATES_REPLAY_RESULTS_REPORT.md` for the raw-sample candidate's replay verdict: rejected as currently tuned, pending correction of contradiction-ring accumulation and same-polarity omission masking.

This document is deliberately independent of `firmware/QUORUM/QUORUM.ino`. QUORUM is read and cited here only as (a) a source of field evidence, (b) a description of known failure mechanisms, (c) a comparison baseline, and (d) a source of specific, independently-reusable infrastructure (the magnet map, the PWM→speed calibration, the safety/manual-authority/MQTT/operational interfaces). QUORUM's acceptance ordering, recovery triggers, candidate fence, evidence-ring semantics, quarantine behavior, and advance-before-validation logic are treated as a **failed attempt** and are not reused. Where this document does reuse a QUORUM constant or pattern, it says so and justifies the reuse on its own terms — never "because QUORUM did it."

**Matching QUORUM's navigation decisions is not a design goal of this document.** Where a choice below would reproduce QUORUM's false inclusion, coordinate corruption, incorrect adoption, or `NO_QUORUM` behavior, that is treated as evidence against the choice, not for it.

## Contents

0. [Framing: what changed from QUORUM, item by item](#0-framing-what-changed-from-quorum-item-by-item)
1. [Input contract and evidence base](#1-input-contract-and-evidence-base)
2. [Passage acquisition](#2-passage-acquisition)
3. [Artifact rejection (category 1 — trash)](#3-artifact-rejection-category-1--trash)
4. [Edge-lobe / companion merge contract](#4-edge-lobe--companion-merge-contract)
5. [Physical arrival gate](#5-physical-arrival-gate)
6. [Map validation and the three-way disposition](#6-map-validation-and-the-three-way-disposition)
7. [Recovery](#7-recovery)
8. [Acceptance criteria](#8-acceptance-criteria)
9. [Output data schema](#9-output-data-schema)
10. [Consolidated constants table](#10-consolidated-constants-table)
11. [Explicitly deferred / open questions](#11-explicitly-deferred--open-questions)
12. [Sources](#12-sources)

---

## 0. Framing: what changed from QUORUM, item by item

The operator named six specific QUORUM mechanisms as open for replacement. This table states, up front, what this document does with each one, so the departure is checkable rather than asserted.

| QUORUM mechanism | Disposition in this design |
|---|---|
| Acceptance ordering (advance `navMm` before polarity is compared, `acceptEvent()` → `processNormalComparison()`) | **Replaced.** §6 makes map-polarity comparison a strict precondition of any position mutation. There is no code path in this design that advances `primary_position` and *then* checks polarity. |
| Recovery triggers (`missStreak==QUORUM_TRIGGER` i.e. 3 consecutive disagreements before evaluation opens) | **Replaced.** §7a's hypothesis test runs on *every* `CREDIBLE_CONTRADICTION`, not after a streak — this is what closes Finding 14 (pure omission, with no disagreeing event at all, must not be invisible to recovery). §7b's liveness check is a second, independent, streak-free trigger for the no-events-at-all case. |
| Candidate fence (`QUORUM_OFFSETS = {-1,0,+1,+2,+3,+4}`) | **Replaced.** §7a's offset set is `{+1..+12}` with **no negative offset** — see §7a for why QUORUM's `-1` ("one phantom wrongly accepted") has no analogue here: this design cannot insert a phantom into `PositionRing` in the first place. |
| Evidence-ring semantics (one ring, `evRing`, fed by every accepted event including disagreements) | **Replaced.** §6 defines two structurally distinct, disjoint rings — `PositionRing` (confirmed advances only) and `ContradictionRing` (credible contradictions only) — with an explicit invariant that no event ever enters both. |
| Quarantine behavior (`adoptionPendingValidation` / `candidateExcluded[]` / `handleFailedAdoption()`) | **Eliminated, not replaced.** §7c explains why: this machinery exists in QUORUM only to undo a provisional adoption made *before* validation. Since this design never adopts before validation, there is nothing to provisionally hold and nothing to invalidate. |
| Advance-before-validation logic | **Replaced.** Same as acceptance ordering, above — this is the single architectural change everything else in §5–§7 is built around. |

What is explicitly **not** touched: E-stop, manual/AUTO bicameral authority, the MQTT command/publish interfaces, and the station/operations layer's own state machine. Where this design needs a stop, it *requests* one through those existing interfaces; it does not redesign how a stop is executed or who is allowed to command motion.

The doctrine documents (`MAGNET_ADMISSION_DOCTRINE.md`, `TARGET_ACQUISITION_DECISION_LOGIC.md`) are treated as mandatory *to consider*, not as immutable conclusions, per their own stated status. Every place this document departs from a literal reading of the doctrine text is called out explicitly at the point of departure (see especially §5's two-tier arrival gate and §7a's pruned candidate fence).

---

## 1. Input contract and evidence base

### 1.1 What this design may read

From `qt_decode.py` SAMPLE rows only: `phys_raw`, `phys_baseline`, `ctl_pwm_actual`, `ctl_pwm_commanded`, `ctl_dir`, `t_ms`, `session`. `ctl_estop` is available for diagnostic completeness but needs no separate handling — E-stop forces `ctl_pwm_actual`/`ctl_pwm_commanded` to zero (`servicePwmRamp()`), which is already covered by the PWM-based suspend rule in §2.4. `ctl_late` is a scheduling-jitter diagnostic, not part of any disposition rule below.

`det_*` and `dec_*` columns (QUORUM's own detector interpretation and navigation decisions) are **never** read as input anywhere in this design. They are used exactly once, structurally: as the "failed baseline" QUORUM's own decisions would have produced on the same raw data, for comparison at replay time. No formula in §2–§9 references a `det_*`/`dec_*` field.

**GAP-row gotcha (found while building the evidence base for this document, recorded so the implementer doesn't repeat it):** `qt_decode.py` emits `GAP` rows independently per `(session, rec_type)` stream — SAMPLE, DECISION, STATUS, and ANCHOR each have their own batch-sequence space. A `GAP` row is relevant to §3's `INCOMPLETE` rule *only* when it was generated from the SAMPLE stream (its `info` text names `"SAMPLE record(s) lost"`). A DECISION- or STATUS-stream `GAP` does not indicate missing physical measurement and must not be treated as a passage-acquisition problem. The same discipline applies to state machines in general: DECISION/STATUS/ANCHOR rows are interleaved throughout the merged, time-sorted CSV and must **not** be treated as breaking continuity of the SAMPLE stream — only a `SESSION` row (a genuine new-boot discontinuity) resets acquisition/dwell/interval-reference state (§2.6, §5.6). An early version of this document's own verification tooling reset state on every non-SAMPLE row and silently truncated every dwell segment to a few seconds as a result; the bug was caught by comparing against the doctrine's own reject counts (§1.3) and is recorded here as an explicit implementation caution.

### 1.2 Configuration must be read from the capture, not from prior documentation

`REVIEW_NOTES.md`'s "Otto config lines 100–166" section describes the live entry threshold as 70 counts (`HALL_DEADBAND_COUNTS=25` + `HALL_ENTRY_MARGIN_COUNTS=45`). Direct inspection of the `STATUS` records embedded in **all four** 2026-08-24/25 captures (`deadband=25 entry_margin=13`, i.e. an open threshold of 38 counts, uniformly across Toby CCW, Otto CCW, Otto CW, and Otto change1) shows the firmware was retuned before these captures were taken. This is exactly the kind of drift a design spec must not silently inherit. Every threshold cited below as "matches the live captures" was checked against the captures' own `STATUS` records or against a direct independent replay, not against prior documentation.

### 1.3 This document's own replay evidence

In addition to the doctrine's own field-evidence table and the prior 18-finding review, this document is grounded in a direct, independent, single-pass reconstruction against the four real capture files, reading **only** `phys_raw`/`phys_baseline`/`ctl_pwm_actual`/`ctl_pwm_commanded` (never `det_*`/`dec_*`). Method: open a candidate at `|phys_raw − phys_baseline| ≥ 38`, close it at `|phys_raw − phys_baseline| ≤ 25` held for `≥20ms`, track peak, trapezoidal-integrated `|deviation|` flux, and PWM at open/peak/close — i.e., exactly the acquisition rule this document proposes in §2, applied to the raw data with no reference to QUORUM's own detector output. Results:

| Capture | SAMPLE rows | Broad (≥40ms) candidates | Sub-floor (<40ms) candidates | Sub-floor median/p90/max duration | Max PWM observed |
|---|---:|---:|---:|---|---:|
| Toby CCW (clean control) | 2,790,270 | 1,189 | **1** | 35ms | 90 |
| Otto CCW | 2,191,680 | 541 | 628 | 21.0 / 29.0 / 39.0 ms | 90 |
| Otto CW (restarted, worst) | 3,656,430 | 1,036 | 654 | 21.0 / 28.0 / 39.0 ms | 120 |
| Otto change1 (post-rewiring, Run 1) | 501,030 | 224 | 6 | 31.5 / 34.0 / 35.0 ms | 120 |

This independently reproduces the doctrine's field-evidence table (Toby: 1 sub-40ms reject; Otto CCW: 626 vs. 628 here; Otto CW restarted: 639 vs. 654 here — QUORUM's own onboard `floor_rejects` counter for that session's `STATUS` record reads exactly 639, confirming the small excess here is this document's own detector-edge differences, not a data problem) **and** independently confirms the sub-floor population's tight 20–22ms clustering, using a completely independent pipeline. It also extends real, validated PWM coverage from the doctrine review's PWM90 ceiling to **PWM120** (observed directly in Otto CW and Otto change1), and surfaces two things the prior review could not see without this reconstruction:

1. **Genuine passages and the frozen-baseline pathology can both hide inside a naive duration-only "broad" bucket.** The maximum candidate duration in Otto CCW was **137.6 seconds**; in Otto CW, 32.8 seconds; in Otto change1, 77.4 seconds. All three are single-digit-second-or-less in every other respect except duration — they are `phys_baseline` staying frozen (QUORUM's own `updateBaseline()` stops adapting at `actualPwm ≤ MOTOR_DEAD_ZONE_PWM=20`) while the loco sits stationary and the raw signal drifts slowly outside the frozen reference band. This is not a passage; it is a baseline-tracking failure. §2.5 and §3.3 name this explicitly as its own artifact category (`EXCESSIVE_OPEN`), which a duration-floor-only design would never catch (a 137-second event trivially clears any duration floor).
2. **Integrated flux is not a free, PWM-invariant discriminator.** Per-PWM-band medians look reassuring (e.g. Otto CW: 66,883 → 25,455 → 16,333 → 11,057 count·ms as PWM rises), but the true *minimum* observed genuine-passage flux in the cleanest capture (Toby, PWM70–99 band) is **1,211 count·ms**, while the worst observed spike flux in the most contaminated capture (Otto CW) is **740 count·ms**. The safe margin between "known spike" and "known genuine, clean-control passage" is only ≈1.6×, not the 5×+ the per-band medians alone would suggest. §3.2 sets the flux floor from this narrower, honest number and flags it as the single highest-priority constant in this document for replay revalidation.

---

## 2. Passage acquisition

### 2.1 State and scope

Acquisition runs per-session (a `SESSION` row is an absolute discontinuity — see §2.6). At any instant there is at most one open `RawCandidate` per session. Let `d(t) = phys_raw(t) − phys_baseline(t)` (signed deviation).

### 2.2 Entry / exit thresholds

| Constant | Value | Status |
|---|---|---|
| `ENTRY_COUNTS` | 38 | Validated by direct replay (§1.3) |
| `EXIT_COUNTS` | 25 | Validated by direct replay (§1.3) |
| `EXIT_HOLD_MS` | 20 | Reused from QUORUM's `EVENT_EXIT_HOLD_MS`; not independently re-derived, flagged provisional |

Open a candidate when `|d(t)| ≥ ENTRY_COUNTS`. While open, close it when `|d(t)| ≤ EXIT_COUNTS` has held continuously for `EXIT_HOLD_MS`; the candidate's `close_t` is the sample at which the hold completes. A brief re-excursion above `ENTRY_COUNTS` during the hold window cancels the hold and keeps the same candidate open (this is deliberate: it is what lets a single genuine passage's internal wiggle stay one candidate instead of fragmenting — see §4 for the separate, later-in-time companion case).

TEMPLATES is free to choose different entry/exit values than QUORUM's onboard detector — offline replay has the complete `phys_raw` stream regardless of what threshold happened to be live in firmware at capture time, so this is a genuine design choice, not an inheritance. 38/25 is adopted here because this document's own replay (§1.3) confirms it recovers a broad-passage population whose duration/peak statistics match the doctrine's independently-reported medians (163–168ms, 180–198 counts) within a few percent, and a sub-floor population whose count and duration match the doctrine's field-evidence table almost exactly — i.e., it is *validated*, not merely inherited. Nothing else in this document depends on the exact entry/exit value; changing it only changes when a candidate starts being tracked, not how it is subsequently judged.

**Amplitude thresholds are not PWM-normalized.** Peak amplitude is close to PWM-invariant in the replay evidence (Toby broad-peak medians: 184 / 201 / 198 across the three observed PWM bands; Otto CW: 176 / 172 / 188 / 169 across four bands) while duration and flux both scale strongly with PWM (§1.3, §3.1). Entry/exit/peak-based rules therefore use fixed counts; only duration- and flux-based rules (§3) need PWM-context reasoning.

### 2.3 PWM is sampled three times, not once

This directly addresses the prior review's Finding 1: QUORUM's `detectorSample()` samples `evStartPwmActual`/`evStartPwmCommanded` once, at open, and never again — so a width/flux rule keyed to that single sample cannot tell whether the passage's acquisition window straddled a PWM transition.

A `RawCandidate` records PWM at three distinct instants:

- **at open**: `ctl_pwm_actual`/`ctl_pwm_commanded`/`ctl_dir` at the sample that crossed `ENTRY_COUNTS`.
- **at peak**: the same three fields at the sample where `|d(t)|` reached its maximum over the candidate's open window.
- **at close**: the same three fields at the sample where the exit-hold condition completed.

Derived diagnostic: `spans_pwm_transition = |pwm_actual_at_close − pwm_actual_at_open| > RAMP_DELTA` where `RAMP_DELTA=10` (reused from QUORUM's `GATE_RAMP_DELTA`, purely as a labeling threshold here — see §5.4 for why this design does not need a separate RAMP *gate* the way QUORUM does). This document's own replay found this flag true for 0.0–2.2% of broad candidates across the four captures — real, but rare. It is recorded as a diagnostic field on every candidate (§9), not used to change a disposition; the evidence does not support either "always merge across a ramp" or "always split," so no such rule is asserted here.

### 2.4 Acquisition suspends while confirmed-stationary

`SUSPEND_PWM_FLOOR = 20` (= QUORUM's `MOTOR_DEAD_ZONE_PWM`, reused because this is architecturally the same boundary QUORUM's own `updateBaseline()` already uses to stop adapting the baseline — "adapt only with positive evidence of tractive motion" — not because QUORUM's navigation policy is being reused). While `ctl_pwm_actual ≤ SUSPEND_PWM_FLOOR`, **no new candidate opens**, regardless of what `d(t)` does. A deviation crossing `ENTRY_COUNTS` during this condition is not recorded as a candidate at all — not even as an `ARTIFACT` disposition — because the reference it would be measured against (`phys_baseline`) is not being kept current by the same logic that produced it.

This is a direct, evidence-driven response to §1.3's finding: every multi-ten-second-to-multi-minute spurious "candidate" this document's replay found (137.6s, 77.4s, 32.8s) occurred with PWM held at or near zero throughout. Suspending acquisition there removes this failure mode at the source, rather than trying to catch its output after the fact with a duration ceiling alone (§3.3 keeps that ceiling too, as defense in depth, since a candidate can still be open when PWM *drops into* the suspend condition mid-passage — see below).

An already-open candidate is **not** force-closed when `ctl_pwm_actual` drops to or below `SUSPEND_PWM_FLOOR` mid-passage; it continues to be governed by its own duration/flux/`MAX_OPEN_MS` rules (§3). Genuine passages complete (per §1.3, the largest clean broad-passage duration observed anywhere was 2,193ms), so this does not meaningfully expose the frozen-baseline failure mode; it only avoids adding a second special case (mid-passage abort) on top of the ceiling that already exists.

**Known interaction not independently checked in this document:** station final-approach deliberately ramps PWM down while still passing the last marker before a stop (`serviceStations()`'s `ST_FINAL` phase, "M+1: ease to finalPwm"). If a station's configured `finalPwm` is at or below 20, a genuine passage occurring during final approach could be suppressed by this rule. This document does not have the `STATIONS[]` table's `finalPwm` values in evidence and does not assert they are safe; §11 lists this as an open item for replay to check directly.

### 2.5 Maximum open duration

`MAX_OPEN_MS = 3000`. Derived from §1.3: the largest *clean* broad-passage duration observed across all four captures (excluding the frozen-baseline artifacts) is 2,193ms (Toby). 3,000ms gives ~37% headroom above the single largest genuine observation in evidence, while sitting roughly 25–2,300× below the frozen-baseline artifacts it exists to catch (32.8s–137.6s). A candidate that reaches `MAX_OPEN_MS` while still open is force-closed and disposed `ARTIFACT` / `EXCESSIVE_OPEN` (§3.3) — never treated as a passage regardless of its accumulated peak or flux.

### 2.6 Session boundaries

A `SESSION` row is an absolute discontinuity, mirroring QUORUM's own reboot-clears-navigation-state behavior (basic infrastructure hygiene, not the acceptance-ordering/recovery-trigger logic under replacement). On a `SESSION` row: any open `RawCandidate` is discarded with **no disposition record** (it never completed; the boot session that would have closed it is gone). All of §2's tracking state, §5's previous-accepted-interval reference, §6's `primary_position`/`primary_direction`, and §7's `PositionRing`/`ContradictionRing` are invalidated and must be re-established by a fresh position declaration before any disposition beyond `ARTIFACT`/diagnostic can be produced.

### 2.7 RawCandidate fields produced by this layer

`event_id, session, open_t_ms, peak_t_ms, close_t_ms, duration_ms, polarity (sign of d at OPEN — fixed at the first threshold crossing so a same-passage internal wiggle cannot flip it, mirroring QUORUM's own "polarity decided by the opening pole" reasoning, which is a sound acquisition-layer engineering principle independent of QUORUM's navigation policy), peak_abs, signed_flux, abs_flux (trapezoidal ∫|d(t)|dt in count·ms), pwm_actual_at_open, pwm_commanded_at_open, pwm_actual_at_peak, pwm_commanded_at_peak, pwm_actual_at_close, pwm_commanded_at_close, direction_at_open, spans_pwm_transition`.

---

## 3. Artifact rejection (category 1 — trash)

A candidate disposed `ARTIFACT` here (or by the physical-arrival-gate rules cross-referenced from §5) produces: no position advance, no `PositionRing` insertion, no `ContradictionRing` insertion, no hypothesis-score participation, no later resurrection. It may still be recorded diagnostically (§9).

### 3.1 Duration floor

`DURATION_FLOOR_MS = 40`. A candidate with `duration_ms < DURATION_FLOOR_MS` is `ARTIFACT` (`rule_id = ARTIFACT_DURATION_FLOOR`).

Justification, from this document's own replay (§1.3), not merely inherited from QUORUM's identically-valued `EVENT_FLOOR_MS`: the sub-floor population clusters extremely tightly at 20–22ms (median 21.0ms, p90 28–29ms, max 39ms, in *both* Otto CCW and Otto CW) and is essentially disjoint from the genuine-passage population, whose observed minimum durations were 53ms (Toby), 40ms (Otto CW/CCW, i.e. exactly at the floor). Real, PWM-extended margin: even at the fastest validated PWM band (100–130), median genuine duration is 114–129ms — 2.85–3.2× the floor — a materially more comfortable margin than the prior review's PWM255-extrapolation worry (which projected as little as 10–12ms of margin) found, because this document now has real data to PWM120 rather than a linear extrapolation past PWM90.

**Boundary ambiguity, stated honestly:** the genuine population's minimum in two of four captures lands *exactly* at 40ms — the same boundary the spike population's maximum (39ms) approaches. A candidate with `35ms ≤ duration_ms ≤ 50ms` is tagged `boundary_ambiguous=true` (§9) regardless of its disposition. This does not change the disposition (precision-over-recall means an ambiguous case still resolves to `ARTIFACT` if it fails the floor), but it flags the case for human/replay review rather than treating it identically to an unambiguous 21ms spike.

### 3.2 Flux floor

`FLUX_FLOOR = 900` count·ms. A candidate with `abs_flux < FLUX_FLOOR` is `ARTIFACT` (`rule_id = ARTIFACT_FLUX_FLOOR`).

Justification: the worst observed spike flux across all captures is 740 count·ms (Otto CW); the weakest observed *clean-control* genuine-passage flux is 1,211 count·ms (Toby, PWM70–99 band). 900 sits inside that gap with ≈1.22× margin above the worst spike and ≈1.35× margin below the weakest genuine observation. **This margin is tight and is flagged as the single highest-priority number in this document for replay revalidation** — tighter than the duration floor's margin, and resting on single data points at both edges rather than a robust percentile. Because this document cannot independently confirm whether Toby's 1,211-count·ms event is a true magnet passage (as its cleanliness and duration strongly suggest) or itself an edge/partial-capture artifact, `FLUX_FLOOR` is recommended as a **secondary, backstop check** — AND-ed with the duration floor, not relied on as the primary discriminator, since duration separates the two populations more cleanly in the available evidence (§1.3). Neither floor is dropped; both are required to clear (matching the one existing prototype's `passes_morphology()` pattern of requiring both duration and flux, which this document independently arrives at rather than copies).

### 3.3 Excessive open duration

A candidate that reaches `MAX_OPEN_MS=3000` (§2.5) while still open is force-closed and disposed `ARTIFACT` (`rule_id = ARTIFACT_EXCESSIVE_OPEN`), regardless of accumulated peak or flux. This is the direct, named response to the frozen-baseline pathology in §1.3.

### 3.4 Incomplete

A candidate whose `[open_t, close_t]` window overlaps a **SAMPLE-stream** `GAP` row (§1.1's gotcha — DECISION/STATUS/ANCHOR-stream gaps do not count) is `ARTIFACT` (`rule_id = ARTIFACT_INCOMPLETE`). This mirrors `hwt_gate_replay.py`'s precedented `REJECT_INCOMPLETE` gate.

### 3.5 Saturated

A candidate whose `phys_raw` reaches within a small margin of the ADC's configured rail is `ARTIFACT` (`rule_id = ARTIFACT_SATURATED`). The exact rail values (`ADC_MIN_RAIL`/`ADC_MAX_RAIL`) are **not independently confirmed in this document** — they depend on the ESP32 ADC's configured resolution/attenuation, which this document did not verify against the firmware build. Observed `phys_raw` stayed within 1,496–2,236 across every capture checked, far from any conventional ADC rail, so this rule is not expected to fire under the operating conditions seen so far; it is specified for hardware-fault defense and left with symbolic bounds pending that confirmation.

### 3.6 Physically-impossible-early (cross-reference)

A candidate that fails the physical arrival gate is **also** `ARTIFACT` — same trash rule as above, same "no ring, no score, no resurrection" contract. The formula lives in §5, not here, because unlike §3.1–§3.5 (properties of the candidate's own waveform, testable in isolation) the arrival gate depends on navigation history (the previous accepted marker). Two distinct rule IDs are used so replay output can separate the two failure modes (per Decision 0043 / Consequence #2's separate-reporting requirement, applied at a finer grain than the doctrine's own two-way split): `ARTIFACT_HARD_IMPOSSIBLE` and `ARTIFACT_CONTEXTUAL_TOO_SOON` (§5.3).

### 3.7 Deliberately not included: a continuity/smoothness gate

`hwt_gate_replay.py`'s own module docstring documents that a continuity-ratio gate was implemented, found to rest on evidence from exactly two hand-picked excursions, and withdrawn — retained only as a diagnostic field, never wired into any disposition. This design follows the same discipline and does not add a smoothness-based artifact rule: the doctrine's own evidence list (waveform duration, curve structure, peak, flux, baseline quality, completeness, PWM/timing context) does not name continuity, and no independently-collected noise-only baseline exists in this repo to validate a threshold against. If such evidence is collected later, it can be added as its own dispositioned rule; it is not invented here.

---

## 4. Edge-lobe / companion merge contract

This resolves Findings 9 and 10: the doctrine text says TEMPLATES must "merge related lobes into one passage" but specifies neither a data contract nor a same-passage-vs-new-passage rule, and same-polarity companions (the more dangerous case — they can pass the map-polarity backstop) are acknowledged but never characterized.

### 4.1 Merge eligibility test

Given a primary candidate `P` that has **already independently cleared §3's duration and flux floors** (only a floor-passing candidate can anchor a merge — a chain of sub-floor spikes does not productively merge onto each other; there is no `primary_polarity` to inherit and the doctrine's concern is protecting real passages' edge structure, not inventing structure among noise), and a subsequent raw candidate `C` opening after `P`'s most recent lobe closes:

```
eligible(C, P) :=
    (C.open_t − P.most_recent_lobe_close_t) ≤ MERGE_WINDOW_MS
    AND (C.peak_abs / P.merged_peak) ≤ AMPLITUDE_RATIO_MAX
```

Polarity is **not** part of the eligibility test — same-polarity and opposite-polarity companions are tested identically (per Finding 10 and the doctrine's own "must not count each polarity crossing as a separate magnet," which is not qualified to opposite-polarity only).

| Constant | Value | Status |
|---|---|---|
| `MERGE_WINDOW_MS` | 350 | Inherited from the doctrine's own descriptive field-evidence figure ("205 broad responses were followed within 350ms..."); this is the empirical window the doctrine's own analysis already used to identify companions in the first place. Provisional. |
| `AMPLITUDE_RATIO_MAX` | 0.5 | Derived: the doctrine's documented companion population has amplitude ≈41 counts against primary medians of ≈180–198 counts, a ratio of ≈0.21–0.23. 0.5 gives roughly 2× headroom above the observed ratio while still excluding a same-or-larger-magnitude second lobe. Provisional pending direct per-pair replay against the 205-pair Otto CW population (which this document did not compute — see §11). |
| `MAX_LOBES` | 5 | Defensive cap on chain length, so a noise burst cannot masquerade as one ever-growing merged passage. Provisional, not evidence-derived. |

If `C`'s peak is **larger** than `P`'s merged peak (ratio > `AMPLITUDE_RATIO_MAX`), it does not merge — this is deliberate. The doctrine's own text flags "a smaller group of wider opposite-polarity companions [that] remains unresolved" as a *distinct*, not-yet-understood phenomenon; a comparable-or-larger second lobe is exactly that unresolved case and must not be silently swept into the same merge rule that the well-characterized (small, brief, asymmetric) companion population justifies. A candidate that fails the merge test is evaluated fully independently, subject to §3 and §5 on its own terms — which, being a genuine new excursion arriving well inside a normal inter-marker interval, will very likely fail §5's arrival gate on its own, which is the correct outcome for an unresolved case: neither silently merged away nor silently promoted to a new marker.

### 4.2 MergedPassage data contract

| Field | Definition |
|---|---|
| `primary_polarity` | Polarity of the lobe with the largest `\|signed_flux\|` among all merged lobes (ties broken toward the earliest lobe) |
| `open_t` / `close_t` | First lobe's `open_t` to last lobe's `close_t` |
| `merged_duration_ms` | `close_t − open_t` |
| `merged_peak` | `max(\|peak\|)` across all merged lobes — this is the running value `AMPLITUDE_RATIO_MAX` tests against for a third-or-later lobe |
| `primary_abs_flux` | `abs_flux` of the primary lobe alone — this is what §3.2's flux floor gates against; a merge does not launder a sub-floor primary into a passing one |
| `merged_total_abs_flux` | Sum of `abs_flux` across all merged lobes — diagnostic only |
| `pwm_*_at_open/peak/close` | From the **primary lobe only** (§2.3) |
| `lobe_count` | 1 if no companions merged |
| `companions[]` | Diagnostic-only list, one entry per merged companion: `{offset_ms_from_previous_lobe_close, polarity, peak_abs, abs_flux, duration_ms, amplitude_ratio}` |

**Invariant:** `companions[]` fields never participate in map validation (§6), the arrival gate's "previous accepted" reference (§5), or any hypothesis score (§7a). They exist for diagnostic/replay review only.

Merge eligibility is tested **before** an otherwise-sub-floor candidate is independently disposed `ARTIFACT` — a companion that would fail §3's floors on its own (as most of the doctrine's documented companions do, being sub-5ms) is still correctly attributed as "companion of event N" rather than logged as an unrelated, unexplained artifact. Companions are exempt from §3's duration/flux floors precisely because they are not being evaluated as standalone passages.

### 4.3 Interaction not independently verified

`MERGE_WINDOW_MS=350` was checked qualitatively against typical marker-to-marker intervals (~900–1,200ms per QUORUM's own comments and the doctrine text) and sits comfortably below them at cruise and low PWM. At the extreme high-PWM end of the physically-impossible bound (§5.2, evaluated at PWM255), a back-of-envelope check using QUORUM's own velocity model suggests the margin could narrow, depending on the real marker spacing (`spacingMm[]`, not read into this document — see §5.1). §11 flags this as an interaction to check once the map is actually pulled in at replay time, rather than asserting a margin this document has not verified.

---

## 5. Physical arrival gate

The doctrine requires "a conservative earliest physically possible arrival for a separate next marker," including "an absolute physical impossibility boundary." Finding 8 shows the one existing implementation's single global bound (evaluated at PWM255) is safe against over-rejection but "nearly toothless where field failures concentrate" (low-PWM/near-stall regimes). This design therefore uses **two** bounds, not one, with different jobs.

### 5.1 Map data

`spacing_mm(from_mm, direction)` and `next_mm(from_mm, direction)` are extracted programmatically from `firmware/QUORUM/QUORUM.ino`'s own map tables, exactly as `tools/quorum_map.py` already does for `hwt_gate_replay.py` — never hand-copied. The map is physical/infrastructure data (the real layout of magnets on the real track), not navigation policy, and is squarely inside the operator's carve-out for independently valuable, reusable interfaces.

### 5.2 The hard, absolute bound

```
HARD_IMPOSSIBLE_MS = 1000 * spacing_mm(prev.mm, direction) / velocity_mm_s(255)
velocity_mm_s(pwm) = VEL_MODEL_SLOPE * pwm + VEL_MODEL_INTERCEPT
```

`VEL_MODEL_SLOPE=3.90`, `VEL_MODEL_INTERCEPT=-99.2` are reused from QUORUM's own linear PWM→speed calibration. This reuse needs its own justification, separate from the general "infrastructure is reusable" carve-out, because the operator's explicit list (safety/manual-authority/MQTT/operational interfaces) does not name it: it is adopted here because QUORUM's own comment already frames it as a physical calibration curve, not a policy — *"PWM is a request, not a result... the wide tolerance exists to absorb the model error until [a wheel sensor] replaces this"* — and evaluating it at PWM255 (the vehicle's own theoretical maximum, regardless of what PWM the current candidate shows) is precisely what makes this bound a legitimate "physically impossible" backstop: a genuine event, even a fast one, should comfortably clear a bound set at the vehicle's absolute ceiling, so a failure here is a strong, near-unambiguous artifact signal. If the operator does not accept this categorization, `HARD_IMPOSSIBLE_MS` should instead be derived from an independently-measured top speed; the formula shape does not change, only the two constants would.

Any candidate (that did not merge, §4) arriving less than `HARD_IMPOSSIBLE_MS` after the previous **accepted** marker (`PositionRing`'s most recent entry) is unconditionally `ARTIFACT` / `ARTIFACT_HARD_IMPOSSIBLE` — regardless of any other evidence, including a matching map polarity.

### 5.3 The contextual, PWM-aware bound

```
pwm_for_gate = max(ctl_pwm_actual sampled over [prev.close_t, candidate.open_t], inclusive)

if pwm_for_gate ≥ PWM_MODEL_VALID_FLOOR:
    CONTEXTUAL_MIN_MS = 1000 * spacing_mm(prev.mm, direction) / velocity_mm_s(pwm_for_gate)
else:
    CONTEXTUAL_MIN_MS = HARD_IMPOSSIBLE_MS   # model invalid below this PWM — see §5.4
```

`PWM_MODEL_VALID_FLOOR = 40` (reused from QUORUM's `GATE_LOW_PWM_FLOOR`; a PWM-count threshold, **not** to be confused with §3.1's numerically-identical-but-unrelated `DURATION_FLOOR_MS=40`, a millisecond threshold — the two constants are named distinctly throughout this document to avoid exactly this confusion).

`pwm_for_gate` uses the **maximum** PWM observed anywhere in the interval since the previous accepted marker, not the candidate's own instantaneous PWM at open. This is the evidence-grounded choice for an *earliest-possible* bound: if the vehicle spent most of the interval at PWM60 but had one burst to PWM90, the interval's own evidence supports having covered the distance at the faster pace for at least part of it, so using the max avoids a false-early rejection driven by an instantaneous-only reading that ignores a real fast stretch within the same interval. This also means TEMPLATES needs **no separate RAMP case** the way QUORUM's `RAMP` gate branch does (§0's table) — a momentary ramp transition is already absorbed correctly by looking at the whole interval rather than one instant.

A candidate arriving between `HARD_IMPOSSIBLE_MS` and `CONTEXTUAL_MIN_MS` is `ARTIFACT` / `ARTIFACT_CONTEXTUAL_TOO_SOON` — distinct from `ARTIFACT_HARD_IMPOSSIBLE` so replay output can separate "unambiguous physical impossibility" from "merely too fast for the prevailing operating context" (§3.6).

### 5.4 Low-PWM regime: the gate is never skipped

This directly closes Findings 4 and 7: QUORUM's `GATE_LOW_PWM_FLOOR` branch calls `acceptEvent()` **unconditionally** when PWM is below the model's valid range — dropping the timing check entirely, exactly where the field evidence shows artifact rate is highest. This design never does that. Below `PWM_MODEL_VALID_FLOOR`, `CONTEXTUAL_MIN_MS` simply falls back to `HARD_IMPOSSIBLE_MS` (§5.3) — the tighter, model-derived bound becomes unavailable, but the absolute bound always applies. The gate is evaluated on every candidate, with no PWM regime that bypasses it.

### 5.5 No previous accepted marker (`NO_PREV`)

When `PositionRing` is empty (fresh declaration, boot, or a just-cleared ring — §6.4), there is no interval to measure `CONTEXTUAL_MIN_MS`/`HARD_IMPOSSIBLE_MS` against, so the arrival gate is inapplicable by definition. This is **not** an unconditional accept: the candidate still passes through §3 (artifact rejection) and §6 (map-polarity validation) in full — it is simply exempt from the one test that has no predecessor to compare against. Contrast with QUORUM's `NO_PREV` branch, which calls `acceptEvent()` (an unconditional advance) before any polarity check.

### 5.6 Reversal (Finding 6)

The doctrine's spacing-based bound assumes the next marker is a full marker-spacing away. Immediately after a direction reversal this is false — the physically "next" marker is the one the locomotive just left, at an unknown, possibly near-zero, sub-marker offset (how far past it the locomotive got before reversing is not observable from Hall data alone). This design combines both of Finding 6's proposed options rather than choosing one:

1. **First**, the candidate is tested against §4's merge-eligibility rule using the most-recently-closed passage as `P`, regardless of the normal `MERGE_WINDOW_MS` interaction with cruise timing (a reversal-adjacent candidate is exactly the case the merge rule exists for — the same magnet's field, re-traversed). If it merges, it is absorbed as a companion lobe, not a new marker — physically correct, since it likely is the same magnet.
2. **If it does not merge**, it is evaluated with **no timing floor at all** (neither `HARD_IMPOSSIBLE_MS` nor `CONTEXTUAL_MIN_MS` — both are suspended, since neither can be bounded without knowing the unobservable sub-marker offset) but **full** §3 artifact rejection and **full** §6 map-polarity validation still apply.

This applies to exactly the first credible candidate following a `direction` change (tracked as a single-shot flag, cleared after one candidate is evaluated by either path above) — it does not silently fall into an unconditional-accept bucket the way QUORUM's `fullRecoveryReset()` → `NO_PREV` chain does after `applyDirection()`.

### 5.7 Stall / dwell interval-reference invalidation

Finding 5: QUORUM's own fix for "a dwell then a restart tests the first marker against a stale interval" (`invalidatePreviousAcceptedDt()` on a nonzero→zero `actualPwm` edge) does not cover a stall where commanded/actual PWM is held at a small *nonzero* value throughout (the Grillers PWM40 stall) — the edge QUORUM watches for never fires.

This design's condition is broader by construction: any time §2.4's confirmed-stationary condition (`ctl_pwm_actual ≤ SUSPEND_PWM_FLOOR=20`) is entered and later exited, the "previous accepted marker" interval reference used for `CONTEXTUAL_MIN_MS` (§5.3) is invalidated for the next candidate only — that next candidate is evaluated as if under §5.5 (`NO_PREV`: full artifact/polarity checks, no timing-interval test). Because `SUSPEND_PWM_FLOOR` is a PWM-level threshold, not an edge-to-zero trigger, this fires correctly for a nonzero-low stall exactly like Grillers, not only for a full stop.

---

## 6. Map validation and the three-way disposition

### 6.1 Ordering

A candidate reaches this section only after: it did not merge as a companion (§4), it cleared §3's artifact rules, and it cleared §5's arrival gate. Call this a **credible passage**. Its `polarity` (or `primary_polarity` if merged) is compared against `expected_polarity = dna_at(next_mm(primary_position, primary_direction))` — read from the same map source as §5.1 — **before** any mutation of `primary_position`. There is no code path in this design that mutates `primary_position` and only afterward checks this comparison.

### 6.2 Two outcomes, structurally distinct rings

**Match → `EXPECTED_ADVANCE`.** `primary_position := next_mm(primary_position, primary_direction)`. Push `{polarity, mm, t_ms, event_id}` onto `PositionRing`. Clear the running contradiction streak (§6.3).

**Mismatch → `CREDIBLE_CONTRADICTION`** ("No Way"). `primary_position` is **not** changed. Nothing is pushed to `PositionRing`. Instead, construct a `ContradictionObservation` (below) and push it onto `ContradictionRing`.

```
INVARIANT: no code path ever pushes a ContradictionObservation into PositionRing,
and no code path ever pushes a PositionRing entry into ContradictionRing.
A single event produces at most one ring insertion, into exactly one of the
two rings, never both. A CREDIBLE_CONTRADICTION disposition produces zero
PositionRing insertions.
```

This is the direct resolution of Finding 11: QUORUM's single `evRing` is fed by `acceptEvent()` unconditionally, before polarity is compared, so a disagreement still gets full navigation standing. Here, the two rings are disjoint by construction — a contradiction physically cannot reach `PositionRing`.

### 6.3 ContradictionObservation contract

| Field | Definition |
|---|---|
| `observation_id` | Monotonic |
| `t_ms` | Candidate's `open_t` |
| `observed_polarity`, `peak_abs`, `duration_ms`, `abs_flux` | Waveform shape summary — from §2.7 (or §4.2's merged fields), copied here so recovery does not need to dereference the original candidate |
| `pwm_actual_at_open`, `pwm_at_peak`, `pwm_at_close`, `pwm_for_gate` | Motion/PWM context (§2.3, §5.3) |
| `primary_position_at_contradiction` | The map `mm` this observation was compared against — `primary_position` did **not** change, so this value doubles as "before" and "after" |
| `expected_polarity` | What the map said at that position |
| `direction_at_contradiction` | `primary_direction` at the time |
| `contradiction_streak_index` | 1 for the first contradiction since the last `EXPECTED_ADVANCE` or `RESYNC_ADOPTED`, incrementing thereafter; reset to 1 whenever either of those occurs |
| `rule_id`, `reasoning` | Free text (§9) |

### 6.4 Ring sizing and clearing

`PositionRing` and `ContradictionRing` are each sized `12` (`POSITION_RING_SIZE = CONTRADICTION_RING_SIZE = 12`), matching `MAX_OMITTED` (§7a) and `QUORUM_MAX` (justified fully in §8; not asserted here without that justification). Sizing both rings identically avoids ambiguity about "why is the ring bigger than the hypothesis window it feeds" for whoever implements this.

`ContradictionRing` is cleared on: (a) a direction reversal — readings collected in one direction cannot be scored against hypotheses framed in another; (b) an explicit operator position declaration; (c) a successful `RESYNC_ADOPTED` (§7a) — the incident is resolved. These are stated here as fresh, independently-reasoned design choices for this document, even though the pattern resembles QUORUM's own ring-clearing — clearing state on resolution is basic hygiene, not the acceptance-ordering/recovery-trigger policy under replacement.

---

## 7. Recovery

### 7a. Hypothesis testing (event-driven, on every credible contradiction)

This directly implements the operator's mechanism (a): *"When the next credible passage arrives, test hypotheses representing one or more omitted markers before deciding whether it is the expected target — event-driven, triggered by the next arrival, not by the silence itself."*

**Trigger.** Every single `CREDIBLE_CONTRADICTION` (§6.2) runs this procedure — not after a streak of N. This is a deliberate departure from QUORUM's `QUORUM_TRIGGER=3`-consecutive-disagreements pattern, made explicit here rather than left as an unstated inheritance, because a streak-gated trigger cannot fire on the very first contradiction after a single omission, delaying recovery for no evidentiary reason once the mechanism exists at all.

**Hypothesis set.** For the contradicting candidate `C` (already pushed to `ContradictionRing`, §6.3), test offsets `o ∈ {+1, +2, ..., +MAX_OMITTED}`, where offset `o` means "`o` markers were silently omitted since the last confirmed position, and `C` is actually the marker `o+1` positions ahead." `MAX_OMITTED = 12` (justified in §8).

**No negative offset.** QUORUM's candidate fence includes `-1` ("one phantom was wrongly accepted, correct by stepping back"). This design's fence has no analogue: because §6.1 makes map-polarity validation a strict precondition of any `PositionRing` push, `PositionRing` cannot contain a false advance by construction — there is no "phantom to retract" case to represent. The offset set here is `{+1..+12}` only.

**Scoring.** For each offset `o`, re-score every entry currently in `ContradictionRing` (plus `C` itself) against the hypothesis that the true position at the time each entry was recorded was `routeMod(entry.primary_position_at_contradiction + direction·o)` (i.e., score against the position *recorded with* each observation, not the current position — the same discipline QUORUM's own `scoreEntry()` uses, reused here as a sound general technique for testing a hypothesis against a moving reference frame, independent of QUORUM's specific, broken deployment of it):

```
score(o) = count of { r ∈ ContradictionRing ∪ {C} :
    r.observed_polarity == dna_at(next_mm(routeMod(r.primary_position_at_contradiction + direction·o), direction)) }
```

**Winner selection.** `leader = argmax score(o)`, `runner_up` = second highest.

```
WIN  :=  score(leader) − score(runner_up) ≥ MARGIN   AND   score(leader) ≥ MIN_SUPPORT
```

`MARGIN = 2` (reused from `QUORUM_MARGIN`, same "clearly ahead, not a coin flip" reasoning). `MIN_SUPPORT = 2` (new: requires at least two independent corroborating observations agreeing on the same offset before ever resyncing — a margin-only test could "win" on a single data point, e.g. `1-0`, which is exactly the false-lock risk Findings 12 and 13 warn about). **Both constants are marked provisional pending replay evidence** — Finding 13 is explicit that no capture in this repo currently has a confirmed absolute start position dense enough to measure this mechanism's true false-lock rate, so these numbers cannot be fully validated without the matched anchor-dense capture pair Finding 13 calls for.

**On WIN → `RESYNC_ADOPTED`.** `primary_position := next_mm(routeMod(primary_position_before + direction·leader), direction)` (i.e., `C`'s own position, under the winning hypothesis). Push `C` onto `PositionRing`. Clear `ContradictionRing`. Log a `RESYNC_ADOPTED` record with the full score vector and every contributing observation's `observation_id`, for auditability.

**On no win (tie, or no offset clears both tests) → stays `CREDIBLE_CONTRADICTION`.** No disposition is silently picked. `primary_position` is unchanged; `C` remains in `ContradictionRing` as already recorded (§6.2).

**If `ContradictionRing` fills (`CONTRADICTION_RING_SIZE=12` entries) without ever reaching WIN → `POSITION_UNRESOLVED`.** This is the terminal condition, and it is reached honestly: if the true omitted-marker count exceeds `MAX_OMITTED`, no offset in `{+1..+12}` will ever accumulate consistent corroborating evidence, so the ring fills without a winner — which is the *correct* behavior (no confident adoption of a nearby-but-wrong offset), not a gap. On `POSITION_UNRESOLVED`: request a safe stop through the existing, unmodified AUTO-chamber motor-authority interface (the same bicameral rule QUORUM already implements — navigation requests, it does not seize authority in MANUAL — is deferred to unchanged); publish the full `ContradictionRing` contents, `PositionRing`'s recent history, and every offset's final score, for an operator declaration. This reuses the *idea* of QUORUM's forensic `NO_QUORUM` snapshot (publish full context before stopping), not any of its specific recovery internals.

### 7b. Travel-without-landmarks liveness (independent of 7a)

This directly implements the operator's mechanism (b) and closes Finding 14: a run of pure omissions — credible passages simply never arriving, no disagreement, nothing wrong-polarity — produces neither an `EXPECTED_ADVANCE` nor a `CREDIBLE_CONTRADICTION`. §7a never fires on silence; it is event-driven off a new *credible* arrival, and if nothing credible arrives, there is no event to trigger it. This requires a wholly separate, time/distance-based mechanism.

**Trigger.** While commanded/measured movement continues (`ctl_pwm_actual > SUSPEND_PWM_FLOOR=20` — gated off during a confirmed-stationary dwell, since no new markers are expected while genuinely not moving; §1.3's own data shows genuine dwells run from tens of seconds to over half an hour, and this check must not fire during any of them), track `elapsed_ms_since_last_position_ring_push` (time since the most recent `EXPECTED_ADVANCE` or `RESYNC_ADOPTED`). Compare against `N_LIVENESS` expected-marker-intervals' worth of elapsed time, using §5.3's own `CONTEXTUAL_MIN_MS` model (`pwm_for_gate` computed the same way, over the elapsed window) as the per-interval yardstick.

`N_LIVENESS = 3`. Chosen independently of `QUORUM_TRIGGER` (which also happens to be 3, for an unrelated reason — disagreement-streak length, not interval-count) on its own reasoning: comfortably longer than any single interval's ordinary variance (avoiding a spurious trigger from one slow patch or one narrowly-missed marker), while not waiting indefinitely to raise a genuinely broken-channel warning. **Provisional pending replay evidence.**

**Effect — two stages, not a single binary:**

- **Stage 1** (`N_LIVENESS` intervals elapsed, zero credible arrivals of any kind): `LIVENESS_WARNING`. Publish a diagnostic event and reduce a published position-confidence indicator. Does **not** change `primary_position`. Does **not** request a stop.
- **Stage 2** (`2 × N_LIVENESS` intervals elapsed, still nothing): `LIVENESS_TIMEOUT`. Request a safe stop through the same existing motor-authority interface as §7a's `POSITION_UNRESOLVED`, and publish the same kind of forensic context (elapsed time, PWM history summary, last confirmed position/time).

**Explicit confirmation, stated exactly because the task requires it stated exactly:** *this mechanism never writes to `primary_position`. It has no code path that calls `next_mm()`, `routeMod()`, or any `PositionRing`/`ContradictionRing` push. Its only two effects are (1) publishing a confidence/diagnostic signal and (2) requesting a stop through the existing motor-authority interface. Elapsed time and PWM history are used only to decide *when* to raise the alarm, never to decide *where* the vehicle is. There is no dead-reckoning position estimate anywhere in this design.*

**Why long dwells do not consume this budget.** A genuinely long, confirmed-stationary dwell does not itself create omission risk under this design: while stationary, the vehicle is not passing new magnets to miss, and this mechanism is explicitly gated off during confirmed-stationary periods (same condition as §2.4). The only thing §7b watches for is elapsed *travel* with nothing credible — a materially rarer and different failure than a station stop, however long.

### 7c. What replaced each named QUORUM mechanism (cross-reference to §0)

| §0 item | Where handled here |
|---|---|
| Acceptance ordering | §6.1 |
| Recovery triggers | §7a (event-driven), §7b (time/distance-driven) |
| Candidate fence | §7a, `{+1..+12}`, no negative offset |
| Evidence-ring semantics | §6.2–§6.4, two disjoint rings |
| Quarantine behavior | Eliminated — see below |
| Advance-before-validation | §6.1 |

QUORUM's `adoptionPendingValidation` / `candidateExcluded[]` / `handleFailedAdoption()` machinery exists to undo a **provisional** adoption made before it could be validated against a few more markers. This design has no provisional-adoption state: `RESYNC_ADOPTED` only ever fires after `WIN` (§7a), which already requires `MIN_SUPPORT=2` independent corroborating observations — there is nothing adopted "pending validation" that could later need to be invalidated and excluded. The elimination is total, not a redesign of an equivalent.

---

## 8. Acceptance criteria

### 8.1 False inclusion: zero, as a count, not a rate

No candidate independently identifiable as belonging to a known-artifact class — `duration_ms < DURATION_FLOOR_MS`, OR `abs_flux < FLUX_FLOOR`, OR occurring while §2.4's suspend condition should have blocked it from opening at all, OR failing §5.2's `HARD_IMPOSSIBLE_MS` bound — may reach `EXPECTED_ADVANCE` or `RESYNC_ADOPTED`. **A single confirmed instance, across the full replayed Toby + Otto(CCW/CW/change1) corpus, fails this design.** This is not averaged into a rate, per Finding 18's proposal, which this document adopts.

This document validates the claim directly against its own proposed thresholds, not only against the doctrine's prior numbers: Toby's clean trace produced exactly **one** sub-floor candidate (35ms duration, 582 count·ms flux) out of 1,190 total opened excursions — and that single candidate's flux (582) is comfortably below this document's own `FLUX_FLOOR=900` as well as `DURATION_FLOOR_MS=40`, so it would be correctly disposed `ARTIFACT` under the exact rules proposed here. Under these specific thresholds, Toby's own clean data replays to **zero** surviving artifacts, not merely "near zero" — a stronger, directly-checked claim.

### 8.2 Omission-recovery bound: 12, inherited with justification, flagged for revalidation

`MAX_OMITTED = 12`, matching `QUORUM_MAX`. Adopted as the starting bound for the same structural reason Finding 15 already credits it: it is the one existing number in this codebase describing how much ambiguity a recovery architecture here has been sized to absorb, and reusing it keeps `PositionRing`/`ContradictionRing` sizing, §7a's hypothesis window, and this bound all mutually consistent (§6.4).

Finding 15's own critique is **not** resolved by this reuse and is restated as a required follow-up, not silently inherited: `QUORUM_MAX=12` was sized against a *less* conservative gate than the one this document proposes, and was never checked against the actual longest run of consecutive expected-but-unadmitted credible passages under a gate this strict. This document could not perform that check itself — it requires a full map-and-position replay (tracking `primary_position` across an entire capture against operator anchors), which is implementation-and-replay-stage work, not design-stage work. **The exact validation task for that stage:** replay the complete pipeline against Toby and Otto's traces, measure the longest actual run of consecutive credible-but-ultimately-real omitted markers, and increase `MAX_OMITTED` (and both ring sizes, and §7a's hypothesis set) to exceed that measured maximum with a stated margin if it exceeds 12, exactly as Finding 15 proposes.

**Dwells do not consume this budget** — see §7b's closing note. The omission-run concern this bound protects against is specifically in-motion classifier failure (several real passages in a row producing no credible candidate despite genuine motion), not a station stop or a stall, both of which are handled by the separate, dwell-aware §7b mechanism.

---

## 9. Output data schema

One row per `RawCandidate` (including merged companions and reversal-adjacent candidates), covering every disposition this design can produce.

| Column | Type | Notes |
|---|---|---|
| `event_id`, `capture`, `session` | id | |
| `candidate_open_t_ms`, `candidate_close_t_ms` | int | |
| `evidence_duration_ms`, `evidence_peak`, `evidence_polarity`, `evidence_abs_flux`, `evidence_signed_flux` | numeric/enum | §2.7 / §4.2 |
| `evidence_pwm_actual_at_open`, `..._commanded_at_open`, `..._at_peak`, `..._at_close`, `evidence_pwm_for_gate`, `evidence_direction` | numeric/enum | §2.3, §5.3 |
| `evidence_lobe_count`, `evidence_companions_json` | int / text | §4.2; blank/`1` if unmerged |
| `evidence_spans_pwm_transition` | bool | §2.3, diagnostic only |
| `evidence_boundary_ambiguous` | bool | §3.1, `35–50ms` window, diagnostic only |
| `map_position_before`, `map_direction_before`, `map_expected_polarity` | | |
| `map_position_after` | | equals `before` unless `EXPECTED_ADVANCE` or `RESYNC_ADOPTED` |
| `disposition` | enum | `ARTIFACT \| MERGED_COMPANION \| EXPECTED_ADVANCE \| CREDIBLE_CONTRADICTION \| RESYNC_ADOPTED \| POSITION_UNRESOLVED` |
| `disposition_rule_id` | text | e.g. `ARTIFACT_DURATION_FLOOR`, `ARTIFACT_FLUX_FLOOR`, `ARTIFACT_EXCESSIVE_OPEN`, `ARTIFACT_INCOMPLETE`, `ARTIFACT_SATURATED`, `ARTIFACT_HARD_IMPOSSIBLE`, `ARTIFACT_CONTEXTUAL_TOO_SOON`, `MERGED_INTO=<event_id>`, `MAP_MATCH`, `MAP_MISMATCH`, `RESYNC_OFFSET=<n>`, `LIVENESS_WARNING`, `LIVENESS_TIMEOUT` |
| `disposition_reasoning` | free text | human-readable, with the exact numbers that drove the decision (e.g. `"duration=147ms >= 40ms floor; flux=16333 >= 900 floor; pwm_for_gate=88 -> contextual_min_ms=612ms; elapsed=812ms -> PASS"`) |
| `contradiction_streak_index` | int | blank unless `CREDIBLE_CONTRADICTION`/`RESYNC_ADOPTED`; §6.3 |
| `resync_offset`, `resync_score_vector` | int / text | blank unless a §7a hypothesis test ran |
| `position_ring_inserted`, `contradiction_ring_inserted` | bool | explicit, so §6.2's invariant is programmatically checkable: at most one true, and it must match `disposition` |

---

## 10. Consolidated constants table

| Constant | Value | Units | Status |
|---|---|---|---|
| `ENTRY_COUNTS` | 38 | ADC counts | Validated by direct replay |
| `EXIT_COUNTS` | 25 | ADC counts | Validated by direct replay |
| `EXIT_HOLD_MS` | 20 | ms | Reused from QUORUM, not re-derived |
| `SUSPEND_PWM_FLOOR` | 20 | PWM | Reused from `MOTOR_DEAD_ZONE_PWM`, architecturally justified |
| `MAX_OPEN_MS` | 3000 | ms | Derived from replay (2,193ms clean max × margin) |
| `RAMP_DELTA` | 10 | PWM | Reused, diagnostic-only here |
| `DURATION_FLOOR_MS` | 40 | ms | Validated by direct replay; boundary-ambiguous near this value |
| `FLUX_FLOOR` | 900 | count·ms | Derived from replay; **tight margin, highest revalidation priority** |
| `MERGE_WINDOW_MS` | 350 | ms | Inherited from doctrine's descriptive figure; provisional |
| `AMPLITUDE_RATIO_MAX` | 0.5 | ratio | Derived with ~2× headroom over doctrine's companion ratio; provisional |
| `MAX_LOBES` | 5 | count | Defensive, not evidence-derived |
| `PWM_MODEL_VALID_FLOOR` | 40 | PWM | Reused from `GATE_LOW_PWM_FLOOR` (distinct from `DURATION_FLOOR_MS` despite equal value) |
| `VEL_MODEL_SLOPE` / `VEL_MODEL_INTERCEPT` | 3.90 / −99.2 | mm/s per PWM / mm/s | Reused from QUORUM, explicitly provisional there too |
| `MAX_OMITTED` | 12 | count | Inherited from `QUORUM_MAX` with justification; flagged for revalidation |
| `POSITION_RING_SIZE` / `CONTRADICTION_RING_SIZE` | 12 / 12 | count | Matched to `MAX_OMITTED` |
| `MARGIN` | 2 | score points | Reused from `QUORUM_MARGIN`; provisional |
| `MIN_SUPPORT` | 2 | count | New; provisional pending anchor-dense validation |
| `N_LIVENESS` | 3 | intervals | New, independently reasoned; provisional |
| False-inclusion tolerance | 0 | count (not rate) | Validated directly against this document's own thresholds on Toby |

---

## 11. Explicitly deferred / open questions

These are the specific places this document could not derive a number from evidence in hand and marked it provisional rather than inventing false precision, plus the concrete task each one implies for the next (implementation/replay) stage:

1. **`FLUX_FLOOR=900`** — the tightest, least-comfortable margin in this document (§3.2). Highest-priority target for revalidation once real per-event anchor ground truth is available.
2. **Whether Toby's own 1,211 count·ms minimum-observed "genuine" event is really a magnet passage** or an edge/partial-capture artifact — this single data point currently defines one edge of `FLUX_FLOOR`'s justification and cannot be resolved without anchor-level ground truth.
3. **`AMPLITUDE_RATIO_MAX=0.5`** (§4.1) — not checked per-pair against the doctrine's 205-pair Otto CW companion population; Finding 9 explicitly calls for that per-pair replay before a threshold is finalized.
4. **`MAX_OMITTED=12` / ring sizes** (§8.2) — requires a full map-and-position replay to measure the actual longest consecutive-omission run under this document's specific (stricter than QUORUM's) gate; not performable without implementing §2–§6 first.
5. **`MARGIN=2` / `MIN_SUPPORT=2`** (§7a) — Finding 13 is explicit that no capture in this repo currently has a confirmed absolute start dense enough to measure the resync mechanism's true false-lock rate. Needs the matched anchor-dense capture pair (deliberate single omission + deliberate PWM-varying no-omission control) Finding 13 proposes.
6. **`N_LIVENESS=3`** (§7b) — reasoned from first principles, not measured against a real broken-channel event, because none of the four captures contains one.
7. **Station `finalPwm` vs. `SUSPEND_PWM_FLOOR=20`** (§2.4) — whether any station's configured final-approach speed dips at or below the suspend threshold while a genuine last-marker passage is still expected. This document does not have the `STATIONS[]` table's values in evidence.
8. **`MERGE_WINDOW_MS` vs. the arrival gate at extreme high PWM** (§4.3) — a qualitative check only; needs the real `spacingMm[]` values (via `quorum_map.py`) to confirm quantitatively.
9. **`ADC_MIN_RAIL`/`ADC_MAX_RAIL`** for §3.5's saturation rule — not independently confirmed against the firmware's ADC configuration.
10. **All PWM-dependent constants above PWM130** — this document's own replay extended validated coverage from the prior review's PWM90 ceiling to PWM120 (real data, Otto CW / Otto change1), a material improvement, but nothing here is validated up to QUORUM's own PWM255 reference point. Any candidate arriving with `pwm_for_gate` in `(130, 255]` should be tagged with a diagnostic `pwm_regime_unvalidated=true` at replay time rather than silently trusted at the same confidence as the validated range.

---

## 12. Sources

- `TEMPLATES/REVIEW of HALL SENSOR LOGIC/MAGNET_ADMISSION_DOCTRINE.md`, `TARGET_ACQUISITION_DECISION_LOGIC.md`, `REVIEW_NOTES.md`
- Prior adversarial review of the doctrine (18 findings; Findings 1, 2, 4–15, 18 are cited by number above at the point each is used)
- `firmware/QUORUM/QUORUM.ino` — read for reference only, per this task's hard constraints; specific mechanisms cited: `detectorSample()`/`updateBaseline()` (~L620–761), the §3 timing gate and `navOnMarker()` (~L1768–1879), `acceptEvent()`/`pushRing()`/`scoreEntry()`/recovery state (~L1339–1685), `applyDirection()` (~L2024–2062), `servicePwmRamp()` (~L2101–2126), station/dwell handling (~L2171–2330), and the named constants collected in §10
- `tools/qt_decode.py` (capture→CSV decoder) and `tools/hwt_gate_replay.py` (QUORUM-era prior art, read for style/pattern precedent only — its `passes_morphology()` dual-floor pattern and `EVENT_COLUMNS` field-grouping convention are structurally echoed in §3.2 and §9, with entirely new field semantics)
- Direct, independent single-pass replay against `toby_ccw_20260824.csv`, `otto_ccw_20260824.csv`, `otto_cw_20260824.csv`, and `otto_change1_20260825.csv`, reading only `phys_raw`/`phys_baseline`/`ctl_pwm_actual`/`ctl_pwm_commanded` — method described in full in §1.3, so it is reproducible without depending on any particular tool file persisting
