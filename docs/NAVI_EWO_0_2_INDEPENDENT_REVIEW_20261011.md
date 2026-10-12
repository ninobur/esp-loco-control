# NAVI_EWO_0_2 (commit cbdfe94): independent architectural and code review

**October 11, 2026. Read-only review: the reviewed firmware, tests and decisions were not
modified.** The probe programs cited below are committed as review evidence in
[`tools/analysis/navi_ewo_0_2_0_3_review_probes_20261011/`](../tools/analysis/navi_ewo_0_2_0_3_review_probes_20261011/README.md).

## 0. Access and provenance

| Item | Status |
|---|---|
| `cbdfe94` | Accessible on `origin/codex/navi-ewo-0-2-20261010`; parent `85a2cf2` (the 0.1 MM045 STOP build). Complete sketch directory and shared headers present. |
| `NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md` | **Not in cbdfe94's history.** It exists only on the `docs/navi-*-20261009` branches. Read from `docs/navi-cto-bubble-decisions-20261009`. |
| `NAVI_EWO_GOVERNING_DOCUMENTS.md`, `NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`, 0104/0109/0116 | Present in cbdfe94. |
| CODEX `NAVI_EWO_DNA_IR_CREDIBILITY_ARCHITECTURAL_REASSESSMENT_20261010.md` | **Not found on any remote branch.** Not reviewed, and its contents are not assumed. |
| Claude October 10 review and script | On `claude/hopeful-edison-jt4qow` (`docs/NAVI_EWO_DNA_IR_CREDIBILITY_REVIEW_20261010.md`, `tools/analysis/navi_dna_ir_review_20261010/reproduce.py`). |

Verification performed:

* `run_tests.sh` with GCC 13: the reconciliation, map, recorder, STOP-overlay and
  IR-configuration tests pass. `test_pulse_observation.cpp` fails to compile under
  `-Werror=misleading-indentation`; without `-Werror` it passes.
* All retained tests that `run_tests.sh` excludes were compiled and run.
* Probe host tests were written against 0.2's `NaviIntegratedCore.h`.
* 0.2's credibility rule was replayed against the October 9 Type-6 timestamps.
* 0.2's encounter detector was replayed on the 16 archived 1 kHz magnet waveforms.
* No Arduino toolchain was available, and no field data for 0.2 exists.

Status vocabulary used below: **VERIFIED** (implemented and checked), **UNVERIFIED**
(implemented, not checked), **MISSING**, **CONTRADICTORY**, **DECISION** (requires an
architectural decision).

---

## Blocking defects (found before the architectural questions)

| # | Defect | Evidence | Status |
|---|---|---|---|
| B1 | **The sketch cannot compile.** New enum values `DnaPending` and `DnaResolved` take IDs 15 and 16, which collide with `ObservationLoss = 15` and `SpatialInvalidated = 16` (`NaviIntegratedCore.h:16-24`). The event-name `switch` in `NAVI_EWO_0_2.ino` then has duplicate `case` values. NSR1 and MQTT event IDs would be ambiguous even if it compiled. | The extracted switch fails with "duplicate case value" ×2 | VERIFIED |
| B2 | **DNA resolution assigns the wrong marker, one behind.** `sequenceMatches(last,a,b)` tests a = `last`, b = `last+dir`. `reachable()` and `resolveLatest(candidate,b)` then give b the identity `last` (`:485-519`). | Probe P2: physical MM028 resolved as MM027. MM029–MM042 are then confirmed one behind by the normal Hall+IR path, with `position_reliable=1`, no withdrawal, never corrected. This recreates the October 9 failure signature through the new mechanism. | VERIFIED |
| B3 | **A mid-interval declaration never confirms anything.** The 0118 first-target exception was removed. The first target now needs ±15% of the full interval measured from the declaration point. `reconcile()` requires `physicalPositionAnchored_`, and `evaluateMissing()` is disabled before the first confirmation. | Probe P1: declared 025→026, MM026 at 106 mm, then MM027–MM030 pass. `mm` stays 025, confirmed=0, missed=0, `reliable=1`, no withdrawal. The retained 0118 test fails. | VERIFIED |
| B4 | **IR pitch changed from 9652 to 10000 µm in the shared header** `firmware/common/IrMovementWire.h`. The IR transmitter (`IR_SCOPE_ESPNOW_TX`, `PulseEventEvidence.h`) takes its pitch from this header. `validConfiguredDistance()` rejects any other pitch. An IR car not rebuilt from this commit makes every Type-5 packet `PACKET_INVALID` (frame lost), and every Type-6 packet `BAD_WIRE`, so the credibility evaluator never runs. No calibration evidence or authorization is cited. 0122 was edited in place, and Decision 0022 (96.52 mm measured) is not addressed. | Code; the retained station-stop test fails on 9.652 | VERIFIED (code). Calibration basis: DECISION |
| B5 | **The AUTO-withdrawal latch fires at most once per boot.** `serviceReconciliationHold()` uses a function-local `static bool reported` that is never reset (`.ino:286`). AUTO admission (`positionKnown`, `.ino:509`) ignores `autoWithdrawalRequired()`, and loopstat republishes `nav_ready=1`. AUTO can be re-enabled without redeclaration, contradicting both the warning text and revised 0109. | Code | VERIFIED |
| B6 | **Core regression coverage was removed.** `run_tests.sh` excludes the retained integrated-core, IR-authority, first-target, PWM-zero and review-regression tests "because they conflict". All five fail against 0.2. The only new test checks that an encounter event is emitted and that a questionable flag is set. Reconciliation, reachability, retrospective identification, withdrawal and spatial timeout are untested. The README is a stale copy of 0.1's ("independently reviewed", wrong branch, PR #9). | Test runs | VERIFIED |

