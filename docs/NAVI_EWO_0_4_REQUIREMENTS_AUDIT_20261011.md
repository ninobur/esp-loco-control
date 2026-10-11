# NAVI_EWO_0_4 requirements: independent specification audit

**Date:** 2026-10-11
**Status:** Read-only audit. No code, governing documents or branches were changed, and nothing was committed.

**Labels used throughout:**

- **[DEC]** Established operator decision recorded in the repository.
- **[CHAT]** Stated as established in the October 10–11 summaries or the 0_3 prompt, but not found in any decision record.
- **[IMPL]** Historical implementation behavior (0_1 or 0_3 code).
- **[REV]** Review finding.
- **[PROP]** Proposed requirement, not yet approved.
- **[OPEN]** Explicitly undecided in a governing document.

---

## 0. Sources examined and repository conditions

### Sources examined

| Source | Location |
|---|---|
| EWO governing directory and decisions 0109–0122 | `85a2cf2:docs/NAVI_EWO_GOVERNING_DOCUMENTS.md`, `docs/decisions/` |
| Decision 0123 | Branch `docs/navi-stop-overlay-20261009` |
| October 9 "mandatory primary architectural guide" | `docs/NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md`, branch `docs/navi-stop-overlay-20261009` @ `bd0b15f` (the arch note) |
| 0_1 | `85a2cf2:firmware/programs/NAVI_EWO_0_1_MM045_STOP/` (`NaviIntegratedCore.h`, `NaviStopOverlay.h`, `.ino`) |
| 0_3 | `16c65f6` and its successors `253faa4`, `c4cd2b2`, `4ad1df2`. Includes `STATUS.md`, `IR_VALIDITY_CORRECTION.md`, `STOP_HANDOFF_CORRECTION.md` and the core headers. |
| Claude October 10 pre-implementation review | `claude/hopeful-edison-jt4qow` @ `f3084b6`, `NAVI_EWO_DNA_IR_CREDIBILITY_REVIEW_20261010.md` |
| October 10–11 summaries (A = findings, B = reconstruction) and the 0_3 prompt | Supplied in this conversation |

### Repository conditions affecting this audit

These are reported, not fixed.

1. **`main` is not current.** Its last commit is 2026-09-25 (`7409c19`). The current project state lives on unmerged feature branches.
2. **The arch note exists on unmerged branches only, in at least three divergent versions:**
   - `docs/navi-ewo-0-1-architecture`: 158 lines
   - `docs/navi-architecture-reconciliation-20261009`: 240 lines
   - `docs/navi-stop-overlay-20261009`: 445 lines

   The note is not in `85a2cf2` or in 0_3. I used `bd0b15f`, which appears to be the latest. **David should confirm which version is authoritative.**
3. **0_3 departs from the governing decisions on the strength of an implementation prompt.** `0_3/STATUS.md` says: *"The October 11 assignment supplies implementation authority and supersedes the older target-only/mandatory-IR rules for this isolated candidate. Existing numbered governing records have not been edited."* No decision record supersedes 0109, 0110 or 0116.
4. **Corrective commits exist after `16c65f6`.** All three are dated 2026-10-11:

   | Commit | What it does |
   |---|---|
   | `253faa4` | Stale-IR expiry; enforces the ±15% upper edge; withdraws authority from the timing detector |
   | `c4cd2b2` | IR validity requires the current time |
   | `4ad1df2` | Hands Hall-only accepted geography to STOP |

   This contradicts summary A's statement that "no corrective implementation was approved or performed." The audit assignment names only `16c65f6`.
5. **The October 11 Claude and CODEX review reports for 0_3 were not found** on the 0_3 branches or the October 10 Claude branch. Review findings below are taken from summary A and from the correction documents that cite them.

---

## 1. The central issue: the draft rests on unrecorded reversals of governing decisions

The arch note (§ "Governing authority") says:

> Compatible earlier decisions remain binding, particularly Decisions 0116 and 0120 … do not restore superseded mechanisms by relying on an older record. Any proposed departure requires David's explicit approval before implementation.

The draft 0_4 list, summary A, summary B and the 0_3 prompt all include mechanisms that the recorded decisions withdraw or exclude:

