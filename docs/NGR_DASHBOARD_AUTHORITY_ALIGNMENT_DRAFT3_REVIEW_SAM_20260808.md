# Review — NGR console authority alignment, Draft 3

**Reviewer:** Sam (Codex/ChatGPT)  
**Review date:** 2026-08-08  
**Subject:** `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 3  
**Scope:** Architecture and authority model, not merely dashboard UI  
**Disposition:** Approve for implementation with two specification clarifications; T10 recommended

This review was supplied by the operator for preservation in the repository. It
records review advice; it does not itself amend the controlling specification.

## Assessment

V3 is substantially ready for implementation. I do not see a fundamental
authority-model error that should block it.

More importantly, it now has a quality that earlier versions lacked: each
control action has a reasonably clear owner. The console is no longer being
allowed to quietly acquire train-control authority merely because it has
buttons.

I would approve it with two clarifications before code, neither requiring a
change in the operating model.

## 1. P6 needs sharper wording

P6 says:

> Refuse enlistment without pre-flight… Console-side

while §8 correctly says the firmware should own the motion refusal and the
console should generally present firmware decisions.

The distinction is legitimate, but “refuse enlistment” makes the console sound
authoritative.

I would define P6 instead as:

> The console shall not issue `cmd/auto 1` until ORIENTATION and LOCATION have
> been supplied.

That is different from the console deciding whether the locomotive may enlist.

Then:

- UI sequencing belongs to the console.
- Actual enlistment authority remains firmware.
- Motion refusal belongs to firmware under P11.
- The displayed ENLISTED state still comes only from reported `state/auto`.

That preserves the principle V3 is otherwise very careful about.

## 2. P12 needs the source of E-STOP state explicitly named

V3 says:

> the dispatcher console renders E-stopped state per locomotive

but does not say, at least in this document, which MQTT state is authoritative
for that rendering.

That matters because P4 has just established an excellent architectural rule:

> the alert stream is never the source of record for any authority state

I would extend that discipline to P12 explicitly:

> The E-STOP toggle’s displayed state must be derived from an authoritative
> locomotive-published E-STOP state, not from the last command sent by Flask and
> not optimistically from the button press.

If QUORUM already publishes an appropriate retained state, name the topic in
P12. If it does not, that is an implementation dependency that should be
identified now.

This is the same lesson learned from the AUTO problem: command issued does not
equal state achieved.

## The strongest part of V3

The separation of ENLISTED from RUNNING is exactly right.

Those are not two UI states. They are two different facts about authority.

An enlisted, stopped locomotive is still under dispatcher authority. Therefore
`autoRunning=0` cannot possibly mean MANUAL. That conceptual mistake is what
allowed the alert packet to destroy the dashboard’s representation of
authority.

P4 fixes the immediate bug and, importantly, M4 turns the lesson into an
architectural rule rather than a one-off patch.

I strongly support that.

## P8 is also now right

The revised retained-state handling is much better than simply accepting
retained MQTT state.

The crucial proposition is:

> retained authority state + `online=0` does not describe current authority.

The provisional-seed approach handles MQTT arrival ordering without
manufacturing certainty. Blank-until-proven is the correct failure direction
because displaying a powered-off locomotive as presently enlisted is a false
claim about authority.

T7 is consequently an important test, not an edge case.

## R11/P12 closes a genuine authority trap

This is an important improvement.

Under R7, once enlisted:

- locomotive throttle disappears,
- direction disappears,
- locomotive-page E-STOP disappears.

If dispatcher E-STOP could assert but not clear, the system would create a
state in which the dispatcher possesses the locomotive but lacks the means to
resume control of it.

Making dispatcher E-STOP reversible is therefore not merely UI convenience. It
makes the authority model internally complete:

> the authority that invokes the stop can release that stop without
> surrendering the locomotive.

END remains a completely different door because END transfers authority back
to MANUAL.

That distinction is excellent.

## R5/R6/R12

Retiring the “autopilot handoff” conception is important.

A locomotive moving manually should not cross the authority boundary and
continue moving merely because the software has changed the name of its
operator.

The clean sequence is:

> MANUAL → stopped → enlist → DISPATCHER/AUTO authority → BEGIN

R12 correctly puts enforcement of the stopped condition in the locomotive.

That means another client, a future console, MQTT injection, or any other
command source cannot bypass the rule.

This is a good example of putting a rule where the truth necessary to enforce
it exists.

## Q4b should remain unresolved

I agree with leaving Q4b open rather than smuggling an answer into this
implementation.

There are actually two separate events:

- Enlistment: transfer of authority.
- BEGIN: execution of orders.

There is no inherent reason enlistment itself must alter the physical direction
output. In fact, changing it during enlistment would mix an authority transition
with an operating decision.

The Codex position quoted in V3 seems architecturally cleaner: transfer
DIRECTION authority at enlistment without necessarily changing its current
physical state; let BEGIN determine what motion/direction the orders require.

But that belongs in the later decision record exactly as V3 says.

## Recommended additional test

### T10 — PAUSE/BEGIN authority persistence

1. Enlist stopped locomotive.
2. BEGIN operations.
3. PAUSE AUTO OPERATIONS.
4. Verify RUNNING becomes false.
5. Verify ENLISTED remains true.
6. Verify manual controls remain unavailable.
7. BEGIN again.
8. Verify operations can resume without reenlistment.
9. END AUTO OPERATIONS.
10. Verify ENLISTED becomes false and manual controls return.

That single test proves the distinction underlying R2/P3/P4:

> PAUSE changes operating state; END changes authority.

That distinction is important enough to deserve its own field test.

## Conclusion

I would mark V3:

**APPROVE FOR IMPLEMENTATION, with two specification clarifications:**

1. Reword P6 so the console withholds the enlistment request until pre-flight
   is complete rather than appearing to own enlistment refusal.
2. Explicitly identify the authoritative reported source for the P12 E-STOP
   state.

I recommend adding T10.

Beyond those points, I would not redesign this further before implementation.
V3 has reached the point where additional conceptual refinement is more likely
to delay learning than improve the architecture.

Most importantly, it achieves the primary objective in §7 cleanly:

> AUTO is a handoff, not a launch command.

That is now expressed consistently through the controls, MQTT state, authority
boundaries, failure behavior, and field tests.
