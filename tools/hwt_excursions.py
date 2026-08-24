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
        --plot-peak-vs-width run_peak_width.png

Or hand it the raw capture directly (it decodes internally, on the fly):

    python3 tools/hwt_excursions.py run.hwt -o run_excursions.csv

================================================================================
ALGORITHM — deliberately simple; this section is the whole of it
================================================================================

1. BASELINE (--baseline-window, --baseline-method)
   A rolling statistic of channel A's raw ADC counts, centered on each
   sample: the mean (default, fast) or the median (slower, more robust to
   the excursions themselves) over a window of N samples either side.
   This tracks slow drift — temperature, supply voltage, sensor offset —
   without needing a fixed constant. Nothing about this window is a claim
   about the physical layout of the track; it is purely a smoothing choice,
   and it is left in the operator's hands because the right window depends
   on how fast the locomotive is moving and how far apart real excursions
   are expected to be. Too narrow, and the baseline is pulled up into a
   passage's own excursion; too wide, and slow drift leaks into what is
   reported as "flux". Both failure modes are visible in the width-vs-flux
   plot as artefacts, which is why that plot exists.

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
     - integrated |flux|, in count*milliseconds, is
           sum( |dev[i]| * dt_ms[i] )
       over the excursion's samples — a rectangle rule using the REAL
       measured interval before each sample, not a fixed step;
     - rise time is peak time minus opening time; fall time is closing
       time minus peak time;
     - PWM and direction are read at the peak sample, with a flag raised
       if either one changed anywhere across the excursion (worth knowing
       before trusting the "at peak" values as representative).

5. INCOMPLETE EXCURSIONS
   Every GAP / MISSED / DROP / SESSION row the decoder produced is a
   named, timestamped hole in the physical record. If one of those holes
   falls inside an excursion's own time span, or the excursion runs off
   the end of a session without ever dropping back below the exit
   threshold, the excursion is marked incomplete=1. It is still reported
   — narrow AND broad, complete AND incomplete, all of it — because
   silently dropping incomplete excursions would silently discard exactly
   the evidence needed to judge whether a given observation should be
   trusted.
