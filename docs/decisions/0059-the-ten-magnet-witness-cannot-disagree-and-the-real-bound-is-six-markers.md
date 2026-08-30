# 0059 — The ten-magnet witness cannot disagree; the real bound is six markers

**Status:** Accepted
**Date:** 2026-08-30
**Decided by:** the operator ruled the witness should stop the locomotive; the
measurement then showed the witness never fires. Both are recorded here.

## Context

`Navigator::verifySequence()` was built on a true fact about this route: every
window of ten consecutive polarities on `NGR_DNA1` is unique, CW and CCW (nine
is not — SEQ_N = 10 is exactly minimal). The intent was a second instrument:
ten readings have exactly one place they can have come from, so the sequence
either confirms the declaration or names where the locomotive really is.

It was written report-only. The 0.1 review called it *"the best instrument in
the sketch, deliberately unarmed"* and argued that a missed magnet would be
caught by it *"almost immediately"*, with `seqAt` naming the true position,
while the locomotive drove on for up to seven markers. The operator ruled:

> Stop and report the true MM.

## What the measurement showed

The witness was armed. A gate was then written to watch it work: from all 171
start positions, declare, run twelve clean advances until `trust` reads
`PROVEN`, then drop one magnet and feed the track's real polarities from the
drifted position.

**The witness fired zero times out of 171.**

The reason is structural, and once seen it is obvious:

> `verifySequence()` compares each stored reading against the polarity of the
> marker it was stored at. A reading is only ever *stored* after it matched
> that marker's polarity — that is the whole of `judge()`. The word therefore
> always fits the claimed position, exactly, by construction.

`Trust::Contradicted` is unreachable from `judge()`. The ten-magnet check is a
re-derivation of the polarity test that has already run, one magnet at a time.
It cannot disagree with itself.

`Trust::Proven` is not meaningless — surviving ten consecutive polarity tests
is roughly a 1-in-1000 event for a wrong position, and that is real evidence.
But it is evidence the polarity chain produced. The window adds none.

## Decision

1. **The witness stays armed, as an assertion rather than an instrument.**
   `Contradicted` stops the locomotive and withdraws position exactly as a
   strike does (0058), and names the true marker when the word fits exactly one
   place. It should never fire. If it ever does, an invariant this program
   depends on has broken, and stopping is the right answer to that.
2. **No claim may be made for it.** Not in the code, not in the README, not to
   the operator. The comment in `Navigator.h` now says it is a tautology and
   points at the gate that proves it.
3. **The real bound is stated instead, and measured.** What catches a missed
   magnet is the polarity chain, when the same-polarity run ends.

## The measured bound

From the gate, over all 171 start positions, one magnet missed:

| | |
|---|---:|
| drifts caught immediately by polarity | 90 of 171 |
| drifts that advanced silently at the moment of the miss | **81 of 171** |
| drifts caught by the ten-magnet witness | **0** |
| worst case markers hidden before the chain refused it | **6** |

Six markers is roughly 1.8 m on this route. The review's estimate of seven came
from the longest same-polarity run (MM107–MM113); the drift survives one fewer
advance than the run is long.

So the honest statement of what NAVI_ONE guarantees is:

> No identification error survives more than **six** markers, about 1.8 m,
> before the locomotive stops. Roughly half are caught on the very next magnet.
> Nothing on board detects the other half sooner.

## What would close the window

Another bit. Identity today is one polarity bit per magnet checked against one
expected value; a missed magnet is invisible to a test that only asks *is this
the polarity I expected next*. `ROUTE_SPACING_MM` is surveyed and already on
board — a missed magnet arrives at roughly twice the expected spacing — but the
navigator has no distance measurement to compare it against. Time is not a
substitute: PWM is not linear with speed, and 0057 bans the inference.

That makes closing this window the same problem as the IR thread, and it should
be argued there or not at all. It must never be closed by a mechanism that
*survives* a disagreement rather than stopping at one — 0053, 0055 and 0056.

## References

- `firmware/test-programs/NAVI_ONE/tests/contract.cpp` — gate T9
- `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md` — Finding 2, and its premise
- decisions 0053, 0056, 0057, 0058
