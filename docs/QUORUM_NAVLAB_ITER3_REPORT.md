# navlab iteration 3 — corrected record

Date: 2026-08-22 (corrected the same day, operator directive).
**Overall: FAIL — 2 PASS, 1 FAIL, 6 NOT_DEMONSTRATED.**
Machine verdict: `tools/navlab/results/iter3_acceptance_corrected.json`.
Supporting classifications: `tools/navlab/results/iter3_evidence_classification.json`,
`tools/navlab/results/iter3_probe_dt0_unknown_time.json`.

This is an evidence and reporting correction. No navigator logic was changed,
no threshold was tuned, no firmware was touched, and no train run was
requested. The candidate is frozen as it stands.

## What this correction withdraws

The first iteration-3 report scored **8 PASS / 0 FAIL / 1 NOT_DEMONSTRATED**.
That score does not survive its own evidence:

| Claim in the first iteration-3 report | Status now |
|---|---|
| "C7 PASS — no false confirmation detected + stop-honest" | **withdrawn.** Direction consistency detects contradictions and proves no absolute position. 1 of 559 confirmations is independently validated; **0 of 10 incident outcomes** are. C7 = NOT_DEMONSTRATED. |
| "51 physical-signature ghosts exercised, none advanced" | **withdrawn.** The Otto boot1 replay stopped after **85 of 4,726 events**; the navigator saw **2** of those 51 ghosts. The other 49 were never processed. C5 = NOT_DEMONSTRATED. |
| "C8 PASS — expectations derive from records" | **withdrawn.** Deriving the windows proves reproducible *selection*, not that each rejected event was genuine. No expectation has independent support. C8 = NOT_DEMONSTRATED. |
| "dt=0 handling is fail-safe" | **refuted by counterexample.** See `docs/NAVLAB_DT0_SEMANTICS.md` review. C1 = FAIL. |
| "C4/C6 PASS — frozen runs eliminated" | **downgraded.** The runs did advance, 48/48 events across four windows. Whether they advanced *correctly* rests on heuristic labels. NOT_DEMONSTRATED. |

The one claim that stands unchanged: the permitted wording is **"zero false
confirmations detected"**, never "zero false confirmations".

Iteration 2, rescored under this same corrected checker, is **FAIL — 2 PASS, 3
FAIL, 4 NOT_DEMONSTRATED** (previously reported as 4/2/3, and before that as
"7 of 9"). Its report carries the second correction.

## Corrected scoreboard

| | Condition | Verdict | Evidence basis |
|---|---|---|---|
| C1 | never outside the physically reachable set | **FAIL** | counterexample (development probe) |
| C2 | never behind the last confirmation without a reversal | PASS | internal consistency only |
| C3 | no route-wide search before a full circuit is reachable | PASS | internal consistency only |
| C4 | known genuine acceleration remains genuine | NOT_DEMONSTRATED | development regression, heuristic labels |
| C5 | known phantoms do not advance confirmed position | NOT_DEMONSTRATED | 2 ghosts actually processed |
| C6 | frozen runs of genuine rejections disappear | NOT_DEMONSTRATED | development regression, heuristic labels |
| C7 | known incidents recover or stop without false relocation | NOT_DEMONSTRATED | no independently validated incident outcome |
| C8 | every expectation carries event-level justification | NOT_DEMONSTRATED | heuristic + firmware-derived only |
| C9 | results hold on data excluded from envelope generation | NOT_DEMONSTRATED | no untouched data exists |

C2 and C3 pass as **internal safety properties**: the navigator's claims are
consistent with its own corridor and its route-wide search is correctly gated.
That is worth having and it is not evidence about the railway.

## 1. C1 fails on a counterexample (dt=0 review)

A dt-chain reset means traversal time is **unknown**. It does not establish
that one interval elapsed. The one-interval grant is therefore an
*under*-approximation of reachability whenever the unknown time covered more
than one interval — it can exclude the true position, which is the opposite of
what a corridor is for.

`tools/navlab/probe_dt0_unknown_time.py` sweeps all 171 start positions on the
real committed map. With no reset (control) the navigator is correct 171/171.
At true gaps of 0 and 1 interval — the rule's assumed domain — it is correct
and never confirms wrongly. Beyond that domain most cases stop, but **7 to 25
of 171 confirm a marker the locomotive does not occupy**, off by 1 to 11
markers. Failure is not always a stop; sometimes it is a confident wrong
position.

Missed markers are not hypothetical here: Otto boot16 contains ten inter-event
gaps over 20 s, and 21 dt-chain resets. The safe representation is a bounded
hypothesis expansion with **confirmation authority suspended** until the
position is re-established by uniqueness, or an explicit unknown-position
state. Both are recorded in the semantics document as candidates for iteration
4. **Neither is implemented** — the rule was not tuned to preserve the Otto
replay, which is exactly what tuning it would have done.

