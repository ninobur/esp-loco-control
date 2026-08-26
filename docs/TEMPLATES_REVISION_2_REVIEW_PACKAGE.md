# TEMPLATES Revision 2 — Review and replay acceptance package

**Companion design:** `docs/TEMPLATES_REVISION_2_DESIGN.md`  
**Status:** Review package only; no implementation or field authority.  
**Date:** 2026-08-26

## 1. What reviewers are being asked to review

Review the Revision 2 architecture, especially:

1. target identity as a conjunction rather than a polarity match;
2. replacement of the persistent contradiction ring with one causal
   uncertainty episode;
3. processing every credible passage through an active episode, including an
   expected-polarity passage;
4. physical reachability as candidate elimination, not dead reckoning;
5. bounded, independently corroborated omission adoption;
6. explicit observation/inference/navigation authority boundaries; and
7. IR as a future local distance constraint with no marker authority.

Review does not approve numerical thresholds, replay implementation, firmware,
or flashing.

## 2. Evidence disposition

### Established and retained

- Precision over recall at admission.
- Raw-sample passage reconstruction rather than QUORUM event summaries.
- Artifact terminality and disjoint observation/position state.
- Expected-map validation before mutation.
- Companion/edge-lobe grouping as one physical passage.
- Separate movement-without-landmarks liveness.
- Operator anchors kept independent from navigator belief.
- Reversal and dwell invalidate direction/timing context.

### Established failures that Revision 2 must close

- QUORUM advance-before-validation.
- Otto narrow PWM-associated spike inclusion.
- Persistent contradiction pooling that stopped Toby after 30 advances.
- Same-polarity omission masking with silent two-marker displacement.
- Liveness thresholds based on an unvalidated/reused PWM statistic.
- Gap-resumption evidence corruption.
- Invisible suppression of low-PWM threshold crossings.
- Unconditional reversal resume.

### Useful but insufficient evidence

- One correct two-marker recovery adoption.
- Current flux separation.
- Self-selected 350-ms/0.5 companion population.
- Current maximum-speed linear model.
- Sparse operator anchors with +/-1 marker ambiguity.
- Opportunistic IR accuracy while valid.

### Explicitly unavailable or failed

- Absolute marker-by-marker truth for the existing Otto captures.
- Validated target-specific Hall fingerprints.
- Replay of the Grillers schema in the current raw-sample tool.
- Approved IR distance authority; stationary bright-light truthfulness failed.
- Evidence that PWM alone proves movement or distance.

## 3. Conceptual adversarial review

### 3.1 Clean Toby continuity

The prior raw candidate failed because isolated contradictions remained in a
global ring. Under Revision 2, the first contradiction opens one episode. The
next credible target cannot take an ordinary advance path; it either resolves
that episode as the expected target, establishes an omission, or causes a hold.
Once resolved, the episode is closed and cannot combine with a contradiction
minutes later.

Expected result: complete replay without the ring-full stop. The capture can
test this directly. Absolute passage truth remains limited by anchors, so broad
clean-control passages and declared anchors must be reported separately from
QUORUM labels.

### 3.2 Otto contamination

Narrow spikes fail passage acquisition and are terminal. Borderline broad
responses either fail the richer physical gate or become local observations;
they cannot advance solely because they crossed a floor. Repeating echoes
cannot accumulate across closed episodes.

Expected result: zero known-artifact navigation effects. When broad credible
noise remains indistinguishable, TEMPLATES holds rather than guesses. Existing
captures can test artifact inertness and bounded holds, but not certify absolute
position everywhere.

### 3.3 Same-polarity omissions

Deletion of a real passage changes the physical-reachability situation before
the later same-polarity passage is evaluated. If a later compatible mapped
target is viable, nominal polarity match is non-exclusive and opens/extends an
episode. Every live hypothesis sees every subsequent credible passage.

Expected result: for one through four deleted raw passages, either the correct
map position is uniquely recovered with corroboration or primary position
freezes before a wrong advance. Zero clean-audit silent displacement is
permitted.

### 3.4 Stall and dwell

PWM supplies maximum reach but no actual distance. A high-PWM stall can
therefore cause conservative loss of continuity, not fabricated movement. A
supported dwell resets timing without losing the anchor. Low PWM never disables
Hall acquisition silently.

Expected result: no liveness adoption from PWM, no false marker at baseline
drift, and no first-sample-after-dwell timing rejection. Grillers requires a
schema adapter before the claim is replay-validated.

### 3.5 Reversal

Clean reversal before another marker is reachable expects re-encounter of the
last confirmed target without double counting. Reversal after uncertain travel
preserves uncertainty.

Expected result: no unconditional resume, no evidence crossing direction epochs,
and no coordinate change from recognizing the same anchor on return.

### 3.6 Gap and reboot

