# NAVI_EYES_WIDE_OPEN spatial Hall-reference field test

Date: 2026-09-27
Capture: `/home/david/NGR/navi_sync/navi_sync_20260927_214310.nsr`
Scope: research/evaluation only. No firmware, encounter recognition, Hall
thresholds, IR qualification, station logic or NAVI behavior was changed.

## Result

**SUPPORTED**, with one qualification: this capture supports the hypothesis
that a robust IR-spatial Hall median is a better Hall reference measurement
than elapsed-time or PWM-weighted sampling when a stationary Hall level can
occupy a large part of a completed interval. It does not select a production
bin width. The simplest spatial method is sufficient for this evidence; no
lobe classifier or recovery logic is warranted by this test.

The strongest demonstration is an unplanned but useful AUTO station interval,
`MM109 -> MM110`: 22,399 Hall samples accumulated at one zero-progress
location. The time and PWM-positive medians moved to about 2003 counts, while
the spatial medians remained 1824.5, 1824.5 and 1825 counts for 25, 50 and
100 mm bins. The IR-spatial representation prevented the stationary location
from dominating.

The deliberately observed MM056 stop is important but less dramatic. Its
stationary Hall level was already close to the surrounding moving level, so
all methods remained within two counts. Spatial weighting succeeded without
claiming a correction that the data did not require.

## A. Capture integrity and synchronization

The replay used the existing NSR1 decoder and the local ESP32 timestamps:

- 19,791 datagrams;
- 622,320 native Hall samples;
- 6,207 accepted IR snapshots;
- 619 status records;
- one session, locomotive 9950012;
- zero bad datagrams, sequence gaps or missing batches;
- capture SHA-256:
  `954edeb2d4585976bce42eafd66f2c2525689b9c4d6138b127931d7ca12a2847`.

Hall samples use the recorder's local `t_us`; accepted IR snapshots use the
same local receive clock. Cumulative completed IR pulses were converted with
the existing test-car calibration, **9.652 mm per pulse**. Pulse position for
an individual Hall sample was linearly interpolated between adjacent accepted
IR snapshots. This is an offline coordinate reconstruction from synchronized
observations, not firmware behavior.

The status stream recorded two Hall ring-drop units at startup. The counter
remained at two for the rest of the capture. IR ring drops, IR input-queue
drops and UDP failures remained zero. The Hall cadence was approximately 1 kHz:
median `dt_us` 1000, p95 1158, p99 1511, maximum 5172. The startup loss is a
capture caveat, not a mid-run spatial gap.

## B. Experimental phases identified

Times below are recorder-local seconds from the Toby boot/session, because the
operator's 21:45 time was approximate.

| Phase | Recorded interval | Evidence | IR distance |
|---|---:|---|---:|
| Setup/declaration | 0.604-50.122 s | stationary, 0 pulses; NAVI becomes MM040, CW at 50.122 s | 0 mm |
| Normal manual movement | 50.122-80.934 s | MM040 -> MM046, applied PWM about 47 | 164 pulses = 1582.9 mm |
| Ordinary-track stop | 81.933-113.941 s | zero-pulse plateau at MM046 | 0 mm |
| Slow manual movement | 113.941-203.773 s, excluding a second stop | MM046 -> MM055, lower applied PWM; 297 moving pulses | 2866.6 mm |
| Additional stationary interval | 141.339-166.568 s | zero-pulse plateau at MM051 | 0 mm |
| MM056 magnetic stop | 203.773-263.794 s | zero-pulse plateau; operator placed Hall sensor over MM056; NAVI accepted MM056 after restart at 265.186 s | 0 mm during stop |
| AUTO restart and circuit run | 263.794-596.648 s | AUTO context; MM056 -> MM040, including later station intervals | 4893 pulses = 47227.2 mm |
| Final stop | 596.648-622.746 s | zero-pulse plateau at MM040 | 0 mm |

The raw capture therefore contains one extra stop at MM051 and several later
AUTO station stops. They are not discarded. They make the capture more useful:
the planned ordinary and MM056 cases can be compared with the later AUTO
station case where time weighting demonstrably fails.

## C. Ordinary-track stop

