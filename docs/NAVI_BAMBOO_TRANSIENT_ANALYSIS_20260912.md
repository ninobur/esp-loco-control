# NAVI_ONE — Bamboo CCW transient acquisition analysis

**Date:** 2026-09-12  
**Status:** The analysis and fixture remain evidence. The working tree now also
contains an operator-approved experimental 82 ms field-test implementation;
that implementation changes acquisition behavior and is not field-accepted.  
**Locomotive:** Otto, 9950011  
**Build:** `NAVI_ONE_1_0X16_FLOOR82_FIELDTEST`

## Field result

At 11:22:54.749 Otto accepted MM151 North while travelling CCW at PWM 90.
At 11:22:55.351 a second North passage was admitted 558 ms later while MM150
South was expected. Navigation correctly withdrew position. The genuine MM150
South passage arrived 689 ms later, after withdrawal.

The admitted event lasted 47 ms. Its raw oriented record exceeded Otto's
70-count entry threshold for exactly three consecutive 1 ms samples:
`78, 86, 93`; it then fell to 31. The baseline was flat. This was not a
station-departure plateau, a post-stop successor, morphology, or baseline
adaptation.

The exact 60-sample record is preserved in
`tests/fixtures_bamboo_20260912.h`. `gate_bamboo_transient.cpp` reproduces the
field peak and evaluates candidate screens without being included by
production firmware or the production gate runner.

## Candidate replay

| Candidate | Bamboo 47 ms transient | Existing evidence | Finding |
|---|---:|---|---|
| Accepted temporal median-of-three | peak 86, admitted | Current behavior | Reproduces the stop |
| Temporal median-of-five | peak 78, still above entry | Would amend accepted decision 0065 | **Does not solve this event** |
| Temporal median-of-seven | peak 61, from the later shelf | More aggressive smoothing; no operator authority | The spike itself falls to 52, but the reported peak moves to a different feature; indirect and fragile |
| Four consecutive entry samples | only three present | Genuine survey minimum observed: 24 ms above entry | Promising, but retained, decimated, and truncated records do not all preserve entry persistence well enough for complete replay |
| Completed-passage floor 82 ms | duration 47 ms | See corpus replay below | Approved experimental boundary; rejects the event |

The phrase “single-conversion fault” is not supported. `hallRead()` already
returns the median of five consecutive ADC conversions. The published samples
are the output of that acquisition filter. This was a short multi-sample
excursion at the 1 kHz Hall stream.

## Corpus replay

The duration candidate was checked against:

- Otto's hardware-consistent 2026-09-09 CW and CCW survey: 4,090 waveforms,
  including 3,164 records labelled admitted by the survey firmware;
- Toby's 2026-08-28 survey: 351 waveforms, including 195 primary passages;
- the six exact passages in the 2026-09-12 withdrawal window.

After excluding three PWM-zero records (one of which is the stationary
direction-switch record), an 82 ms completed-passage floor retains 3,159 of
3,161 moving survey records labelled admitted. The two it removes are:

| mm | duration | peak | significance |
|---:|---:|---:|---|
| 42 | 51 ms | 41 | below Otto's current 70-count entry threshold |
| 68 | 41 ms | 67 | the documented spurious accepted event; below current entry |

It retains all 195 Toby primary passages. Toby's minimum is 131 ms. In Otto's
clean, untruncated, current-threshold survey population, the shortest remaining
passage is 82 ms; the next shortest is 128 ms. The current field run's shortest
accepted genuine passage before the strike was 107 ms.

Nine of the eleven earlier measured Otto phantoms lasted 40–58 ms; the other
two durations are not recoverable from the repository. An 82 ms floor rejects
that measured band and the new 47 ms event. Because rejection is strictly
below the floor, the single known genuine 82 ms passage remains admissible at
equality, with zero margin above that observation; the next shortest is 128 ms.

The four-sample entry-persistence candidate also rejects this event, and all
complete ordinary survey passages clear it comfortably. It is not recommended
first because it adds a new state rule at passage entry and the truncated
records cannot all establish their original entry persistence. The duration
floor uses an existing, explicit electrical-noise screen at passage close.

