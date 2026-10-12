# NAVI_EWO_0_4 revised requirements (October 11 draft): specification audit, revision 2

**Date:** 2026-10-11
**Status:** Read-only audit. No code, governing documents or branches were changed.
**Follows:** `docs/NAVI_EWO_0_4_REQUIREMENTS_AUDIT_20261011.md` (commit `0db254f`), called "audit 1" below.

**Sources:** the same as audit 1. I also read decisions 0113–0115 in full and the 0_1 spatial-reference and boot code (`85a2cf2:…/NaviIntegratedCore.h`, lines 205–222 and 450–500).

**Labels:**

| Label | Meaning |
|---|---|
| [DEC] | Recorded decision |
| [CHAT] | Operator decision stated in the draft but not recorded |
| [IMPL] | Code behaviour |
| [REV] | Review finding |
| [OPEN] | Undecided |
| ⚠ | Needs an operator decision before an implementation prompt |

---

## 0. Overall assessment

This revision is substantially better than the first draft. It now does the following:

- names the median-of-five departure explicitly;
- cites 0118 and 0120 correctly;
- retains 0109 Missed Magnet;
- adds the stall rule, the spatial reference, low voltage, the MQTT contract and the track stage;
- states the 0_1 PWM values;
- defines STOP completion and the rule that a completed stop is not reissued;
- lists the open questions instead of hiding them.

It is still not ready to be converted into an implementation prompt, for four reasons:

1. **One factual misstatement of a governing decision.** The startup reference in §3.2 is described as using five pulse positions. That method is the withdrawn Decision 0114.
2. **Two internal contradictions.** §2.9 conflicts with §5.3, and §9.5 conflicts with its own "do not invent a limit" note.
3. **The supersession is still blanket and unrecorded** (§1.7 and §15.3). In addition, §15.3 now delegates the reconciliation to the implementer.
4. **Several requirements and tests depend on questions the draft itself lists as open.**

---

## 1. Status of audit-1 findings

| Audit-1 item | Status in this revision |
|---|---|
| 1. Median-of-five vs 70×2 | **Decided in the text** (§2.1) [CHAT]. Not yet recorded. |
| 2. 0116 hold vs 650-ms fallback | **Decided in the text** (§5.3) [CHAT]. Not recorded. Creates contradiction C1 below. |
| 3. Hall-only confirmation, re-anchor and STOP | Partly decided (§10.1, §10.7). STOP without IR is listed as open. |
| 4. DNA correction | Decided in principle (§8) [CHAT]. Not recorded. |
| 5. 0109 Missed Magnet | **Resolved:** retained. Its interaction with the fallback is listed as open. |
| 6. DNA resolution criterion | **Open**, and listed. |
| 7. Recovery limit | Decided in part (§9). Terms are still undefined (see §3.6 below). |
| 8. 20-second limit | **Resolved:** removed. |
| 9. Missed-pulse detector and restoration | Restoration **resolved** (§4.7–4.8). Detector given a definition and an acceptance test, but see §3.4 below. |
| 10. Credible IR disagreement | Implicitly resolved: with credible IR, ±15% governs (§5.2). Acceptable if intended. |
| 11. Duplicate-suppression mechanism | **Not addressed.** §2.7 states the goal only. 0_3's mechanisms produced problem cases, per summary A. |
| 12. "10 MM standard length" | **Audit-1 finding withdrawn.** MM means mile markers, not millimetres (see §3.9). Where it applies is still to be stated. |
| 13. PWM values | **Resolved:** 0_1 values (cruise 90) govern (§12.5). Must be recorded, because the arch note says 60. |
| 14. Overlay latch after a correction | Partly resolved (§12.13). Only one direction is covered (see §3.8). |
| 15. Arch-note version; grade ruling | **Not addressed.** |
| 0116 frame-loss redeclaration (audit 1 §2.3.5) | **Dropped.** Not in this revision. |
| 0116 missing-packet bridging (audit 1 §2.3.4) | **Dropped.** |
| 0115 Missed-Magnet origin rule | **Not cited.** |
| 0_3 line head (`4ad1df2` / `c4cd2b2`) for comparison | **Not addressed** (§15.5). |
| Decision record committed before the implementation prompt | **Not addressed.** §15.3 assigns reconciliation to the implementer. |

---

## 2. Factual misstatement

### §3.2: the startup reference

The draft says:

> Startup reference uses five distinct observed IR pulse positions … including the later startup amendment.

The record says otherwise:

