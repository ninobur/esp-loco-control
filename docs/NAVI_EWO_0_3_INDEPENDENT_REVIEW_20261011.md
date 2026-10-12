# NAVI_EWO_0_3: independent adversarial review

Commit `16c65f61662c358923c780a2edffda9733f0a8da` (branch `codex/navi-ewo-0-3-complete`).
Foundation: `85a2cf2`. Read-only review, October 11, 2026. The reviewed firmware, tests and
decisions were not changed. Probe programs are committed as review evidence in
[`tools/analysis/navi_ewo_0_2_0_3_review_probes_20261011/`](../tools/analysis/navi_ewo_0_2_0_3_review_probes_20261011/README.md).

This report is on branch `claude/hopeful-edison-jt4qow`, which is based on `main`, not on the
reviewed branch. File and line references refer to commit `16c65f6`.

## Executive verdict: FAIL for controlled field testing as committed. Correctable.

0.3 is a large improvement on 0.2 and on 0.1's failure mode:

* 70×2 detection is sound.
* Opening-boundary IR association is correct.
* The ±15% IR-window cascade from October 9 is gone.
* The first-target-after-declaration rule is restored.
* The STOP overlay is untouched.
* Unresolved contradictions are reported honestly as `position_reliable=0`.

Four defects block a controlled field test:

1. **C1.** A field hovering at threshold under a stalled locomotive reproduces the
   0116 false-advance failure.
2. **C2.** A single missed Hall magnet can never be resolved.
3. **C3.** While identity is pending, AUTO keeps running indefinitely at a frozen PWM,
   with no STOP. This removes 0.1's existing withdrawal.
4. **C4.** Credible IR has no identification authority, contrary to the October 11
   assignment.

Each correction is small (see the Required corrections section).

The ESP32 build was **not independently verified**. The toolchain install was stopped at
the operator's request.

---

## Critical findings

### C1. Stalled field at threshold re-qualifies as successive magnets (0116 failure class). CONFIRMED (host probe)

**Where:** `NaviEvidenceCore.h:30,38-42`; `NaviIntegratedCore.h:250-263, 277-287`.

**Defect:**

* Any single sub-threshold sample un-latches the field (`:30`), with no hysteresis.
* When IR credibility is false, the only same-magnet protection is 650 ms between
  detections (`:41`).
* Credibility is false whenever Type-6 timing is unobservable. That includes after every
  PWM change, which resets the population (`NaviIntegratedCore.h:313-316`). STOP
  deceleration changes PWM every 300 ms.
* The coherent Type-5 counter showing zero movement is not consulted.

**Evidence:** probe E. NAVI confirmed MM030, then stalled at PWM 90 on MM030's field with
raw readings alternating 72/72/68 counts of departure, and no IR progress. Within 4 s it
advanced 030 → 031 → 032 with `reliable=1`. It became pending only when target 033's
polarity disagreed.

0.3's own test (`test_navigation.cpp:186-189`) holds a constant above-threshold field, so
the field never un-latches. It does not exercise this path.

**Realistic scenario:** stopping or stalling on a magnet with the reading near threshold.
Both have been seen:

* Toby run 3, event #65: −70 at threshold, PWM 24, 720 ms after the previous detection,
  while stopping over MM053.
* October 9 third stop: PWM 31 with no progress.

**Consequence:** false identity advances while reporting reliable. These are exactly the
MM13 → 12 → 11 false advances that 0116 withdrew the 650 ms fallback over.

**Simplest correction:** do not accept a new encounter while fresh, same-frame Type-5 shows
zero completed pulses since the previous accepted opening. Zero movement means no new
magnet. This uses existing data and adds no threshold.

**Residual risk (operator decision):** the IR car stalling for an entire interval while
the locomotive moves. Run 1's 1.1 s stall was shorter than one interval at cruise.

### C2. A single missed Hall magnet is permanently unresolvable. CONFIRMED (probes A and C)