The seven dt=0 synthetic tests are relabeled **development tests, not
validation**: they pin the implementation to the written rule, and they assume
the same thing the rule assumes.

## 2. The eight route-wide recoveries (Otto boot16)

| # | motion state | to LOST | lost for | reacquired | firmware cross-check | corridor ÷ evidenced travel | independent support |
|---|---|---|---|---|---|---|---|
| 1 | dwell-interrupted | 131 s / 40 ev | 16 s | mm39 | fw mm39 agrees | 4.3× | none |
| 2 | dwell-interrupted | 78 s / 37 ev | 19 s | mm155 | fw mm155 agrees | 4.6× | none |
| 3 | dwell-interrupted | 127 s / 38 ev | 22 s | mm44 | **fw mm38 disagrees** | 4.5× | none |
| 4 | dwell-interrupted | 79 s / 36 ev | 19 s | mm154 | fw mm154 agrees | 4.8× | none |
| 5 | dwell-interrupted | 131 s / 42 ev | 20 s | mm39 | **fw mm41 disagrees** | 4.1× | none |
| 6 | continuous, PWM 99 | 36 s / 36 ev | 12 s | mm92 | fw mm92 agrees | 4.8× | none |
| 7 | continuous, PWM 99 | 36 s / 38 ev | 12 s | mm143 | fw mm143 agrees | 4.5× | none |
| 8 | continuous, PWM 99 | 36 s / 35 ev | 13 s | mm20 | fw mm20 agrees | 4.9× | none |

**Why the corridor reached a full circuit.** The corridor only resets on
confirmation. When confirmation stops, it grows monotonically at the envelope's
fast-bound speed until it exceeds the 52,150 mm circuit — and that speed is far
above anything the locomotive does:

| PWM bucket | n | fastest admitted sample | median | corridor speed | median speed | ratio | ghost/dwell signature in the fastest 20 |
|---|---|---|---|---|---|---|---|
| 40 | 533 | 1729 ms | 3140 ms | 0.208 mm/ms | 0.097 | 2.1× | 14 |
| 60 | 4,796 | 1192 ms | 2313 ms | 0.301 mm/ms | 0.132 | 2.3× | 20 |
| **90** | 13,400 | **246 ms** | 1246 ms | **1.459 mm/ms** | 0.245 | **6.0×** | **20** |
| 100 | 139 | 801 ms | 992 ms | 0.449 mm/ms | 0.307 | 1.5× | 2 |

`fast_bound = min × (1 − margin)`, so a **single** contaminated sample sets the
speed at which uncertainty grows. The admission filter rejects slow, dwell,
stationary and PWM-uncovered samples — and nothing at the fast end. In the
PWM-90 bucket every one of the twenty fastest samples carries a ghost
signature (peak 41–44) or a dwell signature (duration 1.0–4.1 s); the
uncontaminated PWM-100 bucket sits at 1.5×. Episodes 6–8 are the visible
consequence: at constant full speed with a clean one-per-second marker stream,
the model reached total positional uncertainty in 36 seconds.

**Was route-wide uncertainty physically justified?** Formally yes, operationally
no. The corridor is an upper bound, so the true position was inside it — but in
all eight episodes the locomotive is evidenced to have covered at most about a
fifth of what the model admitted. Episodes 1–5 at least follow real station
dwells with 22–35 s gaps; episodes 6–8 have no such excuse.

**Does the reacquired position have independent support?** **None of the eight.**
The nearest operator declaration to episodes 6 and 7 is 53 and 104 marker
events away, which anchors nothing. Six of eight agree with the firmware label
— a firmware-derived cross-check, from the firmware whose navigation is the
subject of the study, and in the two disagreements the firmware was itself in a
degraded verdict state (RAMP, LOW_PWM). No episode is independently validated.

**Necessary or merely permitted?** Permitted. In every episode the marker
stream was continuous (35–42 events) and the locomotive never left the modelled
corridor. Correct gating is not the same as a desirable outcome: a navigator
that declares total positional uncertainty three times in one session while
receiving a marker every second is not fit to run a railway, however sound its
bookkeeping.

## 3. The Otto boot1 contradiction

Classified. The replay stopped 177 s in, at event 85 of 4,726.

- **Triggering event** (line 1848): firmware label mm82, polarity S against a
  map pole of N, **peak 44, duration 42 ms**, dt 303 ms at PWM 75, firmware
  verdict *quarantined*. It arrived 83 ms after a genuine event at the same
  label (peak 161, duration 255 ms, dt 1198 ms). It carries the ghost signature
  exactly: genuine markers measure ≥104 peak and ≥116 ms.
- **Position before the contradiction: not independently supported.** The stop
  came at the end of a five-confirmation chain seeded by a route-wide
  re-acquisition at mm52. No operator declaration touches the lineage.
