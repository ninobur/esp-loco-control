# QUORUM 1.10 — console-authority firmware items: implementation report

Date: 2026-08-09
Spec: `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md` Draft 5.1 (five drafts,
eight reviews; CODEX disposition: approved for implementation)
Rulings implemented: R12/R15/H2 (P11), R14 (P13), R13 (P14)
Baseline: QUORUM 1.9 (`ede7a08`, itself awaiting CODEX commit review)
Status: **built both profiles, committed. NOT flashed.**

## What changed

**P11 — enlistment guards** (`T_CMD_AUTO` handler). `cmd/auto 1` is now
refused, each with a published `ENLIST_REFUSED` + reason on
`state/station`, in order: energised (`WAIT_FOR_STOP`); orientation unset
(`NO_SESSION_DIRECTION`); NEUTRAL (`NEUTRAL_SELECT_DIRECTION` — the H2
trap); `NAV_UNSET` (`NO_POSITION_DECLARE_START_MM`); `NAV_NO_QUORUM`
(`NO_QUORUM_DECLARE_POSITION`). `NAV_EVALUATING` remains usable, matching
the BEGIN contract. Refusals mutate nothing — the absence of change is
the signal (R8). **`cmd/auto 0` is unconditional** (Claude-G3 invariant):
it drops both flags, zeroes PWM, resets the station machine — unchanged
from 1.9. Successful enlistment behaves exactly as before. Reasons reuse
the existing BEGIN vocabulary, so the console's raw-string display (Q3)
needs one dictionary. Not guarded, deliberately: E-STOP state — spec §8's
gate sweep covers `ESTOP_ACTIVE` via the dispatcher clear path (ii), and
adding unlisted gates would be scope invention.

**P13 — always-observable command responses** (`stationPublish()`).
Events matching `*_REFUSED` or `STOP_IGNORED` bypass the transition dedup
(which compares event+offset and ignores the note) and carry a monotonic
`"seq"` field. The dedup's memory is not touched by responses, so the
transition stream behaves exactly as before. Buffer 256 → 288 for the
seq field; worst-case payload re-checked against the 288 bound.

**P14 — E-STOP preserves DIRECTION** (`T_CMD_ESTOP` handler). The
`DIRECTION_NEUTRAL` writes are removed from **both** branches (Draft 4
had specified clear-only; CODEX-C1/Claude-H1 caught that the assert
branch had already destroyed the value). The interlock is `estopped`:
`servicePwmRamp()` clamps PWM to zero every pass while set, and BEGIN
refuses `ESTOP_ACTIVE`. Assert still drops `autoRunning`, zeroes PWM,
and resets the station machine. Clear now publishes **`ESTOP_CLEARED`**
(was `ESTOP_CLEARED_NEUTRAL` — the old name would now be false). That is
a nav-event vocabulary change; no current console consumes the old
string (v1.10.9 renders no nav-event names), recorded here for log
readers.

Unchanged: navigation, detection, the 1.8 motion gate, the 1.9 station
mission filter, all topics and payload shapes except the two named
additions (`seq` on responses; the renamed clear event).

## Builds

| Profile | 1.9 | 1.10 | Δ |
|---|---|---|---|
| Otto (9950011) | 982 539 B | 982 839 B | +300 |
| Toby (9950012) | 982 475 B | 982 771 B | +296 |

RAM 52 452 B (+8, the seq counter and response flag). Selector restored
byte-identical after the Toby build. All three changes are
profile-independent — both locomotives get identical authority behaviour.

## Field tests unblocked

- **T2** (enlist while energised → firmware refusal) — was blocked on P11.
- **T6** (E-STOP ALL → per-loco clear → DIRECTION preserved → BEGIN
  restarts) — was blocked on P14.
- **T9** (repeat a refused BEGIN → second response visible, `seq`
  advancing) — was blocked on P13.

The full T1–T10 sweep additionally needs console v1.10.10 (next work
item). Trial protocol unchanged: `station-stop-v1/README.md` for the
Arches mission; spec §10 for the authority sweep.

## Not done, deliberately

- The −2 candidate window: **deferred by operator ruling** — six
  candidates retained, hand-repositioning handled operationally
  (`QUORUM_CANDIDATE_WINDOW_ANALYSIS.md` records the ruling).
- Q4b DIRECTION-transfer semantics: open, non-blocking; the GO handler's
  REVERSE→FORWARD behaviour is untouched pending that decision record.
- H4 (NO_QUORUM after enlistment): accepted residual, owner CTO3 /
  Station Stop follow-up.

Handoff: CODEX review of this commit (and 1.9's, still pending) before
any flash.
