# Independent review — Codex's clean-sheet Power of Normal proposal

**Status:** Read-only review. No firmware, specification, or proposal text was
modified. This document records findings for David's decision; it is not
itself a decision and has no force until reviewed and approved by him
(per standing practice for decision records).

**Reviewer:** Claude, single-agent, read-only.
**Reviewed object:** `docs/navi/NAVI_POWER_OF_NORMAL_CLEAN_SHEET_PROPOSAL_20261024.md`, commit `04ce4b6`.
**Governing authority:** `docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md`, commit `9a6bbb7b`.
**Independent audit:** `docs/NAVI_HOST_SKETCH_BEHAVIORAL_AUDIT_20261024.md`, commit `160945ab`.
**Firmware inspected directly:** `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1, commit
`5b7a518b`, read from the `agent/navi-rvm-gated-peer-20261020` worktree at
`/Users/davidbrown/esp-loco-control-rvmgated` (object store shared with this
checkout; nothing was checked out or modified there).

## 0. Repository-state note (not a finding against the proposal)

The three assigned documents live on different branches. The conference
record and the clean-sheet proposal are both reachable from the current
branch (`docs/navi-tracer-failure-20261022`). The audit document, commit
`160945ab`, is **not** reachable from that branch, from `main`, or from any
local branch — it exists only on `remotes/origin/codex/navi-rvm-field-candidate-20261017`,
a remote branch whose name predates the audit by a week. I read it with
`git show 160945ab:<path>` (read-only, no checkout) rather than resolve the
branch-naming mismatch myself. Flagging this per the branch-discipline
standing policy rather than silently working around it — it may be worth
your attention independent of this review.

Separately, I note (but did not review, as it is out of this assignment's
scope) that a later commit, `340b999` ("docs: align proposal with spatial
baseline decisions"), already modifies the proposal lineage after the
clean-sheet proposal this review covers. That sequencing — a follow-on
proposal landing before this fidelity review completed — is worth your
attention; I have not read or evaluated that document.

## 1. Overall fidelity assessment

**The proposal is a faithful, complete, and practical translation of the
conference architecture. I found no confirmed contradiction between the
proposal's stated design and any of the ten propositions.**

I independently re-derived the audit's five critical findings (C1–C5)
against r5.1 source at `5b7a518b` rather than taking the audit's prose on
trust, since the proposal's entire integration inventory (§7) rests on
those findings:

- **C1/C2 confirmed at source.** `RvmShape.h`'s `step()` admits a candidate
  only if `c.llr > 0 && c.amp >= MIN_EXCURSION_ADC` (70) — a reachable
  Gaussian-mixture admission gate over a *completed* fitted window, not a
  70×2 onset sentinel over raw samples. This is an active rejection
  authority, exactly as C1/C2 describe.
- **C5 confirmed at source.** `RvmNavigator::onEnsemble()` treats any
  accepted ensemble whose polarity doesn't match the expected marker as an
  immediate `Contradiction`, with no arrival-time clock anywhere in the
  path. This is an immediate stop-producing state on a single nonqualifying
  observation, exactly as C5 describes.
- **C3/C4 confirmed at source.** `RvmContinuity::admit()` returns early for
  any block with `pwm < CONT_STOPPED_PWM`, and `decideOldest()` withholds
  blocks that are flat and off the recognizer's moving baseline
  (`withhold = ... && flat && off`). Navigation observation is conditional
  on motor state and on baseline proximity, exactly as C3/C4 describe.

Given that the audit's load-bearing claims hold up against source, and the
proposal's design directly and explicitly reverses each one (unconditional
observer, retrospective-RTB completion with shape as non-veto diagnostic,
target-pending-not-contradiction on nonqualifying evidence), the proposal's
central technical claim — that it corrects r5.1's specific conflicts with
the ten propositions rather than just restating them — is substantiated,
not merely asserted.

The proposal's discipline is also real, not just stated: it declines to
assign any of the open numerical values (minimum width, arrival boundary,
pace cap) that the conference left open, it explicitly refuses three-sigma
(correctly, since the conference rejected it on evidence), and its §6
component table explicitly bans a named list of hypothetical subsystems
(recovery engine, generalized classifier-as-gate, fault diagnoser,
stop-reason subsystem) that would have reintroduced exactly the kind of
complexity Prop 9/10 warn against.

The risk in this proposal is forward-looking, not present-tense: the
decision register (§8) is the place where C1–C5-shaped problems could be
reintroduced under new names (most acutely in D2, the baseline/reference
policy — see §4 below) when those thirteen items are actually resolved.
The proposal is self-aware of this (its own §11 "Fidelity risk" column
names the same hazard for several propositions), which I read as a point in
its favor rather than a gap.

## 2. Ten-proposition compliance matrix

| # | Proposition | Compliant? | Proposal reference | Note |
|---|---|---|---|---|
| 1 | Target-acquisition framing | Yes | §2 ¶1, §6 "Geographic state/judge" | One mapped target, no identity enumeration. |
| 2 | Recovery out of scope | Yes | §2 ¶1, §4 | Explicitly rejects r5.1's search-free-but-still-reactive `Contradiction` state. |
| 3 | Timing reference verified from source | Yes, distinct question correctly separated | §3.1, D8 | Conference closed the *historical corpus* question (onset vs RTB, ~50/50 split). D8 is a *new* forward-looking question — which point defines "the event" for the new implementation's own movement-history/prediction — not a reopening of Prop 3. The proposal's phrasing in its own §11 row 3 blurs this distinction slightly; worth tightening (see §6 below) but it is not a contradiction. |
| 4 | Expected neighborhood | Yes | §2 ¶1, §4 | Prediction locates; Hall evidence alone confirms. |
| 5 | No new architecture for Prop 4 | Yes (trivially, for a clean-sheet design) | §2, §6 | Single geography model throughout. |
| 6A | Complete retrospective ensemble (as amended by Prop 7) | Yes | §2 ¶3, §3.3, §6 "Encounter evaluator" | Correctly replaces r5.1's fitted-window/half-width-delay completion with observed return-to-baseline; width is a lower bound only. |
| 6B | Protective stop on failed normal progression | Yes, design-complete; values correctly deferred | §5, D6/D7, §6 | Boundary is the sole hold trigger, independent of qualification outcome; no numeric value assigned, consistent with conference §3's open items. |
| 7 | Shape is diagnostic, not a gate | Yes | §3.3, §6, §11 row 7 | Unambiguous: shape metrics are telemetry only. This is the proposition r5.1 violates most directly (C1); the proposal's correction is explicit and specific. |
| 8 | One target; no search/recovery | Yes | §4 | Explicitly names and rejects the r5.1 mechanism (`Contradiction`) that violates it (C5). |
| 9 | Normal operation defines the model; minimal evidence-justified safeguards; unconditional observation | Design intent compliant; **not yet proven feasible** | §3.1, D1, D11 | States the right principle forcefully ("a suppression rule must not be invented as an implementation convenience") but does not yet show an unconditional-delivery mechanism that survives the actual 5 kHz ADC / 20 ms block / queue-contention path that today drops blocks (audit C3, `blockQ` full → drop). This is a verification gap, not a design contradiction — see §5 below. |
| 10 | Demonstrate simplicity, not presume it | Yes | §9, §10 | Matches the conference's own required next step (replay including MM110/146/169/113) and gates hardware work behind it. |

**Zero confirmed contradictions.** One proposition (9) is correctly stated
in design intent but not yet demonstrated against real hardware
constraints — that is an open verification item the proposal itself
schedules (Phase 2), not a fidelity defect in the document under review.

## 3. Critical deviations, ranked by severity

None found at "contradicts the conference" severity. Two minor documentation
issues, both correctable without re-architecting anything:

1. **(Low) Unnamed "diagnostic recorder" component.** §11 row 7 names a
   "Diagnostic recorder, isolated from judge" as the responsible boundary
   for Prop 7's diagnostics, but §6's boundary table — which explicitly
   enumerates every proposed component and explicitly forbids adding
   components beyond that list — does not list one. Most likely this is
   meant to be the existing `NaviSyncRecorder` (§7's own inventory row
   covers it: "preserve recording/transport functions"), and the proposal
   just didn't cross-reference it. Left as-is, a future implementer could
   read §11 as license to build an eighth component. Recommend naming it
   explicitly in §6 or §11 before implementation authorization.

2. **(Low) Prop 3 / D8 phrasing conflation.** As noted in the matrix above,
   §11 row 3 ("keep event timing tied to validated evidence convention")
   reads as if it is still discharging Prop 3 itself, when the closed
   conference finding and the open D8 decision are two different
   questions (historical corpus reconciliation vs. the new
   implementation's own event-timing convention). Recommend the proposal
   say this explicitly, so a reader doesn't conclude Prop 3 is being
   reopened.

## 4. Missing or improperly resolved design decisions

I checked each of the thirteen decision-register items against the
conference text and the audit for (a) whether it is a genuine
operator-level decision, (b) whether its alternatives all respect the ten
propositions, and (c) whether the proposal has silently assumed an answer.
I did not find an item where Codex assumed an answer without flagging it,
and the register correctly excludes three-sigma (D6) and r5.1's
held-window replacement (D3) as options, since the conference rejected
both on evidence.

One gap worth adding, not a wrong resolution:

- **D2 (baseline/reference) does not explicitly enumerate every PWM-off
  condition it must handle uniformly.** The audit's C3/C4 both trace back
  to r5.1 treating "motor off" as a single condition (`pwm < CONT_STOPPED_PWM`)
  regardless of *why* the motor is off — manual stop, station dwell, bubble
  hold, E-stop, or a future NAVI protective hold are all currently
  conflated into one gate. The proposal's §5 states the right principle in
  prose ("an operational stop or dwell must not disable Hall observation
  or reset the geographic target") but D2 itself is phrased generically
  about "standing/moving offsets" without naming this as a required test
  case for whatever reference policy is chosen. Recommend D2 explicitly
  require the chosen baseline/reference policy to be exercised against
  each of those PWM-off causes separately, since this is exactly the shape
  of the bug being replaced.

This is the single ambiguity I'd flag for the most operator scrutiny when
it is actually resolved, since a baseline/reference policy that quietly
reintroduces a flat-and-off-baseline withholding rule under a new name
would satisfy D2's letter while recreating C3/C4's substance.

## 5. Operational integration risks

Checked against the audit's own stated coupling requirement
("confirmed NAVI position → read-only snapshot → station/grade driver →
PWM request; no station, peer, dispatcher, or motor state must become an
alternative source of NAVI location") and against the proposal's §5/§7.

1. **Status conflation at the AUTO-withdraw boundary.** Today, `serviceAuto`
   withdraws AUTO on any "not Tracking" state (sketch:555–575), which
   collapses `Unset` (never declared) and `Contradiction` (an active fault)
   into one signal. The proposal correctly eliminates `Contradiction` as a
   state, but §7's `RvmStations.h` row and §12's integration section don't
   explicitly require the *replacement* status model to distinguish
   "NAVI not yet declared," "NAVI waiting normally," and "NAVI in
   protective hold" as three separately legible states to the station/AUTO
   consumer. Collapsing these again would silently reproduce the same
   class of problem the proposal is otherwise careful to avoid. Recommend
   adding this as an explicit requirement under D12 or §5.

2. **Peer (ESP-NOW) symmetry is outside both documents' audited scope.**
   Both the audit (§Scope, explicitly) and the proposal (§1, §7's closing
   paragraph) scope themselves to the one sketch's local files. Neither
   examines whether the ESP-NOW peer packet (`CtoPeerPacket`-style
   structure, referenced only as "peer radio packet" in the audit's
   `serviceAuto`/`StationDriver` row) carries any field whose meaning
   depends on today's `Tracking`/`Contradiction` semantics. Since Otto and
   Toby both run this sketch and coordinate bubble spacing over that
   packet, a NAVI semantics change that isn't mirrored in what gets
   broadcast to the peer is a real two-locomotive integration risk that
   neither document currently covers. Recommend this be added explicitly
   to D13's scope question or to the Phase 4 operational-integration gate.

3. **`docs/CLAUDE.md`'s ESP-NOW/CTO2 compatibility constraint likely
   doesn't apply here, but is worth noting.** That file's "hard constraint"
   section (packet structs/version constants must not change) describes
   the older `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST` lineage, not
   `NAVI_RVM_AUTO_STATIONS_BUBBLE`. Neither the conference, the audit, nor
   the proposal cites it, which appears correct — but it means
   `docs/CLAUDE.md` is stale relative to the NAVI_RVM-era work this review
   covers. I'm surfacing this as an observation for the durable-record
   policy (AGENTS.md §7), not as a finding against the proposal.

## 6. Verification gaps

The eight-step plan in §9 is thorough and conference-consistent (it
correctly gates hardware work behind decision closure, per AGENTS.md §8's
hierarchy). Two gaps:

1. **Named corpora aren't pinned down.** Step 1 says "identify raw
   recordings... route interval, and quality gaps" as if the corpora were
   still to be discovered. The conference record already names them
   specifically — Otto's SOLONAV calibration corpus (70×2 onset data,
   §2 Prop 6B), Toby's `NAVI_COHERENCE(X22)` automatic logs, and the
   2026-10-21 two-train field runs on r5.1 (§4). Recommend step 1 commit to
   these named sources explicitly rather than leaving corpus
   identification itself open-ended, so the replay doesn't under-collect
   relative to data the conference already has in hand.

2. **Dual-locomotive peer-state propagation isn't a named test.** Step 7
   compares "ESP-NOW coordination/bubble" against r5.1 operational
   baselines, but only as one of many single-locomotive function
   comparisons. Given the risk flagged in §5.2 above, recommend an explicit
   test exercising both locomotives simultaneously after the NAVI
   semantics change, specifically checking whatever the peer packet
   communicates about NAVI status.

## 7. Specific corrections required before implementation authorization

In order of what would most reduce risk if fixed first:

1. Add D2's PWM-off-cause enumeration (§4 above) before resolving D2 itself —
   this is the ambiguity most likely to silently reintroduce C3/C4.
2. Add the three-state (`unset` / `waiting` / `protective-hold`) status
   requirement to D12 or §5, so AUTO/station integration can't collapse
   "never declared" and "actively held" the way r5.1 collapses
   "Unset"/"Contradiction" today (§5.1 above).
3. Extend D13's scope question (or add a D14) to explicitly cover the
   ESP-NOW peer packet and dual-locomotive propagation of NAVI status
   (§5.2, §6.2 above).
4. Name the diagnostic recorder explicitly in §6 so §11 row 7 doesn't read
   as license for an eighth component (§3.1 above).
5. Separate the Prop 3 / D8 framing so the historical-corpus finding and
   the new implementation's own event-timing decision aren't read as the
   same question (§3.2, §2 row 3 above).
6. Pin step 1 of the verification plan to the conference's already-named
   corpora rather than leaving source identification open (§6.1 above).

None of these require resolving a governing ambiguity myself, and I have
not done so. They are corrections to the proposal document and its
decision register, offered for your review alongside the thirteen items
Codex already flagged.

---

*Compiled by Claude from direct reading of the three assigned documents and
direct inspection of `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1 source
(`5b7a518b`) for the claims in Critical Finding sections C1, C2, C3, C4,
and C5. No firmware, specification, or proposal text was modified.*