- **Cause: hypothesis handling / phantom recognition.** Not sensing — the
  firmware quarantined the same event. Not envelope coverage — the timing test
  was never reached. Not dt semantics — dt was 303 ms. The navigator has **no
  amplitude or duration criterion at all**. Its only phantom test is positional
  (`hi + 30 < next spacing`), so a weak, short, wrong-pole event is recognised
  as a phantom only when the corridor happens to be narrower than one interval.
  The corridor was 418 mm, wider than the ~305 mm interval, so the test did not
  fire; no S-pole candidate existed inside the corridor; the run stopped.
- **Was stopping necessary?** Safe, but not necessary. Given what the model
  uses, it had no basis to place the event and stopping was the correct
  fail-safe. Physically, the event announces itself as a ghost, and a
  signature-aware navigator had grounds to hold it as a suspect. Recorded as a
  finding; **not implemented**.
- **Did the 51 ghosts exercise C5?** No. Two of them occurred before the stop.
  The claim of 51 was inflated 25-fold by counting events the navigator never
  processed. C5 is NOT_DEMONSTRATED on two anecdotes.

## 4. C8 expectation evidence

Four frozen-run expectations exist: 13, 16 and 14 firmware-rejected events at
labels mm130, mm52 and mm126 in Otto boot16, and 5 at mm101 in Toby boot2. For
each, the expectation is *"these events were genuine marker crossings and must
advance the navigator position"*. Committed evidence, per event, in
`iter3_evidence_classification.json`:

- **Measurement:** peak 134–263, duration 201–377 ms, dt 2.2–3.2 s at constant
  PWM (43–44 for Otto, 60–62 for Toby). 48 of 48 events meet the genuine
  signature.
- **The strongest single argument** is polarity alternation: 5, 7, 7 and 2 pole
  flips *within runs the firmware labels as one marker*. A stationary re-read of
  one magnet cannot alternate pole, so the locomotive was moving past distinct
  markers.
- **Classification basis:** the peak/duration thresholds are heuristics derived
  from this same corpus; the mm label and the "rejected" verdict come from the
  firmware under study; **independent position truth: none**, in any window.
- **Which marker was crossed is not established** — and neither C4 nor C6 tests
  it, so "the frozen runs disappeared" means the position advanced, not that it
  advanced to the right place.

That is real event-level evidence for the physical interpretation and it is not
independent support. C8 = NOT_DEMONSTRATED, and C4 and C6 inherit it.

## 5. What the model has actually achieved

**Demonstrated internal safety properties.** Forward-only motion with no
backward claim absent a reversal; route-wide search correctly gated behind a
full-circuit corridor and a unique 12-window with three-fold confirmation;
contradictions stop the run instead of relocating; pending events are held, not
discarded; zero externally seeded positions in any strict replay.

**Successful development-log regressions.** All four frozen runs advanced
(48/48 events). Otto's cascade day runs end to end with 219 confirmations and
zero contradictions, including 14 motor-direction reversals tracked natively —
notably one where marker-by-marker backward tracking continued through a dt
reset. These are regressions against logs used to build the navigator; they
show the mechanisms work, not that the answers are right.

**Direction-consistent but position-unvalidated.** 559 confirmations across
three replays. **1** is independently anchored. 558 are consistent with powered
direction and otherwise unchecked. **0 of 10** incident outcomes — eight
re-acquisitions, two stops — have any independent support.

**Assumptions still awaiting physical validation.** The dt=0 one-interval grant
(counterexample recorded). The envelope fast bounds, which set the corridor's
growth rate and are contaminated at the fast end. The absence of any amplitude
criterion in the navigator. The genuine/ghost signature thresholds themselves.

**Conditions requiring a future untouched recording.** C9 in full; C7 at any
scale; the correctness half of C4, C6 and C8.

## 6. The evaluation protocol this record defines

The candidate is frozen at commit-of-record. To evaluate it honestly, a future
ordinary session must produce a capture that is:

1. **Untouched** — kept out of normalization, envelope building, replay and
   inspection until the evaluation run, then replayed exactly once.
2. **Anchored** — operator declarations at known markers, with the locomotive
   near-stationary and no marker events between declaration and check, so each
   one anchors a confirmation. Roughly a dozen scattered through the session
   converts C7 from anecdote to measurement. This is a note for whenever the
   railway next runs; **no train run is requested here.**
3. **Ghost-bearing** — at least one slow or low-PWM stretch of the sort that
   produces phantom events, so C5 has a real population.

Before that data exists, the honest position is that navlab has a candidate
with sound internal bookkeeping, one demonstrated design counterexample, and
essentially no independent evidence about position.

## Standing constraints

T remains rejected. O remains archived. Otto remains on rollback commit
`6d35bb7`. No firmware implementation is proposed, and no firmware was modified
or flashed in this work.
