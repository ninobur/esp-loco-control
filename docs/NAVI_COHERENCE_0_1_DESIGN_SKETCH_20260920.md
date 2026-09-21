# NAVI_COHERENCE 0.1 — design sketch

Requested by Sam, 2026-09-20: not just document the optimistic-continuity
principle, but concretely sketch the rework, so it can be compared against
whatever the next firmware patch turns out to be before it goes on Toby.
This is a sketch to argue about, not a spec to implement blind — see "Open
questions" at the end.

Governing statement (from
[the principle doc](NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md)):
NAVI maintains the position that makes the available evidence most coherent
with the physical railway. It does not require perfect observations. A
sensor discrepancy is not a strike. Multiple hypotheses exist only when
independent evidence genuinely supports more than one physically plausible
location.

## Why NAVI_IR's model can't just be patched again

0.1 through 0.4 all share one shape, inherited from decision 0089: an
observation is checked against a set of **retained branches**; each branch
independently forks into "advanced," "missed-marker," and "false-
observation" candidates; any branch that syntactically survives becomes a
new retained candidate; more than one distinct surviving position is
`Ambiguous`. 0.2 fixed which absences count against the recovery budget. 0.4
added an IR-too-early veto and removed the fault cap as a terminal rule. Both
were real, correct, narrowly-scoped fixes — and both left the fork-on-any-
discrepancy shape intact. The 0.4 review found the direct consequence:
`Ambiguous` (and the movement-reseed it triggers) fires on routine single-
signal disagreement, not just genuine multi-source conflict, because the
architecture forks first and only asks "is this actually ambiguous" as an
afterthought (`distinctPositions()==1`).

NAVI_COHERENCE inverts that: score candidates against the preponderance of
evidence first, and only materialize more than one retained position when
the scoring itself can't produce a clear winner.

## What stays exactly as it is

- `MovementEvidence.h` — `WireSnapshot`, `MotionPoint`, `MotionIssue`,
  `between()`, `MovementSource`'s CRC/boot/sequence/pairing validation.
  Nothing here is about navigation decisions; it's transport-layer evidence
  production and it works.
- `RouteMap.h` — `ROUTE_N`, `ROUTE_POLARITY`, `ROUTE_SPACING_MM`,
  `spanMm()`, `nextMarker()`, `polarityAt()`. Pure surveyed geometry.
