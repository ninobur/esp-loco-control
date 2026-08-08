# Stage C field verdict — QUORUM 1.8 acceptance matrix

Date: 2026-08-07
Firmware: QUORUM_1_8 (`daf468d`), flashed ~12:30
Protocol: `QUORUM_1_8_FIELD_TEST_PROTOCOL.md` §5
Evidence: `field-records/logs/20260807_C_acceptance_1_8.log`,
`…_part2.log`
Operator: David. Locomotive: Otto (9950011), Lowline, CCW.
Conditions: Stevenson Ranch, ~13:00 PDT, high heat — session ended on a
derailment (see C5).

## Verdict: **PASS — all five rows, including the residual.**

| Row | Condition | Observed | |
|---|---|---|---|
| C1 | Clear track | baseline steady 2024–2025 | PASS |
| C2 | Fringe field, Δ −3…+7 | 79 s parked, baseline **2024**, single value | PASS |
| C3 | North magnet, Δ +73 | 196 s parked, baseline **2023**, single value | PASS |
| C4 | South magnet, Δ −65 | 88 s parked, baseline **2023**, single value | PASS |
| C5 | Stall above dead zone | baseline **migrated 2043→2097** | PASS *(residual reproduced as designed)* |

## C3 — north magnet, the headline row

Parked 12:36:01–12:39:17, **196 seconds**, Δ +73 held steady.

```
distinct baseline values during the entire dwell: ['2023']
```

One value. For comparison, the same experiment on 1.7 ninety minutes
earlier (`QUORUM_1_8_STAGE_A_VERDICT.md`) collapsed 2019 → 1892 within
33 seconds. 196 s is ~6× past 1.7's failure point.

### Saturation checklist (CODEX review item #3) — verified from the marker line

Two independent north-magnet dwells, both clean:

```
12:41:50  mm=112  obs=N  ms=65535  gate=LOW_PWM  dt=694
12:41:50  mm=111  obs=S  ms=306    gate=RAMP     dt=65535
12:41:51  mm=110  obs=N  ms=234    gate=NO_PREV  dt=2028
12:41:53  mm=109  obs=N  ms=225    gate=ACTIVE   dt=1741

12:51:17  mm=112  obs=N  ms=65535  gate=LOW_PWM  dt=45898
12:51:20  mm=111  obs=N  ms=261    gate=RAMP     dt=65535
12:51:21  mm=110  obs=N  ms=202    gate=NO_PREV  dt=1885
12:51:23  mm=109  obs=N  ms=208    gate=ACTIVE   dt=1464
```

- [x] Exactly **one** event for the parked magnet
- [x] `ms == 65535` (u16 saturated, as predicted)
- [x] Polarity = arrival pole (N, matching the positive delta)
- [x] MM advanced by exactly one (112 → 111)
- [x] Successor markers normal short-duration `ACTIVE` readings — **no
      phantom, no flood**
- [x] Stale arrival timestamp benign — no station or age anomaly

This is the exact inverse of 1.7's departure signature (opposite-polarity
phantom + 2.3× event flood).

## C2 — the fringe row, and why the motion gate beat the excursion gate

Parked 13:09:52–13:11:11 with the sensor mount half over a magnet: Δ
ranged −3…+7 — real field, far below the ±38 entry threshold. Baseline
held at **2024**, one value.

This is the row the spec called out as decisive between the two designs.
An excursion gate keyed to ±38 would have admitted every one of these
samples and let them accumulate; the motion gate is indifferent to field
strength and holds regardless. Argument now has evidence behind it.

## C5 — the residual, reproduced by accident

**Not run deliberately.** During C5 setup Otto derailed with the motor
still running, wheels free, sensor over a magnet — which is precisely the
stall condition. The log caught it:

```
13:18:41  base=2043  raw=2096  d=+53  NO_QUORUM  pwm=60
13:18:59  base=2045
13:18:59  base=2083            ← median crosses over
13:19:00  base=2094
13:19:43  base=2097  raw=2097  d= 0  ← reference has become the magnet
```

2043 → 2097 in ~4 seconds, the same cliff shape as Stage A, opposite
direction. `actualPwm` 42–60 was above the dead zone, so the gate believed
he was moving and kept feeding the median. **This is the residual decision
0017 names and accepts** — "believed moving" is belief, not measurement.
It is now evidence-backed rather than theoretical, and it is the second
field example today arguing for decision 0005's motion witness (the first
being the patio-table phantom markers at 46 pKPH while stationary).

Note the recovery at 13:21:44: once rolling, **2097 → 2044 → 2026 in about
three seconds**. The gate does not impede healing; motion restores the
reference immediately.

## Session totals on 1.8

`20260807_C_acceptance_1_8.log`: 382 AGREE / 10 DISAGREE, one
`QUORUM_OPEN → ADOPTED → CLOSED` self-recovery. The best sustained
stretch was 62 consecutive AGREEs with zero disagreements and a
one-count baseline spread.

Disagreements and the two NO_QUORUM entries are attributable to operator
hand-repositioning (see `QUORUM_HAND_REPOSITION_HAZARD.md`) and the
derailment, not to the gate.

## Not covered

- **Stage D regression lap** — not run as a dedicated row. Partial
  coverage exists: LOW_PWM-gated markers navigated correctly throughout,
  and loop/hall task gaps stayed at the 2026-08-06 baseline. A clean lap
  should be recorded when convenient.
- **AUTO operations under QUORUM — never exercised.** Everything above is
  MANUAL. See `NGR_DASHBOARD_FINDINGS_20260807.md`.

## Still open, unrelated to 1.8

The online/stale telemetry gaps persisted all session (no reboots — uptime
ran 56 min continuous at 13:29). Separate investigation, untouched.
