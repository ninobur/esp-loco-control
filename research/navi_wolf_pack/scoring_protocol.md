# Frozen scoring protocol

This protocol is frozen before a final comparison. It prevents a detector's
own event declarations or map labels from becoming its proof of correctness.

## Preconditions for a definitive score

A recording is eligible only when it has all of the following:

1. native Hall observations in chronological order, with timestamp and any
   recorded IR, PWM, direction, and movement fields;
2. a supplied initial position/direction and an immutable route-map version;
3. independent encounter/location truth, such as synchronized operator/track
   annotations derived without the candidate's declarations; and
4. a session-level manifest assignment made before the candidate is evaluated.

No eligible recording currently exists in the frozen manifest. The included
historical labels are comparison hypotheses, not independent geographic truth.

## Measures once eligibility exists

- Recognition: TP, FP, FN, duplicates, and causal declaration delay.
- Geographic continuity: correct/incorrect identities, first-loss timestamp,
  duration and distance while wrong, recovery, and explicit uncertainty.
- Operational resilience: per-session stop/restart, dwell, slow-run, drift,
  and stall slices. `uncertain` and confidently wrong are separate outcomes.
- Simplicity: discrete mechanisms, adjustable parameters, special-case rules,
  per-sample runtime, bounded online memory, and calibration artifacts.

Results are reported per recording and locomotive. A catastrophic wrong,
confident location is listed individually; it cannot be hidden in an average.
No aggregate weighted winner is produced.
