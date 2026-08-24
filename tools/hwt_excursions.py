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

7. SAME-RANGE BASELINE REMEASUREMENT (--compare-baselines) — NOT an
   independent comparison
   For every excursion found under the primary --baseline-mode, the exact
   SAME sample range (frozen's boundaries, or moving's, whichever is
   primary) is ALSO measured against the OTHER mode's baseline array. This
   isolates what the baseline choice alone changes on one already-fixed
   range — it answers "how would the other method have scored this exact
   stretch of samples", not "what would the other method have DETECTED".
   The boundaries themselves are never revisited. See §8 for the
   independent comparison that does revisit them.

8. INDEPENDENT DETECTOR COMPARISON (--independent-compare)
   Runs BOTH detectors to completion, fully separately: moving gets its
   own excursion list from collect_excursions() against rolling_baseline();
   frozen gets its own, independently, from frozen_baseline_collect().
   Neither list is derived from, constrained by, or remeasured against the
   other — each is exactly what that detector alone would have reported
   run in isolation. The two lists are then MATCHED by where their
   independently-drawn [start_sample, end_sample] ranges overlap
   (match_events()), classified as one-to-one, one frozen event covering
   several moving events, several frozen events covering one moving event,
   many-to-many, or unmatched on either side. Only this comparison can show
   a frozen event absorbing several independently-detected moving events —
   the same-range remeasurement in §7 cannot, because it never lets moving
   propose its own boundaries.

9. SETTLEDNESS EXPERIMENT (--settle-metric, --settle-threshold) —
   EXPERIMENTAL, off by default, changes nothing unless both are given
   Investigating whether the frozen mode's merging failure (§5/§6 of the
   accompanying report) can be caught or prevented starts from one
   question: was the pre-event quiet window itself actually settled at the
   moment it got frozen, or was it still sliding down some earlier
   transient's decay tail? Three deliberately simple, transparent measures
   of the quiet window at freeze time are always computed and reported,
   whether or not gating is enabled: pre_range_counts (max-min),
   pre_stdev_counts (population standard deviation), and
   pre_slope_counts_per_sample (last sample minus first, divided by window
   length) — see _pre_window_metrics(). None of the three is asserted to
   be the right one; the report evaluates all three against the real
   merging cases found.
   When --settle-metric and --settle-threshold are BOTH given, an
   experimental gate activates: a sample that crosses --entry-threshold is
   only allowed to freeze if its live pre-window's chosen metric is at or
   below the threshold. If not, that sample opens NOTHING — it is treated
   as neither quiet (it has clearly departed) nor excursion (freezing
   would use an unsettled baseline); the collector simply moves on. If the
   signal never settles before it returns near baseline on its own, the
   excursion is never reported at all under the gate. This is a real,
   reportable failure mode of gating, not swept under anything. The gate
   never terminates or truncates an already-open excursion by duration —
   it only ever affects whether a NEW excursion is allowed to open.
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
    "pre_range_counts", "pre_stdev_counts", "pre_slope_counts_per_sample", "baseline_settled",
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
def _pre_window_metrics(values):
    """Three deliberately simple, transparent settledness measures over a
    quiet window's raw values, taken at the moment of a potential freeze.
    None is asserted to be "correct" -- see module docstring §9.
      range  = max - min                          (counts)
      stdev  = population standard deviation       (counts)
      slope  = (last - first) / (n - 1)             (counts / sample)
    Returns (range, stdev, slope); (0.0, 0.0, 0.0) for an empty window."""
    n = len(values)
    if n == 0:
        return 0.0, 0.0, 0.0
    lo, hi = min(values), max(values)
    mean = sum(values) / n
    var = sum((v - mean) ** 2 for v in values) / n
    slope = (values[-1] - values[0]) / (n - 1) if n > 1 else 0.0
    return float(hi - lo), var ** 0.5, slope


