#!/usr/bin/env python3
"""Make orientation plots from extracted numerical passage CSVs.

Plotting only. No boundary or RTB point is inferred or marked.
"""

import argparse
import csv
from pathlib import Path


def read_csv(path):
    with path.open(newline="") as fh:
        return list(csv.DictReader(fh))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("passages", type=Path)
    ap.add_argument("--outdir", type=Path, required=True)
    args = ap.parse_args()
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    files = sorted(args.passages.glob("*.csv"))
    args.outdir.mkdir(parents=True, exist_ok=True)
    for path in files:
        rows = read_csv(path)
        if not rows:
            continue
        xkey = "t_ms"
        x = [float(r[xkey]) for r in rows]
        raw = [float(r["raw"]) for r in rows]
        baseline = [float(r["baseline"]) for r in rows]
        pwm = [float(r["pwm_actual"]) for r in rows]
        fig, (ax, axp) = plt.subplots(2, 1, figsize=(12, 6), sharex=True,
                                      gridspec_kw={"height_ratios": (3, 1)})
        x0 = x[0]
        x = [(v - x0) / 1000.0 for v in x]
        ax.plot(x, raw, lw=0.7, color="#1f77b4", label="Hall value")
        ax.plot(x, baseline, lw=0.9, color="#2ca02c", alpha=0.8, label="recorder baseline")
        ax.set_ylabel("counts")
        ax.set_title(path.stem + " — raw numerical trace; no boundary marked")
        ax.grid(alpha=0.25)
        ax.legend(loc="best", fontsize=8)
        axp.plot(x, pwm, lw=0.8, color="#7f7f7f")
        axp.set_ylabel("PWM")
        axp.set_xlabel("seconds from window start")
        axp.grid(alpha=0.25)
        fig.tight_layout()
        fig.savefig(args.outdir / (path.stem + ".png"), dpi=130)
        plt.close(fig)


if __name__ == "__main__":
    main()
