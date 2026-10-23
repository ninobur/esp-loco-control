# NAVI_TRACER_0_2 — Experiment 1: causal rolling line/curve evaluator

**Status: offline research prototype only. Not firmware. No TARGET/
NOT_THE_TARGET decision implemented. No thresholds authorized as final —
every number below is exploratory, chosen to get a first result, not
proposed as a design value.** Per Sam's framing: this experiment answers
one question — can a continuously operating, causal line/curve evaluator
reliably distinguish magnetic excursions from intervening background in
actual Hall recordings?

## Data

Two real recordings, both pulled read-only from the Pi's native Hall
capture (`navi_sync_20261022_163743.nsr` and `navi_sync_20261015_100403.nsr`),
binned exactly per `RvmTracing.h`'s own rule (20ms blocks, mean of native
reads, NaN on <50 reads or a serial-jump-touched block):

- **Fresh CW/PWM~90 run** (2026-10-22, 22:49:53 PDT) — 370,848 native
  samples, 74.2s. Ground truth known exactly from telemetry: 22 clean,
  consecutive magnets, MM034→MM056, zero contradictions.
- **Oct 22 CCW/PWM~35-60 run** (Run 2, 15:31:48–15:41:57 PDT, the one
  containing the MM045 investigation) — 3,160,752 native samples, 632.1s
  (the full session, not the narrow slice used for the earlier MM045
  report).

## Method

A single rolling evaluator: at every new 20ms block (PWM>0 only; PWM=0
suspends without resetting state), compute ΔBIC (line vs. line+Gaussian,
the same method already in this project's research history) over a
trailing window of the most recent blocks. State is LINE or CURVE. A
transition requires the test to agree for N consecutive blocks
(persistence) before it's accepted — added after the first, unpersisted
pass flickered badly (see below). On LINE→CURVE, the just-ended LINE
segment's median is finalized. On CURVE→LINE, the excursion's peak
(max |departure| from that median) and duration are recorded.

Exploratory parameters used for this pass: window = 15 blocks (300ms),
ΔBIC threshold = 10 (the same loose historical reference noted before, not
re-derived), persistence = 4 consecutive agreements (80ms). These were
hand-tuned against the PWM-90 case only, then applied unchanged to the
PWM-60 case — deliberately, to see how a single fixed tuning behaves
across the range, not to flatter either result.

## Result 1 — the naive version flickers, exactly as Sam predicted

Before adding persistence, a single-sample threshold crossing immediately
flipped state. Against the real PWM-90 recording this produced 38–51
"excursions" for 22 real magnets — not because the data is noisy, but
because a window-based test keeps showing high ΔBIC for as long as a real
excursion sits anywhere inside the trailing window, and near a genuine
peak that signal can cross back and forth over a fixed threshold many
times in succession. This is not a coding bug — it was checked directly
against the raw samples. It's the risk Sam named explicitly before this
experiment ran: "I would also avoid assuming that every brief change in
the evaluator's classification constitutes a physical transition." Adding
a persistence requirement (same consecutive-agreement idea already used
elsewhere in this project) removed it for the PWM-90 case.

## Result 2 — PWM~90: exact match, clean segmentation

With persistence added, the evaluator finds **exactly 22 excursions**,
matching the known telemetry count with no extras and no misses. Peak
departures 160–189 ADC, durations 0.74–1.18s, each intervening LINE
segment's median landing in a tight 1954–1957 ADC band. See
[fresh_pwm90_plot.png] — 25 seconds of the trace, red shading is the
evaluator's CURVE state, each magnet cleanly bracketed with nothing
left over.

## Result 3 — PWM~35-60: the MM045 problem is gone, but MM044 fragments

This is the important one. Run against the full Oct 22 session with the
*same, unchanged* tuning from the PWM-90 case:

- **The phantom-MM045 problem does not reproduce.** The evaluator finds
  one continuous LINE segment spanning device t_us 165,448,549 to
  169,168,549 — 3.7 seconds, 186 samples — between the real MM046
  excursion's recovery and the real MM044 excursion's onset. No spurious
  excursion is declared in between. Its median: **1903.645 ADC** —
  matching the manually-computed independent background (1903.66) from
  the earlier investigation to within 0.02 ADC. This is the first time
  this project has produced a clean, representative median for this
  specific gap, using a mechanism that doesn't depend on declared
  closure/opening events at all.
- **MM044's own excursion gets fragmented into 2–3 overlapping pieces**
  under this tuning, not one continuous excursion. The ΔBIC trace dips
  below threshold briefly somewhere inside MM044's real rise, triggers a
  premature LINE declaration, then immediately re-triggers CURVE as the
  real excursion continues. This is visible directly in
  [mm044_region_plot.png] as overlapping shaded bands right where MM044's
  single real bump is. This is exactly the risk Sam and I discussed
  conceptually — a short portion of a real magnetic curve can look
  approximately linear — now confirmed on the actual recording it was
  worried about.

## What this does and doesn't establish

**Established:** the basic mechanism — causal, windowed, ΔBIC-based
segmentation with persistence — works well on the easy case and directly
resolves the specific failure (phantom MM045) this whole effort grew out
of, without reintroducing it or any new admission threshold.

**Not established:** that a single fixed window/persistence tuning works
across the whole PWM range. It visibly doesn't yet, on real data, for
MM044 specifically — the one real excursion that matters most for this
project's history.

**Explicitly not done:** no attempt to tune away the MM044 fragmentation
in this pass. That felt like the point to stop and bring this back,
rather than keep adjusting numbers until one more case looked good.

**Known weak spot in the experiment itself, not the evaluator:** the
detection-latency measurement I built (comparing the evaluator's onset
against an independent simple estimate) is producing suspiciously uniform
values (~-980ms) across almost every excursion, which looks like a bug in
that comparison code rather than a real result. I'm not reporting latency
numbers as trustworthy this round — flagging it rather than presenting a
number I don't believe.

## Candidate next question

The short-window/long-window separation discussed under Principles 3–4
(a quick early test to confirm closure, a slower/different test to end
baseline collection) was not implemented in this first pass — this used
one test for both jobs. MM044's fragmentation may be exactly the case
that separation was meant to prevent. That seems like the natural next
experiment, but I'm stopping here for review before deciding that myself.

[fresh_pwm90_plot.png]: fresh_pwm90_plot.png
[mm044_region_plot.png]: mm044_region_plot.png
