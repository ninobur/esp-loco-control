# Verdict — Toby certification run, 2026-08-02 / 08-03

**Source:** `field-records/logs/20260802_toby_lapA-sensor-flip-cert.log`
(2026-08-02T18:31:41 → 2026-08-03T20:54:25, 17 MB). Locomotive **Toby
(9950012)**, sketches SOLONAV_2_14 (first six boots) then SOLONAV_2_22.

Answers Sam's five questions on quantifying the shielded-cable payoff.

## The five numbers

Counted from Toby's own published events: `mm/marker` for detections,
`state/nav` for verdicts, `state/loopstat` for `floor_rejects` (cumulative
per boot, so increments are summed across the 49 boots in the file).

| | 2026-08-02 (before) | 2026-08-03 (after) |
|---|---|---|
| **1. Total markers detected** | 380 | 3,631 |
| **2. Disagreements** | 26 (AGREE 69) | 222 (AGREE 2,148) |
| — polarity agreement | **72.6%** (69/95) | **90.6%** (2148/2370) |
| **3. Phantom detections** | not measurable — see below | not measurable — see below |
| **4. Floor rejects** | 42 | 121 |
| — floor rejects per 100 markers | **11.1** | **3.3** |
| **5. Hall peak distribution** | p05 47, p25 83, **median 103**, p75 130, max 340, mean 109.5 | p05 138, p25 169, **median 187**, p75 202, max 975, mean 190.0 |
| — weak reads (peak < 80) | **21.6%** (82/380) | **1.4%** (51/3631) |

Supporting counts: LOST 2 → 24, REACQUIRED 0 → 21, BUFFERING 256 → 754,
`queue_drops` 0 on both days.

## Where the change happened

Splitting the marker stream into contiguous runs (>5 min gap) puts the step
change in the overnight window between **2026-08-02T20:43:46** and
**2026-08-03T15:30:54**. It is not gradual — every run before the gap is
weak, every run after is strong:

| run | markers | p05 | median | max | weak (<80) |
|---|---|---|---|---|---|
| 08-02 18:51:07 → 18:58:43 | 175 | 47 | 103 | 340 | 29.1% |
| 08-02 19:08:47 → 19:10:46 | 2 | 51 | 124 | 124 | 50.0% |
| 08-02 19:28:27 → 19:29:15 | 3 | 42 | 59 | 231 | 66.7% |
| 08-02 19:43:02 → 19:49:03 | 10 | 38 | 50 | 107 | 90.0% |
| 08-02 19:54:20 → 20:01:27 | 45 | 79 | 116 | 247 | 6.7% |
| 08-02 20:39:19 → 20:43:46 | 145 | 52 | 103 | 231 | 12.4% |
| — overnight gap — | | | | | |
| 08-03 15:30:54 → 15:31:22 | 5 | 178 | 627 | 975 | 0.0% |
| 08-03 16:46:27 → 16:52:01 | 298 | 126 | 152 | 400 | 0.3% |
| 08-03 17:18:34 → 17:23:18 | 234 | 127 | 149 | 399 | 0.0% |
| 08-03 17:46:27 → 17:53:15 | 359 | 149 | 182 | 399 | 0.0% |
| 08-03 19:54:40 → 20:54:02 | 2,735 | 153 | 192 | 526 | 1.8% |

**Operator confirmation needed:** the log records signal, not maintenance.
That the boundary coincides with the shielded-cable installation is
inference from the discontinuity, not a logged fact — David should confirm
the cable went in during that overnight window before this is cited as the
cable's payoff. The 08-02 sessions also ran SOLONAV_2_14 for the first six
boots, so the earliest 08-02 data carries a firmware difference as well as
a cable difference; the 20:39 run (2_22, weak) is the cleanest pre-change
comparison because it isolates the hardware.

## Caveats that matter for the comparison

- **Phantom detections are not directly measurable in this data.** The
  conservation timing gate that classifies a phantom (`PHANTOM_REJECTED`)
  is a QUORUM 1.x feature; this run predates it, and neither SOLONAV_2_14
  nor 2_22 publishes such an event. `floor_rejects` (sub-40 ms events the
  detector refuses electrically) is the nearest available proxy and is
  reported above; the 3.4× per-marker reduction is the strongest phantom
  signal the data contains. A post-QUORUM run will answer this question
  directly.
- **Sample sizes are unequal** (380 vs 3,631 markers). The rates are
  comparable; the absolute counts are not.
- **Disagreement counts include DISAGREE while LOST/BUFFERING.** The 08-03
  DISAGREE total (222) is inflated relative to its AGREE by long buffering
  stretches after induced LOST events during certification; the percentage
  is the fair comparison, not the raw count.
- The 08-03 peak maximum of 975 comes from the 15:30 five-marker run
  (median 627) — a stationary or very slow pass over a magnet, not
  representative of running signal.

## Verdict

Median Hall peak rose **103 → 187** (+82%), the weak-read fraction fell
**21.6% → 1.4%** (15× better), floor rejects per 100 markers fell
**11.1 → 3.3**, and polarity agreement rose **72.6% → 90.6%**. The
locomotive went from a navigator that could not hold position through a
lap to one that ran 2,735 markers in a single hour-long session at 1.8%
weak reads.

The improvement is in the **signal**, exactly where a shielding fix should
show up — not in the navigator's tolerance of bad signal. That is the
distinction worth preserving: the payoff was bought in hardware, and the
firmware's job got easier as a result rather than the firmware hiding the
problem.
