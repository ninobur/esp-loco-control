# Evaluation of the proposed >185%-of-previous-interval missed-pulse rule

**Date:** 2026-10-11
**Status:** Read-only analysis. No code was changed and the threshold has not been implemented.
**Reference:** NAVI_EWO_0_3 IR validity correction `253faa4`.

---

## 1. Answer in brief

1. **The rule does not remove the flags.** On the October 9 "healthy" control it raises **226** candidates. The previous detector raised 136 timing questions and 17 missing-compatible findings on the same data.
2. **The premise behind the assignment is not supported by the data.** The flags in the healthy control have three properties:
   - they are intervals at almost exactly **2× or 3× the local pulse period**;
   - they recur **every 8–9 counted pulses**, which is once per revolution of the ten-spoke IR wheel;
   - every mile-marker interval that contains one reads about **one pulse (~3%) short of the mapped distance**.

   That is the expected signature of spokes passing without being counted. The evidence indicates the previous detector was mostly *correct*, and that the October 9 window is healthy in transmission but not in counting.
3. **As a comparator, the immediately preceding interval is workable but fragile.** Its weaknesses:
   - a short or bouncy preceding interval inflates the ratio;
   - there is only a 7.5% margin below the nominal 2.0 ratio;
   - the preceding event must have been received.

   A median-of-five comparator at 150% detected the synthetic case more reliably (§6). That is characterization, not proof.
4. **The acceptance criterion in revised requirement §4 needs your review.** "The October 9 healthy control must not be incorrectly classified as corrupted" would oblige a detector to *ignore* what appear to be genuine missed pulses.

None of this is ground truth. No optical or video record confirms which spokes were counted. The conclusions above rest on timing structure and on map-distance correlation (§5).

---

## 2. Data used

| Source | Content | Events |
|---|---|---|
| `0_3 tests/fixtures/oct09_healthy` (19:31–19:41) | boot, completed count, completed µs, last 1-s PWM | 9,484 |
| `oct09_run1_onset`, `oct09_run2_onset` | Same columns | 275 / 333 |
| `oct10_121800` | Same columns | 12,848 |
| `oct10_124555` | Empty fixture | 0 |
| `field-records/logs/20261009_MM045_EVENING/all_20261009.log.gz` (`bf95cc4`) | Full raw evening log: Type-6 `ir_pulse` EVENT records with transmitter `interval_us`, plus `loopstat` PWM and `nav/evidence` TARGET_CONFIRMED | 19,296 unique events, 18:56–19:58 |

**Data checks:**
- Transmitter `interval_us` equals the difference in `completed_us` for all 15,874 consecutive pairs (0 mismatches).
- Timestamps are 1-ms quantized.
- The log is an MQTT subsample. Serial steps are 1 for 82% of events and 2 for 15%, and longer gaps also occur. Intervals were evaluated only where three consecutive serials were all logged.

**Not analysed:** the 2026-09-10 bench pulse traces.

---

## 3. Reproduction of the previous detector

`replay_timing.cpp` with `NaviIrEvidence.h` from `253faa4`, compiled and run on the fixtures:

| Fixture | Timing questions (>150% median5) | Missing-compatible brackets |
|---|---|---|
| oct09_healthy | 136 | 17 |
| oct09_run1_onset | 1 | 0 |
| oct09_run2_onset | 22 | 6 |
| oct10_121800 | 46 | 7 |

These match `verification/replay-results.json` exactly.

The previous detector reports fewer events because it resets on every reported PWM change and every serial gap, and needs five intervals to re-baseline. Its lower count reflects lost coverage, not better specificity.

---

## 4. Results for the >185%-of-previous-interval rule

| Data | Interval pairs | Candidates | With nonzero, unchanged PWM | Next interval returns within ±25% of prior |
|---|---|---|---|---|
| oct09_healthy | 5,095 | **226** | 226 | 217 |
| oct09_run1_onset | 58 | 0 | 0 | — |
| oct09_run2_onset | 298 | 20 | 20 | 20 |
| oct10_121800 | 5,739 | **182** | 182 | 158 |
| Oct 9 full evening | 12,960 | **643** | 628 | 617 |

**Ratio distribution I(i)/I(i−1), full evening:**

| Percentile | Value |
|---|---|
| p50 | 1.000 |
| p90 | 1.078 |
| p99 | 2.956 |
| p99.9 | 3.098 |

Ordinary interval-to-interval variation is small; the top 1% is almost entirely the doubled and tripled intervals. Only 107 of 12,960 ratios fall between 1.5 and 1.85.

**What the candidates look like.** Candidate intervals measured against the median of the preceding five intervals, rounded to the nearest 0.5:

| Multiple of median5 | Count |
|---|---|
| 1.5× | 1 |
| 2.0× | 396 |
| 2.5× | 12 |
| 3.0× | 234 |

In every case the candidate exceeded 1.5× median5, so none was caused by a short preceding interval acting as the comparator. 637 of 643 exceeded 1.85× median5.

### Examples (raw transmitter intervals in ms)

**19:14:17, PWM 74.** A doubled interval every 9 counted pulses:

```
s15181:94  s15190:97  s15199:100  s15208:96  s15217:91  s15226:91
s15235:48 (no doubling this revolution)  s15245:94
```

Normal intervals in this run are 41–50 ms.

**19:52:10–13, PWM 90.** A tripled interval every 8 counted pulses (two spokes per revolution):

```
s31549:125  s31557:123  s31583:125  s31609:115  s31617:113  s31625:117
```

Normal intervals are 36–42 ms. A doubled interval also appears at s31591:81 and s31601:76.

**Spacing.** Counted-pulse spacing between successive candidates over the full evening:

| Spacing | Count |
|---|---|
| 8 | 142 |
| 9 | 128 |
| 17–19 | 43 |
| 27–29 | 18 |

Spacings of 17–19 and 27–29 mean one or two intervening revolutions had no miss.

**Episodes.** Candidates cluster in time rather than spreading evenly:

| Time | Candidates per minute |
|---|---|
| 19:14 | 54 |
| 19:32–19:41 | 18–43 |
| 19:50–19:53 | 58–95 |
| Most other minutes | 0–2 |

---

## 5. Are these genuine missed pulses?

**The map-distance test.** For each pair of adjacent TARGET_CONFIRMED events I compared the counted IR distance (`opening_ir_um` difference) with the mapped interval from `NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h`. Spans were grouped by how many 185% candidates fell inside them. There were 670 adjacent confirmed spans.

| Candidates in span | Spans | Median counted ÷ mapped |
|---|---|---|
| 0 | 438 | **0.997** |
| 1 | 133 | **0.933** |
| 2 | 63 | **0.901** |
| 3 | 31 | **0.872** |
| ≥4 | 5 | 0.869 |

Each candidate costs about 3% of a ~300–340 mm span. One pulse is 9.652 mm, which is 2.9–3.2% of such a span.

Inside the healthy window alone, spans read 0.965 without candidates and 0.921 with them. The no-candidate figure is likely lowered by misses hidden in unlogged intervals.

**Why this points to missed pulses:**
- Each candidate interval is an integer multiple of the local spoke period, so the elapsed time is right for 2 or 3 spoke passages.
- The pattern is periodic once per wheel revolution.
- Each event removes about one pulse of counted distance.

**Competing explanations considered:**

| Explanation | Assessment |
|---|---|
| Speed variation, acceleration or PWM change | Would produce continuous ratios, not quantized 2.0× and 3.0× multiples locked to the wheel revolution. Rejected for nearly all candidates. |
| Wheel slip or skid | Changes distance per revolution but does not create exact multiples of the spoke period at a fixed wheel position. Unlikely. |
| Fixed wheel geometry (a missing or irregular spoke) | Would be present in every revolution. The pattern is episodic, absent for minutes at a time, so a fixed defect alone does not fit. A spoke that registers only marginally (contrast, alignment, wobble) fits better. **This cannot be determined from telemetry.** |
| Misidentified mile markers corrupting the map comparison | Would add random error, not a monotonic shortfall that grows with the candidate count. |

**Limits on this test:**
- Only spans that NAVI confirmed are included. These must fall within ±15%, which excludes heavily undercounted spans.
- NAVI's identity may be wrong in places.
- PWM is the last 1-second `loopstat` value, not the PWM at pulse time.

**Conclusion.** The evidence strongly supports genuine uncounted spokes. It is not proof. A bench or track check of the IR wheel and sensor during an episode would settle it (AGENTS.md §8).

**Consequence for calibration.** Spans with no candidate have a median of 0.997, which supports the installed 9,652 µm/pulse. Summary A rejected CODEX's 10,000 µm/pulse proposal on the grounds that miscounting could explain the same shortfall. This data supports that caution: the apparent miscalibration is consistent with missed pulses.

---

## 6. The preceding interval as comparator

**Synthetic characterization (not proof of real-world detection).** I took real intervals at steady positions with no nearby candidate and merged two adjacent ones, which is what a single uncounted spoke would produce. Detection rates:

| Comparator | Oct 9 healthy | Oct 10 | Oct 9 evening |
|---|---|---|---|
| Preceding interval × 1.85 | 94.2% | 94.9% | 94.0% |
| Median of 5 × 1.85 | 90.5% | 91.8% | 91.1% |
| Median of 5 × 1.50 | 100% | 100% | 99.9% |