A passage overlapping sample loss is inert. Movement-possible gaps invalidate
continuity. Reboot requires declaration.

Expected result: the first post-gap sample cannot be treated as a complete
passage; no recovery adoption spans a material gap.

### 3.7 IR-assisted cases

Revision 2 must run Hall-only when IR is unavailable. Synthetic IR observations
can exercise the interface, but no field acceptance may rely on IR until its
stationary false-valid problem is corrected and its uncertainty is calibrated.

Expected result: invalid IR has exactly zero position effect; valid simulated
distance can eliminate hypotheses but cannot create a target without Hall/map
evidence.

## 4. Disposition of Claude's 17 implementation findings

The numbering below follows Part 2 of
`TEMPLATES/REVIEW of HALL SENSOR LOGIC/ADVERSARIAL_REVIEW_FINDINGS.md`.

| Finding | Revision 2 disposition |
|---|---|
| 2.1 Recovery runs only on mismatches | **Replace.** Every credible passage runs through an active episode, including nominal polarity matches. |
| 2.2 Rich fields stored but not scored | **Revise rationale.** Morphology earns passage credibility; it does not become positional weight unless anchored marker-specific fingerprints demonstrate that use. |
| 2.3 Ring pools disjoint episodes | **Replace.** One fixed-anchor causal episode; no normal advance while open; explicit close disposition. |
| 2.4 One PWM maximum used for arrival and liveness | **Replace.** Integrated maximum-reach envelope is separate from source-tagged liveness context. |
| 2.5 Invalid low-PWM liveness fallback | **Unresolved calibration.** No safety authority until measured envelopes exist; PWM fallback may hold but never relocate. |
| 2.6 Liveness omits intervening artifact activity | **Accept.** Later replay must report artifact counts/types and data quality inside every liveness interval. |
| 2.7 Ordinary runs graze liveness threshold | **Reject current threshold.** Recalibrate against complete Toby/steady runs and require zero unnecessary clean-run timeout. |
| 2.8 Ring contaminates hypothesis search | **Replace with causal episode**, same architectural correction as 2.3. |
| 2.9 `MIN_SUPPORT=2` permits thin adoption | **Replace.** Require unique physical/map identity plus independent corroboration; no score count alone suffices. |
| 2.10 Sparse doubt can dangle indefinitely | **Replace.** Bound each episode by reachability, credible observations, movement, elapsed uncertainty, and operational proximity. |
| 2.11 Adoption overstates contributing IDs | **Accept.** Report exact mapped/non-target assignment per observation and only actual supporting IDs. |
| 2.12 Merge constants remain self-selected | **Retain as provisional evidence only.** The observed population supports continued investigation, not validation of its selector. |
| 2.13 Merge boundary is close to observations | **Revise.** Treat time/ratio as features; independent duration/flux/continuity evidence decides tail responses. Replay boundary-near cases explicitly. |
| 2.14 Lobe cap not exercised live | **Unresolved/test.** Do not make five lobes an architectural truth; test observed and one-beyond chains. |
| 2.15 PWM validation ceiling cannot fire | **Accept.** Report actual envelope coverage and synthetically exercise out-of-range behavior; no extrapolated authority. |
| 2.16 Grillers schema is unreachable | **Accept/block claim.** A schema adapter and replay are mandatory before stall-related validation claims. |
| 2.17 Low-PWM suppression is uncharacterized | **Replace.** Acquisition continues; every crossing is classified or diagnostically accounted for. |

Revision 2 therefore agrees with the factual findings but does not adopt every
suggested remedy. In particular, it rejects morphology-weighted position votes
without marker fingerprints and rejects “clear the ring on success” as
insufficient: a same-polarity event must first prove that success.

## 5. Offline replay acceptance criteria

The later Revision 2 replay is rejected unless all applicable criteria pass.

### 5.1 Input and reproducibility

- Read Hall SAMPLE measurement fields, integrity records, manifests, and map;
  never read QUORUM decisions as classifier input.
- Record tool version, configuration, capture hash, session, manifest, and map
  provenance.
- Stream complete captures without silently resetting on DECISION/STATUS rows.
- Adapt and test the Grillers schema rather than claiming coverage.
- Repeated runs produce identical results.
- Record whether interval lengths and maximum-speed bounds are measured or
  provisional. Production-candidate bounds require the operator-directed
  centerline interval survey and PWM 50–120 maximum-speed campaign.

### 5.2 Passage acquisition

- Reproduce the known narrow-pulse populations and clean Toby control.
- Report every suppressed/ignored threshold crossing, including low-PWM cases.
- Separate narrow artifact, incomplete passage, held field, baseline failure,
  and companion disposition.
- No categorical long-duration rejection without held-field analysis.
- Characterize near-threshold false-negative and false-positive boundaries;
  do not optimize a blended accuracy score.

