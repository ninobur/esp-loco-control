# Repaired IR car: September 20 noon run

## Scope and conclusion

Only the September 20 midday run is analyzed. The September 19 19:52 run is
excluded: the operator reports its sensor wire was producing garbage signals.
Today's configuration follows the GPIO connection repair, wheel insert, and
bench verification of ten counts per revolution documented in
`IR_MIXED_LIGHT_LAP_20260920.md`. No firmware or calibration was changed here.

The repaired sensor produced stable, repeatable counts in both lighting zones.
Sunlight did not cause detected contrast collapse, saturation, or a measurable
loss of repeatability in this run. Radio delivery was substantially worse in
the sunny route section, but cumulative onboard counts remained usable.
The data supports further IR development; it does not yet certify a pulse-error
rate, distance bound, or navigation authority.

## Timeline and reference

All times PDT, September 20. Toby declared MM40 CW at 12:11:27.609, reported
powered at 12:11:33.626, and powered off at 12:23:12.402 at MM40. There are
513 consecutive accepted MAGNET advances, first MM41 at approximately
12:11:50.261 and last MM40 at 12:23:00.667, with no rejected marker in this
window. This sequence represents three circuits from the declared start;
the first-to-last measured interval is three circuits minus MM40-to-MM41.
This is telemetry's route account, not independently verified physical laps.

The saved operator notes are latest RX timestamps checked after messages,
not exact physical transition times. Mapped to Toby's accepted sequence:

| Note | Pi time | Toby bracket |
| --- | --- | --- |
| Start message | 12:12:06.582 | MM53-54 |
| Leaving shade for mostly sun | 12:16:01.696 | MM66-67 |
| Very sunny | 12:16:50.755 | MM105-106 |
| Shade starts | 12:17:54.109 | MM159-160 |
| Mostly sun starts again | 12:19:27.622 | MM65-66 |
| Complete lap reported | 12:19:37.599 | MM72-73 |
| Final stop reported | 12:23:22.052 | after final MM40 |

The two sun-entry notes agree to roughly one marker. For comparisons across
passes, infer sunny core MM70-155 and shaded core MM164 through MM61 CW,
excluding boundary margins. These are approximate operator-labelled route
zones, not independent light measurements. Sensor-to-locomotive separation,
message latency, and small patches of shade remain uncertainties.
Importantly, the entire first start-message-to-sun-message interval spans
more than a lap and MUST NOT be classified as all shade.

## Sun versus shade

Packet loss is missing unique sequence numbers between received endpoints,
summed separately over visits. Boundary packets without observed endpoints
are not counted as known missing. Radio reception is distinct from sensing.

| Metric, inferred route cores | Sunny | Shaded |
| --- | ---: | ---: |
| Raw waveform packet loss | 34.94% | 17.60% |
| Movement snapshot loss | 30.07% | 13.89% |
| Raw median RSSI | -91 dBm | -86 dBm |
| Median detector ADC span | 1,247 | 1,263 |
| Received ADC samples at 0/4095 | 0 / 202,560 | 0 / 240,480 |
| Received movement snapshots TRACKING | 2,174 / 2,174 | 2,510 / 2,510 |

The directly annotated very-sunny interval, 12:16:50.755-12:17:54.109,
also retained median span 1,247, zero saturation or unreliable-sample increase,
and all 491 received movement snapshots TRACKING. Its raw/snapshot losses
were 28.75% / 21.69%. The immediately following shaded interval had losses
19.22% / 18.93% and median span 1,263.

Thus worse sunny-section reception is an RF/location association, not evidence
that sunlight disrupted the optical sensor. Distance, obstructions, antenna
orientation and route geometry were not controlled separately.

## IR versus Toby MM distance

Nominal pitch is the transmitted 9.652 mm/pulse. The surveyed route table sums
to 52,150 mm. Accepted Hall identities are a provisional comparison reference;
IR does not validate them or assign new identities.

| Comparison | IR pulses | Nominal IR | Mapped distance | Difference |
| --- | ---: | ---: | ---: | ---: |
| All matched MM41 to final MM40 | 16,107 | 155.465 m | 156.150 m | -0.439% |
| MM41 lap 1, 12:11:50-12:15:29 | 5,379 | 51.918 m | 52.150 m | -0.445% |
| MM41 lap 2, 12:15:29-12:18:59 | 5,381 | 51.937 m | 52.150 m | -0.408% |

The two full-lap counts differ by just two pulses (0.037%). The final part of
the third circuit is included in the full comparison, not presented as a
third MM41-to-MM41 lap because the run stopped before that final MM41.