def frozen_baseline_collect(samples, pre_window, method, entry_threshold, exit_threshold,
                            *, settle_metric=None, settle_threshold=None):
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

    Every potential freeze also computes _pre_window_metrics() of the live
    quiet window — always, regardless of gating — and carries them on the
    resulting excursion as diagnostics (module docstring §9).

    EXPERIMENTAL: if settle_metric and settle_threshold are both given, a
    sample that would otherwise open an excursion is refused instead when
    its window's chosen metric exceeds the threshold. A refused sample is
    neither pushed to the quiet window (it has clearly departed from
    baseline; treating it as quiet would misrepresent the window) nor
    opened as an excursion — the collector simply advances. If the signal
    never settles before returning near baseline on its own, no excursion
    is ever reported there under the gate. Off (settle_metric is None),
    this reproduces the original unconditional freeze exactly.

    When an excursion closes, the closing sample (the first one to fall
    below exit_threshold) is re-evaluated in the SAME iteration against a
    freshly recomputed live baseline — it may itself immediately open a
    new excursion (a sharp reversal), or it may simply be quiet and join
    the trailing window. Either way no sample is skipped or double-counted.

    Returns (baseline_array, excursion_list) in the same shape
    rolling_baseline()+collect_excursions() produce for the moving mode,
    so downstream measurement code (measure_excursion) is identical for
    both baseline modes. Each excursion dict additionally carries
    "baseline_n" (quiet samples actually feeding the frozen value, <=
    pre_window) and "pre_range"/"pre_stdev"/"pre_slope"/"settled"."""
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

    metric_of = {"range": lambda r, s, sl: r, "stdev": lambda r, s, sl: s,
                "slope": lambda r, s, sl: abs(sl)}

    excursions = []
    in_exc = False
    idxs = None
    frozen = None
    frozen_n = 0
    frozen_diag = (None, None, None, None)   # pre_range, pre_stdev, pre_slope, settled

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
            excursions.append({"idxs": idxs, "forced_end": False, "baseline_n": frozen_n,
                               "pre_range": frozen_diag[0], "pre_stdev": frozen_diag[1],
                               "pre_slope": frozen_diag[2], "settled": frozen_diag[3]})
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
            pre_range, pre_stdev, pre_slope = _pre_window_metrics(list(quiet))
            settled = None
            if settle_metric is not None and settle_threshold is not None:
                settled = metric_of[settle_metric](pre_range, pre_stdev, pre_slope) <= settle_threshold
                if not settled:
                    # Experimental gate: refuse the freeze. Not quiet, not
                    # excursion -- just skipped, per the docstring above.
                    i += 1
                    continue
            in_exc = True
            frozen = b
            frozen_n = len(quiet)
            frozen_diag = (pre_range, pre_stdev, pre_slope, settled)
            idxs = [i]
            baseline[i] = frozen
        else:
            baseline[i] = b
            push_quiet(raw)
        i += 1

    if in_exc:
        excursions.append({"idxs": idxs, "forced_end": True, "baseline_n": frozen_n,
                           "pre_range": frozen_diag[0], "pre_stdev": frozen_diag[1],
                           "pre_slope": frozen_diag[2], "settled": frozen_diag[3]})
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
                       baseline_mode, baseline_method, baseline_n_quiet=None,
                       pre_range=None, pre_stdev=None, pre_slope=None, settled=None):
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
        "pre_range_counts": "%.2f" % pre_range if pre_range is not None else "",
        "pre_stdev_counts": "%.3f" % pre_stdev if pre_stdev is not None else "",
        "pre_slope_counts_per_sample": "%.4f" % pre_slope if pre_slope is not None else "",
        "baseline_settled": "" if settled is None else int(settled),
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
                     compare_baselines=False, settle_metric=None, settle_threshold=None):
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
                samples, pre_window, baseline_method, entry_threshold, exit_threshold,
                settle_metric=settle_metric, settle_threshold=settle_threshold)
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
                baseline_n_quiet=c.get("baseline_n"), pre_range=c.get("pre_range"),
                pre_stdev=c.get("pre_stdev"), pre_slope=c.get("pre_slope"),
                settled=c.get("settled"))
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
# Independent detector comparison — see module docstring §8.
# ------------------------------------------------------------------------
def match_events(frozen_events, moving_events):
    """Match two INDEPENDENTLY detected, independently measured excursion
    lists by where their own [start_sample, end_sample] ranges overlap.
    Neither list is held fixed and remeasured against the other's
    baseline (compare_against_baseline() does that, for a different,
    narrower question) — this only asks where the two detectors' own,
    separately-drawn boundaries agree, split, or miss.

    Overlap is grouped into connected components with the standard
    "merge overlapping intervals" sweep: sort every interval from both
    lists by start_sample, and extend the current group's end to
    max(current_end, this interval's end) whenever the next interval's
    start falls at or before it. Because overlap on a line is transitive
    within a start-ordered sweep (if A overlaps B and B overlaps C, the
    sweep's running max-end already covers C by the time it is reached
    even if A and C do not themselves overlap), one linear pass finds
    every connected component exactly.

    Returns a list of match-group dicts:
      kind: "one_to_one" | "frozen_covers_multiple_moving" |
            "multiple_frozen_cover_one_moving" | "many_to_many" |
            "unmatched_frozen" | "unmatched_moving"
      frozen: [event, ...]   moving: [event, ...]
      deltas: present only when kind == "one_to_one" -- frozen minus
              moving, for open/close time, duration, peak and both flux
              measures. Each side's own peak/flux was measured against
              its OWN baseline throughout; nothing here is remeasured."""
    tagged = ([("frozen", e) for e in frozen_events] +
             [("moving", e) for e in moving_events])
    tagged.sort(key=lambda t: t[1]["start_sample"])

    groups = []
    cur, cur_end = [], None
    for src, e in tagged:
        s, en = e["start_sample"], e["end_sample"]
        if cur and s <= cur_end:
            cur.append((src, e))
            cur_end = max(cur_end, en)
        else:
            if cur:
                groups.append(cur)
            cur, cur_end = [(src, e)], en
    if cur:
        groups.append(cur)

    out = []
    for g in groups:
        frozen = [e for src, e in g if src == "frozen"]
        moving = [e for src, e in g if src == "moving"]
        if len(frozen) == 1 and len(moving) == 1:
            kind = "one_to_one"
        elif len(frozen) == 1 and len(moving) > 1:
            kind = "frozen_covers_multiple_moving"
        elif len(frozen) > 1 and len(moving) == 1:
            kind = "multiple_frozen_cover_one_moving"
        elif len(frozen) > 1 and len(moving) > 1:
            kind = "many_to_many"
        elif frozen and not moving:
            kind = "unmatched_frozen"
        else:
            kind = "unmatched_moving"
        entry = {"kind": kind, "frozen": frozen, "moving": moving}
        if kind == "one_to_one":
            f, m = frozen[0], moving[0]
            entry["deltas"] = {
                "open_time_delta_s": float(f["start_time_s"]) - float(m["start_time_s"]),
                "close_time_delta_s": float(f["end_time_s"]) - float(m["end_time_s"]),
                "duration_delta_ms": float(f["duration_ms"]) - float(m["duration_ms"]),
                "peak_delta_counts": float(f["max_abs_flux"]) - float(m["max_abs_flux"]),
                "signed_flux_delta": (float(f["integrated_signed_flux_count_ms"])
                                      - float(m["integrated_signed_flux_count_ms"])),
                "abs_flux_delta": (float(f["integrated_abs_flux_count_ms"])
                                   - float(m["integrated_abs_flux_count_ms"])),
            }
        out.append(entry)
    return out


def analyze_independent(path, *, entry_threshold, exit_threshold,
                        baseline_window=501, pre_window=200,
                        baseline_method="mean", gap_margin_s=0.05,
                        dwell_pwm_max=5.0, dwell_min_ms=1000.0,
                        settle_metric=None, settle_threshold=None):
    """Runs BOTH detectors to completion, fully independently, over the
    same samples, then matches the two resulting event lists (see
    match_events()). Returns {"frozen": [...], "moving": [...],
    "matches": [...], "sessions": {sid: {"samples", "frozen_baseline",
    "moving_baseline", "breaks", "anchors", "dwell_windows"}}}."""
    rows = load_rows(path)
    frozen_all, moving_all, matches_all = [], [], []
    sessions_ctx = {}
    fid = mid = 0
    for sid, srows in split_sessions(rows):
        samples, breaks, anchors = build_series(srows)
        if not samples:
            continue
        raw = [s["raw"] for s in samples]
        dwell_windows = detect_low_pwm_dwells(samples, dwell_pwm_max, dwell_min_ms)

        moving_baseline = rolling_baseline(raw, baseline_window, baseline_method)
        moving_coll = collect_excursions(samples, moving_baseline, entry_threshold, exit_threshold)
        moving_events = []
        for c in moving_coll:
            mid += 1
            moving_events.append(measure_excursion(
                mid, sid, samples, moving_baseline, c["idxs"], c["forced_end"],
                breaks, anchors, gap_margin_s, dwell_windows,
                baseline_mode="moving", baseline_method=baseline_method))

        frozen_baseline, frozen_coll = frozen_baseline_collect(
            samples, pre_window, baseline_method, entry_threshold, exit_threshold,
            settle_metric=settle_metric, settle_threshold=settle_threshold)
        frozen_events = []
        for c in frozen_coll:
            fid += 1
            frozen_events.append(measure_excursion(
                fid, sid, samples, frozen_baseline, c["idxs"], c["forced_end"],
                breaks, anchors, gap_margin_s, dwell_windows,
                baseline_mode="frozen", baseline_method=baseline_method,
                baseline_n_quiet=c.get("baseline_n"), pre_range=c.get("pre_range"),
                pre_stdev=c.get("pre_stdev"), pre_slope=c.get("pre_slope"),
                settled=c.get("settled")))

        matches_all.extend(match_events(frozen_events, moving_events))
        frozen_all.extend(frozen_events)
        moving_all.extend(moving_events)
        sessions_ctx[sid] = {"samples": samples, "frozen_baseline": frozen_baseline,
                             "moving_baseline": moving_baseline, "breaks": breaks,
                             "anchors": anchors, "dwell_windows": dwell_windows}
    return {"frozen": frozen_all, "moving": moving_all, "matches": matches_all,
           "sessions": sessions_ctx}


# ------------------------------------------------------------------------
# Review-group categorization — item-4 groups. Descriptive selection only;
# see categorize_for_plots() for the item-6 groups.
# ------------------------------------------------------------------------
def categorize_for_review(frozen_events, matches, *, long_ms=1000.0, material_ms=20.0):
    principal = [e for e in frozen_events if not e["incomplete"] and not e["in_low_pwm_dwell"]]
    long_duration = sorted([e for e in principal if float(e["duration_ms"]) >= long_ms],
                           key=lambda e: -float(e["duration_ms"]))

    covers_multiple = []
    for m in matches:
        if m["kind"] in ("frozen_covers_multiple_moving", "many_to_many"):
            covers_multiple.extend(m["frozen"])

    boundary_differs = []
    for m in matches:
        if m["kind"] != "one_to_one":
            continue
        d = m["deltas"]
        if (abs(d["open_time_delta_s"]) * 1000.0 >= material_ms or
                abs(d["close_time_delta_s"]) * 1000.0 >= material_ms or
                abs(d["duration_delta_ms"]) >= material_ms):
            boundary_differs.append(m)

    incomplete = [e for e in frozen_events if e["incomplete"]]
    dwell = [e for e in frozen_events if e["in_low_pwm_dwell"] and not e["incomplete"]]

    return {
        "long_duration_ge_1s": long_duration,
        "frozen_covers_multiple_moving": covers_multiple,
        "boundary_differs_materially": boundary_differs,
        "incomplete": incomplete,
        "in_low_pwm_dwell": dwell,
    }


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
    p("  complete, outside low-PWM dwell   %d (%.1f%%)  -- called \"principal\" below. This"
      % (len(principal), 100.0 * len(principal) / len(excursions)))
    p("  label describes what was EXCLUDED (incomplete, dwell), not a claim that what")
    p("  remains is free of merged multi-event excursions -- see the long-duration")
    p("  count immediately below and, if this run used --independent-compare, the")
    p("  match-summary file for events actually shown to span multiple independently")
    p("  detected moving excursions.")

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

    long_ms = getattr(args, "long_duration_ms", 1000.0)
    long_dur = [e for e in principal if float(e["duration_ms"]) >= long_ms]
    p("")
    p("  --- long-duration flag within PRINCIPAL: >= %.0f ms (n=%d, %.1f%% of principal) ---"
      % (long_ms, len(long_dur), 100.0 * len(long_dur) / len(principal) if principal else 0.0))
    p("  NOT excluded from the PRINCIPAL statistics above -- duration alone is not")
    p("  proof of a merged multi-event excursion (a genuinely slow single response")
    p("  is also possible and must not be assumed away). Investigation to date found")
    p("  every checked long-duration case had gaps_within_count>0 or, under")
    p("  --independent-compare, spanned multiple independently detected moving")
    p("  excursions -- but this run did not necessarily re-verify that for every")
    p("  case listed here. Treat this as a REVIEW LIST, not a verdict.")
    if long_dur:
        for e in sorted(long_dur, key=lambda e: -float(e["duration_ms"]))[:10]:
            p("    id=%-6s dur=%9s ms  gaps_within=%s  sample %s..%s"
              % (e["excursion_id"], e["duration_ms"], e["gaps_within_count"],
                 e["start_sample"], e["end_sample"]))
        if len(long_dur) > 10:
            p("    ... and %d more (see the excursion CSV for the full list)" % (len(long_dur) - 10))

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
        p("  --- SAME-RANGE remeasurement: %s (primary) vs the alternate mode ---" % args.baseline_mode)
        p("  NOT an independent comparison of what each detector would find on its own --")
        p("  see --independent-compare / the *_matches.csv file for that. This holds the")
        p("  %s detector's own boundaries FIXED and asks only what the other baseline" % args.baseline_mode)
        p("  would have measured across that identical, already-decided range.")
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


def write_match_summary(result, args, fh):
    """Summarizes an --independent-compare run: result is the dict returned
    by analyze_independent() (keys "frozen", "moving", "matches")."""
    p = lambda s: print(s, file=fh)
    frozen, moving, matches = result["frozen"], result["moving"], result["matches"]
    p("HALL_WAVEFORM_TEST independent detector comparison (investigatory)")
    p("  source                   %s" % args.capture)
    p("  entry / exit threshold   %g / %g counts" % (args.entry_threshold, args.exit_threshold))
    p("  frozen: pre-event %s over up to %d quiet samples"
      % (args.baseline_method, args.pre_window))
    p("  moving: centered %s over %d samples" % (args.baseline_method, args.baseline_window))
    p("")
    p("  Both detectors ran to completion, fully independently. Neither list was")
    p("  derived from, constrained by, or remeasured against the other; each event")
    p("  below is exactly what that detector alone reported. Matching is by where")
    p("  the two detectors' own [start_sample, end_sample] ranges overlap.")
    p("")
    p("  frozen events found      %d" % len(frozen))
    p("  moving events found      %d" % len(moving))
    p("  match groups             %d" % len(matches))

    by_kind = {}
    for m in matches:
        by_kind.setdefault(m["kind"], []).append(m)
    order = ["one_to_one", "frozen_covers_multiple_moving",
            "multiple_frozen_cover_one_moving", "many_to_many",
            "unmatched_frozen", "unmatched_moving"]
    p("")
    for kind in order:
        grp = by_kind.get(kind, [])
        p("  %-32s %d" % (kind, len(grp)))

    one_to_one = by_kind.get("one_to_one", [])
    if one_to_one:
        p("")
        p("  --- one-to-one matches: frozen minus moving (n=%d) ---" % len(one_to_one))
        for key, label, scale in (
            ("open_time_delta_s", "open time (ms)", 1000.0),
            ("close_time_delta_s", "close time (ms)", 1000.0),
            ("duration_delta_ms", "duration (ms)", 1.0),
            ("peak_delta_counts", "peak (counts)", 1.0),
            ("signed_flux_delta", "signed flux (count*ms)", 1.0),
            ("abs_flux_delta", "abs flux (count*ms)", 1.0),
        ):
            vals = [m["deltas"][key] * scale for m in one_to_one]
            st = _stats(vals)
            p("  %-24s median=%12.2f  mean=%12.2f  min=%12.2f  max=%12.2f"
              % (label, st["median"], st["mean"], st["min"], st["max"]))

    covers = by_kind.get("frozen_covers_multiple_moving", []) + by_kind.get("many_to_many", [])
    if covers:
        p("")
        p("  --- frozen events covering >=2 independently-detected moving events (n=%d) ---"
          % len(covers))
        for m in sorted(covers, key=lambda m: -len(m["moving"]))[:15]:
            f = m["frozen"][0] if len(m["frozen"]) == 1 else None
            fids = ",".join(str(e["excursion_id"]) for e in m["frozen"])
            p("    frozen[%s] dur=%s ms  covers %d moving events (ids %s)"
              % (fids, f["duration_ms"] if f else "?", len(m["moving"]),
                 ",".join(str(e["excursion_id"]) for e in m["moving"])))
        if len(covers) > 15:
            p("    ... and %d more" % (len(covers) - 15))

    p("")
    p("  CAUTION: this comparison shows where the two detectors' independently drawn")
    p("  boundaries agree, split, or miss. It does not establish which detector (if")
    p("  either) is correct for any one event, and it is not a classifier -- no")
    p("  event here is labelled genuine or spurious.")


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


def plot_excursion_detail(samples, baseline, e, out_path, margin_s,
                          entry_threshold, exit_threshold, capture_label, *,
                          alt_baseline=None, alt_events=None, alt_label=None):
    """The item-7 detail plot. Always shows: raw samples, the primary
    baseline, the primary detector's deviation with entry/exit thresholds
    and open/close boundary, and a full numeric header (duration, peak,
    signed+absolute integrated flux, completeness/gap info, capture name,
    session, sample range, timestamps).

    When alt_baseline is given (an --independent-compare run), ALSO shows
    the other detector's baseline overlaid on the raw trace, a second
    deviation panel against that alt baseline, and the boundaries of every
    entry in alt_events -- 0 or more independently-detected events from
    the OTHER detector that overlap this window, each drawn so a frozen
    event covering several moving events (or vice versa) is visible at a
    glance, not just described in the header."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available -- skipping detail plot", file=sys.stderr)
        return False

    alt_events = alt_events or []
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
    primary_label = e.get("baseline_mode", "primary")

    n_panels = 3 if alt_baseline is not None else 2
    heights = [2, 1, 1] if n_panels == 3 else [2, 1]
    fig, axes = plt.subplots(n_panels, 1, sharex=True, figsize=(10, 3.2 * n_panels + 1.5),
                             gridspec_kw={"height_ratios": heights})
    ax1, ax2 = axes[0], axes[1]
    ax3 = axes[2] if n_panels == 3 else None

    ax1.plot(ts, raw, lw=0.9, color="#1f77b4", label="raw ADC")
    ax1.plot(ts, base, lw=1.2, color="#2ca02c", ls="--", label="baseline (%s)" % primary_label)
    ax1.axvspan(x_lo, x_hi, color="#ff7f0e", alpha=0.15)
    ax1.axvline(x_lo, color="#ff7f0e", lw=1.3, label="%s open" % primary_label)
    ax1.axvline(x_hi, color="#8c564b", lw=1.3, label="%s close" % primary_label)

    if alt_baseline is not None:
        alt_base = [alt_baseline[i] for i in idx_win]
        ax1.plot(ts, alt_base, lw=1.1, color="#9467bd", ls="-.",
                 label="baseline (%s)" % (alt_label or "alt"))
        for k, ae in enumerate(alt_events):
            a_lo = float(ae["start_time_s"]) - t0
            a_hi = float(ae["end_time_s"]) - t0
            ax1.axvspan(a_lo, a_hi, color="#17becf", alpha=0.12)
            ax1.axvline(a_lo, color="#17becf", lw=1.0, ls=":",
                       label=("%s open" % (alt_label or "alt")) if k == 0 else None)
            ax1.axvline(a_hi, color="#0b6b76", lw=1.0, ls=":",
                       label=("%s close" % (alt_label or "alt")) if k == 0 else None)

    ax1.set_ylabel("Hall A raw ADC counts")
    ax1.legend(fontsize=7, loc="best")
    ax1.grid(alpha=0.25)

    ax2.plot(ts, dev, lw=0.9, color="#1f77b4")
    for lvl in (entry_threshold, -entry_threshold):
        ax2.axhline(lvl, color="#999999", lw=0.8, ls=":")
    for lvl in (exit_threshold, -exit_threshold):
        ax2.axhline(lvl, color="#cccccc", lw=0.8, ls="--")
    ax2.axvspan(x_lo, x_hi, color="#ff7f0e", alpha=0.15)
    ax2.axvline(x_lo, color="#ff7f0e", lw=1.3)
    ax2.axvline(x_hi, color="#8c564b", lw=1.3)
    ax2.set_ylabel("dev from\n%s baseline" % primary_label)
    ax2.grid(alpha=0.25)

    if ax3 is not None:
        alt_dev = [samples[i]["raw"] - alt_baseline[i] for i in idx_win]
        ax3.plot(ts, alt_dev, lw=0.9, color="#9467bd")
        for lvl in (entry_threshold, -entry_threshold):
            ax3.axhline(lvl, color="#999999", lw=0.8, ls=":")
        for lvl in (exit_threshold, -exit_threshold):
            ax3.axhline(lvl, color="#cccccc", lw=0.8, ls="--")
        for ae in alt_events:
            a_lo = float(ae["start_time_s"]) - t0
            a_hi = float(ae["end_time_s"]) - t0
            ax3.axvspan(a_lo, a_hi, color="#17becf", alpha=0.15)
            ax3.axvline(a_lo, color="#17becf", lw=1.0, ls=":")
            ax3.axvline(a_hi, color="#0b6b76", lw=1.0, ls=":")
        ax3.set_ylabel("dev from\n%s baseline" % (alt_label or "alt"))
        ax3.grid(alpha=0.25)

    axes[-1].set_xlabel("time (s), window start = %.6f s firmware time" % t0)

    status = "INCOMPLETE" if int(e["incomplete"]) else "COMPLETE"
    if int(e["in_low_pwm_dwell"]):
        status += " / LOW-PWM DWELL (unconfirmed stall/assist proxy)"
    lines = [
        "%s   session %s   %s excursion #%s   [%s]"
        % (capture_label, e["session"], primary_label, e["excursion_id"], status),
        "sample %s..%s   t=%s..%s s   duration=%s ms"
        % (e["start_sample"], e["end_sample"], e["start_time_s"], e["end_time_s"], e["duration_ms"]),
        "peak delta=%s counts (pos=%s neg=%s)   integrated flux: abs=%s signed=%s count*ms"
        % (e["max_abs_flux"], e["max_pos_flux"], e["max_neg_flux"],
           e["integrated_abs_flux_count_ms"], e["integrated_signed_flux_count_ms"]),
        "baseline=%s counts (%s/%s, n_quiet=%s)   gaps within=%s near=%s"
        % (e["baseline_at_excursion"], e["baseline_mode"], e["baseline_method"],
           e["baseline_n_quiet"], e["gaps_within_count"], e["gaps_near_count"]),
    ]
    if alt_events:
        lines.append("  %d independently-detected %s event(s) overlap this window "
                     "(all boundaries plotted; text limited to the first %d):"
                     % (len(alt_events), alt_label or "alt", min(len(alt_events), 6)))
        for ae in alt_events[:6]:
            lines.append(
                "  %s counterpart #%s: sample %s..%s  dur=%s ms  peak=%s  abs_flux=%s  signed_flux=%s"
                % (alt_label or "alt", ae["excursion_id"], ae["start_sample"], ae["end_sample"],
                   ae["duration_ms"], ae["max_abs_flux"],
                   ae["integrated_abs_flux_count_ms"], ae["integrated_signed_flux_count_ms"]))
        if len(alt_events) > 6:
            lines.append("  ... and %d more %s event(s) (see the matches CSV for the full list)"
                         % (len(alt_events) - 6, alt_label or "alt"))
    elif alt_baseline is not None:
        lines.append("  %s: no independently-detected event overlaps this window" % (alt_label or "alt"))
    info = "\n".join(lines)
    fig.suptitle(info, fontsize=8, family="monospace", ha="left", x=0.02)
    fig.tight_layout(rect=[0, 0, 1, 0.80])
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
                    help="SAME-RANGE remeasurement (not independent -- see "
                         "--independent-compare): also evaluate each "
                         "excursion's identical sample range against the "
                         "OTHER baseline mode (adds alt_* CSV columns and a "
                         "summary section)")
    ap.add_argument("--independent-compare", action="store_true",
                    help="run BOTH detectors to completion, fully "
                         "independently, and match their own separately-"
                         "drawn event boundaries (module docstring §8). "
                         "Writes <base>_frozen_events.csv, "
                         "<base>_moving_events.csv, <base>_matches.csv and "
                         "<base>_matching_summary.txt in addition to the "
                         "normal single-mode output")
    ap.add_argument("--long-duration-ms", type=float, default=1000.0,
                    help="principal excursions at/above this duration are "
                         "flagged (not excluded) in the summary as possible "
                         "merged/multi-event excursions for review "
                         "(default 1000)")
    ap.add_argument("--material-boundary-ms", type=float, default=20.0,
                    help="[--independent-compare] a one-to-one matched "
                         "pair's open/close/duration difference at or "
                         "above this (ms) is flagged as a material "
                         "boundary disagreement for review (default 20)")
    ap.add_argument("--settle-metric", choices=("range", "stdev", "slope"),
                    help="EXPERIMENTAL (off unless given with "
                         "--settle-threshold): gate a frozen-mode freeze on "
                         "this measure of the pre-event quiet window. See "
                         "module docstring §9 -- this can suppress "
                         "detection entirely, it is not a drop-in fix")
    ap.add_argument("--settle-threshold", type=float,
                    help="EXPERIMENTAL: threshold for --settle-metric, in "
                         "counts (range/stdev) or counts/sample (slope)")
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
    ap.add_argument("--plot-max-per-category", type=int, default=None,
                    help="OPTIONAL convenience cap on how many excursions to "
                         "plot per category under --plot-categories-dir / "
                         "--plot-review-dir. Default is UNCAPPED -- every "
                         "excursion in every category is plotted, and the "
                         "exact found/plotted counts are always printed. "
                         "Pass a number only to deliberately limit output "
                         "for a quick look; truncation is always logged, "
                         "never silent")
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
    ap.add_argument("--plot-review-dir",
                    help="generate the item-4 review-group plots (long-"
                         "duration >=1s, frozen-covers-multiple-moving, "
                         "materially-differing boundaries, incomplete, "
                         "low-PWM dwell) into subdirectories under this "
                         "path. Requires --independent-compare")
    args = ap.parse_args()

    if args.plot_review_dir and not args.independent_compare:
        print("--plot-review-dir requires --independent-compare", file=sys.stderr)
        return 2

    if args.exit_threshold > args.entry_threshold:
        print("--exit-threshold must be <= --entry-threshold", file=sys.stderr)
        return 2

    result = analyze_captures(
        args.capture, entry_threshold=args.entry_threshold,
        exit_threshold=args.exit_threshold, baseline_mode=args.baseline_mode,
        baseline_window=args.baseline_window, baseline_method=args.baseline_method,
        pre_window=args.pre_window, gap_margin_s=args.gap_margin_s,
        dwell_pwm_max=args.dwell_pwm_max, dwell_min_ms=args.dwell_min_ms,
        compare_baselines=args.compare_baselines,
        settle_metric=args.settle_metric, settle_threshold=args.settle_threshold)
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
    def cap_group(group):
        if args.plot_max_per_category is None:
            return group, len(group)
        return group[:args.plot_max_per_category], args.plot_max_per_category

    if ids:
        os.makedirs(args.plot_dir, exist_ok=True)
        for eid in sorted(ids):
            e = by_id.get(eid)
            ctx = sessions_ctx.get(e["session"]) if e else None
            if ctx is None:
                continue
            out_path = os.path.join(args.plot_dir, "excursion_%04d.png" % eid)
            if plot_excursion_detail(ctx["samples"], ctx["baseline"], e, out_path,
                                     args.plot_margin_s, args.entry_threshold,
                                     args.exit_threshold, label):
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
            capped, _cap = cap_group(group)
            if len(group) > len(capped):
                print("category %-24s : %d found, plotting %d (capped by --plot-max-per-category)"
                     % (cat_name, len(group), len(capped)))
            else:
                print("category %-24s : %d found, plotting %d" % (cat_name, len(group), len(capped)))
            cat_dir = os.path.join(args.plot_categories_dir, cat_name)
            os.makedirs(cat_dir, exist_ok=True)
            for e in capped:
                ctx = sessions_ctx.get(e["session"])
                if ctx is None:
                    continue
                out_path = os.path.join(cat_dir, "excursion_%04d.png" % e["excursion_id"])
                if plot_excursion_detail(ctx["samples"], ctx["baseline"], e, out_path,
                                         args.plot_margin_s, args.entry_threshold,
                                         args.exit_threshold, label):
                    print("  wrote %s" % out_path)

    if args.independent_compare:
        ind = analyze_independent(
            args.capture, entry_threshold=args.entry_threshold,
            exit_threshold=args.exit_threshold, baseline_window=args.baseline_window,
            pre_window=args.pre_window, baseline_method=args.baseline_method,
            gap_margin_s=args.gap_margin_s, dwell_pwm_max=args.dwell_pwm_max,
            dwell_min_ms=args.dwell_min_ms, settle_metric=args.settle_metric,
            settle_threshold=args.settle_threshold)

        frozen_out = base + "_frozen_events.csv"
        with open(frozen_out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=EXCURSION_COLUMNS)
            w.writeheader()
            w.writerows(ind["frozen"])
        print("wrote %s (%d events)" % (frozen_out, len(ind["frozen"])))

        moving_out = base + "_moving_events.csv"
        with open(moving_out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=EXCURSION_COLUMNS)
            w.writeheader()
            w.writerows(ind["moving"])
        print("wrote %s (%d events)" % (moving_out, len(ind["moving"])))

        match_cols = ["kind", "frozen_ids", "moving_ids",
                     "open_time_delta_s", "close_time_delta_s", "duration_delta_ms",
                     "peak_delta_counts", "signed_flux_delta", "abs_flux_delta"]
        matches_out = base + "_matches.csv"
        with open(matches_out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=match_cols)
            w.writeheader()
            for m in ind["matches"]:
                row = {"kind": m["kind"],
                      "frozen_ids": ";".join(str(e["excursion_id"]) for e in m["frozen"]),
                      "moving_ids": ";".join(str(e["excursion_id"]) for e in m["moving"])}
                row.update(m.get("deltas", {}))
                w.writerow(row)
        print("wrote %s (%d match groups)" % (matches_out, len(ind["matches"])))

        match_summary_path = base + "_matching_summary.txt"
        with open(match_summary_path, "w") as fh:
            write_match_summary(ind, args, fh)
        write_match_summary(ind, args, sys.stdout)
        print("wrote %s" % match_summary_path)

        if args.plot_review_dir:
            review = categorize_for_review(
                ind["frozen"], ind["matches"],
                long_ms=args.long_duration_ms, material_ms=args.material_boundary_ms)

            # For each frozen event we plot, find which moving events (if
            # any) share its match group, so the plot can show both.
            moving_for_frozen = {}
            for m in ind["matches"]:
                for fe in m["frozen"]:
                    moving_for_frozen[fe["excursion_id"]] = m["moving"]

            def plot_frozen_group(name, group):
                if not group:
                    print("review %-28s : none found" % name)
                    return
                capped, _cap = cap_group(group)
                if len(group) > len(capped):
                    print("review %-28s : %d found, plotting %d (capped by --plot-max-per-category)"
                         % (name, len(group), len(capped)))
                else:
                    print("review %-28s : %d found, plotting %d" % (name, len(group), len(capped)))
                out_dir = os.path.join(args.plot_review_dir, name)
                os.makedirs(out_dir, exist_ok=True)
                for e in capped:
                    ctx = ind["sessions"].get(e["session"])
                    if ctx is None:
                        continue
                    out_path = os.path.join(out_dir, "excursion_%04d.png" % e["excursion_id"])
                    if plot_excursion_detail(
                            ctx["samples"], ctx["frozen_baseline"], e, out_path,
                            args.plot_margin_s, args.entry_threshold, args.exit_threshold,
                            label, alt_baseline=ctx["moving_baseline"],
                            alt_events=moving_for_frozen.get(e["excursion_id"], []),
                            alt_label="moving"):
                        print("  wrote %s" % out_path)

            for name in ("long_duration_ge_1s", "frozen_covers_multiple_moving",
                        "incomplete", "in_low_pwm_dwell"):
                plot_frozen_group(name, review[name])

            bd = review["boundary_differs_materially"]
            if not bd:
                print("review %-28s : none found" % "boundary_differs_materially")
            else:
                capped, _cap = cap_group(bd)
                if len(bd) > len(capped):
                    print("review %-28s : %d found, plotting %d (capped by --plot-max-per-category)"
                         % ("boundary_differs_materially", len(bd), len(capped)))
                else:
                    print("review %-28s : %d found, plotting %d"
                         % ("boundary_differs_materially", len(bd), len(capped)))
                out_dir = os.path.join(args.plot_review_dir, "boundary_differs_materially")
                os.makedirs(out_dir, exist_ok=True)
                for m in capped:
                    fe = m["frozen"][0]
                    ctx = ind["sessions"].get(fe["session"])
                    if ctx is None:
                        continue
                    out_path = os.path.join(out_dir, "excursion_%04d.png" % fe["excursion_id"])
                    if plot_excursion_detail(
                            ctx["samples"], ctx["frozen_baseline"], fe, out_path,
                            args.plot_margin_s, args.entry_threshold, args.exit_threshold,
                            label, alt_baseline=ctx["moving_baseline"],
                            alt_events=m["moving"], alt_label="moving"):
                        print("  wrote %s" % out_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