- **0114** (five positions) is "Historical and superseded/withdrawn by 0115 on 2026-09-29 … not current NAVI_EWO authority."
- **0115**, the later amendment, says: "At boot, NAVI takes the first nonzero-PWM native Hall ADC observation as its provisional initial reference. No IR movement, acquisition window, five-position collection … is used."
- 0115 withdrew 0114 because it "introduced a demonstrated startup failure mode."
- 0_1 implements 0115: `activeReference_ = sample.raw` on the first observation (core lines 208–211).

**Correction:** "Startup uses 0115's provisional single-sample reference, replaced by the 0113 cycle at 200 mm." If David intends to restore 0114, that is a new decision that reverses 0115 and must say so.

---

## 3. Contradictions, imprecision and unsupported items

### 3.1 C1: §2.9 contradicts §5.3

- **§2.9:** "No IR pulse progression means no measured movement, even when PWM is nonzero."
- **§5.3:** with IR *unavailable*, use the 650-ms fallback.

When IR is unavailable there is no pulse progression by definition. §2.9 then says NAVI is not moving, so a Hall encounter cannot advance position, and §5.3 can never take effect. Read the other way, §5.3 overrides §2.9, and the 0116 failure returns: MM13, MM12 and MM11 confirmed from one sustained field without pulse progression.

The two rules can coexist only with an explicit scope:

- §2.9 applies when IR is applicable (pulses are expected and absent);
- §5.3 applies only when IR is *corrupted but still progressing*, or genuinely unavailable.

If that is the scope, what then prevents the MM13/12/11 failure at nonzero PWM with IR unavailable? ⚠

**Required test (add to §14):** reproduce the 0116 sustained-field regression with the fallback enabled.

### 3.2 C2: §9.5 is unsatisfiable as written

§9.5 requires that NAVI "not silently continue indefinitely when identity remains unresolved." The note forbids inventing a limit. An implementer cannot meet both.

Claude's 0_3 finding [REV] originates in the shell rule that publishes `position_reliable=0` while the overlay keeps its current instruction (0_3 STATUS, shell change 2). That finding stays open until David specifies the termination criterion. ⚠

### 3.3 The supersession is blanket and unrecorded (§1.7, §15.3)

§1.7 ("October 11 operator decisions supersede conflicting provisions") does not name the clauses. The superseded text includes the following:

| Source | Clause |
|---|---|
| 0110 | Median-of-five; two-sample "not carried" |
| 0116 | 650-ms withdrawal; "Hall observations … cannot advance MM" without IR; Hall-only re-anchor withdrawal |
| 0109 | Shrug excludes "identification of another MM" and "automatic sequence correction"; "10-MM AUTO recovery limit" not authorized; no CRM threshold authorized |
| 0104 | Bounded resolution |
| Arch note | §§4, 6, 7: "does not … authorize Hall-only manufactured travel"; cruise PWM 60; §6 "Lost/unreliable IR distinctions … remain open" |

**Why §15.3 inverts authority.** §15.3 ("Reconcile superseded decisions explicitly") as an implementation control would have the implementer decide which clauses survive. Under AGENTS.md §§1 and 7 and the arch note's precedence rule, reconciliation is David's to make and must be recorded before the prompt. Claude's October 10 review §9 already drafted the list of clause revisions.

**Recommendation:** add a requirement that a decision record naming each superseded clause is committed before any implementation prompt is issued. ⚠

### 3.4 Missed-pulse detection (§4.4–4.5 and the acceptance note)

- **The definition is close to the 0_3 detector that `253faa4` withdrew.** "Isolated, unsustained increase in interval inconsistent with normal variation at applicable PWM" resembles the 0_3 rule (interval greater than 1.5× median, bracketed by a return to range). That rule flagged the healthy October 9 control. The draft's acceptance condition correctly forbids this, but gives no threshold, no window and no definition of "normal variation."
- **"At the applicable PWM" is weaker than it reads.** Received PWM is not transmitter-time PWM (0_3 STATUS, limits).
- **The positive class has no ground truth.** The acceptance test needs "corrupted real-world pulse data." The October 9 run-1 and run-2 onset fixtures are *consistent with* missed pulses, but "the real logs cannot distinguish wheel slowdown/slip/binding from an uncounted optical cycle" (0_3 STATUS; `253faa4`). A test against them can show only that the detector fires, not that it fires correctly.
- **⚠ Decide two things:**
  1. how ground-truth corrupted data will be obtained. This is a track-instrumentation question under AGENTS.md §8, and I am not proposing a method.
  2. what 0_4 does if no detector meets the acceptance condition. Is IR then treated as credible, as under 0116?

### 3.5 IR staleness and restoration (§4.7–4.9)

