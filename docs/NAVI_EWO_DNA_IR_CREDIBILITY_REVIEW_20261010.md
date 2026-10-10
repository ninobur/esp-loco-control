# NAVI_EWO architectural reassessment: magnetic DNA identification and IR credibility

**October 10, 2026. Read-only architectural review. No firmware, threshold, calibration,
STOP behaviour, decision record or deployed service was changed.** This document is
analysis and recommendation only. Nothing in it is an approved design (AGENTS.md §1–2).

## 0. Provenance and repository condition

| Item | Value |
|---|---|
| Firmware reviewed | `firmware/programs/NAVI_EWO_0_1_MM045_STOP/` on branch `codex/navi-cruise-stop-20261009`, commit `85a2cf2` |
| Uploaded copies | All 13 files David uploaded on 2026-10-10 are byte-identical to that commit |
| Field log | `field-records/logs/20261009_MM045_EVENING/all_20261009.log.gz` (same commit; SHA-256 stated in its README) |
| DNA map | `NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h` (171 markers; included by `NaviMapAdapter.h`) |
| Decisions read | 0056, 0098, 0104, 0109, 0116, plus the 0118/0120 annotations in `HISTORY.md` |
| Prior art read | `ProximalRecovery.h` (PROXIMAL_R1), Toby 20Q3 run 3 verdict, `NAVI_MM045_NAVIGATION_INVESTIGATION_20261009.md` |
| Reproduction | `tools/analysis/navi_dna_ir_review_20261010/reproduce.py` (on this branch) regenerates every number below |

**Repository condition (reported, not changed).** `main` does not contain NAVI_EWO, decisions
0100–0122 or the October field evidence. The current NAVI state is on
`codex/navi-cruise-stop-20261009`. Three October 9 documentation branches
(`docs/navi-architecture-reconciliation-20261009`, `docs/navi-stop-overlay-20261009`,
`docs/navi-cto-bubble-decisions-20261009`) diverge from it and are not ancestors of it. This
report is committed on the task branch `claude/hopeful-edison-jt4qow`, which is based on `main`.
Rebasing it onto the NAVI branch would have required a force-push, which AGENTS.md §3 does
not permit without David's request. File references below therefore refer to commit `85a2cf2`.

Evidence labels used throughout:
**[LOG]** read directly from the October 9 log. **[CODE]** read directly from source.
**[MAP]** computed from the route map. **[INFERENCE]** follows from the above but has not
been independently verified on track. **[HYPOTHESIS]** is plausible but unproven.

---

## 1. Corrections to the brief's premises

The brief's central claim is mostly right, but four premises need correction before the
recommendations make sense.

1. **"IR subsequently underreported travel" describes two different failures, not one.**
   - *Run 1 (19:03–19:13):* one abrupt counting stall **[LOG]**. Completed counts 5923→5926
     took 1,182 ms at PWM 90. The surrounding intervals are about 40 ms. At about
     241 mm/s, this is roughly 255 mm of travel that was not counted, or about 26 pulses
     **[INFERENCE]**. After the stall, NAVI's own target magnets arrive a nearly constant
     224, 234 and 244 mm early. That pattern indicates a step loss rather than a
     progressive scale error. Optical aborts then climb from 57 to 3,743 under reason 1.
   - *Run 2 (19:49–19:54):* progressive undercount from the start **[LOG]**. Long
     intervals of about 3× recur every 8 counts with healthy contrast (reason 6). They
     make up 4.1% of intervals, and the longest unbroken 8-count streak is 9. Saved
     anchors MM045→MM063 are 11.7% short of the map. That figure is roughly what about two
     uncounted spokes per long interval would produce **[INFERENCE]**.
2. **The "3× every 8 counts" pattern is not by itself the failure signature.** The
   19:31–19:41 session ran on the same IR boot and recorded **428 confirmations and zero
   missed markers**. It also contains these long intervals, at 0.9% of intervals, with
   8-count streaks of up to 3 **[LOG]**. The 18:56–18:58 session, before the obstruction and
   IR-car reseat, contains none. The pattern is intermittent, and what separates it from
   healthy running is how much of it there is, not whether it appears.