The ordinary stop lasted **32.008 seconds** and accumulated **32,008 Hall
samples**. The IR counter remained at 173, so measured distance was **0 mm**.
The completed interval `MM046 -> MM047` nevertheless covered 30 IR pulses,
or 289.6 mm, outside the dwell.

For comparison, the offline surrounding-moving reference was 1826.5 counts.
The interval candidates were:

| Method | Reference | Difference from surrounding moving reference |
|---|---:|---:|
| time-weighted Hall median | 1830 | +3.5 |
| PWM > 0 Hall median | 1829 | +2.5 |
| IR-spatial, 25 mm bins | 1827 | +0.5 |
| IR-spatial, 50 mm bins | 1827 | +0.5 |
| IR-spatial, 100 mm bins | 1828 | +1.5 |

The ordinary stop produced a small but real time-weighting shift. Spatial
binning reduced it.

## D. MM056 magnetic stop

The zero-progress plateau lasted **60.021 seconds**, not approximately 20
seconds in the recorded clock, and contained **60,021 Hall samples**. The IR
counter remained at 470, so the measured distance during the dwell was **0
mm**. This is a valid no-change measurement under decision 0108, not an IR
failure.

The completed interval `MM055 -> MM056` lasted 71.225 seconds, covered 31
IR pulses (299.2 mm), and contained 71,224 Hall samples. Its offline
surrounding-moving reference was 1827 counts:

| Method | Reference | Difference from surrounding moving reference |
|---|---:|---:|
| time-weighted Hall median | 1825 | -2 |
| PWM > 0 Hall median | 1825 | -2 |
| IR-spatial, 25 mm bins | 1826.5 | -0.5 |
| IR-spatial, 50 mm bins | 1825.5 | -1.5 |
| IR-spatial, 100 mm bins | 1827 | 0 |

The MM056 magnetic dwell did not create a large Hall-level bias. That is a
result, not a failure of the experiment: this stop tests whether the method
can remain stable when a long stationary magnetic observation is present, and
it did.

## E. Normal, slow and AUTO moving results

The normal manual segment advanced 164 pulses (1582.9 mm) across six mapped
MM advances. The slow manual movement advanced 297 pulses (2866.6 mm) across
MM046 -> MM055, with the additional MM051 stop retained as a separate
interval.

After the MM056 stop, AUTO resumed and the locomotive advanced 4893 pulses
(47227.2 mm) until the final MM040 stop. The later AUTO intervals provide the
most informative adversarial evidence. In particular, `MM109 -> MM110`
contains a 22.399-second, 22,399-sample zero-progress plateau:

| Method | Reference |
|---|---:|
| time-weighted Hall median | 2003 |
| PWM > 0 Hall median | 2003 |
| IR-progress moving comparator | 1824 |
| IR-spatial, 25 mm bins | 1824.5 |
| IR-spatial, 50 mm bins | 1824.5 |
| IR-spatial, 100 mm bins | 1825 |

PWM filtering did not solve this case because the locomotive's ramp/control
context included positive PWM while the wheel remained at one measured
position. Physical distance, not PWM, separated movement from dwell.

## F. Time median versus PWM median versus spatial median

For every one of the 171 completed Hall-to-Hall intervals, the offline moving
comparator was the weighted median of Hall samples whose applied PWM was
positive and whose enclosing accepted IR snapshots advanced the cumulative
pulse count. It is a diagnostic comparator only; it uses surrounding evidence
that would not be available when the first reference is produced.

Absolute error against that comparator was:

| Candidate | Median error | p95 error | Worst error |
|---|---:|---:|---:|
| time-weighted | 0 | 0 | 179 |
| PWM > 0 | 0 | 0 | 179 |
| spatial 25 mm | 0 | 1 | 1 |
| spatial 50 mm | 0.5 | 1.5 | 6.5 |
| spatial 100 mm | 0.5 | 2 | 3 |

The 179-count failure is the MM109 station interval. The 6.5-count 50 mm
maximum occurs in the short initial MM040 -> MM041 interval with only two
occupied 50 mm bins. Restricting the bin comparison to intervals with at
least three occupied bins gives worst errors of 1 count (25 mm), 4 counts (50
mm), and 3 counts (100 mm). The bin width matters modestly, but none is
justified as a production constant by this single run.

