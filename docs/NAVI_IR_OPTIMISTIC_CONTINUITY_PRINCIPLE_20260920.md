# NAVI optimistic continuity — design principle for the next NAVI_IR revision

Recorded 2026-09-20, following the NAVI_IR 0.4 independent review. Governs
the next revision (after 0.4); not yet implemented. Operator and Sam (ChatGPT/
Codex), verbatim:

## The principle

**David:**

> I have said this before. NAVI should be an optimist. NAVI should proceed
> on the assumption that if most of the data align, he is in the right place
> proceeding as expected. He is on tracks. He cannot jump to a place
> unrelated to where he has just been. He can stop. He can speed up. He can
> get faulty data from a sensor. He cannot move from magnet A to magnet C
> without passing magnet B. If he is at magnet C without having seen magnet
> B, oh well, he missed magnet B. Life goes on. Let's look for Magnet D.

**Sam:**

> Yes. That should be a governing principle of NAVI, and it changes how
> aggressively we need to treat discrepancies.
>
> NAVI should have a strong continuity prior: I am probably where the
> railway, my previous position, my direction, and the preponderance of
> evidence say I am. A contradictory observation is more likely to be
> imperfect evidence than evidence that the train teleported.
>
> The physical railway imposes enormous constraints. Toby cannot
> spontaneously move from MM117 to MM143. If traveling CW, his possible
> future positions form an ordered path through the map.
>
> That makes the ordinary cases simpler:
>
> - **Extra Hall event**: too early by timing/IR → ignore it as a landmark
>   and continue expecting the same MM.
> - **Missed Hall event**: travel and subsequent observations indicate we've
>   gone far enough for MM B and are now encountering MM C → conclude B was
>   missed, accept C when the evidence fits, and continue looking for D.
> - **Wrong polarity**: distance, timing, sequence and history say this is
>   probably B but Hall polarity is strange → accept that Hall may be
>   wrong; don't throw away an otherwise coherent location.
> - **IR temporarily unavailable**: continue from Hall + map + timing +
>   history. One instrument being unavailable doesn't make NAVI lost.
> - **IR disagrees while everything else agrees**: question the IR evidence
>   rather than automatically questioning position.
> - **Several independent sources disagree**: now confidence should fall
>   and NAVI should broaden its hypotheses.
>
> So I think "LOST" should become genuinely difficult to reach. It should
> mean something like: *I can no longer construct a physically coherent
> location from the recent history and available evidence.* Not: *One of my
> sensors told me something unexpected.*
>
> And the A→B→C example captures an important distinction: **missing an
> observation is not the same as losing position.** If NAVI was securely at
> A, then sees nothing at B, but the distance/time traveled and the next
> Hall observation strongly correspond to C, the sensible judgment is:
>
> ```
> B: MISSED OBSERVATION
> C: ACCEPTED
> POSITION: C
> NEXT EXPECTED: D
> ```
>
> No strike. No recovery episode. No drama.
>
> That should be a central design rule for the next revision: **preserve a
> physically coherent location hypothesis unless the weight of evidence
> makes it untenable. Sensor discrepancies modify confidence and explain
> observations; they do not automatically invalidate navigation.** That is
> the "optimistic NAVI" you have been describing.

## Implications for the current (0.4) implementation

This section is analysis, not part of the ruling above — added for whoever
picks up the next revision, grounded in the 0.4 independent review's
findings.

0.4 already resolves the "IR temporarily unavailable" case correctly per
this principle (verified: 30/30 observations advance normally across all
three IR-unavailability reasons, with no ambiguity). The rest of the
principle points at three places where 0.4's architecture — one-fault-per-
branch forking, inherited all the way from decision 0089's model — is not
yet optimistic by default:

- **Wrong polarity still forks rather than accepts.** `HypothesisNavigator::
  observe()` creates a competing "ignored/stayed put" branch alongside the
  "advanced, at fault" branch whenever polarity disagrees, rather than
  confidently accepting the distance/timing/history-favored candidate and
  recording the polarity mismatch as diagnostic. The principle says accept
  B outright when the rest of the evidence coheres.
- **`Ambiguous` triggers too easily.** It currently fires whenever two or
  more branches syntactically survive a single local fork — including cases
  where only one weak signal (one Hall polarity bit, one IR interval) is
  off against an otherwise coherent picture. The principle reserves broadened
  hypotheses for when independent sources actually conflict with each
  other, not merely with the map.
- **`LOST` triggers on the wrong condition.** Currently: the branch set
  empties because no candidate satisfied every gate simultaneously (timing
  AND polarity-or-fault-budget AND reachability, all as a strict
  conjunction). The principle's definition is a preponderance judgment —
  "no physically coherent location constructible from recent history and
  available evidence" — not a strict AND across independent gates.

Structurally, this is a bigger change than 0.1→0.4's iterative fixes, which
all stayed inside the same shape (retain several discrete forked hypotheses,
fault each independently, count toward a recovery budget). This principle
points toward something closer to a single continuity-preferring position
plus a confidence/coherence measure, where a missed magnet, a wrong-polarity
read, or a temporarily absent sensor are evidence to weigh against the prior
rather than events that spawn a competing branch — with explicit multi-
candidate ambiguity reserved for genuine cross-source disagreement.

This also bears on two things the 0.4 review flagged: the eager
`reseedFromMovement` firing on ordinary local ambiguity, and `Trust::
Sequence`/`CONDITIONAL_SEQUENCE` being granted on any reestablishment. Under
an optimistic-by-default local model, both of those symptoms may simply stop
occurring as often, since fewer ordinary cases would reach `Ambiguous` (or a
reseed attempt) in the first place.

## References

- [NAVI_IR 0.4 independent review](NAVI_IR_0_4_INDEPENDENT_REVIEW_20260920.md)
- [NAVI_IR 0.3 design ruling](NAVI_IR_0_3_DESIGN_RULING_20260920.md) —
  refined, not contradicted, by this principle: rules 5/6's "no second
  ten-magnet qualification" and "keep reasoning, don't guess" already point
  the same direction this principle makes explicit and general.
- Decision [0089](decisions/0089-navi-ir-joint-evidence.md) — the one-fault
  hypothesis model this principle proposes moving away from.
