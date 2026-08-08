# Review — NGR console authority alignment, Draft 3

**Reviewer:** Claude (earlier dashboard consultation)  
**Review date:** 2026-08-08  
**Subject:** `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 3  
**Disposition:** Approve with two edits to P12 and §11; three smaller implementation notes

This review was supplied by the operator after the reviewer examined the
current repository state. It records review advice; it does not itself amend
the controlling specification.

## Verdict

**Approve with two edits to P12 and §11.** The additions are sound — R11 and
R12 both went the safer way, R12 especially. Moving the motion refusal into
firmware is the right call and removes the layer-boundary inconsistency cleanly.

Draft 3 disposes of all twelve Draft 2 findings correctly. QUORUM 1.9's diff
does not touch the `auto` or `estop` paths, so the revalidation claim holds;
only line numbers shifted.

## G1 — P12's clear path must be per-locomotive, or it damages manual operations

P12 says `pub_dispatcher()` gains a payload. If clearing publishes `"0"` to
the same broadcast topic it sets on, it reaches every locomotive — and the
E-STOP handler does this on any `"0"`, regardless of prior state:

```cpp
if(!estopped){
  motorDirection=DIRECTION_NEUTRAL; applyDirection();
  navPublishState("ESTOP_CLEARED_NEUTRAL",nullptr);
}
```

So clearing Otto's E-STOP from the dispatcher console drops Toby into NEUTRAL —
a locomotive that was never E-stopped, possibly running manually, silently
losing its direction selection. That is a “computer says no” in the manual
path, which §2 defines as a defect. The spec would be introducing one while
fixing another.

The asymmetry is correct for an emergency control and should be stated
explicitly in P12:

- **Set:** broadcast (`ngr/dispatcher/cmd/estop`) — everyone stops is the right
  behaviour.
- **Clear:** per-locomotive (`ngr/loco/<id>/cmd/estop = 0`), the same fan-out
  shape as P2.

`T_CMD_ESTOP` is already subscribed per locomotive, so this needs no firmware
change — only that P12 says which topic it clears on.

## G2 — T2 now couples the console release to a firmware release

Draft 2 could ship and be fully validated console-only. Draft 3's T2 tests a
**firmware** refusal, so the field check cannot complete until P11 ships. §11
step 2 defers sequencing to Codex without stating what happens if P11 slips or
fails review.

Recommend one line making the fallback explicit: the console iteration may
ship with T2 marked *blocked pending P11*, R6 remaining presentation-only, and
the residual D-d hazard still open and owned. Otherwise a firmware delay
silently blocks a console deliverable that is independently useful — and §7 is
explicit that the handoff is valuable on its own.

## Three smaller points

### `cmd/auto 0` must never be refused

P11 correctly says `cmd/auto 1`. Worth stating as an invariant rather than
leaving it to the reader, because a guard written as “refuse `cmd/auto` while
moving” — an easy slip — traps a rolling enlisted locomotive in AUTO with no
disenrollment path. Disenrollment while moving is a safety action; it zeroes
PWM.

### P11 closes both halves of D-d; do not add the second half

`motorIsMoving()` is `actualPwm>0 || commandedPwm>0`, so a stopped locomotive
with throttle still applied is refused too. Enrollment therefore can only
succeed at zero throttle — which makes zeroing on enrollment unnecessary rather
than merely deferred. Refusing beats mutating, and adding a redundant zero
would mask the refusal. This also satisfies Q4b's throttle clause in advance;
only its DIRECTION clause remains open.

### R8 has no field test

Everything else in §3 traces to §10. Suggest folding this into T3: press AUTO
with the locomotive unreachable and confirm the console shows no change rather
than an optimistic ENLISTED.

## Dispositions confirmed

The reviewer checked and agreed with the following, requiring no further
discussion:

- R10 annotation (C1)
- §4.3 (C2)
- D-a's ESP-NOW provenance (M1)
- Hans scoped out (M2)
- Q3 resolved to raw strings on the drift evidence (M3)
- P4's general rule (M4)
