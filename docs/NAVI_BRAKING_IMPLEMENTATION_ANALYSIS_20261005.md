# NAVI Braking — Implementation Analysis (2026-10-05)

Status: Analysis and proposal for review. **Not an approved implementation.**
No firmware was modified, flashed, or deployed.

Governing architecture: [`NAVI_INTELLIGENT_BRAKING_ARCHITECTURE_20261005.md`](NAVI_INTELLIGENT_BRAKING_ARCHITECTURE_20261005.md)
(rev 1 `f3149a8`; rev 2 adaptive stopping model `97164ec`). Where this analysis
and the architecture differ, the architecture governs.

Author: Claude (analysis requested by David). Code inspected at
`origin/codex/ewo-pwm-zero-localization` tip `dadafa9`, unless stated.

---

## 1. Repository findings

### 1.1 `EwoStationStop` is not in the repository

The experimental IR speed-servo stopping controller (`EwoStationStop`) whose
field undulation motivated the braking architecture could not be found. Searched:

* every `origin` branch (37 at the time);
* all PR refs (`refs/pull/1..8/head`);
* all tags;
* full history via `git log --all -S EwoStationStop`.

No match. It presumably exists only in a local working copy or an unpushed
branch. Consequently the requested analysis of *which parts implement the
rejected instantaneous speed-servo concept* and *which IR distance/speed
plumbing remains useful* in that code **could not be performed**. Nothing in
this report should be read as a description of that code.

### 1.2 EWO work is on unmerged, divergent branches

* `codex/ewo-pwm-zero-localization` (tip `dadafa9`, 2026-10-02) carries the
  newest EWO firmware: retaining the EWO interval after PWM-zero movement,
  requiring operator declaration after PWM-zero IR movement, and the MM011–MM014
  survey correction.
* `claude/trusting-wright-m311vv` (tip `ead5c4f`) carries later documentation:
  decisions 0118–0120, the firmware build register, Otto R2_FT2 station-run
  records.
* Neither branch contains the other. Both contain `ab0938b` (R2_FT2, flashed to Otto).

### 1.3 Architecture documents are not on the EWO lineage

`claude/keen-keller-v7kbqp` was cut from `main` (2026-09-25), which contains no
EWO work. The NAVI CTO note, the CTO2 scope clarification, the braking
architecture, and this report therefore sit on a branch that does not contain
the code they govern. No merge, rebase, or move was performed (AGENTS.md §3).

---

## 2. Current manual deceleration ramp

Source: `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino`
(`throttle` command, `brakeStepMs()`, `requestPwm()`, `serviceRamp()`).

| Property | Value |
|---|---|
| PWM step size | always 1 count; manual range clamped 0–255 |
| Ramp-up interval (manual) | `MANUAL_STEP_UP_MS` = 150 ms/count |
| Ramp-down interval (manual) | `rampDownMs = 0` → `brakeStepMs()` |
| `brakeStepMs()` | `400 − (385 · brake) / 255` ms/count (integer arithmetic) |
| Brake 0 | 400 ms/count (`BRAKE_STEP_COAST_MS`) |
| Brake 255 | 15 ms/count (`BRAKE_STEP_HARD_MS`) |
| Brake input | MQTT `brake` 0–255, published `state/brake`; takes effect on the next step, including mid-ramp |
| Approaching zero | no special handling — linear 1-count steps to 0; no stall-floor jump |
| E-stop | immediate PWM 0 |
| Low voltage | target 0 at `AUTO_STEP_DOWN_MS` = 31 ms/count |

For comparison: AUTO ramp-down 31 ms/count; current station zero-ramp
200 ms/count (`STATION_STOP_STEP_MS`).

**Observation.** The existing mechanism already matches the architecture's
actuator model: brake modulation *is* ramp-down slope, and a ramp toward a
target of 0 can only decrease PWM. PWM could increase during braking only if
some other code path raised the ramp target.

---

## 3. Current station-stop implementation (pushed EWO firmware)

The pushed EWO firmware does **not** use an IR-based stop. `serviceStation()`
drives `StationMachine` from
`firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/Stations.h`:

| Phase | Behavior |
|---|---|
| APPROACH (Station −10 … −6) | linear PWM descent to station PWM, paced by per-locomotive marker-time table `NAVI_APPROACH_MARKER_MS` |
| ZONE | hold per-station, per-direction station PWM (`pwmCW` / `pwmCCW`) |
| ZERO_RAMP | begins at per-station, per-direction `stopOffsetCW/CCW` (seeded +1 past centre); PWM to 0 at 200 ms/count |
| DWELL | begins when `actualPwm == 0` |
| DEPART | departs to per-station/direction PWM; completes at `stopAt + 3` |