## Recommendation

For the approved experimental field test, change the shared completed-passage
floor from 40 ms to 82 ms. The constant is shared, so this is fleet-wide rather
than Otto-only; Toby's measured primary-passage minimum is 131 ms.

Keep the unconditional 500 ms event-to-event guard unchanged. Do not add PWM,
map position, expected polarity, station state, or post-stop status to this
decision. A passage shorter than 82 ms is rejected as acquisition noise before
navigation sees it. The implementation adds background per-event diagnostic
reporting for floor rejections that has no influence on admission or
navigation; the aggregate counter remains as a cross-check.

This is a bounded, partial screen. No known genuine passage in the available
Otto or Toby corpora is shorter than 82 ms, but a future genuine passage below
82 ms would be silently absent from navigation, and phantoms of 82 ms or
longer remain possible.

The active Otto profile also contains a contradictory threshold derivation:
it says entry 70 excludes peaks 40–66 while the same profile records a
spurious maximum of 91. That record should be corrected. Do not raise the
entry threshold in this remedy: an earlier entry-90 field trial caused genuine
misses. Duration has direct evidence for this event without reopening that
threshold decision.

## Behavior-neutral reporting boundary

The current `HallCapture::close()` increments `floorRejects_` and returns
`false` before it constructs `out_`. Consequently, `hallTask()` cannot tell a
floor rejection from an ordinary sample on which nothing closed. Polling the
aggregate counter would identify that *something* happened but would not
preserve the event that was rejected.

The approved 82 ms implementation provides observability at that boundary:

1. On a sub-floor close, `HallCapture` stores a small one-shot acquisition
   record: opening time, closing time, duration, entry baseline, sample count,
   pre-roll count, decimation, clipping/truncation state, signed sum, and raw
   peak magnitude.
2. `hallTask()` consumes that record immediately after `sample()` returns and
   enqueues it on a diagnostic-only topic. It does not construct a `Judged`
   message, call `MagnetRecognizer::examine()`, or place anything on
   `judgedQ`.
3. The record is emitted once and then cleared. Queue failure increments the
   existing publication-loss accounting; it may not retry in a way that can
   block the 1 kHz Hall task.
4. The boundary gate proves that a sub-floor event creates exactly one diagnostic record
   and no passage, recognizer call, navigation message, or position change,
   while an 82 ms event follows the ordinary passage path.

This preserves the authority boundary: acquisition reports why it discarded
an electrical event, while recognition and navigation remain unaware of it.
Full waveform publication is not required for the first implementation and
should not be added casually to the 1 kHz task; the bounded metadata above is
enough to identify duration-floor behavior and correlate it with recorder
logs.

This does not solve or explain the separate multi-second departure plateaus at
Arches and Grillers. It also does not validate the provisional post-stop
resolver. Those remain separate work items.

## 90 ms follow-up

The operator directed an experimental 90 ms trial after reviewing the 60 ms
proposal. Pre-flash audit found that the original green suite exercised the
40 ms `CaptureConfig` default rather than the flashed value. Sharing 90 ms
between firmware and every acquisition gate exposed a blocking regression in
gate 8's sustained −50-count, opposite-polarity, below-tractive-floor case:
marker visibility changed from 20/20 at 40 ms to 0/20 at 90 ms.

A one-millisecond sweep found the transition between 87 and 88 ms: 40–87 ms
retained 20/20; 88–90 ms retained 0/20. This is independent of the corpus
constraint: a floor above 82 ms also rejects Otto's one known genuine 82 ms
passage. Thus 82 ms is the highest presently evidenced floor that both retains
every known genuine passage and remains below the gate-8 failure boundary.
The 90 ms image is not safe to flash merely for being labelled experimental.

## Reproduction

Compile and run `tests/gate_bamboo_transient.cpp` directly. It establishes:

- exact median-of-three peak: 86;
- median-of-five peak: 78 (still above entry);
- median-of-seven peak: 61;
- longest above-entry run: 3 ms;
- rejection by the active 82 ms duration floor and by the test-only
  four-sample persistence candidate.
