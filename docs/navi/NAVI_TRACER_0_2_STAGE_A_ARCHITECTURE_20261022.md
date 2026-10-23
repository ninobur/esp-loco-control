# NAVI_TRACER_0_2 — Stage A architectural specification (revision 2)

**Status: DRAFT, pending operator approval. Not a decision record. No firmware
changed. Nothing in this document authorizes Stage B or Stage C.**

Date: 2026-10-22. Scope: Stage A of the NAVI_TRACER_0_2 assignment (architectural
reconstruction and specification only).

**Revision 2 note:** this revision incorporates seven explicit operator
decisions that settle most of revision 1's open questions. Those decisions are
recorded as given, in §3–§6 below, not re-derived or second-guessed. Revision 1
is superseded; its reasoning is kept only where it still explains *why* a
decided mechanism works, not as a competing proposal.

## 1. Sources read for this document

- `docs/navi/NAVI_TRACER_0_1_ENGINEERING_FAILURE_20261022.md` (commit `e4831e7`)
- `docs/navi/NAVI_TRACER_0_1_ARCHITECTURAL_CONTINUITY_20261022.md` (commit `e4831e7`)
- `firmware/test-programs/NAVI_TRACER_0_1/NaviTracerCore.h`, `RvmTracing.h`
  (branch `feature/navi-tracer-0-1`, commits `fb8c3840`, `0d6657a`)
- `research/navi_target_acquisition_20261021/target_acquisition.py`, `REPORT.md`
  (commit `6efc9522`)
- `research/NAVI_CPR_0_1/REPORT.md` (commit `fca9e5b8`)
- `firmware/programs/NAVI_EYES_WIDE_OPEN/SpatialHallReference.h` (commit `7cc9e602`)
- Decisions 0023, 0093, 0106, 0107 (full text, at the commits that introduced
  or last revised each)
- `firmware/test-programs/NAVI_TRACER_0_1/RouteMap.h` (surveyed marker spacing)

For revision 2, also checked: the three committed MQTT/serial logs at the
repository root (`9950011_20261022_144255.log`, `_145334.log`, `_145712.log`)
— these cover Run 1 (14:42:55–15:01:16) only, not Run 2
(15:31:48–15:41:57, boot `B4F02DFBE4416E6C`), which is where the failing
MM045→MM044 event in `docs/NAVI_TRACER_0_1_FIELD_RUN_20261022.md` occurred.
The raw native-ADC extraction for Run 2 lives on the Pi and was not
accessible from this session — see §7.

Not yet read in full: `98db802` (normal-curve research) beyond what
`target_acquisition.py`'s own comments and docstring describe. I have not
replayed any recording for this document. Every quantitative number below is
quoted from the two mandatory failure reports or from the PWM-90 research's
own `REPORT.md`, not independently recomputed by me.

## 2. Operator decisions governing this revision

Given verbatim, as the architecture to specify (not as proposals I evaluated
and chose among):

1. Baseline collection begins at confirmed magnet closure and ends at the
   next confirmed magnetic opening (70 ADC departure for two consecutive
   observations).
2. No per-sample admission exclusion inside the collection window, time-based
   or otherwise. Revision 1's §4.3 proposal (a continuously-running,
   candidate-independent departure test gating which samples enter the
   median) is withdrawn, not replaced with anything else.
3. Collect baseline observations only while PWM > 0. When PWM = 0, suspend
   collection without resetting the accumulated observations, changing
   recognition state, or declaring closure.
4. The baseline ADC reference is the median of the accumulated observations.
   No fitted-line extrapolation, ever, as a reference value.
5. Line-versus-curve (ΔBIC) analysis is retained as its own recognition
   function, separate from baseline collection. Its mathematical
   implementation is to be evaluated on its own terms. It introduces no
   additional admission threshold.
6. The governing model stays: ordinary → magnetic excursion → ordinary.
7. Evaluate the median, using the actual MM045→MM044 recording, for whether
   the observations accumulated between closure and opening give a
   representative background reference.

No recognition timers, candidate timeouts, or new thresholds are added
anywhere below.

**On item 2's wording:** nothing in revision 1 proposed a "50 mm" exclusion
— revision 1 had no distance measurement to express a 50 mm quantity in at
all (NAVI_TRACER_0_2 is Hall-only). Read together with item 5's parallel
wording ("without introducing an additional admission threshold"), I take
"the proposed 50 mm exclusion" to mean revision 1's §4.3 departure-based
admission gate, which is the only admission-filtering mechanism revision 1
proposed. I am proceeding on that reading. If a literal 50 mm distance
exclusion was meant from some other source, say so and I will correct this
revision — I am not aware of one in anything I have read for this
assignment.

## 3. The question the operator asked to be answered first

> Can the recognizer identify the next magnetic excursion without allowing
> that excursion to corrupt its baseline reference?

