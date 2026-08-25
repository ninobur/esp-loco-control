#!/usr/bin/env python3
"""qt_plot.py — plot a decoded QUORUM TRACE run.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

    python3 tools/qt_decode.py run.qtcap -o run.csv
    python3 tools/qt_plot.py run.csv --save run.png
    python3 tools/qt_plot.py run.csv --start 12 --duration 5

Draws, on one timeline: the averaged Hall input QUORUM itself computed
(phys_raw), its adaptive baseline, the four amplitude thresholds
reconstructed from baseline + the compile-time deadband/entry-margin
(carried once in a STATUS row -- see README_TRACE.md for why these are not
repeated on every SAMPLE), shaded event-open spans, every DECISION record
as a labelled vertical line, and every operator ANCHOR (ngr/loco/<id>/
cmd/trace_anchor) as a distinct full-height labelled line, so a claimed
landmark can be checked by eye against what QUORUM was doing at that
instant. Motor PWM underneath, same as hwt_plot.py.

Deliberately dumb: no smoothing, no re-detection, no judgement of whether a
decision was correct. It shows what QUORUM recorded about itself. Judging it
is a separate job, done offline, by a person.
"""

import argparse
import csv
import sys


def load(path):
    samples, decisions, anchors, breaks = [], [], [], []
    thresholds = None   # (deadband, entry_margin) from the first STATUS row seen
    prev_seq = None
    with open(path, newline="") as fh:
        for r in csv.DictReader(fh):
            t = r["row_type"]
            if t == "SAMPLE":
                seq = int(r["phys_sample_seq"])
                if prev_seq is not None and seq != prev_seq + 1:
                    samples.append(None)
                prev_seq = seq
                samples.append((
                    float(r["t_ms"]) / 1000.0,
                    int(r["phys_raw"]), int(r["phys_baseline"]),
                    int(r["det_event_active"]),
                    int(r["ctl_pwm_actual"]) if r["ctl_pwm_actual"] else 0,
                ))
            elif t == "DECISION":
                decisions.append((float(r["t_ms"]) / 1000.0, r["dec_kind"],
                                  r["dec_quorum_event"], r["dec_nav_mm_after"],
                                  r["dec_observed_polarity"], r["dec_expected_polarity"]))
            elif t == "ANCHOR":
                anchors.append((float(r["t_ms"]) / 1000.0, r["op_anchor_id"],
                                r["op_sample_seq"], r["op_text"]))
            elif t == "STATUS" and thresholds is None:
                import re
                m1 = re.search(r"deadband=(\d+)", r["info"])
                m2 = re.search(r"entry_margin=(\d+)", r["info"])
                if m1 and m2:
                    thresholds = (int(m1.group(1)), int(m2.group(1)))
            elif t in ("GAP", "SESSION"):
                last = next((x for x in reversed(samples) if x), None)
                breaks.append((t, last[0] if last else 0.0, r["info"]))
                if samples and samples[-1] is not None:
                    samples.append(None)
                prev_seq = None
    return samples, decisions, anchors, breaks, thresholds


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="CSV from qt_decode.py")
    ap.add_argument("--start", type=float, help="window start, seconds")
    ap.add_argument("--duration", type=float, help="window length, seconds")
    ap.add_argument("--save", help="write a PNG instead of opening a window")
    ap.add_argument("--deadband", type=int, help="override HALL_DEADBAND_COUNTS "
                    "(default: read from the capture's own STATUS record)")
    ap.add_argument("--entry-margin", type=int, help="override HALL_ENTRY_MARGIN_COUNTS")
    args = ap.parse_args()

    try:
        import matplotlib
        if args.save:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is required:  pip install matplotlib", file=sys.stderr)
        return 1

    samples, decisions, anchors, breaks, thresholds = load(args.csv)
    if not samples:
        print("no SAMPLE rows in %s" % args.csv, file=sys.stderr)
        return 1

    deadband = args.deadband if args.deadband is not None else (thresholds[0] if thresholds else None)
    entry_margin = args.entry_margin if args.entry_margin is not None else (thresholds[1] if thresholds else None)
    if deadband is None or entry_margin is None:
        print("warning: no STATUS row in this capture and no --deadband/--entry-margin "
              "given -- thresholds will not be drawn", file=sys.stderr)

    t0 = next(x for x in samples if x)[0]
    lo = args.start if args.start is not None else 0.0
    hi = lo + args.duration if args.duration else float("inf")
    win = [s for s in samples if s is None or lo <= (s[0] - t0) <= hi]
    while win and win[0] is None:
        win.pop(0)
    if not any(win):
        print("window %.3f..%.3f s contains no samples" % (lo, hi), file=sys.stderr)
        return 1

    nan = float("nan")
    ts = [(s[0] - t0) if s else nan for s in win]
    raw = [s[1] if s else nan for s in win]
    baseline = [s[2] if s else nan for s in win]
    active = [s[3] if s else 0 for s in win]
    pwm = [s[4] if s else nan for s in win]

    fig, (ax, axp) = plt.subplots(2, 1, sharex=True, figsize=(13, 7),
                                  gridspec_kw={"height_ratios": [3, 1]})

    ax.plot(ts, raw, lw=0.8, color="#1f77b4", label="QUORUM's own averaged Hall value")
    ax.plot(ts, baseline, lw=1.0, color="#2ca02c", label="adaptive baseline", alpha=0.8)
    if deadband is not None and entry_margin is not None:
        north_enter = [b + deadband + entry_margin if b == b else nan for b in baseline]
        north_exit  = [b + deadband if b == b else nan for b in baseline]
        south_exit  = [b - deadband if b == b else nan for b in baseline]
        south_enter = [b - deadband - entry_margin if b == b else nan for b in baseline]
        for series, style, label in (
                (north_enter, "--", "north enter"), (north_exit, ":", "north exit"),
                (south_exit, ":", "south exit"), (south_enter, "--", "south enter")):
            ax.plot(ts, series, style, lw=0.7, color="#7f7f7f", alpha=0.7)
        ax.plot([], [], "--", color="#7f7f7f", label="thresholds (baseline ± deadband[±entry margin])")

    # Shade event-open spans.
    in_event, start_t = False, None
    for x, a in zip(ts, active):
        if a and not in_event:
            in_event, start_t = True, x
        elif not a and in_event:
            in_event = False
            if lo <= start_t <= hi:
                ax.axvspan(start_t, x, color="#ff7f0e", alpha=0.15)
    if in_event and start_t is not None:
        ax.axvspan(start_t, ts[-1], color="#ff7f0e", alpha=0.15)

    for kind, t, info in breaks:
        x = t - t0
        if lo <= x <= hi:
            colour = {"GAP": "#d62728", "SESSION": "#000000"}[kind]
            ax.axvline(x, color=colour, lw=1.2, ls="--", alpha=0.8)
            ax.annotate(kind, (x, ax.get_ylim()[1]), color=colour, fontsize=7, rotation=90, va="top")

    for x, kind, qev, mm, obs, exp in decisions:
        xx = x - t0
        if not (lo <= xx <= hi):
            continue
        label = qev if kind == "QUORUM_EVENT" else kind
        colour = ("#d62728" if kind == "DISAGREE" or "REJECT" in qev or qev == "NO_QUORUM"
                  else "#2ca02c" if kind == "AGREE" or qev == "QUORUM_ADOPTED"
                  else "#9467bd")
        ax.axvline(xx, color=colour, lw=0.8, alpha=0.5)
        ax.annotate("%s%s" % (label, (" mm=%s" % mm) if mm else ""), (xx, ax.get_ylim()[0]),
                    fontsize=6, rotation=90, va="bottom", color=colour)

    # Operator anchors: drawn distinctly from DECISION lines (solid, opaque,
    # full-height, its own colour) since an anchor is ground truth an
    # operator stated, not something QUORUM itself decided.
    ANCHOR_COLOUR = "#000000"
    anchors_in_window = 0
    for x, aid, sseq, text in anchors:
        xx = x - t0
        if not (lo <= xx <= hi):
            continue
        anchors_in_window += 1
        ax.axvline(xx, color=ANCHOR_COLOUR, lw=1.4, alpha=0.9, zorder=5)
        ax.annotate("ANCHOR #%s (seq %s) \"%s\"" % (aid, sseq, text),
                    (xx, ax.get_ylim()[1]), color=ANCHOR_COLOUR, fontsize=7,
                    rotation=90, va="top", ha="right", zorder=5)
    if anchors_in_window:
        ax.plot([], [], color=ANCHOR_COLOUR, lw=1.4, label="operator anchor")

    ax.set_ylabel("counts (QUORUM's own averaged reading)")
    ax.set_title("QUORUM TRACE — %s   INVESTIGATORY, UNAPPROVED, NO NAVIGATION AUTHORITY\n"
                 "shaded = event open; vertical lines = navigation decisions"
                 % args.csv, fontsize=10)
    ax.legend(loc="upper right", fontsize=7)
    ax.grid(alpha=0.25)

    axp.plot(ts, pwm, lw=0.9, color="#7f7f7f")
    axp.set_ylabel("motor PWM")
    axp.set_xlabel("time (s from first sample in file)")
    axp.grid(alpha=0.25)

    decisions_in_window = sum(1 for x, *_ in decisions if lo <= (x - t0) <= hi)
    fig.tight_layout()
    if args.save:
        fig.savefig(args.save, dpi=130)
        print("wrote %s (%d samples, %d decisions, %d anchors plotted)"
              % (args.save, sum(1 for s in win if s), decisions_in_window, anchors_in_window))
    else:
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
