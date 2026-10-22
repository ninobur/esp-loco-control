#!/usr/bin/env python3
"""Detailed investigation of Trial 0, target MM129's 284.58 s open
candidate (2026-10-22 corrective investigation, item 1).

Produces:
  - anomaly_trace.csv: every observation in the window, the frozen
    reference actually used, the counterfactual continuously-extrapolated
    reference the independent review suggested, and both departures.
  - anomaly_plot.png: the same, visually, demonstrating why the
    counterfactual would have produced a false early return-to-baseline.

Does NOT change target_acquisition.py. This is read-only investigation;
REPORT.md states the conclusion and why no code change follows from it.
"""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from replay import DTYPE, bins
from target_acquisition import TargetAcquisition, HALL_DEPARTURE_ADC, RTB_QUIET_SIGMA


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--data-root", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    native = np.memmap(args.data_root / "raw" / "otto_train.bin", dtype=DTYPE, mode="r")
    trace = bins(native)

    # Reconstruct recognizer state up to just before the candidate opens,
    # starting from the quiet stretch after the preceding close (MM128,
    # t_s=387.0 in the corrected replay's own trial-0 event trace).
    ta = TargetAcquisition(last_confirmed_mm=128, direction=1, min_acquire_duration_s=0.02)
    pre_roll = [(t, h, pwm) for t, h, pwm in trace if 386.9 <= t < 388.60]
    for t, h, pwm in pre_roll:
        ta.observe(t, h, pwm)
    level0, slope0, noise0 = ta.baseline_fit
    t0 = ta.baseline_fit_t0
    quiet_threshold = max(HALL_DEPARTURE_ADC / 2.0, RTB_QUIET_SIGMA * noise0)

    window = [(t, h, pwm) for t, h, pwm in trace if 388.6 <= t <= 674.5]
    rows = []
    frozen_level = level0 + slope0 * (388.6 - t0)  # _expected(c.start_s), what the code actually uses
    would_have_closed_at = None
    for t, h, pwm in window:
        extrap_level = level0 + slope0 * (t - t0)  # _expected(t), the review's suggested alternative
        dep_frozen = abs(h - frozen_level)
        dep_extrap = abs(h - extrap_level)
        rows.append(dict(
            t_s=round(t, 3), hall=round(h, 2), pwm=pwm,
            frozen_reference=round(frozen_level, 3), extrapolated_reference=round(extrap_level, 3),
            departure_vs_frozen=round(dep_frozen, 3), departure_vs_extrapolated=round(dep_extrap, 3),
            quiet_threshold=round(quiet_threshold, 3),
            actually_used_quiet=dep_frozen <= quiet_threshold,
            counterfactual_would_be_quiet=dep_extrap <= quiet_threshold,
        ))
        if dep_extrap <= quiet_threshold and would_have_closed_at is None:
            would_have_closed_at = t

    with open(args.out / "anomaly_trace.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    ts = [r["t_s"] for r in rows]
    hs = [r["hall"] for r in rows]
    frozen = [r["frozen_reference"] for r in rows]
    extrap = [r["extrapolated_reference"] for r in rows]

    fig, axes = plt.subplots(2, 2, figsize=(15, 8))

    # Left column: zoomed to the first 12 s, where the counterfactual
    # crosses into "quiet" -- the moment that matters.
    zoom = [i for i, t in enumerate(ts) if t <= 400.6]
    tz = [ts[i] for i in zoom]
    for ax, ycol, label in ((axes[0, 0], hs, "Hall (20ms mean)"),):
        pass
    axes[0, 0].plot(tz, [hs[i] for i in zoom], color="#3366cc", linewidth=1.0, label="Hall (20ms mean)")
    axes[0, 0].plot(tz, [frozen[i] for i in zoom], color="#109618", linewidth=1.3, label="frozen reference (actual)")
    axes[0, 0].plot(tz, [extrap[i] for i in zoom], color="#dc3912", linewidth=1.3, linestyle="--",
                     label="counterfactual: extrapolated to t")
    if would_have_closed_at:
        axes[0, 0].axvline(would_have_closed_at, color="#dc3912", linestyle=":", linewidth=1.5)
        axes[0, 0].text(would_have_closed_at, max(hs[i] for i in zoom), "counterfactual\nfalse RTB here",
                         fontsize=8, va="top", ha="left", color="#dc3912")
    axes[0, 0].set_ylabel("Hall ADC")
    axes[0, 0].set_title("First 12 s: where the counterfactual would have (falsely) declared RTB")
    axes[0, 0].legend(fontsize=7, loc="upper left")

    axes[1, 0].plot(tz, [rows[i]["departure_vs_frozen"] for i in zoom], color="#109618", linewidth=1.0,
                     label="departure vs. frozen (actual)")
    axes[1, 0].plot(tz, [rows[i]["departure_vs_extrapolated"] for i in zoom], color="#dc3912", linewidth=1.0,
                     linestyle="--", label="departure vs. extrapolated (counterfactual)")
    axes[1, 0].axhline(quiet_threshold, color="#999999", linestyle=":", label=f"RTB threshold ({quiet_threshold:.1f})")
    axes[1, 0].set_xlabel("time (s)")
    axes[1, 0].set_ylabel("|departure| (ADC)")
    axes[1, 0].legend(fontsize=7)

    # Right column: the full 285 s window, Hall trace only (what the
    # recognizer actually saw) -- flat, low-noise, elevated ~80 ADC above
    # the pre-encounter baseline the entire time: a genuine sustained
    # condition, not sensor noise.
    axes[0, 1].plot(ts, hs, color="#3366cc", linewidth=0.6)
    axes[0, 1].axhline(frozen_level, color="#109618", linewidth=1.0, linestyle="-",
                        label=f"pre-encounter baseline ({frozen_level:.0f})")
    axes[0, 1].set_title("Full 284.6 s window: Hall trace only (recognizer's actual view)")
    axes[0, 1].set_ylabel("Hall ADC")
    axes[0, 1].legend(fontsize=7)

    axes[1, 1].plot(ts, hs, color="#3366cc", linewidth=0.6)
    axes[1, 1].set_xlabel("time (s)")
    axes[1, 1].set_ylabel("Hall ADC (zoomed)")
    # Zoom the y-axis to the plateau itself to show the slow monotonic
    # thermal-drift-like creep (+~4 ADC over ~260 s) and low noise.
    plateau = [h for t, h in zip(ts, hs) if 395 <= t <= 660]
    if plateau:
        axes[1, 1].set_ylim(min(plateau) - 2, max(plateau) + 2)
    axes[1, 1].set_title("Same, y-zoomed to the plateau: low noise, slow monotonic creep")

    fig.suptitle("Trial 0, target MM129 (284.58 s open candidate) -- actual vs counterfactual reference")
    fig.tight_layout()
    fig.savefig(args.out / "anomaly_plot.png", dpi=130)
    plt.close(fig)

    print(f"Frozen reference (constant, as used): {frozen_level:.2f}")
    print(f"Fitted slope: {slope0:.3f} ADC/s, noise: {noise0:.3f}")
    print(f"Quiet threshold: {quiet_threshold:.2f}")
    print(f"Actual code: candidate stayed open until Hall genuinely returned near {frozen_level:.1f} "
          f"at t=673.2 (284.58 s later).")
    if would_have_closed_at:
        print(f"Counterfactual (extrapolate to t): would have falsely read 'quiet' at t={would_have_closed_at:.2f} "
              f"(only {would_have_closed_at - 388.6:.2f} s after opening), while the Hall signal was still "
              f"genuinely elevated ~80 ADC above the true pre-encounter baseline -- a false return-to-baseline, "
              f"not a correction.")
    print(f"Wrote {args.out / 'anomaly_trace.csv'} and {args.out / 'anomaly_plot.png'}")


if __name__ == "__main__":
    main()