3. **Run 1's first MISSED_MAGNET was not an IR-window rejection.** NAVI confirmed MM095 at
   19:05:20.727 and entered the 100–200 mm spatial-reference collection phase at 21.089.
   That phase is measured in IR distance, so the IR stall held it open until 22.536
   **[LOG]**. While the phase is active, `observeHall()` sends every Hall sample to
   `collectSpatial()` and never forms target evidence **[CODE, `NaviIntegratedCore.h`
   lines 219–223]**. MM096 is expected to have passed during that window **[INFERENCE]**,
   and no HALL_SUPPORT for MM096 exists in the log. **NAVI was blind to Hall while IR was
   stalled.** This is a concrete defect, separate from the brief's framing.
4. **"Largely reliable, independent DNA" was true of the magnets, not of the earlier
   recognizers.**
   - The physical layer is strong. The clean session had zero Hall misses across 428
     confirmations. Toby's run 3 matched 32 of 32 opening poles **[LOG/verdict]**.
   - The earlier software that used the DNA failed in documented ways:
     - The old whole-route matcher jumped MM065→MM166 during AUTO (0098).
     - PROXIMAL_R1 held a two-marker error for 32 events, because its alternative check
       depended on an IR travel reference that it could only re-establish once its history
       agreed with the wrong incumbent (run 3).
     - The 650 ms Hall-only fallback confirmed MM13, MM12 and MM11 from one stationary field
       (0116).

   Restoring "what we had" is not the goal. The goal is to restore the evidence while
   avoiding those three specific failure mechanisms.

Smaller point: the operator-reported offsets were about 18 markers (run 1, third stop) and
15–16 markers (run 2), not 10. "About 10 per lap" was David's provisional per-lap estimate.

---

## 2. Architectural verdict (Deliverable 1)

**Yes. EWO's simplification made NAVI fragile, but the mechanism is more specific than
"IR became the authority."** EWO forms a closed loop that never checks identity:

```
target-polarity-only Hall test  ──►  ±15 % window from last anchor  ──►  confirm & re-anchor
        ▲                                                                     │
        └──────── Missed Magnet advances target blindly when window passes ◄──┘
```

* **Opposite-polarity fields are discarded by design.** The 0109 "shrug" is
  `return; // target-only shrug, including opposite field` **[CODE]**. Those discarded
  fields are exactly the evidence that would expose a one-marker identity slip. **[MAP]**
  Against the 171-marker polarity sequence, a ±1 or ±2 slip contradicts an observed
  polarity within a median of 1–2 markers, 4 markers at the 90th percentile, and **7 at
  worst**, provided every magnet is observed.
* **The only identity check is a single-polarity coin flip.** After an IR step loss,
  NAVI's next target is often physically one marker away. The field it eventually accepts
  matches target polarity about half the time by chance. When it does, NAVI re-anchors to
  the wrong magnet, its distance residuals go back to normal, and telemetry shows
  `position_reliable=1` and `HALL_IR_READY` while NAVI is wrong **[LOG: 19:12:20 status]**.
  Each such event adds a permanent slip, which is consistent with the accumulating offsets
  David reported **[INFERENCE]**.
* **IR vetoed NAVI's own correct target.** In run 1, the support events after the onset
  were for targets MM097, 098 and 099, each with the correct polarity and each at a
  near-constant offset matching the measured stall **[LOG]**. In run 2, the supports for
  064–068 fall short by an amount that grows steadily with distance (74 → 227 mm), which
  fits a scale undercount on the correct magnets **[LOG]**. In both runs, the first
  rejected fields are best explained as *NAVI's correct magnet rejected by bad IR*
  **[INFERENCE: no independent physical marker identification exists at those
  timestamps]**. The brief's "IR was wrong. That was my magnet." is supported by the
  evidence. It is not yet proven.

**Is the proposed direction simpler and more robust than the alternatives?**

* **Wider IR windows: no.** Run 1 lost about 255 mm in one step. Only a window close to
  ±85% would admit that, and at that width it would also admit the neighbouring magnet
  (spacing 280–360 mm). Wider windows would turn sensing faults into identity slips more
  often. Windows also cannot detect an identity error at all: every distance in run 3 was
  inside ±15% while NAVI was two markers wrong.
* **Map-wide or alternative-position search: no.** The MM065→MM166 jump is the documented
  result.
* **More exception handling: no.** The spatial-phase blindness and the gate/anchor
  mismatch are examples of how exception paths interact badly.
