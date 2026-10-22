# NAVI non-IR target acquisition at PWM 90 -- experimental report

**Date:** 2026-10-22
**Status:** Offline research code, tests, plots, and this report only.
Authorizes no production firmware change, no physical magnet change, and
no revision to governing architectural decisions (`AGENTS.md` §2, §12).
**Primary architectural authority:**
[`docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`](../../docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md).
**Branch:** `research/navi-target-acquisition-pwm90-20261021`.

## The central question, answered directly

*Given a trusted starting magnet and the known magnetic map, can NAVI
follow the sequence of real Hall waveforms and reliably acquire each
successive target without IR?*

**Yes, for the recorded data available, once one specific threshold --
the 110 ms minimum-acquisition-duration figure the assignment itself
named as a starting point to evaluate, not a requirement -- is set to
match what real PWM-90 magnetic curves on this locomotive actually
provide.** With that one change, 162 of 162 target acquisitions advanced
the geographic sequence exactly one mapped magnet at a time (never two or
more, by construction), 88.9% diagnostically agree with independent
historical annotations (144/162; annotations are not ground truth -- see
below), and acquisition occurs within 20 ms of the qualifying departure,
well ahead of encounter closure. With the 110 ms figure as originally
proposed, the same architecture recognizes far fewer targets (63) and
produces many apparent polarity contradictions (59) that are, on
inspection, a downstream consequence of that one threshold -- not an
architectural defect, and not a map, baseline, or polarity-interpretation
problem. Both claims are evidenced below, with event-level data and a
labelled plot.

This is feasibility evidence from one locomotive, one recording, and one
operating condition. It is not a 99.9% reliability demonstration, and
§11 states precisely what would still be needed for one.

## 1. What was built

Exactly `CLOSE -> TARGET -> BASELINE -> ACQUIRE -> CLOSE AND ADVANCE`, as
fixed by the architectural document, implemented causally
(`target_acquisition.py`):

- **TARGET** is `route_map.next_marker(last_confirmed_mm, direction)` --
  the deterministic next mapped magnet, never a candidate set.
- **BASELINE** fits a line (level, slope, noise) from a quiet stretch of
  at least 0.65 s / 8 bins (decision 0093's existing Hall-only guard,
  reused rather than invented -- see §6), then **freezes** it. It is not
  re-extended sample by sample while ACQUIRE is watching; see §5 for why
  this specific choice was necessary and what it costs.
- **ACQUIRE** opens a candidate on a departure of at least 70 ADC counts
  for 2 consecutive 20-ms bins (the assignment's own stated criterion),
  resolves its sign against the FROZEN baseline, and checks that sign
  against the target's known polarity (`hall_sign_for_polarity`, the same
  convention as `RvmNavigator.h`, commit `3a8953c`: North = ADC above
  baseline). A matching candidate is provisionally **ACQUIRED** once its
  *active* departure has persisted for a configurable minimum duration
  (the assignment's 110 ms starting point, evaluated in §7). Return to
  baseline is recognized as 6 consecutive quiet bins (120 ms) against the
  same frozen reference -- the phenomenon, not a fixed total duration.
- **CLOSE AND ADVANCE** fires only when a matching, already-ACQUIRED
  candidate closes. A **non-matching** candidate (wrong resolved polarity)
  is logged as **CONTRADICTION** with its full evidence preserved and is
  disposed without advancing, without relabeling it as a different mapped
  magnet, and without abandoning the target. A matching candidate that
  never reaches the duration gate before closing is logged **DISPOSED**,
  also without advancing.
- There is no automatic distance- or time-based LOST transition anywhere
  in this module, no elapsed-time acquisition timeout, and no maximum
  rejected-observation count -- none of the four were implemented, per
  the assignment's explicit prohibition and the architecture document
  this experiment follows.
- Every threshold the recognizer uses and its justification is in the
  module docstring and `target_acquisition.py`'s own header comments; none
  is derived from IR or the historical ±15% tolerance.

## 2. The data

`raw/otto_train.bin`, Otto, boot `15549e9385faa4d5` (the `otto_train`
stream), cross-checked against `data/train_annotations.csv`, from
`research/NAVI_CPR_0_1` (per `sources/data_manifest.json`, the same
lineage `98db802` and `a15036f` used). This raw data has never been
committed to this repository on any branch (`research/NAVI_CPR_0_1/.gitignore`
excludes it); it was located outside the repo at two copies, verified
identical to each other and to the manifest's own recorded hash
(`b366bff8f17d28c64f7b666b68e171cef8521dcd1a791ecf6b147713506d0b20`). See
`README.md` for both paths. Direction is CW (`direction=1`) throughout;
confirmed against the raw stream's own `direction` field (constant `1`
across all 8,468,560 reads) and against every annotation's `dir` field.

