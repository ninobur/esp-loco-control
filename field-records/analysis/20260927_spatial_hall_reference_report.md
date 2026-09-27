# Spatial Hall-reference investigation

Date: 2026-09-27  
Scope: research/evaluation only. No firmware, NAVI, encounter, IR, station,
threshold, dashboard, or decision-record changes were made.

## Result

The simplest promising concept is:

**completed Hall-to-Hall interval -> IR-distance bins -> one robust Hall
representative per bin -> median of the representatives -> reference for the
next encounter.**

A 50 mm bin is a useful diagnostic example, not a settled firmware constant.
The important property is equal treatment of traveled distance, not the exact
bin width. This avoids making elapsed time, sample count, or PWM stand in for
physical progress.

The evidence supports this as the preferred conceptual measurement, but does
not yet establish its field accuracy in the failure case that motivated it:
a long stationary dwell or no-progress segment inside a Hall interval. The
only genuinely synchronized record available for this study has approximately
1 Hz Hall status telemetry and contains no verified zero-progress dwell inside
an interval.

## Data and provenance

The complete Hall-only sources were kept separate from the synchronized field
record:

| source | session | contents | SHA-256 |
|---|---|---|---|
| `/home/david/NGR/hall_records/xhr_20260916_191904.xhr` | `C3B93D0B` | Otto XHR1, continuous 1 kHz Hall capture | `349b7690b930b1847c7846d7fd729fbdc83054a0ba356f6011ad449d7709f34d` |
| `/home/david/NGR/qt_logs/qt_20260824_184813.qtcap` | `D7651658` | Toby QTRACE01, native Hall samples and decisions | `0c77333963c3d2dd82c14068c14172cb97d4f0aeeb051d3c7270aadd2ed4b99c` |

The synchronized Toby record is:

- `field-records/logs/20260920_ir_noon_capture.log`
  (`68b68e3d9676dee230ca13cac3811234e3c153f9137adfd1d840015e80c58f63`)
- `field-records/logs/20260920_toby_noon_capture.log`
  (`8d02b8ee3d859b46508846fb46999a8239ea01ec2dab5c519c5530403ae2a024`)
- `field-records/analysis/20260920_ir_noon_analysis.json`
  (`3e6225345c4479e1c3c6fe2d4d573ad1f0206df88e40be6cc4ebbb97d95de1a4`)

The IR parser reports one valid packet stream, 11,148 packets, cumulative
pulses 213 to 16,428, and a p95 packet-time gap of about 0.23 s. The paired
analysis uses the existing wheel calibration of 9.652 mm per completed pulse.
There is no IR distance stream embedded in either complete Hall-only source.

## Methods compared

The diagnostic script compares these representations without changing any
operational code:

1. ordinary time-weighted median of the matched Hall status samples;
2. PWM-positive-only median;
3. spatial bin medians at 25, 50, and 100 mm, followed by a median across bins;
4. the previous interval's result, retained only as a comparison, never as a
   replacement for a new measurement.

The offline clean-window comparisons in the raw Otto trace are diagnostic
yardsticks only. They are not treated as spatial truth because that capture
has no synchronized IR.

## Observed performance

### Synchronized Toby field record

- 438 IR-defined Hall-to-Hall intervals were available.
- 1,269 of 1,272 Hall status records matched an IR packet within 0.75 s;
  median match separation was 34 ms, p95 176 ms.
- Only 111 intervals had at least two matched Hall status samples. The other
  327 had one sample, so no spatial estimator can demonstrate within-interval
  robustness there.
- In the 111 multi-sample intervals, the 25 mm and 50 mm spatial candidates
  exactly matched the ordinary median for every interval. The 100 mm candidate
  differed by at most 1 Hall count.
- The PWM-positive-only median was identical to the ordinary median in all 438
  intervals because this paired run was continuously moving. The previous
  interval's 50 mm result differed from the new result by median 1 count, p95
  3, maximum 9; that is continuity context, not a substitute for measuring
  the completed interval.
- Every matched interval in this record had moving status; none contained a
  verified stationary dwell, and none showed PWM-positive status with an
  unchanged IR pulse count between adjacent observations.
- The IR travel agreed with the mapped interval at a median ratio of 0.997;
  the observed range was 0.740 to 1.190. That is a check that the paired IR
  stream is carrying physical distance, not a score of Hall-reference error.

This is a clean continuous-motion result. It shows that spatial binning is
simple and does not disturb the ordinary moving case. It does **not** show
that it removes dwell bias.

### Otto complete XHR session

The target session contains 3,537,200 samples over 3,537.2 s, 1,538
`ADVANCED` rulings, and no missing, duplicate, or reordered sample datagrams.
Using deliberately wide native sample windows before and after each ruling as
an offline diagnostic, the post-minus-pre ordinary-level error had median
absolute value 1 count and p95 3 counts. The worst observed difference was
209 counts, in a trace containing the documented level change/stop effects.
That worst case is precisely why time/sample windows cannot be promoted to a
spatial measurement.

The raw session also contains the relevant adversarial material: long
stationary passages, both polarities, weak and strong accepted magnets, and
the approximately 33-count level change documented in the existing baseline
reports. It cannot tell us how much physical distance those structures occupy
because IR was not recorded synchronously.