---

## Findings

### 1. Architecture: does 0.2 implement the intended decision model?

The conceptual split in revised 0109 and 0110 is right: a Hall encounter is an observation,
and identity is NAVI's judgment. The `HallEncounter` struct matches it. **The decision
procedure does not.**

* Identity is still decided first by the old target-polarity plus ±15% IR window (the
  "normal" path, `:534-545`).
* DNA runs only when that path fails.
* The ledger is erased at every normal confirmation (`resetReconciliation`).
* DNA judgment uses exactly the last two encounters.

So DNA is a fallback that fires on IR disagreement, not continuous verification. A
wrong identity accepted by the normal path (polarity coincidence inside the window) is
never re-examined; B2's probe shows 15 consecutive markers of this. **CONTRADICTORY**
with the brief, and with revised 0109's own wording ("ordered observations, surveyed
geometry, physical reachability and journey history"): surveyed spacing is not used in
reconciliation at all.

### 2. Magnet identification without IR agreement

**Detection versus identification.**

* Detection is implemented as 70 counts ×2 consecutive raw samples (`observePhysicalHall`).
  Both polarities are latched. **VERIFIED.** But:
  - "Two consecutive samples" are adjacent conversions inside the 5-sample ADC burst
    (`hallTask`), microseconds apart, not two 1 kHz samples as revised 0110 states. The
    temporal filtering is therefore close to single-sample. **CONTRADICTORY** with 0110's
    text.
  - There is no hysteresis and no rearm or same-magnet separation. Any sub-threshold
    sample un-latches the field. Replaying the detector on the 16 archived 1 kHz magnet
    waveforms (September 1 captures) gives duplicate encounters on 2 of 16 passages. The
    burst sampling makes this likely worse. The probe produced 6 encounters from one
    chattering field. The September 18 accepted design (NAVI_SIMPLIFIED, items 4–6)
    separated electrical rearm from physical same-magnet protection for exactly this
    reason. **MISSING.**
* Identification without IR is possible only through `reconcile()`. That requires an
  earlier IR-agreed anchor (B3), and it gives the wrong answer (B2). **CONTRADICTORY.**
* The circular dependency the brief asked about **remains**: after every declaration,
  Hall can correct nothing until IR has first agreed.

### 3. IR credibility

| Requirement | 0.2 | Status |
|---|---|---|
| Pulse timing and continuity | Consecutive Type-6 completion intervals. **A sequence or pulse gap silently resets the baseline instead of marking the span questionable.** | Partial |
| Implausible patterns | An interval ≥1.5× the *single previous* interval, followed by a return within 1.4×. The rationale ("1.4× over a whole marker") is applied per pulse. | UNVERIFIED threshold |
| PWM and movement history | Same-PWM requirement. No evaluation during ramps. | VERIFIED |
| Isolated versus sustained | **Not distinguished.** One 3× interval scores "2 equivalents", which is enough to mark the span questionable. | MISSING |
| Prevents IR veto | A questionable span skips the window and Missed Magnet. Distance is not repaired. | VERIFIED |

Replay of 0.2's rule on October 9 (one span ≈ 31 pulses):

