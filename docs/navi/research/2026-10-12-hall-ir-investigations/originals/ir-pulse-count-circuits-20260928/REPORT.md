# September 28 Otto IR pulse count per Lowline circuit

## Result

The continuous recording contains **three consecutive, geographically identified CW circuits** from the MM000 opening region back to MM000. Using the exact cumulative `completed_pulses` values in the Type-5 IR snapshots that September 28 NAVI associated with each Hall event, the circuit counts are **5,342, 5,385, and 5,351 pulses**.

- Mean: **5,359.33 pulses/circuit**
- Median: **5,351 pulses**
- Range: **43 pulses** (0.80% of the mean)
- Sample standard deviation: **22.68 pulses** (0.42%)
- Three-circuit aggregate: **16,078 pulses**

The aggregate is the strongest estimate because the starting and ending Type-5 snapshots have similar age relative to their Hall openings. It gives **5,359.33 recorded counter increments per circuit**.

If the endpoint rule is narrowed to Hall events that NAVI itself marked `ADVANCED` + `CONFIRMED`, only events 315 and 665 qualify. They enclose **two circuits and 10,727 pulses**, or **5,363.5 pulses/circuit**, but the rejected event 488 prevents an online-confirmed split into the two individual circuits. This stricter result differs from the three-circuit geographic estimate by only 4.17 pulses/circuit (0.078%).

## Circuit table

| Circuit | MM000 boundary events | Direction | Start counter | End counter | Completed-pulse difference | Run conditions | Boundary qualification |
|---:|---|---|---:|---:|---:|---|---|
| 1 | 315 → 488 | CW | 10,625 | 15,967 | **5,342** | Scheduled stops at Patio, Grillers, Arches, Bamboo; no reversal or e-stop | Event 315 confirmed. Event 488 was geographically MM000 but NAVI rejected it as `NON_LANDMARK_HALL`; see below. |
| 2 | 488 → 665 | CW | 15,967 | 21,352 | **5,385** | Same four scheduled stops; no reversal or e-stop | Event 488 reconstructed MM000; event 665 confirmed MM000. |
| 3 | 665 → 836 | CW | 21,352 | 26,703 | **5,351** | Same four scheduled stops; no reversal or e-stop | Event 665 confirmed. Event 836 was accepted at MM000 with one coherent position but had a Hall polarity discrepancy. |

All four endpoint records use the same IR transmitter boot ID, `6EBF1A480BB71806`. The cumulative pulse counter never regressed.

### Why event 488 is geographically usable

Event 488 cannot be described as a NAVI-confirmed MM000 landmark. NAVI measured 28 pulses from MM170, converted that to 270.256 mm with its configured pitch, and rejected the opening because it fell just outside the then-current distance gate. The geographic evidence is nevertheless strong:

1. Event 487 was a confirmed MM170 opening.
2. Event 488 was the next Hall opening, had north polarity matching MM000, and retained a single coherent navigation hypothesis.
3. NAVI then advanced to MM000 without Hall when the distance exceeded its high window.
4. Event 489 was accepted as MM001; its distance from MM170 was consistent with the two mapped intervals combined.
5. The stable MM sequence continues after event 489.

Thus event 488 is used as a **reconstructed MM000 opening boundary**, while preserving its online `NON_LANDMARK_HALL` classification.

## Snapshot age and exact-opening uncertainty

The exact cumulative count at the 1 kHz Hall opening was not recorded. The IR transmitter reported cumulative counts about every 100 ms, so a Hall opening can only be bracketed between two Type-5 counts. NAVI's selected snapshot was 95–137 ms older than the Hall opening:

| Hall event | Geographic boundary | NAVI-selected count | Logged snapshot age | Count immediately before opening | Count immediately after opening | Offline time bracket |
|---:|---|---:|---:|---:|---:|---|
| 315 | MM000 | 10,625 | 121.471 ms | 10,628 at −19.947 ms | 10,631 at +80.053 ms | 10,628–10,631 |
| 488 | MM000, rejected online | 15,967 | 95.469 ms | 15,967 at −93.944 ms | 15,969 at +6.056 ms | 15,967–15,969 |
| 665 | MM000 | 21,352 | 117.476 ms | 21,355 at −15.951 ms | 21,357 at +84.049 ms | 21,355–21,357 |
| 836 | MM000, polarity discrepancy | 26,703 | 136.502 ms | 26,705 at −34.950 ms | 26,707 at +65.050 ms | 26,705–26,707 |

These brackets imply possible exact opening-to-opening counts of:

- Circuit 1: **5,336–5,341**
- Circuit 2: **5,386–5,390**
- Circuit 3: **5,348–5,352**
- Three-circuit aggregate: **16,074–16,079**, or **5,358.0–5,359.67 per circuit**

The ranges are conservative: they allow every pulse reported between the two surrounding snapshots to fall on either side of the Hall opening. The data do not contain individual IR edge timestamps, so choosing a single exact count at each opening would be interpolation, not a measured integer result. The Type-5 snapshot differences in the main table are exact recorded counter differences and reproduce what the September 28 NAVI data made available online.

The first and second circuits remain measurably different despite endpoint quantization: circuit 1 can be no more than 5,341 pulses and circuit 2 can be no fewer than 5,386 pulses, a separation of at least 45 pulses. That variation is therefore not caused solely by the 100 ms synchronization phase.

## Comparison with the 52,150 mm map

The route-spacing array used in the recorder build contains 171 intervals and sums to **52,150 mm**. Treating that tape-measured map as an estimated physical reference, rather than as calibration truth, gives:

| Circuit | Recorded pulses | Map-implied mm/pulse | Distance represented at configured 9.652 mm/pulse | Difference from 52,150 mm |
|---:|---:|---:|---:|---:|
| 1 | 5,342 | 9.7623 | 51,560.984 mm | −589.016 mm (−1.129%) |
| 2 | 5,385 | 9.6843 | 51,976.020 mm | −173.980 mm (−0.334%) |
| 3 | 5,351 | 9.7458 | 51,647.852 mm | −502.148 mm (−0.963%) |
| Three-circuit aggregate | 16,078 | **9.7307** | 155,184.856 mm | −1,265.144 mm over three circuits (−0.809%) |

Using the exact-opening aggregate bracket instead gives **9.7301–9.7331 map-mm per pulse**. This is a comparison to an estimated tape-measured circumference. It does not demonstrate the true wheel travel per pulse: wheel slip, wheel effective diameter, the geographic location of the Hall opening, and map survey error are not independently measured here.

At the configured 9.652 mm/pulse, the 52,150 mm map would predict 5,403.03 pulses. The observed snapshot differences are 18–61 pulses lower. This comparison is deliberately performed only after counting raw integer counter increments; no calibrated distance was used to produce the pulse totals.

## Stops and IR continuity

Each of the three circuits contains the same four programmed station-stop sequences: Patio, Grillers, Arches, and Bamboo. Each includes a five-second `DWELL_BEGIN` to `DEPART` interval, with a longer zero-ramp/stopping sequence around it. There were no direction commands, reversals, or nonzero e-stop states within the three circuit spans.

| Circuit | Missing Type-5 sequence positions | Longest capture-report gap | Pulse counter regressions | Transmitter `sample_gaps` increment | Inferred added/removed pulses | Saturated samples |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 36 | 202 ms | 0 | 0 | 0 / 0 | 0 |
| 2 | 34 | 400 ms | 0 | 0 | 0 / 0 | 0 |
| 3 | 32 | 300 ms | 0 | 0 | 0 / 0 | 0 |

The receiver missed some periodic Type-5 reports, but the cumulative counter continued monotonically and later reports recovered its value. Health/readiness faults occurred during the station stops when the optical signal was stale; the endpoints themselves were healthy and ready. These diagnostics show continuity of the reported counter. They do not independently prove that the optical detector counted every physical wheel mark.

## Exclusions

- **Capture start through event 315:** excluded. The recording begins without a geographic position, contains an early CCW segment followed by a reversal to CW, and the first MM000-region passage around event 156 occurs during sequence recovery with seven corrections and missing/forced advances. It does not provide a reliable MM000 start boundary.
- **After event 836:** excluded. Otto reaches only about MM062 CW, reverses at event 900, and the capture ends during the CCW return near MM011. There is no later MM000 endpoint, so no complete circuit exists.
- **No CCW circuit:** the CCW portions at the beginning and end are both incomplete and cannot be paired into MM000-to-MM000 circuits.

## Conclusion

Otto recorded **about 5,359 completed IR pulses per full 171-magnet Lowline circuit** in this September 28 run. The most defensible figure is the three-circuit aggregate, **16,078 recorded counter increments**, because it minimizes endpoint report-phase bias. Individual circuit snapshot differences span **5,342–5,385 pulses**, and synchronized count brackets prove that at least part of this variation is real rather than 100 ms reporting quantization.

The run is suitable for estimating circuit pulse count and repeatability. It is not, by itself, a physical calibration of millimeters per pulse because the 52,150 mm map is an estimated tape measurement and the recording has no independent distance standard or individual IR edge timestamps.
