# NAVI_POWER_OF_NORMAL_MANUAL_ONLY: investigation of the onset shortfall

2026-10-24 · Claude. Investigates David's question of the same date: whether
the 924-vs-1,087 onset shortfall reported for `NAVI_POWER_OF_NORMAL_MANUAL_ONLY`
represents genuine missed magnetic encounters or a counting artifact.
Read-only analysis against the recorded boot-1 corpus
(`~/NGR/NGR-Files/hall-ground-20261015/derived/native_15549e9385faa4d5.bin`,
`analysis/navi_rvm_shape_20261018/derived/annotation_boot1.csv`, 1,087
annotated OPENING events). No firmware behavior changed except the one
correction described below.

## Headline finding

**One mechanism accounts for essentially the whole shortfall.** It is not a
counting artifact and not primarily an amplitude/threshold problem. It is a
demonstrated implementation defect in how `NaviCandidate.h` and
`NaviEncounter.h` interact: once a candidate opens, the whole detector is
globally gated closed to any further onset until that ONE candidate is
decided, and its return-to-baseline test compares against a reference level
frozen at the instant of onset and never updated for the life of that
candidate (up to the 10 s abandon timeout). The real Hall baseline on this
railway drifts on the order of 100+ ADC counts over the course of several
consecutive magnets — confirmed directly in the raw trace, not inferred — so
that frozen reference is often never satisfied again. The candidate times
out, abandons, and because nothing else can be detected while any candidate
is open, every real magnet underneath the 10-second window is lost. A new
candidate then opens immediately off the same stale, frozen level and
frequently repeats the same failure, chaining into multi-minute blackouts.

## Corrected run (after the one fix made — see below)

| | count |
|---|---|
| Annotated OPENING events | 1,087 |
| Matched (an onset within ±100 ms of the annotated extent) | 548 |
| Missed | 539 |
| — missed, inside an abandoned encounter's span | 524 (97%) |
| — missed, isolated (not explained by that mechanism) | 15 (3%) |
| Abandoned (never-returned-to-baseline) encounters, whole recording | 184 |
| Total time inside an abandoned-encounter window | 1,840 s of 3,843 s (48%) |

Reproduce: `tests/navi_power_of_normal/run_checks.sh`, then
`python3 tests/navi_power_of_normal/investigation_20261024/match.py` and
`classify_misses.py` against the replay's `onsets.csv`/`encounters.csv`.

## Answering the four questions, for the dominant class (524 of 539 misses)

1. **Did a genuine magnetic encounter occur?** Yes. Directly inspected the
   raw native trace (`inspect_window.py`) across a representative lockup
   episode (t=673–697 s, MM130–149, 20 consecutive annotated magnets all
   missed in one run): the signal is real, continuous, physically plausible
   track structure — not noise — rising and falling smoothly over tens of
   seconds, consistent with a train traversing a sequence of real magnets.
2. **Did the signal satisfy the 70-count, two-consecutive-sample
   requirement?** The FIRST magnet in each locked episode did — that is what
   opened the candidate in the first place, confirmed in `onsets.csv`. Every
   subsequent magnet in the same episode is simply never evaluated at all:
   detection is globally off while any candidate is open, so whether their
   individual signals would have satisfied the rule is moot — the detector
   never looked.
3. **Did baseline tracking, sampling, or detector state prevent
   recognition?** Yes — decisively, and the dominant answer: detector STATE.
   The single global "one candidate open at a time" gate, combined with a
   reference that is frozen at onset and never re-estimated, is the
   mechanism. This is not what the old recognizer did: `RvmShape.h` re-
   anchors its local baseline at every accepted ensemble specifically to
   track this kind of drift. This build's `NaviEncounter.h` does not.
4. **Are the annotation and the detector measuring equivalent events?** Yes,
   for this class — the annotated openings are real passages the detector
   genuinely cannot see because it isn't looking, not because they differ in
   kind from what it's built to recognize.

## The 15 isolated misses (not explained by the lockup)

Two representative cases were inspected directly in the raw trace (one
`bar`-type at MM66, t≈132 s; pattern checked for a `disk`-type cluster too,
which accounts for several of the 15 — `disk`-type openings are
disproportionately represented among the isolated misses, though the sample
is too small to call that conclusive here). In the inspected case, the raw
ADC trace does not exceed roughly 15–20 counts above its local baseline
anywhere in or near the annotated window — well under the 70-count floor.
**No detector built around that floor, old or new, could recognize this
one.** This is corroborated independently: the old recognizer's own
navigation-replay harness (`analysis/navi_rvm_shape_20261018/results/local/
nav_harness_summary.json`) separately flags the same region (truth_mm 64–72,
t_s≈125–139 in that boot) as a contradiction/miss in its own right — this
looks like a genuinely weak passage in the recorded data, not a defect
specific to this implementation. The remaining isolated misses were not
individually inspected; they are a small fraction (3% of the shortfall) and
this is flagged as unfinished rather than assumed closed.

## The 1 kHz reconciliation: implemented, and reported honestly

Per instruction, `NaviCandidate.h` now evaluates the two-consecutive-sample
test at the established rule's own cadence (`docs/NAVI_RVM_70_COUNT_GATE_
20261020.md`: "The NAVI_SIMPLIFIED form of the rule used two consecutive
1 kHz samples") rather than on every raw 5 kHz read — a five-times stride
decimation (`CANDIDATE_STRIDE`), with the EMA time constant re-expressed so
the real-time behavior is unchanged. This is a genuine, demonstrable
reconciliation of the implementation to the governing specification, not a
new threshold.

**Measured in isolation, it performs worse on this corpus**, not better:
onsets 924 → 694, matched annotations 834 → 548. This was tested, not
assumed. It is reported as-is rather than reverted, because reverting it to
get a better-looking number would be choosing an implementation that doesn't
match the established rule for the sake of the percentage — exactly what was
asked not to do. But the comparison is confounded by the dominant lockup
mechanism above: once any candidate opens, both versions are equally blind
to everything else regardless of how good the raw onset test is, so this
measurement mostly reflects when the FIRST candidate of each lockup episode
happens to open, not the quality of 1 kHz vs. 5 kHz detection generally. **It
is not safe to conclude anything about 1 kHz vs. 5 kHz performance until the
lockup is fixed and the comparison is redone without that confound** — this
is flagged as an open question for the next pass, not resolved here.

## What was NOT done, per instruction

No new thresholds, classifiers, or failure-handling mechanisms were added to
improve the detection percentage. `RTB_MARGIN_ADC`, `RTB_RUN_BLOCKS`, and
`MAX_ENCOUNTER_BLOCKS` (`NaviEncounter.h`) are unchanged from their original,
admittedly-arbitrary values — the lockup finding above shows those values
interact badly with real baseline drift, but changing them would be
retuning a number to chase a percentage, not correcting a demonstrated
coding defect, and the instruction was explicit on that distinction. The
actual fix — giving `NaviEncounter` a baseline that can legitimately move
during a long encounter, or allowing detection of a new candidate without
fully resolving the prior one first — is an architectural choice about how
"observed return to baseline" should be implemented when real magnets are
closely spaced, not a parameter tweak. It is reported here for decision, not
made unilaterally.

## Bottom line

The 15% apparent shortfall significantly understated the real problem once
measured properly (48% of the recording, not 15%, ends up inside this
mechanism once onset detection is made more conservative). It is one
well-evidenced, well-understood cause, not 500+ independent failures. Fixing
it is an architecture question, not a threshold question, and per David's
instruction this build does not proceed to arrival-boundary calibration or
AUTO authorization until that is resolved.