- **§4.9 gives no value.** 0_1 already has `kIrFreshUs = 1,000,000` (line 49); `253faa4` used it for Type-6 expiry. State it.
- **Staleness and corruption need separate restoration rules.**
  - 0116: "A temporary missing packet or stale link only suspends applicability; a later same-frame cumulative counter can bridge the gap."
  - Draft §4.7: restoration happens only at the next verified MM.

  If staleness is treated as corruption, 0116's bridging is lost. ⚠ Specify: is stale IR "unavailable" (bridgeable under 0116) or "corrupted" (restored only at an MM)?
- **The frame-loss rule is missing (dropped since audit 1).** 0116 and 0_1 (`FRAME_LOST_REDECLARE`): a reset, source, scale or order break requires redeclaration. §4.10 does not state it. Add it.

### 3.6 Recovery limit (§9): terms still undefined ⚠

- **"Correction attempt":** a proposed alternative, an accepted correction, or both? Does a 0109 Missed Magnet advance count?
- **"Within 10 mile markers":** counted how? While identity is uncertain, the MM count is itself uncertain. Is it Hall encounters, mapped MMs passed under the incumbent identity, or IR distance?
- **"Controlled stop":** the STOP-sequence overlay at which target, AUTO withdrawal through the existing 0120-style path, or something else? Also needed: the release condition (redeclaration?) and its telemetry state.
- **Recording:** 0109 says no CRM threshold is authorized, so this limit is a new authorized restriction. §13.8 ("no unauthorized restrictions") is satisfied only once it is recorded.

### 3.7 DNA (§8)

- **§8.9 "local alternatives" has no bound.** Claude's October 10 review used ±2 markers. The draft does not say how far "local" extends. ⚠
- **§8.10 "evidence distinguishes the alternatives" is the resolution criterion,** listed as open. With the 11-encounter wait removed (§1.6, §8.5), nothing else gates a correction.
- **§1.6 is moot for an 0_1 build.** The 11-encounter wait exists only in 0_3 (`pendingCount_<11`). 0_1 has no such code, so "remove" has nothing to act on. The real requirement is the criterion in 8.10.
- **Add 0115's origin rule.** "A Missed Magnet advances the mapped target only … The last physically observed/confirmed field boundary remains the origin"; tolerance is ±15% of the *current* interval, not the accumulated distance [DEC 0115]. This governs the retained Missed Magnet.

### 3.8 Re-anchoring and STOP interactions

- **§10.7 leaves the next origin undefined.** If no IR coordinate exists at a verified opening, what is the origin for the next interval's ±15% test? The choices are:
  - keep the last IR-anchored boundary and accumulate, as in 0115;
  - declare the next interval IR-unavailable, so that §5.3 applies again. That option could chain Hall-only confirmations indefinitely.

  ⚠
- **§3 spatial reference vs Hall-only confirmation.** The 0113 cycle is distance-based. 0_1 collects only when `irApplicable && relationshipReliable` (line 219). Specify what happens to the reference cycle after a confirmation without IR. ⚠
- **§3.6 / §2.5 (Hall active during collection) is correct but incomplete.** 0_1 diverts every sample into collection during the phase (lines 219–222), which is the blindness. If Hall stays active, specify whether a qualifying field inside 100–200 mm is excluded from the reference population, aborts the cycle, or is simply ignored. Otherwise the reference can absorb magnet samples. ⚠
- **§12.13 covers one direction only.** A correction can also move position *into* an unexecuted stop footprint (an unintended new STOP) or *past* one (a skipped stop). 0_3 reset the execution latch on revision [IMPL]. ⚠ Specify both directions, or state that a skipped stop is accepted.
- **§7 (0118) and the fallback.** 0118 requires "applicable IR and `traveled > 0`." 0_3 added a Hall/sequential fallback without IR [IMPL]. Under §5.3, does the first target qualify without IR? ⚠
- **Reversal before the first confirmation.** 0118 states that "reverse math is explicitly out of scope" there. §6.6 should carry that limit.

### 3.9 "10 MM standard length" (corrected after David's clarification)

**Correction to audit 1 and the first issue of this audit.** David has confirmed that "MM" means **mile markers**. "10 MM standard length" is therefore a window of 10 mile markers. It is not 10 millimetres, and it is not related to the 9,652 µm/pulse calibration or to CODEX's rejected 10,000 µm/pulse.

Both audits misread it as millimetres:
- audit 1 linked it to the 10,000 µm/pulse calibration;
- this audit's first issue linked it to 0113's superseded first-10-mm startup method.

Both readings are withdrawn. The draft's trailer warning about confusion with 10,000 µm/pulse rests on the same misreading and should be deleted.

**Support for 10 mile markers as a standard length** [CHAT; not recorded]:

| Source | Use of 10 markers |
|---|---|
| NAVI_COHERENCE `SANS_MM_AUTO_LIMIT = 10` (summary A) | 10 consecutive unverified MMs |
| Claude's October 10 review §7 | X = 10 markers, the map's proven global-uniqueness length |
| Draft §9 | The rolling correction window |

**Still unclear:** where the standard applies. ⚠ Decide whether it is only the §9 correction window, or also:
- the unresolved-identity termination criterion (§3.2 C2);
- the DNA evaluation horizon;
- the history bound (§8.2).

**Recommendation:** replace the trailer note with "The standard geographic window is 10 mile markers," and name each requirement that uses it. Record it with the other decisions in §3.3.

### 3.10 Smaller precision gaps

- **650-ms basis.** State whether it is measured opening-to-opening or detection-to-detection. 0_3 used detection-to-detection; the opening is now the landmark.
- **§2.2 and §2.4 are clear now.** The decision is made at the second sample; the landmark is the first. Keep both.
- **§13.2 / §13.4 and arch-note §§5, 9.** Collision release is governed by separation, not by a dwell timer. Carry this forward for when collision is implemented.
- **§15.5.** Name the 0_3 comparison commits: `4ad1df2` (stop integration) and `c4cd2b2` (IR validity review), not only `16c65f6`.
- **§15.** Name the authoritative arch-note version (three divergent unmerged versions exist; `bd0b15f` is the apparent latest). Record the grade-retention ruling, which currently exists only as a code comment at `.ino:735`.

---

## 4. Verification (§14): corrections

**Tests that cannot be specified until an open question is decided:**

| Test | Depends on |
|---|---|
| #6 "local recovery" | §3.7 |
| #7 "recovery-limit enforcement" | §3.6 |
| #10 "Hall-only STOP integration" | The "STOP without IR" open question |
| #4 missed-pulse detection | §3.4 ground truth |

Mark these as blocked rather than required.

**Tests to add:**

- the 0116 sustained-field regression with the fallback enabled (C1);
- the 0115 boot reference;
- frame-loss redeclaration;
- stale-IR expiry and the bridging rule;
- the 0118 first target, including its accepted silent-offset risk;
- a qualifying field inside the 100–200 mm collection window;
- DNA correction into and past a STOP footprint;
- the unchanged MQTT/NSR wire contract.

**Track stage:** retained, which is correct. It should also name the missed-pulse ground-truth question as one of its objectives.

---

## 5. Items requiring operator decision before an implementation prompt

| # | Decision | Draft § |
|---|---|---|
| 1 | Commit a decision record naming each superseded clause (0109, 0110, 0116, 0104, arch note); remove §15.3's delegation to the implementer | 1.7, 15.3 |
| 2 | Correct the startup reference to 0115, or explicitly reverse 0115 | 3.2 |
| 3 | Scope §2.9 against §5.3, and prevent the 0116 sustained-field failure under the fallback | 2.9, 5.3 |
| 4 | Termination criterion for unresolved identity | 9.5 |
| 5 | Recovery-limit terms: attempt, window unit, controlled stop, release | 9 |
| 6 | DNA resolution criterion and the bound on "local" | 8.9–8.10 |
| 7 | Missed-pulse threshold, ground-truth source, and the fallback if no detector passes | 4.4 |
| 8 | Stale IR: unavailable (bridgeable) or corrupted (restored at MM) | 4.7–4.9 |
| 9 | Next-interval origin after a confirmation without an IR coordinate; spatial-reference cycle in that case | 10.7, 3 |
| 10 | Handling of a qualifying field during reference collection | 3.6 |
| 11 | Effect of a correction entering or passing a STOP footprint | 12.13 |
| 12 | First target without IR | 7 |
| 13 | STOP without IR (already listed as open) | 12 |
| 14 | Where the 10-mile-marker standard window applies (§9 only, or also unresolved identity, DNA horizon, history bound) | 9, trailer |
| 15 | Authoritative arch-note version; record cruise 90 and the grade ruling | 12.5, 15 |
| 16 | Duplicate-suppression mechanism | 2.7 |

**Faithful and now well supported** (keep as written, with citations):

- 0118 (§7);
- 0120 (§11);
- the STOP overlay items in §12.1–12.12;
- the operating-integration items in §13;
- the stall rule (§2.9, once scoped);
- the landmark and decision distinction (§2.2, §2.4);
- the 0113 post-confirmation cycle (§3.3–3.5);
- 9,652 µm/pulse;
- the cumulative counter;
- retention of 0109 Missed Magnet;
- the track-test stage;
- the deployment controls.