| Session | Marker spans flagged questionable |
|---|---|
| 18:56–18:58 (clean) | 0% |
| **19:31–19:41 (clean: 428 confirmed, 0 missed)** | **29%** |
| Run 1 | 33% |
| Run 2 | 63% |

The log's MQTT omissions break consecutive pairs, so on-locomotive rates would be equal
or higher. In healthy running, roughly 3 in 10 identifications would be diverted from
Hall+IR into the weaker reconcile path. The rule does not separate healthy from failing
IR well enough to carry authority.

Ordering limit: a flag is raised only after the long interval ends *and* a normal one
follows. A stall immediately before a magnet is still judged with unflagged IR (October 9
MM096). Credibility is reset at each encounter.

### 4. DNA: continuous, and able to correct?

| Requirement | Status |
|---|---|
| Preserves contradicting observations | Ledger of 12, but **erased at every normal confirmation** → MISSING in substance |
| Evaluates both polarities | VERIFIED (detection), but opposite-polarity encounters reach reconcile with at most one predecessor |
| Uses accumulated DNA and journey history | MISSING (two encounters only, no spacing) |
| Reconsiders an earlier identification | MISSING (only the latest encounter is ever assigned; earlier entries are never relabelled) |
| Corrects identity when evidence suffices | CONTRADICTORY (B2: corrects to the wrong marker) |
| Avoids earlier whole-route and proximal failure modes | Partial (see below) |

On the last row:

* The loop over all 171 markers is bounded only by elapsed time, so after a long
  unresolved period it becomes effectively map-wide.
* A unique two-polarity match after a single encounter pair is a single-shot decision.
  Nothing later confirms it. That is the "one datum overturns the world view" pattern
  0104 rejected.
* `matches == 1` is sound only if a and b are consecutive, genuine and correctly polar.
  Duplicates (finding 2) and missed Hall readings violate that silently.

### 5. Physical reachability

Reachability is used as an **upper bound only**: "too far to have reached." That is
implemented as a global PWM-band speed table of 200/250/325/425 mm/s (`reachSpeed`). Four
problems:

* There is no **lower** bound ("too soon to be the next magnet"). That is the exclusion
  that would reject duplicates and spurious re-detections. **MISSING.**
* The September 18 accepted design explicitly rejected global speed envelopes. It required
  locomotive-, direction- and interval-specific timing with a permissive fallback.
  **CONTRADICTORY.**
* The bound is anchored to `mm_`, which Missed Magnet may have advanced without updating
  `lastVerifiedUs_`. Candidates are forward-only, so a NAVI that is *ahead* of the truth
  can never be corrected.
* In practice reachability is doing identification work. With only a two-polarity
  pattern, uniqueness comes from how tight the speed bound is, not from DNA. The brief
  says reachability should exclude, not identify.

Margin check: the highest October 9 IR speed was 354 mm/s, and IR undercounts. The
425 mm/s bound is unvalidated.

### 6. Re-anchoring

* Anchors are taken from the encounter (`e.irUm`), not a later packet. The window is now
  tested at the same coordinate, which removes the 0.1 gate/anchor mismatch. **VERIFIED.**
* Minor inconsistency: the timestamp and serial come from the first latched sample, while
  the IR coordinate is from processing of the second.
* `resolveLatest` correctly uses the *latest* encounter as the anchor, but with the wrong
  identity (B2).
* The authoritative position, the observed magnet's identity, its captured IR, and the
  movement since then are distinct fields only for confirmed landmarks. Unassigned ledger
  entries are never given identities retrospectively.
* The spatial-reference blindness found on October 9 is addressed: Hall encounters are
  now evaluated during collection, which has a PWM-banded timeout. The timeout table is
  another global constant set. **UNVERIFIED.**

### 7. Failure behavior: does NAVI report its uncertainty?

* `positionReliable_` stays true throughout DNA-pending, and through B2's wrong
  resolution.
* Withdrawal requires 10 counted events. Counting begins only after two non-normal
  encounters while anchored. Duplicates count. Missed Magnet counts only once pending.
* P1 and P2 show indefinite false confidence with no withdrawal.
* Withdrawal is one-shot per boot and re-admissible without redeclaration (B5).

**CONTRADICTORY** with the objective that NAVI stop reporting false confidence.

The value X = 10 is hard-coded and written into revised 0109. You state X has not been
selected or authorized. **DECISION, and implemented without authorization.**

### 8. Regression: is the STOP sequence preserved?

