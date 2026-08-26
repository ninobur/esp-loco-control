# TEMPLATES Adversarial Offline Replay — Results Report

Status: investigatory / design evidence only. Not firmware. Nothing under `firmware/` was modified or proposed for modification. No hardware was touched. The files this exercise produced were later preserved in a dedicated investigatory-evidence commit (see **Files produced** at the end for the exact list and status); no `firmware/` file, no hardware, and no unrelated in-progress work was touched by that commit either.

**Verdict, stated plainly up front:** `tools/templates_replay_spec.py` is the raw-SAMPLE experimental candidate this report evaluates (reads only `phys_*`/`ctl_*` measurement fields). `tools/templates_replay.py` is Sam's separate, independently-built QUORUM-event-summary baseline (reads QUORUM's own `dec_*` decision output) — a different design, not a variant of this one. **Neither tool is firmware-approved or canonical.** The raw-sample candidate evaluated here is **rejected as currently tuned**, pending correction of two confirmed structural defects: contradiction-ring accumulation (§5) and same-polarity omission masking (§2/§9) — see §12 for the full verdict against the operator's stated criteria. Everywhere this report states "zero false inclusions," that refers only to the tool's own `false_inclusion_audit`/`ring_contamination_audit` post-hoc invariant checks (does a rejected candidate ever reach an advancing disposition) — it is **not** a claim of absolute coordinate integrity; §3's anchor-drift findings and §2's omission-masking findings show the tracked position can still be wrong even when that audit reports clean.

Scope: offline replay of the TEMPLATES three-way admission taxonomy (Artifact / Expected credible passage / Credible contradiction) against four real QUORUM TRACE captures (Toby CCW, Otto CCW pre-rewiring, Otto CW pre-rewiring "restarted boot"/worst contamination, Otto change1 post-rewiring) plus 16 targeted adversarial cases. QUORUM (`firmware/QUORUM/QUORUM.ino`) is used throughout only as a failed-baseline comparison and evidence source, never as a standard to reproduce.

## 0. Tooling caveat — read this before the numbers below

Two, not one, implementations exist in the tree under confusingly similar names, and the replay work used both at different points. This materially affects which results below count as evidence about the actual TEMPLATES candidate.

- **`tools/templates_replay.py`** (264 lines, tracked/committed) — an earlier, two-way accept/reject design. Its `extract_events()` reads `t_ms`/`duration_ms`/`peak`/`polarity` off QUORUM's own **DECISION** records (`EVENT_CLOSED`/`AGREE`/`DISAGREE`), i.e. off QUORUM's own detector-and-hysteresis interpretation of the waveform, not raw samples. This is exactly the circular input path the operator's brief forbids ("det_*/dec_* columns... never as input to the new classifier's logic, or the exercise is circular"). It also implements no three-way taxonomy, no MergedPassage contract, no §7b liveness mechanism.
- **`tools/templates_replay_spec.py`** (1,280 lines, untracked, new in this exercise) — implements `docs/TEMPLATES_REPLAY_DESIGN_SPEC.md` end to end: reads only `phys_raw`, `phys_baseline`, `ctl_pwm_actual`, `ctl_pwm_commanded`, `ctl_dir`, `t_ms`, `session` from SAMPLE rows, plus `op_*` fields from ANCHOR rows for drift-reporting only (never fed to the classifier); GAP/SESSION rows are used structurally; DECISION/STATUS rows are never opened for classifier input anywhere in the file. Implements the full three-way taxonomy, the disjoint PositionRing/ContradictionRing pair, §7a event-driven omission recovery, and §7b silence-driven liveness. **This is the compliant candidate; `templates_replay.py` is not**, despite the task's tool-path assignment and the design spec's own CLI examples naming it `templates_replay.py`.

Of the replay work performed:
- **Compliant-tool runs** (evidence that counts toward the verdict): Otto CW worst-contamination full session, Otto change1 Run 1 (3E6A88F1), Otto change1 Run 2 (103B1969), Toby full session (via three independent cluster runs that converge on identical disposition counts), Otto CCW full session (via the liveness cluster), adversarial cases 1–3 (spike adjacency/merge), 7–16 (all omission, wrong-start, boot, reversal, dwell, baseline-shift, gap, liveness, pulse-storm cases).
- **Non-compliant-tool runs** (informative about design risk, but NOT evidence about the actual candidate — reported separately and excluded from the verdict): the first full-session Toby run and the first full-session Otto CCW run (both item 1/2 below), and adversarial cases 4–6 (opposite/same-polarity companion danger case, credible-contradiction demonstration), which were run against `templates_replay.py` before the compliant tool existed or by mistake. These are flagged inline everywhere they appear below.

A human needs to reconcile the naming collision (rename or deprecate `tools/templates_replay.py`) before either tool is invoked again by path name alone.

---

## 1. False inclusions

