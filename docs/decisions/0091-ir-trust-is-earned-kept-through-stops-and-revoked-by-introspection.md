# 0091 - IR trust is earned over 10 magnets, kept through stops, and revoked only by introspection

**Superseded in part by 0092** (2026-09-22): see 0092 for the NAVI / IR decision model the operator adopted with Sam.
Status: Proposed (2026-09-22). These are the operator's rulings from a chat
review of the day's field runs. They are not authoritative until the operator
ratifies this record. Mechanisms are **not chosen here**: they are for Sam,
the primary author of NAVI_COHERENCE.
Decided by: David. Framing: Sam's "Transparent Judgment", endorsed by David.
Recorded by: Claude, with field evidence attached.

## Governing concept: Transparent Judgment (Sam, endorsed by the operator)

> NAVI is the decision authority, but NAVI's decisions are made by explicit,
> inspectable rules applied to all available evidence. An individual sensor
> does not acquire decision authority merely because a rule currently uses its
> output. When no established rule resolves the available evidence, NAVI
> preserves the uncertainty rather than inventing a judgment.

Rules are added from field cases: run, meet an unresolved pattern, find what
physically happened, then write the rule. Evidence is never weighed or voted.

## The operator's rulings (verbatim, 2026-09-22)

1. "IR data should not be ignored if NAVI knows that PWM <20."
2. "NAVI should see IR data at standstill as consistent with state of
   locomotive."
3. "It should not be a hard rule. NAVI should trust IR report of movement once
   established, unless, like happened in CCW round at PATIO station, the IR is
   totally inconsistent. If PWM is decreasing, then it is expected that
   movement will decrease and stop. IR is the best measurement of that. Dont
   turn it off."
4. "If IR says stopped and PWM is >20 or at cruise speed, it could mean many
   things, all bad. NAVI do a bit of introspection. Are MM Hall signals
   progressing? If they are, then IR is invalid. The run should continue."
5. "If Hall not progressing: SHUTDOWN"
6. "A stop and restart should initiate resumption of trust after the IR
   calibration. That is the case for requiring recalibration. Once an IR is
   installed on the car itself, the failure modes will be reduced."
7. IR calibration means "a correspondence between the IR signal and the first
   10 hall hits like at startup."

Ruling 3 replaced rulings 1 and 2 as a *hard* rule. PWM 20 is context for
NAVI, not a gate.

## The IR model these rulings define (policy)

1. **Earn trust.** At startup, IR is not counted until its distances
   correspond to the surveyed spans over the first **10 Hall-confirmed
   magnets**. This mirrors the 10-magnet sequence rule of 0090.
2. **Keep trust through slowdowns and stops.** Once established, IR's report
   of movement is trusted. That includes decreasing movement as PWM falls, and
   standstill. IR at standstill is consistent evidence (zero travel), not
   missing data.
3. **Introspection on contradiction.** If IR reports stopped while PWM is
   driving (above 20, or at cruise):

   | Hall magnets | NAVI's ruling | Action |
   |---|---|---|
   | Progressing | IR invalid | Continue on Hall + map + sequence; announce it |
   | Not progressing | Stall, derailment or slip | **SHUTDOWN** |

4. **Resume trust.** An IR declared invalid stays invalid until a stop, a
   restart, and a fresh 10-magnet calibration.
5. **Direction of travel.** IR mounted on the locomotive itself, with no
   separate car, removes the "car left behind" failure.

## Field evidence (2026-09-22)

Logs are in `field-records/logs/20260922_navi_coherence_*`. Write-ups:
`docs/NAVI_COHERENCE_0_4_FIRST_LAP_20260922.md`,
`docs/NAVI_COHERENCE_0_4_SECOND_RUN_20260922.md`, and
`docs/NAVI_COHERENCE_0_5_FIRST_AUTO_RUN_20260922.md`.

**A. Arches departure, CW, 15:02: IR turned off at the one moment it was
needed.**
- Last good anchor: MM109 at 15:01:46.775, about 13,101 pulses.
- IR stopped reporting TRACKING at 15:01:51 while PWM was still 27. It came
  back at 15:02:07.847.
- Serial 243 (15:02:07.587, about 13,128 pulses) was accepted as MM110 on
  timing. Serial 245 (15:02:08.943, peak 38, about 13,133 pulses) was
  accepted as MM111 on timing. That was **false**. The 0.5 sequence rule
  corrected it four magnets later, MM117 → MM116.
- **Under ruling 3:** measured from MM109 across the dwell, serial 243 is
  about 265 mm against a 300 mm span (0.88), so it is accepted as MM110.
  Serial 245 is about 48 mm further on, so it is refused. **No false advance.**

