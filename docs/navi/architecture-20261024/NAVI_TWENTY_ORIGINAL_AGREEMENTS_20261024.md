# NAVI — Twenty Original Agreements, October 22–24, 2026

**Status:** evidence record. This document preserves the final available closing text from the recovered Claude session. It is not a new design. Citations refer to the searchable source archive: `docs/navi/source-archive/20261022-24/NAVI_CLAUDE_SESSION_ENGINEERING_TRANSCRIPT_20261022_24.md`.

## Reading rule

Later explicit conference decisions amend earlier ones. In particular, Power of Normal Proposition 7 explicitly supersedes the **admission-gate** part of Principle 4 / Proposition 6A’s earlier shape discussion. The original words remain below as history; the amendment is stated beside them.

## Part I — Ten Principles of NAVI

### Principle 1 — Defined railway and reliability objective

**Closing heading:** “Sam to David and Claude — Closing Principle 1” (2026-10-23T08:17:06.986Z, `607c48e1-aec2-4113-98ea-ac28d3b635d6`).

> The locomotive operates on a defined railway containing known, measurable, geographically ordered features. These features, together with the available sensor and movement information, are believed sufficient to support reliable identification of the locomotive's physical location against the map.
>
> Associated design objective: NAVI should ultimately achieve the operational reliability demonstrated by the original block CTO, while providing more precise geographic identification.
>
> Qualification: The sufficiency of the available information is our working engineering hypothesis. The reliability of the model that interprets it must be established experimentally.

### Principle 2 — Automatic movement is constrained

**Source and consensus:** David’s statement in the Principle 1/2 transition; Claude’s explicit acceptance (2026-10-23T08:17:06.986Z, `607c48e1-aec2-4113-98ea-ac28d3b635d6`; 2026-10-23T08:42:24.166Z, `4067b4a6-5e2f-4584-8a70-c8b169cc3c59`). No separately titled closing block was retained.

> The locomotive only goes forward, on a loop, in a designated direction, from a defined starting point. The primary focus of NAVI is to find and identify the next magnet. Then make a tic mark on the map and start looking for the next one. There are 3 possibilities from NAVI point of view: find the next magnet, stopped, or the system is broken.
>
> “Normal” means the full programmed operating range, not a favorable subset.

### Principle 3 — Hall observes physical reality

**Closing heading:** “Sam to David and Claude — Principle 3 Approved” (2026-10-23T09:41:59.710Z, `930566d3-ed39-489e-9cdf-e84320f2d764`).

> The Hall sensor produces a continuous record of physical magnetic observations. NAVI should interpret the structure of those observations over time, rather than relying on isolated samples or premature interpretations of incomplete patterns.
>
> Agreed implication: NAVI must retain and examine sufficient recent observations to support reliable recognition. The choice of pattern boundaries and counting conventions should serve the quality of the evidence, not dictate it.
>
> We have not yet selected a mathematical recognition method or defined precisely when a pattern is complete.

### Principle 4 — Magnets produce recognizable waveforms

**Closing heading:** “Sam to David and Claude — Principle 4 Approved” (2026-10-23T10:07:24.570Z, `5236f16d-7a25-4f21-be6a-d8b2ae88cff6`).

> Magnetic encounters produce characteristic structures in the Hall signal, distinguishable from surrounding signal behavior. NAVI should recognize these structures through accumulated observations, emphasizing their dominant shape rather than incidental noise or jitter. Prior knowledge may inform recognition, but current physical observations remain the primary evidence.
>
> Agreed implementation implication: NAVI must accommodate variations in amplitude, duration, and background ADC level while recognizing the characteristic magnetic shape.
>
> The specific statistical method, observation window, and decision point remain open.

**Later amendment:** Proposition 7 retains shape as diagnostic evidence but removes it as an independent admission veto.

### Principle 5 — Recognition serves identification

**Closing heading:** “Sam to David and Claude — Principle 5 Approved” (2026-10-23T17:29:49.482Z, `46d90857-8bc0-4660-9a9a-efb19eae29ed`).

> NAVI's primary recognition task is to identify the next expected mapped magnet. It evaluates accumulated Hall observations and relevant movement evidence against that target, producing a binary navigation judgment: TARGET or NOT_THE_TARGET. Nonmatching observations do not require fault classification, and the recognizer does not diagnose the cause of a mismatch.
>
> The recognizer is a focused MM detector. Its purpose is to find the target, not explain every observation that fails to match.

### Principle 6 — The map defines the expected target