| Draft item | Recorded decision | Conflict |
|---|---|---|
| §2: 70 counts × two consecutive 1-ms samples | **0110**: Hall statistic is a NAVI-owned *rolling median of five*; "Two-sample persistence is not carried." 0_1 implements median-of-five (`NaviIntegratedCore.h:233`). | Direct |
| §4: 650-ms fallback when IR invalid | **0116**: "The 650-ms Hall-only fallback and degraded Hall re-anchor are withdrawn for EWO." It withdraws 0111's degraded mode. 0120 says it "stays withdrawn." | Direct |
| §4, §9: Hall confirmation, and STOP, without IR distance | **0116**: confirmation requires ±15% IR distance; "Without applicable IR distance, NAVI holds its last established MM … Hall observations continue but cannot advance MM." The arch note §§4, 6 and 7 restate that Hall-only advancement is not authorized. | Direct |
| §6: DNA evaluation of non-target and opposite-polarity encounters, plus retrospective correction | **0109**: the "shrug". Excluded: "identification of another MM", "automatic sequence correction", "alternative-position search". | Direct |
| §7: correction-count limit | **0109**: "Not authorized: … the 10-MM AUTO recovery limit." The CRM challenge has "No threshold, count or automatic mechanism … authorized." | Direct (new mechanism) |
| §6: "do not introduce speculative missing-magnet recovery" | **0109**: IR-based Missed Magnet is "the only recovery mechanism currently authorized." 0_1 implements it (`evaluateMissing`, lines 502–513). 0_3 removed it ("No IR-only `evaluateMissing()` progression remains"). | Ambiguous. The draft does not say whether 0109's Missed Magnet survives. |

**These are two separate questions, and the second one matters more.**

1. *Did David decide these departures?* Summary A says the Hall requirements were "established" on October 11, and the 0_3 prompt states them as "Established requirements." So they may well be his decisions.
2. *Are they recorded?* No. Under AGENTS.md §7 and the arch note's precedence rule, a 0_4 prompt built on them would contradict the repository's governing record. A future agent reading the record would be told to follow 0109, 0110 and 0116.

Claude's October 10 review (§9) already listed the specific revisions that would be needed:

- 0109 reversed in part;
- 0116 given a credibility qualifier;
- 0104 given a bounded resolution mechanism;
- Type-6 observation-only status revised;
- 0118's accepted limit noted as detectable.

**Recommendation:** before the 0_4 implementation prompt is issued, David either records a decision that supersedes the named clauses of 0109, 0110, 0111/0116 and 0104, or the draft reverts to them. This audit does not choose.

---

## 2. Corrected requirements list

Bracketed labels give the source status. ⚠ marks an item that needs an operator decision before it can go into an implementation prompt.

### 2.1 Foundation and governance

1. Build NAVI_EWO_0_4 from `NAVI_EWO_0_1_MM045_STOP` at `85a2cf2`. This is also the **rollback point** (AGENTS.md §3). [CHAT; the 0_1 commit is the established operational foundation]
2. 0_3 is not the foundation. Its modules may be consulted as **reviewed code, not proven function**: 0_3 was never flashed and has no field evidence.
   - Compare against the 0_3 *line head* (`4ad1df2`; `c4cd2b2` on the review branch), not only `16c65f6`.
   - The later commits contain corrections for three reviewed defects: stale IR, the ±15% upper edge, and the STOP handoff.
3. Governing documents, in precedence order (arch note precedence rule):
   1. the arch note (version to be confirmed ⚠);
   2. decisions 0109–0122, as amended by any new decision under §1;
   3. implementation specifications;
   4. history.
4. NAVI is the sole navigation authority. STOP and overlays consume NAVI's position through a read-only interface and do not create a second position estimator. [DEC 0102; arch note §§2, 7]
5. "Remove obsolete machinery" must list what is removed. As written it could remove 0109 Missed Magnet, the 0113 spatial reference or the 0120 hold. ⚠ [PROP; insufficiently precise]

### 2.2 Hall detection

1. ⚠ **Detection statistic: choose one.** Either median-of-five ≥70 counts [DEC 0110; IMPL 0_1], or raw ≥70 counts on two consecutive native 1-ms samples [CHAT; IMPL 0_3]. Record the choice.
2. **Landmark and decision point are different events.** The *landmark* is the earliest raw qualifying sample, the "leading field boundary" [DEC 0110/0113]. It carries the polarity, timestamp and IR coordinate. The *decision* occurs at the second qualifying sample (or at the median crossing).
   - The draft's "Detection occurs on the second qualifying sample" together with "preserve … timestamp" does not say which timestamp is the anchor.
   - §2.8 requires the opening.