**Compliant tool (`templates_replay_spec.py`), audited via its own `false_inclusion_audit` post-hoc invariant check** (does any candidate that failed the artifact/duration/flux/timing gate ever reach `EXPECTED_ADVANCE` or `RESYNC_ADOPTED`) across every session tested:

| Session | Candidates | `false_inclusion_audit.count` |
|---|---|---|
| Toby CCW (D7651658) | 1,190 | 0 |
| Otto CCW (A3201FFF) | 1,121 | 0 |
| Otto CW worst-contamination (77943FAD) | 1,626 | 0 |
| Otto change1 Run 1 (3E6A88F1) | 239 | 0 |
| Otto change1 Run 2 (103B1969) | 119 | 0 |
| **Total** | **4,295** | **0** |

Zero confirmed false inclusions across the compliant candidate's entire tested corpus, including the worst-contamination file. Case-level confirmation: the isolated-spike and companion-merge adversarial cases (1–3, real Otto CW data, raw-sample-verified) show the artifact gate and the forward-only merge mechanism correctly rejecting/absorbing marginal excursions without ever letting them touch `EXPECTED_ADVANCE`; the baseline-shift case (13) shows a genuine raw/baseline divergence during a stall correctly force-closed at the `MAX_OPEN_MS=3000` ceiling as `ARTIFACT_EXCESSIVE_OPEN` rather than accepted.