* **Bounded, continuous polarity-sequence verification with an IR span-credibility flag:
  yes, as a candidate.** It uses evidence NAVI already acquires and currently throws away.
  But it is honestly a **bounded alternative-hypothesis evaluation**. It directly reverses
  clauses of 0109 rather than refining them (see §9). It should not be presented as
  anything less.

**Where the proposal is weaker than the brief suggests:**

* An IR credibility flag on its own does not fix anything. Without DNA, the only thing a
  "questionable IR" state could do is hold, or advance on Hall alone, and 0116 withdrew
  Hall-only advancement for good reason. Credibility is useful only as an input that
  decides whether IR's veto applies to a given span. DNA is the primary capability.
* The credibility evaluator would not have prevented run 1's first miss. The stall
  interval is only known when count 5926 completes, at 19:05:22.440, which is after MM096
  had already passed into the blind spatial phase.

---

## 3. IR credibility: smallest practical mechanism (Deliverable 2)

### What the October 9 timestamps support

Results from `reproduce.py` §2. The probe flags a consecutive Type-6 interval that is at
least 2.2× the median of the surrounding 17 intervals, at PWM > 0.

| Session | Intervals | Long | Max single ratio at PWM≥60 | Longest 8-count streak | NAVI outcome |
|---|---|---|---|---|---|
| 18:56–18:58 (pre-reseat) | 1,507 | 0 (0.0%) | — | 0 | 94 confirmed, 0 missed |
| Run 1 | 6,776 | 94 (1.4%) | **13.7** (19:05:22.440) | 4 | 142 confirmed, 218 missed |
| Clean 19:31–19:41 | 8,138 | 77 (0.9%) | 3.1 | 3 | 428 confirmed, 0 missed |
| Run 2 | 4,438 | 180 (4.1%) | 3.3 | **9** (≥4 first at 19:49:59.109) | 71 confirmed, 104 missed |

**What the evidence proves:**

* A single steady-PWM interval more than about 5× its neighbours occurred exactly once
  during navigation that day: at run 1's onset. The healthy maximum was 3.1, so there is
  clear headroom. It was flagged 0.7 s before the first MISSED_MAGNET and 0.9 s before the
  first wrongly rejected support **[LOG]**.
* For run 2, *magnitude* does not separate it from healthy running (3.3 vs 3.1).
  *Persistence* does: an 8-count streak of 4 or more first appeared at 19:49:59.1, 26 s
  before the first rejection, and never appeared in the clean session **[LOG]**.

**What remains hypothesis:**

* The run-2 separating margin is one streak length (3 vs 4), measured on one day. A
  threshold chosen from this day would be overfitted. It needs more track sessions before
  it can be called a rule.
* Neither signature proves skipped spokes as opposed to IR-car wheel stall, slip or
  binding (the investigation document reached the same conclusion). That does not matter
  for credibility, because in every case IR has stopped representing locomotive travel.

### Smallest mechanism (candidate)

Judge credibility **per span between two consecutive Hall field events**, not
continuously. Mark a span QUESTIONABLE if any of the following occur inside it at PWM > 0:

1. One consecutive Type-6 interval at least R× the median of its recent neighbours. R is to
   be set from data; this day supports R ≈ 5.
2. Any existing Type-6 sequence, pulse, time or order break, or a queue loss. These are
   already computed in `NaviPulseObservation.h`.
3. *Optionally, pending more evidence:* a sustained periodic long-interval pattern, such as
   an 8-count streak of at least 4.

A QUESTIONABLE flag **withdraws IR's veto and its Missed Magnet authority for that span
only**. It does **not** alter cumulative distance, reconstruct pulses, or touch
calibration. A CREDIBLE span keeps today's behaviour exactly. Two constraints follow:

* **It needs Type-6 to enter NAVI.** Today Type-6 is deliberately observation-only
  ("No Type-6 evidence or loss enters NAVI/control authority", HISTORY.md, October 6). That
  is a governance change.
* **Type-5 alone is too coarse for run 2.** Type-5 packets arrive at about 10 Hz, which
  would show run 1's stall as a speed dip (246 → 32 mm/s in the existing 1-second speed),
  but not run 2's periodic pattern.

---

## 4. DNA identification: what can be reused (Deliverable 3)

