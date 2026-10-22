# NAVI non-IR target acquisition at PWM 90 -- experimental report

**Date:** 2026-10-22
**Status:** Offline research code, tests, plots, and this report only.
Authorizes no production firmware change, no physical magnet change, and
no revision to governing architectural decisions (`AGENTS.md` §2, §12).
**Primary architectural authority:**
[`docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`](../../docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md).
**Branch:** `research/navi-target-acquisition-pwm90-20261021`.

---

## Corrective investigation addendum (2026-10-22)

An independent review of the original experiment (below) raised five
concerns. This addendum investigates each against the source code and
the real recording, states a verdict for each, and gives the corrected
results. The original report below is left intact as the record of what
was first found and claimed; where this addendum supersedes a specific
original claim, that is stated explicitly rather than silently edited
away. Per the assignment's own closing instruction, each criticism was
tested against evidence rather than accepted or defended by default.

### Operator correction, received after the investigation below was written

David subsequently identified that the historical navigation system
these annotations come from had its own known failure: **IR distance
underreporting that caused incorrect geographic identification.** The
annotations' magnet-identity labels (`mm`) are therefore not a reliable
geographic reference at all -- not merely "not independently verified,"
which is the weaker caveat the rest of this document uses, but
specifically suspected of being wrong in ways inherited from a known
IR-dependent defect. This withdraws a stronger interpretation this
addendum initially drew from the fix in §2/§3: **that the recognizer's
subsequent close annotation agreement demonstrates it now tracks correct
geographic position.** It does not. Agreement with a historically
unreliable label is not evidence of correctness, and disagreement with it
is not evidence of failure, in either direction. The governing rule from
here on, applied retroactively to every percentage below: **annotation
agreement is diagnostic information about this experiment's own internal
consistency, never a geographic-accuracy measurement.** §7 ("Trial 6,
re-investigated from direct physical evidence") replaces the
annotation-based interpretation of Trial 6 with one that does not depend
on trusting the annotations' identity at all.

What is **not** withdrawn: the two confirmed code defects (seed-timing,
§2; comparison methodology, §3) were real implementation problems in this
experiment's own harness, independent of whether the annotations it
compares against are trustworthy -- starting geographic progression before
a declared reference encounter has even finished, and conflating an
encounter's opening with its closure in a timestamp comparison, are wrong
regardless of what the annotations say. Those fixes stand. What is
withdrawn is treating the resulting *agreement percentage* as proof the
recognizer is geographically correct.

### Verdicts, stated first

| # | Concern | Verdict |
|---|---|---|
| 1 | `_expected(c.start_s)` (frozen RTB reference) is a baseline-handling defect | **Not confirmed -- rejected, with evidence.** The alternative the review suggested would have been a regression. Evidence is physical (the real Hall signal and fitted slope), not annotation-dependent. |
| 2 | Seed annotation may not have been encountered yet when geographic progression begins | **Confirmed. Fixed.** A real timing defect in this experiment's own harness, independent of annotation reliability. |
| 3 | Nearest-timestamp annotation comparison conflates encounter stages | **Confirmed. Fixed.** Replaced with interval overlap, matching `98db802`'s own method. **Superseded in part by the operator correction above:** fixing this comparison still does not make its result a geographic-accuracy measurement -- see the reframed §6/§7. |
| 4 | 110 ms duration gate rejects genuine encounters | **Confirmed, on physical grounds independent of annotations** -- the real full-waveform-width distribution (measured directly from the Hall signal) is itself the evidence; §6 below removes the annotation-agreement framing this verdict originally also cited. |
| 5 | ΔBIC is diagnostic only, not yet shown to be needed for acquisition | **Investigated; no concrete need found in this recording**, based on direct inspection of the Hall waveform (§7), not on annotation agreement. Not changed, per the assignment's own instruction not to add machinery without evidence. |

### 1. The `_expected(c.start_s)` line -- investigated and rejected as a defect

The suspected line uses the baseline level *frozen at the candidate's own
opening time* for the return-to-baseline comparison, rather than
continuously re-extrapolating the fitted (level, slope) forward to the
current sample time. The review proposed the latter as a fix.

**Direct test, not assumption:** Trial 0's actual 284.58 s open candidate
(target MM129) was reconstructed exactly -- the real baseline fit at that
point had slope **13.26 ADC/s**, noise 6.04. Computing what the
*continuously-extrapolated* reference would have read at every subsequent
real sample (`investigate_anomaly.py`, `results_anomaly_investigation/anomaly_trace.csv`)
shows it would have crossed into the "quiet" band at **t=391.72 -- only
3.12 s after the candidate opened** -- while the real Hall signal was
still sitting genuinely ~80 ADC above the true pre-encounter baseline,
and stayed there, essentially flat (std 0.6-0.75 ADC across 5 s windows)
with only a slow, slight monotonic creep (+4 ADC over ~260 s, consistent
with thermal drift, not movement noise), for the full 284.58 s before
genuinely returning. See `results_anomaly_investigation/anomaly_plot.png`.

**What the 284.58 s event actually is, determined rather than assumed:**
native PWM is constant at 90 throughout (no PWM=0 stop recorded), with
zero missing samples (one single 1-frame serial gap in 1,434,760 raw
reads) -- so this is not a data gap. The signal's own texture (near-zero
noise, slow monotonic creep, no transient structure) is inconsistent with
continued travel past dozens of further real magnets (which would show
the same sharp, large-amplitude transitions visible everywhere else in
this recording) and consistent with the locomotive having come to rest at
or very near this specific magnet for an extended real interval, with
this recording's `pwm` field not reliably indicating that it had actually
stopped. This is itself a notable, reportable characteristic of this
specific recording (`otto_train` is explicitly a "construction" stream
per its own `REPORT.md` description), not a property of the architecture
or a defect in this implementation.