- The 0/1/2-mapped-interval nearest-distance classification
  (`classifyDistance`'s actual math, including its tie detection) — this is
  a correct, useful computation; what changes is how its output is *used*,
  not the computation itself.
- The ten-clean-observation streak concept, repurposed below as real
  confidence rather than a `Trust` label that the 0.4 review found could be
  granted on the very first reestablishment.

## Evidence, not branches

Each observation produces a small set of typed evidence contributions
against a **candidate marker** (usually just the next marker; occasionally
the one after it):

```cpp
enum class EvidenceKind { HallPolarity, IrDistance, Timing, /* future: */
                           MotorCommand, OperatorHistory };
struct EvidenceContribution {
  EvidenceKind kind;
  enum class Verdict { Supports, Contradicts, Unavailable } verdict;
  bool hardVeto = false;   // physically impossible, not just unlikely
};
```

Two contributions are **hard vetoes**, unconditional, matching "he cannot
move from magnet A to magnet C without passing magnet B" and "too early by
timing/IR":

- `Timing`: elapsed time since the last confirmed anchor is less than
  `LEGACY_MIN_MARKER_MS` for even one interval. (Unchanged from 0.4.)
- `IrDistance`: IR is usable and `classifyDistance` says `closest==0` (the
  travel is still closer to "haven't moved" than to the next marker).
  (Unchanged gate from 0.4 — this part of 0.4 was already correct.)

A hard veto means this observation is **not a landmark crossing at all** —
record `NON_LANDMARK_HALL`, leave the preferred position untouched, done.
This is tier-1 by construction: nothing to weigh, nothing to branch.

Everything else is **soft**: `HallPolarity` supports or contradicts the
candidate's expected polarity; `IrDistance` (when usable and not vetoing)
supports the candidate if it's the classified-closest interval, contradicts
it if a *different* interval is closer; `Timing` supports if travel is
physically plausible without being suspicious. `Unavailable` (no IR, stale,
optical-invalid, no-source) is explicitly **not** a `Contradicts` — it's
absent from the tally, per "missing IR doesn't create ambiguity, it simply
removes one piece of evidence."

`MotorCommand` (PWM) and `OperatorHistory` are named here as evidence kinds
because David's evidence list includes them, but neither is wired into a
per-observation decision anywhere in 0.1-0.4 today (`motionPermitsAdvance`
exists in the struct but nothing ever sets it from `actualPwm`). This sketch
leaves them as declared-but-unimplemented evidence slots rather than
inventing scoring rules for them without more grounding; the `EvidenceKind`
enum exists so adding them later is additive, not another rearchitecture.

## The three tiers, concretely

```cpp
struct Candidate { uint8_t mm; uint8_t supports=0, contradicts=0; };

Tier classify(const std::vector<Candidate>& reachable) {
  // reachable = {next marker}, plus {skipped marker} only if IrDistance
  // itself pointed at it (closest>=2) or Hall polarity ruled out `next`
  // entirely (disagreement/false-observation territory in 0.4's terms).
  if (reachable.empty()) return Tier::ActualLoss;          // nothing survives a hard veto at all
  auto best = std::max_element(reachable, by supports-contradicts);
  auto rivals = count candidates within a small margin of `best`;
  if (rivals == 1) return Tier::NormalImperfection;         // clear winner, others may still exist as noise but aren't competitive
  if (rivals <= MAX_ALTERNATIVES) return Tier::MeaningfulUncertainty;
  return Tier::ActualLoss;                                  // more genuine rivals than we're willing to track
}
```

- **Normal imperfection** → accept `best` as the new preferred position.
  Any soft contradiction on the WINNING candidate is recorded, not
  discarded: `POLARITY_CONTRADICTION` if Hall disagreed but distance/timing
  carried it; `B NOT OBSERVED` / `MISSED_MARKER` if the winner is the
  skipped marker. Ruling: ordinary `Advanced` (or `Retained` if the winner
  is "stayed put" — see below). No fork, no fault-budget interaction, no
  `Unresolved` state.
- **Meaningful uncertainty** → retain the (small, bounded — see "Open
  questions") set of genuinely competitive candidates. Ruling: `Ambiguous`.
  Unlike 0.4, this should be rare: it requires two candidates with
  comparable independent support, not merely one candidate having a flaw.
  The suite's "exactly half a mapped span" tie is the canonical example —
  and under this model it *should* surface as Ambiguous, not get silently
  resolved by an eager reseed (see next section).
- **Actual loss** → no reachable candidate has any coherent support at all.
  This should require more than one bad observation in a row (a single
  totally-unsupported reading is itself just evidence against a backdrop
  where "unchanged, I'm still where I was" remains coherent) — track a
  small consecutive-incoherence count, separate from and much stricter than
  0.4's `unresolvedCount`, since ordinary tier-1/tier-2 traffic never
  touches it. Response: invoke movement/map continuity search — this is
  where `reseedFromMovement` belongs, and *only* here.

## Reseed, properly scoped

`reseedFromMovement`'s actual mechanism from 0.4 — walk the map from the
last confirmed anchor using accumulated IR travel (lap-aware), keep the
polarity-supported candidates closest to the remainder — is sound and
should be kept close to as-is. What changes is *when* it's allowed to run:
only on `Tier::ActualLoss`, never as a side effect of an ordinary tier-1/2
observation. That alone resolves both things the 0.4 review flagged:

- It stops firing on routine single-signal disagreement (the half-span tie
  and the i=2 "exhaust loop" case both resolve as tier-1/tier-2 locally now,
  never reaching the reseed at all).
- `Trust`/confidence stops being granted unconditionally on reestablishment.
  Reseed success at tier 3 is a real recovery and can still carry a
  distinct, honestly-labeled confidence tier (call it `RECOVERED`, not
  `CONDITIONAL_SEQUENCE`); the ten-consecutive-clean-observation streak is
  what should earn the label the 0.4 sketch's README describes as "a
  provably unique ten-marker word" — gate it on the actual streak count
  again, the way 0.2 did before the field it read was dropped.

## What this replaces in the existing files

- **`HypothesisNavigator.h`** — the branch-array (`Hypothesis
  hypotheses_[MAX_HYPOTHESES*3]`, `addCandidate`, `distinctPositions`) is
  replaced by: one preferred `Position` (mm, anchor movement, streak,
  discrepancy log) plus a small fixed-size alternatives array used only
  during `MeaningfulUncertainty`. `observe()` becomes: compute evidence for
  the reachable candidate(s), classify the tier, act per tier above.
  Materially simpler than today's triple-branch-per-retained-hypothesis
  loop, because there is almost always exactly one retained position to
  evaluate against, not up to eight.
- **`Navigator.h`** — `NavState` likely collapses toward `{Unset, Declared,
  Uncertain, LimitStopped}` (tier 2 = `Uncertain`, replacing `Unresolved`;
  tier 3 sustained failure = `LimitStopped`). `judge()` becomes mostly a
  thin wrapper: call the coherence engine, map its tier to a `NavState`/
  `Ruling`, publish the discrepancy log. The `WaitingDistance` state 0.4
  removed doesn't come back — CANNOT_ASSESS is still just an absent
  evidence contribution, not a state.
- **`RecoveryControl.h`** — `stationConsensus()` currently compares station
  actions across every retained hypothesis. It gets *simpler*, not harder:
  almost always one candidate to check, occasionally two-to-three during
  genuine `MeaningfulUncertainty`, using the same "all must agree or
  withdraw" rule it already has.
- **Telemetry** — `nav/discrepancy` gains explicit reason codes
  (`NON_LANDMARK_HALL`, `POLARITY_CONTRADICTION`, `MISSED_MARKER`,
  `IR_UNAVAILABLE`) instead of the current free-form `causes` bitmask, so
  the console can show *why* a position was accepted despite an
  imperfection, not just that faults accumulated somewhere.

## Open questions this sketch does not resolve

Named honestly rather than fudged with a plausible-looking constant:

- **Competitive margin.** "Rivals within a small margin of the best
  candidate" needs an actual number (or a principled rule, e.g. "only an
  exact `classifyDistance` tie counts, no near-ties") before this compiles.
  Too loose and tier-2 becomes as common as 0.4's `Ambiguous`; too tight and
  genuine ambiguity gets silently resolved the way the half-span case does
  today.
- **Consecutive-incoherence threshold for tier 3.** How many bad-in-a-row
  observations before "actual loss" — one is clearly too few (a single
  freak reading), ten (0.4's number) may be too many now that tier-1 no
  longer eats into it. This is a field-tuning question, not a design one.
- **`MAX_ALTERNATIVES` bound.** 2 is assumed above by analogy to "one
  missed marker," but a genuine three-way tie is conceivable on some route
  segments; needs checking against `ROUTE_POLARITY`/`ROUTE_SPACING_MM`
  directly, the way the 0.4 review checked polarity-aliasing empirically
  rather than assuming.
- **`MotorCommand`/`OperatorHistory` scoring.** Named as evidence kinds,
  not specified — genuinely new territory, not a NAVI_IR carryover.
- **Where `RECOVERED` sits relative to `DECLARED`/`CONDITIONAL_SEQUENCE`**
  in the trust vocabulary is a naming/telemetry decision, not sketched in
  detail here.

## Comparison checklist for the next concrete patch

Before trusting a "NAVI_COHERENCE 0.1" (or further-revised NAVI_IR) patch
against this sketch:

1. Does a single wrong-polarity reading, with distance/timing/history
   agreeing, accept the position and record a discrepancy — or does it
   still create a competing branch?
2. Does one missed magnet (A→C, no B) resolve without entering an
   ambiguous/recovery state at all?
3. Does the movement/map continuity search fire only after sustained,
   multi-observation incoherence — or still on the first local tie?
4. Is a genuinely earned confidence label (ten-clean-streak or equivalent)
   distinguishable in telemetry from an ordinary tier-1 acceptance?
5. Re-run the independent-review method from the 0.4 review, not just the
   existing suite's literal assertions: instrument real scenarios
   (wrong-polarity-but-coherent, one missed marker, sustained IR outage,
   a genuine two-way tie) and verify actual behavior empirically before
   accepting a patch's own description of itself.

## References

- [Optimistic continuity principle](NAVI_IR_OPTIMISTIC_CONTINUITY_PRINCIPLE_20260920.md)
- [NAVI_IR 0.4 independent review](NAVI_IR_0_4_INDEPENDENT_REVIEW_20260920.md)
- Decision [0089](decisions/0089-navi-ir-joint-evidence.md) — the one-fault
  model this sketch replaces
