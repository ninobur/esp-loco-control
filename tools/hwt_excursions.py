#!/usr/bin/env python3
"""hwt_excursions.py — offline Hall-excursion measurement tool for
HALL_WAVEFORM_TEST captures.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

This is a MEASUREMENT instrument, not a decision-maker. It finds every
candidate excursion in channel A's raw waveform and reports what the
waveform actually did — width, flux, timing, motor context, nearby
acquisition holes. It never says an excursion is a genuine magnet response,
a phantom, or anything else. It never reads the old Module C annotation
bits (ann_ch0 in the decoded CSV) at all, because those are exactly the
threshold-rule verdicts this instrument exists to let a person judge
independently. Anchors are the only operator ground truth carried through.

    python3 tools/hwt_decode.py run.hwt -o run.csv
    python3 tools/hwt_excursions.py run.csv -o run_excursions.csv \\
        --summary run_summary.txt \\
        --plot-width-vs-flux run_width_flux.png \\
        --plot-peak-vs-width run_peak_width.png \\
        --plot-categories-dir run_plots

Or hand it the raw capture directly (it decodes internally, on the fly):

    python3 tools/hwt_excursions.py run.hwt -o run_excursions.csv

================================================================================
ALGORITHM — deliberately simple; this section is the whole of it
================================================================================

1. BASELINE — two selectable, independent modes (--baseline-mode)

   "frozen" (default). The corrected method. For every sample NOT currently
   inside an excursion, a live baseline is maintained as a trailing
   statistic (--baseline-method: mean or median) of the last --pre-window
   samples that were themselves never part of ANY excursion ("quiet"
   history — this window never contains an excursion sample, however far
   back it has to reach to find enough quiet ones). The instant a sample's
   deviation from that live baseline crosses --entry-threshold, the
   baseline value AT THAT MOMENT is FROZEN and reused, completely
   unchanged, for every sample of the excursion — opening, peak, closing,
   all of it. No excursion sample, however large or however long the
   excursion runs, ever feeds back into its own baseline. This is what
   "cannot absorb the candidate excursion" means literally: absorption
   requires the baseline to keep looking at the event while it is scored
   against it, and this baseline stops looking the moment the event opens.

   "moving" (the original method, kept selectable for direct comparison).
   A CENTERED rolling statistic (mean or median) over a window of
   --baseline-window samples, straddling every sample including the ones
   inside an excursion. This tracks slow drift well, but a broad or slow
   excursion pulls its own local window toward itself while it is still
   being measured against that same window — the wider or slower the
   excursion relative to the window, the more of its own amplitude the
   baseline silently absorbs. See the module-level comparison feature
   below, which measures this directly rather than asserting it.

2. DEVIATION
   dev[i] = raw[i] - baseline[i], for every sample that exists. No sample
   is ever invented to fill a hole; the deviation series simply has no
   entry where the sample does not exist, and the next real sample picks
   up wherever it is timestamped, however far that is from the last one.

3. COLLECTOR (--entry-threshold, --exit-threshold) — a two-threshold
   ("Schmitt trigger") state machine, run once, forward, over the samples
   that exist:
     - a sample OPENS an excursion the first moment |dev| >= entry;
     - once open, the excursion stays open for as long as |dev| stays at
       or above the lower exit threshold (hysteresis, so a signal sitting
       right at one cutoff is not chopped into dozens of one-sample
       excursions);
     - it closes at the last sample with |dev| >= exit.
   This is PERMISSIVE ON PURPOSE: no minimum width, no minimum peak, no
   shape test, no polarity requirement, nothing that decides whether an
   excursion is a magnet, a motor-commutation pulse, or a wiring artefact.
   A single-sample spike is kept and reported exactly like a 300-sample
   passage. Filtering candidates down to what matters is the analyst's job,
   done afterward, on the CSV this tool writes — not this collector's job.

4. PER-EXCURSION MEASUREMENT (no interpolation anywhere)
   Every quantity below is computed only from samples that were actually
   acquired, using each sample's own measured inter-sample interval
   (dt_us from the decoded record, not an assumed 1 kHz grid):
     - integrated flux, in count*milliseconds, is reported BOTH signed
       (sum(dev[i]*dt_ms[i]) — positive and negative lobes cancel) and
       absolute (sum(|dev[i]|*dt_ms[i])) — a rectangle rule using the REAL
       measured interval before each sample, not a fixed step;
     - rise time is peak time minus opening time; fall time is closing
       time minus peak time;
     - PWM and direction are read at the peak sample, with a flag raised
       if either one changed anywhere across the excursion.

5. INCOMPLETE EXCURSIONS
   Every GAP / MISSED / DROP / SESSION row the decoder produced is a
   named, timestamped hole in the physical record. If one of those holes
   falls inside an excursion's own time span, or the excursion runs off
   the end of a session without ever dropping back below the exit
   threshold, the excursion is marked incomplete=1. It is still reported
   — narrow AND broad, complete AND incomplete, all of it — but it is
   EXCLUDED from the principal statistics in the summary report (reported
   in its own separate block instead), because pooling unreliable
   measurements into a headline number would misrepresent them as trusted.

6. LOW-PWM DWELL WINDOWS (--dwell-pwm-max, --dwell-min-ms) — a
   telemetry-only proxy, NOT a stall detector
   Every maximal run of consecutive samples with ctl_pwm_actual at or
   below --dwell-pwm-max, lasting at least --dwell-min-ms, becomes one
   "dwell window". This is a literal read of the recorded PWM column —
   nothing more. It does NOT distinguish a scheduled stop, an operator's
   manual assistance on a stall, or the ordinary start/end of the
   recording itself; all three look identical in this signal. Excursions
   overlapping a dwell window are flagged in_low_pwm_dwell=1 and, like
   incomplete excursions, excluded from the principal statistics and
   reported separately. This tool never invents exact stall boundaries
   from anchors or logs that do not state them — it only ever reports
   what the PWM column itself says.

7. BASELINE COMPARISON (--compare-baselines)
   For every excursion found under the primary --baseline-mode, the exact
   same sample range is ALSO measured against the OTHER mode's baseline
   array (moving <-> frozen), without changing what counts as "the
   excursion". This isolates what the baseline choice alone changes on an
   identical physical event, reported as alt_max_abs_flux and
   alt_integrated_abs_flux_count_ms columns plus a comparison block in the
   summary.
================================================================================
"""

import argparse
import bisect
import csv
import os
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_format as F   # noqa: E402
import hwt_decode as D   # noqa: E402