3. Waveform morphology has no navigation authority. [DEC 0110, 0080]
4. One physical field produces at most one encounter. [CHAT; REV]
   - **Mechanism undecided ⚠.** 0_3 uses a continuous-field latch, an 85% lower-edge exclusion and the 650-ms fallback. Summary A says both the exclusion and the fallback produced problem cases. `253faa4` states it "does not solve the stationary finding."
5. **Add, omitted from the draft: the spatial Hall reference.** The reference is spatial and is replaced from the leading-field boundary [DEC 0113, 0115].
   - Hall observation must stay active during spatial-reference collection [CHAT 0_3 prompt].
   - Claude's October 10 review §8 found that the IR-gated reference phase blinds Hall and caused run 1's first miss.
6. At motive PWM = 0, Hall observations are recorded and are non-actionable. [DEC 0106/0112]
7. **Add, omitted from the draft: the stall rule.** "No pulse progression means no measured movement; it is provisionally STOPPED, even at nonzero PWM." [DEC 0116]
   - This bears on Claude's stalled-magnet finding [REV].
   - The draft's PWM = 0 rule does not cover a stall at nonzero PWM.

### 2.3 IR measurement and credibility

1. The installed pitch is 9,652 µm/pulse. Type-5 pitch is configuration, and calibration metadata has no runtime authority. [CHAT; DEC 0122]
   - CODEX's 10,000 µm/pulse proposal was not authorized [CHAT, summary A].
2. **Delete or define "Preserve 10 mm as the standard length."** I found no source for it in 0_1, 0122 or the summaries.
   - It risks being read as the rejected 10,000 µm/pulse.
   - Note that 0_1's STOP geometry uses *fractions of the surveyed interval, "not a fixed number of millimetres"* (`NaviStopOverlay.h:8`). ⚠ [unsupported]
3. Preserve the cumulative counter. Never invent pulses or edit counts. Health is observation, "not authority to erase coherent cumulative distance." [DEC 0116; CHAT]
4. A missing packet or stale link suspends applicability. A later same-frame cumulative counter bridges the gap. [DEC 0116]
5. A genuine reset, source, scale or order break, or IR displacement at PWM = 0, breaks the map relationship and requires redeclaration. [DEC 0116, 0120]
6. ⚠ **"One credible missed pulse withdraws IR credibility" has no established detector.**
   - `253faa4` found that the 0_3 timing detector flags the *healthy* October 9 control, and cannot distinguish uncounted optical cycles from wheel timing variation. It removed the detector's authority: "Genuine physical missed pulse invalidates IR: **Not demonstrated; unresolved**."
   - Summary B §9 says the algorithm was never recovered.
   - The arch note §6 leaves "Lost/unreliable IR distinctions" **[OPEN]**.
   - The requirement is valid as intent. The mechanism needs a decision or a track-instrumentation study (AGENTS.md §8).
7. ⚠ **Three different restoration rules exist; pick one.**

   | Source | Rule |
   |---|---|
   | Draft | One subsequent qualifying pulse restores credibility |
   | 0_3 prompt | Credibility returns after a verified re-anchor, when subsequent measurements are credible |
   | `c4cd2b2` | A fresh five-interval population restores it; the first resumed event does not inherit authority |

8. Credibility expires when observations cease. The existing `kIrFreshUs = 1,000,000` (present in 0_1 at line 49) was reused by `253faa4` for Type-6 expiry [IMPL]. The value must be stated in the requirement, not left implicit.

### 2.4 Magnet qualification

1. With applicable, credible IR: the encounter must fall within ±15% of the mapped interval, measured at the opening coordinate. [DEC 0110/0116; IMPL `253faa4` enforces both edges]
2. ⚠ **Without applicable IR: 650-ms fallback [CHAT], or hold [DEC 0116]?** See §1.
3. Corrupted IR must not veto a legitimate Hall encounter. [CHAT]
   - **Dependency the draft does not state:** "corrupted" can only be recognized by 2.3.6, which has no working detector.
   - With rule 1 enforced at both edges and no detector, the October 9–10 failure returns: undercounted IR rejected genuine Hall magnets. **Flag as a functional contradiction inside the draft.**
