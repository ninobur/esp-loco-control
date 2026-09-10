# Response to the NAVI_ONE operator-ruling audit — the NAVI_CTO discrepancy

**Date:** 2026-09-10
**Branch:** `agent/toby-1-13-flash`
**Responds to:** `docs/NAVI_ONE_OPERATOR_RULING_AUDIT_20260910.md`
**Subject asked:** the discrepancy between NAVI_CTO as currently designed
(`docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md`) and the operator's
intended design.

**Status: analysis only.** No firmware was modified. No decision record is
created or amended here. Nothing below authorizes an implementation choice.

---

## 1. Disposition of the audit

I re-derived the audit's confirmed failures from the tree at `15b4ae7` rather
than accepting them. All five stand.

| finding | verified how |
|---|---|
| 1 — 0070 morphology still controls navigation | `NAVI_ONE.ino:1311` routes `kind 2/3` to `unresolvedInterruption()`; `:1379` routes `kind == 1` to `refusedStitched()`; both call `navigator.unresolved()` + `withdraw()` (`:1222-1231`, `:1247-1260`). `stationService()` arms `stopArming` at `:1286-1288`. |
| 2 — tests require the superseded behavior | `tests/run_tests.sh:56` names gate 12 "the interrupted-traversal rule (decision 0070)"; `:60` gate 13 "the archaeology". |
| 3 — accepted median-of-three silently replaced | `MagnetRecognizer.h:215` defines `medianOfFive()`; `:227` is `inline void medianOfThree(...) { medianOfFive(...); }`. |
| 4 — the build explains itself with expired policy | `LocoConfig.h` header says `TARGET: Toby (9950012)` and directs the verifier to expect `NAVI_ONE_1_0X11_FIELDTEST — 9950012`, while the active include is Otto and `NAVI_ONE.ino:95` is `NAVI_ONE_1_0X13_FIELDTEST`. The block still states that a refused stitched waveform stopping the locomotive "is the point". |
| 5 — gate 1 now fails on the correct 500 ms guard | Ran it: 351 records, `misclassified: 1`, `rej=0 t=596692 pk=81 dur=596 ratio=0.426 gap=436 -> TOO_SOON`, **FAIL**. |

Its "presently conform" list also stands: shape is computed after admission and
cannot flip `isMagnet` (`MagnetRecognizer.h:295-323`), and the guard is 500 ms in
both profiles and the recognizer default.

### 1.1 Two things the audit did not have, both load-bearing

**(a) Gate 1's oracle is a previous firmware's opinion, not the railway.**
`replay_survey.cpp:41` reads `rej` **out of the survey log**. That field is the
2026-08-28 recorder's own on-locomotive classification. Gate 1 therefore asserts
"the current recognizer must agree with the 1.13X build", which is a regression
test against a superseded image, not a test of an operator rule. Under accepted
0057/0081 a 436 ms close-to-open gap *is* `TOO_SOON`; the recognizer is right and
the fixture label is wrong. This is the precise mechanism by which the audit's
"the replay oracle embeds the same mistaken interpretation" is true.

**(b) There is no clean image to fall back to.**
`NAVI_ONE_STATION_CURVES_0_3` — the image the NAVI_CTO plan names as the Stage 1
build, and credits with Toby's clean 2 h session of 2026-09-04 — carries the
identical machinery: `stopArming` (`:283`, `:1388-1390`), `unresolvedInterruption()`
(`:1315`, called at `:1413`), `refusedStitched()` (`:1341`, called at `:1481`), and
the same `medianOfThree → medianOfFive` substitution (`MagnetRecognizer.h:227`).

The 2026-09-04 result is therefore evidence that the stitched-refusal path is
**rarely taken**, not that it is absent. "Go back to the image that flew clean"
is not available as a corrective option, and the audit's instruction to treat
`1b8b828` as a comparison point rather than a rollback target is correct for the
same reason at a different point in the lineage.

---

## 2. The discrepancy, stated once

**The operator's design:** position moves on magnet evidence and explicit
navigation state. Morphology — the shape and continuity of the Hall waveform — is
observation only: it may be measured, published and archived, and may not admit
or refuse a magnet, pause, resume or stitch a measurement, alter position,
withdraw authority, or stop the locomotive (0080). CTO consumes NAVI truth; it
may reduce motion authority and may never manufacture it (0077), and it maps NAVI
states onto the frozen wire without redefining them (0078).