| Capability | Where | Reuse |
|---|---|---|
| 171-marker polarity and spacing map | `RouteMap.h` | As is. **[MAP]** Every 10-marker window is unique. 9 → 163/171 unique, 8 → 129/171, 7 → 65/171. Worst ambiguity at 8 is 3 candidates. |
| Hall acquisition, median-of-5, ±70 threshold, leading-boundary landmark, spatial reference | `NaviIntegratedCore.h::observeHall` | Reuse the threshold and landmark logic, **but evaluate both signs** and add field *clear* detection so one field produces exactly one event (the 0116 lesson). |
| Rolling history with UNKNOWN slots, incumbent-default, "unique strictly better" replacement, never scoring invented polarity | `ProximalRecovery.h` (PROXIMAL_R1) | Reuse the *rules*, not the class. Shrink the ±10 bound to ±2 (see §5). **Do not** reuse its absolute-travel gate, which caused the run 3 failure. |
| Missed Magnet bookkeeping, first-target rules (0118), reversal maths | Core | Keep. A Missed Magnet advance becomes *provisional* until the sequence verifies it. |
| Type-6 per-pulse anomaly accounting | `NaviPulseObservation.h` | Source for span credibility. |
| NSR1 native Hall and consumption records | `NaviSyncRecorder.h` | Needed to *validate* any DNA mechanism on track. **No October 9 native recording exists** (newest Pi capture is September 28). |

**Evidence gap that limits this review.** The log records only target-polarity
HALL_SUPPORT events. Opposite-polarity fields were never published. The October 9 runs
**cannot be replayed through a DNA recognizer**. Every DNA-performance figure in this
report comes from map statistics, not from Hall data observed on October 9. Before any
DNA mechanism is accepted, a track session with native Hall NSR1 recording (or at least
both-polarity field events in telemetry) and independently identified physical markers is
required. That is a deployment step and needs separate authorization.

---

## 5. Retrospective confirmation (Deliverable 4)

**Does the current implementation keep enough history? No [CODE].**

* `hallWindow_` holds 5 ADC samples.
* A rejected support is published once and then lost.
* Opposite-polarity fields never become events.
* There is no record of which IR span each event followed.

**Smallest addition: a fixed field-event ledger** of about 12 entries, roughly 32 bytes
each, so under 0.5 KB in total. Each entry holds:

* polarity;
* leading-boundary IR coordinate (the existing `landmark.irUm`);
* Hall serial and timestamp;
* clear IR coordinate;
* credibility of the span since the previous entry;
* PWM;
* assignment: an MM number or UNASSIGNED.

**Rule sketch (candidate, for discussion):**

1. Every field event at PWM > 0 is appended, whatever its polarity or IR window. This is
   the 0104 OBSERVE step: "an observation that cannot be identified is not discarded."
2. A CREDIBLE span gives the number of mapped intervals between consecutive events by
   *relative* distance, which is robust even to a 20% scale error over one interval. A
   QUESTIONABLE span makes that count uncertain between n and n+2.
3. The incumbent assignment of the ledger is kept (HOLD) unless all three of these hold:
   - it has at least two polarity contradictions;
   - exactly one alternative within ±2 explains every observed polarity and every
     CREDIBLE span;
   - that alternative is supported by at least one later event.

   This is RESOLVE.
4. **Re-anchor at the most recent identified event's own stored boundary coordinate**, never
   at an older one. An older anchor would carry an intervening IR loss forward.

**Applied to run 1 [INFERENCE from logged supports and the map]:**

* MM095 is anchored. The MM096 field is unobserved (blind phase).
* An N field arrives over a QUESTIONABLE span measured at 376 mm. It could be 096 (N) or
  097 (N), so it is ambiguous.
* The next field is S, over a CREDIBLE span of 290 mm, so it is one interval later. If the
  N field were 096, this field would be 097 (N), which contradicts the observed S. So the
  N field was 097 and this one is 098.
* Identity is resolved within two events, about 2 seconds, and re-anchored at 098's
  boundary. No wider window and no pulse correction are needed.

**How the four cases are told apart:**

| Situation | Ledger signature |
|---|---|
| Correct magnet arriving early (IR undercount) | Polarity sequence matches the incumbent. The disagreement is only absolute distance across a QUESTIONABLE span. |
| Spurious field | Isolated. The following CREDIBLE spans and polarities continue the incumbent sequence. In run 3, the spurious threshold-level re-detections sat 9.7–19.3 mm after the previous field, far below the 280 mm minimum spacing. |
| Missed physical magnet | CREDIBLE span ≈ two intervals with consistent polarities: an UNKNOWN slot, as in PROXIMAL_R1. |
| Wrong identity | Repeated polarity contradictions of the incumbent, with one unique ±1/±2 alternative that fits fully. |