BREAK_TYPES = ("GAP", "MISSED", "DROP", "SESSION")

EXCURSION_COLUMNS = [
    "excursion_id", "session",
    "start_sample", "end_sample", "n_samples", "nominal_span_samples",
    "start_time_s", "end_time_s", "duration_ms",
    "baseline_mode", "baseline_method", "baseline_n_quiet", "baseline_at_excursion",
    "max_pos_flux", "max_neg_flux", "max_abs_flux", "t_max_abs_flux_s",
    "integrated_abs_flux_count_ms", "integrated_signed_flux_count_ms",
    "rise_time_ms", "fall_time_ms",
    "pwm_actual_at_peak", "pwm_commanded_at_peak", "pwm_changed",
    "dir_at_peak", "dir_changed",
    "gaps_within_count", "gaps_near_count", "incomplete",
    "in_low_pwm_dwell",
    "anchor_before_id", "anchor_before_text", "anchor_before_dt_s",
    "anchor_after_id", "anchor_after_text", "anchor_after_dt_s",
    "alt_baseline_mode", "alt_max_abs_flux", "alt_integrated_abs_flux_count_ms",
]


# ------------------------------------------------------------------------
# Loading — accept either a raw .hwt capture or an already-decoded CSV.
# ------------------------------------------------------------------------
def load_rows(path):
    if path.lower().endswith(".hwt"):
        rows, _report = D.decode(path)
        return rows
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def split_sessions(rows):
    """Group decoded rows by session, preserving arrival order within each.
    Returns an ordered dict-like list of (session_id, rows_for_session)."""
    sessions = {}
    order = []
    for r in rows:
        sid = r["session"] or "?"
        if r["row_type"] == "SESSION":
            continue   # boundary marker only; does not belong to either side
        if sid not in sessions:
            sessions[sid] = []
            order.append(sid)
        sessions[sid].append(r)
    return [(sid, sessions[sid]) for sid in order]


def build_series(session_rows):
    """From one session's decoded rows, extract:
      samples  - ordered list of dicts (seq, t_s, raw, dt_ms, pwm_actual,
                 pwm_commanded, dir)
      breaks   - ordered list of (kind, t_s_at_break, info); t_s_at_break is
                 the timestamp of the last real sample before the hole, same
                 convention hwt_plot.py uses to anchor a hole in time
      anchors  - ordered list of (t_s, anchor_id, text)
    Channel A only — the installed sensor; this tool does not look at a
    second channel even when one is present in the file."""
    samples, breaks, anchors = [], [], []
    for r in session_rows:
        t = r["row_type"]
        if t == "SAMPLE":
            if r["phys_ch0_raw"] == "":
                continue          # nothing acquired for this slot; not our concern here
            samples.append({
                "seq": int(r["phys_sample_seq"]),
                "t_s": float(r["phys_t_s"]),
                "raw": int(r["phys_ch0_raw"]),
                "dt_ms": (int(r["phys_dt_us"]) / 1000.0) if r["phys_dt_us"] != "" else 1.0,
                "pwm_actual": int(r["ctl_pwm_actual"]) if r["ctl_pwm_actual"] != "" else 0,
                "pwm_commanded": int(r["ctl_pwm_commanded"]) if r["ctl_pwm_commanded"] != "" else 0,
                "dir": r["ctl_dir"] or "?",
            })
        elif t == "ANCHOR":
            if r["phys_t_s"] != "":
                anchors.append((float(r["phys_t_s"]), r["op_anchor_id"], r["op_text"]))
        elif t in BREAK_TYPES:
            last_t = samples[-1]["t_s"] if samples else None
            breaks.append((t, last_t, r["info"]))
    return samples, breaks, anchors