**Where:** `NaviIntegratedCore.h:606-637`.

**Defect:** `resolvePending` tests exactly two hypotheses about the leading
contradiction:

* it was an extra encounter, so the following encounters start at the pending target; or
* it was the real target with the wrong polarity, so the following encounters start at
  target+1.

When NAVI is one behind because a magnet went undetected, the following encounters start
at target+2. Neither hypothesis fits, the 10-pole uniqueness rule rejects both, and after
12 entries (`:608`) resolution is never attempted again.

**Evidence:**

* Probe A (MM027 absent): pending from MM028 onwards, `mm` frozen at 026 through MM045 and
  beyond.
* Probe C (MM031 absent, MM032 the same polarity as 031): silently confirmed as 031 with
  `reliable=1`, then pending at MM033 for good.
* The asymmetry is real. A spurious *extra* encounter (probe D) resolves correctly after
  10 markers.

**Bound on silent false confidence:** limited by map polarity. A one-marker slip is
contradicted after a median of 1 marker, 7 at most, given complete observation. This is
much better than October 9's 18 markers.

**Not covered by tests:** no test covers a missed physical magnet. `test_navigation.cpp:133-153`
covers only the two hypotheses that are implemented.

**Consequence:** honest but permanent loss of navigation after one Hall miss. Every
magnet excluded by the 650 ms or 85% guard becomes such a miss.

**Simplest correction (DECISION):** add a third, journey-local hypothesis: one unobserved
marker, so the following encounters start at target+2. It stays sequential and needs no
map search. The October 11 assignment's "do not invent unobserved geographic markers" and
0109 bear on it, so it needs operator approval. The alternative is to accept
redeclaration as the remedy and say so.

### C3. Pending identity leaves AUTO running indefinitely at a frozen PWM, with no STOP. CONFIRMED (code)

**Where:**

* `NAVI_EWO_0_3.ino:761-764`: the 0.1 rule "unreliable → withdraw" now exempts
  `geographicJudgmentPending()`.
* `NaviStopOverlay.h:194-196`: with geography unavailable, `out.pwm = currentCommand`.
* `.ino:770`: `cruisePwmAt(navi.mm())` uses the stale `mm`.

**Defect:** combined with C2, pending can last for ever. AUTO keeps the PWM in force when
pending began.

**Scenario:** a contradiction during the 11-marker STOP footprint.

* If pending begins in the Approach or Penultimate section, the locomotive holds PWM 35–55
  indefinitely, past the stop target, lap after lap. October 9's third stop showed PWM 31
  stalling.
* If pending begins at cruise, it runs at 90 and never stops.

**Consequence:** loss of the established STOP behavior, and a running locomotive with
unknown position that only the operator can halt. STATUS presents this as "without adding
an AUTO withdrawal threshold." In fact it **removes** 0.1's existing withdrawal, which the
assignment did not authorize.

**Simplest correction (DECISION):** restore 0.1's withdrawal of AUTO on unreliable
position. That is preservation, not a new threshold. Alternatively, approve an explicit
bound X.

### C4. Credible IR has no identification authority. CONFIRMED (code)

**Where:**

* `NaviSequentialJudgment.h:33` computes `irDisagrees`.
* `NaviEvidenceBridge.h:37` copies it.
* **Nothing reads it**: the core never consumes `ruling.irDisagreement`, and it is not
  published.
* An expected-polarity encounter is confirmed whatever the distance (`NaviIntegratedCore.h:277-287`).
* `irCredible` only selects the duplicate-exclusion method (`:250-263`).

**Conflict:** the assignment's §3 IR credibility says to "restore IR's
magnet-identification authority after a verified re-anchor when subsequent measurements
are credible" and "withdraw its veto" on a credibly identified missed pulse. That implies
a veto exists when IR is credible. STATUS also says NAVI "withdraws IR's magnet veto";
there is no veto to withdraw.