Repeatable route blocks provide a more useful check than single 300 mm gaps:

| Block | Pass counts | Nominal/map difference |
| --- | --- | --- |
| Sunny MM70 to MM155, 25.555 m | 2,642 / 2,644 / 2,645 | -0.213% / -0.137% / -0.100% |
| Shaded MM164 through MM61, 21.010 m | 2,160 / 2,160 | -0.770% both |
| MM0 to MM40, 12.375 m | 1,264 / 1,266 / 1,262 | -1.413% / -1.257% / -1.569% |

The residual is route-dependent and repeatable, not concentrated in sunlight.
Calibration, surveyed spacing, Hall event timing, train geometry, and genuine
count errors cannot be separated by these logs alone. In particular, the
0.439% difference is NOT a measured missed-pulse rate. Do not automatically
add pulses or recalibrate the wheel to force agreement with the route table.

## Sensor and waveform audit

Between first and final matched MM events:

- 5,151 received movement snapshots, all TRACKING.
- No increase in cumulative unreliable samples, saturation, sample gaps,
  or open-pulse aborts. Raw missed-sample, queue-drop and send-error counters
  also did not increase.
- Raw reception: 5,068 of 6,984 sequence positions (27.43% absent).
  Movement reception: 5,151 of 6,700 (23.12% absent).
- 10,630 rise-to-rise intervals were measurable without crossing a missing
  raw sample. Median 38 ms; fifth/95th percentiles 33/56 ms; range 28-243 ms.
  No interval was below 20 ms. Deliberate final slowing is retained.
- A conservative local screen evaluated 7,420 periods with four contiguous
  neighboring periods mutually within 30%. None was below 0.6 or above 1.6
  times its neighboring lower median. No pulse was added or removed.
- No recorder transport or payload CRC errors were found in decoded IR
  packets in the retrieved September 20 file.

This screen looks for isolated anomalies, not all possible errors. It cannot
detect sustained proportional miscounting, errors inside missing packets, or
every adjacent double/miss combination. Zero candidates is not zero proven
pulse errors. Cumulative counters survive absent snapshots, but absent raw
waveforms cannot be reconstructed from those counters.

## Alignment method and limits

Type-5 snapshots are filtered to one boot whose low 32 bits are e4410597.
Captured ESP microseconds are mapped to Pi time using the fifth percentile of
receipt-minus-capture offsets. Their fifth-to-95th offset spread is 117 ms;
this is a latency diagnostic, not a formal timestamp-error bound. Hall
wave-close uptime is mapped using nearby (within 30 s) Toby status timestamps.
No IR-derived route identities and no unrecorded interpolation are used.

The full interval's nearest snapshot offsets are +24 and -17 ms. Bracketing
snapshots give 16,107-16,110 counts conditional on that clock mapping; that
bracket is not a certified physical-distance confidence interval. The named
lap and sunny/shaded block endpoints are within 50 ms of a received snapshot.

438 of 512 adjacent marker intervals meet the 150 ms endpoint-nearness gate.
Their median nominal/map ratio is 0.9974, fifth/95th percentiles 0.8861/1.0939.
Single-marker intervals are too sensitive to count quantization, surveyed
spacing and timestamp/trigger differences to diagnose one missed pulse here.

## Next engineering action

Preserve today's repaired physical setup. Improve RX placement/antenna path
and/or retain compact per-pulse intervals on the car for later retrieval;
the present raw radio loss limits exact error diagnosis. Validate the small
scale residual against independently measured travel before changing pitch.
Continue phase/interval plausibility as confidence evidence, not automatic
correction or permission to invent a Hall identity. No navigation integration
or firmware change was made by this analysis.

## Reproduction and verification

Sources are archived as `field-records/logs/20260920_ir_noon_capture.log` and
`field-records/logs/20260920_toby_noon_capture.log` (snapshots retrieved around
12:31, containing the completed noon run). Structured results are in
`field-records/analysis/20260920_ir_noon_analysis.json`.

From the repository root:

```sh
python3 tools/ir_noon_analysis.py field-records/logs/20260920_ir_noon_capture.log field-records/logs/20260920_toby_noon_capture.log firmware/test-programs/NAVI_SIMPLIFIED/RouteMap.h
python3 -m unittest discover -s tools -p test_ir_noon_analysis.py
```

The analysis intentionally asserts the known 171-marker/52.150 m route and
513 consecutive noon advances. Reuse for another run requires explicit scope
changes, not silently applying these lighting annotations to a different day.
