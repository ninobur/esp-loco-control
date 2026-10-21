# NGR magnet ensemble width study

## Evidence coverage

Direct native Hall: Otto CW and CCW, Toby CW, all 2026-10-19. No raw 2026-10-20/21 Hall stream was recoverable locally; it is not represented by a width estimate.

Boundary convention: three successive samples departing a preceding observed 200 ms near-horizontal line by at least 12 ADC (or four robust-noise units); end = first following observed 200 ms near-horizontal line. This is a measurement convention, not an RVM rule. Right-censored/ambiguous traces remain in the CSV.

## Distribution

| Locomotive | ensembles | median width s | 5–95% s | min–max s | median baseline s |
|---|---:|---:|---:|---:|---:|
| Otto | 704 | 0.180 | 0.100–0.240 | 0.100–0.660 | 0.960 |
| Toby | 160 | 0.220 | 0.160–0.360 | 0.080–0.500 | 1.100 |

| Locomotive | polarity | n | median width s | 5–95% s |
|---|---|---:|---:|---:|
| Otto | N | 343 | 0.180 | 0.100–0.240 |
| Otto | S | 361 | 0.180 | 0.100–0.260 |
| Toby | N | 75 | 0.240 | 0.160–0.346 |
| Toby | S | 85 | 0.220 | 0.160–0.376 |

## RVM-window comparison

Negative `window_minus_ensemble_seconds` means the accepted RVM window is shorter than the independently measured ensemble. The comparison is overlap-matched only; it does not declare either boundary physically true.

| Locomotive | matched | median window − ensemble s | 5–95% s |
|---|---:|---:|---:|
| Otto | 704 | 0.020 | -0.060–0.060 |
| Toby | 28 | -0.010 | -0.199–0.112 |

IR distance is unavailable in the portable October-19 Hall arrays, so this study reports no IR widths. Polarity is retained per ensemble in `ensembles.csv`; speed/stop state is not available consistently enough in these arrays to support a valid stratified result.

The long/merged and right-censored cases are evidence of ambiguous baseline return, not discarded failures. These measurements describe this corpus only and do not establish a firmware requirement.