4. Credible IR disagreement: what NAVI does with *credible* IR that conflicts with an otherwise qualified Hall encounter is **[OPEN]** (summary A, question 9D). Claude's finding was that it is "calculated but not used" [REV].
5. All Hall encounters are retained in the evidence record. [DEC 0109 "preserve the evidence"; 0072]

### 2.5 Sequential identification, declarations and reversals

1. The next target is the next mapped MM in the current direction, judged on Hall support, polarity, IR (per §2.4), direction and context. [DEC 0109/0110]
2. Never search all 171 magnets. [DEC 0109]
3. **First target after declaration: cite 0118 explicitly.** Hall onset after an observed absence, positive applicable IR travel, no distance window and no Missed Magnet ruling. [DEC 0118]
   - Carry its **accepted risk**: if the first magnet is undetected, the next same-polarity magnet is accepted silently, one or more markers off.
   - ⚠ Decide what 0_4 does without usable IR here. 0_3 adds a "Hall/sequential fallback" [IMPL] that 0118 does not contain.
4. Operator declaration is authoritative and clears incompatible evidence. [DEC 0109]
5. **Reversal:** keep the IR/map frame; capture the latest cumulative counter as the directional origin; select the target in the new direction, including a second reversal before a marker. [DEC 0116; IMPL 0_1 `reverse()`]
   - CODEX found wrong target selection after two reversals *without IR* in 0_3 [REV].
   - Under 0116, "without IR" means hold. The case exists only if §1 is decided toward Hall-only operation.

### 2.6 Magnetic DNA and recovery (entirely ⚠ under §1)

1. Evaluate each encounter against the expected sequence. If it agrees, continue; if it contradicts, mark position unreliable and reopen judgment. [CHAT; PROP against DEC 0109]
2. **"Remove the vestigial 11-encounter waiting requirement" is mislabelled.**
   - `pendingCount_ < 11` (0_3 `NaviIntegratedCore.h:620`) is 0_3's only resolution criterion, not vestigial code.
   - Claude's October 10 review derived 10 markers as the map's proven global-uniqueness length.
   - Removing it without a replacement leaves "correct only when evidence supports it" with no definition of "supports". ⚠ **Specify the resolution criterion.**
3. State the ledger bound. 0_3 uses 12 entries [IMPL]; summary A states 12; the draft omits it.
4. ⚠ **Missing physical magnet: three options, and the draft picks none.**
   - (a) Keep 0109's IR-based Missed Magnet [DEC; IMPL 0_1].
   - (b) Operator redeclaration only.
   - (c) No handling.

   The draft's "do not introduce speculative missing-magnet recovery" does not choose between (a) and (b). Both reviewers' "missing magnet" finding [REV] arises partly because 0_3 *removed* (a).
5. A retrospective correction must state its effect on the STOP overlay.
   - 0_3 resets the overlay's execution latch on revision [IMPL, STATUS shell change 1].
   - That interacts with arch note §13.7 ("completed overlay cannot reissue"). ⚠

### 2.7 Uncertainty and recovery limits (⚠ new mechanism; not supported by any record)

1. The draft's rule: 3 correction attempts in a rolling 10-MM window trigger a controlled stop. [PROP]
   - It is excluded by 0109's wording unless that decision is superseded.
   - It differs from summary B, which gives 10 unverified MM or 20 s and calls them "approved."
   - It differs from Claude's October 10 candidate: withdraw AUTO when identity stays unresolved for X = 10 markers.
2. **Gap: the rule does not cover unresolved identity with no correction attempt.**
   - That is exactly Claude's 0_3 finding (AUTO runs indefinitely on stale instructions [REV]).
   - 0_3 STATUS shell change 2 is the cause: `position_reliable=0` without AUTO withdrawal, and the overlay retains its current instruction.
   - A correction-count limit never fires if no correction is attempted.
3. ⚠ **Undefined terms:**
   - "correction attempt" (proposed, accepted, or both?);
   - "controlled stop" (STOP-sequence overlay, AUTO withdrawal via the 0120 path, or E-stop?);
   - release condition (redeclaration? as in 0120?);
   - whether the window is counted in observed or mapped markers.