================================================================================
"""

import argparse
import bisect
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_format as F   # noqa: E402
import hwt_decode as D   # noqa: E402

BREAK_TYPES = ("GAP", "MISSED", "DROP", "SESSION")

EXCURSION_COLUMNS = [
    "excursion_id", "session",
    "start_sample", "end_sample", "n_samples", "nominal_span_samples",
    "start_time_s", "end_time_s", "duration_ms",
    "baseline_at_excursion",
    "max_pos_flux", "max_neg_flux", "max_abs_flux", "t_max_abs_flux_s",
    "integrated_abs_flux_count_ms",
    "rise_time_ms", "fall_time_ms",
    "pwm_actual_at_peak", "pwm_commanded_at_peak", "pwm_changed",
    "dir_at_peak", "dir_changed",
    "gaps_within_count", "gaps_near_count", "incomplete",
    "anchor_before_id", "anchor_before_text", "anchor_before_dt_s",
    "anchor_after_id", "anchor_after_text", "anchor_after_dt_s",
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
# Baseline
# ------------------------------------------------------------------------
def rolling_baseline(values, window, method):
    """Centered rolling statistic over `values` (a plain list of numbers).
    window is the FULL window width in samples; half goes each side of the
    center sample, clipped at the ends of the list (no wraparound, no
    reflection, no fabricated padding)."""
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


# ------------------------------------------------------------------------
# Collector
# ------------------------------------------------------------------------
def collect_excursions(samples, baseline, entry_threshold, exit_threshold):
    """The permissive two-threshold collector described in the module
    docstring. Returns a list of dicts with sample-INDEX ranges only
    (start_i, end_i, peak_i, forced_end); measurement happens separately."""
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
                out.append({"idxs": idxs, "forced_end": False})
                in_exc = False
                idxs = None
    if in_exc:
        out.append({"idxs": idxs, "forced_end": True})
    return out


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
                       breaks, anchors, gap_margin_s):
    start_i, end_i = idxs[0], idxs[-1]
    s0, s1 = samples[start_i], samples[end_i]

    devs = [samples[i]["raw"] - baseline[i] for i in idxs]
    peak_local = max(range(len(idxs)), key=lambda k: abs(devs[k]))
    peak_i = idxs[peak_local]
    peak_dev = devs[peak_local]
    peak_s = samples[peak_i]

    max_pos = max([0.0] + [d for d in devs if d > 0])
    max_neg = min([0.0] + [d for d in devs if d < 0])
    integrated = sum(abs(d) * samples[i]["dt_ms"] for d, i in zip(devs, idxs))

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

    ab_id, ab_text, ab_dt = nearest_anchor(anchors, t_lo, before=True)
    aa_id, aa_text, aa_dt = nearest_anchor(anchors, t_hi, before=False)

    return {
        "excursion_id": eid, "session": sid,
        "start_sample": s0["seq"], "end_sample": s1["seq"],
        "n_samples": len(idxs),
        "nominal_span_samples": s1["seq"] - s0["seq"] + 1,
        "start_time_s": "%.6f" % t_lo, "end_time_s": "%.6f" % t_hi,
        "duration_ms": "%.3f" % ((t_hi - t_lo) * 1000.0),
        "baseline_at_excursion": "%.2f" % baseline[start_i],
        "max_pos_flux": "%.2f" % max_pos, "max_neg_flux": "%.2f" % max_neg,
        "max_abs_flux": "%.2f" % abs(peak_dev),
        "t_max_abs_flux_s": "%.6f" % peak_s["t_s"],
        "integrated_abs_flux_count_ms": "%.3f" % integrated,
        "rise_time_ms": "%.3f" % ((peak_s["t_s"] - t_lo) * 1000.0),
        "fall_time_ms": "%.3f" % ((t_hi - peak_s["t_s"]) * 1000.0),
        "pwm_actual_at_peak": peak_s["pwm_actual"],
        "pwm_commanded_at_peak": peak_s["pwm_commanded"],
        "pwm_changed": int(pwm_changed),
        "dir_at_peak": peak_s["dir"], "dir_changed": int(dir_changed),
        "gaps_within_count": len(within), "gaps_near_count": len(near),
        "incomplete": int(incomplete),
        "anchor_before_id": ab_id, "anchor_before_text": ab_text,
        "anchor_before_dt_s": ab_dt,
        "anchor_after_id": aa_id, "anchor_after_text": aa_text,
        "anchor_after_dt_s": aa_dt,
    }


def find_excursions(path, *, entry_threshold, exit_threshold,
                    baseline_window, baseline_method, gap_margin_s):
    rows = load_rows(path)
    excursions = []
    eid = 0
    for sid, srows in split_sessions(rows):
        samples, breaks, anchors = build_series(srows)
        if not samples:
            continue
        raw = [s["raw"] for s in samples]
        baseline = rolling_baseline(raw, baseline_window, baseline_method)
        for c in collect_excursions(samples, baseline, entry_threshold, exit_threshold):
            eid += 1
            excursions.append(measure_excursion(
                eid, sid, samples, baseline, c["idxs"], c["forced_end"],
                breaks, anchors, gap_margin_s))
    return excursions


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
    p("  entry / exit threshold   %d / %d counts" % (args.entry_threshold, args.exit_threshold))
    p("  baseline                 %s over %d samples" % (args.baseline_method, args.baseline_window))
    p("  gap margin               %.3f s" % args.gap_margin_s)
    p("  excursions found         %d" % len(excursions))
    if not excursions:
        return
    incomplete = [e for e in excursions if e["incomplete"]]
    p("  incomplete               %d (%.1f%%)" % (len(incomplete),
      100.0 * len(incomplete) / len(excursions)))

    dur = _stats([float(e["duration_ms"]) for e in excursions])
    flux = _stats([float(e["integrated_abs_flux_count_ms"]) for e in excursions])
    peak = _stats([float(e["max_abs_flux"]) for e in excursions])
    p("")
    p("  duration_ms      n=%d  min=%.2f  median=%.2f  mean=%.2f  max=%.2f"
      % (dur["n"], dur["min"], dur["median"], dur["mean"], dur["max"]))
    p("  integrated_flux  n=%d  min=%.2f  median=%.2f  mean=%.2f  max=%.2f"
      % (flux["n"], flux["min"], flux["median"], flux["mean"], flux["max"]))
    p("  max_abs_flux     n=%d  min=%.2f  median=%.2f  mean=%.2f  max=%.2f"
      % (peak["n"], peak["min"], peak["median"], peak["mean"], peak["max"]))

    p("")
    p("  NOTE: this is a descriptive quantile split, not a classification.")
    p("  Splitting at the median duration only shows how the population")
    p("  divides on ONE axis; whether that split means anything physically")
    p("  is a judgement for whoever reads the plots, informed by anchors.")
    if dur["median"] > 0:
        narrow = [e for e in excursions if float(e["duration_ms"]) <= dur["median"]]
        broad = [e for e in excursions if float(e["duration_ms"]) > dur["median"]]
        nf = _stats([float(e["integrated_abs_flux_count_ms"]) for e in narrow])
        bf = _stats([float(e["integrated_abs_flux_count_ms"]) for e in broad])
        p("  at/below median duration (n=%d): integrated flux median=%.2f"
          % (nf["n"], nf["median"]))
        p("  above median duration    (n=%d): integrated flux median=%.2f"
          % (bf["n"], bf["median"]))

    pwms = {e["pwm_actual_at_peak"] for e in excursions}
    if len(pwms) > 1:
        # Bucketed by 10 counts of PWM -- an exact-value table is unreadable
        # once PWM has been ramped continuously through a run.
        bin_w = 10
        by_bin = {}
        for e in excursions:
            b = (e["pwm_actual_at_peak"] // bin_w) * bin_w
            by_bin.setdefault(b, []).append(float(e["duration_ms"]))
        p("")
        p("  duration_ms by PWM actual at peak, binned by %d (descriptive only):"
          % bin_w)
        for b in sorted(by_bin):
            st = _stats(by_bin[b])
            p("    pwm %3d-%-3d  n=%-5d median=%-8.2f mean=%-8.2f max=%-8.2f"
              % (b, b + bin_w - 1, st["n"], st["median"], st["mean"], st["max"]))


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

    complete = [e for e in excursions if not e["incomplete"]]
    incompl = [e for e in excursions if e["incomplete"]]

    if args.plot_width_vs_flux:
        fig, ax = plt.subplots(figsize=(8, 6))
        for group, colour, label in ((complete, "#1f77b4", "complete"),
                                     (incompl, "#d62728", "incomplete")):
            if not group:
                continue
            ax.scatter([float(e["duration_ms"]) for e in group],
                      [float(e["integrated_abs_flux_count_ms"]) for e in group],
                      s=10, alpha=0.5, color=colour, label=label)
        ax.set_xlabel("duration (ms)")
        ax.set_ylabel("integrated |flux| (count*ms)")
        ax.set_title("width vs. integrated flux — %s\nINVESTIGATORY, no classification implied"
                     % args.capture, fontsize=10)
        ax.legend(fontsize=8)
        ax.grid(alpha=0.25)
        fig.tight_layout()
        fig.savefig(args.plot_width_vs_flux, dpi=130)
        plt.close(fig)
        print("wrote %s" % args.plot_width_vs_flux)

    if args.plot_peak_vs_width:
        fig, ax = plt.subplots(figsize=(8, 6))
        for group, colour, label in ((complete, "#1f77b4", "complete"),
                                     (incompl, "#d62728", "incomplete")):
            if not group:
                continue
            ax.scatter([float(e["duration_ms"]) for e in group],
                      [float(e["max_abs_flux"]) for e in group],
                      s=10, alpha=0.5, color=colour, label=label)
        ax.set_xlabel("duration (ms)")
        ax.set_ylabel("max |flux| (counts)")
        ax.set_title("peak vs. width — %s\nINVESTIGATORY, no classification implied"
                     % args.capture, fontsize=10)
        ax.legend(fontsize=8)
        ax.grid(alpha=0.25)
        fig.tight_layout()
        fig.savefig(args.plot_peak_vs_width, dpi=130)
        plt.close(fig)
        print("wrote %s" % args.plot_peak_vs_width)


def plot_selected_waveforms(path, excursions, ids, outdir, margin_s):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available -- skipping individual waveform plots",
             file=sys.stderr)
        return

    rows = load_rows(path)
    by_session = dict(split_sessions(rows))
    wanted = {e["excursion_id"]: e for e in excursions if e["excursion_id"] in ids}
    os.makedirs(outdir, exist_ok=True)

    for eid, e in wanted.items():
        srows = by_session.get(e["session"])
        if srows is None:
            continue
        samples, _breaks, anchors = build_series(srows)
        t_lo = float(e["start_time_s"]) - margin_s
        t_hi = float(e["end_time_s"]) + margin_s
        win = [s for s in samples if t_lo <= s["t_s"] <= t_hi]
        if not win:
            continue
        t0 = win[0]["t_s"]
        fig, ax = plt.subplots(figsize=(9, 4))
        ax.plot([s["t_s"] - t0 for s in win], [s["raw"] for s in win],
               lw=0.9, color="#1f77b4")
        ax.axvspan(float(e["start_time_s"]) - t0, float(e["end_time_s"]) - t0,
                  color="#ff7f0e", alpha=0.15)
        for at, _aid, text in anchors:
            if t_lo <= at <= t_hi:
                ax.axvline(at - t0, color="#111111", lw=1.0, alpha=0.6)
                ax.annotate(text, (at - t0, ax.get_ylim()[0]), fontsize=8,
                           rotation=90, va="bottom")
        ax.set_xlabel("time (s), window start")
        ax.set_ylabel("Hall A raw ADC counts")
        ax.set_title("excursion #%d — session %s — %s..%s (%s)\nINVESTIGATORY"
                     % (eid, e["session"], e["start_time_s"], e["end_time_s"],
                        "incomplete" if int(e["incomplete"]) else "complete"),
                     fontsize=9)
        ax.grid(alpha=0.25)
        fig.tight_layout()
        out = os.path.join(outdir, "excursion_%04d.png" % eid)
        fig.savefig(out, dpi=130)
        plt.close(fig)
        print("wrote %s" % out)


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
    ap.add_argument("--baseline-window", type=int, default=501,
                    help="baseline window width in samples, centered (default 501)")
    ap.add_argument("--baseline-method", choices=("mean", "median"), default="mean",
                    help="rolling baseline statistic (default mean; median is "
                         "more robust but slower on large captures)")
    ap.add_argument("--gap-margin-s", type=float, default=0.05,
                    help="how close (seconds) an acquisition hole must be to "
                         "an excursion to count as 'near' rather than 'within' "
                         "(default 0.05)")
    ap.add_argument("--plot-width-vs-flux", help="PNG: duration vs. integrated |flux|")
    ap.add_argument("--plot-peak-vs-width", help="PNG: max |flux| vs. duration")
    ap.add_argument("--plot-excursions",
                    help="comma-separated excursion_id list to plot individually")
    ap.add_argument("--plot-top", type=int,
                    help="also plot this many excursions individually, ranked by --plot-top-by")
    ap.add_argument("--plot-top-by", choices=("max_abs_flux", "duration_ms",
                                              "integrated_abs_flux_count_ms"),
                    default="max_abs_flux", help="ranking metric for --plot-top")
    ap.add_argument("--plot-dir", default=".",
                    help="directory for individual waveform PNGs (default: cwd)")
    ap.add_argument("--plot-margin-s", type=float, default=0.5,
                    help="context shown either side of an individually plotted "
                         "excursion (default 0.5 s)")
    args = ap.parse_args()

    if args.exit_threshold > args.entry_threshold:
        print("--exit-threshold must be <= --entry-threshold", file=sys.stderr)
        return 2

    excursions = find_excursions(
        args.capture, entry_threshold=args.entry_threshold,
        exit_threshold=args.exit_threshold, baseline_window=args.baseline_window,
        baseline_method=args.baseline_method, gap_margin_s=args.gap_margin_s)

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

    ids = set()
    if args.plot_excursions:
        ids.update(int(x) for x in args.plot_excursions.split(",") if x.strip())
    if args.plot_top:
        ranked = sorted(excursions, key=lambda e: float(e[args.plot_top_by]), reverse=True)
        ids.update(e["excursion_id"] for e in ranked[:args.plot_top])
    if ids:
        plot_selected_waveforms(args.capture, excursions, ids, args.plot_dir,
                                args.plot_margin_s)

    return 0


if __name__ == "__main__":
    sys.exit(main())
