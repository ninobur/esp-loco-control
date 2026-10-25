# Otto 70x2 magnet-to-magnet timing analysis — 2026-10-23

Research only. No threshold, navigation, or firmware behaviour was changed.

## Finding

The available raw Otto record supports a highly repeatable **within-session,
manual PWM-90** 70x2-onset timing profile in both directions. It does not
contain normal automatic PWM-60/90 multi-lap operation with raw Hall samples,
so it cannot establish automatic-operation distributions or cross-day Otto
validation.

| 2026-09-09 QUORUM survey | CW | CCW |
|---|---:|---:|
| reconstructed complete admitted labelled 70x2 openings | 1,071 | 2,063 |
| clean adjacent PWM-90 onset intervals | 1,049 | 2,048 |
| mapped positions represented | 171 | 171 |
| observations per position | 6–7 | 11–12 |
| median relative-time SD | 0.00556 | 0.00594 |
| sequential held-out uniform MAE | 71.23 ms | 86.81 ms |
| sequential held-out geographic MAE | 5.85 ms | 5.51 ms |

## Measurement and sources

Sources are `field-records/logs/20260909_survey/otto_1_13X_cw_waveforms.log.gz`
and `otto_1_13X_ccw_waveforms.log.gz`. The survey README identifies them as
manual PWM-90 QUORUM 1.13X operation. Every `mm/wave` record retains a
base64-encoded baseline-relative waveform, accepted map label, opening polarity,
opening PWM, quality flags, and firmware monotonic `t` at an earlier 38-count
event opening.

This study does not use `t` as its onset. It decodes the waveform and selects
the first two consecutive raw samples at least 70 counts in the recorded opening
polarity. With the declared 12 pre-opening samples, onset is reconstructed as
`t + (70x2_index - 12)` ms. The Hall task target is 1 ms, but scheduler jitter
between samples is not retained. This is raw waveform reconstruction, not RTB,
peak, return, navigation decision, or MQTT receipt timing.

## Qualification and limitations

Primary intervals require admitted (`rej:0`), labelled, non-truncated,
non-clipped waveforms with a polarity-consistent 70x2 opening; adjacent mapped
labels in the recorded direction; both opening PWM values in 85–95; and an
onset interval of 500–3,000 ms. Wave exclusions were CW: 398 not admitted, 11
truncated, one no-70x2; CCW: 528 not admitted, 17 truncated, one no-70x2. Pair
exclusions were CW: 19 non-adjacent and two non-PWM-90; CCW: nine non-adjacent
and five non-PWM-90.

These filters deliberately remove starts, stops, bad associations and
sub-threshold activity, so this is calmer than all operation. Map identity is
QUORUM's accepted label: repeated adjacent progression supports sequencing but
is not independent physical proof of each label.

## Profile and prospective validation

Each interval uses its surveyed map span and actual onset-to-onset pace. A
circuit reference pace is the median clean pace; relative time is interval pace
divided by that reference. The analysis script writes every retained interval
with both MM labels, onset timestamps, polarities, PWM, distance, pace, circuit,
and relative time, followed by per-MM/direction profile rows with SD, range,
and a descriptive median-plus-three-SD value.

The prospective test holds out each later circuit, trains only on earlier
complete circuits, and estimates current pace from only the preceding eight
eligible current-circuit intervals. Geographic prediction removes/restores the
historical MM factor; uniform prediction uses recent raw pace. No held-out
target or later timing enters either method.

The small same-session samples do not justify a normal distribution assumption.
The descriptive three-SD values were exceeded by 28/669 eligible CW predictions
(4.19%) and 48/1,626 CCW predictions (2.95%), not the ideal normal one-sided
three-sigma rate. They are not stop limits.

## Relation to other evidence

Claude's Otto calibration used RTB-era interval records; those timestamps are
not renamed or pooled here. This is a distinct 70x2 waveform-onset result.
Toby's automatic profile used Pi/NAVI timing in a different locomotive and
programme. The studies jointly support only the narrower proposition that
movement timing can be geographically structured; their profiles are not
interchangeable.

## Reproduction and needed recording

Run:

```sh
python3 tools/analyze_otto_70x2_timing_20261023.py --out /private/tmp/otto-70x2.json
```

The command writes JSON and CSV while leaving evidence unchanged. The next
needed evidence is multiple Otto automatic circuits on separate days with
native-rate Hall samples, baseline/opening metadata, PWM history, and an
independent movement/identity witness.
