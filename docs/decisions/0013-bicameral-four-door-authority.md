# 0013 — Bicameral locomotive authority with four crossing doors

Status: Accepted (2026-08-05). Implemented in QUORUM 1.6.
Supersedes: earlier implicit "smart manual" behavior in dashboard v1.10.x.

## Decision

Every NGR locomotive control sketch is **bicameral**: one firmware with two
propulsion authorities.

- **MANUAL** — the operator controls propulsion directly and is sovereign. No
  automatic layer may command PWM in manual.
- **AUTO** — onboard software controls propulsion while enrolled and running.

The chambers interact through **exactly four doors, and no others**:

1. **E-STOP** — works from either chamber, in every state, and overrides
   everything.
2. **Enrollment (manual → auto)** — the locomotive's own act (`cmd/auto`),
   after which a dispatcher GO may launch it. Auto cannot seize an un-enrolled
   locomotive.
3. **Release (auto → manual)** — dispatcher END/RELEASE drops enrollment and
   running deterministically and returns propulsion to the operator.
4. **Dispatcher STOP** — halts an *enrolled* auto locomotive only; a manual
   (un-enrolled) locomotive ignores it. The auto side must never zero a manual
   throttle.

Navigation observes in both chambers. Manual is navigation-*aware* but not
navigation-*dependent*. A powered locomotive broadcasts self-truth in every
chamber and motion state (see `docs/CTO3/CTO3_INTENT_BASELINE.md`).

## Context

The dashboard's v1.10.x line progressively added confirmation-gating, motion
locks, and "wait for confirmation" logic to the manual controls. Each guard was
individually justified but collectively degraded manual control from
"delightful" to, at worst, inert — the dashboard inserting itself between the
operator's hand and the motor. Separately, QUORUM 1.2 was found to call
`requestPwm(0)` on NO_QUORUM *unconditionally*, meaning the navigator could stop
a locomotive the operator was driving in manual. Both are the same error: an
automatic authority reaching into the manual authority.

The operator's ruling: overrides have no place in a manual control system, and
the two halves interact only through explicit, named doors.

## Alternatives considered

- **Single unified control with "smart" assistance** (the v1.10.x direction) —
  rejected: it repeatedly produced automatic behavior intruding on manual, and
  no amount of per-case patching removed the class of bug.
- **Two separate sketches (a manual-only and an auto-only)** — rejected: Claude
  Code briefly produced a manual-only sketch; the operator clarified there must
  be exactly one sketch that is bicameral, so either chamber is usable from the
  same firmware without reflashing.
- **Auto always able to stop for safety, even in manual** — rejected: E-STOP
  already covers the operator's own emergency authority; a navigator stopping a
  manually driven train is precisely the override the ruling forbids.

## Consequences

- In MANUAL, no navigation or dispatcher path may write PWM. Enforced lexically:
  every `requestPwm` site gated on `autoRunning` except operator sites and the
  E-STOP direct-write (audited in QUORUM 1.3/1.4, CODEX-ratified).
- The dashboard's manual controls must never be disabled except by AUTO mode
  (dashboard v1.10.5+). Stale telemetry / unknown motion may show a reminder,
  never a block.
- A dispatcher STOP must check enrollment before acting (QUORUM 1.6 chamber
  boundary).
- This doctrine is constitutional: it is normative for QUORUM and every future
  navigator/control sketch, and is recorded in the QUORUM spec §0.2 and the
  CTO3 intent baseline §2.

## References

- QUORUM spec §0.2 (bicameral control)
- QUORUM 1.3/1.4 requestPwm audit; QUORUM 1.6 chamber-boundary commit
- CTO3 intent baseline §2; CTO3 spec §2
- Dashboard v1.10.5 (manual controls never disabled)