**What NAVI_CTO currently is:** a plan whose foundation is the X13 tree taken
*unmodified*; whose acceptance criterion is that X13's own tests keep passing
*unchanged*; and one of whose stated design requirements is that a CTO-imposed
stop must arm the 0070 sentinels so that the passage "pauses and stitches
correctly".

**The discrepancy:** NAVI_CTO does not merely inherit the expired 0070
experiment. It **promotes** it — from a field-test behaviour into a load-bearing
CTO contract and into the definition of a passing build — at exactly the moment
0080 removed all of its authority.

---

## 3. Where the discrepancy lives, item by item

### 3.1 The foundation is declared unmodified, and it is the wrong foundation

§1 of the plan lists `HallCapture.h` (1065 lines) **Unmodified**,
`MagnetRecognizer.h` **Unmodified**, `TwoSided.h` **Unmodified**, `tests/`
"inherited whole and must keep passing". Those are precisely the files carrying
0070's pause/resume/stitch, the proposed 0071/0073 reading construction, the
proposed 0075 discard branch, and the median-of-five substitution.

"Unmodified" was written as a safety property — *we are not perturbing a working
navigator*. After 0080 it reads as the opposite: it is the mechanism by which
retired policy is carried forward without anyone having to argue for it.

### 3.2 §7 makes morphology a CTO requirement — this is the sharpest point

> "A CTO-imposed controlled stop uses NAVI's existing mechanism, and there is no
> second implementation. It must arm `stopArming` exactly as the station machine
> does … so that decision 0070's two sentinels see the stop and the passage
> measurement pauses and stitches correctly."
>
> "**This is the sharpest failure mode in the whole design.**"

Under 0080 that requirement is inverted. A CTO stop must **not** arm a stitch,
because there is to be no stitch. What §7 is actually reaching for — *a
controlled stop must not silently cost a marker* — remains a real requirement,
and 0080 says how it is to be met instead: station arrival/dwell/departure marker
accounting from magnet evidence and explicit navigation state, with the
already-counted / not-yet-counted question answered explicitly rather than hidden
behind a morphology-derived stitch. That accounting does not exist yet in any
build. It is the actual work §7 was standing in for.

### 3.3 The authority boundary is drawn in the right place for the wrong threat

§6.2 forbids CTO to advance position, score candidates, hold an alternative
hypothesis, repair NAVI, or restart a withdrawn locomotive. That is correct and
conforms to 0077.

But the boundary is drawn at the CTO/NAVI line, and **morphology sits inside
NAVI, on the trusted side of it**. A perfectly implemented CTO boundary does not
prevent a Hall-shape heuristic from withdrawing position and stopping the train.
That is exactly what happened twice after Grillers on 2026-09-10, with no CTO
code present at all.

The rule the design is missing is an intra-NAVI one, and the plan has no sentence
that expresses it. Something with the force of: *within NAVI, only magnet
evidence and declared navigation state may reduce motion authority; no derived or
diagnostic quantity may.* **I am not writing that as a decision.** It is offered
as the sentence that would have caught this, for the operator to accept, amend or
reject.

### 3.4 The acceptance criteria certify the retired design

Stage 1 requires "all 19 inherited NAVI tests pass unchanged" — gates 12 and 13
among them — and lists as a field-acceptance condition:

> "Complete station behaviour at every station in both directions: approach, zero
> ramp, dwell, departure, and **correct stitching around each controlled stop**."

After 0080, a correct build must *fail* that criterion. §12 repeats it as a new
CTO test: "a CTO-imposed controlled stop arms `stopArming` and stitches
correctly". So the plan's definition of green is anchored to the design the
operator retired, and "all gates green" could not have caught the Grillers stop —
it would have certified it.

### 3.5 The plan still specifies a constant the operator has ruled against

§14's proposed Otto block contains `NAVI_GUARD_MS 200U`, argued for in §3.4
("proposed 200, margin measured"). Accepted 0057 and 0081 say 500. The active
code is already correct — `LL_LocoConfig_9950011.h:234` is `500U`, restored by
`4e72ff1` — but the plan document still asks for 200 and still contains the
reasoning that produced the unauthorized change. That reasoning should be
withdrawn explicitly rather than left to be silently superseded, because it is
the written form of the error: a 436-438 ms re-read treated as a genuine adjacent
marker without a physical-speed check.

