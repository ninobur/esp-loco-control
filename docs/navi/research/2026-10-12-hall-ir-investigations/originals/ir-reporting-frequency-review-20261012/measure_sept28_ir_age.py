#!/usr/bin/env python3
"""Measure Type-5 receiver-arrival age at September 28 Hall openings."""

from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/ngr-ir-report-matplotlib")
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


ROOT = Path("/Users/davidbrown/NGR/NGR-Files")
ANALYSIS = ROOT / "hall-field-analysis-20260929"
CAPTURE = ANALYSIS / "sources/otto_navi_sync_20260928_101705.nsr"
EVENTS = ANALYSIS / "continuous_hall_analysis.json"
OUTPUT = Path(__file__).resolve().parent


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def stats(values):
    a = np.asarray(list(values), dtype=float)
    return {
        "n": len(a),
        "min": float(a.min()),
        "p05": float(np.quantile(a, 0.05)),
        "p50": float(np.median(a)),
        "p95": float(np.quantile(a, 0.95)),
        "p99": float(np.quantile(a, 0.99)),
        "max": float(a.max()),
        "mean": float(a.mean()),
        "sd": float(a.std(ddof=1)),
    }


def main():
    base = load_module(ANALYSIS / "analyze_continuous_hall.py", "continuous_hall")
    ircal = load_module(ANALYSIS / "analyze_ir_odometry_calibration.py", "ircal")
    hall, *_ = base.load_capture(CAPTURE)
    ir = ircal.load_ir(CAPTURE, base)
    global_offset = ircal.prepare_ir(ir)
    events = json.loads(EVENTS.read_text())["events"]

    records = []
    for event in events:
        hall_us = int(hall["t_us"][int(event["sample_index"])])
        index = int(np.searchsorted(ir["arrival_us"], hall_us, side="right") - 1)
        if index < 0:
            continue
        records.append({
            "event_serial": int(event["event_serial"]),
            "ruling": event.get("ruling"),
            "arrival_age_ms": (hall_us - int(ir["arrival_us"][index])) / 1000.0,
            # Diagnostic only: the global-min offset folds unknown one-way delay
            # and clock drift into one number; it is not a latency measurement.
            "apparent_capture_age_ms": (
                hall_us - int(ir["aligned_global_us"][index])
            ) / 1000.0,
        })

    primary_rulings = {"ADVANCED", "ADVANCED_WITH_DISCREPANCY"}
    primary = [r for r in records if r["ruling"] in primary_rulings]
    secondary = [r for r in records if r["ruling"] == "NON_LANDMARK_HALL"]
    raw_clock_offset_us = ir["arrival_us"] - ir["captured_us"]
    sequence_delta = np.diff(ir["sequence"])
    result = {
        "capture": str(CAPTURE),
        "type5_snapshots": len(ir["sequence"]),
        "type5_capture_interval_ms": stats(np.diff(ir["captured_us"]) / 1000.0),
        "type5_receiver_arrival_interval_ms": stats(np.diff(ir["arrival_us"]) / 1000.0),
        "type5_sequence_gap_count": int(np.sum(sequence_delta > 1)),
        "type5_missing_sequence_positions": int(np.sum(np.maximum(sequence_delta - 1, 0))),
        "raw_arrival_minus_capture_offset_us": stats(raw_clock_offset_us),
        "raw_clock_offset_range_us": int(raw_clock_offset_us.max() - raw_clock_offset_us.min()),
        "definition": "Hall time minus arrival time of latest Type-5 snapshot received at or before Hall time",
        "all_hall_events_arrival_age_ms": stats(r["arrival_age_ms"] for r in records),
        "primary_hall_events_arrival_age_ms": stats(r["arrival_age_ms"] for r in primary),
        "secondary_hall_events_arrival_age_ms": stats(r["arrival_age_ms"] for r in secondary),
        "apparent_capture_age_global_min_alignment_ms": stats(
            r["apparent_capture_age_ms"] for r in records
        ),
        "global_min_clock_offset_us": int(global_offset),
        "clock_alignment_warning": (
            "Apparent capture age is not end-to-end latency: transmitter and receiver clocks "
            "were unsynchronized and their raw offset changed during the run."
        ),
        "counts_over_arrival_age_ms": {
            str(limit): sum(r["arrival_age_ms"] > limit for r in records)
            for limit in (50, 100, 150, 200, 500, 1000)
        },
    }
    (OUTPUT / "sept28_ir_age_metrics.json").write_text(json.dumps(result, indent=2) + "\n")

    plt.style.use("seaborn-v0_8-whitegrid")
    fig, ax = plt.subplots(figsize=(8.2, 4.6))
    bins = np.arange(0, 181, 5)
    ax.hist(
        [r["arrival_age_ms"] for r in primary], bins=bins,
        color="#276FBF", alpha=0.82, label=f"mapped primary openings (n={len(primary)})"
    )
    ax.axvline(np.median([r["arrival_age_ms"] for r in primary]),
               color="#173F5F", linewidth=2, label="primary median 48.4 ms")
    ax.axvline(np.quantile([r["arrival_age_ms"] for r in primary], 0.95),
               color="#C0392B", linewidth=2, linestyle="--", label="primary p95 95.2 ms")
    ax.axvline(100, color="#555555", linewidth=1.5, linestyle=":",
               label="configured Type-5 period 100 ms")
    ax.set(xlabel="Age of last received Type-5 snapshot at Hall opening (ms)",
           ylabel="Opening count",
           title="September 28: receiver-arrival age at mapped Hall openings")
    ax.legend(frameon=True, fontsize=9)
    fig.tight_layout()
    fig.savefig(OUTPUT / "sept28_hall_opening_ir_arrival_age.png", dpi=180)
    plt.close(fig)


if __name__ == "__main__":
    main()