The case the ledger **cannot** resolve is a QUESTIONABLE span combined with a missed Hall
observation and a short sequence. The evidence says this combination is rare: zero Hall
misses in 428 clean confirmations. Under AGENTS.md §8 it should be handled by the stop
rule in §7, not by more machinery.

---

## 6. Re-anchoring (Deliverable 5)

* **Preserve:** `confirm()` already re-anchors at the field's leading boundary
  (`landmark.irUm`), not at the later median decision **[CODE]**. The ledger keeps that
  same value, so retrospective identification can re-anchor without extra state.
* **Existing inconsistency, already documented on October 9:** the window is *tested* with
  `latestIr_` at decision time but *saved* from the landmark. In run 2, 18 confirmations
  passed only because of the extra 19–29 mm of decision lag. This lets anchors drift
  silently under undercount. A DNA-verified identity makes this matter less, but the
  inconsistency should be resolved explicitly: either test the landmark's coordinate, or
  record both. The choice is David's.
* **Precision limit [CODE]:** `landmark.irUm` is the most recently *received* Type-5
  snapshot. At about 10 Hz and 245 mm/s it can trail the physical boundary by about
  25 mm. That is enough for identity (spacing of 280 mm or more) and enough for the current
  STOP geometry, which works in fractional markers. It does not justify any
  millimetre-level correction.

---

## 7. Bounded AUTO during unresolved disagreement (Deliverable 6)

Distinguish the states and give each the least action:

| State | Evidence | Action (candidate) |
|---|---|---|
| One QUESTIONABLE IR span | Credibility flag | No action. That span's IR veto is withdrawn. Publish it. |
| One unconfirmed magnet | Field held UNASSIGNED, or Missed Magnet | No action. The advance is provisional. |
| Sustained disagreement | At least 2 incumbent contradictions, no unique alternative yet | Publish `IDENTITY_UNRESOLVED` and set `position_reliable=0`. This is a telemetry honesty fix: today it reads 1 while 18 markers wrong. |
| Demonstrably incorrect identity | Incumbent contradicted, one unique ±2 alternative fits everything | Option A: stop and report (the 0109 CRM message). Option B: re-identify and re-anchor. |
| Unresolved within X markers | — | Withdraw AUTO through the existing withdrawal path. |

**Evidence for X [MAP]:**

| Requirement | Markers needed |
|---|---|
| Contradict a ±1/±2 slip, complete observation | median 1–2, p90 4, worst 7 |
| Same, 5% Hall misses | worst 10–12 |
| Same, 20% Hall misses | worst 14–22; not supported by observed Hall reliability |
| Unique identification from nothing | 10, as a proven property of the map |

**Recommendation for discussion: X = 10 mapped markers** counted from the first
unresolved contradiction. That is about 3 m, or 12–15 s at cruise. It covers the
worst-case local slip disambiguation with complete observations, it equals the map's
proven global-uniqueness length, and it leaves room for about one missed observation.
Under this rule, run 1 would have stopped or resolved within about 10 markers of
19:05:22. In the actual run it kept going for about two more circuits, until the third stop.

This is a test limit, not a performance governor. It acts only on unresolved identity and
uses the existing withdrawal path. It is distinct from the "10-MM AUTO recovery limit"
that 0109 excluded, which was a recovery budget rather than an identity-uncertainty limit.
David decides X.

**Recommended staging (consistent with AGENTS.md §8):**

1. **Stage 1:** ledger, both-polarity events, span credibility, `IDENTITY_UNRESOLVED`
   telemetry, and the X-marker stop (Option A). **No automatic re-identification.** Run it
   on track with native Hall recording and the operator naming physical markers. The
   question to answer: does the ledger's unique alternative match physical truth every
   time?
2. **Stage 2:** enable Option B only if Stage 1 shows the alternatives are right.

---

## 8. Implementation scope (Deliverable 7)

These are estimates. No change is authorized.

