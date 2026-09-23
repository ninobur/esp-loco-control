# NAVI movement contract implementation status

`firmware/common/IrMovementContract.h` implements boot-scoped timestamped
snapshots, observed rises, completed pulses, separately exposed inferred-added
and inferred-removed counts (currently zero), calibrated nominal cumulative
distance, optical reason, and cumulative unreliable-sample/health counters.

`between()` reports a nominal interval distance plus minimum/maximum and
explicit issue flags. By default the range is [0, infinity), UNVALIDATED.
Only a separately measured error budget matching calibration and interval
limits can yield finite bounds. The implementation supplies no production
budget. Caller must ensure the tested speed/lighting/operating conditions apply;
the budget is not automatically validated by optical TRACKING.

For an independently validated budget, bounds include extra/missed counts,
scale uncertainty, and at least one pulse pitch of endpoint phase uncertainty.
Boot mismatch, time/counter reversal, invalid endpoints, intervening unreliable
samples, health changes, or inferred corrections invalidate bounded comparison.
A recovered healthy endpoint does not conceal a failure between snapshots.

`couldReach()` accepts route-supplied travel windows and rejects only disjoint
bounded ranges. Unknown ranges leave every route candidate possible. Hall
identity, route direction, Hall sensing extent, and route geometry remain the
caller's responsibility. This helper is not yet wired into running NAVI.

Host tests: compile `tools/test_ir_movement_contract.cpp` with
`c++ -std=c++11 -Wall -Wextra -Werror -I firmware/common`.
Tests passed for unknown bounds, synthetic same/next/later windows, reset,
backwards time, stale signal, and saturation hidden between healthy endpoints.
Synthetic geometry and budgets do not prove real adjacent-marker discrimination.

No firmware flash or new live measurement was performed in this step. Next
work remains integration into TX snapshots, protocol transport/receiver tests,
continuous-span replay, and independently measured travel validation across
speed, departures/stops, and sunlight transitions. Detector TRACKING is not
equivalent to validated movement. This goal remains incomplete.