4. **The 20-second timeout:** summary B calls it established; the draft calls it "proposed" and removes it. ⚠ **Resolve which is true and record it.**
5. Keep 0120 unchanged: measured IR displacement at PWM = 0 means hold navigation, withdraw AUTO, report `PWM_ZERO_MOVEMENT_REDECLARE`, and only a declaration clears it. Zero-displacement dwell preserves localization. [DEC 0120] The draft omits this entirely.

### 2.8 Re-anchoring

1. Every verified magnet re-anchors. The IR coordinate is the latest valid same-boot snapshot at or before the opening; a later packet is never substituted. [CHAT; DEC 0110/0113; IMPL 0_3]
2. ⚠ **A magnet verified with no IR coordinate at the opening** (0_3: "MM identified but fine IR coordinate unavailable"):
   - 0116 forbids Hall-only re-anchoring.
   - The draft's §8 says every verified magnet re-anchors.
   - Specify one of the two.
3. Preserve the cumulative counter. [DEC 0116]

### 2.9 STOP overlay

All behavior in this section is preserved as demonstrated in 0_1. Arch note sections are cited; field basis is October 9–10.

1. **STOP sequence:** DECELERATE → STOP → WAIT → RESTART → ACCELERATE. It is assignable to any Lowline target, can be entered anywhere, and has no ARMED or admission latch. [arch note §§3–4, §13.3/13.5]
2. The profile is built backward from the target, with per-pulse-position PWM. The existing `requestPwm()`/`serviceRamp()` path is the sole executor, and its time-based ramp continues without further IR pulses. [arch note §§4, 6, 10]
3. ⚠ **PWM values: the arch note and 0_1 differ.**

   | | Arch note | 0_1 |
   |---|---|---|
   | Base cruise | 60 | 90 (`NAVI_BASE_CRUISE_PWM = 90`, `.ino:57`) |
   | Approach | creep 50 → final 20 | 60 → 50 → 35 → final ramp (`NaviStopOverlay.h:65–74`; commit `0fdc6c9`, "50-to-35 … 300 ms final ramp") |

   The draft says "preserve demonstrated 0_1", which implies the 0_1 values. Record that the 0_1 field-calibrated values supersede the note's initial values.
4. **Completion (add):** applied PWM = 0 starts the dwell (5,000 ms, `kStopDwellMs`). The stop is complete only when the dwell has expired *and* applied PWM is still zero. There is no IR rest test and no position gate. [arch note §5, "Resolved by David"]
5. **Accuracy (add):** the target is not a completion gate. Early and late stops are accepted and recorded, with no corrective repositioning. [arch note §5]
6. **Release and departure (correct the wording):**
   - The completed overlay retires.
   - The established ramp resumes regardless of where the locomotive stopped.
   - NAVI then follows the next applicable tile.
   - The completed overlay must not reissue the same stop. [arch note §§4, 5, 13.7]

   The draft's "automatic restart governed by the next geographic tile" merges restart with tile authority. Whether departure-ramp authority holds until the next tile boundary is **[OPEN]** (arch note open question 5).