This has to be answered with the actual MM044 numbers, because the honest
answer has a real limitation in it that a clean restatement of the assignment
would hide.

### 3.1 What actually happened at MM044

From the failure report: MM045 closed at t=169.61s. MM044 opened at t=170.37s
— **0.76 s later**. The surveyed spacing between adjacent markers on this
route is uniform, 280–360 mm (`RouteMap.h`); MM044/045 are not a special
closely-spaced pair. At the recorded speed, this particular inter-magnet
interval was simply short.

NAVI_TRACER_0_1's baseline window after MM045 closed had, by design, only the
6 samples from the closing candidate's own tail plus whatever arrived after —
it needed 8 samples spanning 650 ms before it would even attempt a fit. By
170.37s, under 0.76s had elapsed. The fit that got poisoned (slope
+213.80 ADC/s, level 2115.41 ADC against a true background near 1900 ADC) was
built almost entirely from samples that already had the next magnet's leading
field in them, because there was almost no genuinely clean interval available
in the window the code was willing to use.

### 3.2 Why "continuous accumulation over the whole inter-magnet interval"
    is a real fix in general, but not a complete one

The operator's proposed fix — accumulate the median continuously across the
*entire* interval since the last close, not a fixed 650 ms window — works by
a specific statistical mechanism: **the median has a 50% breakdown point.** A
median computed over N samples is only wrong if a majority of those N samples
are themselves contaminated by the oncoming field. If a typical inter-magnet
interval is, say, 3–8 s of travel and the field's detectable influence only
occupies the last 0.1–0.3 s of it (the PWM-90 corrected measurement: real
full-waveform width 0.10–0.20 s, median 0.12 s, n=148), the contaminated
fraction of a *long* window is small and the median is unaffected. This is a
real improvement over NAVI_TRACER_0_1's 8-sample window, where 4 contaminated
samples out of 8 is already a breakdown-level fraction.

**But MM044 is exactly the case where this general argument doesn't
automatically apply.** The available physical interval itself was only
0.76 s. If the leading field of MM044 was already detectably present for, say,
0.3–0.5 s of that 0.76 s (consistent with the slope seen: +213.80 ADC/s means
roughly 0.33 s to cover the 70-ADC departure threshold from a flat start), the
"contaminated" fraction of *this specific* interval could be 40–65% —
comparable to or exceeding the median's own breakdown point, not comfortably
below it. Continuous accumulation makes the general case much safer. It does
not, by itself, prove MM044 would resolve correctly, because this interval is
short enough that the asymptotic argument may not have had room to work.

By operator decision 2 (§2 above), no admission filter is used — every
sample collected between closure and opening enters the median unfiltered.
That means the median's own breakdown-point robustness is the *only*
defense, with no second layer. §7 below evaluates this directly against the
actual MM045→MM044 numbers, which is the operator's item 7 and the real
test of whether that single layer is enough for this specific, short
interval. This is also why MM152, MM031, and MM063 still need their own
replay in Stage B — each has its own available interval and its own
contamination fraction, and nothing here generalizes across them.

### 3.3 The harder problem: a slow leading flank looks locally like a line

The assignment asks for a fitted-line-vs-curved-model (ΔBIC) test to decide
whether a stretch of samples is "ordinary background" or "magnetic activity,"
and to use that as a recognition feature rather than diagnostic-only
instrumentation (Part IV).

There is a mathematical limitation here that the assignment's text doesn't
state, and I should not paper over it: **any smooth curve looks
arbitrarily close to a straight line over a short enough span or a small
enough amplitude.** The ΔBIC test in `target_acquisition.py`
(`delta_bic`, comparing a 2-parameter line against a 5-parameter line + one
Gaussian bump, fit over a window) needs to see the signal bend — ideally both
a rise and a point of inflection — before a Gaussian fits meaningfully better
than a line. The early part of a magnet's leading field, while it is still
small relative to the 70-ADC departure threshold, is close to monotonic and a
sloped line can fit it about as well as a line+bump can. MM044's own fitted
slope (+213.80 ADC/s) is a *line* — the code fit a line to it and the fit
was good (noise only 16.20 ADC). A ΔBIC test run on that same stretch of data
would plausibly have *also* called it "line-like," not "curve-like," for the
same reason the naive linear-regression baseline estimator did: at that
point in its development, it actually was close to linear.

This means ΔBIC, applied the way the assignment describes it (as a shape
classifier for a window of recent samples), is not guaranteed to catch the
developing flank *before* it has already biased a naive accumulator. It is
real, useful evidence once a bump is actually visible (the PWM-90 report's
own corrected-run ΔBIC values, min 24.79 to max 78.95, are all computed on
*closed* candidates with both shoulders visible) — but it is evidence about
a completed or well-developed excursion, not an early warning for a baseline
accumulator trying to decide, sample by sample, whether to admit the newest
reading.