Relative to the braking architecture (rev 2), the current machine:

* uses PWM as the operating language and per-station/per-direction PWM tables
  (architecture: PWM is an actuator output; braking must be adaptive, not
  station-calibrated);
* treats `actualPwm == 0` as the end of the stop (architecture: PWM = 0 is not
  the definition of physical stop; physical stop is terminal);
* uses no IR evidence during braking.

Side observation, not investigated: the `Stations.h` header comment describes a
30 s dwell; the constant `STATION_DWELL_MS` is 5000 ms.

---

## 4. IR evidence available during braking

Sources: `NaviIntegratedCore.h`, `firmware/common/IrMovementWire.h`.

* **Pulse resolution.** `WireSnapshot.pitchUm = 9652` → 9.652 mm per completed pulse.
* **What a packet carries.** Cumulative `completedPulses`, `nominalUm`,
  `capturedUs` (transmitter clock), plus health counters. **No per-pulse edge
  timestamps.** NAVI therefore gains evidence per packet, not per pulse.
* **Cadence.** Approximately 100 ms per packet, per the core's comment
  ("0/1/2-pulse, 100-ms speed steps"). **Not verified** in the IR transmitter sketch.
* **Measured speed.** `irSpeedMmS()` keeps a 16-point ring and computes speed
  against the most recent point 0.9–1.5 s old — effectively a ~1 s trailing
  average, recomputed on each packet; cleared on discontinuity/order fault;
  usability gated by `irApplicable()`.