**B. Patio, CCW, 15:07: IR car left behind (the "totally inconsistent" case).**
- The car's pulse count froze at 17,800 at 15:07:32, during the dwell. The
  radio link stayed up, so the car was standing, not disconnected.
- Toby departed and served Bamboo, Arches and Grillers on Hall alone, with
  every advance agreeing on polarity.
- 0.5 fell back silently: the interval was voided as `OPTICAL_INVALID`.
  **Under ruling 4:** "IR stopped, PWM driving, magnets progressing" names
  the IR invalid and announces it.

**C. Slow Manual, 15:21:04, serial 721: IR prevented a false advance.**
- A weak reading (peak −40) came 700 ms after a genuine magnet, so it passed
  the 500 ms timing gate. IR measured 38.6 mm, and the reading was refused.
- This is the same pattern as serial 245 in case A, with the opposite outcome,
  because IR was admitted.

**D. Movement against PWM (today's AUTO run, IR car coupled).**

| PWM | Samples with pulses | Mean speed |
|---|---|---|
| 0–19 | 8 / 560 | ~1 mm/s (coast after a ramp-down, or handling) |
| 20–24 | 46 / 81 | ~16 mm/s |
| 25–29 | 48 / 59 | ~25 mm/s |
| 60–64 | 662 / 662 | ~230 mm/s |

## What 0.5 does today, compared with the model

| Model | NAVI_COHERENCE 0.5 |
|---|---|
| Earn trust over 10 magnets | IR is counted from the first anchor after declaration |
| Keep trust through slowdowns and stops | Any non-TRACKING sample voids the whole interval (`between()` in `MovementEvidence.h`). Every stop voids IR |
| Introspection on contradiction | No check. A stationary car self-invalidates silently; no SHUTDOWN path exists |
| Resume after stop, restart and calibration | Resumes automatically as soon as two TRACKING endpoints recur |

The car side also drops IR at crawl. The detector disarms when its 512-sample
(0.5 s) contrast window collapses and must see a full pulse before TRACKING
returns. A phase-retention mode (`Detector(true)`) exists but is off. Its report,
`docs/IR_PHASE_RETENTION_EXPERIMENT.md`, shows it helped in simulation, made no
difference on a real recorded run, and could produce false counts under
changing light.

## Open questions for Sam (mechanism)

1. **Calibration.** Is it an agreement check (each of the 10 spans inside the
   window), or does it also fit the session's mm/pulse from the 10 spans? The
   fixed pitch is 9.652 mm; today's median IR/map was 0.997.
2. **"IR reports stopped"** and **"Hall progressing"** need operational
   definitions. Examples: no IR pulses across one Hall-confirmed span; or N
   seconds at PWM ≥ 20 with no pulses and no Hall event.
3. **Keeping trust through a stop** requires using the pulse count across
   non-TRACKING stretches. Pulses the detector misses while re-arming make IR
   read slightly short on the departure span. Case A read 0.88.
4. **SHUTDOWN.** Recorded here as the existing controlled safety stop: ramp
   to zero, AUTO withdrawn, a sticky warning, and wait for the operator. The
   operator has not ruled out an immediate cut.
5. **Still unresolved:** the rule for "IR disagrees, but polarity and timing
   agree". Today the ±15% window refuses, which gives IR a veto. There is one
   case from today (MM122, 0.837) and four from run 2 (at ±10%).

## Implications and unintended-consequence risks

- **For the first 10 magnets after startup, and after each recalibration, IR
  is not counted.** A case-A departure error can happen in that stretch. The
  0090 sequence rule is the net, and today it caught one within four magnets.
  **Operator's assessment (2026-09-22): "I think that first 10 magnets is low
  risk."** Sam: no extra mitigation is requested for this window.
- **Trusting IR through stops depends on the car's count being honest at
  crawl.** A count that misses pulses reads short and could refuse the first
  real magnet after departure. The window then recovers it with a two-step
  advance, as it did for MM122.
- **SHUTDOWN by introspection** needs IR stopped, PWM driving, and no Hall
  progress all at once. A failed Hall sensor with the IR car left behind
  would also trigger it. That is the correct outcome, but the operator should
  expect it.
- **An IR invalidated mid-run stays out until the next stop and restart.**
  If the operator recouples the car without stopping, the rest of the run is
  Hall-only by design.
- **These rulings cut into the 0.5 mechanisms,** so 0090's IR window
  behaviour should be re-read against this record once it is ratified.
