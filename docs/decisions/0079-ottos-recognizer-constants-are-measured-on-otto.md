# 0079 — Otto's recognizer constants are measured on Otto, and on Otto only

**Date:** 2026-09-10
**Status:** PROPOSED, with its `NAVI_GUARD_MS 200U` line **superseded by 0081,
2026-09-10**. The remainder is not authoritative until the operator reviews and
approves it. Drafting it allocates it no authority whatsoever, and nothing in
it authorizes an agent to act. **The entry threshold is expressly excluded from
this record** — see "What this record does not decide".
**Follows:** 0057 (the close-to-open guard), 0072/0074 (every judged passage is
kept; shape is recorded, not refused), 0076 (navigation recovers from one wrong
observation).
**Evidence:** `field-records/logs/20260909_survey/` — Otto's bidirectional Hall
survey on the new external-antenna board, PWM 90, CW and CCW, 171/171 position
coverage in both directions. The hardware-consistency cutoff was applied
absolutely: nothing before the 21:04:17 boot of 2026-09-09 enters any figure.
Derivation in `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §3.
**Builds:** `NAVI_ONE_1_0X13_FIELDTEST`, flashed to Otto 2026-09-10 with
operator authorization; boot identity verified from the locomotive itself.

---

## Decision

Otto's NAVI_ONE recognizer block is derived from Otto's own waveforms by
NAVI_ONE's own stated method. It is not Toby's block rescaled.

```c
#define NAVI_RECOGNIZER_MEASURED_ON  9950011UL
#define NAVI_GUARD_MS                500U  // 0081; the former 200U is superseded
#define NAVI_AMPLITUDE_FLOOR         0.34f
#define NAVI_RESIDUAL_CEILING        0.13f
#define NAVI_BOOTSTRAP_GAIN          175U
#define NAVI_AUTO_CRUISE_PWM         90
#define NAVI_APPROACH_MARKER_MS   { 1213, 1340, 1496, 1694, 1952 }
#define NAVI_BASELINE_ADAPT_PWM   24
#define IR_FITTED                 0
```

Approach times come from Otto's measured 1,158 ms per marker at PWM 90, scaled
on his own fit `speed_mm_s = 3.872 x (PWM - 23.6)` — 4,959 phantom-filtered
MOVING rows across 34 PWM bins, predicting that 1,158 ms to +0.8%.
`NAVI_BASELINE_ADAPT_PWM` is that fit's intercept of 23.6 rounded **up**, so the
gate errs toward freezing the Hall reference rather than adapting it on a
locomotive that may not be moving. `IR_FITTED 0` is the operator's declaration,
never a probe.

`MOTOR_DIR_PIN` is 16 on Otto, not 2. His ESP32 was replaced; commit `4f6e0b0`
carried the fix only into the QUORUM profiles, leaving every other Otto profile
pointing at a pin the H-bridge is not on. All eleven were swept on 2026-09-10.

## Context

Toby's profile states the derivation method two lines above a "TO MEASURE THESE
FOR ANOTHER LOCOMOTIVE" note describing a throttle ladder. The ladder was never
run on Toby; his values reproduce to the millisecond from the scaling formula.
An earlier session in this thread proposed running that ladder on Otto and
withdrew it after the operator challenged the premise — at constant PWM 90 the
per-marker median gap already spans 1.60x CW and 1.68x CCW across the circuit,
while lap-to-lap at a fixed marker varies by ~27 ms. A ladder spanning a
modelled 1.63x would have measured the railway, not the locomotive.

## What this record does not decide

**The entry threshold.** The operator has reserved that ruling to himself. The
replay against the complete eligible post-21:04:17 dataset found two
double-count phantoms — mm 68 CW (pk 67, opposite polarity, 41 ms, dt 250) and
mm 100 CW (pk 101, 82 ms, dt 372) — against a genuine steady-state amplitude
floor of 120 and a shortest genuine gap of 917 ms. Entry 38 admits both
phantoms; entry 70, which is what Otto is running, admits one. Evidence and
options are in the field record; no operational threshold has been selected or
committed.

## Consequences, including the unwelcome ones

- A per-locomotive block means the fleet has no single recognizer to reason
  about, and every future locomotive owes its own survey before it can run
  NAVI_ONE at all. The `NAVI_RECOGNIZER_MEASURED_ON` compile guard enforces
  that, which is a feature until the night someone needs a locomotive on the
  track quickly.
- §3.9 records a corroboration that is uncomfortable: Toby's profile fit
  predicts 1,159 ms where Toby actually runs 1,326 (-12.6%), while it predicts
  Otto's measured time almost exactly. The fit written in Toby's profile appears
  to describe Otto. **Toby's own numbers were left exactly as they are** and no
  change to them is proposed here, but the discrepancy is real and should be
  settled before Toby's approach pacing is trusted further.
- `NAVI_RESIDUAL_CEILING 0.13` is carried as a diagnostic reference and cannot
  refuse a passage. Recording it next to values that can refuse invites a future
  reader to assume it gates. It does not.

## References

- `field-records/logs/20260909_survey/`
- `docs/NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md` §3, §3.9
- `firmware/test-programs/NAVI_ONE/LL_LocoConfig_9950011.h`
- Commit `4f6e0b0` (board swap), `c479c2d` (Otto's block), `0f552d2` (pin sweep)

## Review

Unreviewed. Operator has not ruled.