**Polarity-encoding cross-check, done rather than assumed:** the
annotation CSV's `pole` column (`-1`/`+1`) was checked against
`route_map.polarity_at(mm)` for all 376 OPENING rows. `pole=-1 <->
South(0)`, `pole=+1 <-> North(1)` holds for **376/376**, zero mismatches
(`annotations.py`'s own header documents this). This is the only place
the annotations' content was used to validate something about the
recognizer's own inputs (the map/polarity convention); it is independent
of, and prior to, any causal replay decision.

**Scope actually run:** the recording was split into maximal contiguous
stretches where every 20-ms bin's native PWM is 90 or 0 (ordinary running
or an ordinary stop at constant commanded power -- this experiment's
declared Stage-1 scope; any other PWM value, e.g. a ramp transition, ends
a stretch). Nine such stretches of at least 15 s were found and seeded
with a starting position (see §3); four longer stretches (15.7-217.4 s)
had no historical OPENING annotation anywhere inside their time window to
seed from and were **not run** -- not because the data was unusable, but
because the annotation effort that produced `train_annotations.csv`
itself only covers select windows of the ~1800 s recording, leaving gaps
up to 287 s (verified directly: zero OPENING rows inside any of the four
skipped windows, against gaps of 287 s, 227 s, 112 s, and the recording's
own first annotation at 128 s excluding a 100-115 s stretch entirely).
Per the assignment's explicit instruction, no synthetic or manufactured
starting position was substituted.

## 3. What "trusted starting position" means here

