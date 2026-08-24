#!/usr/bin/env python3
"""hwt_continuity_population_report.py — descriptive report of
det_continuity_ratio across the COMPLETE candidate-event population of one
or more hwt_gate_replay.py decisions.csv files.

INVESTIGATORY, DIAGNOSTIC ONLY. This tool selects NO threshold and makes NO
accept/reject recommendation. Its only purpose is requirement 10 of the
continuity correction: look at how continuity_ratio actually distributes
across every candidate event (not two hand-picked examples), broken out by
the disposition each event received from the corrected pipeline (which does
not use continuity at all -- so grouping by disp_final here is not
circular). Do not read a separation between groups as validation of the
metric: disp_final already reflects duration, flux, timing, return-response
and polarity, all of which are plausibly correlated with continuity_ratio
for physical reasons (e.g. spikes are both short AND choppy), so any
apparent separation is consistent with continuity being redundant with
those gates rather than independently informative. Establishing that would
need the events held out and tested, not eyeballed off this table.

    python3 tools/hwt_continuity_population_report.py \\
        .../grillers/decisions.csv .../pwm40_run/decisions.csv .../pwm90/decisions.csv \\
        --out population_report.csv
"""
import argparse
import csv
import os
import sys


def stats(vals):
    if not vals:
        return {"n": 0}
    vals = sorted(vals)
    n = len(vals)
    mean = sum(vals) / n
    var = sum((v - mean) ** 2 for v in vals) / n if n > 1 else 0.0

    def pct(p):
        return vals[min(n - 1, int(p * n))]
    return {"n": n, "mean": mean, "stdev": var ** 0.5, "min": vals[0], "max": vals[-1],
            "p5": pct(0.05), "p25": pct(0.25), "p50": pct(0.50), "p75": pct(0.75), "p95": pct(0.95)}


def load(path):
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("decisions_csv", nargs="+", help="one or more hwt_gate_replay.py decisions.csv files")
    ap.add_argument("--out", help="CSV of every (capture, disposition) group's stats")
    args = ap.parse_args()

    rows_out = []
    all_rows_all_captures = []

    for path in args.decisions_csv:
        label = os.path.basename(os.path.dirname(os.path.abspath(path))) or path
        rows = load(path)
        all_rows_all_captures.extend(rows)
        print("=== %s (%d candidate events) ===" % (label, len(rows)))

        by_disp = {}
        for r in rows:
            by_disp.setdefault(r["disp_final"], []).append(float(r["det_continuity_ratio"]))

        print("  %-32s %8s %8s %8s %8s %8s %8s %8s" %
              ("disposition", "n", "mean", "stdev", "p5", "p50", "p95", "max"))
        for disp in sorted(by_disp, key=lambda d: -len(by_disp[d])):
            st = stats(by_disp[disp])
            print("  %-32s %8d %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f" %
                  (disp, st["n"], st["mean"], st["stdev"], st["p5"], st["p50"], st["p95"], st["max"]))
            r = {"capture": label, "disposition": disp}
            r.update(st)
            rows_out.append(r)

        st_all = stats([float(r["det_continuity_ratio"]) for r in rows])
        print("  %-32s %8d %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f" %
              ("(all dispositions)", st_all["n"], st_all["mean"], st_all["stdev"],
               st_all["p5"], st_all["p50"], st_all["p95"], st_all["max"]))
        r = {"capture": label, "disposition": "(all)"}
        r.update(st_all)
        rows_out.append(r)
        print()

    print("=== combined across all %d captures (%d candidate events total) ===" %
          (len(args.decisions_csv), len(all_rows_all_captures)))
    by_disp_all = {}
    for r in all_rows_all_captures:
        by_disp_all.setdefault(r["disp_final"], []).append(float(r["det_continuity_ratio"]))
    print("  %-32s %8s %8s %8s %8s %8s %8s %8s" %
          ("disposition", "n", "mean", "stdev", "p5", "p50", "p95", "max"))
    for disp in sorted(by_disp_all, key=lambda d: -len(by_disp_all[d])):
        st = stats(by_disp_all[disp])
        print("  %-32s %8d %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f" %
              (disp, st["n"], st["mean"], st["stdev"], st["p5"], st["p50"], st["p95"], st["max"]))
        r = {"capture": "(combined)", "disposition": disp}
        r.update(st)
        rows_out.append(r)

    print()
    print("No threshold is proposed by this report. Any apparent gap or overlap between")
    print("groups above is descriptive only -- see the module docstring for why grouping")
    print("by disp_final cannot, by itself, validate continuity_ratio as a gate.")

    if args.out:
        cols = ["capture", "disposition", "n", "mean", "stdev", "min", "max", "p5", "p25", "p50", "p75", "p95"]
        with open(args.out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(rows_out)
        print("\nwrote %s (%d rows)" % (args.out, len(rows_out)))

    return 0


if __name__ == "__main__":
    sys.exit(main())
