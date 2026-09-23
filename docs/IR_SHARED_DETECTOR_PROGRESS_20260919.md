# Shared detector candidate progress

Files in the established repository:
- `firmware/common/IrMovementDetector.h`
- `tools/ir_movement_replay.cpp`
- `tools/test_ir_movement.cpp`

Compile host tests and replay with C++11 or later and
`-I firmware/common`. The replay accepts boot-id, sample-index, raw-ADC triples
on stdin. One nominal sample slot is 1 ms. It separates boots and reports
completed pulses, observed rises, sample gaps, and TRACKING sample count.

The candidate uses a 512-sample histogram, exact 5th/95th rank definitions,
50 ms envelope refresh, 128-sample prime, existing 120-count detection floor,
and 300-count tracking floor. It counts on completion, not rise. Timeout,
contrast loss, saturation and sample gap require low-crossing rearm. No
corrections or silence-derived STOPPED state exist. TRACKING is an optical
diagnostic and is expressly not validated distance accuracy.

Host tests passed with `-Wall -Wextra -Werror`: stationary noise, periodic
completed cycles, plateau silence, saturation and a sample gap.

Fresh failed stationary capture, last 60 seconds of boot 0x536abd10:
- Old installed detector: 2072 pulses.
- Candidate: 58656 received samples, zero rises, zero completed pulses,
  zero TRACKING samples, 15 raw-stream gaps.

Moving replay results (local time September 19):
- 17:34:17-17:43:12: 301728 samples, 3284 completed, 3562 rises,
  753 gaps, 137573 TRACKING samples.
- 17:35:53-17:36:21: 21312 samples, 239 completed, 260 rises,
  43 gaps, 10092 TRACKING samples.
- 17:35:42-17:35:54: 8832 samples, 89 completed, 102 rises,
  22 gaps, 4603 TRACKING samples.

These counts do not establish physical missed-pulse rates. Radio gaps cause
conservative detector reset and remove optical history; they are not evidence
of actual sampling gaps on the ESP32. Live firmware has those samples, so
these replay totals are not directly comparable with cumulative live counts.
Future evaluation must score continuous spans and label gap-affected intervals.

The header is NOT yet integrated into the transmitting sketch or flashed.
Next: coherent diagnostic snapshot/distance contract with unbounded ranges
when error budgets are unvalidated; firmware integration using the same
detector; continuous-span replay; independently measured travel validation.
Do not infer acceptance from the stationary improvement alone.