**Under operator decision 2, this limitation is accepted rather than
engineered around.** No admission gate is added, ΔBIC or otherwise. The
median's own robustness (§3.2) and the closure-to-opening boundary (decision
1) are the whole defense. §7 evaluates directly whether that is enough for
the one case where we have real numbers.

## 4. The three functions, as decided

### A. Baseline identification
There is no separate "is this sample ordinary" admission test (decision 2).
Every sample collected while PWM > 0 (decision 3) within the current
closure→opening window (decision 1) is ordinary by definition of being in
that window — admission is structural (which window a sample falls in), not
evaluative (no per-sample test is run). Line-versus-curve (§5) is evidence
about the *shape of the whole candidate waveform* once a candidate is open,
not a filter on what enters the baseline accumulator.

### B. Baseline measurement
The median of every Hall sample collected while PWM > 0 since the last
confirmed closure, unfiltered (decision 4). Never a fitted line, never
extrapolated. PWM = 0 suspends collection — the accumulator is paused, not
reset, and no recognition state changes while suspended (decision 3,
consistent with decision 0106: PWM = 0 is a non-navigation interval, not an
event that itself alters a measurement in progress).

### C. Magnetic encounter recognition
Unchanged from the operator's explicit instructions (Part V of the original
assignment), evaluated against the *measured median* (§B) instead of a
frozen local-linear fit:
- Departure: ≥70 ADC for 2 consecutive 20 ms bins against the current median
  — same criterion, same two-bin persistence, different reference. This is
  also the event that ends baseline collection (decision 1) and opens the
  candidate.
- Acquisition: active duration ≥ the duration-floor (0.02 s) — unchanged,
  not restoring 110 ms.
- RTB/closure: return to a Hall-evidence-based "ordinary again" state,
  evaluated against the *measured* median rather than a frozen extrapolated
  reference — see §6. Closure is also the event that starts the next
  baseline-collection window (decision 1).

## 5. Line-versus-curve, as its own function (decision 5)