7. **Grade compensation:** preserved [IMPL 0_1; `.ino:735` comment "grade settings (David, 2026-10-09)"]. This ruling exists only as a code comment and is not in a decision record.
8. ⚠ **"Correct operation when Hall geography is established but IR distance is unavailable"** conflicts with 0116 and is **[OPEN]** in arch note §6 and open question 3.
   - `4ad1df2` implements an MM-resolution fallback under an implementation assignment.
   - This is a design decision, not just a defect fix (CODEX's Hall-only STOP finding [REV]).
9. A collision hold releases on separation, not on a dwell timer. [arch note §§5, 9]
10. Record the still-open questions from arch note §14 as open, not silently resolved:
    1. stop-progress and consumption representation;
    2. priority below collision;
    3. how the deceleration behaves across IR-position gaps;
    4. field profiles;
    5. departure-ramp authority.

### 2.10 Operating integration

1. Preserve Manual, AUTO and STOP/GO admission, MQTT command responsiveness, telemetry, E-stop, **low-voltage protection** (omitted from the draft; 0_1 `.ino:251, 659–670`), and the sole actuator path. No controller may compete for PWM. [arch note §10]
2. Exactly one overlay commands PWM at a time. Collision has highest priority *when implemented* (arch note §13.2). The lower-priority order is **[OPEN]**; do not invent it.
3. NAVI's accepted identity reaches STOP. The *form* of that hand-off when IR is unavailable is §2.9.8 ⚠.
4. **MQTT constraint (add):** existing topics and wire layouts are unchanged. Any new fields need David's sign-off (`docs/CLAUDE.md` MQTT constraint). 0_3 added event IDs 17–22 and NSR input kind 6.
5. **Stop logging (add):** per arch note §11 (target, entry tile, assigned and applied PWM, observed stop coordinate, dwell timing, release authority).
6. No unapproved governors, braking limits or restrictions. Note that §2.7, if approved, is a restriction and needs explicit approval under this same rule.

### 2.11 Verification

Keep the draft's list, and correct or add the following:

- **Remove** "three-correction limit" unless §2.7 is approved.
- **Add tests for:**
  - the 0120 PWM-zero IR displacement hold and redeclaration;
  - zero-displacement dwell preserving localization;
  - the 0116 stationary sustained-field regression (MM13/12/11);
  - a stall at nonzero PWM;
  - the spatial reference phase not blinding Hall;
  - the 0118 first-target behavior and its accepted limit;
  - no reissue of a completed stop;
  - early and late stop acceptance;
  - low voltage;
  - the unchanged MQTT/NSR wire contract.
- **Add the track stage (AGENTS.md §8):**
  - Host tests establish coded behavior only.
  - October 9–10 logs lack raw two-polarity Hall, so DNA cannot be replayed (Claude, October 10, §4; 0_3 STATUS).
  - Require native Hall NSR1 recording and an operator-named-marker track protocol before any field claim.
  - The missed-pulse question (§2.3.6) needs instrumentation, not a synthetic test.

### 2.12 Implementation controls

Keep the draft's controls and add:

- a named base (`85a2cf2`) and rollback;
- the confirmed arch note version;
- any new decision record from §1, *committed before the implementation prompt*;
- a requirement-by-requirement compliance report against this list's labels;
- no merge, flash, Pi or live-service change;
- no edits to governing records in place.

---

## 3. Traceability summary: items requiring operator decision

| # | Item | Draft § | Conflict or source |
|---|---|---|---|
| 1 | Supersede or retain 0110's median-of-five | 2 | 0110 vs CHAT/0_3 |
| 2 | Supersede or retain 0116's hold vs the 650-ms fallback | 4 | 0116 vs CHAT |
| 3 | Hall-only confirmation, re-anchor and STOP when IR is unavailable | 4, 8, 9 | 0116; arch note §6 [OPEN]; `4ad1df2` |
| 4 | DNA identity and retrospective correction | 6 | 0109 exclusions |
| 5 | Keep 0109 IR Missed Magnet? | 6 | 0109; 0_1 vs 0_3 |
| 6 | DNA resolution criterion replacing `pendingCount_ < 11` | 6 | 0_3 IMPL; no record |
| 7 | Recovery and uncertainty limit: form, terms, coverage of no-correction uncertainty | 7 | 0109; summary B; Claude October 10; no record |
| 8 | Status of the 20-second limit | 7 | Summary B vs draft |
| 9 | Missed-pulse detector and credibility restoration rule | 3 | 253faa4 "unresolved"; three rules |
| 10 | Credible IR disagreement handling | 4 | Summary A 9D [OPEN] |
| 11 | Duplicate-suppression mechanism | 2 | Summary A 9A [OPEN] |
| 12 | "10 mm standard length" | 3 | Unsupported |
| 13 | PWM values: arch note (60/50/20) vs 0_1 (90/60–50–35) | 9 | Arch note vs IMPL |
| 14 | Overlay latch reset after a geographic correction | 6, 9 | 0_3 IMPL vs arch note §13.7 |
| 15 | Authoritative version of the arch note; record the grade ruling | 12 | Repository condition |

**Items that are faithful and well supported** (no change other than adding a citation):

- 9,652 µm/pulse;
- the cumulative counter;
- no invented pulses;
- ±15% with credible IR;
- re-anchoring at the opening;
- no 171-magnet search;
- morphology without authority;
- PWM = 0 Hall non-actionable;
- the backward-built, any-target, any-entry STOP;
- the final ramp without IR;
- the 5-second dwell at applied zero;
- one overlay, with collision highest;
- the deployment prohibitions.
