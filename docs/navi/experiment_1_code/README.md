# Experiment 1 & 2 — causal rolling line/curve evaluator, code

Offline research code only, not firmware. Same code serves both
experiments; see
`../NAVI_TRACER_0_2_EXPERIMENT_1_LINE_CURVE_EVALUATOR_20261023.md` and
`../NAVI_TRACER_0_2_EXPERIMENT_2_WINDOW_DURATION_20261023.md` for the
writeups and findings.

## Files

- `extract_run2_full_csv.py` — read-only extraction of a bounded time
  window of native Hall samples from a Pi NSR1 capture (requires
  `navi_sync_format.py` from `CURRENT/tools/` or the Pi's own copy on
  `sys.path`). Binary-searches by `recv_us` (Pi wall-clock microseconds)
  to locate the window, then streams sequentially, writing CSV
  (`received_unix_us,serial,sample_t_us,adc,pwm,direction`), gzip'd.
  Usage: `extract_run2_full_csv.py <nsr_path> <loco_id> <start_us> <end_us> <out.csv.gz>`.
- `evaluator_experiment.py` — the evaluator itself: bins native samples
  into 20ms blocks exactly per `RvmTracing.h`'s rule, runs a causal,
  PWM-gated, ΔBIC-based rolling line/curve classifier with a configurable
  window and persistence (consecutive-agreement) requirement, and reports
  the resulting excursion segmentation plus each intervening segment's
  median. Usage: `evaluator_experiment.py <label> <csv> <out.json> [window_blocks] [threshold] [persistence]`.
- `plot_evaluator.py` — renders the trace with the evaluator's CURVE-state
  classification shaded, for independent visual review. Usage:
  `plot_evaluator.py <result.json> <result_trace.npz> <t_lo_us> <t_hi_us> <out.png> [title]`.

## Data provenance (not committed — regenerate from the Pi if needed)

- Fresh CW/PWM~90 run: `navi_sync_20261022_163743.nsr`, loco 9950011,
  device-time window found via wall clock 2026-10-22 22:49:45–22:51:00
  PDT.
- Oct 22 CCW/PWM~35-60 run (Run 2, the MM045 investigation):
  `navi_sync_20261015_100403.nsr`, loco 9950011, wall clock
  2026-10-22 15:31:30–15:42:10 PDT.

Both on the Pi at `/home/david/NGR/navi_sync/`. Extraction is read-only;
nothing on the Pi is modified by this code.

## History

Experiment 1: persistence (consecutive-agreement smoothing) was added
mid-experiment, after the unpersisted version flickered on real data,
rather than being decided jointly beforehand as the agreed process called
for — disclosed in that writeup's closing section. Its detection-latency
measurement (`independent_true_onset` / `detection_latency_ms`) had a
real bug (not just noise): it scanned past the detected onset and kept
overwriting its answer with the *most recent* qualifying quiet run,
finding the post-excursion recovery instead of the pre-excursion
background for short excursions. Fixed for Experiment 2 — see the
function's current docstring for the fix and why the old results were
wrong, not just unreliable.

Experiment 2: `persistence=1` (no smoothing) used throughout, isolating
`window_blocks` as the only variable, per Sam and David's request.
