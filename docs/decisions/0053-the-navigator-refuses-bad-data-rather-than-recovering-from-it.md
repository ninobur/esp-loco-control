# 0053 — The navigator refuses bad data at the input rather than recovering from it downstream

Status: Accepted (operator, 2026-08-28)

## Decision

NAVI 2.0 replaces the QUORUM navigator. Position advances only when a magnet
passes a conjunctive identity test against the single marker the map predicts.
Every mechanism that existed to survive bad data is deleted, not disabled.

The whole contract:

1. Position must be **declared**. Until then nothing advances, whatever arrives.
2. Exactly **one target** per event: `nextMm(navMm, navDir)`. Never a list.
3. Identity is **conjunctive**. Every test with evidence must pass; a test
   without evidence abstains and never votes in favour.
4. Pass → `navMm` advances by exactly one. `acceptEvent()` is the only writer.
5. Fail → `navMm` does not move, the reason is published, and in AUTO the
   locomotive ramps to a stop.
6. Nothing else exists.

Two tests remain: **entry threshold** (the detector's noise floor) and
**polarity against the map**, with a **500 ms debounce measured on moving
time**.

## Context

Operator, 2026-08-28: *"I do not like the offsets. I want to clean up the data.
I want NAVI to detect the magnets and act on them and reject anything else.
Building a model that admits bad data then has a built in mechanism to deal
with the bad data is an unacceptable model."*

The 2026-08-27 field record shows the cost of the old model. Toby's position
advanced four markers in 1,005 ms while the IR wheel sensor recorded three
pulses — 29 mm of travel. Across the run, 254 markers of believed advance
against 47.0 m of wheel travel: roughly half a lap the locomotive never drove.
The sketch's own comments name the same failure as an 82-marker drift.

The recovery machinery did not fail to correct that. It *caused* it: offsets
relocated position, and "silent magnets" was the story told about the track in
between.

## What was deleted, and on what evidence

| Deleted | Evidence |
|---|---|
| `QUORUM_OFFSETS`, scoring, adoption, evidence ring | relocated position by vote; the 82-marker drift |
| NO_QUORUM, EVALUATING, self-resolution, suffix rescue | states that existed only to recover from the above |
| Conservation gate (`DT_CONSERVE_TOL`) | refused 42 events on 2026-08-27; 29 were real magnets on full intervals |
| Velocity model (`VEL_MODEL_*`) | PWM is a request, not a measurement (decision 0024) |
| Amplitude floor `NAVI_MAGNET_MIN_PEAK` (60) | real magnets run to 101 counts; MM012 medians 127 |
| Speed-scaled duration floor `ADMIT_DUR_*` | duration is direction-dependent by grade; 52/170 refused if tables swapped |
| `FORCED_OFFSET` MQTT fixture | displaced `navMm` ±8 markers with no magnet — an offset by another name |
| IR as a voting test | undercounts silently by up to 39% with no flag raised |
| "Silent magnets" | banned by the operator; it was the alibi for skipping untravelled track |

## The moratorium

Operator: *"There should be a moratorium on timing when the loco is stopped.
If PWM >35 there should be movement."*

Every navigator clock accrues only while `actualPwm > 35`. This closes the
operator's own edge case: stopping inside a magnet's footprint used to let a
wall-clock debounce expire during the dwell, admitting the rebound on restart.

The threshold is a presumption, not a measurement — the operator's caveat that
it varies by direction, geography, consist and locomotive is recorded in the
source. It is safe in one direction only: below 35 there is certainly no
movement; above it there may be none. Nothing depends on the unsafe half.

## Alternatives considered

- **Keep the offsets as a fallback.** Rejected by the operator: the fallback is
  what produced the drift.
- **Admit IR to the identity test.** Rejected: the test car is not in the normal
  consist, and IR's dropout tail is still silent. IR observes.
- **Keep a low amplitude floor at 60 counts.** Rejected: it is the MM140 rule
  with a smaller constant, and it sits where the navigator cannot see it.

## Consequences

- A single missed magnet leaves the navigator one marker behind. Polarity
  catches it on average within two markers, then stops the train. This is
  intended — damaged magnets should reveal themselves for repair — but it is a
  behaviour change from a system that used to paper over misses.
- Rebound refusals (~150/lap) are recorded and do **not** stop the train. Only
  an identity failure does.
- `nav_state` no longer emits `EVALUATING` or `NO_QUORUM`. Dashboard bindings
  that switch on those strings will never see them.
- `EVENT_FLOOR_MS` (40 ms) survives as the detector's electrical noise screen.
  It is the one rule not on the operator's list of three, and it is named at the
  admission site so it can be struck.

## References

- `firmware/test-programs/NAVI_2/NAVI_2.ino` (NAVI_2_0)
- `firmware/test-programs/NAVI_2/tests/harness_navi2.cpp` — 758 checks
- `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md`
- decisions 0021, 0022, 0024, 0043
- `docs/IR_DEV_REC/2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md`
