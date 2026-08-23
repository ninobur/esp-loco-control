#!/usr/bin/env python3
"""hwt_plot.py — plot a decoded HALL_WAVEFORM_TEST run.

INVESTIGATORY / UNAPPROVED. Diagnostic tooling only.

    python3 tools/hwt_decode.py run.hwt -o run.csv
    python3 tools/hwt_plot.py run.csv --save run.png
    python3 tools/hwt_plot.py run.csv --start 120 --duration 5

Draws both Hall channels against firmware time (channel B only when it is
present in the data), with the motor PWM underneath, operator anchors as
labelled vertical lines, and every discontinuity shaded and named.

Deliberately dumb: no smoothing, no filtering, no peak finding, no event
marking beyond the annotation the firmware already carried. It shows what was
recorded. Judging it is a separate job, done offline, by a person.
"""

import argparse
import csv
import sys


def load(path):
    """Rows in file order. A hole in the sample sequence becomes an explicit
    break (a None row): the trace must never be drawn straight across samples
    that do not exist."""
    samples, anchors, breaks = [], [], []
    prev_seq = None
    with open(path, newline="") as fh:
        for r in csv.DictReader(fh):
            t = r["row_type"]
            if t == "SAMPLE":
                seq = int(r["phys_sample_seq"])
                if prev_seq is not None and seq != prev_seq + 1:
                    samples.append(None)          # break the line over the hole
                prev_seq = seq
                samples.append((
                    float(r["phys_t_s"]),
                    int(r["phys_ch0_raw"]) if r["phys_ch0_raw"] else None,
                    int(r["phys_ch1_raw"]) if r["phys_ch1_raw"] else None,
                    int(r["ctl_pwm_actual"]) if r["ctl_pwm_actual"] else 0,
                    r["ann_ch0"], int(r["phys_late"] or 0),
                ))
            elif t == "ANCHOR":
                anchors.append((float(r["phys_t_s"]) if r["phys_t_s"] else None,
                                r["op_text"]))
            elif t in ("GAP", "MISSED", "SESSION", "DROP"):
                last = next((x for x in reversed(samples) if x), None)
                breaks.append((t, last[0] if last else 0.0, r["info"]))
                # The decoder already named this hole, so break the trace here
                # rather than waiting for a sequence jump that will not come.
                if samples and samples[-1] is not None:
                    samples.append(None)
                prev_seq = None
    return samples, anchors, breaks


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="CSV from hwt_decode.py")
    ap.add_argument("--start", type=float, help="window start, firmware seconds")
    ap.add_argument("--duration", type=float, help="window length, seconds")
    ap.add_argument("--save", help="write a PNG instead of opening a window")
    ap.add_argument("--annotate", action="store_true",
                    help="mark samples the OLD Module C threshold would have "
                         "called N or S (annotation only — not detections)")
    args = ap.parse_args()

    try:
        import matplotlib
        if args.save:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is required:  pip install matplotlib", file=sys.stderr)
        return 1

    samples, anchors, breaks = load(args.csv)
    if not samples:
        print("no SAMPLE rows in %s" % args.csv, file=sys.stderr)
        return 1

    t0 = next(x for x in samples if x)[0]
    lo = args.start if args.start is not None else 0.0
    hi = lo + args.duration if args.duration else float("inf")
    win = [s for s in samples if s is None or lo <= (s[0] - t0) <= hi]
    while win and win[0] is None:
        win.pop(0)
    if not any(win):
        print("window %.3f..%.3f s contains no samples" % (lo, hi), file=sys.stderr)
        return 1

    # A None entry becomes a NaN, which matplotlib renders as a gap in the
    # line rather than a straight segment across missing data.
    nan = float("nan")
    ts = [(s[0] - t0) if s else nan for s in win]
    ch0 = [s[1] if s else nan for s in win]
    have_b = any(s and s[2] is not None for s in win)
    ch1 = [(s[2] if s else nan) for s in win] if have_b else None
    pwm = [s[3] if s else nan for s in win]

    fig, (ax, axp) = plt.subplots(2, 1, sharex=True, figsize=(13, 7),
                                  gridspec_kw={"height_ratios": [3, 1]})

    ax.plot(ts, ch0, lw=0.8, color="#1f77b4", label="Hall A (GPIO 33) raw ADC")
    if have_b:
        ax.plot(ts, ch1, lw=0.8, color="#d62728", label="Hall B raw ADC")
    if args.annotate:
        for pole, colour in (("N", "#2ca02c"), ("S", "#9467bd")):
            xs = [t for t, s in zip(ts, win) if s and s[4] == pole]
            ys = [s[1] for s in win if s and s[4] == pole]
            if xs:
                ax.plot(xs, ys, ".", ms=2, color=colour,
                        label="old Module C rule would say %s" % pole)

    for kind, t, info in breaks:
        x = t - t0
        if lo <= x <= hi:
            colour = {"GAP": "#d62728", "MISSED": "#ff7f0e",
                      "SESSION": "#000000", "DROP": "#8c564b"}[kind]
            ax.axvline(x, color=colour, lw=1.2, ls="--", alpha=0.8)
            ax.annotate(kind, (x, ax.get_ylim()[1]), color=colour, fontsize=7,
                        rotation=90, va="top")

    for at, text in anchors:
        if at is None:
            continue
        x = at - t0
        if lo <= x <= hi:
            ax.axvline(x, color="#111111", lw=1.0, alpha=0.6)
            ax.annotate(text, (x, ax.get_ylim()[0]), fontsize=8, rotation=90,
                        va="bottom", color="#111111")

    ax.set_ylabel("raw ADC counts (12-bit)")
    ax.set_title("HALL_WAVEFORM_TEST — %s   INVESTIGATORY, UNAPPROVED\n"
                 "vertical black lines are operator anchors: the only ground truth here"
                 % args.csv, fontsize=10)
    ax.legend(loc="upper right", fontsize=8)
    ax.grid(alpha=0.25)

    axp.plot(ts, pwm, lw=0.9, color="#7f7f7f")
    axp.set_ylabel("motor PWM")
    axp.set_xlabel("firmware time (s from first sample in file)")
    axp.grid(alpha=0.25)

    fig.tight_layout()
    if args.save:
        fig.savefig(args.save, dpi=130)
        print("wrote %s (%d samples plotted, %d break(s) left open)"
              % (args.save, sum(1 for s in win if s), sum(1 for s in win if not s)))
    else:
        plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
