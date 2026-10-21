# Independent Hall-ground ensemble and baseline measurement

This supersedes the October-19 RVM-adjacent width estimate for requirements work. It uses the earlier October-15 **ADC-only** ground/encounter partition, frozen before opening decisions, geographic sequence and RVM work were read. Its ground intervals are observed stable, low-variation Hall segments; encounters are the bounded complements.

## Inclusion and exclusion

Included **1081** preserved opening records only when the opening has `matched_supported_activity` and `mm == target`. Excluded: uncertain instability, censored edge/gap activity, unsupported activity, unmatched openings, and mismatched identity. The retained row-level reasons and bracket quality are in `measured_supported_matched.csv`.

## Measurements

| Quantity | n | median | 5th–95th | min–max |
|---|---:|---:|---:|---:|
| Ensemble width (s) | 1081 | 0.250 | 0.200–0.660 | 0.150–1.670 |
| Ensemble width (native Hall samples) | 1081 | 1250.000 | 999.000–3300.000 | 746.000–8354.000 |
| Ensemble IR distance (mm) | 1080 | 55.178 | 46.842–64.338 | 38.608–76.996 |
| Baseline before ensemble (s) | 1081 | 1.000 | 0.070–4.300 | 0.050–211.900 |
| Baseline before ensemble (native Hall samples) | 1081 | 1000.000 | 70.000–6730.000 | 50.000–211906.000 |

## Direction

| Direction | n | median ensemble width s | 5th–95th s |
|---|---:|---:|---:|
| CW | 731 | 0.240 | 0.190–0.650 |
| CCW | 350 | 0.260 | 0.210–0.670 |

## What RVM 5.x actually bounds

RVM is **not** an independent physical-boundary measurement. `RvmShape.h` scores a whole window `[c−W/2, c+W/2]`, but publishes its `Ensemble.s/e` as only the middle half `[c−W/4, c+W/4]`. On acceptance it starts the next reference baseline at the **full window end**, not the published core end. Thus the accepted window can include baseline, its published core can truncate tails, and neither is a measured magnet width. This conclusion is from source inspection, not from using RVM to select the rows above.

## Limits

The IR coordinate is interpolated and its per-boundary bracket time is retained. It is valid movement evidence, not a direct Hall boundary sensor. This earlier source is Otto-only; it does not establish Toby dimensions. It also does not claim that every supported interval is a physical magnet—only that it is a well-bounded, ADC-supported encounter associated with a matching recorded opening.
