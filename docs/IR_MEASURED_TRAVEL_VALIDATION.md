# Independent measured-travel validation

`tools/ir_measured_travel.py CAPTURE TRIALS.json` evaluates type-5 snapshots
against independent travel truth. It never treats Hall counts as independent
wheel truth. Each trial supplies boot id and exact start/end snapshot sequence
numbers; choose endpoints during recorded stationary dwells bracketing motion.
Do not substitute approximate USB receipt timestamps for physical event times.

Example schema (numbers below are synthetic, not field evidence):

```json
[
  {
    "label": "synthetic-ten-turn-example",
    "boot_id": 123,
    "start_sequence": 10,
    "end_sequence": 200,
    "truth_min_mm": 964.2,
    "truth_max_mm": 966.2,
    "truth_pulses": 100,
    "truth_method": "Example only: ten independently counted full turns"
  }
]
```

Use `truth_pulses` only when exact pulse truth is justified, for example counted
complete turns with the same wheel phase at each endpoint. Partial measured
travel has endpoint phase uncertainty; omit exact pulse truth in that case.
For a stationary trial truth distance and pulses are zero. Document lighting,
power, orientation, speed, and departure/stop conditions in the trial label or
additional manifest fields. Independently measured travel bounds must include
measurement uncertainty. Do not derive truth distance from the tested count.

Output includes completed-count delta, nominal distance error interval, net
count error when available, optical reason distribution, cumulative failure
evidence, and missing snapshot reports. Missing reports do not themselves mean
missing optical pulses. Net count error cannot identify compensating missed and
doubled pulses; waveform/independent per-turn observations are needed for that.
This evaluator does not automatically fit or validate a production error budget.

Required physical matrix remains: stationary shade/sun; USB/battery stationary
comparison; crawl/slow/medium/fast known travel; slow departures; stops; changing
light while stopped; moving shade/sun transitions; repeated track distances.
Field distance validity remains false until this evidence establishes bounds.

Five host regression tests passed: missing reports with exact count, hidden
unreliability, reset endpoint rejection, stationary false count, and counter
regression. Run `python3 -m unittest -v test_ir_measured_travel` from `tools`.

Latest Pi-tail inspection during this step still showed raw packets from boot
0x536abd10. It did not establish that TX 1.5 had been flashed or tested. No
physical acceptance claim is made.

Existing route comparison documentation is
`docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md`, section 7, which uses
`spanMm(previousAcceptedMm, navDir, acceptedSteps)`. Future candidate tests must
use actual route spans and sensing extent, not assumed equal marker spacing.