**Consequence:** credible distance that shows a skipped marker (about two intervals) is
ignored. This is the evidence that would prevent C2 and the silent half of probe C.

**Simplest correction:** when IR is credible and the encounter is outside the window,
treat it as a contradiction (pending, unreliable) instead of confirming it. Publish
`ir_disagreement`. Unreliable IR still never vetoes.

---

## Other findings

| # | Location | Finding | Status |
|---|---|---|---|
| O1 | `NaviIntegratedCore.h:317-320` | Any interval above 1.5× the median withdraws IR credibility until the next anchor. The assignment says one *credibly identified missed pulse* (the bracketed case). The healthy October 9 control has 136 anomalies against 17 brackets in 9,484 events. Over-withdrawal widens C1 exposure. **Correction:** withdraw only on `BracketedMissingCompatible`. | Confirmed (replay) |
| O2 | `tests/replay_pulses.py`, `replay_timing.cpp` | The replay exercises only the `IrPulseEvidence` primitive. It does not cover the integrated `observePulse` path or Hall/DNA. `proven_physical_missed_pulses` is hard-coded to 0 in the printf, so asserting it is tautological. Run 1's real stall (5923 → 5926) **is not flagged** in replay, because the log omits count 5924 and the gap resets the population. The October 9 run-1 detection is therefore not demonstrated. | Confirmed |
| O3 | Fixtures | The three October 9 fixtures are exact subsets of the committed log (verified). The October 10 source logs `9950011_20261010_121800.log` and `_124555.log` are **not in the repository**, so their hashes and provenance cannot be checked. The 20-byte fixture is an empty gzip with 0 events, honestly labelled. 22,940 = 9,484 + 275 + 333 + 12,848 is arithmetically correct. | Confirmed |
| O4 | `NaviEvidenceCore.h:41`; spacing table | The 650 ms guard rejects a real next magnet above roughly 430 mm/s (280 mm minimum spacing). The highest October 9 IR-derived speed was 354 mm/s at PWM 110, and IR undercounts. A rejected magnet becomes C2. | Plausible, unverified |
| O5 | `NaviIntegratedCore.h:585-597` | The "continuous DNA" check is largely tautological for confirmed entries: a confirmation requires matching polarity and advances by exactly one. Real detection comes only from new mismatching encounters. Adequate, but not the independent verification STATUS describes. | Confirmed |
| O6 | `.ino:744-748` | Overlay reset on geographic revision can cancel a Braking execution mid-stop if resolution occurs while braking at PWM>0. The assignment authorized resetting obsolete STOP state, so this follows the assignment; the in-braking case is untested. | Plausible |
| O7 | Tests | Not covered: missed physical magnet, threshold-hovering stall, AUTO behavior during long pending, reversal under pending, 650 ms rejection at speed. | Confirmed |

---

## Governing-document conflicts

* **The October 11 assignment** (the authority STATUS cites) **is not committed** to the
  repository. The candidate does not contain `NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md`
  (it is on the `docs/navi-*-20261009` branches only).
* **Decision 0123 does not exist in the candidate.** The only 0123 in the repository
  (`navi-pass2-uses-geographic-speed-control-and-pwm-ticker-tape`, docs branches) is
  unrelated.
* **0109 conflicts:**
  - Its target-only shrug, "no automatic sequence correction", and "Missed Magnet is the
    only recovery" are contradicted by `resolvePending` and by the removal of
    `evaluateMissing`.
  - The assignment authorizes these changes. No superseding record exists, and the decision
    was correctly not edited in place.
* **0116 conflicts:**
  - "Confirmation requires … ±15% IR distance"; "Without applicable IR distance … Hall
    cannot advance MM"; "the 650-ms Hall-only fallback … withdrawn."
  - 0.3 restores the 650 ms Hall-only advancement, as the assignment authorized. C1 shows
    0116's original failure reason is not fully mitigated.
