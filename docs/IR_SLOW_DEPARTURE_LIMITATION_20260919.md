# Slow-departure limitation found in TX 1.5

The exact common detector was swept with 1 kHz synthetic ADC input, levels
1000/2000, and independently known full low/high cycles. Warmup exceeds one
second and five cycles. Duty fractions tested were 25%, 50%, and 75%.

| Period | Frequency | Expected completed | Observed completed |
| --- | --- | --- | --- |
| 20 ms | 50 Hz | 19 | 19 |
| 40 ms | 25 Hz | 19 | 19 |
| 100 ms | 10 Hz | 19 | 19 |
| 250 ms | 4 Hz | 19 | 19 |
| 500 ms | 2 Hz | 19 | 19 |
| 1000 ms | 1 Hz | 19 | 0 |
| 2000 ms | 0.5 Hz | 19 | 0 |
| 4000 ms | 0.25 Hz | 19 | 0 |

All three duty fractions produced these counts. Every measured sample at the
three slowest rates was non-TRACKING. Thus uncertainty detection catches this
synthetic failure, but slow-departure measurement itself is not achieved.
This is not field validation at the successful rates. The short rolling
envelope loses a plateau before the full pulse completes. The next detector
change must preserve phase across slow cycles without promoting baseline
drift or a lighting change to validated movement. Simply increasing window
length reintroduces the measured sunlight-tracking lag and is insufficient.

Reproduce with `tools/ir_detector_speed_sweep.cpp`, C++11, include path
`firmware/common`. Initial exploration used insufficient warmup at 50 Hz;
after correcting warmup, its apparent fast-rate deficits disappeared. The
table above is the corrected result.

TX 1.5 remains useful for the stationary noise test only. It is not accepted
for the complete speed range or NAVI navigation. The shared detector and
firmware have not been changed during this sweep.

## Actual route geometry

`firmware/test-programs/NAVI_SIMPLIFIED/RouteMap.h` contains 171 surveyed
CW spacings, documented min 280 mm and max 355 mm. `spanMm()` selects the
proper segment for CW or CCW. Prior generic same/next/later tests did not
exercise this map. With nominal 9.652 mm pitch, the minimum spacing is about
29 pulses. This suggests adequate nominal resolution but does not establish
adequate error bounds. Adjacent candidates can only be separated when their
distance windows, expanded for Hall sensing extent and measurement error,
are disjoint. Hall sensing extent and validated IR error bounds are still
missing, so no physical distinguishability claim is justified.