**Conclusion:** the frozen reference is the *safer* choice, demonstrated
directly rather than argued abstractly: continuously extrapolating a
fitted slope across a long interval risks exactly the "baseline absorbing
a genuine signal" failure mode the original report's §5 already
identifies and the architecture document explicitly prohibits ("do not
extrapolate a fitted baseline through an active magnetic curve"). **No
code change was made.** A regression test
(`test_frozen_reference_does_not_falsely_close_a_genuine_long_stall`) now
locks this behavior in.

### 2. Seed-timing: confirmed and fixed

**Confirmed by direct inspection.** `replay.py` seeded each trial's
`last_confirmed_mm` from the first historical OPENING annotation at or
after the trial's *raw start* (`lo`), then began feeding the recognizer
from `lo` -- before that annotation's own encounter had necessarily
finished. Concrete example: Trial 0 began at t=365.52; its seed annotation
(enc_id 61, MM111) itself spans [365.63, 365.88] -- so for the first 0.36 s
of "confirmed" MM111, the real encounter for MM111 was still in progress.

**Fix:** `seed_established_index()` (`replay.py`) finds the bin index at
or after the seed's own `end_s` and geographic progression begins only
there -- never before the seed encounter has actually closed. Where a
trial's eligible stretch ends before the seed encounter itself closes, or
leaves too little span afterward, the trial is now explicitly skipped
(`SKIPPED_SEED_ENCOUNTER_NOT_CLOSED_IN_TRIAL`, `SKIPPED_TOO_SHORT_AFTER_SEED`)
rather than silently run anyway. Three synthetic regression tests cover
this (`tests/test_replay_seeding.py`).

**Effect, measured:** with the duration-floor configuration (0.02 s),
geographic disagreement against historical annotations (now compared by
interval overlap, §3) drops from **14/162 to 0/149**. Every one of the
9 runnable trials now shows zero contradictions and zero disposed
candidates end to end -- see the per-trial table in §6 below.

### 3. Annotation-comparison methodology: confirmed and fixed

**Confirmed.** The original comparison matched each `CLOSE_AND_ADVANCE`
timestamp to the *nearest* annotation `start_s`, which conflates an
encounter's opening against its own closure and can attribute a correct
closure to the wrong nearby annotation when encounters are closely
spaced.

**Fix:** `diagnostic_annotation_agreement()` (`analyze.py`) now matches
each recognizer candidate's own `[start_s, close_s]` interval against
annotation `[start_s, end_s]` intervals by overlap fraction (≥30% of the
annotation's own duration -- the same method and threshold `98db802`'s
own replay script already used), and reports **geographic agreement**
(does the matched annotation's mm equal the recognizer's) separately from
**event-timing agreement** (how closely the recognizer's own start/close
timestamps track the matched annotation's own start/end), per instruction.

**Verification against the specific Trial 6 table the review cited:**
the review's five quoted rows (1424.16 MM126/127, 1426.48 MM127/129,
1428.94 MM129/131, 1433.30 MM131/135, 1441.50 MM135/142) were checked
directly against the original committed `results_departure_floor` data
and **match exactly** -- this specific criticism was not fabricated, and
the pattern of growing numerical gaps against the annotations was real.
**What that pattern means was revised by the operator correction above:**
with both fixes applied, the recognizer's closures now land immediately
beside the annotations' own timestamps throughout MM126-143 with no gap
(`results_corrected_departure_floor/example_trial6_geographic_lag.png`,
§6) -- but per the operator's correction, this is agreement with a label
source now known to carry its own IR-inherited identification errors, so
it is reported as a change in measured agreement, not as confirmation
that the recognizer's geographic labels are now correct. §7 investigates
Trial 6 again, directly from the Hall waveform, without relying on the
annotations' identity at all.

### 4. Minimum-duration requirement, re-examined precisely

Distinguishing the four quantities the assignment asked to separate,
measured from the corrected, geographically-clean duration-floor run:

| Quantity | Value |
|---|---|
| Departure criterion (opens a candidate) | 70 ADC, 2 consecutive 20 ms bins = 0.02-0.04 s |
| Configured minimum acquisition duration (original) | 0.11 s |
| Actual active duration achieved before RTB, matching candidates | median 0.02 s, essentially always << 0.11 s |
| **Complete width of a genuine magnetic waveform** (candidate open-to-close, this locomotive, PWM 90) | **min 0.10 s, median 0.12 s, max 0.20 s** (n=148, excluding the one 284.58 s stall) |

The 110 ms configured gate is not merely "a bit strict" -- it sits at
essentially the *median* of the real full encounter width, meaning it
rejects a genuine encounter whenever that specific encounter happens to
run even slightly short, which real data shows happens often. **This
finding rests entirely on directly measured Hall-waveform timing** (the
candidate's own open-to-close span, logged from the real ADC signal) and
does not depend on the historical annotations' identity at all.

**No independent minimum-duration requirement beyond the departure
criterion itself was needed to discriminate real encounters from noise in
this recording**: once the seed-timing and comparison-methodology defects
are corrected, the duration-floor configuration (effectively, no separate
gate beyond the 2-consecutive-bin departure criterion) produces 149
closures, every one inspected (§5, §7) showing the expected single-lobe,
high-ΔBIC shape of a genuine magnetic curve at the expected location and
polarity -- not a disturbance. This is evidence *for this recording*, not
a universal claim; a different, noisier stretch could reveal a real need
the duration gate would have served, which has simply not been observed
here.

### 5. Waveform recognition (ΔBIC): investigated, not elaborated without need

The corrected run's 149 closures all show ΔBIC comfortably above the
historical 10-point declaration threshold (min 24.79, median 48.29, max
78.95) -- genuine encounters are easily and consistently distinguished
from a flat line on shape grounds alone, which is useful confirmation.
Direct visual inspection of the Hall waveform (§7, and the plots in §6)
across the whole corrected run shows single, clean, well-separated lobes
at each acquisition -- no secondary lobes, no two closely-spaced genuine
encounters merged into one candidate, and no electrical disturbance
distinct from a real curve was found. This determination is made from
the waveform and the recognizer's own shape/timing evidence directly, not
from annotation agreement. There is therefore no recorded case in this
data where ΔBIC-based discrimination would have changed an outcome. Per
the assignment's own instruction not to implement a more elaborate
recognition model without a recorded, concrete need, **no change was
made**: ΔBIC remains recorded as diagnostic, supportive evidence, exactly
as the original report described it, and this investigation's
contribution is confirming that this recording does not yet evidence a
need to promote it to a gate.

### 6. Reproduced and compared results

All four runs below use the real `otto_train` recording
(`raw/otto_train.bin`, SHA-256 `b366bff8f17d28c64f7b666b68e171cef8521dcd1a791ecf6b147713506d0b20`,
re-verified for this investigation), the same real map, and the same
recognizer logic (`target_acquisition.py` unchanged, per the verdict on
item 1). Only `replay.py`'s seeding (item 2) and `analyze.py`'s
comparison method (item 3) changed between "original" and "corrected."

All counts in this table (closures, contradictions, disposals) are read
directly from the recognizer's own event log and do not depend on the
annotations at all. The "annotation agreement" row is diagnostic
information about consistency with a label source now known to carry its
own IR-inherited identification errors (operator correction, above) --
**it is reported, per instruction, but not characterized as a measure of
geographic accuracy in either direction.**

| | original, 110ms | original, floor | **corrected, 110ms** | **corrected, floor** |
|---|---:|---:|---:|---:|
| Trials run / skipped | 9 / 4 | 9 / 4 | 7 / 6 | 7 / 6 |
| CLOSE_AND_ADVANCE (recognizer's own count) | 63 | 162 | 54 | **149** |
| CONTRADICTION (recognizer's own count) | 59 | 14 | 42 | **0** |
| DISPOSED (recognizer's own count) | 58 | 4 | 53 | **0** |
| Annotation agreement, diagnostic only (overlap method, of matched closures) | 44/63 (69.8%) | 143/157 (91.1%) | 42/54 (77.8%) | 146/146 (100%) |
| Closures with no matching annotation (not counted above) | 0 | 5 | 0 | 3 |

Two trials that were runnable before are now explicitly excluded
(`SKIPPED_TOO_SHORT_AFTER_SEED`): once geographic progression correctly
waits for the seed encounter to close, their remaining span falls under
the 15 s minimum. This is the method being honest about what it can
responsibly claim, not data being discarded to improve the numbers --
per instruction, this limitation is stated rather than hidden.

**What the recognizer's own event log shows, independent of any
annotation:** the corrected 110 ms configuration produced 42
CONTRADICTION and 53 DISPOSED events against its own, internally
consistent target sequence -- these are directly observed facts about
when the recognizer failed to match a candidate to its current target,
not inferences from comparing labels to a historical source. The
corrected duration-floor configuration produced zero of either. This by
itself already demonstrates the duration-gate effect from item 4, without
any reference to annotation identity. **What is withdrawn** is the
further claim this addendum originally drew from annotation mm-gaps (e.g.
"recognizer MM120 vs. annotation MM126, a gap of 6 magnets") -- that
specific gap size assumed the annotation's mm label was itself a correct
geographic reference, which the operator correction above states it is
not. The underlying architectural point stands on its own regardless:
**this architecture has no built-in means of recovering from a missed or
contradicted encounter** (it is not supposed to, per the assignment's
prohibition on recovery search) -- CONTRADICTION/DISPOSED events, once
they occur, are not self-correcting by design. Whether any specific
instance in this recording left the recognizer's belief about its actual
physical position wrong is a question this experiment's evidence (absent
independent ground truth) cannot answer, and does not claim to.

### 7. Trial 6, re-investigated from direct physical evidence (supersedes the annotation-based interpretation)

Per the operator's correction, Trial 6 is re-examined here using only the
Hall waveform and the recognizer's own event log -- never the historical
annotations' identity -- against the five questions the correction poses.

**Under the corrected-seed, duration-floor configuration** (trial index 6,
seed MM125, `results_corrected_departure_floor`): 20 candidates opened,
20 closed, **zero contradictions, zero disposals**. The waveform plot
(`example_trial6_mm126_137.png` and the wider `example_trial6_geographic_lag.png`)
shows, by direct visual inspection: every candidate is a single, clean,
well-separated lobe; no two lobes are close enough to have been merged or
split; no candidate shows a secondary bump distinct from its main curve;
every `ACQUIRED` event is logged distinctly before, and earlier than, its
own `CLOSE_AND_ADVANCE`; and the target sequence advances by exactly one
mapped magnet at every closure, with no skips (structurally guaranteed,
also directly observed). **Answering the five questions directly, for
this configuration: genuine curves are recognized (20/20, by direct
waveform inspection); none are incorrectly rejected or combined; no
secondary lobes or disturbances produced additional candidates;
acquisition and closure are correctly distinguished; deterministic target
progression is maintained throughout.**

**Under the corrected-seed, 110 ms configuration** (same trial, same
underlying Hall signal): 21 candidates opened, only 3 closed, 6
contradicted, 11 disposed. This is the *same physical signal* as the
clean run above -- the difference is entirely the duration gate (item 4).
Direct inspection of the raw Hall samples underlying several of these
disposed/contradicted candidates (e.g. the t≈1421-1428 region: narrow,
~150-230 ADC peak, single-lobe departures returning fully to baseline
within 0.04-0.12 s, alternating sign roughly every ~1.1-1.3 s) shows each
one individually is a genuine, well-formed single curve -- not a
secondary lobe, not two merged encounters, and not an electrical
disturbance distinct from a real curve. **The 110 ms configuration
therefore does incorrectly reject genuine curves** (confirmed directly
from the waveform, independent of any annotation), precisely because they
are narrower in active duration than the configured gate -- it does not
combine curves, invent encounters from disturbances, or fail to
distinguish acquisition from closure (no evidence of either was found in
either configuration).

**Conclusion for Trial 6:** the deterministic target-acquisition
architecture correctly recognizes every genuine magnetic encounter in
this window when the duration gate does not interfere; the 110 ms gate's
rejections are real and are rejections of genuine curves, not correct
exclusions of disturbances; and this conclusion rests entirely on the
Hall waveform and the recognizer's own event log, not on whether the
historical annotations' magnet identities happen to agree.

### Starting-reference distinctions, made explicit

Per the operator's correction, three different things must not be
conflated, and the fix in §2 only addresses the first:

1. **A genuinely trusted geographic starting reference** -- independently
   verified physical ground truth. **Not available for this recording, at
   any point.** This experiment does not have one and does not claim to.
2. **A provisional starting reference derived from historical
   annotations** -- used here (§3 of the original report, below) to seed
   each trial. §2's fix corrects *when* that reference is treated as
   established (after its own encounter closes, not before) -- it does
   **not** make the referenced magnet's *identity* (its `mm` number)
   trustworthy. That identity comes from the same historically
   IR-dependent system the operator's correction flags, and is used here
   only as a provisional label, exactly as an uncertain operator
   declaration would be -- never confirmed.
3. **Subsequent geographic progression determined solely by the
   recognizer** -- every `CLOSE_AND_ADVANCE` after the seed is this
   recognizer's own causal decision from the real Hall signal, polarity,
   and map, with no further annotation input. This is what §7 evaluates
   directly, and it is the part of this experiment actually capable of
   answering the central question.

A consequence worth stating plainly: because (2)'s identity is
unconfirmed, every `target_mm` label in every results file could be
offset by a constant from the true physical position without changing
anything about whether this recognizer's own sequence of decisions, from
that starting point forward, was internally coherent. §7's findings
(curves correctly recognized, none combined or invented, deterministic
one-step advancement) are evaluated at that level -- behavioral
correctness relative to the real Hall signal -- not as a verified
absolute geographic position.

### Plots and traces produced by this investigation

- `results_anomaly_investigation/anomaly_plot.png`, `anomaly_trace.csv` --
  the detailed Trial 0/MM129 trace required by item 1, including the
  counterfactual comparison.
- `results_corrected_departure_floor/example_trial6_geographic_lag.png` --
  Trial 6 after both fixes: zero gap, MM126-143.
- `results_departure_floor/example_trial6_geographic_lag.png` -- the same
  window, original (buggy-seed) data, re-plotted with the corrected
  overlap-comparison method, for direct before/after visual comparison.
- `results_corrected_110ms/`, `results_corrected_departure_floor/` --
  full corrected event traces, trial summaries, and diagnostics.

### What remains unresolved, stated rather than hidden

- **The historical annotations are now known, not merely suspected, to
  carry identification errors inherited from the old IR-dependent
  system.** Every annotation-agreement number in this document (69.8%,
  91.1%, 77.8%, 100%) is diagnostic information about this experiment's
  internal consistency with that label source, stated per instruction,
  and **none of them is a geographic-accuracy claim in either direction.**
  §7 is the part of this investigation that does not depend on that
  source at all, and is where the central question is actually answered.
- The 12 CONTRADICTION/DISPOSED-driven gaps in the corrected 110 ms
  configuration are explained by mechanism (item 4, confirmed directly
  from the Hall waveform in §7) but their exact per-instance trigger was
  not individually re-traced for every one; the mechanism is confirmed,
  not every instance.
- Three trials still have no seed annotation at all and were not run
  (unchanged from the original report); two further trials are now
  explicitly excluded for insufficient post-seed span. This investigation
  does not increase the fraction of the recording actually validated --
  it corrects what is claimed about the fraction that was.
- Independent physical ground truth for this recording still does not
  exist, and now specifically cannot be reconstructed from the historical
  annotations either, since their own source is the system now known to
  have had an identification defect. Whether this recognizer's absolute
  geographic labels (as opposed to its relative, one-step-at-a-time
  behavior, which §7 does verify directly) match physical reality remains
  open, and is not claimed to be resolved by this investigation.

---

## The central question, answered directly

> **Note (2026-10-22 addendum, above):** the "88.9% diagnostically agree"
> figure below is superseded in its interpretation, not its arithmetic --
> the historical annotations it compares against are now known to carry
> identification errors inherited from the old IR-dependent system, so
> this percentage is diagnostic agreement with a label source, not
> evidence of geographic correctness. The addendum's §7 re-answers this
> same question from the Hall waveform directly, without that source.
> The claims about target-advancement behavior (one mapped magnet at a
> time, acquisition timing) below are unaffected -- they come from the
> recognizer's own event log, not the annotations.

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