## G. 25/50/100 mm sensitivity

All three widths behave essentially alike in well-covered moving intervals.
The 25 mm candidate is the least sensitive in this capture. The 50 mm and
100 mm candidates remain close, but their slightly larger worst deviations
show why the widths should remain diagnostic parameters until more captures
are available.

The physical principle is the important result: one robust representative per
traveled-distance bin, followed by a median across bins, prevents a stationary
location from receiving thousands of votes merely because time passed there.

## H. Worst observed spatial-reference error

Against the IR-progress comparator, the worst raw spatial error was **6.5 Hall
counts** for the 50 mm candidate. That occurred with inadequate coverage of
the initial short interval. With at least three occupied bins, the worst was
**4 counts** at 50 mm, **3 counts** at 100 mm, and **1 count** at 25 mm.

At the controlled MM056 interval, the worst spatial error was **1.5 counts**.
These are comparative reference errors, not certified absolute Hall accuracy.

## I. Following-magnet test

The reference produced by the completed `MM055 -> MM056` interval was tested
against the next real encounter, `MM056 -> MM057`. The following interval
contained 4712 Hall samples, 32 IR pulses (308.9 mm), and a Hall range of
1682..2037 counts, a 355-count encounter span.

| Reference used | Value | Following residual range |
|---|---:|---:|
| time-weighted | 1825 | -143..+212 |
| PWM > 0 | 1825 | -143..+212 |
| spatial 25 mm | 1826.5 | -144.5..+210.5 |
| spatial 50 mm | 1825.5 | -143.5..+211.5 |
| spatial 100 mm | 1827 | -145..+210 |

All five references preserve the same broad two-sided magnetic excursion and
the next NAVI advance to MM057. No encounter classifier or threshold was added
to reach that conclusion. The spatial references are therefore suitable for
the following encounter in this recorded case; they do not invent a landmark
or erase the real one.

## J. Direct answer

**SUPPORTED.**

This field capture supports the candidate measurement architecture:

```text
completed Hall-to-Hall interval
  -> accepted IR-measured physical distance
  -> one robust Hall representative per distance bin
  -> robust median across bins
  -> reference for the next magnetic encounter
```

The evidence is strongest because it contains both a planned MM056 magnetic
stop and a later AUTO stop with a large time-weighting failure. The MM056 stop
itself did not produce a large distortion, while the MM109 stop did; the same
simple spatial method handled both without special cases.

No more reference-measurement complexity is justified by this capture. Keep
the bin widths and the minimum-coverage question open for later evidence, but
do not add lobe classifiers, polarity rules, new thresholds, or recovery logic
to this measurement on the strength of this test.

## IR interpretation

Across the capture, 2,898 IR snapshots coincided with a positive cumulative
pulse advance. 2,897 of those were health-fault 0; one boundary snapshot at
the onset of the MM056 stop carried `INADEQUATE_CONTRAST` while the cumulative
count advanced by one pulse. The other 2,674 fault-labeled snapshots occurred
while the cumulative pulse count was unchanged, with one initial snapshot
having no predecessor for a delta comparison.

There is no sustained movement-period IR distance gap in this capture. The
long stationary `INADEQUATE_CONTRAST` runs are treated as the valid result
that no significant movement was measured, consistent with decision 0108.

## Reproduction

The compact result is committed at:

[`20260927_navi_eyes_wide_open_spatial_reference.json`](20260927_navi_eyes_wide_open_spatial_reference.json)

With the NSR1 capture copied locally, run:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/analyze_nsr_spatial_hall_reference.py \
  /private/tmp/navi_sync_20260927_214310.nsr \
  --source-label /home/david/NGR/navi_sync/navi_sync_20260927_214310.nsr \
  --output field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference.json
```

The research tool is [`tools/analyze_nsr_spatial_hall_reference.py`](../../tools/analyze_nsr_spatial_hall_reference.py).
It reuses [`tools/navi_sync_format.py`](../../tools/navi_sync_format.py),
preserves the native Hall and accepted IR observations, and emits no firmware
or operational configuration changes.