Two other §14 items are settled and should not be re-opened: `IR_FITTED 0` is the
operator's declaration and is in the profile; `NAVI_BASELINE_ADAPT_PWM` is 24.

### 3.6 Stage 1 as flown was not Stage 1 as designed

§11 specifies the Stage 1 image as `NAVI_ONE_STATION_CURVES_0_3` compiled against
Otto's profile, "**no source change**", with two files touched — Otto's profile,
and `LocoConfig.h` "**and update the TARGET/boot-verify comment block with it**".

What was selected for the field test was the X13 tree (`7781951` flips the
include in `NAVI_ONE/LocoConfig.h`; `NAVI_ONE.ino:95` is
`NAVI_ONE_1_0X13_FIELDTEST`), and the comment block was **not** updated — it still
reads `TARGET: Toby (9950012)` and still tells the verifier to expect
`NAVI_ONE_1_0X11_FIELDTEST — 9950012`.

Per §1.1(b) the substitution did not change the defect: both images carry the
same paths. What it changed is the ability to verify what flew. And the plan had
named this exact trap, in this exact file, listing its three prior occurrences —
and the execution walked into it anyway. That is a process discrepancy
independent of the design one, and it is why the audit's finding 4 matters more
than a stale comment normally would.

### 3.7 The blast radius grows with the plan

§10 makes a locomotive with withdrawn position an **unresolved obstruction**:
peers fleet-stop, and no CTO path may restart it — correctly, per 0077. Combined
with §3.3, one morphology-driven withdrawal on one locomotive becomes a fleet
stop from Stage 3 onward.

This intersects the audit's finding 11. The operator has said shutdown is not a
sustainable operating strategy; 0076's `{-1, 0, +1}` hypothesis is proposed and
unimplemented; the active Navigator still follows accepted 0053/0058 — one
disagreement strikes, withdraws, stops. The recovery question therefore has to be
answered **before** Stage 3, not after, because Stage 3 is where its cost
multiplies by the number of locomotives on the railway.

---

## 4. What this response does not do

- It does not modify firmware, tests, fixtures or constants.
- It does not create or amend a decision record. Any of the sentences offered
  above would need to be drafted as *Proposed* and reviewed by the operator
  before it has force.
- It does not cite any past decision as license to act.
- It does not repackage the audit's separate questions (0066-0068, 0071/0073/0075,
  0076, 0079, Otto's entry threshold) into one approval, and neither should the
  corrective work.

## 5. Questions for the operator

These are the NAVI_CTO-specific ones. They sit alongside, not inside, the
audit's list.

1. **Should `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §7, §12 and
   §3.4/§14's 200 ms be withdrawn and re-derived, or amended in place?** Amending
   in place preserves the provenance chain the document exists to carry; a
   rewrite risks losing why each donor choice was made.
2. **Does the intra-NAVI authority rule of §3.3 exist as a rule you want?** If so,
   I will draft it as Proposed and it governs the corrective build; if not, the
   corrective build needs some other way to prevent this class of recurrence.
3. **Where should station marker accounting be settled — before the corrective
   NAVI build, or as part of it?** §3.2 is the design gap that removing 0070
   exposes; it is real work, not a deletion.
4. **Is the recovery question (audit 11 / proposed 0076) to be answered before
   Stage 3?** §3.7 is why I think it cannot wait until Stage 4.
5. **Confirm the corrective sequence's step 9 covers this document's items too** —
   i.e. that the firmware diff, the replays, *and* the amended architecture
   document all come to you together before anything is committed or flashed.

---

## Evidence

- Audit under response: `docs/NAVI_ONE_OPERATOR_RULING_AUDIT_20260910.md`.
- Plan under response: `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §1, §6.2, §7, §10, §11, §12, §14.
- Active tree: `firmware/test-programs/NAVI_ONE/` at `15b4ae7`.
- Fallback image examined: `firmware/test-programs/NAVI_ONE_STATION_CURVES/` (`SKETCH_NAME "NAVI_ONE_STATION_CURVES_0_3"`).
- Gate 1 executed against `field-records/logs/20260828_survey/toby_1_13X_survey_waveforms.log.gz`; result quoted verbatim in §1.
- Governing decisions read: 0053, 0057, 0058, 0064, 0065, 0070, 0074, 0076, 0077, 0078, 0079, 0080, 0081.