* `NaviStopOverlay.h` is byte-identical. The `.ino` STOP, acceleration, dwell and restart
  code and constants are unchanged; the diff adds only the withdrawal hook and pulse
  routing. **VERIFIED by diff.**
* Two indirect changes:
  - The pitch change scales the within-interval IR offset by +3.6%, which moves the
    geographic STOP trigger by millimetres.
  - `EwoStationStop.h` pitch constant changed.
* Stale STOP instruction:
  - Execution state (Braking/Dwell/Released) survives a geographic correction.
  - A forward correction across the target before the final section skips that circuit's
    stop.
  - A backward correction after Release, within the footprint, does not re-stop.
  - Nothing invalidates or recomputes on correction.

  With B2 active, DNA corrections leave NAVI one marker behind, so the STOP would land
  about one marker late. **UNVERIFIED; no test exists.**

### 9. Governance

cbdfe94 **edits the governing decisions in place**: 0109, 0110, 0113 and 0116 get
"2026-10-10 revision" sections, and 0122's pitch is changed. It also changes
`NAVI_DECISION_MODEL.md` and the governing index, and its test script calls this "the
approved evidence model." This conflicts with:

1. Your statement that 0109 and 0116 "have not yet been formally revised."
2. The decision-record rule in `docs/CLAUDE.md`: a decision is never edited away; a new
   superseding record is added.
3. AGENTS.md §1–2: architectural changes and thresholds need David's approval.

The original 0109 text (no alternative-position search, no automatic sequence correction,
no CRM threshold) and the original 0116 text (Hall-only advancement withdrawn) remain in
the same files directly below the new paragraphs, so each record now contradicts itself.

The Architectural Note §7 also binds: "must not … permit Hall-only navigation advancement
contrary to Decision 0116." DNA resolution *is* Hall-only advancement.

Required revisions, to be made as new records approved by David:

* 0109 shrug and recovery prohibitions
* 0116 IR authority
* 0118 (first-target exception now dropped)
* 0110 detection definition (and its sampling basis)
* 0113 spatial timeout
* 0122 and 0022 pitch
* the Type-6 observation-only rule (the October 6 specification)
* the September 18 reachability rulings
* X

### 10. Simplicity: the smallest genuinely necessary set

What 0.2 got right and should keep:

* encounter as observation;
* both polarities;
* spatial-phase blindness removed;
* window tested at the anchor coordinate;
* IR veto withdrawn for a questionable span.

What 0.2 adds that has not earned its place:

* the global speed table used as an identifier;
* a credibility rule that flags 29% of healthy spans;
* a ledger that is erased before it can accumulate;
* a hard-coded X.

The smallest coherent correction, for David's decision and not implemented:

1. Same-magnet protection on detection. Rearm hysteresis plus a lower-bound "too soon"
   exclusion. This is prior art from September 18.
2. Keep the encounter ledger **across confirmations**, roughly the last 10 encounters. Run
   one consistency check on **every** encounter, normal or not: incumbent versus ±1/±2
   alternatives, polarity plus credible relative spacing. Identity changes only with
   repeated contradiction of the incumbent and a unique, fully consistent alternative
   confirmed by a later encounter. This replaces both the two-encounter `reconcile` and
   the global reachability table.
3. Restore 0118 for the first target. DNA must not depend on a prior IR-agreed anchor.
4. Credibility that is persistence-based, not single-event, before it gains authority.
   Validate it on track first.
5. `position_reliable=0` while contradicted. Withdrawal latched in the core and checked by
   AUTO admission, reset only by declaration.
6. Revert the pitch, or document its calibration and coordinate the IR transmitter.
   Separately authorized.
7. Unique event IDs. Restore the excluded tests, updated to the new model, rather than
   dropping them.

Stage it as detect-and-report on track with native Hall recording before any automatic
correction is allowed to move NAVI.

---

## Answer to the central question

**No.** 0.2 moves in the right conceptual direction, separating observation from
identity. As committed it does not implement a coherent or reliable decision model:

* It cannot compile.
* Its DNA correction assigns the wrong marker and then self-confirms the error.
* A mid-interval declaration halts identification entirely.
* Its IR credibility rule cannot tell healthy IR from failing IR.
* It still requires IR agreement before Hall can correct anything.
* It continues to report reliable position while wrong.
* It changes the IR calibration and rewrites governing decisions without recorded
  authorization.

Detection, the spatial-phase fix and the preserved STOP sequence are sound foundations.