**Closing heading:** “Sam to David and Claude — Principle 6 Approved” (2026-10-23T19:13:59.918Z, `7227e7f7-26f4-4ca3-9dcf-e72c07f14564`).

> From the last confirmed geographic position and declared direction, NAVI identifies the next mapped magnet. Its expected polarity and approximate spacing are necessary conditions for a TARGET match. NAVI evaluates the accumulated Hall waveform and available movement evidence against those expectations, allowing for measurement and operating variation.
>
> We have also agreed to preserve the rolling line/curve evaluator as a candidate recognition mechanism, with two distinct transitions: curve → line confirms the preceding excursion; line → curve identifies the beginning of the next candidate excursion. The longer intervening segment supplies the baseline median. Numerical criteria remain to be established experimentally.

### Principle 7 — Geographic continuity is established by confirmation

**Closing heading:** “Sam to David and Claude — Principle 7 Approved” (2026-10-23T19:25:49.637Z, `53bdc419-3b33-4f58-bbc5-41c58743cd3f`).

> Beginning from the operator-declared position and direction, NAVI maintains a single confirmed geographic reference. Each successful TARGET identification advances that reference by exactly one magnet in the mapped sequence, including wraparound. NAVI does not guess its location, skip ahead, or attempt geographic reacquisition when an expected target is not confirmed.
>
> Raw observations remain preserved. No QUORUM-style recovery mechanism is authorized. Whether a prolonged unsuccessful search requires protective action remains an empirical question, separate from geographic recovery.

### Principle 8 — Movement provides supporting evidence

**Closing heading:** “Sam to David and Claude — Principle 8 Approved” (2026-10-23T20:55:42.367Z, `f3c2922e-a69e-445b-be2b-c38611283483`).

> In this Hall-only navigation project, NAVI uses elapsed time and known locomotive operating history, including PWM, to evaluate whether a magnetic encounter occurs at a physically plausible point relative to the last confirmed mile marker. Movement estimates support target identification but do not substitute for magnetic evidence.
>
> Agreed scope: IR does not participate in NAVI's navigation decisions. The IR test car may provide independent corroborating data during field testing.
>
> Candidate mechanism: A localized PWM/Hall ticker-tape history could provide empirical movement estimates using confirmed magnet encounters, known mapped distances, elapsed time, and PWM history. Its structure, calibration, and reliability remain to be investigated.

### Principle 9 — Observation is independent of navigation judgment

**Closing heading:** “Sam to David and Claude — Principle 9 Approved” (2026-10-23T20:59:48.415Z, `36a71594-8f85-4d94-b4e0-f4fa0424df6a`).

> The rolling line/curve evaluator continuously observes and characterizes the Hall signal, independently of NAVI's geographic decisions. It has no authority to identify mile markers or advance position. NAVI alone evaluates the evidence against the expected target and makes the TARGET/NOT_THE_TARGET decision. An unsuccessful match cannot interrupt subsequent observation.
>
> Observation: Continuous, unconditional, and independent of geographic expectations.
>
> Navigation: Target-focused, using the observation stream and map to make geographic decisions.

### Principle 10 — Simplicity and architectural discipline govern design

**Closing heading:** “Sam to David and Claude — Principle 10 Approved” (2026-10-23T21:04:13.226Z, `e986617d-d16a-45af-8cac-7c45cefdf1da`).

> NAVI shall implement only mechanisms justified by an architecturally defined requirement and a demonstrated operational need. Failures must be investigated and corrected at their root cause rather than addressed through accumulating patches. Implementation must not introduce new navigation behaviors, recovery mechanisms, or assumptions without explicit architectural agreement.
>
> Every mechanism must be traceable to an approved requirement. Ambiguities return to David, Claude, and Sam for discussion. Proposed corrections must address demonstrated causes, not merely suppress symptoms. Independent source review will check for unauthorized mechanisms and deviations from the agreed architecture.

## Part II — The Power of Normal

### Proposition 1 — Target-acquisition framing

**Final status:** closed by consensus in the conference record; source discussion establishes NAVI’s task as acquiring the one expected mapped magnet, not open-ended classification. Source: 2026-10-24T01:33:26.689Z, `e4088002-2f31-488e-806c-793f01b02b53`; see also the exact consolidated closure in the conference record.

### Proposition 2 — Recovery is out of scope

**Final status:** closed by consensus. Geography establishes the next target; possible recovery by measured MM063→MM065 distance was expressly outside the discussion. Source: 2026-10-24T02:30:38.287Z, `6d7500c0-fe97-4064-8405-5df6e0dc986d`.