* **Units.** 5.37325 mm/s per house pKPH (`server/ir_hall_live.py`, "matches
  navigation firmware").
  * 25 pKPH ≈ 134 mm/s ≈ 13.9 pulses/s ≈ 1.4 pulses per packet.
  * A uniform deceleration from 25 pKPH over 337.8 mm lasts ≈ 5.0 s ≈ 50 packets.
  * The ~1 s speed window is therefore ~20 % of the whole final-braking manoeuvre.
    This is consistent with rev 2's direction to avoid heavily backward-looking
    filtering.
* **Retained history.** The 16-point speed ring, plus the recorder, which logs
  each IR packet with `pwmAtReceive` / `commandedAtReceive`.

### 4.1 Interaction with decision 0120 (PWM-zero movement)

In `NaviIntegratedCore`, any IR packet showing advanced pulses while
`pwm == 0` (and position declared) increments `pwmZeroDisplacements`, requires
operator declaration, and causes the sketch to withdraw AUTO.

If commanded PWM reaches 0 while the locomotive is still coasting, the final
coast pulses would trigger that hold. The architecture commands PWM 0 only after
physical stop is recognized, but a false early stop recognition (slow creep
between pulses) would then produce the same outcome.

Whether coast pulses inside a braking manoeuvre are exempt from 0120 is a
decision for David. **No change to the 0120 rule is proposed here.**

---

## 5. Proposal status after architecture rev 2

An implementation proposal was presented in conversation on 2026-10-05, before
architecture rev 2. Its status:

### 5.1 Superseded by rev 2 — do not implement

* **Fixed stall-PWM projection.** The proposal estimated stopping distance as
  `d_proj = d + v · t_rem / 2`, with `t_rem = (P − P_stall) / R` and `P_stall`
  a measured per-locomotive constant obtained by replay or coast-down test.
  Rev 2 rejects fixed assumptions about the PWM at which a locomotive stops and
  requires the response to be observed within the current stop
  (Δ IR speed / Δ PWM). **Superseded.**
* **Per-locomotive calibration of `R_nom` and `P_stall` as control data.**
  Rev 2: field data may evaluate the architecture but must not become required
  location- or locomotive-specific control data unless evidence shows it is
  necessary. **Superseded** as framed; a generic nominal ramp remains.

### 5.2 Still consistent with rev 2 — candidate elements for a future estimator review

These are recorded as candidates only. The architecture deliberately leaves the
predictive estimator to a separate implementation design review.

* **Ramp mechanism.** Hold the ramp target at 0 and adjust only the ramp-down
  interval through the existing `requestPwm` / `serviceRamp` path. PWM is then
  monotonic by construction.
* **Guard.** The idle-cruise path in `serviceStation()` must not raise the
  target during braking; telemetry should flag any `actualPwm` increase.
* **±20 % bound applies to rate.** Rate `R` ∈ [0.8 · R_nom, 1.2 · R_nom]. In
  step-interval terms this is [0.833×, 1.25×] the nominal interval, not ±20 % of
  the interval.
* **Modest stepwise adjustment.** E.g. ±5 % per evaluation, rate-limited (on
  the order of 200–300 ms), clamped to the bound. Values are placeholders for
  review, not proposals of record.
* **Physical-stop recognition by absence of further pulses**, confirming a
  forward prediction (rev 2). The no-pulse interval must come from track
  evidence; the creep/false-stop risk and the §4.1 interaction apply.
* **Dwell begins at recognized physical stop**, not at `actualPwm == 0`.
* **IR-unusable fallback.** Stop adjusting, continue the ramp at the current
  bounded rate, and report `NO_EVIDENCE`. Holding the current rate versus
  reverting to nominal is David's choice.
* **Telemetry.** A compact `state/brake` JSON per evaluation with the fields
  listed in the architecture, plus recorder actions. Publish rate must respect
  `PUB_QUEUE_DEPTH = 16` × 1273-byte entries (known heap pressure on Otto).
* **Structure.** A pure, host-testable header (as `Stations.h` is), with host
  tests for PWM monotonicity, ±20 % enforcement, stop recognition, and IR-loss
  fallback; behavioral validation on the track (AGENTS.md §8).
* **Sequencing (suggestion).** Region 3 (final braking) first as a bounded
  track test, since that is where the undulation was observed; Regions 1–2
  initially unchanged or on a simple station speed.

### 5.3 Open design question raised by the architecture

Region 2 requires maintaining approximately 25 pKPH from Station −5 to
Station 0. The architecture does not say how. A fixed PWM chosen to give about
25 pKPH would amount to a station calibration, which rev 2 discourages. A speed
governor risks reintroducing speed-chasing. The method is David's decision.

---

## 6. Architecture rev 2 — summary of changes

Rev 2 (`97164ec`) added: the human-engineer model; the adaptive / works-anywhere
requirement; "ramp control, not speed control"; the governing adaptive
relationship Δ IR speed / Δ PWM (not a locomotive constant); forward-looking
judgment and the caution against heavy filtering; the
OBSERVE → JUDGE → ADJUST RAMP → OBSERVE AGAIN cycle; predicting physical stop
(PWM = 0 is not physical stop); the principle statement "The ramp provides
smoothness. IR provides evidence. NAVI provides judgment."; architecture versus
implementation; and an explicit superseded-concepts list.

Wording from rev 1 replaced in rev 2:

* "know approximately how the locomotive normally coasts/decelerates" — removed;
  could be read as a previously learned braking model;
* IR evidence list "recent speed history", "recent ramp history" — replaced by
  "recent change in IR speed" and "recent change in PWM" within the
  forward-looking judgment;
* "nominal/planned ramp rate" → "nominal ramp rate";
* Region 2 "operates at 25 pKPH" → "maintains approximately 25 pKPH";
* final-braking step 5 expanded from classification alone to comparing the
  physical response with the recent PWM reduction and estimating the stopping
  location.

One sentence in rev 2 is Claude's inference rather than David's text: "The PWM
at which movement ceases is itself an observation of the current stop, not a
fixed locomotive constant." It is flagged for David's confirmation or removal.

The superseded concepts in rev 2 (median-of-three, PWM-zero stop, IR chasing a
speed target, exact checkpoint, station calibration) did not appear in rev 1.
They correspond to:

* the current `StationMachine` (per-station/direction PWM; dwell at PWM 0);
* the conversation proposal (§5.1);
* presumably the unpushed `EwoStationStop` code (median-of-three was not seen
  in any pushed code).

---

## 7. Open decisions for David

1. Where is `EwoStationStop` (branch, commit, or local file)? Please push it so
   §1.1's analysis can be completed.
2. Which branch is the current EWO base for governance linkage and future implementation?
3. Governance linkage (not yet made):
   * a pointer in `docs/CLAUDE.md` and in
     `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md` requiring agents
     to read the braking architecture before modifying station stopping/braking;
   * optionally, a numbered decision record on the EWO lineage (the latest seen is 0120).
4. Are coast pulses within a braking manoeuvre exempt from the decision 0120 PWM-zero hold?
5. How is ~25 pKPH maintained in Region 2 (§5.3)? Should Region 3 be built first?
6. Confirm or remove the Claude-inferred sentence noted in §6.
7. Approval to proceed to a separate estimator design review against rev 2.
