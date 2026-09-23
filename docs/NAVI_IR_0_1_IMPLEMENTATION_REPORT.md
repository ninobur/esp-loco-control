# NAVI_IR 0.1 implementation record - 2026-09-20

## Request and status

David requested a Toby NAV sketch coordinating independent IR wheel movement
with mapped Hall landmarks, transparent decisions and an opportunity to recover
from ambiguity. He named it **NAVI_IR 0.1**. Implementation is at
`firmware/test-programs/NAVI_IR/NAVI_IR.ino`.

Build-completion status, before the operator's subsequent flash: **Built and
host tested; awaiting independent review and hardware validation. Default build
runs Manual control; AUTO is gated off.**

Deployment update, 2026-09-20: the operator flashed Toby; NAVI_IR_0_1 boot telemetry
is confirmed at 13:31:50 PDT. The first attempt found IR audible but unpaired,
so it is not joint-navigation validation. See the
[first-startup record](NAVI_IR_FIRST_STARTUP_20260920.md).
No train commands, Pi changes, TX/RX changes, or QUORUM promotion were performed.
The repository had substantial unrelated changes and untracked evidence; none
was reverted. No commit was made.

## Review before implementation

- Read the supplied ChatGPT Classic principles, repo instructions, recent NAV
  sketches and historical IR work. Evaluated NAVI_ONE, X22, NAVI_SIMPLIFIED and
  NAVI_HYPOTHESIS_SHADOW as experiments, not proven operating baselines.
- Preserved median-of-five Hall acquisition, independently useful even though
  the surrounding navigation trials failed. X22's flat-fringe baseline risk
  remains; its PWM-timed re-prime is not independent physical motion evidence.
- Found that the shadow navigator's one-fault budget lasted for the entire run.
  This build renews it only after a resolved ten-clean-observation word.
- Identified the danger of restoring cruise when ambiguity cancels station
  state. This build keeps the actual station state, compares all candidate
  actions, and stops on disagreement, overshoot or timeout.
- Noon repaired-car evidence supports nominal distance association at running
  speed. Earlier 19:52 faulty-wire data is not validation. The two repeatable
  laps and light-sector counts do not establish a hard per-interval error bound.
- Synthetic TX low-speed behavior and transport/clock uncertainty prevent an
  honest claim that all-speed autonomous navigation is already validated.

## Implementation and provenance

The new sketch's motor, MQTT, declaration, brake, INA219 and station scaffolding
originates in NAVI_SIMPLIFIED at reviewed tree base `ae4b7e6`. Its navigator was
replaced, not retained as hidden authority. New files are scoped to NAVI_IR;
older experimental sketches and QUORUM remain unchanged.

- `MovementEvidence.h`: paired type-5 receiver, CRC/schema/boot/sequence/counter
  checks, cumulative snapshots, continuity frames, clock alignment, diagnostics.
- `HallObserver.h`: X22 acquisition configured with no refractory time exclusion
  and no PWM-timed lost-lock recovery. Median-of-five ADC input remains.
- `HypothesisNavigator.h`, `Navigator.h`: joint Hall/IR alternatives with per-
  branch distance anchors, conditional identity, ten-observation recovery and
  fault-budget renewal. Exact distance ties cannot confirm a position.
- `RecoveryControl.h`: all-branch station action/state agreement, failed station
  approach withdrawal, no ambiguity-triggered cruise restoration.
- `NAVI_IR.ino`: existing-console manual motor control, direct ESP-NOW reception,
  persistent explicit MAC pairing, evidence telemetry and gated AUTO integration.
- `tests/`: sanitizer-backed host checks and reproducible noon association replay.
- `README.md`: wiring, pairing, operation, telemetry semantics and field gates.

The source MAC is intentionally not prefilled: the Pi RX archive does not prove
the sender MAC. `seen_mac` exposes a received candidate, but explicit operator-
verified pairing is required. The present repeater does not relay type-5 data.

## Operator principles carried forward

These are the task's requirements, not conclusions inferred from a successful
compile:

- The measuring wheel is unpowered. IR is independent of commanded PWM and
  motor rotation; it measures rolling travel between established MM landmarks.
- Braking through the locomotive motor, a derailment requiring navigation
  reset, and wheel diameter as a fixed calibration constant are treated as
  resolved premises. They are not reasons to reopen the sensor choice.
- Sensor development targets missed/doubled pulses and sunlight interference.
  Reception quality and optical measurement quality must be assessed separately.
- Hall supplies landmark identity and the absolute route reference. Once the
  MM pattern is established, the surveyed inter-magnet distances must agree
  with independent IR travel.
- Ambiguity must leave an opportunity for later observations to restore
  certainty. It must not silently advance the committed position or turn
  IR distance into an invented magnet identity.
- Motor feedback may later diagnose stalls, traction loss or abnormal operation;
  it is not a replacement distance channel.
- The decision process must expose the observations, alternative explanations,
  reason for commitment/refusal and resulting control action.

