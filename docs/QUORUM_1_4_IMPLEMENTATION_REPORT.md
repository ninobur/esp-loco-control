# QUORUM 1.4 — implementation report: constitutional hardening

**Describes commit 1c5bad2 (QUORUM 1.4) and the R21 erratum (5981a65).**
Follows CODEX's ratification of the bicameral property and the 1.3 terminal
gate; CODEX conditioned formal §8 clearance on one hardening and two wording
corrections, all made here.

## The three changes

1. **M+1 fallback explicitly gated** (CODEX reclassification of audit site
   3): it is AUTO-chamber station automation, chamber (a), not a motor-safety
   fact — §0.2 admits no navigation-originated ungated motor write except
   E-stop. The timeout condition now leads with `autoRunning`. No currently
   reachable behaviour changes — the invariant (non-IDLE station phase
   implies AUTO, because every path clearing `autoRunning` also calls
   `stationReset()`) holds today — but the explicit gate means a future
   station-reset regression cannot violate the constitution silently.
2. **Terminal-entry log honesty**: `enterNoQuorum()` captures the chamber at
   entry (`wasAuto`) and prints "AUTO stop requested; operator declaration
   required" or "MANUAL, motor unchanged; operator declaration required".
   The old unconditional "stopped" was false in MANUAL.
3. **Spec R21 erratum** (no revision bump): §2.5's marker-handling passage
   now states that in AUTO the locomotive is decelerating under the stop
   request while in MANUAL it continues at the operator's unchanged
   throttle; a changelog line under R21 records both CODEX wording
   corrections.

## Amended audit table (supersedes the 1.3 table; site 3 reclassified)

Line numbers are QUORUM 1.4. Classes: (a) AUTO chamber, gated on
`autoRunning`; (b) operator chamber, never gated.

| # | Line | Site | Class | Gating | Verdict |
|---|---|---|---|---|---|
| 1 | 872 | `enterNoQuorum()` terminal stop | (a) | `if(autoRunning)` (1.3) | gated |
| 2 | 1486 | `serviceStations()` PHASE_TIMEOUT return-to-cruise | (a) | `if(autoRunning && navPositionUsable())` | gated |
| 3 | 1499–1503 | M+1 zero-ramp fallback (`ST_FINAL`) | **(a) — reclassified from (c) by CODEX** | **`if(autoRunning && stPhase==ST_FINAL && …)` (1.4)** | **gated** |
| 4–12 | 1529–1614 | station machine ramps (arm, cruise-follow, overshoot escape, approach, zone hold, final, M+2 zero-ramp, depart) | (a) | `serviceStations()` entry guard `if(!autoRunning ‖ !navPositionUsable() ‖ navDir==MAP_UNSET) return` | gated |
| 13 | 2206 | `cmd/auto` 0 → stop | (b) | none, by design | correct |
| 14 | 2229 | `cmd/go` launch | (a) | GO refusal chain; sets `autoRunning=true` first | gated |
| 15 | 2234 | dispatcher STOP | (b) | none | correct |
| 16 | 2253 | dispatcher RELEASE (END CTO) | (b) | none | correct |
| 17 | 2275 | `cmd/throttle` | (b) | `!autoRunning && !estopped` | correct |

Footnote: the E-stop path writes `commandedPwm`/`actualPwm` directly — a
deliberate, documented exception to the `requestPwm()` authority rule —
operator chamber, never gated, untouched.

**Every navigation-originated motor write is now lexically gated on
`autoRunning`. Class (c) is empty.**

## Verification

- Both configs compile: **951,951 bytes flash (72%), 50,596 bytes static
  RAM (15%)** — +104/+0 over 1.3 (the second log string and the gate).
  `LocoConfig.h` restored, empty diff.
- Identity: header line 3 and `SKETCH_NAME` both `QUORUM_1_4`.
- `grep requestPwm` matches the table above: navigation-originated sites
  gated, operator sites ungated, E-stop direct-write untouched.

## Status

CODEX's stated conditions for formal §8 clearance are met. Field/replay
items remain as listed in `docs/QUORUM_1_0_IMPLEMENTATION_REPORT.md` with
the §8 NO_QUORUM item in its R21 form. No tag until the field.
