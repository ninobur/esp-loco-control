# Review — NGR console authority alignment, Draft 4

**Reviewer:** Claude (earlier dashboard consultation)  
**Review date:** 2026-08-08  
**Subject:** `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 4  
**Repository state:** `8c338c6`  
**Disposition:** Approve after H1 and H2 — two single-line firmware edits, same
gate; one structural check recommended; one residual to record

This review was supplied by the operator after the reviewer examined the current
repository state. It records review advice; it does not itself amend the
controlling specification.

Every claim below was checked against `firmware/QUORUM/QUORUM.ino` at `8c338c6`.
Line references are to that commit.

## Verdict

**Approve after H1 and H2.** Draft 4 adopts G1–G5 and generalises G1 better than
the Draft 3 review stated it: deriving displayed E-STOP state from retained
`state/estop` rather than from the last command sent is the stronger rule, and
it follows P4's principle rather than merely coexisting with it. R13, R14 and
R15 each push authority toward the layer that owns the truth, which is the right
direction of travel.

R13's diagnosis is correct, and the trap it addresses is deeper than the Draft 3
review said: recovery was blocked not only by a one-way toggle but by a BEGIN
gate. However **P14 as drafted does not deliver R13**, and the same gate traps a
locomotive by a second route that involves no E-STOP at all. Both findings sit on
`NEUTRAL_SELECT_DIRECTION` (QUORUM.ino:2575).

## H1 — P14 removes the NEUTRAL write from the wrong branch

**Blocking. Single-line firmware edit.**

Both branches of the E-STOP handler force NEUTRAL:

```cpp
if(!estopped){ motorDirection=DIRECTION_NEUTRAL; applyDirection(); ... }   // 2624  clear
if(estopped) { ... motorDirection=DIRECTION_NEUTRAL; applyDirection(); ... } // 2629  assert
```

P14 addresses the clear path only (2624). Remove the write there and DIRECTION is
still NEUTRAL when the clear arrives, because the **assert** already set it. BEGIN
then hits:

```cpp
if(motorDirection==DIRECTION_NEUTRAL){ stationPublish("GO_REFUSED",0,"NEUTRAL_SELECT_DIRECTION"); return; }  // 2575
```

The loco-page DIRECTION control is greyed by R7, so there is no way to satisfy the
gate. T6 fails and R13's promise — *restart an enlisted, E-stopped locomotive with
BEGIN alone* — is not met.

**The write to remove is the one on the assert path (2629).** That is safe, and the
firmware already demonstrates why: `estopped` is an independently and continuously
enforced interlock.

| Protection | Evidence |
|---|---|
| Motor clamped to zero every loop pass while E-stopped | `servicePwmRamp()`: `if(estopped){ commandedPwm=0; actualPwm=0; pwmWriteCompat(0); return; }` (1656) |
| BEGIN separately refused while E-stopped | `GO_REFUSED / ESTOP_ACTIVE` (2570) |

NEUTRAL on assert is redundant belt-and-braces, not the interlock. Dropping the
write from **both** branches preserves DIRECTION across the whole E-STOP episode,
which is what R13 ruled. P14's prose should say assert, not clear — or, more
simply, *neither*.

## H2 — NEUTRAL traps a locomotive with no E-STOP involved

**Blocking. One additional enlistment guard.**

Independent of H1, and reachable in ordinary operation:

1. A locomotive is parked in NEUTRAL. NEUTRAL forces `commandedPwm = actualPwm = 0`
   (1652), so `motorIsMoving()` is false and it **passes P11's energisation guard**.
2. R15's guards cover ORIENTATION and navigation readiness. Neither covers
   DIRECTION.
3. Enlistment succeeds. R7 greys the DIRECTION control.
4. BEGIN → `NEUTRAL_SELECT_DIRECTION`. No console path to set direction. The
   locomotive is recoverable only through the release door.

Note the interaction with H1: today the *only* way to clear an E-STOP leaves the
locomotive in NEUTRAL, so the ordinary sequence "E-STOP in manual → pre-flight →
enlist" walks directly into this. Fixing H1 removes that route; explicit NEUTRAL
selection still reaches it.

**Fix (this iteration):** add DIRECTION ≠ NEUTRAL to P11's enlistment guards,
refused with a published reason, exactly as R15 added the other two. *The rule
lives where the truth lives.*

**Alternative (supersedes the gate, but is Q4b):** resolve DIRECTION-transfer
semantics so BEGIN derives direction from mission + orientation. That is the
CODEX/Sam preference already recorded in §5 and is out of scope here.

## H3 — this is the third instance of one pattern; make it a standing check

**Structural. Recommend adding to §8.**

The spec has now met the same shape three times: **R7 withdraws a control, and a
downstream BEGIN gate still requires it.**

- §4.4 — caught it for pre-flight; R9 closes the trap R7 opens.
- Draft-3 F1 — caught it for E-STOP; R11/R13 close it.
- H2 — DIRECTION, still open.

Proposed invariant:

> Every BEGIN gate must be either (i) also an enlistment gate, or (ii) clearable
> from the dispatcher console without crossing the release door.

Applied to all eight gates, the sweep closes and yields exactly the two open
items already named:

| Gate | Line | Status under the invariant |
|---|---|---|
| `ESTOP_ACTIVE` | 2570 | (ii) via P12 clear — satisfied |
| `WAIT_FOR_STOP` | 2574 | self-clearing — satisfied |
| `NEUTRAL_SELECT_DIRECTION` | 2575 | **neither — H2** |
| `NOT_ENROLLED_IN_AUTO` | 2578 | inherent — satisfied |
| `NO_QUORUM_DECLARE_POSITION` | 2581 | **release door only — H4** |
| `NO_POSITION_DECLARE_START_MM` | 2582 | (i) via R15 — satisfied |
| `NO_SESSION_DIRECTION` | 2583 | (i) via R15 — satisfied |
| `ALREADY_RUNNING` | 2584 | benign — satisfied |

The check is cheap to re-run whenever a gate is added, and it would have caught
all three instances before field testing.

## H4 — NO_QUORUM after enlistment: record, do not fix

**Residual. Recommend recording alongside P11's coasting-locomotive residual.**

Navigation can degrade to `NAV_NO_QUORUM` during a run. PAUSE then leaves an
enlisted locomotive whose BEGIN is refused (2581) and whose declare-position
control is greyed by R7. Recovery is END → declare → re-enlist — cheap, because
R10 retains orientation and location, but it crosses the release door, which is
the standard Draft-3 F1 declared a trap.

It can only arise from a running episode, so it cannot block T6 or T10, and
closing it properly is entangled with what NO_QUORUM should mean for an enlisted
locomotive — a larger question than this iteration. Recommend recording as an
accepted residual with an owner rather than expanding scope.

## Draft 4 material reviewed and agreed — no action

- **P12** — broadcast set, per-locomotive clear, displayed state from retained
  `state/estop`. The convergent design is right, and sourcing display from
  retained state rather than the last command is a genuine improvement on G1 as
  written.
- **P13 / R14** — refusals bypassing the transition dedup with a sequence number.
  `state/station` has no current console consumer, so there is no compatibility
  risk in adding the field.
- **R15 / P11** — enlistment guards for ORIENTATION and navigation readiness;
  the CODEX-F5 wording narrowing (*propulsion de-energised*, not physical rest)
  is the honest statement, and routing the residual to decision 0005's motion
  witness is correct.
- **P6** — Sam's rewording separates sequencing from authority cleanly.
- **§8** — the G2 fallback (console may ship with T2 blocked pending P11, D-d
  recorded open) is exactly the intent.
- **T10** — proves PAUSE changes operating state while END changes authority.
  Good test; it is the one that would catch a regression of Fault B's class.

## Summary of proposed changes to Draft 4

| # | Target | Change | Type |
|---|---|---|---|
| H1 | P14 | Remove the NEUTRAL write from the **assert** branch (2629), not the clear branch; state that `estopped` is the interlock (1656, 2570) | firmware, blocking |
| H2 | P11 | Add DIRECTION ≠ NEUTRAL to the enlistment guards, with a published reason | firmware, blocking |
| H3 | §8 | Adopt the BEGIN-gate invariant as a standing check; include the eight-gate sweep | spec edit |
| H4 | §5 or residuals | Record NO_QUORUM-after-enlistment as an accepted residual with an owner | spec edit |

## Effect on the field check

No new tests are required. Two existing tests gain preconditions:

- **T6** already asserts "confirm DIRECTION preserved (P14); press BEGIN… confirm
  the locomotive restarts." With H1 unfixed, T6 fails at BEGIN. The test is
  correctly written; it would catch H1 in the garden. Fixing H1 first is cheaper.
- **T3** should be run once with the locomotive parked in NEUTRAL, which exercises
  H2's enlistment guard once added.

## References

`NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md` (Draft 4) ·
`NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT2_REVIEW_20260808.md` ·
`NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT3_REVIEW_{CLAUDE,CODEX,SAM}_20260808.md` ·
`docs/CTO3/AUTHORITY_MODEL.md` · decision 0013 · decision 0005 (motion witness) ·
`firmware/QUORUM/QUORUM.ino` at `8c338c6`