# ------------------------------------------------------------------------
# Baseline — "moving" (centered, original) mode
# ------------------------------------------------------------------------
def rolling_baseline(values, window, method):
    """Centered rolling statistic over `values` (a plain list of numbers).
    window is the FULL window width in samples; half goes each side of the
    center sample, clipped at the ends of the list (no wraparound, no
    reflection, no fabricated padding). Used by --baseline-mode moving:
    every sample, including ones inside an excursion, contributes to its
    own local baseline."""
    n = len(values)
    if n == 0:
        return []
    half = max(1, window // 2)

    if method == "mean":
        prefix = [0.0] * (n + 1)
        for i, v in enumerate(values):
            prefix[i + 1] = prefix[i] + v
        out = [0.0] * n
        for i in range(n):
            lo, hi = max(0, i - half), min(n, i + half + 1)
            out[i] = (prefix[hi] - prefix[lo]) / (hi - lo)
        return out

    if method == "median":
        # A sliding sorted window, maintained with bisect.insort/del as the
        # window advances one sample at a time. Simple, exact (no binning),
        # and — because the window only ever grows or slides by one element
        # per step — cheap enough for offline use even on hour-long captures.
        out = [0.0] * n
        win = []
        cur_lo, cur_hi = 0, -1
        for i in range(n):
            want_lo, want_hi = max(0, i - half), min(n - 1, i + half)
            while cur_lo < want_lo:
                del win[bisect.bisect_left(win, values[cur_lo])]
                cur_lo += 1
            while cur_hi < want_hi:
                cur_hi += 1
                bisect.insort(win, values[cur_hi])
            m = len(win)
            out[i] = win[m // 2] if m % 2 else (win[m // 2 - 1] + win[m // 2]) / 2.0
        return out

    raise ValueError("unknown baseline method %r" % method)


def collect_excursions(samples, baseline, entry_threshold, exit_threshold):
    """The permissive two-threshold collector, run against an already-
    computed baseline array (used for --baseline-mode moving). Returns a
    list of dicts with sample-INDEX ranges only (idxs, forced_end);
    measurement happens separately in measure_excursion()."""
    out = []
    in_exc = False
    idxs = None
    for i, s in enumerate(samples):
        dev = s["raw"] - baseline[i]
        adev = abs(dev)
        if not in_exc:
            if adev >= entry_threshold:
                in_exc = True
                idxs = [i]
        else:
            if adev >= exit_threshold:
                idxs.append(i)
            else:
                out.append({"idxs": idxs, "forced_end": False, "baseline_n": None})
                in_exc = False
                idxs = None
    if in_exc:
        out.append({"idxs": idxs, "forced_end": True, "baseline_n": None})
    return out


# ------------------------------------------------------------------------
# Baseline — "frozen" (pre-event, corrected) mode
# ------------------------------------------------------------------------
def frozen_baseline_collect(samples, pre_window, method, entry_threshold, exit_threshold):
    """Detection AND baseline computation combined into one causal forward
    pass, because in this mode the two cannot be separated: what the
    baseline IS depends on where excursions open, and where they open
    depends on the baseline.

    A trailing window (deque, maxlen=pre_window) accumulates the raw values
    of "quiet" samples only — samples that were never part of any
    excursion, open or closed. Before a sample is tested for opening a new
    excursion, the live statistic (mean or median, per `method`) of that
    window is used as its baseline. If |raw - live_baseline| crosses
    entry_threshold, that live_baseline value is FROZEN into `frozen` and
    used, completely unchanged, for every subsequent sample of the
    excursion (open test uses entry_threshold; staying open uses the lower
    exit_threshold; both against the SAME frozen number). Excursion
    samples are never pushed into the quiet window, so they can never
    influence any baseline, including their own.

    When an excursion closes, the closing sample (the first one to fall
    below exit_threshold) is re-evaluated in the SAME iteration against a
    freshly recomputed live baseline — it may itself immediately open a
    new excursion (a sharp reversal), or it may simply be quiet and join
    the trailing window. Either way no sample is skipped or double-counted.

    Returns (baseline_array, excursion_list) in the same shape
    rolling_baseline()+collect_excursions() produce for the moving mode,
    so downstream measurement code (measure_excursion) is identical for
    both baseline modes. Each excursion dict additionally carries
    "baseline_n": how many quiet samples actually fed the frozen value
    (<= pre_window; fewer if excursions are close together and quiet
    history is short)."""
    n = len(samples)
    baseline = [0.0] * n
    quiet = deque(maxlen=pre_window)
    sorted_quiet = []
    running_sum = 0.0

    def live_stat():
        if not quiet:
            return None
        if method == "mean":
            return running_sum / len(quiet)
        m = len(sorted_quiet)
        return (sorted_quiet[m // 2] if m % 2
               else (sorted_quiet[m // 2 - 1] + sorted_quiet[m // 2]) / 2.0)

    def push_quiet(v):
        nonlocal running_sum
        if quiet.maxlen and len(quiet) == quiet.maxlen:
            old = quiet[0]
            running_sum -= old
            if method == "median":
                del sorted_quiet[bisect.bisect_left(sorted_quiet, old)]
        quiet.append(v)
        running_sum += v
        if method == "median":
            bisect.insort(sorted_quiet, v)

    excursions = []
    in_exc = False
    idxs = None
    frozen = None
    frozen_n = 0

    i = 0
    while i < n:
        raw = samples[i]["raw"]
        if in_exc:
            dev = raw - frozen
            if abs(dev) >= exit_threshold:
                baseline[i] = frozen
                idxs.append(i)
                i += 1
                continue
            excursions.append({"idxs": idxs, "forced_end": False, "baseline_n": frozen_n})
            in_exc = False
            idxs = None
            # fall through: re-evaluate sample i fresh, against a live baseline

        b = live_stat()
        if b is None:
            # No quiet history yet (start of session): a deviation cannot
            # be judged, so this sample cannot open an excursion.
            baseline[i] = raw
            push_quiet(raw)
            i += 1
            continue
        dev = raw - b
        if abs(dev) >= entry_threshold:
            in_exc = True
            frozen = b
            frozen_n = len(quiet)
            idxs = [i]
            baseline[i] = frozen
        else:
            baseline[i] = b
            push_quiet(raw)
        i += 1

    if in_exc:
        excursions.append({"idxs": idxs, "forced_end": True, "baseline_n": frozen_n})
    return baseline, excursions


def compare_against_baseline(samples, idxs, baseline_alt):
    """Re-measure peak and integrated |flux| for an already-collected
    excursion (idxs into `samples`) against a DIFFERENT baseline array,
    holding the excursion's sample range fixed. Used only to compare
    baseline methods on the identical physical event — never to redefine
    what counts as an excursion."""
    devs = [samples[i]["raw"] - baseline_alt[i] for i in idxs]
    peak = max(abs(d) for d in devs)
    integrated = sum(abs(d) * samples[i]["dt_ms"] for d, i in zip(devs, idxs))
    return peak, integrated


# ------------------------------------------------------------------------
# Low-PWM dwell windows — telemetry-only proxy, see module docstring §6.
# ------------------------------------------------------------------------
def detect_low_pwm_dwells(samples, pwm_threshold, min_duration_ms):
    windows = []
    lo_i = None
    for i, s in enumerate(samples):
        if s["pwm_actual"] <= pwm_threshold:
            if lo_i is None:
                lo_i = i
        else:
            if lo_i is not None:
                if (samples[i - 1]["t_s"] - samples[lo_i]["t_s"]) * 1000.0 >= min_duration_ms:
                    windows.append((samples[lo_i]["t_s"], samples[i - 1]["t_s"]))
                lo_i = None
    if lo_i is not None:
        if (samples[-1]["t_s"] - samples[lo_i]["t_s"]) * 1000.0 >= min_duration_ms:
            windows.append((samples[lo_i]["t_s"], samples[-1]["t_s"]))
    return windows


# ------------------------------------------------------------------------
# Per-excursion measurement
# ------------------------------------------------------------------------
def nearest_anchor(anchors, t, *, before):
    """Nearest anchor at/before t (before=True) or at/after t (before=False).
    Returns (anchor_id, text, dt_s) or ("", "", "") if none exists."""
    best = None
    for at, aid, text in anchors:
        if before and at <= t:
            if best is None or at > best[0]:
                best = (at, aid, text)
        elif not before and at >= t:
            if best is None or at < best[0]:
                best = (at, aid, text)
    if best is None:
        return "", "", ""
    at, aid, text = best
    return aid, text, "%.6f" % (t - at if before else at - t)


def measure_excursion(eid, sid, samples, baseline, idxs, forced_end,
                       breaks, anchors, gap_margin_s, dwell_windows, *,
                       baseline_mode, baseline_method, baseline_n_quiet=None):
    start_i, end_i = idxs[0], idxs[-1]
    s0, s1 = samples[start_i], samples[end_i]

    devs = [samples[i]["raw"] - baseline[i] for i in idxs]
    peak_local = max(range(len(idxs)), key=lambda k: abs(devs[k]))
    peak_i = idxs[peak_local]
    peak_dev = devs[peak_local]
    peak_s = samples[peak_i]

    max_pos = max([0.0] + [d for d in devs if d > 0])
    max_neg = min([0.0] + [d for d in devs if d < 0])
    integrated_abs = sum(abs(d) * samples[i]["dt_ms"] for d, i in zip(devs, idxs))
    integrated_signed = sum(d * samples[i]["dt_ms"] for d, i in zip(devs, idxs))

    pwm_changed = any(samples[i]["pwm_actual"] != s0["pwm_actual"] or
                      samples[i]["pwm_commanded"] != s0["pwm_commanded"]
                      for i in idxs)
    dir_changed = any(samples[i]["dir"] != s0["dir"] for i in idxs)

    t_lo, t_hi = s0["t_s"], s1["t_s"]
    within = [b for b in breaks if b[1] is not None and t_lo <= b[1] <= t_hi]
    near = [b for b in breaks if b[1] is not None
           and (t_lo - gap_margin_s) <= b[1] <= (t_hi + gap_margin_s)
           and b not in within]
    incomplete = bool(within) or forced_end or start_i == 0

    in_dwell = any(lo <= t_hi and hi >= t_lo for lo, hi in dwell_windows)

    ab_id, ab_text, ab_dt = nearest_anchor(anchors, t_lo, before=True)
    aa_id, aa_text, aa_dt = nearest_anchor(anchors, t_hi, before=False)

    return {
        "excursion_id": eid, "session": sid,
        "start_sample": s0["seq"], "end_sample": s1["seq"],
        "n_samples": len(idxs),
        "nominal_span_samples": s1["seq"] - s0["seq"] + 1,
        "start_time_s": "%.6f" % t_lo, "end_time_s": "%.6f" % t_hi,
        "duration_ms": "%.3f" % ((t_hi - t_lo) * 1000.0),
        "baseline_mode": baseline_mode, "baseline_method": baseline_method,
        "baseline_n_quiet": baseline_n_quiet if baseline_n_quiet is not None else "",
        "baseline_at_excursion": "%.2f" % baseline[start_i],
        "max_pos_flux": "%.2f" % max_pos, "max_neg_flux": "%.2f" % max_neg,
        "max_abs_flux": "%.2f" % abs(peak_dev),
        "t_max_abs_flux_s": "%.6f" % peak_s["t_s"],
        "integrated_abs_flux_count_ms": "%.3f" % integrated_abs,
        "integrated_signed_flux_count_ms": "%.3f" % integrated_signed,
        "rise_time_ms": "%.3f" % ((peak_s["t_s"] - t_lo) * 1000.0),
        "fall_time_ms": "%.3f" % ((t_hi - peak_s["t_s"]) * 1000.0),
        "pwm_actual_at_peak": peak_s["pwm_actual"],
        "pwm_commanded_at_peak": peak_s["pwm_commanded"],
        "pwm_changed": int(pwm_changed),
        "dir_at_peak": peak_s["dir"], "dir_changed": int(dir_changed),
        "gaps_within_count": len(within), "gaps_near_count": len(near),
        "incomplete": int(incomplete),
        "in_low_pwm_dwell": int(in_dwell),
        "anchor_before_id": ab_id, "anchor_before_text": ab_text,
        "anchor_before_dt_s": ab_dt,
        "anchor_after_id": aa_id, "anchor_after_text": aa_text,
        "anchor_after_dt_s": aa_dt,
        "alt_baseline_mode": "", "alt_max_abs_flux": "",
        "alt_integrated_abs_flux_count_ms": "",
    }


def analyze_captures(path, *, entry_threshold, exit_threshold,
                     baseline_mode="frozen", baseline_window=501,
                     baseline_method="mean", pre_window=200,
                     gap_margin_s=0.05, dwell_pwm_max=5.0, dwell_min_ms=1000.0,
                     compare_baselines=False):
    """Core orchestrator. Returns {"excursions": [...], "sessions": {sid:
    {"samples", "baseline", "breaks", "anchors", "dwell_windows"}}} — the
    per-session context is exposed so callers (the CLI's detail plotter)
    can redraw exactly the data and baseline the measurements came from,
    without recomputing anything differently."""
    rows = load_rows(path)
    excursions = []
    sessions_ctx = {}
    eid = 0
    for sid, srows in split_sessions(rows):
        samples, breaks, anchors = build_series(srows)
        if not samples:
            continue
        raw = [s["raw"] for s in samples]

        if baseline_mode == "moving":
            baseline = rolling_baseline(raw, baseline_window, baseline_method)
            coll = collect_excursions(samples, baseline, entry_threshold, exit_threshold)
        elif baseline_mode == "frozen":
            baseline, coll = frozen_baseline_collect(
                samples, pre_window, baseline_method, entry_threshold, exit_threshold)
        else:
            raise ValueError("unknown baseline_mode %r" % baseline_mode)

        alt_mode, alt_baseline = None, None
        if compare_baselines:
            if baseline_mode == "moving":
                alt_mode = "frozen"
                alt_baseline, _ = frozen_baseline_collect(
                    samples, pre_window, baseline_method, entry_threshold, exit_threshold)
            else:
                alt_mode = "moving"
                alt_baseline = rolling_baseline(raw, baseline_window, baseline_method)

        dwell_windows = detect_low_pwm_dwells(samples, dwell_pwm_max, dwell_min_ms)

        for c in coll:
            eid += 1
            row = measure_excursion(
                eid, sid, samples, baseline, c["idxs"], c["forced_end"],
                breaks, anchors, gap_margin_s, dwell_windows,
                baseline_mode=baseline_mode, baseline_method=baseline_method,
                baseline_n_quiet=c.get("baseline_n"))
            if compare_baselines:
                alt_peak, alt_integ = compare_against_baseline(samples, c["idxs"], alt_baseline)
                row["alt_baseline_mode"] = alt_mode
                row["alt_max_abs_flux"] = "%.2f" % alt_peak
                row["alt_integrated_abs_flux_count_ms"] = "%.3f" % alt_integ
            excursions.append(row)

        sessions_ctx[sid] = {"samples": samples, "baseline": baseline,
                             "breaks": breaks, "anchors": anchors,
                             "dwell_windows": dwell_windows}
    return {"excursions": excursions, "sessions": sessions_ctx}


def find_excursions(path, **kwargs):
    """Thin wrapper for callers that only need the excursion list (tests,
    simple scripts) — see analyze_captures() for full per-session context."""
    return analyze_captures(path, **kwargs)["excursions"]


# ------------------------------------------------------------------------
# Summary report
# ------------------------------------------------------------------------
def _stats(vals):
    if not vals:
        return {"n": 0, "min": 0, "median": 0, "mean": 0, "max": 0}
    vals = sorted(vals)
    n = len(vals)
    mid = vals[n // 2] if n % 2 else (vals[n // 2 - 1] + vals[n // 2]) / 2.0
    return {"n": n, "min": vals[0], "median": mid,
           "mean": sum(vals) / n, "max": vals[-1]}


def write_summary(excursions, args, fh):
    p = lambda s: print(s, file=fh)
    p("HALL_WAVEFORM_TEST excursion summary (investigatory)")
    p("  source                   %s" % args.capture)
    p("  entry / exit threshold   %g / %g counts" % (args.entry_threshold, args.exit_threshold))
    if args.baseline_mode == "moving":
        p("  baseline                 moving %s over %d samples (centered) [comparison mode]"
          % (args.baseline_method, args.baseline_window))
    else:
        p("  baseline                 frozen-pre-event %s over up to %d quiet samples [corrected mode]"
          % (args.baseline_method, args.pre_window))
    p("  gap margin               %.3f s" % args.gap_margin_s)
    p("  dwell detector            PWM <= %g sustained >= %.0f ms (telemetry-only proxy, see NOTE below)"
      % (args.dwell_pwm_max, args.dwell_min_ms))
    p("  excursions found         %d" % len(excursions))
    if not excursions:
        return

    incomplete = [e for e in excursions if e["incomplete"]]
    dwell = [e for e in excursions if e["in_low_pwm_dwell"] and not e["incomplete"]]
    principal = [e for e in excursions if not e["incomplete"] and not e["in_low_pwm_dwell"]]

    p("  incomplete                %d (%.1f%%) -- EXCLUDED from principal stats, reported separately below"
      % (len(incomplete), 100.0 * len(incomplete) / len(excursions)))
    p("  in low-PWM dwell window   %d (%.1f%%) -- EXCLUDED from principal stats, reported separately below"
      % (len(dwell), 100.0 * len(dwell) / len(excursions)))
    p("  principal (clean) set     %d (%.1f%%)"
      % (len(principal), 100.0 * len(principal) / len(excursions)))

    def block(label, group):
        p("")
        p("  --- %s (n=%d) ---" % (label, len(group)))
        if not group:
            return
        dur = _stats([float(e["duration_ms"]) for e in group])
        flux = _stats([float(e["integrated_abs_flux_count_ms"]) for e in group])
        peak = _stats([float(e["max_abs_flux"]) for e in group])
        p("  duration_ms      n=%d  min=%.2f  median=%.2f  mean=%.2f  max=%.2f"
          % (dur["n"], dur["min"], dur["median"], dur["mean"], dur["max"]))
        p("  integrated_flux  n=%d  min=%.2f  median=%.2f  mean=%.2f  max=%.2f"
          % (flux["n"], flux["min"], flux["median"], flux["mean"], flux["max"]))
        p("  max_abs_flux     n=%d  min=%.2f  median=%.2f  mean=%.2f  max=%.2f"
          % (peak["n"], peak["min"], peak["median"], peak["mean"], peak["max"]))

    block("PRINCIPAL statistics (complete, outside any low-PWM dwell window)", principal)
    block("incomplete excursions (reported separately, NOT pooled above)", incomplete)
    block("low-PWM-dwell-window excursions (reported separately, NOT pooled above)", dwell)

    p("")
    p("  NOTE on the dwell detector: this counts periods where recorded PWM")
    p("  telemetry alone stayed at or below %g for >= %.0f ms. It is a literal"
      % (args.dwell_pwm_max, args.dwell_min_ms))
    p("  read of the capture's own ctl_pwm_actual column, NOT a confirmed")
    p("  stall or manual-assistance period -- this capture's anchors do not")
    p("  establish exact stall boundaries, and none are invented here. A")
    p("  commanded stop, a scheduled dwell, and a genuine stall are")
    p("  indistinguishable in PWM telemetry alone.")

    p("")
    p("  NOTE: this is a descriptive quantile split, not a classification.")
    p("  Splitting at the median duration only shows how the PRINCIPAL")
    p("  population divides on ONE axis; whether that split means anything")
    p("  physically is a judgement for whoever reads the plots, informed by")
    p("  anchors.")
    durs_p = sorted(float(e["duration_ms"]) for e in principal)
    if durs_p:
        n = len(durs_p)
        med = durs_p[n // 2] if n % 2 else (durs_p[n // 2 - 1] + durs_p[n // 2]) / 2.0
        if med > 0:
            narrow = [e for e in principal if float(e["duration_ms"]) <= med]
            broad = [e for e in principal if float(e["duration_ms"]) > med]
            nf = _stats([float(e["integrated_abs_flux_count_ms"]) for e in narrow])
            bf = _stats([float(e["integrated_abs_flux_count_ms"]) for e in broad])
            p("  at/below median duration (n=%d): integrated flux median=%.2f"
              % (nf["n"], nf["median"]))
            p("  above median duration    (n=%d): integrated flux median=%.2f"
              % (bf["n"], bf["median"]))

    pwms = {e["pwm_actual_at_peak"] for e in principal}
    if len(pwms) > 1:
        bin_w = 10
        by_bin = {}
        for e in principal:
            b = (e["pwm_actual_at_peak"] // bin_w) * bin_w
            by_bin.setdefault(b, []).append(float(e["duration_ms"]))
        p("")
        p("  duration_ms by PWM actual at peak, binned by %d, PRINCIPAL set only "
          "(descriptive only):" % bin_w)
        for b in sorted(by_bin):
            st = _stats(by_bin[b])
            p("    pwm %3d-%-3d  n=%-5d median=%-8.2f mean=%-8.2f max=%-8.2f"
              % (b, b + bin_w - 1, st["n"], st["median"], st["mean"], st["max"]))

    if args.compare_baselines:
        p("")
        p("  --- baseline comparison: %s (primary) vs the alternate mode ---" % args.baseline_mode)
        p("  Same excursion, same sample range, two different baseline")
        p("  yardsticks -- isolates what the baseline choice alone changes.")
        alt_rows = [e for e in principal if e["alt_max_abs_flux"] != ""]
        buckets = (("narrow (<%.1fms)" % args.narrow_max_ms,
                   [e for e in alt_rows if float(e["duration_ms"]) < args.narrow_max_ms]),
                  ("broad (>=%.1fms)" % args.broad_min_ms,
                   [e for e in alt_rows if float(e["duration_ms"]) >= args.broad_min_ms]))
        for label, group in buckets:
            if not group:
                p("  %s  n=0" % label)
                continue
            pct_peak = [100.0 * (float(e["alt_max_abs_flux"]) - float(e["max_abs_flux"]))
                       / float(e["max_abs_flux"]) for e in group if float(e["max_abs_flux"]) > 0]
            pct_flux = [100.0 * (float(e["alt_integrated_abs_flux_count_ms"]) - float(e["integrated_abs_flux_count_ms"]))
                       / float(e["integrated_abs_flux_count_ms"]) for e in group
                       if float(e["integrated_abs_flux_count_ms"]) > 0]
            sp, sf = _stats(pct_peak), _stats(pct_flux)
            p("  %s  n=%d" % (label, len(group)))
            p("      alt-vs-primary peak %%              median=%7.1f%%  mean=%7.1f%%  min=%7.1f%%  max=%7.1f%%"
              % (sp["median"], sp["mean"], sp["min"], sp["max"]))
            p("      alt-vs-primary integrated-flux %%    median=%7.1f%%  mean=%7.1f%%  min=%7.1f%%  max=%7.1f%%"
              % (sf["median"], sf["mean"], sf["min"], sf["max"]))

    p("")
    p("  CAUTION: any separation of broad responses from narrow spikes seen")
    p("  in the plots is NOT evidence of repeatability for any one physical")
    p("  marker. This tool pools every excursion in a capture; it does not")
    p("  identify which excursions, if any, correspond to the same physical")
    p("  location on different passes. Variation AMONG different broad")
    p("  responses is visible here; repeatability of the SAME marker across")
    p("  repeated passes remains unmeasured by this analysis.")


# ------------------------------------------------------------------------
# Categorization for the item-6 illustrative plots — descriptive selection
# ONLY, never used to filter the CSV, and not a classifier.
# ------------------------------------------------------------------------
def categorize_for_plots(principal, *, narrow_max_ms, broad_min_ms, outlier_mad_k):
    def durf(e):
        return float(e["duration_ms"])

    def fluxf(e):
        return float(e["integrated_abs_flux_count_ms"])

    narrow = [e for e in principal if durf(e) < narrow_max_ms]
    broad = [e for e in principal if durf(e) >= broad_min_ms]
    overlap = [e for e in principal if narrow_max_ms <= durf(e) < broad_min_ms]

    def median_of(vals):
        vals = sorted(vals)
        n = len(vals)
        if not n:
            return 0.0
        return vals[n // 2] if n % 2 else (vals[n // 2 - 1] + vals[n // 2]) / 2.0

    def mad_outliers(group, *, high):
        vals = [fluxf(e) for e in group]
        if not vals:
            return []
        med = median_of(vals)
        mad = median_of([abs(v - med) for v in vals])
        # A near-zero MAD (many near-identical low-flux narrow spikes) would
        # make the threshold degenerate; floor the spread at 10% of the
        # median so the outlier test stays meaningful.
        spread = max(mad, 0.1 * med, 1e-9)
        if high:
            cutoff = med + outlier_mad_k * spread
            hits = [e for e in group if fluxf(e) > cutoff]
        else:
            cutoff = med - outlier_mad_k * spread
            hits = [e for e in group if fluxf(e) < cutoff]
        return sorted(hits, key=fluxf, reverse=high)

    def representative(group):
        if not group:
            return []
        med_d = median_of([durf(e) for e in group])
        med_f = median_of([fluxf(e) for e in group])

        def score(e):
            dd = (durf(e) - med_d) / (med_d or 1.0)
            df = (fluxf(e) - med_f) / (med_f or 1.0)
            return dd * dd + df * df
        return [min(group, key=score)]

    return {
        "narrow_high_flux_outlier": mad_outliers(narrow, high=True),
        "broad_low_flux_outlier": mad_outliers(broad, high=False),
        "overlap_region": sorted(overlap, key=durf),
        "representative_narrow": representative(narrow),
        "representative_broad": representative(broad),
    }


# ------------------------------------------------------------------------
# Plots
# ------------------------------------------------------------------------
def make_plots(excursions, args):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available -- skipping plots", file=sys.stderr)
        return

    principal = [e for e in excursions if not e["incomplete"] and not e["in_low_pwm_dwell"]]
    incompl = [e for e in excursions if e["incomplete"]]
    dwell = [e for e in excursions if e["in_low_pwm_dwell"] and not e["incomplete"]]
    groups = ((principal, "#1f77b4", "principal"),
             (dwell, "#9467bd", "low-PWM dwell"),
             (incompl, "#d62728", "incomplete"))

    xlim = getattr(args, "plot_max_duration_ms", None)

    def clip(group):
        if not xlim:
            return group, 0
        kept = [e for e in group if float(e["duration_ms"]) <= xlim]
        return kept, len(group) - len(kept)

    if args.plot_width_vs_flux:
        fig, ax = plt.subplots(figsize=(8, 6))
        dropped_total = 0
        for group, colour, label in groups:
            kept, dropped = clip(group)
            dropped_total += dropped
            if not kept:
                continue
            ax.scatter([float(e["duration_ms"]) for e in kept],
                      [float(e["integrated_abs_flux_count_ms"]) for e in kept],
                      s=10, alpha=0.5, color=colour, label=label)
        ax.set_xlabel("duration (ms)")
        ax.set_ylabel("integrated |flux| (count*ms)")
        title = ("width vs. integrated flux — %s\nbaseline=%s, INVESTIGATORY, no classification implied"
                % (args.capture, args.baseline_mode))
        if xlim:
            title += "\n(zoomed to <=%g ms; %d excursion(s) beyond this not shown)" % (xlim, dropped_total)
        ax.set_title(title, fontsize=10)
        ax.legend(fontsize=8)
        ax.grid(alpha=0.25)
        fig.tight_layout()
        fig.savefig(args.plot_width_vs_flux, dpi=130)
        plt.close(fig)
        print("wrote %s" % args.plot_width_vs_flux)

    if args.plot_peak_vs_width:
        fig, ax = plt.subplots(figsize=(8, 6))
        dropped_total = 0
        for group, colour, label in groups:
            kept, dropped = clip(group)
            dropped_total += dropped
            if not kept:
                continue
            ax.scatter([float(e["duration_ms"]) for e in kept],
                      [float(e["max_abs_flux"]) for e in kept],
                      s=10, alpha=0.5, color=colour, label=label)
        ax.set_xlabel("duration (ms)")
        ax.set_ylabel("max |flux| (counts)")
        title = ("peak vs. width — %s\nbaseline=%s, INVESTIGATORY, no classification implied"
                % (args.capture, args.baseline_mode))
        if xlim:
            title += "\n(zoomed to <=%g ms; %d excursion(s) beyond this not shown)" % (xlim, dropped_total)
        ax.set_title(title, fontsize=10)
        ax.legend(fontsize=8)
        ax.grid(alpha=0.25)
        fig.tight_layout()
        fig.savefig(args.plot_peak_vs_width, dpi=130)
        plt.close(fig)
        print("wrote %s" % args.plot_peak_vs_width)


def plot_excursion_detail(ctx, e, out_path, margin_s, entry_threshold,
                          exit_threshold, capture_label):
    """The item-7 detail plot: raw samples with the baseline overlaid (top),
    the baseline-relative deviation with the entry/exit thresholds and the
    open/close points marked (bottom), and every required number in a
    monospace header."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available -- skipping detail plot", file=sys.stderr)
        return False

    samples, baseline = ctx["samples"], ctx["baseline"]
    t_lo = float(e["start_time_s"]) - margin_s
    t_hi = float(e["end_time_s"]) + margin_s
    idx_win = [i for i, s in enumerate(samples) if t_lo <= s["t_s"] <= t_hi]
    if not idx_win:
        return False
    t0 = samples[idx_win[0]]["t_s"]
    ts = [samples[i]["t_s"] - t0 for i in idx_win]
    raw = [samples[i]["raw"] for i in idx_win]
    base = [baseline[i] for i in idx_win]
    dev = [samples[i]["raw"] - baseline[i] for i in idx_win]
    x_lo = float(e["start_time_s"]) - t0
    x_hi = float(e["end_time_s"]) - t0

    fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(10, 7),
                                   gridspec_kw={"height_ratios": [2, 1]})

    ax1.plot(ts, raw, lw=0.9, color="#1f77b4", label="raw ADC")
    ax1.plot(ts, base, lw=1.2, color="#2ca02c", ls="--",
             label="baseline (%s)" % e["baseline_mode"])
    ax1.axvspan(x_lo, x_hi, color="#ff7f0e", alpha=0.15)
    ax1.axvline(x_lo, color="#ff7f0e", lw=1.3, label="open")
    ax1.axvline(x_hi, color="#8c564b", lw=1.3, label="close")
    ax1.set_ylabel("Hall A raw ADC counts")
    ax1.legend(fontsize=7, loc="best")
    ax1.grid(alpha=0.25)

    ax2.plot(ts, dev, lw=0.9, color="#1f77b4")
    for lvl, style in ((entry_threshold, ":"), (-entry_threshold, ":")):
        ax2.axhline(lvl, color="#999999", lw=0.8, ls=style)
    for lvl, style in ((exit_threshold, "--"), (-exit_threshold, "--")):
        ax2.axhline(lvl, color="#cccccc", lw=0.8, ls=style)
    ax2.axvspan(x_lo, x_hi, color="#ff7f0e", alpha=0.15)
    ax2.axvline(x_lo, color="#ff7f0e", lw=1.3)
    ax2.axvline(x_hi, color="#8c564b", lw=1.3)
    ax2.set_ylabel("deviation from baseline (counts)")
    ax2.set_xlabel("time (s), window start = %.6f s firmware time" % t0)
    ax2.grid(alpha=0.25)

    status = "INCOMPLETE" if int(e["incomplete"]) else "COMPLETE"
    if int(e["in_low_pwm_dwell"]):
        status += " / LOW-PWM DWELL (unconfirmed stall/assist proxy)"
    info = (
        "%s   session %s   excursion #%s   [%s]\n"
        "sample %s..%s   t=%s..%s s   duration=%s ms\n"
        "peak delta=%s counts (pos=%s neg=%s)   "
        "integrated flux: abs=%s signed=%s count*ms\n"
        "baseline=%s counts (%s / %s, n_quiet=%s)   "
        "gaps within=%s near=%s"
        % (capture_label, e["session"], e["excursion_id"], status,
           e["start_sample"], e["end_sample"], e["start_time_s"], e["end_time_s"],
           e["duration_ms"], e["max_abs_flux"], e["max_pos_flux"], e["max_neg_flux"],
           e["integrated_abs_flux_count_ms"], e["integrated_signed_flux_count_ms"],
           e["baseline_at_excursion"], e["baseline_mode"], e["baseline_method"],
           e["baseline_n_quiet"], e["gaps_within_count"], e["gaps_near_count"]))
    fig.suptitle(info, fontsize=8, family="monospace", ha="left", x=0.02)
    fig.tight_layout(rect=[0, 0, 1, 0.82])
    fig.savefig(out_path, dpi=130)
    plt.close(fig)
    return True


# ------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", help=".hwt capture or decoded .csv from hwt_decode.py")
    ap.add_argument("-o", "--out", help="excursion CSV output (default: alongside input)")
    ap.add_argument("--summary", help="summary report output (default: alongside input)")
    ap.add_argument("--entry-threshold", type=float, default=30.0,
                    help="counts above the baseline to OPEN an excursion (default 30)")
    ap.add_argument("--exit-threshold", type=float, default=15.0,
                    help="counts above the baseline to STAY in an excursion "
                         "once open (default 15; must be <= entry threshold)")
    ap.add_argument("--baseline-mode", choices=("frozen", "moving"), default="frozen",
                    help="frozen (default, corrected): baseline is fixed from "
                         "samples preceding each excursion's opening and never "
                         "updated by the excursion's own samples. moving: the "
                         "original centered rolling statistic, kept selectable "
                         "for direct comparison.")
    ap.add_argument("--baseline-window", type=int, default=501,
                    help="[moving mode] centered window width in samples (default 501)")
    ap.add_argument("--pre-window", type=int, default=200,
                    help="[frozen mode] trailing quiet-sample window used to "
                         "compute the baseline that gets frozen at each "
                         "excursion's opening (default 200)")
    ap.add_argument("--baseline-method", choices=("mean", "median"), default="mean",
                    help="statistic used to reduce a baseline window to a "
                         "single value, in EITHER mode (default mean; median "
                         "is more robust but slower on large captures)")
    ap.add_argument("--gap-margin-s", type=float, default=0.05,
                    help="how close (seconds) an acquisition hole must be to "
                         "an excursion to count as 'near' rather than 'within' "
                         "(default 0.05)")
    ap.add_argument("--dwell-pwm-max", type=float, default=5.0,
                    help="PWM at/below this counts toward a low-PWM dwell "
                         "window -- a telemetry-only proxy, NOT a confirmed "
                         "stall (default 5)")
    ap.add_argument("--dwell-min-ms", type=float, default=1000.0,
                    help="minimum sustained duration to count as a dwell "
                         "window (default 1000 ms)")
    ap.add_argument("--compare-baselines", action="store_true",
                    help="also evaluate each excursion's identical sample "
                         "range against the OTHER baseline mode, for direct "
                         "before/after comparison (adds alt_* CSV columns "
                         "and a summary section)")
    ap.add_argument("--narrow-max-ms", type=float, default=5.0,
                    help="duration below which an excursion is 'narrow' for "
                         "PWM-binning and plot categorization only (default 5)")
    ap.add_argument("--broad-min-ms", type=float, default=20.0,
                    help="duration at/above which an excursion is 'broad' "
                         "for plot categorization only (default 20)")
    ap.add_argument("--outlier-mad-k", type=float, default=5.0,
                    help="robust outlier multiplier (median +/- k*MAD of "
                         "integrated flux within its width group) used only "
                         "to pick which excursions to plot as outliers, for "
                         "--plot-categories-dir (default 5)")
    ap.add_argument("--plot-max-per-category", type=int, default=8,
                    help="cap on how many excursions to plot per category "
                         "under --plot-categories-dir (default 8; truncation "
                         "is always logged, never silent)")
    ap.add_argument("--plot-width-vs-flux", help="PNG: duration vs. integrated |flux|")
    ap.add_argument("--plot-peak-vs-width", help="PNG: max |flux| vs. duration")
    ap.add_argument("--plot-max-duration-ms", type=float,
                    help="zoom the two scatter plots to durations at/below "
                         "this (ms) -- purely a display range; excluded "
                         "points are counted in the plot title, never "
                         "silently dropped. Omit for the full, honest range "
                         "(a few extreme incomplete/merged excursions can "
                         "otherwise compress everything else into a corner)")
    ap.add_argument("--plot-excursions",
                    help="comma-separated excursion_id list to plot individually")
    ap.add_argument("--plot-top", type=int,
                    help="also plot this many excursions individually, ranked by --plot-top-by")
    ap.add_argument("--plot-top-by", choices=("max_abs_flux", "duration_ms",
                                              "integrated_abs_flux_count_ms"),
                    default="max_abs_flux", help="ranking metric for --plot-top")
    ap.add_argument("--plot-dir", default=".",
                    help="directory for --plot-top/--plot-excursions PNGs (default: cwd)")
    ap.add_argument("--plot-margin-s", type=float, default=0.5,
                    help="context shown either side of an individually plotted "
                         "excursion (default 0.5 s)")
    ap.add_argument("--plot-categories-dir",
                    help="generate the item-6 category plots (narrow-high-"
                         "flux outliers, broad-low-flux outliers, overlap-"
                         "region excursions, one representative narrow and "
                         "one representative broad excursion) into "
                         "subdirectories under this path")
    args = ap.parse_args()

    if args.exit_threshold > args.entry_threshold:
        print("--exit-threshold must be <= --entry-threshold", file=sys.stderr)
        return 2

    result = analyze_captures(
        args.capture, entry_threshold=args.entry_threshold,
        exit_threshold=args.exit_threshold, baseline_mode=args.baseline_mode,
        baseline_window=args.baseline_window, baseline_method=args.baseline_method,
        pre_window=args.pre_window, gap_margin_s=args.gap_margin_s,
        dwell_pwm_max=args.dwell_pwm_max, dwell_min_ms=args.dwell_min_ms,
        compare_baselines=args.compare_baselines)
    excursions = result["excursions"]
    sessions_ctx = result["sessions"]

    base = os.path.splitext(args.capture)[0]
    out = args.out or base + "_excursions.csv"
    with open(out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=EXCURSION_COLUMNS)
        w.writeheader()
        w.writerows(excursions)
    print("wrote %s (%d excursions)" % (out, len(excursions)))

    summary_path = args.summary or base + "_excursion_summary.txt"
    with open(summary_path, "w") as fh:
        write_summary(excursions, args, fh)
    write_summary(excursions, args, sys.stdout)
    print("wrote %s" % summary_path)

    make_plots(excursions, args)

    label = os.path.basename(args.capture)
    by_id = {e["excursion_id"]: e for e in excursions}

    ids = set()
    if args.plot_excursions:
        ids.update(int(x) for x in args.plot_excursions.split(",") if x.strip())
    if args.plot_top:
        ranked = sorted(excursions, key=lambda e: float(e[args.plot_top_by]), reverse=True)
        ids.update(e["excursion_id"] for e in ranked[:args.plot_top])
    if ids:
        os.makedirs(args.plot_dir, exist_ok=True)
        for eid in sorted(ids):
            e = by_id.get(eid)
            ctx = sessions_ctx.get(e["session"]) if e else None
            if ctx is None:
                continue
            out_path = os.path.join(args.plot_dir, "excursion_%04d.png" % eid)
            if plot_excursion_detail(ctx, e, out_path, args.plot_margin_s,
                                     args.entry_threshold, args.exit_threshold, label):
                print("wrote %s" % out_path)

    if args.plot_categories_dir:
        principal = [e for e in excursions if not e["incomplete"] and not e["in_low_pwm_dwell"]]
        cats = categorize_for_plots(
            principal, narrow_max_ms=args.narrow_max_ms,
            broad_min_ms=args.broad_min_ms, outlier_mad_k=args.outlier_mad_k)
        for cat_name, group in cats.items():
            if not group:
                print("category %-24s : none found" % cat_name)
                continue
            capped = group[:args.plot_max_per_category]
            if len(group) > len(capped):
                print("category %-24s : %d found, plotting %d (capped by --plot-max-per-category)"
                     % (cat_name, len(group), len(capped)))
            else:
                print("category %-24s : %d found" % (cat_name, len(group)))
            cat_dir = os.path.join(args.plot_categories_dir, cat_name)
            os.makedirs(cat_dir, exist_ok=True)
            for e in capped:
                ctx = sessions_ctx.get(e["session"])
                if ctx is None:
                    continue
                out_path = os.path.join(cat_dir, "excursion_%04d.png" % e["excursion_id"])
                if plot_excursion_detail(ctx, e, out_path, args.plot_margin_s,
                                         args.entry_threshold, args.exit_threshold, label):
                    print("  wrote %s" % out_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
