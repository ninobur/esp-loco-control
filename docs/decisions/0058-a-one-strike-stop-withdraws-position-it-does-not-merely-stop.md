# 0058 — A one-strike stop withdraws position; it does not merely stop

**Status:** Accepted
**Date:** 2026-08-30
**Decided by:** the operator, on a review finding
**Supersedes nothing. Extends 0053, 0056, 0057.**

## Context

NAVI_ONE 0.1 ran the Lowline on 2026-08-29: 528 accepted markers, 0 refusals,
3.09 circuits. Rule 5 of the navigator's contract — *one strike* — was never
exercised in the field, so the code that implements it was never watched.

A fresh review of 0.1 walked it. `oneStrike()` published the words

> WRONG MAGNET at MM042 … Position is not known. Declare it.

and then the program went on knowing where it was:

- `Navigator::judge()` counted the refusal and returned. `NavState` stayed
  `Declared`; `positionKnown()` stayed true; `navMm` and `target` were unchanged.
- `serviceStatus()` kept publishing `level: CLEAR`, `nav: NORMAL`,
  `nav_ready: 1` and the same `mm`, one second after saying position was unknown.
- Judging continued through the coast-down. The next magnet was still judged
  against the stale target, and on a polarity match — half the time — **navMm
  advanced on the position the strike had just discredited.**
- `autoEnrolled` was not cleared. A later `cmd/go`, from the operator *or from
  the dispatcher*, passed every check and restarted AUTO on that position with
  no new declaration.

The stop was real. Everything the stop was for was not.

## Decision

**A one-strike stop withdraws position.** The navigator enters a latched
`Struck` state in which:

1. `positionKnown()` is false. Every later passage rules `NoPosition`; nothing
   can advance `navMm`, including a passage already in flight on the queue.
2. `nav_ready` goes to 0 and the status fields the console reads for position
   go to `UNSET`, so the dashboard stops rendering a marker rather than
   rendering a discredited one.
3. Enrolment is withdrawn — `autoEnrolled` is cleared and `state/auto 0` is
   published — so `cmd/go` is refused and the operator must re-enlist.
4. Only `declare()` clears it. Not a direction change, not clearing e-stop, not
   time.

`Struck` is kept distinct from `Unset` so the operator can tell *"I never told
him"* from *"he caught himself lying."* Position is equally unknown in both.

## Why

The operator's constraint was *"An error stops the locomotive. One strike
rule."* A stop that leaves the discredited position standing, keeps judging on
it, and accepts GO on it is not one strike — it is a warning message with a
brake attached. Under 0056 the navigator's whole job at a disagreement is to
stop saying where it is. It said so once, in a string, and carried on.

The worst of the four consequences is the third: a strike could be *followed*
by a silent wrong advance. That inverts the rule. The one moment the program
has the strongest evidence its position is wrong was the one moment it was most
willing to change that position without evidence.

## Cost

After a strike the operator must re-declare and re-enlist before AUTO will run.
This is deliberate. Re-declaration is the only thing that makes the position
true again, and there is no version of "resume" that is not a recovery
mechanism — which 0053, 0055 and 0056 all forbid by name.

Manual driving is unaffected: the throttle answers, because the operator has
authority and a locomotive that will not move is the operator's least favourite
message.

## Verification

Contract gate T4 now pins all of it: `state == Struck`, `positionKnown()` false,
a *correct* magnet after the strike rules `NO_POSITION` and does not advance,
the advance counter does not move, and a new declaration clears the latch.

Gates re-run: survey replay 195/195 accepted and 156/156 refused, 0
misclassified; contract 176 checks 0 failures; real-lap replay 172 advances, 0
refusals, closed at MM040. Sketch compiles at 974,707 bytes.

## References

- `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md` — Finding 1
- `firmware/test-programs/NAVI_ONE/Navigator.h`, `NAVI_ONE.ino` (`withdraw()`)
- decisions 0053, 0055, 0056, 0057
