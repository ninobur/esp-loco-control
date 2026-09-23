# NAVI_ONE_SIMPLE_HALL

Experimental magnet-only navigator for Otto. It is **not field accepted** and
has not been flashed. It uses the existing motor, battery, MQTT, route, and
station operations from X18. IR remains an observer and has no navigation vote.

## Detector

- Prime the Hall baseline from 2,000 readings while clear of magnets.
- Open one magnet event after five consecutive 1 kHz readings at least 70
  counts from the current baseline. Send it immediately to the navigator.
- Keep that event open through a stop on the magnet. Close after 30 consecutive
  readings below the threshold.
- 80 ms after that closure is **confirmed**, collect 200 readings. Accept their
  median only if the prior magnet start gap was at most 3 seconds, actual PWM
  stayed above 30 throughout, and raw spread is at most 32 counts. Otherwise
  retain the previous baseline. A new magnet interrupts collection.
- Navigation checks the polarity of the one expected next magnet. A mismatch
  withdraws position and stops AUTO. No waveform morphology, lap adjustment,
  station timing gate, or post-stop rescue participates.

The 80 ms timer starts after the 30 ms below-threshold confirmation. The
recorded waveform replay exposed that distinction: starting the timer at the
last above-threshold reading put many windows into the magnetic tail.

Each Hall decision is published on `diag/hall_decision`: OPEN, CLOSE,
BASELINE_ACCEPT, or BASELINE_REJECT. It reports the baseline, candidate,
spread, PWM, prior marker gap, and rejection reason. Navigation messages take
queue priority over this diagnostic. `mm/marker` continues to carry the map
ruling. An overflow of the navigation event queue withdraws position and stops
AUTO.

## Verification

```sh
cd firmware/programs/NAVI_ONE/variants/NAVI_ONE_SIMPLE
c++ -std=c++17 -Wall -Wextra -Werror -I . tests/test_simple_hall.cpp -o /tmp/test_simple_hall
/tmp/test_simple_hall
c++ -std=c++17 -Wall -Wextra -Werror -I . tests/test_simple_navigator.cpp -o /tmp/test_simple_navigator
/tmp/test_simple_navigator
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

The local September 16 snapshot replay (`/tmp/xhr_ccw_stall_samples.csv`,
truncated at the end) produced 1,550 openings and closures, 1,513 accepted
baselines, one low-PWM rejection, one spread rejection, and 35 skipped
collections after long marker gaps. This checks detector behavior on that
recording; it does not certify navigation or a new station stopping position.

**Field risk:** marker identity is now ruled at sustained threshold entry,
where X18 ruled after passage closure. That advances the station machine
earlier in an approach. The landing must be observed before AUTO operation is
trusted. Morning thermal drift and long post-stop baseline staleness also
need field evidence; no speculative recovery code was added.