### Toby complete QT session

The target session contains 2,790,270 Hall samples and 1,200 accepted events,
with 601 North and 599 South accepted polarities. It includes the difficult
passage already packaged at `t=998001`, where a South structure is followed by
an opposite-polarity opening after only 16 ms. It also includes weak accepted
South events, including a peak of 39 counts. These are useful stress passages
for a later spatially synchronized capture, but QT itself contains no IR
distance and cannot score a spatial reference.

## Adversarial-case assessment

| case | result from available evidence |
|---|---|
| continuous motion | spatial and ordinary medians agree in the paired record |
| slow approach | the paired record includes the slower MM28-40 section; agreement remains, but telemetry is sparse |
| long stationary dwell | present in Otto Hall data, but no synchronized IR; unresolved |
| PWM nonzero without progress | not present in the paired record; reason not to use PWM as spatial evidence remains valid |
| weak/strong magnets | present in both raw Hall sources; no spatial score without IR |
| both polarities | present; no evidence requiring polarity-specific reference logic |
| short physical intervals | not adequately tested; most paired intervals have only 1-2 Hall status samples |
| difficult passages | present in Toby QT, including the 16 ms polarity conflict; no synchronized IR |
| capture gaps | Otto target session has none; paired IR has 3 unmatched Hall status records; gaps were preserved, not interpolated |
| Otto ~33-count level change | confirmed by existing raw analysis; spatial response unresolved without paired IR |
| legitimate ordinary-level change across an interval | unresolved; a single median would represent the traveled interval's dominant distance, not explain the cause |

## Answers to the requested questions

1. **Does spatial representation remove dwell failures?** It is the only
   tested candidate with the right physical weighting, but this dataset does
   not contain a synchronized dwell case capable of proving removal.
2. **Simplest association:** associate each native Hall sample with the
   cumulative IR distance, group into fixed traveled-distance bins, take one
   robust representative per bin, then take the median.
3. **Required resolution:** unresolved. The 1 Hz paired Hall stream is too
   sparse to distinguish 25 from 50 mm in a meaningful way. Native-rate Hall
   plus IR is needed.
4. **One representative per increment:** sufficient in the continuous-motion
   evidence, subject to adequate bin coverage and a minimum spatial span.
5. **Lobe/core occupancy:** the raw captures show structures that are usually
   brief relative to a normal interval, but synchronized IR is required to
   answer this in distance units. No lobe classification is needed for the
   reference measurement unless a structure occupies a substantial fraction
   of traveled distance.
6. **At least 50% of traveled distance:** not observed or measurable in the
   synchronized material available here.
7. **Changing ordinary level:** a median remains robust to a minority of
   contaminated distance, but a genuine level transition across most of the
   interval becomes a real ambiguity rather than a lobe-classification
   problem.
8. **10/20/30-count adjacent differences:** the raw Otto source demonstrates
   that level changes of this scale exist; without synchronized IR we cannot
   score whether a spatial interval measurement carries the change correctly.
9. **Worst observed errors:** the paired spatial-versus-time difference was
   0 counts at 25/50 mm and at most 1 count at 100 mm. That is not the same as
   absolute reference accuracy; the no-IR raw-window worst case was 209 counts
   and is explicitly not a spatial result.
10. **What breaks the simple method:** insufficient spatial coverage, a
    no-progress/dwell segment occupying most of the interval, or a genuine
    ordinary-level transition across the interval. The first two require a
    native-rate synchronized field capture to quantify.

## Recommendation and unresolved work

Recommend carrying forward the conceptual **IR-distance-binned robust median**
as the simplest reference measurement to evaluate next. Do not add lobe,
polarity, PWM, elapsed-time, or waveform-classification logic to this
measurement on the present evidence.

The next evidence needed is one native-rate synchronized Hall+IR capture that
deliberately includes: a moving interval, a controlled stop with the wheel
stationary, restart, a slow approach, and both polarities. The extraction
must preserve the raw Hall samples and cumulative IR distance. Until that
exists, the method is promising for continuous motion but not field-cleared
for the dwell case that motivated the investigation.

## Reproduction

From the repository root, with the two complete Pi captures available locally:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/evaluate_spatial_hall_reference.py \
  --xhr /Users/davidbrown/NGR/RTB_EVIDENCE_20260926/xhr_20260916_191904.xhr \
  --xhr-session C3B93D0B \
  --qt /Users/davidbrown/NGR/RTB_EVIDENCE_20260926/qt_20260824_184813.qtcap \
  --qt-session D7651658 \
  --paired-ir field-records/logs/20260920_ir_noon_capture.log \
  --paired-hall field-records/logs/20260920_toby_noon_capture.log \
  --paired-analysis field-records/analysis/20260920_ir_noon_analysis.json \
  --output field-records/analysis/20260927_spatial_hall_reference.json
```

The result is `field-records/analysis/20260927_spatial_hall_reference.json`.
The script reuses `tools/xhr_format.py`,
`tools/ir_scope_espnow_analyze.py`, and the existing QTRACE adapter in the RTB
evidence package. It preserves missing/unmatched records and does not
interpolate distance or assign any sample an RTB label.
