# QUORUM 1.3 — implementation report: bicameral control

**Describes commit f02efd1 (QUORUM 1.3) and spec amendment R21 (cb8672b).**
CODEX ratification followed: audit site 3 (the M+1 fallback) was
reclassified from motor-safety fact to AUTO-chamber station automation and
explicitly gated in QUORUM 1.4 — the amended table is in
`QUORUM_1_4_IMPLEMENTATION_REPORT.md`. The table below is the 1.3-era
record.

## The ruling and the regression

Operator architectural ruling (2026-08-02, spec §0.2, constitutional): all
NGR locomotive controllers are BICAMERAL. MANUAL — the operator is
sovereign; navigation observes, records, publishes, and warns, but NEVER
writes to the motor. AUTO — navigation acts with full authority. E-STOP
belongs to the operator and works in every chamber and every state.

v2.22 stated this in its LOST handler ("AUTO only. In manual the operator
has the throttle and the navigator does not take it. Navigation observes
always; navigation acts only in AUTO.") and honored it. QUORUM 1.0–1.2
regressed it exactly once: the `NAV_NO_QUORUM` terminal entry called
`requestPwm(0, NORMAL_STEP_MS)` unconditionally. The certified property
"NAV_NO_QUORUM stops once" was reviewed by everyone and gated by no one —
hence the spec-level fix (R21), not a quiet patch.

## The audit — every motor-write call site in QUORUM 1.2

Line numbers are QUORUM 1.2 (pre-fix). Classes: (a) AUTO chamber, must gate
on `autoRunning`; (b) operator chamber, never gated; (c) motor-safety fact,
individually justified.

| # | Line | Site | Class | Gating found | Verdict |
|---|---|---|---|---|---|
| 1 | 843 | `enterNoQuorum()` terminal stop | (a) | **none** | **THE STRAY — the 1.0–1.2 regression; gated in 1.3** |
| 2 | 1454 | `serviceStations()` PHASE_TIMEOUT return-to-cruise | (a) | `if(autoRunning && navPositionUsable())` | correct |
| 3 | 1464 | M+1 fallback zero-ramp in `ST_FINAL` ("a fact about the motor, not about the map") | (c) | lexically ungated — sits above the entry guard | justified, unchanged: fires only when `stPhase==ST_FINAL`, and a non-IDLE station phase implies AUTO by invariant — every path that clears `autoRunning` (`cmd/auto` 0, dispatcher STOP, RELEASE, E-stop) also calls `stationReset()` → `ST_IDLE`, and `enterNoQuorum()` also resets the station machine. Its job is stopping a train mid-station-stop whose M+2 never came |
| 4 | 1490 | station arming approach ramp | (a) | `serviceStations()` entry guard `if(!autoRunning \|\| !navPositionUsable() \|\| navDir==MAP_UNSET) return` | correct |
| 5 | 1498 | idle cruise-follow (section map) | (a) | entry guard | correct |
| 6 | 1508 | overshoot escape return-to-cruise | (a) | entry guard | correct |
| 7 | 1515 | `ST_APPROACH` derived ramp | (a) | entry guard | correct |
| 8 | 1518 | `ZONE_HOLD` | (a) | entry guard | correct |
| 9 | 1525 | `FINAL_APPROACH` | (a) | entry guard | correct |
| 10 | 1541 | `FINAL_TARGET` (M / M+1) | (a) | entry guard | correct |
| 11 | 1547 | `ZERO_RAMP` at M+2 | (a) | entry guard | correct |
| 12 | 1575 | `DWELL_COMPLETE` depart-to-cruise | (a) | entry guard | correct |
| 13 | 2167 | `cmd/auto` 0 → stop | (b) | none, by design | correct — the operator's own act of leaving AUTO |
| 14 | 2190 | `cmd/go` launch | (a) | GO refusal chain; sets `autoRunning=true` first | correct |
| 15 | 2195 | dispatcher STOP | (b) | none | correct — explicit stop command |
| 16 | 2214 | dispatcher RELEASE (END CTO) | (b) | none | correct — hand-back includes the stop |
| 17 | 2236 | `cmd/throttle` | (b) | `!autoRunning && !estopped` | correct — manual only, E-stop precedence |

Footnote: the E-stop path writes `commandedPwm`/`actualPwm` directly — a
deliberate, documented exception to the `requestPwm()` authority rule —
operator chamber, never gated, untouched.

**Audit finding: exactly one stray (line 843). No other site required a
change.**

## The fix (QUORUM 1.3)

The terminal stop became `if(autoRunning) requestPwm(0,NORMAL_STEP_MS);`.
Everything else in terminal entry is unchanged and unconditional in both
chambers: snapshot, retained desired state, decision event, station reset,
diagnostics retention, incident close. In MANUAL the locomotive keeps the
operator's commanded PWM and the operator learns of NO_QUORUM through every
publication channel. The entry comment states the bicameral rule and cites
v2.22's doctrine line.

## Spec R21 (docs/QUORUM_v3_0_implementation_spec.md)

- New top-level **§0.2 Bicameral control (constitutional)** — the doctrine
  verbatim, normative for this and all future navigators.
- §2.5's stop-request step: "AUTO chamber only, gated on `autoRunning`",
  in both the behaviour block and the numbered entry sequence.
- §8 NO_QUORUM checklist item verifies the gate: AUTO stops; MANUAL retains
  the operator's commanded PWM.
- Header Revision → 21 with a changelog paragraph naming the operator
  ruling and the 1.0–1.2 regression it corrects. Body otherwise frozen.

## Verification

- Both configs compile: **951,847 bytes flash (72%), 50,596 bytes static
  RAM (15%)** — +16/+0 over 1.2. `LocoConfig.h` restored, empty diff.
- Identity: header line 3 and `SKETCH_NAME` both `QUORUM_1_3`.
- `grep requestPwm` post-fix matches the table: the terminal stop is the
  gated site; 2186/2214/2233 (operator chamber, 1.3 numbering) remain
  ungated; all station-machine sites remain behind the entry guard.
- Nothing else changed: the 1.3 diff is the header/version block, the gate,
  and its comment.

## Status

Field/replay campaign items remain as listed in
`docs/QUORUM_1_0_IMPLEMENTATION_REPORT.md`, now with the §8 NO_QUORUM item
in its R21 form (verify the gate in both chambers). No tag until the field.