Line-versus-curve (ΔBIC: a 2-parameter line against a 5-parameter line + one
Gaussian bump, fit over the candidate's own waveform) stays a separate
recognition function from baseline collection, per decision 5 — it does not
gate what enters the median, and introduces no new admission threshold.

What it evaluates, precisely: whether a window of *already-collected*
samples (a candidate's own open-to-close span, same as the PWM-90 research's
usage) is better explained by a flat/sloped line or by a line with one
embedded bump. This is diagnostic/confirmatory shape evidence about a
waveform that has already been observed — not a prediction, and not a
reference value any future sample is compared against. That distinction
(characterizing observed shape vs. extrapolating a future reference) is the
one the original assignment's Part IV draws between permitted and prohibited
uses of a fitted line, and ΔBIC as specified here stays on the permitted
side.

Revision 1 raised a limitation worth restating here since it bears on how
much weight this function can carry: a magnet's early, low-amplitude
leading field is close to linear, so ΔBIC run on a window that only contains
that early portion will often not yet favor the Gaussian alternative — the
PWM-90 report's own ΔBIC values (min 24.79, median 48.29, max 78.95) are all
computed on *closed* candidates with both shoulders visible. Per decision 5,
this is not a reason to fold ΔBIC into baseline admission; it is a reason
its mathematical evaluation (decision 5's instruction) should report, for
each candidate, how early in that candidate's own open span ΔBIC first
crosses whatever threshold is used, not just its value at close. That is an
evaluation task for Stage B, not a change proposed here.

## 6. Completion (RTB) evidence, proposed starting point

Per Part V §9, Decision 0107 lists candidate RTB evidence: agreement with
baseline, ordinary-track spread/noise, disappearance of systematic trend,
persistence, subsequent waveform structure. Concretely, against the
continuously-measured median:
- Hall level within the admitted samples' own spread of the current median
  (replaces the frozen-reference distance test).
- Persistence: the existing 6-consecutive-bin requirement (120 ms) — I see
  no evidence yet that this number itself is wrong; the MM044 failure was
  about *what it was being compared against*, not the persistence count. I
  am not proposing to change it, consistent with Part V §9's own framing
  ("may be retained if justified by the recorded evidence... not
  automatically governing").
- No candidate timeout, per explicit instruction — none proposed.

This is a starting point for Stage B, not a final claim; Part V §9 asks for
this to be investigated against real data, which I have not done yet.

## 7. Item 7 — evaluating the median against the actual MM045→MM044 recording

**What I could and could not access.** The native 5 kHz Hall ADC recording
for the failing run (Run 2, 15:31:48–15:41:57, boot `B4F02DFBE4416E6C`, the
run containing the MM045→MM044 failure) lives on the Pi
(`/home/david/NGR/navi_sync/`) and was extracted and replayed by Codex to
produce the committed numbers in
`docs/navi/NAVI_TRACER_0_1_ENGINEERING_FAILURE_20261022.md`. That raw
extraction is not committed to this repository and I do not have Pi access
in this session. The three MQTT/serial log files committed at the
repository root (`9950011_20261022_144255.log`, `_145334.log`, `_145712.log`)
cover 14:42:55–15:01:16 — Run 1 — not Run 2; they do not contain the failing
MM045→MM044 event. I have not replayed raw samples myself. Everything below
is derived from the numbers the engineering failure report already
committed, not from re-running the recording.

**What the committed numbers say.** MM045 closed at replay time t=169.61 s;
MM044 opened at t=170.37 s — an available closure→opening window of 0.76 s.
NAVI_TRACER_0_1's fit over the last 0.65 s of that window (8 samples) found
slope +213.80 ADC/s, fitted level 2115.41 ADC at t=170.37s, residual noise
16.20 ADC, against an ordinary background the report independently states
as "approximately 1900 ADC." A linear rise of +213.80 ADC/s over 0.65 s
accounts for about +139 ADC — consistent with the window spanning roughly
1976 ADC (0.65 s before opening) up to 2115 ADC (at opening), which is in
the right neighborhood of the quoted ~1900 ADC background given these are
independently rounded figures from two different passages of the report.

**Applying the decided design (items 1–4) to this same interval:** the
window is now the *entire* 0.76 s (not just the last 0.65 s the old 8-sample
rule needed), with every sample unfiltered. But the fitted-line evidence
already shows the signal rising essentially the whole way from background
to the 2115 ADC opening value across that span — there is no indication in
the committed numbers of a materially longer flat stretch earlier in the
0.76 s that the line fit simply didn't need to use. If that is right, the
rising portion is not a small minority of this window; it plausibly
dominates it. A median of a predominantly-rising sequence sits near its own
midpoint value, not near the window's start (true background) — so **the
median over this specific 0.76 s window should be read as biased toward the
elevated end, not as a clean recovery of the ~1900 ADC background.**

**What is nonetheless better than the frozen-line failure, and why it
matters:** the median is bounded within the range of samples actually
observed in the window — it cannot sit below the lowest sample or above the
highest, and it cannot flip sign the way `baselineLevel_ + baselineSlope_ *
(t - t0)` did when a steep positive slope was extrapolated forward past the
window into negative apparent departures. A departure test run against a
biased-but-bounded median would most plausibly produce a *reduced effective
margin* on the next real departure (because the reference itself sits closer
to "elevated" than true background), not the observed MM044 failure mode of
a wrong-signed, unrecoverable reference that no genuine return-to-baseline
sample could ever satisfy. I cannot state from the numbers available
whether the reduced margin would have been enough to still open MM044
correctly and let it close — that requires the actual sample-by-sample
sequence, which I do not have.

**Conclusion for this item:** the closure→opening window, as decided, is
very likely too short at this specific marker pair and this recorded speed
for *any* central-tendency statistic — median included — to recover a
clean background value; the physical interval itself does not contain
enough uncontaminated signal. The median is a real improvement over the
frozen line (bounded, no catastrophic extrapolation, no wrong-signed
reference), but "representative of ordinary background" is the wrong bar
to judge it against for this one case — it is not representative, by the
numbers already on record. Whether the resulting bounded, biased reference
is still good enough for the departure/RTB logic to recognize MM044
correctly is a question only the actual sample sequence can answer, and
that is Stage B's replay, not something resolvable from the committed
summary numbers alone.

## 8. What Stage A does not include

- No replay of the raw recording — see §7's access note. Every number in
  this document is quoted from the two mandatory failure reports or the
  PWM-90 `REPORT.md`, not recomputed by me.
- No waveform-level analysis of MM031, MM152, or MM063 — the failure report
  explicitly says these need their own traces, which is Stage B work.
- No new admission threshold, timer, or candidate timeout — none proposed,
  per decision 2 and the operator's closing instruction.
- No firmware change of any kind.

## 9. Still open, after this revision

- **§7's residual risk** — whether the median, as decided, is actually
  sufficient at MM044 (and at MM152/MM031/MM063) — is not resolved by this
  document and cannot be from the summary numbers alone. It needs the Stage
  B replay against real sample sequences, and if Pi access is available,
  that is where the raw `otto_adc` extraction for Run 2 should come from.
- **Item 2's reading** — confirm or correct my interpretation of "the
  proposed 50 mm exclusion" (§2 above) before this is treated as settled.

Everything else revision 1 raised as an open question (collection boundary,
stationary/PWM=0 handling, ΔBIC's role) is resolved by the operator's seven
decisions in §2 and reflected in §4–§6 above.

I have not proceeded to Stage B. No firmware has been modified.