| File | Change |
|---|---|
| `NaviIntegratedCore.h` | `observeHall`: two-sign field onset/clear → ledger append. `confirm`: ledger assignment. `evaluateMissing`: provisional advance; no advance across a QUESTIONABLE span. A new private `verifyIdentity()`. `positionReliable_` semantics. New `EwoEventKind` values (keeping existing IDs). Estimated 120–180 lines. |
| *New* `NaviFieldLedger.h` (suggested) | Pure, host-testable ledger and local consistency check, so the core stays small. |
| `NaviPulseObservation.h` / `.ino` ingress | Hand per-span credibility (not raw distance) to NAVI. |
| `NAVI_EWO_0_1_MM045_STOP.ino` | AUTO withdrawal on `IDENTITY_UNRESOLVED` beyond X (existing path). Telemetry fields on `nav/evidence` and `state/loopstat` (new fields and values on existing topics, which needs David's sign-off under the MQTT constraint). |
| `NaviStopOverlay.h`, `EwoStationStop.h` | **No change.** They consume `mm()` and `physicalOffsetUm()`. STOP sequence, deceleration, dwell and restart are untouched. |
| `tests/` | Host tests: run-1 step-loss and run-2 periodic IR timestamp fixtures; map-derived ±1/±2 slips; run-3 reversal identity; the 0116 stationary sustained-field regression. |
| `tools/navi_sync_decode.py` | Decode the new NSR1 event kinds. |

**Separate defect, reported for a separate decision and not included in the scope above:**
the spatial-reference phase blinds Hall target evaluation while it is IR-gated
(`NaviIntegratedCore.h` lines 219–223 and 464–488). Its interaction with an IR stall
caused run 1's first miss. Whatever is decided for DNA, this needs an explicit ruling.

**Rollback:** the known-good starting point is commit `85a2cf2`, the
`NAVI_EWO_0_1_MM045_STOP` build. Stage 1 would be additive and self-contained in one new
header plus core hooks.

---

## 9. Governance: revisions that would be required (Deliverable 8)

Not revised here, per the restrictions.

* **0109** would need to be **reversed in part, not refined**:
  - The shrug ("does not identify another MM") would have to admit recorded non-target and
    opposite-polarity fields as identity evidence.
  - "No alternative-position search / no automatic sequence correction" would have to admit
    a ±2 local consistency check, under Stage 2.
  - "RESOLVE has no mechanism" would get one.
  - Missed Magnet becomes provisional.
  - The CRM direction gets a threshold (X).

  Map-wide matching, proximal ±10 recovery and guessed positions stay excluded.
* **0116** would need a credibility qualifier:
  - Change "EWO target confirmation requires … ±15% IR distance" to "requires it **when the
    span is CREDIBLE**."
  - Missed Magnet requires a CREDIBLE span.

  What stays: frame-integrity rules, PWM-zero rules, the ban on a single stationary field
  advancing NAVI, and "health does not erase distance." Credibility withdraws veto; it
  never edits distance.
* **0104** would gain a bounded RESOLVE mechanism. **0056** and **0098** keep their status,
  with a cross-reference recording which PROXIMAL_R1 rules were reused and which gate was
  rejected.
* **Type-6 observation-only status** (October 6 pulse-event specification) would need
  revising for the per-span credibility input.
* **0118's accepted limit** ("next target-polarity magnet accepted silently one or more
  off") would become detectable. Record that.

---

## 10. Summary answer to the central question

**Did EWO become more fragile? Yes.** The specific cause is that a target-only,
single-polarity confirmation loop re-anchors on any in-window same-polarity field, discards
the opposite-polarity evidence that would expose slips, and reports itself as reliable
while wrong. IR distance became the practical authority because nothing else could
contradict it.

**Smallest correction, as a candidate:**

1. Record every field event.
2. Judge IR credibility per span.
3. Let polarity sequence plus credible relative spans verify identity continuously within
   ±2 markers.
4. Re-anchor at the latest identified boundary.
5. Stop if identity remains unresolved after X ≈ 10 markers.

Run Stage 1 (detect and stop) on track with native recording before allowing automatic
re-identification. The spatial-phase blindness is a separate, concrete defect that needs
its own decision.

**Proven by the October 9 evidence:** two distinct IR failure signatures; the spatial-phase
blind spot; target-polarity supports consistent with NAVI's own correct targets being
rejected; and `position_reliable=1` while about 18 markers wrong.
**Not yet proven:** that the rejected fields were physically those magnets; the physical
cause of the IR anomalies; the run-2 detector threshold; and the real-world DNA resolution
time, because no both-polarity Hall data exists for October 9.
