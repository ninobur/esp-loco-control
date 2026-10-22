# Independent Hall-ground ensemble and baseline measurement

This supersedes the October-19 RVM-adjacent width estimate for requirements work. It uses the earlier October-15 **ADC-only** ground/encounter partition, frozen before opening decisions, geographic sequence and RVM work were read. Its ground intervals are observed stable, low-variation Hall segments; encounters are the bounded complements.

## Inclusion and exclusion

Included **945** preserved opening records only when the opening has `matched_supported_activity`, `mm == target`, and the immediately preceding native encounter is also `supported_activity`. Excluded: uncertain instability, censored edge/gap activity, unsupported activity, unmatched openings, mismatched identity, and baseline fragments adjacent to a failed/edge encounter. The retained row-level reasons and bracket quality are in `measured_supported_matched.csv`.

## Measurements

| Quantity | n | median | 5th–95th | min–max |
|---|---:|---:|---:|---:|
| Ensemble width (s) | 945 | 0.240 | 0.200–0.620 | 0.150–1.670 |
| Ensemble width (native Hall samples) | 945 | 1201.000 | 1000.000–3100.000 | 746.000–8354.000 |
| Ensemble IR distance (mm) | 944 | 55.556 | 47.489–65.714 | 38.608–79.344 |
| Baseline before ensemble (s) | 945 | 1.050 | 0.850–3.058 | 0.720–225.850 |
| Baseline before ensemble (native Hall samples) | 945 | 5250.000 | 4250.000–15286.800 | 3600.000–1129250.000 |
| Baseline before ensemble IR distance (mm) | 945 | 245.999 | 226.327–277.484 | 211.779–328.168 |

## Direction

| Direction | n | median ensemble width s | 5th–95th s |
|---|---:|---:|---:|
| CW | 667 | 0.240 | 0.200–0.610 |
| CCW | 278 | 0.250 | 0.210–0.650 |

## What RVM 5.x actually bounds

RVM is **not** an independent physical-boundary measurement. `RvmShape.h` scores a whole window `[c−W/2, c+W/2]`, but publishes its `Ensemble.s/e` as only the middle half `[c−W/4, c+W/4]`. On acceptance it starts the next reference baseline at the **full window end**, not the published core end. Thus the accepted window can include baseline, its published core can truncate tails, and neither is a measured magnet width. This conclusion is from source inspection, not from using RVM to select the rows above.

## Limits

The IR coordinate is interpolated and its per-boundary bracket time is retained. For the preceding-baseline distance, the largest endpoint bracket has median 101.0 ms (5th–95th 99.1–108.8 ms). It is movement evidence, not a direct Hall boundary sensor. This earlier source is Otto-only; it does not establish Toby dimensions. It also does not claim that every supported interval is a physical magnet—only that it is a well-bounded, ADC-supported encounter associated with a matching recorded opening.