## Design choices and tradeoffs

The following are Codex's provisional implementation choices for 0.1, not
operator-approved numerical limits or a field verdict. The architectural record
is [decision 0089](decisions/0089-navi-ir-joint-evidence.md).

| Choice | Reason and alternative considered | Cost or limit |
|---|---|---|
| New Toby experiment, existing console integration | The user requested a NAV build from principles. Reuse motor/console interfaces while replacing the failed navigation authority, instead of modifying QUORUM or requiring a new Pi application. | Not production parity: no CTO or multi-train protection. |
| Direct car-to-loco ESP-NOW, cumulative type-5 counts | Exercise on-loco decisions before rewiring. Cumulative counts survive missing individual packets; ex-post alignment alone cannot make a live decision. | Intermediate radio freshness and clock alignment remain dependencies. Pi RX is a recorder, not a control relay. |
| Explicit persistent source-MAC pairing | Avoid treating whichever car happens to be audible as Toby's distance source. An observed MAC is not automatically trusted. | Requires a verified pairing step; no verified car MAC was available to prefill. |
| Separate movement evidence from transport | Keep the navigation contract usable with future local GPIO acquisition. | The direct-wired adapter is not yet implemented or tested. |
| Hall opening plus completed 400 ms window | Retain opening timing and an additional polarity observation without reinstating waveform-shape identity rules. | Judgement is deferred; close-spaced signals can be suppressed during acquisition, and station timing needs new tests. |
| Disable refractory time exclusion and PWM-timed baseline rescue | Test independent distance evidence without treating powered time as traveled distance. Keep the old 500 ms comparison visible as diagnostics. | Electrical rearm and baseline-fringe risks still exist; this is not a claim that every former acquisition defect is fixed. |
| Each interpretation owns its Hall/IR anchor | A false event must not advance the distance origin of the branch that ignored it. A skipped magnet spans two map intervals. | More state and telemetry than a single running MM counter. |
| Nominal nearest zero/one/two-interval comparison | Make short travel and double-span travel visible without inventing a calibrated percentage tolerance. Exact ties remain unresolved. | This is coarse association evidence, not proof that the sensor is physically over a magnet or that an event is impossible. |
| One-fault alternatives, up to 24 stored states | Represent ordinary progression, a false Hall event, one missed marker or a polarity error. Do not silently choose a distinct location just to retain a single answer. | Multiple overlapping faults are outside the model; capacity exhaustion reports LOST. |
| Ten additional unresolved observations, then withdrawal | Permit a bounded opportunity for recovery instead of stopping at the first discrepancy. | An observation count is not a time/travel fence if observations cease. |
| Renew allowance after a resolved ten-clean-observation word | Avoid the prior lifetime-one-fault budget; host tests confirm ten-bit word uniqueness in both directions on this route map. | Depends on this map and the conditional fault model, not an independent physical guarantee. |
| Compare station actions from the same actual state | Continue only if retained interpretations request identical station actions and resulting state. Refuse timeout/overshoot cruise restoration. | Can stop conservatively; the gated AUTO path is not yet hardware validated. |
| Default Manual evaluation; AUTO disabled | Build and replay results cannot validate live acquisition, stopping distance or departures. | This version is not yet an accepted automatic railway-running build. |
| No inferred pulse repair or map-forced recalibration | Preserve independent measurement and expose discrepancies rather than forcing IR to fit suspect Hall positions. | Isolated-error detection/repair remains future sensor work, subject to evidence. |
| One repo source, NGR-Files symlink | Preserve the familiar IDE opening path without divergent editable copies. | The sketch still requires the repo's shared headers and credentials. |

Equivalent branch states at the same MM and last-real Hall timestamp are
coalesced: the implementation retains the lower fault count and conservative
distance-consistency flag. That is not a choice between distinct MM locations.
False/missed alternatives are created when polarity, vote disagreement or IR
provides a reason for doubt; not every arbitrary error history is enumerated.

Operational confirmation and physical validation are deliberately different:
`distance_confirmed` is nominal association consistency, `CONDITIONAL_SEQUENCE`
is certainty within the stated model, and `UNVALIDATED` means there is no
certified physical distance-error bound. The operator's initial declaration
establishes the starting frame; it does not provide an exact on-magnet IR
anchor. The first observed Hall event provides that anchor.

## Development and evidence trail

- [September 19 session record](IR_SESSION_RECORD_2026-09-19.md):
  earlier bench work, receiver recording and run history.
- [TX 1.5 build record](IR_TX_1_5_BUILD_RECORD_20260919.md):
  movement packet/detector provenance.
- [Repaired-car mixed-light record](IR_MIXED_LIGHT_LAP_20260920.md) and
  [noon run analysis](IR_NOON_RUN_ANALYSIS_20260920.md):
  the relevant repaired-hardware evidence, including approximate operator
  lighting annotations. Do not substitute the rejected 19:52 faulty-wire run.