* **Architectural Note §7:** "must not … permit Hall-only navigation advancement contrary to
  Decision 0116." Same conflict as 0116.
* **0120 (PWM-zero movement requires declaration): preserved.** `requireDeclarationAfterMovement`
  is unchanged.
* **0121 (overlay authority): preserved**, apart from C3's change to the shell's
  unreliable-position rule.
* **Needed:** new superseding records for 0109 and 0116 that reflect the October 11
  assignment, plus the assignment itself, committed before field use.

## Preserved capabilities (verified against the source)

| Area | Status |
|---|---|
| `NaviStopOverlay.h`, `EwoStationStop.h`, `NaviEstop.h`, `NaviCompatibility.h`, `NaviMapAdapter.h`, `NaviSyncRecorder.h`, `NaviPulseObservation.h`, `NaviPulseTelemetry.h` | Byte-identical to `85a2cf2` (`cmp`) |
| `.ino` diff against 0.1 | Limited to the items listed under "Changed shell paths" below |
| Actuator and ramp, manual throttle and direction, brake, E-stop, battery, MQTT command handling, Type-5 parsing, pitch 9652 µm, STOP target, grade table, 300 ms final ramp, 5 s applied-zero dwell, 150 ms restart and acceleration | Unchanged (diff) |
| MQTT topics | Unchanged. Event payload buffer enlarged and four fields appended (additive). |

Changed shell paths in the `.ino`:

* Hall acquisition changed to 1 sample/ms (authorized "intended cadence").
* Type-6 ingress captures PWM and direction, and runs before Hall judgment.
* Overlay reset on geographic revision (authorized).
* Pending exemption from withdrawal (C3, not authorized).
* Event names and payload fields added.

## Verification performed

| Item | Result |
|---|---|
| `sh firmware/programs/NAVI_EWO_0_3/run_tests.sh` (GCC 13.3.0, Linux) | **exit 0, all pass** |
| Fixture provenance (October 9) | Regenerated from the committed log. Fixture rows ⊂ log events, no duplicates. |
| Replay claim of 22,940 events | Sum verified. Behavioral significance limited (O2). |
| Host probes A, C, D, E against the real 0.3 core | Results as stated in C1 and C2 |
| Code-level checks: 70×2 timing, opening IR association (64 Type-5 snapshots, received ≤ opening, ≤1 s, same boot, no later substitution), first target after a mid-interval declaration | Correct |
| Field-width check | On the 16 archived 1 kHz magnet passages, fields hold ≥80 contiguous samples above 70 counts, so 70×2 has ample margin |
| **ESP32 compile** | **Not performed.** The Arduino index is blocked by the proxy. A GitHub-hosted toolchain route was started and then stopped at the operator's request. The candidate's `verification/esp32-build.txt` (exit 0, 78% flash) is **unverified**. |
| Physical behavior | Not tested (outside scope) |

## Required corrections before field test

**Mandatory:**

1. **C1:** reject a new encounter when fresh, same-frame Type-5 shows zero pulses since the
   previous accepted opening.
2. **C3:** restore 0.1's AUTO withdrawal on unreliable position, or obtain an approved
   bound. (Operator decision; restoring is the preservation default.)
3. **C4:** a credible-IR window disagreement makes the encounter a contradiction instead of
   a confirmation. Publish `ir_disagreement`.
4. Commit the October 11 assignment, and new superseding records for 0109 and 0116.
5. Independently confirm the ESP32 build.

**Operator decision:**

6. **C2:** add a one-unobserved-marker hypothesis to `resolvePending`, or accept
   redeclaration as the remedy for a Hall miss.

**Optional:**

7. **O1:** narrow credibility withdrawal to bracketed findings.
8. Add tests for O7, and remove the tautological replay assertion.
9. Commit the October 10 source logs.

Field testing should also record native Hall NSR1, because full-polarity DNA has never
been observed on track.