This recording carries no independently surveyed ground truth. The
historical OPENING annotations are themselves explicitly disqualified
from that role by their own source documentation (`a15036f`'s
`REPORT.md`: "ADC-derived historical comparison hypotheses, not
independent physical ground truth"). Each of the nine trials is therefore
seeded from **the first historical OPENING annotation at or after the
trial's start**, used as a stand-in for what an operator's own session-
start declaration would supply in real operation -- not a discovered
truth, and not fed into the recognizer's own decisions from that point
on. Everything after the seed is this recognizer's own causal output.
This substitution, and its limit, is stated here precisely because §11
requires it: a different, independently-surveyed starting reference could
in principle shift every subsequent `target_mm` label by a constant
offset without changing anything about whether the *sequence* of
decisions was internally coherent, which is most of what this experiment
actually tests.

## 4. Results

### A/B -- geographic sequence and magnetic recognition, by configuration

| | 110 ms (assignment's stated starting point) | duration floor (evaluated alternative, §7) |
|---|---:|---:|
| Trials run / skipped (no seed) | 9 / 4 | 9 / 4 |
| Candidates opened | 190 | 190 |
| **CLOSE_AND_ADVANCE** (acquired targets) | 63 | **162** |
| CONTRADICTION (wrong polarity, disposed) | 59 | 14 |
| DISPOSED (matching but under duration) | 58 | 4 |
| Incorrect geographic advancement (2+ magnets at once) | **0 (structurally impossible)** | **0 (structurally impossible)** |
| Diagnostic agreement with historical annotations | 44/63 (69.8%) | **144/162 (88.9%)** |
| Diagnostic disagreement | 19 | 14 |
| No annotation within ±0.5 s of closure | 0 | 4 |

"Incorrect geographic advancement" cannot occur under either
configuration by construction -- `TARGET` is always exactly
`next_marker(last_confirmed, direction)`, so every `CLOSE_AND_ADVANCE`
moves the trusted position by exactly one mapped magnet, never more. This
satisfies the assignment's "do not implement automatic advancement by two
or more magnets" directly, as an architectural property, not an observed
absence.

**The diagnostic agreement figures are agreement with historical,
ADC-derived annotation hypotheses, not a false-positive/false-negative
rate against independent physical ground truth** -- repeating the same
caveat `98db802`'s own `REPORT.md` states, because it applies here with
exactly the same force. `results_*/annotation_diagnostic.json` carries
every individual disagreement for inspection.

### Why the two configurations differ this much (root cause, not a second rule)

Every one of 58 "DISPOSED" candidates under the 110 ms configuration
shares the same shape: a genuine, high-amplitude departure (peak 71.7-
233.8 ADC, well above the 70-count floor, median 174.6) that returns to
baseline within 0.04-0.12 s of *active* duration -- consistently *at or
just under* the 110 ms gate, never comfortably past it. Inspecting the
raw Hall trace directly (Trial 6, MM126-137, `results_*/example_trial6_mm126_137.png`)
shows why: these are real, clean, narrow (~0.1-0.2 s wide at this
locomotive's PWM-90 speed) magnetic curves, visually textbook Gaussian
bumps with high signal-to-noise, occurring on a regular ~1.1-1.3 s cycle
consistent with the corrected width study's own reported baseline/ensemble
timing (`a15036f`). They are simply narrower in **active** duration (the
portion exceeding this recognizer's own quiet-vs-active split) than 110 ms
asks for.

The consequence cascades: when the matching candidate for a target is
disposed rather than acquired, the target does not advance. The *next*
real, correctly-timed encounter -- which is usually a **different**
physical magnet with a **different**, often opposite, polarity -- then
gets compared against the *same, still-unadvanced* target and is logged
as a **CONTRADICTION**. Plotting the 110 ms run over the same window
(`results_110ms/example_trial6_mm126_137.png`) makes this directly
visible: a dense run of real, well-formed curves with zero closures and
repeated contradictions, over an identical signal to the floor-
configuration's clean sequence of closures.

This is the specific determination the assignment's §11 asks for: the
problem is not the recorded signal, baseline estimation, target-polarity
interpretation, or the map (all four are shown working correctly in the
successful floor-configuration run against the *same* signal) -- it is
the 110 ms duration figure, the one threshold the assignment itself
flagged as "a candidate minimum duration... an empirical starting point
to evaluate, not an unquestionable requirement." It is evaluated, not
replaced by a second independent rule: the floor configuration requires
only the already-justified 2-consecutive-bin departure criterion (0.02-
0.04 s), adding nothing beyond what opening a candidate already requires.

### C -- timing

| | 110 ms | duration floor |
|---|---:|---:|
| Time from first qualifying departure to ACQUIRED (median) | 0.10 s | 0.02 s |
| Time from first qualifying departure to CLOSE (median) | 0.12 s | 0.12 s |
| Time from first qualifying departure to CLOSE (max) | 0.18 s | 284.58 s (one case; see below) |

Acquisition under the duration-floor configuration occurs essentially as
soon as a candidate opens with a resolved, matching sign (median 20 ms) --
well before encounter closure (median 120 ms later). **Waiting for
closure is not assumed operationally acceptable**, per the assignment's
own instruction: the `ACQUIRED` event exists precisely so a future
station-stop control decision could act on it without waiting for `CLOSE`,
and this data shows that distinction is real and fast, not merely
notional.

**One individually-inspected anomaly, not generalized into a rule:** one
candidate (Trial 0, target MM129) was ACQUIRED in 0.02 s but did not
CLOSE for 284.58 s, with native PWM constant at 90 throughout (verified
directly -- no PWM=0 stop occurred). The cause is a limitation of this
experiment's own baseline design, not the signal: §5 freezes the baseline
fit rather than re-extending it, and extrapolating a fixed slope across
nearly five minutes of real operation is not justified -- small genuine
drift eventually keeps every subsequent sample just outside the frozen
reference's quiet band, so the 6-consecutive-quiet-bin return-to-baseline
test never completes. This is reported individually, per the assignment's
instruction, rather than classified as a navigation failure: the target
was correctly acquired and never contradicted or abandoned; closure was
simply delayed by an implementation limitation in long-duration baseline
extrapolation. §11 states this as unvalidated and names the fix direction
(periodic baseline re-establishment during a long quiet stretch) without
implementing it, which would be new scope.

### D -- contradictory evidence

All 59 (110 ms run) / 14 (floor run) `CONTRADICTION` rows are preserved
in `events.csv` with their resolved sign, peak departure, and duration,
and were not automatically classified as navigation failures. Individually
inspecting the floor run's residual 14: most are a single contradicting
candidate immediately preceding a correctly-matching one at the same
target (e.g. Trial 6 MM135 took four attempts: three contradictions, then
an acquisition), consistent with a secondary, smaller-amplitude disturbance
(plausibly ringing or a near-magnet artifact, given the annotation data's
own distinction between "disk" and "bar" magnet types) preceding the true
curve rather than a genuine map or polarity error -- but this experiment
does not have an independent way to confirm that explanation, and does
not claim to.

### E -- continuity

Throughout both runs, no candidate was ever relabeled to a different
mapped magnet than the target it was tested against (`events.csv`'s
`target_mm` column for every `CONTRADICTION`/`DISPOSED` row is always the
target that was active, never invented); no stationary (PWM = 0) sample
contributed to baseline, candidate duration, or RTB in either run (the
pause logic in `target_acquisition.py` is exercised and unit-tested, §9,
though -- as §2 notes -- the actual eligible stretches used here happened
to be constant PWM = 90 throughout, so this specific guarantee is
validated by synthetic test, not by this replay).

## 5. Baseline-estimation method, and why it was chosen this way

The architecture document requires "do not extrapolate a fitted baseline
through an active magnetic curve." The first implementation attempt
(before this report's results were produced) violated this inadvertently:
re-fitting the baseline line from every sub-70-ADC sample, growing the
fit's window indefinitely while quiet, caused the fitted line to *track*
a slow-rising/falling curve's leading edge before it ever crossed the
hard departure threshold, because the least-squares fit absorbed the
trend. A synthetic test (`test_correct_polarity_curve_is_acquired_and_closes`)
caught this directly: a textbook Gaussian curve produced zero `ACQUIRED`
events under the original design. The fix -- establish the baseline fit
once from a genuinely quiet stretch, then **freeze** it until the next
encounter closes -- is what `target_acquisition.py` implements and what
produced every result in §4. Its own limitation (extrapolation drift over
very long active-candidate durations) is the §4/C anomaly discussed
above, and is the specific, evidence-grounded "what remains unvalidated"
item for this method.

## 6. Thresholds used and their justification

| Threshold | Value | Justification |
|---|---:|---|
| Hall departure criterion | 70 ADC counts, 2 consecutive 20-ms bins | Given verbatim by the assignment. |
| Minimum baseline span | 0.65 s / 8 bins | Decision 0093's existing Hall-only fallback guard -- reused, not invented; conservative relative to the corrected width study's own measured minimum preceding-baseline of 0.72 s (`a15036f`). |
| Minimum acquisition duration | 0.11 s (primary) / 0.02 s (evaluated) | The assignment's own PWM-90 starting point, evaluated per its own instruction -- see §4/§7, not a new independent threshold. |
| Return-to-baseline | 6 consecutive quiet bins (120 ms) | Unchanged from `98db802`'s own causal method; RTB recognized from the phenomenon (0107), not a fixed total duration. |
| BIC fit window | 150 bins (3 s) | Performance bound only (keeps the per-sample cost of the supportive ΔBIC trace from growing with how long a candidate stays open); does not change which samples decide ACQUIRE/RTB. Chosen generously relative to every measured curve duration in the corrected study (max 1.67 s). |

No threshold here is derived from IR or the historical ±15% tolerance.

## 7. Evaluating the 110 ms figure, as the assignment explicitly asked

§4 already shows the result; this states the evaluation plainly, per the
instruction not to simply assert a new number. The 110 ms figure was
`98db802`'s own *declaration* gate for a standalone recognizer with no
target context, tuned against a different evaluation (historical-
annotation overlap, not causal target-matching). Under this experiment's
architecture, where the departure criterion and the target's known
polarity already jointly establish that a real, correctly-signed
encounter is occurring, an additional 110 ms sustained-duration
requirement is not supported by the data: every disposed-but-matching
candidate in this recording tops out at 0.12 s of active duration,
meaning the 110 ms gate was, in practice, rejecting essentially every
genuine encounter on this recording rather than discriminating real ones
from noise. The recommended change, evidenced directly above, is to drop
the separate duration requirement to the floor already implied by the
departure criterion itself (2 consecutive qualifying bins) rather than
layer a second, redundant threshold on top of it -- consistent with the
assignment's own instruction not to add independent thresholds simply
because a measurement is available.

## 8. Tests

`tests/test_target_acquisition.py`, stdlib `unittest` (no `pytest` in
this environment), 12 tests, all synthetic, all passing:

```
test_baseline_estimation_from_quiet_samples ... ok
test_baseline_not_trusted_before_minimum_span ... ok
test_causal_no_future_sample_access ... ok
test_contradicting_evidence_is_never_relabeled ... ok
test_correct_polarity_curve_is_acquired_and_closes ... ok
test_extended_acquisition_has_no_automatic_timeout ... ok
test_no_advancement_before_closure ... ok
test_short_noise_excursion_below_duration_is_disposed_not_contradiction ... ok
test_stationary_samples_do_not_invent_progression ... ok
test_stop_mid_candidate_preserves_evidence ... ok
test_target_is_deterministic_next_marker ... ok
test_wrong_polarity_disturbance_is_preserved_not_advanced ... ok
----------------------------------------------------------------------
Ran 12 tests in 0.54s -- OK
```

These synthetic tests, not the real-recording replay, are what directly
validate: no invented progression at PWM=0; preservation of contradictory
evidence without relabeling; no advancement before closure; no automatic
timeout on an artificially long (8 s synthetic) open candidate; and
causal operation (an event is never timestamped beyond the samples
observed so far). The real-recording replay is what the 110 ms/
duration-floor comparison in §4 validates, and is explicitly separate
from these synthetic tests, per the assignment's instruction.

## 9. Interpretation, scope, and what is not claimed

- **Not claimed:** 99.9% reliability, in either configuration. 162
  independent closures across 9 trials on one locomotive, one recording,
  one speed, is feasibility evidence, not a reliability demonstration.
- **Not claimed:** that the duration-floor configuration is now "the"
  correct value. It is the evaluated floor implied by the departure
  criterion already in use; a principled value between 0.02 s and 0.11 s,
  derived from a larger independent dataset, is future work, not asserted
  here.
- **Not claimed:** anything about Toby, CCW running, other PWM values,
  acceleration/deceleration, or stop/restart interaction with a real stop
  event (this recording's eligible stretches happened not to contain a
  PWM=0 interval; the pause logic is unit-tested synthetically, §8, but
  not replay-validated against a real stop).
- **Established by this experiment:** the architecture's own structural
  guarantees (single-step advancement, no fabricated LOST transition, no
  relabeling of contradicting evidence, no IR/tolerance dependency) hold
  throughout both real-data replays and the synthetic test suite; the
  110 ms duration gate is empirically responsible for the large majority
  of this recording's apparent recognition failures, not the map,
  baseline, or polarity logic; and one specific, named limitation (long-
  duration baseline extrapolation) is identified with a concrete example
  rather than papered over.

## 10. Branch, commit, and files

Branch: `research/navi-target-acquisition-pwm90-20261021`. See the commit
this report is delivered with for the exact hash and file list (this
report does not self-reference its own commit hash to avoid it going
stale relative to the actual commit).

## 11. Recommended next steps, not authorized by this document

1. Re-evaluate the 110 ms figure against a second, independent PWM-90
   recording (and, separately, against an independently-bounded ground-
   truth set, per the standing limitation both `98db802` and this report
   share) before treating the floor value as more than an evaluated
   starting point.
2. Address the long-duration baseline-extrapolation limitation (§4/C,
   §5) -- most directly by re-establishing the baseline from a fresh
   quiet stretch after some bounded quiet interval even without a closed
   encounter, rather than extrapolating one fixed slope indefinitely.
3. Extend to PWM 60, acceleration/deceleration, and genuine stop/restart
   replay once available, per the architecture document's own staged
   plan -- not attempted here.
4. Toby, per the architecture document's own Stage 5 -- not attempted
   here; this recording is Otto-only.

None of the above is authorized by this report; per `AGENTS.md` §2, each
would need its own explanation and approval before implementation.