- [Slow-departure limitation](IR_SLOW_DEPARTURE_LIMITATION_20260919.md):
  why good running-speed evidence does not establish crawl/stop reliability.
- [Repeater check](IR_REPEATER_CHECK_20260920.md):
  why an active REP2 is not an IR relay.
- [Sketch README](../firmware/test-programs/NAVI_IR/README.md):
  pairing, operation, telemetry, test commands and acceptance gates.
- [Firmware catalog](../firmware/README.md):
  classification as a built experiment, not field-accepted production control.

The noon audit distinguishes the very repeatable running counts from radio
loss and from absolute accuracy: 5,379/5,381 pulses on the two full matched laps,
sunny block counts 2,642/2,644/2,645, shaded block counts 2,160/2,160.
The overall -0.439% nominal/map difference is not a measured missed-pulse rate
and is not used to add pulses or change the fixed pitch. Hall MM labels remain
provisional comparison evidence.

## Intended hard-wired follow-on

This is a future integration direction, not a completed capability:

1. Validate the ESP-NOW/manual prototype with simultaneous Hall, IR and ruling
   logs. Establish startup, crawl, stopped-wheel, departure, sunlight and error
   behavior before granting automatic running authority.
2. Move optical acquisition onto the locomotive ESP32 using the shared detector
   and movement contract. Preserve cumulative counts, diagnostic counters and
   explicit discontinuities. Motor rotation must not substitute for wheel input.
3. Replace the radio history adapter with local-clock movement snapshots; keep
   the Hall/IR identity split, per-interpretation anchors and refusal semantics.
   Radio delivery and cross-device timing then cease to be navigation inputs.
4. Revalidate scheduling, ADC acquisition, GPIO/electrical integration and stop/
   departure behavior. Direct wiring does not itself repair optical pulse errors
   or establish a physical uncertainty bound.

The production input's exact pin/electrical arrangement and its acceptance
criteria still require review; no wiring change was performed in this task.

## Tests and conclusions

Native C++17 compiled with `-Wall -Wextra -Werror`, AddressSanitizer and
UndefinedBehaviorSanitizer: **35 checks passed**. Tests include both travel
directions and wraparound, ten-bit route-word uniqueness, same-polarity false
signals and omissions, repeated separated faults, stale optical/transport
evidence, clock extremes, retired boot exhaustion, ties, reversal, no-IR recovery
limit and station timeout/action disagreement. No sanitizer findings.

Historical noon association replay: **513 observations; 438 tracking; 75
ambiguous; 30 reestablishments; zero LOST; zero committed MM differences** against
the old recorded positions. Peak branch count one. Uses actual recorded Hall
polarity, the prior audit's paired counts and 150 ms alignment filter, no new
tolerance fitted to that run. Important restrictions:

- The old MM is comparison evidence, not independent truth.
- This replay does not exercise new Hall acquisition, opening/window votes,
  capture-to-arrival clocks, live ESP-NOW reception or physical motor controls.
- The one-branch clean run is not a false-signal benchmark; synthetic tests
  demonstrate recovery mechanics, not field error rates.

Default Arduino build: ESP32 Dev Module, core 3.3.11; **1,007,847 bytes flash,
75,020 bytes global RAM**. No sketch warnings in the final default compile;
installed Adafruit INA219 library emits existing enum-conversion warnings.
AUTO-enabled compile also passed: **1,008,327 bytes flash, 75,020 bytes global
RAM**. This is code-path coverage only, not authorization or evidence of safe
AUTO operation. The source default remains AUTO disabled.

## Deliberate limits and next actions

No hard IR distance rejection, pulse repair, IR-invented marker identity, or
PWM-derived distance was added. Nominal distance selects plausible associations
and vetoes commitment; it is explicitly marked UNVALIDATED for hard bounds.
The one-fault model can be wrong outside its stated scope. A matching phantom
at a plausible distance is not proven distinguishable by this experiment.

Manual control is usable through the existing console; station/grade code is
included but AUTO defaults disabled. CTO, peer wire broadcasts and multi-train
protection are absent, inherited from the experimental scaffold rather than
removed from production. There is no claim of QUORUM parity. Existing motor
and battery priorities are retained, not comprehensively hardware-retested.

Before AUTO: independent review; bench pairing and E-stop; manual paired run;
crawl/stop/departure checks; station timing tests for the 400 ms deferred
judgement; no-observation/IR-unavailability safety policy; uncertainty budget
and a held-out false-Hall comparison against timing. Preserve logs and evaluate
sun/shade separately when there are explicit operator labels.

No need to reflash the car or Pi receiver for this integration. David's Mac
should use 115200 upload speed; 921600 is not required. Serial baud is 115200.

For the established operator workflow, `/Users/davidbrown/NGR/NGR-Files/NAVI_IR`
is a directory symlink to the repo's sketch. There is no second editable copy;
opening the sketch from either location uses the same source and dependencies.
Compilation through this shortcut also passed with the same default flash/RAM
sizes, verifying that its relative header dependencies resolve correctly.
