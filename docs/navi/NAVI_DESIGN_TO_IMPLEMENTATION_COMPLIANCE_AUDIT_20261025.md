# NAVI — Design-to-Implementation Compliance Audit (October 25 firmware)

**Date:** 2026-10-25 · **Author:** Claude (read-only audit) · **Status:** evidence report. No firmware, specification, or design document was changed. Nothing here is a decision.

**Firmware audited:** `NAVI_POWER_OF_NORMAL_MANUAL_ONLY`, recognizer `NAVI_CANDIDATE_70X2_RAW_PLUS_RTB_NO_SHAPE`, source `05393117` (branch `codex/navi-power-normal-timing-20261024`).
All firmware line references below are `05393117:firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY/<file>:<line>`.

---

## 0. Sources and their standing

| Key | Document | Commit / location | Standing |
|---|---|---|---|
| **CONF** | `docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md` | `9a6bbb7` | Governing record: "all ten propositions closed by consensus (David, Sam/Codex, Claude)". Thresholds explicitly open. |
| **AGR20** | `docs/navi/NAVI_TWENTY_ORIGINAL_AGREEMENTS_20261024.md` | `4ca4807` (origin only) | Evidence record of the Ten Principles + ten propositions, cited to transcript message UUIDs. |
| **ODR** | `docs/navi/NAVI_ORIGINAL_DECISION_RECORD_20261024.md` | `610c06a` (origin only) | Evidence archive (D-01…D-08) recovered from conversation extracts. |
| **GDS / REC** | `NAVI_POWER_OF_NORMAL_GOVERNING_DESIGN_SPEC_20261024.md`, `…RECONCILED_SPEC_20261024.md` | `980d8af`, `f83d5ce` (origin only) | Derived specifications. Each says it authorizes no firmware. |
| **CS** | `docs/navi/NAVI_POWER_OF_NORMAL_CLEAN_SHEET_PROPOSAL_20261024.md` | `04ce4b6` | **Proposal** (Codex). Decision register D1–D13, all left for operator decision. |
| **INT** | `docs/navi/NAVI_POWER_OF_NORMAL_INTEGRATED_IMPLEMENTATION_PROPOSAL_20261024.md` | `340b999` | **Proposal**. "No firmware has been created or changed." |
| **REV** | `docs/navi/NAVI_CLEAN_SHEET_PROPOSAL_INDEPENDENT_REVIEW_20261024.md` | `05f9682` | Review of CS. |
| **INV** | `docs/navi/NAVI_POWER_OF_NORMAL_ONSET_SHORTFALL_INVESTIGATION_20261024.md` | firmware branch, `6c9037e` lineage | Claude investigation of the first build, 2026-10-24. |
| **FW-README / comments** | sketch `README.md` and header comments | `05393117` | Implementation documentation. Quotes operator instructions, but is **not** a design record. |

Repository conditions that matter for verification (reported, not fixed):

- The conference record and both proposals are **not ancestors** of the firmware branch. The recovered agreement documents (AGR20, ODR, GDS, REC) exist only on `origin/docs/navi-tracer-failure-20261022`. The local branch of that name is 7 commits behind origin.
- There is **no `docs/decisions/` record after 2026-10-22**. Several mechanisms in the firmware cite "David/Sam, 2026-10-24" instructions only in code comments.
- The named additional repository `ir-navi-reliable-transport` could not be found locally or through `gh` (`ninobur/ir-navi-reliable-transport` does not resolve). Its contents are an **evidence gap**.
- I found no repository copies of the existing October 25 analyses of the MM56 and MM23 events. Only MM157 was re-examined from raw data (§6).

### Flashed-revision identity

- Telemetry carries the sketch name, class, and recognizer string, but **no commit hash**. A byte-level match to `05393117` cannot be shown.
- The `diag/navi_encounter` rows on Oct 25 carry `opening_us`/`opening_pwm`. That schema was introduced in `815a590`, so the flashed source is `815a590` or later.
- `05393117` changes only docs and tools; its firmware is identical to `815a590`. The `esp-loco-control-navi-timing` worktree at `05393117` has no uncommitted firmware edits (one deleted `credentials.example.h`).
- A host replay built from these exact headers reproduces the 15:14 run: 134 encounter closures, matching the field's 134 confirmed / 0 nonqualifying. It also opens the encounter that never closes at the same point (Otto clock 215.58 s, after the MM157 opening at 214.962 s). That is consistent with the flashed source, but it is not proof of it.