**Non-compliant tool** (`templates_replay.py`, excluded from verdict but reported as a design-risk signal): one confirmed false inclusion on Otto CCW — event t=249,030, `duration_ms=65535` (QUORUM's own uint16 saturation ceiling, i.e. a physically impossible ~65s "passage"), `peak=223`, `pwm_actual=0` (locomotive stationary), admitted as `EXPECTED_ADVANCE` (mm 41→40). This passed because that tool's duration/peak floors have no upper bound and no motion-plausibility check. The compliant tool's `MAX_OPEN_MS=3000` ceiling and its 0-violation audit result on the same physical data indicate this specific failure mode is closed in the actual candidate, but this was never independently re-run against the identical saturated-duration event under the compliant tool to confirm by direct reproduction — flagged as an inference, not a re-tested fact.

A second, unconfirmed risk: the same-polarity companion "danger case" (adversarial case 5) was demonstrated reaching `EXPECTED_ADVANCE` only under the non-compliant tool, using a hand-built synthetic event pair (65-count/90ms companion, same polarity, arriving 400ms after a 200-count/150ms primary — just outside that tool's 350ms merge window, landing on a real 6-consecutive-same-polarity map stretch, positions 36–41, verified against QUORUM's own map DNA). The compliant tool shares a conceptually similar `MERGE_WINDOW_MS`/`AMPLITUDE_RATIO_MAX=0.5` construct; this exact scenario was never re-run against `templates_replay_spec.py`. Not a confirmed defect in the candidate — an open, plausible risk carried over from the non-compliant tool's demonstration.

## 2. Deliberate omissions

Tested on the compliant tool against real Toby waveform data with real SAMPLE rows deleted by `t_ms` window (no synthetic events fabricated — a genuine passage's own detected window, and in one case an additional near-duplicate twin, physically removed from a real capture).

**Load-bearing negative result:** 1, 2, and 3 consecutive omissions were **silently and completely absorbed** in 3 of 4 tested counts whenever a same-polarity feature happened to sit in the gap — zero `CREDIBLE_CONTRADICTION`, zero recovery event, zero liveness warning, and both of the tool's own post-hoc audits (`false_inclusion_audit`, `ring_contamination_audit`) reported clean (count=0) despite a real, reproducible, undetected position error:

- **1-marker omission, attempt A** (deleting only the detected open/close window): failed to even construct a clean omission — a previously-unflagged same-polarity twin 1,363ms later (already rejected by the baseline run as `ARTIFACT_CONTEXTUAL_TOO_SOON`) cleared the arrival gate once the primary was gone and matched expected polarity → `EXPECTED_ADVANCE`, landing on the numerically correct position with zero diagnostic trace.
- **1-marker omission, attempt B** (widened deletion removing the twin too — a genuine complete omission): correctly produced 2 `CREDIBLE_CONTRADICTION` observations then `LIVENESS_TIMEOUT` at essentially the exact 2×N_LIVENESS boundary (7505.95ms threshold vs. 7506ms observed).
- **2-consecutive omission**: real markers "25"(N)/"24"(N) deleted, marker "26"(S) left intact — the next real candidate (true marker 23, also N) false-matched against the stale expected polarity → silent `EXPECTED_ADVANCE`, final position off by 2 (25 vs. true 23). Zero diagnostics.
- **3-consecutive omission**: same true-target marker 23, same silent 2-marker error via a different false pathway (an S companion first false-matched, then the marker-23 candidate false-matched). Zero diagnostics.
- **4-consecutive omission**: no same-polarity rescue available this time — correctly produced 2 `CREDIBLE_CONTRADICTION` observations then `LIVENESS_TIMEOUT` before the next real candidate even arrived; froze at the last confirmed marker; every subsequent real candidate correctly reported `POSITION_UNRESOLVED`/ignored rather than guessing.
- **Positive control** (real, undisturbed 2-marker omission naturally present in the baseline capture, markers 29/28 never independently detected as raw candidates): `RESYNC_ADOPTED` fired after exactly 2 `CREDIBLE_CONTRADICTION` observations (`MIN_SUPPORT=2`), offset=+2, latency_ms=**2,089**, landing on the physically correct marker 27.

Conclusion: the mechanism-(a)/(b) split (event-driven hypothesis testing vs. silence-driven liveness) is implemented and both mechanisms independently fire correctly — but the sole discriminating signal for mechanism (a) is a 1-bit polarity comparison, and same-polarity coincidence defeats it on 3 of 4 tested real-data omission counts on Toby's *clean control* capture. This is prior-review Finding #10 ("same-polarity companions... the doctrine's own designed backstop provides no protection here") made concrete on real data, and it is not rare or contrived: it reproduced on the majority of tested cases using the cleanest capture available.

## 3. Primary-position drift at every usable operator anchor

The compliant tool has **no code path that compares its own tracked position against `manifest.anchors[].asserted_mm`** — `anchor_drift_report()` doesn't echo `asserted_mm` into its own output, and `Manifest.start_confirmed` is parsed but never read again. Every number below was computed by manually cross-referencing the tool's reported position at (or immediately before) each anchor's `t_ms` against the anchor's own asserted value; where the run had already terminal-stopped before an anchor, the reported number is the frozen last-known value, not a live measurement, and is labeled as such.

**Otto change1 Run 1 (3E6A88F1) — the only fully anchor-bracketed run available (3 anchors, all usable):**

| Anchor | t_ms | Declared/expected | Candidate position | Drift | Note |
|---|---|---|---|---|---|
| 1 (start) | 131,211 | mm=40 | 40 (CW) | 0 | Seeded from manifest, not independent evidence |
| 2 (reversal) | 286,733 | no asserted mm; direction flips CW→CCW | 52, direction correctly flipped | n/a | Live reversal detected off `ctl_dir` at t=284,085, ~2.6s before the operator's typed anchor text |
| 3 (end) | 385,613 | mm=40/41 | 42 (frozen at t=313,548, terminal stop) | +2 | Last live value before terminal `CONTRADICTION_RING_FULL_NO_WIN`; NOT a live tracked position at the anchor — ~72s of the run past the stop were never re-evaluated |

**Otto CW worst-contamination (77943FAD):** only 1 anchor lands inside the scoreable range — declared start (t=270,762, "POSTREBOOT START WAS 040-041") reads position=40 by construction (seeded). Anchor #1 (t=190,110) precedes the declared start and correctly reports a null position (no scoreable position yet). No anchor exists at or after the freeze point (t=313,791), so numeric drift cannot be measured past mm=54 for this session.

**Toby CCW (D7651658):** 5 anchors exist; only #2 (t=162,224, start-usable) and #5 (t=2,838,170, end) carry a usable asserted value, and both carry an inherent ±1-marker labeling ambiguity ("040-041", "036-037") the manifest itself flags as unresolved. Under the compliant tool's authoritative full run, the terminal stop at t=575,778 freezes position at mm=7 for the remaining 93% of the session — anchor #5 at t=2,838,170 falls 2.26M ms *after* that freeze, so its nominal drift (|7−36|=29 or |7−37|=30 markers) is an artifact of the early terminal stop, not evidence of degraded live tracking accuracy. No live-tracking drift number is available for anchor #5 under the compliant tool because the tool stopped updating position long before reaching it.

**Otto change1 Run 2 (103B1969):** zero ANCHOR rows exist in this session (confirmed by direct scan) — no anchor-based drift is measurable at all; ground truth is only the operator's qualitative statement (started ~040-041, began in disagreement, aborted by INA219 failure), which the run's own opening 5 `CREDIBLE_CONTRADICTION` observations (all observed=N vs. expected=S, t=85,123–88,511) independently corroborate qualitatively.

**Otto CCW (A3201FFF):** the compliant-tool run's anchor-drift numbers were not resolved by any of the replay work performed — the session has 9 anchors per its manifest, but no cluster produced a compliant-tool anchor-by-anchor position table for this file. This is a genuine gap in the evidence base, reported honestly rather than filled with the non-compliant tool's (inadmissible) numbers.

**Net finding:** only one run (otto_change1 Run 1) has a fully bracketed, low-ambiguity anchor set, and even there the end anchor's "drift" is a frozen last-known value across a stop that occurred 72 seconds early, not a live measurement. Prior-review Finding #13 ("no capture in the repo can currently prove the offset-recovery mechanism resyncs to the *correct* absolute marker") stands, confirmed rather than resolved by this exercise.

## 4. Recovery latency after omission

Across all compliant-tool runs, `RESYNC_ADOPTED` fired **exactly once**:

- Toby positive control (natural 2-marker omission, markers 29/28): 2 `CREDIBLE_CONTRADICTION` observations, latency_ms=**2,089**, adopted offset=+2, landed on the physically correct marker (27).

Every other compliant-tool contradiction episode that reached the ring's 12-entry cap ended in an **unresolved tie**, not an adoption, i.e. recovery latency is undefined/infinite for those episodes:

- Otto CW worst contamination: two omission-offset hypotheses tied at 9/12, margin=0 (required margin=2) → `CONTRADICTION_RING_FULL_NO_WIN`.
- Otto change1 Run 1: offsets 10 and 11 tied at 11/12, margin=0 → `CONTRADICTION_RING_FULL_NO_WIN`.
- Otto change1 Run 2: ring capped with no offset in {+1..+12} reaching margin≥2/support≥2 → `CONTRADICTION_RING_FULL_NO_WIN`.

`recovery_latency_ms_summary.count=0` in both otto_change1 reports, confirming no second successful adoption anywhere in that corpus. With n=1 successful adoption total across the whole tested corpus, the MARGIN=2/MIN_SUPPORT=2 constants (inherited from QUORUM's own `QUORUM_MARGIN`/`QUORUM_MAX`, confirmed against QUORUM's own onboard STATUS counter `quorum(trigger=3 margin=2 max=12 cand=6)`) remain statistically unvalidated — consistent with prior-review Finding #15's flag that the recovery window was never checked against Otto's own measured omission-streak distribution.

## 5. Contamination of hypothesis evidence

The most damaging structural finding of this exercise, independently surfaced by three separate clusters against the compliant tool: **the ContradictionRing is never cleared by an intervening successful `EXPECTED_ADVANCE`** — only by a `RESYNC_ADOPTED` win, a live direction reversal, or a session boundary. This means temporally unrelated, individually-harmless mismatches accumulate silently across long stretches of otherwise-correct navigation until the 12-slot cap forces a terminal stop, even though primary-position tracking was correct the entire time.

- **Toby CCW**, the clean control: the terminal ring snapshot at t=575,778 shows 12 mostly-isolated `CREDIBLE_CONTRADICTION` observations (11 of 12 with `contradiction_streak_index=1`, i.e. not part of any consecutive streak) accumulated across ~133 seconds of otherwise-successful travel, with 12 confirmed `EXPECTED_ADVANCE` markers interleaved between them in the same window. Unrelated noise from far apart in time voted together until the cap was hit, then the run gave up for good — 9.6 minutes into a 47-minute clean-control run, discarding the remaining 93% of the session.
- **Otto CW worst contamination**, pulse-storm sub-case (adversarial case 16): a recurring ~150–190ms wrong-polarity "echo pulse" near almost every marker in the first 43 seconds (PWM steady at 90) never produced a single false inclusion — 13/13 genuine crossings correctly became `EXPECTED_ADVANCE`, the echo was mostly caught by `ARTIFACT_CONTEXTUAL_TOO_SOON`, and 6 slip-throughs were correctly labeled `CREDIBLE_CONTRADICTION` — but those 6, filling the ring alongside earlier entries, are the direct, traceable cause of the t=313,791 terminal stop. A different physical mechanism than Toby's (a real recurring artifact, not scattered unrelated noise) but the same structural consequence: correct primary tracking, forced stop anyway.
- **Otto change1 Run 2**: six separate contradiction episodes (sizes 5,3,1,1,1,1), each individually resolved by a subsequent matching passage, accumulated in the same ring until the sixth episode's single entry hit the 12-slot cap — a code comment at the `CONTRADICTION_RING_FULL_NO_WIN` site says "should be unreachable, since is_stopped short-circuits," suggesting this accumulation-via-several-small-resolved-episodes path was not anticipated by the implementation.

The tool's own `ring_contamination_audit` (checks only that `position_ring_inserted`/`contradiction_ring_inserted` are never both true or both false against disposition) reported **0 violations in every session tested** — but that audit checks bookkeeping self-consistency, not whether ring contents are causally related. A design that passes its own audits is still, by this finding, silently over-triggering stops on data where nothing was actually wrong.

## 6. Incorrect adoptions

Zero incorrect (wrong-marker) adoptions were observed. `RESYNC_ADOPTED` fired exactly once across the entire compliant-tool corpus (§4 above) and landed on the physically correct marker. With n=1, this is not a statistically meaningful adoption-accuracy validation — it is a single positive data point, consistent with, but not proof of, correct behavior when the mechanism does converge. The non-compliant tool separately logged one `RECOVERY_ADOPT` on an unanchored Otto CW session (6 observations, +1-omission hypothesis) with no anchor available to confirm or refute correctness — reported for completeness but inadmissible as evidence (wrong tool, unconfirmable).

## 7. Terminal stops

All confirmed compliant-tool terminal stops (`is_stopped_at_end=true`):

| Session | Terminal event | t_ms | Trigger | Frozen position | Fraction of session left unevaluated |
|---|---|---|---|---|---|
| Toby CCW | `CONTRADICTION_RING_FULL_NO_WIN` | 575,778 | Tie (see §5) | mm=7 | 1,108/1,190 candidates (93%) |
| Otto CW worst contamination | `CONTRADICTION_RING_FULL_NO_WIN` | 313,791 | Tie 9/12 vs 9/12, margin=0 | mm=54 | 798/1,626 candidates (49%); never resumes — no direction reversal in this all-CW capture |
| Otto change1 Run 1 | `CONTRADICTION_RING_FULL_NO_WIN` | 313,548 | Tie 11/12 vs 11/12, margin=0 | mm=42 | ~72s of the 385s anchored run, including part of the run leading to the "RUN1 COMPLETE" anchor |
| Otto change1 Run 2 | Two-stage: `CONTRADICTION_RING_FULL_NO_WIN` at 119,908, resumed by real reversal at 177,865, then `LIVENESS_TIMEOUT` at 183,959 | 119,908 / 183,959 | Tie, then silence (56.4s) | mm=51 (both stops) | Remainder of the truncated (INA219-failure-aborted) capture |
| Otto CCW | Multi-stage: earlier contradiction-driven freeze at t=428,436 (mm=30), resumed by a real reversal at t=971,247, refroze via genuine `LIVENESS_TIMEOUT` at t=976,144 (4.9s later) | 428,436 / 976,144 | Contradiction tie, then silence | mm=30 | Majority of a 547s+ window |

A direction reversal is the **only** documented condition that lifts an already-triggered stop, and it does so unconditionally regardless of why the tool stopped (mirrors QUORUM's own `applyDirection()` diagnostic reset) — in the Otto CCW case this resumed tracking for only 4.9s before a genuine, independently-earned liveness timeout refroze it. No capture with a stop and no subsequent reversal (Otto CW worst contamination, and Toby if its clean run never reverses) ever resumes.

## 8. Safe stops caused by uncertainty

Distinguishing mechanism (a) (event-driven, `CONTRADICTION_RING_FULL_NO_WIN`) from mechanism (b) (silence-driven, `LIVENESS_TIMEOUT`):

- **Mechanism (a) fired as the terminal cause** in 4 of 5 sessions (Toby, Otto CW worst contamination, Otto change1 Run 1, the first stage of Otto change1 Run 2 and Otto CCW).
- **Mechanism (b) fired as a terminal cause** in 2 sessions (second stage of Otto change1 Run 2, second stage of Otto CCW) — both times immediately after a reversal-triggered resumption, i.e. `moving_ms_since_prev_push` is not reset on resumption, so a pre-existing silence backlog trips stage-2 immediately rather than restarting a graduated stage-1 warning sequence. This is a policy question, not resolved by this exercise.
- **Mechanism (b) fired non-terminal stage-1 warnings** (elapsed ≈3× the locally-estimated marker interval, matching `N_LIVENESS=3`) in every full-session run tested: Toby (7×), Otto CW worst contamination (5×), Otto CCW (7×) — all pre-empted from reaching stage-2 by an unrelated contradiction-ring stop firing first, except Otto CCW's second-stage genuine timeout.
- No session in the tested corpus ever manufactured a position from PWM/dead-reckoning while stopped — every stop either freezes the last confirmed position or (post-reversal) resumes live tracking. This satisfies the operator's explicit constraint on mechanism (b).

## 9. Behavior on Toby's clean control

Authoritative (compliant-tool) result, cross-confirmed identically by three independent cluster runs: 1,190 candidates from the full 2.8M-ms capture. `ARTIFACT=35`, `CREDIBLE_CONTRADICTION=16`, `EXPECTED_ADVANCE=30`, `MERGED_COMPANION=0`, `POSITION_UNRESOLVED=1,108`, `RESYNC_ADOPTED=1`. `false_inclusion_audit.count=0`, `ring_contamination_audit.count=0`. Terminal stop `CONTRADICTION_RING_FULL_NO_WIN` at t=575,778, ~9.6 minutes into the 47-minute run.

This is the operator's own named acceptance bar ("under Toby's clean data, credible magnets are retained without unnecessary loss") and the candidate does not clear it as currently tuned: only 30 markers were ever confirmed advanced before the run gave up permanently, versus QUORUM's own field-evidence table logging 1,201 accepted passages / 1 disagreement across this identical file. (The non-compliant tool's separate full run of this same file — 1,202 events, 1 artifact, 1,201 `EXPECTED_ADVANCE`, zero contradictions — cannot be cited in support of the candidate; it is circular-input evidence and is excluded from this comparison per §0.) Root cause is traced to §5's ring-never-clears-on-success structural issue, compounded by an unconfirmed map-DNA-phase/starting-marker convention (flagged UNCONFIRMED in every manifest's own `uncertainty_notes`) and the possibility that `MERGE_WINDOW_MS=350`/`AMPLITUDE_RATIO_MAX=0.5` are not fully absorbing genuine multi-lobe passages into single candidates (prior-review Findings #9/#10), fragmenting single magnets into multiple out-of-phase "credible" events that then vote as contradictions.

## 10. Behavior under Otto's worst contamination

Authoritative (compliant-tool) result, Otto CW "restarted boot" session (77943FAD): all 3,665,741 CSV rows / 3,525,930 SAMPLE rows processed, 1,626 candidates emitted. `ARTIFACT=323` (301 duration-floor, 11 contextual-too-soon, 9 flux-floor, 2 excessive-open), `MERGED_COMPANION=358`, `EXPECTED_ADVANCE=14`, `CREDIBLE_CONTRADICTION=11`, `RESYNC_ADOPTED=0`, `POSITION_UNRESOLVED=798`, `PRE_DECLARATION=122`. `false_inclusion_audit.count=0`, `ring_contamination_audit.count=0` across the entire run — zero confirmed structural violations on the file the doctrine calls the worst contamination available.

Terminal stop `CONTRADICTION_RING_FULL_NO_WIN` at t=313,791 (tied hypotheses, 9/12 vs 9/12, margin=0), only 43 seconds / 14 markers into the ~18.5-minute active-driving portion of the 59-minute session, and it never resumes (no direction reversal exists in this all-CW capture). QUORUM's own onboard trace on the same file advanced its believed position 95 more markers (mm 40→135) over the same ~18.5 minutes before its own logic hit a near-tie (margin=1) and declared `NO_QUORUM` — by which point the operator's own qualitative account puts QUORUM's drift at roughly 19 markers. The candidate stopped roughly 81 markers and ~17 minutes earlier than QUORUM's own failure, with zero confirmed false inclusion or ring contamination up to that point — squarely in line with the operator's success criterion of stopping with an evidence-backed explanation rather than inventing coordinates. Independently verified against raw SAMPLE data: after the last candidate row (t=1,386,614), only 0.05% of the remaining ~2.16M samples show `ctl_pwm_actual>20` — the loco is essentially parked for the final ~36 minutes, so the absence of further candidate rows past that point is physically correct, not a silent tool failure.

The pulse-storm finding (§5) indicates the specific stop location here is attributable to a real, recurring wrong-polarity echo artifact near markers, not to the fragmentation mechanism suspected on Toby — two distinct root causes converging on the same structural symptom (ring accumulates unrelated-in-substance evidence and never clears on success).

## 11. Per-adversarial-case results

All 16 requested adversarial cases were found and tested; none required fabricating events from whole cloth (the closest to synthetic construction were: (a) a hand-built same-polarity companion event pair isolating one variable, run only against the non-compliant tool; (b) hand-edited manifests with a wrong `start.mm` layered on the real Toby capture; (c) real SAMPLE rows deleted from a real capture to construct clean omissions).

| # | Case | Tool used | Real/Synthetic | Result |
|---|---|---|---|---|
| 1 | Isolated spike (no adjacent passage) | Compliant | Real (otto_cw 77943FAD:01546) | `ARTIFACT`/`ARTIFACT_DURATION_FLOOR`, no ring insertion — Category 1 exactly |
| 2 | Spike immediately before a genuine passage | Compliant | Real (77943FAD:00229/00230) | Spike→`ARTIFACT` independently; passage→`EXPECTED_ADVANCE` on its own unaffected evidence; no backward merge (design's merge only looks forward) |
| 3 | Spike immediately after a genuine passage (companion merge) | Compliant | Real (77943FAD:00244/00245) | Companion→`MERGED_COMPANION`, never enters either ring; primary's evidence fields (peak/flux/duration/polarity) unaffected — fixed, never laundered by the merge |
| 4 | Opposite-polarity edge lobe (wide + sub-5ms narrow population) | **Non-compliant** | Real (otto_cw, wide instance + 1,881-instance narrow scan) | Wide instance: caught by amplitude floor before merge logic ever runs. Narrow (≤5ms) population: structurally invisible to the tool (never survives QUORUM's own detector latch to produce an input record) — **not validated against the compliant candidate** |
| 5 | Same-polarity companion (danger case) | **Non-compliant**, synthetic construction | Synthetic event pair on real QUORUM map DNA | Companion at dt=400ms (outside merge window) → `EXPECTED_ADVANCE`, silently moved position, purely from map-polarity coincidence — **not re-tested against the compliant candidate; open risk, not a confirmed defect of it** |
| 6 | Credible contradiction (Category 3 disposition) | **Non-compliant** for its dedicated demonstration; independently corroborated by compliant-tool full-session counts | Real (otto_change1 3E6A88F1 t=178,280; also present in every compliant-tool full run — Toby 16, Otto CW 11, change1 Run1 18, Run2 11 `CREDIBLE_CONTRADICTION` events) | Category-3 routing (no primary advance, observation-only) confirmed as a live, firing mechanism in the compliant tool via aggregate counts across every session; the dedicated lifecycle walkthrough used the wrong tool |
| 7 | Single deliberate omission | Compliant | Real (Toby, SAMPLE rows deleted from a real passage) | Split outcome: silently absorbed via same-polarity twin (attempt A) vs. correctly `CREDIBLE_CONTRADICTION`→`LIVENESS_TIMEOUT` on a genuine complete omission (attempt B) — see §2 |
| 8 | Multi (2/3/4 consecutive) deliberate omissions | Compliant | Real (Toby, SAMPLE rows deleted) | 2- and 3-omission: silently absorbed, 2-marker undercount, zero diagnostics. 4-omission: correctly detected and safely stopped — see §2 |
| 9 | Wrong declared start position (off-by-one, gross) | Compliant | Real capture, synthetic (hand-edited) manifest | No code path detects a mismatch against `asserted_mm`; both wrong starts self-destabilized faster than baseline (`LIVENESS_TIMEOUT` ~7min vs. baseline's later contradiction-ring stop ~9.6min) — incidental, not diagnostic |
| 10 | Boot / event-active at stream start | Compliant | Real proxy (9 real GAP-resumption instances in Toby, 1 in otto_change1 Run 2); literal SESSION-start-elevated case absent from all 4 captures | First post-gap sample opens a fresh candidate and was admitted `EXPECTED_ADVANCE` with `gap_overlap` never set — the flag only fires for a candidate already open when a GAP streams by, never for one opening on the very next sample after — a real, silent evidence-corruption path, contrary to the design spec's own §3.4 intent |
| 11 | Reversal mid-interval | Compliant | Real, anchored (otto_change1 3E6A88F1, live `ctl_dir` flip 7.6s before the operator's own anchor text) | `REVERSAL_EXEMPT` correctly suspends only the timing floor; full artifact/polarity checks still ran and correctly produced `CREDIBLE_CONTRADICTION` (not an automatic accept) |
| 12 | Station stop and restart (dwell) | Compliant | Real (Toby, ~29 genuine 15,010ms PWM=0 dwells, matching QUORUM's own `DWELL_MS=15000`) | Both tested dwell-restart transitions correctly discarded the stale pre-dwell interval reference (`NO_PREV_INVALIDATED`) and passed map validation as `EXPECTED_ADVANCE` |
| 13 | Baseline shift (raw/baseline divergence during stall) | Compliant | Real (otto_cw 77943FAD, t≈1,386,614–1,389,614, ~150-count raw/baseline divergence during PWM ramp-to-0) | Force-closed at `MAX_OPEN_MS=3000` → `ARTIFACT_EXCESSIVE_OPEN`, zero ring insertion — QUORUM's own detector opened an equivalent event at the identical instant and (visibly) never closed it |
| 14 | Transport gap overlapping a credible candidate | Compliant | Real (Toby, GAP row t=1,887,952 inside a 526ms/182-peak/57k-flux candidate) | Correctly flagged `gap_overlap=True`, disposed `ARTIFACT_INCOMPLETE` — discarded despite every morphology metric looking like a genuine passage |
| 15 | Extended movement with no credible passage (liveness) | Compliant | Real (Otto CCW, 86.2% PWM-engaged over the frozen window) | Graduated stage-1 warnings fired correctly (7×), genuine stage-2 `LIVENESS_TIMEOUT` fired at t=976,144 after 477,634ms of accumulated moving-time with no credible arrival |
| 16 | Pulse storm / rapid contamination burst | Compliant | Real (Otto CW worst contamination, first 43s, recurring ~150–190ms wrong-polarity echo near almost every marker) | 13/13 genuine crossings correctly `EXPECTED_ADVANCE`; echo mostly `ARTIFACT_CONTEXTUAL_TOO_SOON`, 6 slip-throughs correctly `CREDIBLE_CONTRADICTION`, zero false inclusion — but these 6 are the traced cause of the session's terminal stop |

Summary: 13/16 cases tested against the compliant candidate with real data (cases 1–3, 7–16, minus case 6's dedicated walkthrough); case 6 corroborated indirectly via aggregate compliant-tool counts; cases 4 and 5 tested only against the non-compliant tool and are **not validated evidence about the actual TEMPLATES candidate** — they stand as open, plausible design risks pending re-test against `templates_replay_spec.py`.

## 12. Verdict against the operator's stated success criteria

The operator's own framing, quoted verbatim: *"Success is NOT 'behaves like QUORUM.' Success is: under Toby's clean data, credible magnets are retained without unnecessary loss. Under Otto's worst recorded contamination, artifacts cannot silently corrupt primary position. If the candidate cannot recover confidently from deliberate omissions or credible contradictions, it stops with an evidence-backed explanation rather than inventing coordinates."*

**Criterion 2 (Otto's worst contamination — artifacts cannot silently corrupt primary position): MET.** Zero confirmed false inclusions and zero ring-invariant violations across 1,626 candidates spanning the entire worst-contamination session (§1, §10). The one confirmed false inclusion found anywhere in this exercise (§1) occurred only under the non-compliant, circular tool and is excluded from the compliant candidate's evidence.

**Criterion 3 (stops with an evidence-backed explanation rather than inventing coordinates): MET, with a documented gap.** Every terminal stop observed freezes the last confirmed position rather than fabricating one, and the candidate's stops on Otto's worst contamination arrive ~81 markers and ~17 minutes before QUORUM's own analogous failure, avoiding QUORUM's ~19-marker silent drift (§10). The documented gap: §2 shows that when a same-polarity coincidence happens to sit inside an omission window, the candidate does *not* stop or flag anything — it silently produces a wrong coordinate with a clean audit trail. This is a real, reproducible counter-example to "stops... rather than inventing coordinates," found on Toby's own clean-control capture, and it must be weighed against the successful-stop cases rather than dismissed as a corner case (it reproduced on 3 of 4 tested omission counts).

**Criterion 1 (Toby's clean data — credible magnets retained without unnecessary loss): NOT MET, as currently tuned.** The compliant candidate confirmed only 30 of the (per QUORUM's own field-evidence table) 1,201 accepted passages on this file before permanently stopping 9.6 minutes into a 47-minute clean run, then reporting `POSITION_UNRESOLVED` for the remaining 93% (§9). The root cause is structural and identified with reasonable confidence (§5): the ContradictionRing never clears on ordinary success, so temporally unrelated, individually harmless mismatches accumulate until a 12-slot cap forces a stop regardless of how much correct navigation happened in between. This is not a case of matching QUORUM's own failure mode — QUORUM logged only 1 disagreement on this identical file — so it is a genuine, undischarged regression against the operator's own named acceptance bar, not evidence-against-QUORUM-matching being misread as failure.

**Overall:** two of the three explicitly quoted criteria are met by direct measurement; the third is not met as currently tuned, and the mechanism responsible (ring-never-clears-on-success) is also implicated, via a different failure path (map-polarity-coincidence masking omissions), in the one confirmed counter-example to criterion 3. Both point at the same underlying design gap: the candidate's only two confidence signals (ring-fill state, 1-bit polarity match) are each, independently, too coarse — one over-triggers stops on data with nothing wrong, the other under-triggers on data with something wrong. Neither failure mode reproduces QUORUM's own specific defects (advance-before-validation, unbounded silent drift); both are new failure modes introduced by the replacement design and need deliberate, evidence-based retuning — ring-clearing policy, and a richer contradiction signal than raw polarity — before this candidate can be said to meet its own stated bar.

## Files this exercise produced

Tool implementation:
- `/Users/davidbrown/esp-loco-control/tools/templates_replay_spec.py` — the compliant three-way classifier (1,280 lines)
- `/Users/davidbrown/esp-loco-control/tools/test_templates_replay_spec.py` — its unit tests (15 tests, all passing)

Design spec (pre-existing at start of this report's assignment, produced earlier in this work stream):
- `/Users/davidbrown/esp-loco-control/docs/TEMPLATES_REPLAY_DESIGN_SPEC.md`

Manifests (ground-truth start/anchor declarations, never fed to the classifier as admission input):
- `/Users/davidbrown/esp-loco-control/tools/manifests/toby_ccw_20260824.json`
- `/Users/davidbrown/esp-loco-control/tools/manifests/otto_ccw_20260824.json`
- `/Users/davidbrown/esp-loco-control/tools/manifests/otto_cw_20260824.json`
- `/Users/davidbrown/esp-loco-control/tools/manifests/otto_change1_20260825_run1.json`
- `/Users/davidbrown/esp-loco-control/tools/manifests/otto_change1_20260825_run2.json`
- `/Users/davidbrown/esp-loco-control/tools/manifests/grillers.json` and `pwm40_run.json`/`pwm90.json` (pre-existing, not exercised by the 4 assigned captures — flagged in-line above where relevant)

Pre-existing, non-compliant tool referenced throughout for contrast (not produced by this exercise, tracked/committed prior to it):
- `/Users/davidbrown/esp-loco-control/tools/templates_replay.py`
- `/Users/davidbrown/esp-loco-control/tools/test_templates_replay.py`

This report:
- `/Users/davidbrown/esp-loco-control/docs/TEMPLATES_REPLAY_RESULTS_REPORT.md`

**Nothing produced by this exercise, or by the replay work it summarizes, has been `git add`-ed, committed, or otherwise staged.** `git status` at time of writing shows `tools/templates_replay_spec.py`, `tools/test_templates_replay_spec.py`, all 5 manifests listed above, and `docs/TEMPLATES_REPLAY_DESIGN_SPEC.md` as untracked (`??`); this report is a new untracked file at the path above. No tracked file was edited by this report-writing pass. No file under `firmware/` was read for anything but reference, and none was modified. No destructive or state-changing git command was run.
