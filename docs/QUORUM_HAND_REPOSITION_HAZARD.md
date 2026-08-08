# Hand-repositioning produces an odometer error QUORUM cannot self-correct

Date: 2026-08-07
Status: **hazard documented; firmware proposal below is UNREVIEWED and
UNIMPLEMENTED** — raised by the operator 2026-08-07, needs Sam/CODEX
review before any change.
Observed: `field-records/logs/20260807_C_acceptance_1_8.log` (12:42 NO_QUORUM)

---

## The hazard

Physically pushing or pulling a powered locomotive — routine when
positioning it for a test or nudging it onto a mark — **advances the
odometer**. Marker events fire on field crossings regardless of throttle
(the `LOW_PWM` timing gate still *accepts* for navigation; it only skips
conservation timing). Each event advances `navMm` **in the declared travel
direction**, no matter which way the locomotive was physically moved.

So rolling Otto back and forth over one magnet three times adds three
counts of forward progress to an odometer attached to a locomotive that
went nowhere.

## Why QUORUM cannot fix it

The candidate offsets are deliberately asymmetric
([QUORUM.ino:693](../firmware/QUORUM/QUORUM.ino)):

```c
static const int8_t QUORUM_OFFSETS[QUORUM_CANDIDATES] = { -1, 0, +1, +2, +3, +4 };
```

The set is built for the *common* failure — **missed** magnets, where the
train runs ahead of the odometer, needing positive offsets. Hand-pushing
produces the opposite: the odometer runs ahead of the train, needing
negative offsets. Only −1 is available.

Observed 2026-08-07: odometer at 113, locomotive physically at 115 —
a **−2 correction**, outside the set. QUORUM evaluated, failed to fit any
candidate, and terminated honestly:

```
scores [4,7,6,4,8,6]   leader = +3   margin = 1   → NO_QUORUM
```

Margin 1 against the required 2. It did not guess. Correct behaviour given
an unrepresentable truth, but the position was unrecoverable without an
operator re-declare.

## Operational rule (immediate, free)

**After any hand-repositioning of a powered locomotive, re-declare the
start interval.** This is the complete fix at zero engineering cost. Worth
noting the interval arithmetic when an event is open (locomotive parked
*on* a magnet): the pending event will close on departure and advance one
more, so declare one marker "behind" the physical position — under CCW,
`115-116` lands on 115 after the open event closes.

Relevance to CTO3: Station Stop v1 parks locomotives at mapped positions,
and any manual nudge at a platform carries this hazard.

## Proposal (NOT implemented — for review)

Operator request 2026-08-07: extend the candidate set to include −2.

```c
#define QUORUM_CANDIDATES 7
static const int8_t QUORUM_OFFSETS[QUORUM_CANDIDATES] = { -2, -1, 0, +1, +2, +3, +4 };
```

**Argument for:** hand-repositioning is a real, recurring workflow, and −2
is the observed case. A navigator that recovers from routine operator
handling is more useful than one that requires a re-declare.

**Argument against — this is why it needs review, not a quick edit:**

1. **It spends margin.** `QUORUM_MARGIN` is 2. Every added candidate is
   another way for a plausible-but-wrong offset to score near the leader.
   More candidates can mean *more* NO_QUORUM, not fewer — the opposite of
   the intent. This needs analysis against the actual route DNA, not
   intuition.
2. **Aliasing.** −2 and +2 may score identically on locally symmetric DNA
   stretches, producing ties where today there is a clean winner.
3. **It touches the navigator.** QUORUM 1.8 deliberately changed nothing
   in the navigator; this would, days after a validated release.
4. **A free alternative exists** (the operational rule above), and the
   hazard only arises from an action that already warrants a re-declare.
5. **Powered reversal is already handled** — `applyDirection()` maintains
   `navDir`, and 1.6 added the mid-interval reversal odometry fix. This is
   strictly about *unpowered* hand movement.

**Suggested analysis before adoption:** replay the route DNA offline and
compute, for each candidate set (6 vs 7), the distribution of
leader-vs-runner-up margins across all 171 start positions. If adding −2
materially narrows margins, the operational rule is the better answer.

**Alternative worth considering:** rather than widening the window, refuse
to *advance the odometer* on markers seen while `actualPwm == 0` — the same
"no positive evidence of motion" logic as decision 0017, applied to
odometry instead of the baseline. This would make hand-pushing a no-op
rather than a corruption. Risk: it changes navigation behaviour for
genuine low-speed running and interacts with the LOW_PWM gate; needs its
own analysis.

## References

- Decision 0017 (the same "believed moving" principle, applied to the
  Hall baseline)
- Decision 0005 (`motionWitnessSaysStopped()` — the real fix for both)
- `QUORUM_1_8_STAGE_C_VERDICT.md`