---

## A. Executive findings

1. **The architecture that was agreed is implemented faithfully.** One expected target. No search and no recovery. No shape veto. A nonqualifying encounter leaves the target pending. Hall is recorded continuously. Completion is evidence-based, not timed.

2. **The mechanisms that produced the October 25 failures were never approved.** Specifically:
   - the frozen closure reference;
   - the 10 × 20 ms, ±20 ADC return-to-baseline rule;
   - the single active encounter that silently discards later onsets;
   - the baseline's start-on-one-block and admit-everything collection rule.

   Each one fills a question the records explicitly left open: CS D2 and D3, ODR unresolved #1, REC-11.

3. **The 2026-10-24 investigation (INV) had already identified this failure mode in this build lineage.** INV named the mechanism as "a reference … frozen at the instant of onset" combined with "one candidate open at a time". It referred the fix to David as "an architectural choice … reported here for decision, not made unilaterally". No record of that decision exists. The next commit (`6c9037e`) did three things:
   - removed the lock and private reference from the *detector*;
   - **kept** the frozen reference in the *encounter*, and moved the one-at-a-time rule into `rvmTask`;
   - removed the 10 s abandon timeout.

   The net effect turned INV's bounded 10 s blackout into an **unbounded** one. That is what happened at MM157.

4. **One established requirement is not met: BASE-03, "avoid incorporating magnetic tail into a replacement baseline".** The baseline admits every moving block from a single "appears ordinary" block until the next *opened* encounter. While an encounter is stuck, no encounter can open, so collection runs across whole magnets. The October 25 recording demonstrates this (§6).

5. **Proposition 6B (the per-interval arrival-boundary stop) was removed on 2026-10-24.** The removal followed an operator instruction quoted only in the firmware README. The conference record was not amended. The records now conflict.

6. **No "duration estimator" exists in the records or the firmware under that name.** The two duration-like mechanisms, minimum width and opening-to-opening timing, are both defined, and implemented, as evidence applied only *after* observed completion. Neither can close an encounter. The October 25 failure did not involve either one (§D).

7. **The approved design does not say what happens to an encounter that never completes.** Prop 6B governs motor authority only; it retains position and does not end an encounter. This is a genuine design gap, not an implementation defect.

8. **The subsequent magnetic evidence existed and was discarded silently.** After MM157, 233 later onset clusters match the mapped polarity sequence MM159→MM49 (wrapping once) with **zero mismatches**. The last one maps to MM49, matching the operator's physical end "around MM048–MM049". None of these onsets was counted or published. The code comment claiming visibility "via the existing onsetQueueDrops-style counters" is incorrect (§C-3).

**Answer to the completion question:** In part. The firmware implements the navigation *cycle* we agreed to. Where it departed, it was mostly not from an agreed design but into territory with no agreed design: closure reference, return rule, overlapping encounters, never-completing encounters. Those choices were made in code without a recorded decision. One established baseline requirement (BASE-03) is violated. One established proposition (6B) was withdrawn without amending the governing record.

---

## B. Requirements traceability matrix