### 5.3 Navigation invariants

- Zero rejected-artifact position or episode insertions.
- Zero passage double advances.
- Zero polarity checks after mutation.
- Zero normal advances while an episode is active.
- Zero cross-episode observation reuse.
- Zero recovery candidates outside physical reachability.
- Zero PWM- or IR-only marker advances.
- Every mutation reports evidence and eliminated alternatives.

### 5.4 Toby acceptance

- Process the complete successful 1,100+ marker evidence period.
- No unnecessary terminal stop comparable to the prior 30-advance failure.
- Retain credible broad passages without tuning specifically to Toby.
- The known isolated disagreement opens and closes one causal episode; it does
  not contaminate later navigation.
- Report drift at every usable independent anchor and preserve anchor ambiguity.

### 5.5 Otto acceptance

- Known narrow spikes produce zero coordinate standing.
- Known stationary/PWM artifact windows produce zero coordinate standing.
- Broad credible contradictions never advance merely because a score leads.
- Under unresolved severe contamination, freeze with a durable causal
  explanation before any confirmed phantom or omission-masked advance.
- Report artifact omissions, credible observations, recovery attempts, safe
  holds, and unresolved ground truth separately.

### 5.6 Deliberate omissions

- Delete raw SAMPLE windows for independently selected real passages, not only
  synthetic summary events.
- Cover one, two, three, and four consecutive omissions at same- and
  opposite-polarity map patterns, CW/CCW, and wraparound.
- Cover omissions followed by a same-polarity companion/twin.
- Each case must recover the true target with independent corroboration or
  enter safe hold before incorrect primary mutation.
- Zero silent wrong-coordinate outcomes, even if internal audits are clean.
- Report recovery distance, credible observations, time, competing hypotheses,
  and correctness against the constructed truth.

### 5.7 Liveness

- Separate maximum-reach statistics from current liveness context.
- No marker count or omission count inferred from PWM.
- No false timeout on the complete Toby run under ordinary operation.
- Correctly warn/hold on movement-without-landmark truth cases.
- Report intervening artifact activity rather than treating all silence alike.
- Bound sparse unresolved episodes by time/distance/operation, not ring fill
  alone.

### 5.8 Special cases

- Startup over/near a magnet and invalid baseline.
- Slow approach, low PWM, coasting, manual push, and high-PWM stall.
- Stop directly over a magnet and depart again without double count.
- Direction reversal before and after the next marker becomes reachable.
- Station dwell and restart.
- Sample gap before, during, and immediately after a passage.
- Session reboot.
- Opposite- and same-polarity lobe chains through and beyond current observed
  lobe counts.

### 5.9 Truth and audits

- Self-consistency audits remain necessary but cannot define false inclusion.
- Use operator anchors and deliberate raw-deletion truth independently.
- QUORUM `AGREE`, `DISAGREE`, and timing-phantom labels are comparison proxies
  only.
- “Zero false inclusion” must distinguish artifact-boundary integrity from
  absolute coordinate correctness.
- Incorrect adoption is unmeasurable where no later physical anchor exists and
  must be reported as such.

## 6. Required outputs from the later replay

For every physical candidate:

- raw evidence and integrity;
- passage disposition and reason;
- motion/reachability envelope;
- reachable and polarity-compatible mapped targets;
- uncertainty episode ID and anchor;
- hypothesis assignments/eliminations;
- primary position before/after;
- authority effects; and
- independent truth comparison where available.

Per session:

- false coordinate insertions;
- deliberate omissions and genuine-passage survival;
- recovery distance, observations, and latency;
- incorrect/unconfirmable adoptions;
- safe holds and reasons;
- anchor drift;
- episode count, duration, and cross-contamination audit;
- liveness warnings/timeouts;
- low-PWM and held-field populations; and
- data-integrity limitations.

## 7. Review questions for David

Technical questions have been resolved where the evidence permits. Four
operational choices remain:

1. On unresolved target identity, should the locomotive stop immediately or
   move at a bounded observation speed with location-dependent operations
   inhibited?
2. If recovery proves that a station/operational landmark was passed, should it
   stop immediately, continue to a safe point, or require manual authority?
3. After uncertain manual movement or reversal, which re-anchor procedure best
   matches railway practice?
4. If IR later passes truthfulness and usefulness gates, may it constrain
   reachability/liveness, or should it remain shadow-only?

No answer is required to review the Hall-only architecture. Answers are
required before implementing the affected operational transitions.

## 8. Review verdict

The Revision 2 design closes the two demonstrated navigation-architecture
failures conceptually without weakening artifact terminality. It is ready for
David/Claude/Codex design review. It is not ready for replay implementation
until the review either accepts the causal-episode/reachability model or records
specific corrections. Firmware and flashing remain on hold.