### Proposition 3 — Timing provenance is verified

**Final closing:** “Proposition 3 — Closed by consensus. David, Claude, and Sam agree.” Otto calibration timing is RTB-anchored; Toby automatic timing is onset-anchored. Source: 2026-10-24T03:00:15.968Z, `e2ac0238-bbf7-4af2-a6be-15b5dcfedf56`.

### Proposition 4 — Expected neighborhood

**Final status:** closed by consensus. Map plus movement history predict the expected opening; the Hall ensemble verifies it. Prediction is not a substitute for Hall evidence. Source: 2026-10-24T03:51:15.192Z, `b4f01952-ac9a-4cd6-beb5-fb142dca115d` and the conference record.

### Proposition 5 — No new mechanism required

**Final status:** closed by consensus. Proposition 4’s predictor did not require a separate new navigation mechanism. Source: 2026-10-24T04:01:29.671Z, `5f47a5ba-ebc7-4792-9226-40cb27658a2c`.

### Proposition 6A — Retrospective complete-encounter qualification

**Final closing:** “Retrospective ensemble verification and minimum physically plausible width are accepted architectural principles. Numerical criteria remain subject to validation.” Source: 2026-10-24T04:16:25.585Z, `c2e352a2-e660-479f-b061-ba61342183b6`.

**Amendment:** Proposition 7 later removes shape matching as an admission condition.

### Proposition 6B — Boundary-only protective hold

**Final closing** (2026-10-24T06:03:31.129Z, `e2050d88-0cb4-426e-a69d-5b316bc61f90`):

> Automatic operation requires normal physical progression, measured against an empirically established, per-interval arrival boundary derived from clean historical observations (not a universal timeout, not the raw latest-ever-observed value). When that boundary is reached without confirmation, NAVI stops commanded movement, retains the last confirmed position, and continues recording Hall observations without attempting geographic recovery. Numerical boundaries, the current-pace adjustment, and the data-cleaning rules remain subject to future validation — none are set by this discussion.

### Proposition 7 — Shape is diagnostic, not an admission gate

**Final closing** (2026-10-24T06:37:13.284Z, `b4fcf86d-29aa-4c2c-a11a-459e67523b9b`):

> NAVI uses the 70 × 2 opening, observed excursion completion, minimum physically plausible width, and geographic consistency to confirm the expected magnet. Shape matching is not an admission requirement.
>
> Gaussian fitting may remain available for diagnostic research, but it cannot veto an otherwise qualified encounter.

### Proposition 8 — One target, no search or recovery

**Final closing** (2026-10-24T06:46:16.932Z, `f33c4095-e683-4198-86eb-1ff9d97d6175`):

> NAVI has exactly one expected target, determined by its last confirmed position and the map. Hall observations are evaluated against that target's physical qualifications (Propositions 6/7) and geographic qualifications (polarity, position). Qualifying evidence confirms and advances; nonqualifying evidence leaves the confirmed position unchanged while observation continues — it is not itself a stop trigger. No search among alternative identities, no geographic recovery. Proposition 6B's empirically established arrival boundary is the sole authority for converting unsuccessful waiting into a protective stop.

### Proposition 9 — Normal operation defines the model

**Final closing** (2026-10-24T06:49:47.288Z, `84897099-b561-4ef9-897d-272616d4373c`):

> No additional failure-specific navigation mechanism is introduced unless field evidence demonstrates a problem the established cycle (expect → observe → evaluate → confirm/advance) and the stop-and-hold safeguard cannot adequately handle.

### Proposition 10 — Simplicity is demonstrated

**Final closing** (2026-10-24T06:55:20.515Z, `619cfa5a-5b32-407b-8ca2-84baa53ddc08`):

> Simplicity is demonstrated, not presumed, via two concrete steps: (1) a historical replay of the complete agreed decision cycle against Otto/Toby recordings, including comparison against earlier implementations' documented rejection failures, and (2) a read-only audit of the current host sketch for vestigial recovery or morphology-rejection mechanisms left over from prior NAVI lineages, done before or alongside the replay so the evaluation reflects what will actually run, not just the architecture on paper.

## Evidence gap note

All twenty positions are accounted for. Principle 2, Propositions 1, 2, 4, and 5 do not each have a separately preserved, word-for-word closing block in the text-only extraction; their closure is explicit in the original chronological conference and/or its same-day conference record. They are identified rather than reconstructed as quotations.