| # | Requirement | Authoritative source | Approval status | Current implementation | Finding |
|---|---|---|---|---|---|
| 1 | Hall recorded continuously and unconditionally, independent of navigation | CONF §1.2; AGR20 Principle 9 ("unsuccessful match cannot interrupt observation"); REC-05 | Approved | `hallTask` reads every 5 kHz sample into the NSR native record and the 20 ms tracing (`.ino:279–317`). Onset detection is gated only on a baseline existing (`.ino:305`). | COMPLIANT |
| 2 | 70-count, two-consecutive-sample candidate sentinel; opening polarity captured | CONF §1.3, Prop 7 quote (ODR D-01); REC-06 | Approved (sentinel). Sampling representation open (ODR unresolved #1, CS D1). | 70 ADC on two consecutive samples after 5→1 kHz decimation, against the *live* baseline (`NaviCandidate.h:30–32, 48–69`). Polarity is the sign of the first crossing. | COMPLIANT on the sentinel. **DESIGN UNRESOLVED** on cadence: the records conflict. INT says "two consecutive native observations"; `NAVI_RVM_70_COUNT_GATE_20261020.md:31` says 1 kHz. |
| 3 | Preserve pre-trigger context | GDS HALL-02; INT §3 step 3 | Proposal-level detail (GDS marks HALL-02 "E for sentinel/polarity") | No pre-trigger ring in the encounter. Raw context survives only in the NSR record. | PARTIALLY COMPLIANT |
| 4 | Qualification only after observed return to baseline (evidence-based, not timer-based) | CONF §1.3, Prop 6A; ODR D-06 | Approved as a principle. "Except return-band and width details" (ODR D-06); REC-11 unresolved. | Closure requires `RTB_RUN_BLOCKS=10` consecutive 20 ms blocks within `RTB_MARGIN_ADC=20` of the level frozen at open. No timer path (`NaviEncounter.h:42–44, 71–90`). | Principle COMPLIANT. Band, persistence, and reference: **DESIGN UNRESOLVED** (implementation choice). |
| 5 | Reference used to measure magnetic departure | CS D2 ("No baseline algorithm can be adopted from this proposal"); INT "consumes the active reference without changing it" | **Not approved.** INT proposes a fixed reference; CS leaves it open. | Departure is measured against the **live** baseline, recomputed about every 1 s (`NaviCandidate.h:52`; `.ino:404`). | DESIGN UNRESOLVED |
| 6 | Reference used to determine closure | CS D2; INT §2 (proposal) | **Not approved** | **Frozen** at onset for the life of the encounter (`NaviEncounter.h:61`). INV identified this as the lockup mechanism. | DESIGN UNRESOLVED (implementation choice; demonstrated harmful) |
| 7 | Same reference for departure and closure | None found | No record | **Different** references: live for departure, frozen for closure | DESIGN UNRESOLVED |
| 8 | 200 ms uninterrupted return | None. INV calls the RTB values "admittedly-arbitrary". | **Not approved** | 10 blocks × 20 ms within ±20 | DESIGN UNRESOLVED |
| 9 | Treatment of an onset while an encounter is open | CS D3 ("Decide treatment of a second onset before prior observed completion"); CS §3.2 ("No overlap-merging, candidate replacement, timeout-based completion, or lockout is assumed"); INT §1 claims "a following encounter becomes eligible only after … intervening return", attributed to uncited "prior NAVI decisions" | **Unresolved.** INT's claim is proposal-level and unsourced. | Silently discarded: no counter, no publish (`.ino:382–399`) | DESIGN UNRESOLVED. Silent discard: PARTIALLY COMPLIANT with #1 (raw recording continues; navigation evidence is lost without record). |
| 10 | Encounter that never completes | CONF has no provision. 6B is motor-only and "retain[s] the last confirmed position". CS §3.2 does not assume timeout-based completion. | **Design gap** | No timeout and no abandon (`NaviEncounter.h:21–28`). The encounter can remain open forever. | DESIGN UNRESOLVED |
| 11 | Baseline is spatial, inter-magnet; a median of the intervening segment | AGR20 Principle 6 ("The longer intervening segment supplies the baseline median"); REC-10; ODR D-08 | Approved (principle) | Median of admitted 20 ms block means (`NaviBaseline.h:114–120`) | COMPLIANT (principle) |
| 12 | Baseline start and stop points | ODR D-08 primary fragment B: "Use Magnet Close to start the Baseline measurement and Magnet open 70ADCx2 to stop" | Primary fragment. ODR: "exact … algorithm is not recoverable". | Start: one block within 20 of the committed level, deliberately *not* the encounter's closure (`NaviBaseline.h:13–26, 83–84`). Stop: only an onset that *opens* an encounter (`.ino:383–389`). | PARTIALLY COMPLIANT. Departs from fragment B's "Magnet Close" start. The comment cites a 2026-10-24 correction with no record. |
| 13 | Avoid incorporating magnetic tail into the baseline | GDS BASE-03; ODR D-08 | **Approved** | After entry, every PWM>0 block is admitted "unconditionally (no further per-block deviation test)" (`NaviBaseline.h:17–21, 86–89`). This includes approach tails below 70 in every interval, and whole magnets when an encounter is stuck. | **NONCOMPLIANT** (demonstrated on Oct 25, §6) |
| 14 | No fixed time window substituted for spatial distance | ODR D-08; BASE-02 | Approved | PWM>0 gate used as a travel proxy; bounded ring buffer (`NaviBaseline.h:27–29, 43–48`) | COMPLIANT. The PWM proxy itself was agreed earlier (comment cites 2026-09-28 / 10-18); I did not re-verify that. |
| 15 | Baseline updated live within the interval | Code comment quotes an operator instruction of 2026-10-24 (`NaviBaseline.h:36–41`) | **UNVERIFIED** (no record) | Recompute every 50 admitted blocks | UNVERIFIED |
| 16 | Minimum physically plausible width: necessary, not sufficient; does not determine where the encounter ends | CONF Prop 6A | Approved. Value open (CONF §3). | `MIN_WIDTH_BLOCKS = 0` (disabled). Width is measured to RTB, never used to end the encounter (`NaviEncounter.h:42, 82–84`). | COMPLIANT (value correctly not invented; qualification inert) |
| 17 | Shape is diagnostic only | CONF Prop 7 | Approved | No shape computation. `peakAdc` is diagnostic. | COMPLIANT |
| 18 | One expected target; qualifying evidence advances exactly one | CONF Prop 8; REC-01/03 | Approved | `NaviTarget.h:67–90` | COMPLIANT |
| 19 | Nonqualifying evidence leaves the target unchanged; no contradiction state | CONF Prop 8; ODR D-02 | Approved | `NaviTarget.h:89`. No Contradiction state. | COMPLIANT |
| 20 | No search, skip, or recovery | CONF Props 2, 8 | Approved | None present | COMPLIANT |
| 21 | Movement predicts; Hall confirms | CONF Prop 4; ODR D-07; REC-12 | Approved | Timing can only *veto* (`Mismatches` → nonqualifying). It never confirms (`NaviTarget.h:81–82`). Profiles empty → always `UNPROFILED` on Oct 25 (`NaviTimingProfiles.h:26–27`). | COMPLIANT on "never confirms". Timing *as a qualification veto* is **UNVERIFIED** (cited only in code comments). Inert on Oct 25. |
| 22 | Per-interval arrival boundary → stop-and-hold is the sole NAVI stop | CONF Props 6B, 8; REC-13 | Approved by the conference. Withdrawn 2026-10-24 by operator instruction quoted in FW-README:56–62. Conference not amended. | Removed. No automatic stop of any kind. | **DESIGN UNRESOLVED (records conflict)** |
| 23 | Secondary lobes / rebounds | CONF Prop 7: "The actual discriminator … is amplitude (rebound peaks fall below the 70-count sentinel threshold) and proximity/sequence position — already part of the architecture" | Approved as an observation. Mechanism unspecified. | Lobes below 70 open nothing. Re-crossings during an open encounter are discarded (#9). A lobe of 70 or more after closure opens a new encounter, judged against the *next* target. No explicit proximity rule. | DESIGN UNRESOLVED |
| 24 | Physical-reachability protection / suppression of impossible subsequent magnets | **No source found** in CONF, AGR20, ODR, GDS, REC, CS, INT, REV | No record | Not implemented, except implicitly through #9 | DESIGN UNRESOLVED (no agreement exists) |
| 25 | Unknown or missing evidence recorded as an evidence gap | GDS VAL-04 | Approved (validation discipline) | Discarded onsets are not recorded. Baseline level is not published. | PARTIALLY COMPLIANT |

---

## C. Detailed discrepancy findings

### C-1. Baseline admits magnetic evidence (BASE-03): NONCOMPLIANT

1. **Requirement:** "Avoid incorporating magnetic tail into a replacement baseline" (GDS BASE-03, status E). The baseline is the ordinary-track median between magnets (AGR20 Principle 6).
2. **Evidence:** ODR D-08, fragment B ("concern not to capture a magnet tail"), and REC-10.
3. **Implementation:** Collection begins on *one* block within 20 ADC of the committed level. From then on, every PWM>0 block is admitted with no deviation test, until `finalize()`. `finalize()` is called only when an onset *opens* an encounter (`.ino:383–389`). So the sub-70 approach of each next magnet is admitted in every interval. When an encounter is stuck open, no `finalize()` occurs, and collection spans entire subsequent magnets.
4. **Source:** `NaviBaseline.h:79–90` (entry 83–84, unconditional admission 86–89), `NaviBaseline.h:103–111`, `.ino:383–389, 403–405`.
5. **Consequence:** The baseline can be pulled by magnet peaks. That shifts the candidate detector's reference.
6. **Oct 25 demonstrated:** Yes (§6). Entry was triggered during the MM158 region at about 216.2–216.3 s. The first recompute after entry, at 217.22 s, gave **2126.9**, from a window containing a peak block of **2182**. The background over the next second was about 2090–2115. The estimate ran 25–45 ADC high for about 3 s, then tracked within about 10.

*Minor implementation note, not a design finding:* `std::nth_element` reorders the ring buffer in place (`NaviBaseline.h:117`). Once more than 2048 samples are collected, overwrites therefore no longer remove the oldest samples. The comment says no real interval reaches that capacity.

### C-2. Frozen closure reference (NaviEncounter.h:61): implementation choice, not approved

1. **Requirement:** Completion is the observed return to baseline (approved). The reference, band, and persistence are open (CS D2, REC-11, ODR D-06).
2. **Evidence:** CS D2: "No baseline algorithm can be adopted from this proposal." INT proposes "consumes the active reference without changing it during an encounter", but INT is a proposal. INV (2026-10-24) found a reference "frozen at the instant of onset and never updated" to be the lockup mechanism. INV then stated that "giving NaviEncounter a baseline that can legitimately move … or allowing detection of a new candidate without fully resolving the prior one first — is an architectural choice … reported here for decision".
3. **Implementation:** `open()` copies `levelAtOpen`. `push()` judges every block against it for the life of the encounter.
4. **Source:** `NaviEncounter.h:59–64, 71–79`.
5. **Consequence:** A lasting background offset larger than 20 ADC can never satisfy closure.
6. **Oct 25 demonstrated:** Yes. The frozen level was 1957.0. Moving background 2–4 s later was a median of about 2073, and it was about 2038 five minutes later. It never came within 20 of 1957.

### C-3. Single active encounter with silent discard (.ino:382–399): unresolved policy, silently implemented

1. **Requirement:** The treatment of an onset during an open encounter is explicitly undecided (CS D3, §3.2). Observation must remain continuous (approved).
2. **Evidence:** CS §3.2: "No overlap-merging, candidate replacement, timeout-based completion, or lockout is assumed here." INV: the "one candidate open at a time" gate is "the mechanism" of the 10-24 shortfall.
3. **Implementation:** An onset arriving while `encounter.active()` is dropped. Nothing is counted or published. The comment says such onsets are "visible via the existing onsetQueueDrops-style counters". That is incorrect: `onsetQueueDrops` counts only queue overflow (`.ino:309`).
4. **Source:** `.ino:382–399`; FW-README:21–24 ("a candidate while another encounter is unresolved does not stop detection"). Detection does continue; the evidence does not reach judgement.
5. **Consequence:** One unresolved encounter withholds all later evidence from navigation indefinitely, and invisibly in telemetry.
6. **Oct 25 demonstrated:** Yes. 323 onsets were discarded after 215.58 s. Clustered at a 0.3 s gap (my analysis choice), 233 clusters follow the mapped polarity of MM159→MM49 exactly. NAVI stayed at MM157→MM158 from 15:18:23 until shutdown.

### C-4. Removal of the 10 s abandon timeout made the lockup unbounded

1. **Requirement:** CONF Prop 6A: completion is "evidence-based, not timer-based". REC-14 excludes "a universal timeout".
2. **Evidence:** REC-14's "universal timeout" traces to Prop 6B's arrival-boundary wording ("empirically validated per-interval boundary, not universal timeout/raw maximum"; AGR20 6B). It is about the protective stop. Reading it as a prohibition on encounter abandonment is an *interpretation* recorded in `NaviEncounter.h:21–28`.
3. **Implementation:** No abandon path.
4. **Source:** `NaviEncounter.h:21–28`; commit `6c9037e`.
5. **Consequence:** Combined with C-2 and C-3, NAVI cannot advance again within the session.
6. **Oct 25 demonstrated:** Yes. `encounter_active:1` appears in every `state/nav` row of `9950011_20261025_152023.log` (219 rows).

This is not a recommendation to restore a timeout. The approved design does not say how an uncompletable encounter ends (§F).

### C-5. Proposition 6B withdrawn outside the governing record

The conference made the per-interval boundary the sole NAVI stop authority (Props 6B, 8). FW-README:56–62 records its removal on David's instruction ("missing a magnet is not our problem; correctly recognizing it is"). The conference record and the REC/GDS specifications still require it. Under AGENTS.md §7 the repository record must be reconciled. Which one governs is David's decision.

On Oct 25 the build was manual-only, so 6B would have been a motor request in any case. Even present, 6B would not have closed the stuck encounter: it retains position.

### C-6. Baseline start trigger departs from the primary fragment

Fragment B says "Magnet Close" starts collection. The firmware starts on a single block "appearing" ordinary and explicitly decouples collection from the encounter's closure (`NaviBaseline.h:13–26`). The change is attributed to a 2026-10-24 correction ("David/Sam") with no record. **UNVERIFIED** approval. At MM157 this rule let a magnet dip passing near the stale 1957 level start collection inside magnetic structure (C-1).

---

## D. Duration-estimator determination

**Finding:** I found no record establishing a "duration estimator" or a "magnet-passage duration" estimator. That covers the conference record, AGR20, ODR, GDS, REC, CS, INT, and REV. The firmware contains no such component. Three recorded mechanisms involve duration, and each is defined as follows.

| Mechanism | What it estimates | Recorded role | Can it end or constrain an open encounter? | Oct 25 implementation |
|---|---|---|---|---|
| Minimum physically plausible width (Prop 6A) | Lower bound on excursion width, from the fastest plausible speed (CONF §3) | Qualification only: "necessary, not sufficient … the minimum does not determine where the waveform ends, only the observed Hall signal does that" | **No.** Explicitly excluded by Prop 6A. | Measured from opening to RTB. Threshold 0, so inert. Applied only after closure. |
| Movement-derived timing (Props 3, 4; ODR D-07) | Expected arrival of the one next magnet | Prediction of the neighborhood. "Not a replacement for Hall evidence." CONF §1.3 lists "geographic consistency" among the qualifiers (Prop 7 quote). | **No.** INT §3 step 4: "A time estimate may report lateness but must not declare waveform completion." | Opening-to-opening interval, applied only inside `onEncounter()` after RTB (`NaviTarget.h:67–90`). Its use as a *qualification veto* is cited only in code comments (UNVERIFIED). Profiles empty: inert. |
| Arrival boundary (Prop 6B) | Latest permissible arrival per interval | Motor authority only: stop-and-hold, retain position, keep recording. Does not diagnose. | **No.** It retains position; it was never an encounter-closure rule. | Removed (C-5). |

**Did the design require return-to-baseline before duration could influence a decision?** Yes. CONF §1.3 says a candidate "is evaluated **retrospectively, after the Hall signal itself establishes completion** (observed return to baseline), against: correct opening polarity, a minimum physically plausible width … and observed completion." ODR D-06 says the same.

**Conformance:** The October 25 implementation conforms. No duration or timing quantity participates in opening, continuing, or closing an encounter. Both apply only to a completed encounter. The explanation offered in discussion, that duration helps qualify a completed encounter rather than end one, is **consistent with the records**. The records also leave open whether anything may *constrain* an open encounter that never completes (§F). The October 25 failure did not arise from any duration mechanism.

---

## E. Baseline and encounter-management determination

| Mechanism | Approved? | Basis |
|---|---|---|
| Frozen reference for measuring departure | **No.** Not implemented either: departure uses the live baseline. | INT proposes it. CS D2 leaves it open. |
| Frozen reference for closure | **No.** Implementation choice. | No record. INV flagged it as the defect mechanism and referred the fix for decision. |
| Same reference for both | **No record either way.** The implementation uses different ones. | — |
| 200 ms uninterrupted return (10 × 20 ms, ±20) | **No.** Implementation values. | INV: "admittedly-arbitrary values". ODR D-06 and REC-11 leave the band and persistence open. |
| Suppression of later candidate openings during an unresolved encounter | **No.** Explicitly an open decision. | CS D3 and §3.2. INV identified the gate as the 10-24 failure mechanism. INT's "eligible only after … return" is an uncited proposal claim. |
| Live baseline update during an open encounter | Not separately approved. Cited only in code comments. | `NaviBaseline.h:36–41` |
| Baseline sample admission | The principle is approved (spatial median between magnets, no magnetic tail). The mechanism is not. | REC-10/11, BASE-03. The implementation violates BASE-03 (C-1). |

All three named mechanisms (frozen closure reference, 200 ms return, single active encounter) are **implementation choices, not approved design decisions**. They were introduced into a space the governing records explicitly reserved for decision, after a documented warning (INV) that this combination locks up on this railway.

---

## 6. October 25 reconciliation: MM157 (boot `7463C9467F2273E6`, CW, PWM 90)

Sources: MQTT logs `9950011_20261025_151449.log` and `9950011_20261025_152023.log`, and NSR `navi_session_9950011_7463C9467F2273E6_8DBC960E_20261025_151449.nsr` (in `~/Pi_Log_Downloads`). The NSR was decoded with `analysis/navi_rvm_shape_20261018/unified/u01_decode_nsr.py` and replayed through the `05393117` headers (Appendix). Times are on Otto's clock in seconds. MQTT MM157 `opening_us` = 214962442.

### A. Sensor observations (raw)

- Moving background before MM157: median block mean about **1955–1957**.
- From about 215.42 s the Hall rises gradually: 1960 → 1980 → 2023 by 215.60.
- A positive peak of 2148 follows at 215.78. Then an elevated plateau of about 2128 at 216.7–216.9, a negative-going dip to 1915 at about 216.26, and a peak of 2182 at 216.44.
- Moving background thereafter: median **2073** at 220–222 s (+116), 2061 at 240 s, 2049 at 320 s, **2038** at 540 s (+81). It does not return toward 1957 during the session. PWM was 90 throughout, until a ramp-down from 520 s.
- These are measurements. Their physical cause is **not established** by this audit. (An earlier session's working note attributes Otto's background steps to sensor wiring. That was not ratified, and this audit does not rely on it.)

### B. Implementation behavior (replay of the actual headers)

- MM157 closed normally at 215.32 (width 8 blocks, N, confirmed).
- At **215.580** a +N onset fired against the live baseline of 1957.0, on the rising background, before the mapped MM158 (which is S). The encounter opened with a **frozen level of 1957.0**.
- No block came within ±20 of 1957 for 10 consecutive blocks, so **the encounter never closed**.
- The S-going dip near 216.26 is the only S-going feature in the MM158 interval. It reached only −43 against the 1957.9 reference, so **no S onset was generated**. Attributing that dip to MM158 is an interpretation.
- The baseline restarted collection on a single block near 1957 during this structure. It then admitted every moving block, including the 2182 peak. The first recompute (217.22) gave 2126.9, then 2112.2 (218.24), then 2080 (about 220). After that it tracked the background within about 10.
- Against that live baseline, **323 onsets** were detected after 215.58 and **all were discarded**. Clustered (gap > 0.3 s), 233 clusters match the mapped polarity of MM159 → … → MM170 → MM0 → … → MM49 (one full wrap plus) with **0 mismatches**. The last, at 537.80 s, maps to **MM49**. The operator reported a physical end "around MM048–MM049".
- MQTT agrees: `encounter_active:1` continuously from 15:18:23.96 to shutdown, with NAVI fixed at MM157→MM158.

### C. Design compliance

| Step | Governed by an approved requirement? | Compliance |
|---|---|---|
| Onset at 215.58 on a background rise | 70×2 sentinel (approved); reference open | Compliant with the sentinel as written. The sentinel does not, by itself, distinguish a background step from a magnet, and no agreement says it must. |
| Encounter held open on the frozen 1957 reference | Closure reference and band unresolved | Implementation choice (C-2) |
| Later onsets discarded | Unresolved (CS D3) | Implementation choice. Silent (C-3). |
| Baseline admits magnet blocks | BASE-03 approved | **Noncompliant** (C-1) |
| NAVI stale for 5+ minutes with no indication | No agreement covers this. 6B removed. | Design gap (C-4, C-5) |
| No search or recovery attempted | Props 2, 8 | Compliant |

**MM56 and MM23** (named in the assignment) were not re-analyzed. I did not locate their prior analyses in the repository, and I did not identify the sessions concerned. The mechanism above (C-2/C-3/C-4) is general to any lasting background offset above 20 ADC.

---

## F. Recommended reconciliation (questions for David, not actions)

### F-1. Corrections that existing agreements already require (each still needs your approval before any change)

1. **BASE-03:** the baseline must not admit magnetic tail. The requirement is established; the admission mechanism is not. A correction needs you to choose the admission rule. That rule is the same question as REC-11.
2. **VAL-04 / continuous evidence:** discarded onsets and the live baseline level are invisible in telemetry. Recording them is instrumentation, not behavior, and it is consistent with AGENTS.md §8.
3. **Repository record (AGENTS.md §7):** the conference record still requires Prop 6B. The removal instruction exists only in the firmware README. One of the two must be amended by your ruling.
4. **Factual comment error** at `.ino:395–398`: it claims visibility that does not exist. Correcting it is documentation only.

### F-2. Design decisions the records left open, now exposed by field evidence

1. **Closure reference:** frozen at onset, the live baseline, or something else? (CS D2)
2. **Return criterion:** band, persistence, and its relation to the departure reference. (REC-11)
3. **An onset while an encounter is open:** discard, replace, keep both, or something else? (CS D3)
4. **An encounter that never completes:** the approved cycle has no answer. (CONF has no provision; 6B is motor-only.)
5. **Baseline start point:** "Magnet Close" per fragment B, or the current "appears ordinary" single block?
6. **Timing as a qualification veto:** confirm or withdraw. It is currently cited only in code comments.
7. **Lobes and "proximity/sequence position":** Prop 7 says this is "already part of the architecture", but no mechanism is specified.
8. **Reachability:** no agreement exists. Decide whether one is needed at all (Prop 9: only with field evidence).
9. **Prop 6B:** ratify its removal or reinstate it.

### F-3. Questions that need experimental evidence first

1. **What physically causes the abrupt lasting background offsets?** Locomotive, mount, wiring, or track. This is a bounded bench and track test, not a firmware question.
2. **How often, and on which locomotives?** Otto only, or Toby too? Across the full set of Oct 25 sessions.
3. **Can a lasting background offset be distinguished from a magnet in the Hall data alone,** for example by its absence of return? This decides whether F-2.1–F-2.4 can be answered from Hall evidence at all.
4. **Does route spacing at all permitted speeds preclude real magnetic overlap?** (CS D3's own recommended first step.)

---

## Appendix — replay used in §6

Built against the headers in the `esp-loco-control-navi-timing` worktree at `05393117`:

```
c++ -std=c++17 -O2 -I<sketch dir> trace.cpp -o trace
./trace native_7463C9467F2273E6.bin 0 1e9 blocks.csv onsets.csv
```

`trace.cpp` mirrors `rvmTask` and `hallTask` ordering for the baseline, candidate, and encounter path:

```cpp
#include <cstdio>
#include <cstdlib>
#include "NaviObservation.h"
#include "NaviBaseline.h"
#include "NaviCandidate.h"
#include "NaviEncounter.h"
#pragma pack(push,1)
struct Native { uint32_t serial; uint64_t us; int16_t adc; uint8_t pwm; uint8_t direction; };
#pragma pack(pop)
static navi_power::BaselineTracker baseline;
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"rb"); double t0=atof(argv[2])*1e6, t1=atof(argv[3])*1e6;
  FILE*fb=fopen(argv[4],"w"); FILE*fo=fopen(argv[5],"w");
  fprintf(fb,"t_s,hall,pwm,baseline_level,enc_active,enc_level\n"); fprintf(fo,"t_s,pol,level_at_open,opened,enc_level_if_discarded\n");
  navi_power::Tracing tracing; navi_power::CandidateDetector cand; navi_power::Encounter enc;
  baseline.beginBootCollection(); uint32_t blk=0; float encLevel=0; uint64_t lastUs=0; unsigned closes=0;
  auto onBlock=[&](const navi_power::Block&b){ ++blk; baseline.onBlock(b.hall,b.pwm>0);
    bool was=enc.active(); if(was){navi_power::CompletedEncounter d; if(enc.push(b,d)){++closes; if(lastUs>=t0&&lastUs<=t1) fprintf(fo,"%.3f,CLOSE,%.1f,%u,%u\n",lastUs/1e6,encLevel,d.widthBlocks,unsigned(d.pol>0));}}
    if(lastUs>=t0&&lastUs<=t1) fprintf(fb,"%.3f,%.1f,%u,%.1f,%d,%.1f\n",lastUs/1e6,b.hall,b.pwm,baseline.level(),was?1:0,encLevel);};
  Native r; while(fread(&r,sizeof r,1,f)==1){ lastUs=r.us;
    if(baseline.haveLevel()){ navi_power::CandidateOnset o;
      if(cand.onSample(uint16_t(r.adc),baseline.level(),r.serial,r.us,r.pwm,o)){
        bool opened=!enc.active();
        if(opened){uint32_t n; baseline.finalize(n); enc.open(o,blk); encLevel=o.levelAtOpen;}
        if(r.us>=t0&&r.us<=t1) fprintf(fo,"%.3f,%d,%.1f,%d,%.1f\n",o.openUs/1e6,o.pol,o.levelAtOpen,opened?1:0,opened?0:encLevel);}}
    tracing.add(r.us,uint16_t(r.adc),r.direction,r.serial,r.pwm,255,onBlock);}
  printf("closes=%u blocks=%u\n",closes,blk); return 0;}
```

Result: `closes=134`, matching the field's 134 confirmed. Differences from the device: the replay reads the baseline level directly, where the device uses an atomic updated each block, and cross-task queue timing is not modelled.
