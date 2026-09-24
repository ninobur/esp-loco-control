# IR TX R2: Short-Roll Stop Waveform Investigation

2026-09-23. Follow-up to `IR_TX_R2_SHORT_ROLL_20260923.md`.
No firmware changes, resets, motor commands or new physical movement requested.
Toby remained operator-confirmed off. Read existing Pi recorder only.

## Source and Integrity

Pi `ngr-ir-espnow-record.service` active. Selected current boot's type-1 raw
packets (SID e9f8b7c4) and type-5 movement packets (boot 8dc31ef4e9f8b7c4)
from the existing 20260923 daily log. Archived as
`field-records/logs/20260923_ir_r2_bench_radio.log.gz`.
5,415 raw batches, first sample indices 0 through 525984; 62 missing ranges.
Radio omissions are not evidence of sensor sampling gaps. In particular,
the stopping region 124000-128000 is contiguous. Parsing used the existing
`tools/ir_scope_espnow_analyze.py` transport CRC check; type-5 also checked
its internal CRC. No serial reader attached on the Pi.

## Recorded Observations

- Before the requested roll, at TX capture time 105.089177 seconds: count 53,
  SIGNAL_STALE, span 31, openAborts 0. Prior handling had acquired retention.
- Roll onset: 117.696177 seconds, REACQUIRING, openAborts 1; by 117.896177,
  TRACKING, count 54, span 1071. This shows an additional restart continuity
  loss to inspect, not a claim of same-epoch retention through restart.
- At 126.404177 seconds: count 99, REACQUIRING, span 303, openAborts 2.
- At 126.604177: INADEQUATE_CONTRAST, span 271, openAborts 2.
- At 126.804177: INADEQUATE_CONTRAST, span 95, openAborts 3.
- Sampling-gap counter stayed at its startup value 1; saturation zero.
  This is real detector continuity loss transmitted by radio, not stale output.

Raw batch envelopes near the stop (sample index, not boot milliseconds):

| First sample | Raw min/max | Live low/high | Abort count at batch end |
|---|---|---|---|
| 125664 | 1056 / 1178 | 816 / 1855 | 1 |
| 125760 | 878 / 1163 | 816 / 1231 | 1 |
| 125856 | 835 / 1008 | 832 / 1231 | 2 |
| 125952 | 909 / 941 | 944 / 1231 | 2 |
| 126240 | 900 / 929 | 912 / 1119 | 2 |
| 126336 | 827 / 923 | 896 / 1071 | 3 |
| 126432 | 695 / 927 | 896 / 943 | 3 |

Batch envelope is sampled at batch start, counters at batch end. Do not equate
both with the same ADC conversion or use receiver arrival as ADC sample time.

Available raw samples 130000-170000 (39,904 observations): minimum 655,
5th percentile 877, median 892, 95th percentile 905, maximum 1102. Thus a small
percentile span does NOT mean no large individual ADC excursions. Whole-window
peak-to-peak is 447; no stationary noise certification follows from span 31.
During preceding motion (119000-124000), p05/median/p95 were 897/1475/2058.
The resting central level is near the recent dark level, not evidently halfway
between the prior bright/dark levels. Physical spoke alignment was not observed.

Later, the median changed from about 887 around sample 300000 to about 1290
after 390000. Cause is unknown (lighting, handling, alignment or another
physical change); do not attribute it to sunlight or motion without operator
confirmation. The packet evidence is preserved for follow-up.

## Code Trace and Limited Replay

Replayed received ADC samples through the actual R2 C++ Detector with state
inspection in a temporary host harness (no production code edit). It used
idealized 1 ms sample spacing and treated missing radio ranges as sample gaps;
therefore it is not a timing-exact reproduction of the uninterrupted ESP32.
In samples 119000-129999, 81/11000 stored detector flag words differed from
replay. Absolute counts and abort counts cannot be compared after radio gaps.

Nonetheless the local stop trace reproduces the relevant code path:
at sample 125914, raw 940, live envelope 944/1231 (span 287), learned reference
864/1231. TRACKING becomes INADEQUATE_CONTRAST, abort increases and proven
reference changes true -> false. At sample 126364, span falls below 120 and
remaining armed state is invalidated. The transmitted envelopes and fault
counters support this explanation, but the internal reference is replay-derived.

R2's live-contrast quality-loss branch (`high-low < 300`) deliberately records
continuity loss AND clears proven/candidate retention. As contrast later
collapses below 120, there is no retained reference left. Thus an ordinary
deceleration can plausibly prevent stationary zero even when resting near the
earlier dark plateau. Do not label this stop merely a mid-edge stop based on
the five-second serial summaries.

## Disposition

First rolling signal observed; this stop is NOT a retained-zero pass.
Hardware provides evidence missing from ideal square-wave tests: changing
envelopes during deceleration and individual ADC outliers. No algorithm change
made from a single run. Preserve continuity-loss reporting; do not restore
false-ready/zero just to improve the display.

Next bench steps: controlled stops centered on dark and bright wheel phases,
with waveform capture, then restart and optical perturbation checks. Consider
the separation of a remembered optical reference from measurement continuity
in the next reviewed design discussion, if repeated physical evidence warrants
R3. This is a question for review, not an implemented change or field approval.