**Weaknesses of the preceding-interval comparator:**

| Weakness | Evidence |
|---|---|
| **Short preceding interval inflates the ratio.** | 19:02:03: pre-intervals 58, 57, 37, 44, **10** ms, then 93 ms, ratio 9.3. A 10-ms interval suggests an extra count. |
| **Thin margin.** | A single miss gives a nominal ratio of 2.0. With 1-ms quantization at 35–40 ms intervals, plus acceleration, roughly 6% of synthetic misses fell at or below 1.85. |
| **The preceding interval must be received.** | On-board, if the Type-6 event carrying it is lost, the rule cannot evaluate. If the event carrying the miss is lost, the miss is invisible. The MQTT log cannot measure on-board ESP-NOW loss. |
| **Spacing between misses.** | No problem observed. Misses are about 9 pulses apart, so the comparator is rarely itself a doubled interval. |
| **Two adjacent missed spokes.** | Produce a single 3× event. This is enough to mark the interval corrupted, but it understates the pulse loss. |
| **Low PWM and coasting.** | 12 candidates at about 2.5×, for example 19:03:04 at PWM 10: 113 → 282 → 78 ms. These are ambiguous: stick-slip, or a miss during deceleration. Flags at reported PWM 0 (19:31:01–03, 3× intervals while coasting) appear only without PWM gating. |
| **Stops and starts.** | No candidates of 1 s or more appeared, so stop/start produced no false flags in this data. |

---

## 7. Implications for the revised requirements (operator decisions; none taken here)

1. **The §4 acceptance condition** ("healthy control must not be classified as corrupted") conflicts with this evidence. It would require a detector to ignore apparently genuine misses.
2. **Episodes are dense.** Misses occur about once per revolution for minutes at a time. Under requirements 4.5–4.7, most mile-marker intervals in an episode would be marked corrupted, so the 650-ms fallback would carry navigation for long periods. The physical cause may be a sensor or wheel issue, which is a hardware question and not something a threshold change can fix.
3. **A threshold change does not resolve the demonstrated problem.** A 185%-of-previous threshold flags the same events as the previous detector, and more of them, because those events appear to be real.
4. **Comparator choice.** If a detector is adopted, the synthetic data favours a median-based comparator with an explicit integer-multiple check over the preceding interval alone. Choosing the threshold is a design decision and is not made here.

---

## Reproduction

The scripts live in the session scratchpad and were not committed:

| Script | Purpose |
|---|---|
| `extract.py` | Event extraction |
| `rule185.py` | Rule evaluation |
| `period.py` | Candidate spacing |
| `mapcheck.py` | Map-distance correlation |
| `odd.py` | Atypical candidates |
| `comparators.py` | Comparator comparison |

---

## 8. Addendum: a segment-validity detector (characterisation, not approved)

The question: how could IR intervals be used to detect a missed spoke and mark the current mile-marker segment's IR distance invalid?

### The detector tested

**1. Single-interval rule.**
- P is the median of the last 5 accepted intervals.
- Flagged intervals are excluded from P.
- P is updated only when PWM > 0 and the interval is under 500 ms.
- With k = interval ÷ P, an interval is flagged when k ≥ 1.6 and k is within ±0.3 of an integer of 2 or more.

**2. Gap rule.** When a serial gap of g ≤ 4 occurs, the gap is flagged if (Δt ÷ P) − g ≥ 0.6. That means at least about one spoke period went uncounted. P is then re-baselined.

**3. Consequence of a flag.** The current segment's IR distance becomes invalid until the next verified Hall mile marker. Counts are never modified.

### Results on the full October 9 evening

| Measure | Result |
|---|---|
| Single-interval flags | 577 |
| Gap flags | 64, from 1,396 evaluable gaps |
| Gaps too large to evaluate | 2,025 |
| Adjacent confirmed segments invalidated | 30% of 670 |

**Counted ÷ mapped distance by segment status.** Only segments with 100% of pulses logged are shown, because MQTT loss hides misses:

| Segment status | n | Median | Share below 0.95 |
|---|---|---|---|
| Valid (no flag) | 19 | 0.997 | 11% |
| Invalid | 57 | 0.886 | 86% |

Across all segments, valid ones have a median of 0.982 with 30% below 0.95. That figure is degraded by misses inside unlogged intervals.

### Limits

- The fully logged valid sample is small (19 segments).
- There is no physical ground truth.
- Low PWM and coasting are ambiguous.
- A miss that occurs before P is rebuilt, or inside a large gap, cannot be detected.
- During an episode, most segments become invalid. The hardware cause still needs investigation.
- All thresholds are open for operator decision.
