#!/usr/bin/env python3
"""Plot raw Hall conversions for the requested September 28 event pairs.

Research output only.  Distance is reconstructed by interpolating the IR
recorder's capture-time-aligned cumulative nominal distance at each Hall
sample time.  No smoothing or magnetic-field model is applied.
"""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


PAIRS = ((254, 255), (256, 257))
RAW_COLORS = ("#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd")
EVENT_COLORS = ("#00897b", "#c2185b")


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def longest_true_run(mask: np.ndarray) -> tuple[int | None, int | None]:
    """Return the inclusive endpoints of the longest True run."""
    best = (None, None)
    start = None
    for index, value in enumerate(np.r_[mask, False]):
        if value and start is None:
            start = index
        elif not value and start is not None:
            if best[0] is None or index - start > best[1] - best[0] + 1:
                best = (start, index - 1)
            start = None
    return best


def format_event(event: dict) -> str:
    return (
        f"E{event['event_serial']}  median {event['raw_open']}  "
        f"ref {event['base_open']}  Δ {int(event['raw_open']) - int(event['base_open']):+d}"
    )


def plot_pair(
    output_dir: Path,
    hall: dict,
    hall_position_mm: np.ndarray,
    first: dict,
    second: dict,
) -> dict:
    first_i = int(first["sample_index"])
    second_i = int(second["sample_index"])
    first_position = float(hall_position_mm[first_i])
    second_position = float(hall_position_mm[second_i])
    distance = second_position - first_position

    requested_lo = first_position - 50.0
    requested_hi = second_position + 50.0
    lo = int(np.searchsorted(hall_position_mm, requested_lo, side="left"))
    hi = int(np.searchsorted(hall_position_mm, requested_hi, side="right"))
    lo = max(0, lo)
    hi = min(len(hall_position_mm), hi)
    sample_slice = slice(lo, hi)

    x = hall_position_mm[sample_slice] - first_position
    t = (hall["t_us"][sample_slice].astype(float) - float(hall["t_us"][first_i])) / 1e6
    raw = hall["raw"][sample_slice].astype(float)
    median = hall["median"][sample_slice].astype(float)

    between = slice(first_i, second_i + 1)
    between_raw = hall["raw"][between].astype(int)
    between_median = hall["median"][between].astype(int)
    between_positions = hall_position_mm[between]
    between_times = hall["t_us"][between].astype(float)
    reference = int(first["base_open"])
    references_equal = reference == int(second["base_open"])

    # The two references are equal in these records.  For the numerical return
    # tests, use that shared value; both reference definitions are still drawn.
    median_in_baseline = np.abs(between_median - reference) <= 25
    raw_in_baseline = np.abs(between_raw - reference) <= 25
    median_below_open = np.abs(between_median - reference) < 70
    below_start, below_stop = longest_true_run(median_below_open)
    base_start, base_stop = longest_true_run(median_in_baseline)

    if below_start is None:
        below_run_samples = 0
        below_run_ms = 0.0
        below_run_mm = 0.0
    else:
        below_run_samples = below_stop - below_start + 1
        below_run_ms = float(
            (between_times[below_stop] - between_times[below_start]) / 1000.0
        )
        below_run_mm = float(
            between_positions[below_stop] - between_positions[below_start]
        )
    if base_start is None:
        baseline_run_samples = 0
        baseline_run_ms = 0.0
        baseline_run_mm = 0.0
    else:
        baseline_run_samples = base_stop - base_start + 1
        baseline_run_ms = float(
            (between_times[base_stop] - between_times[base_start]) / 1000.0
        )
        baseline_run_mm = float(
            between_positions[base_stop] - between_positions[base_start]
        )

    fig, axis = plt.subplots(figsize=(14, 7.2))
    for conversion in range(5):
        axis.plot(
            x,
            raw[:, conversion],
            color=RAW_COLORS[conversion],
            linewidth=0.72,
            alpha=0.64,
            label=f"ADC conversion {conversion + 1}",
            zorder=3,
        )
    axis.plot(x, median, color="#111111", linewidth=1.45, label="1 kHz median", zorder=5)

    # Plot both events' independently recorded reference, threshold, and return
    # band.  Equal values deliberately coincide; dash patterns preserve identity.
    dashes = ((6, 3), (2, 2))
    for event_number, event in enumerate((first, second)):
        ref = int(event["base_open"])
        color = EVENT_COLORS[event_number]
        label_prefix = f"E{event['event_serial']}"
        axis.axhspan(
            ref - 25,
            ref + 25,
            color=color,
            alpha=0.075,
            label=f"{label_prefix} ±25 baseline-return band",
            zorder=0,
        )
        axis.axhline(
            ref,
            color=color,
            linewidth=1.15,
            dashes=dashes[event_number],
            label=f"{label_prefix} reference = {ref}",
            zorder=2,
        )
        for sign in (-1, 1):
            axis.axhline(
                ref + sign * 70,
                color=color,
                linewidth=0.95,
                alpha=0.9,
                dashes=dashes[event_number],
                label=f"{label_prefix} ±70 thresholds" if sign == 1 else None,
                zorder=2,
            )

    for event_number, (event, event_x) in enumerate(((first, 0.0), (second, distance))):
        color = EVENT_COLORS[event_number]
        axis.axvline(event_x, color=color, linewidth=1.6, zorder=4)
        axis.scatter(
            [event_x],
            [int(event["raw_open"])],
            s=55,
            facecolor="white",
            edgecolor=color,
            linewidth=1.8,
            zorder=7,
        )
        horizontal_alignment = "left" if event_number == 0 else "right"
        offset = 2.0 if event_number == 0 else -2.0
        axis.annotate(
            format_event(event),
            xy=(event_x, int(event["raw_open"])),
            xytext=(event_x + offset, int(event["raw_open"]) + 22),
            ha=horizontal_alignment,
            va="bottom",
            fontsize=9,
            color=color,
            arrowprops=dict(arrowstyle="-", color=color, linewidth=0.9),
        )

    # Keep exactly the requested spatial coverage even when interpolation makes
    # several Hall samples share one IR position.
    axis.set_xlim(-50.0, distance + 50.0)
    plotted_min = float(np.nanmin(raw))
    plotted_max = float(np.nanmax(raw))
    refs = (int(first["base_open"]), int(second["base_open"]))
    axis.set_ylim(min(plotted_min, min(refs) - 82) - 8, max(plotted_max, max(refs) + 82) + 8)
    axis.set_xlabel(f"Distance from E{first['event_serial']} Hall opening (mm)")
    axis.set_ylabel("Raw Hall ADC count")
    axis.set_title(
        f"September 28 continuous Hall record: MM{int(first['mm']):03d}→MM{int(second['mm']):03d}, "
        f"E{first['event_serial']} → E{second['event_serial']}\n"
        f"capture-time-aligned opening distance {distance:.3f} mm"
    )
    axis.grid(True, alpha=0.16)

    # The top reference maps selected elapsed-time ticks back onto the same
    # distance positions.  This avoids implying constant speed.
    top = axis.twiny()
    top.set_xlim(axis.get_xlim())
    tick_lo = np.ceil(float(np.min(t)) / 0.2) * 0.2
    tick_hi = np.floor(float(np.max(t)) / 0.2) * 0.2
    time_ticks = np.arange(tick_lo, tick_hi + 0.0001, 0.2)
    time_ticks[np.isclose(time_ticks, 0.0, atol=1e-10)] = 0.0
    distance_ticks = np.interp(time_ticks, t, x)
    visible = (distance_ticks >= axis.get_xlim()[0]) & (distance_ticks <= axis.get_xlim()[1])
    top.set_xticks(distance_ticks[visible])
    top.set_xticklabels([f"{value:.1f}" for value in time_ticks[visible]])
    top.set_xlabel(f"Elapsed time from E{first['event_serial']} opening (s)")

    event_handles, event_labels = axis.get_legend_handles_labels()
    # Suppress repeated coincident negative-threshold labels while retaining all
    # five conversion names and the independently recorded event definitions.
    axis.legend(
        event_handles,
        event_labels,
        loc="lower center",
        bbox_to_anchor=(0.5, -0.31),
        ncol=4,
        fontsize=8,
        frameon=False,
    )
    note = (
        "No smoothing. Lines join consecutive 1 kHz records. "
        "Distance uses linear interpolation of capture-time-aligned IR cumulative distance."
    )
    fig.text(0.5, 0.015, note, ha="center", fontsize=8.5, color="#444444")
    fig.subplots_adjust(left=0.075, right=0.985, top=0.84, bottom=0.31)
    plot_path = output_dir / f"events_{first['event_serial']}_{second['event_serial']}_raw_adc.png"
    fig.savefig(plot_path, dpi=190, bbox_inches="tight")
    plt.close(fig)

    return {
        "from_event": int(first["event_serial"]),
        "to_event": int(second["event_serial"]),
        "from_mm": int(first["mm"]),
        "to_mm": int(second["mm"]),
        "opening_distance_mm": float(distance),
        "opening_elapsed_ms": float((hall["t_us"][second_i] - hall["t_us"][first_i]) / 1000.0),
        "mean_speed_mm_s": float(distance / ((hall["t_us"][second_i] - hall["t_us"][first_i]) / 1e6)),
        "from_reference_adc": int(first["base_open"]),
        "to_reference_adc": int(second["base_open"]),
        "references_equal": references_equal,
        "between_samples_inclusive": int(second_i - first_i + 1),
        "median_min_adc": int(np.min(between_median)),
        "median_max_adc": int(np.max(between_median)),
        "median_min_departure_adc": int(np.min(between_median - reference)),
        "median_max_departure_adc": int(np.max(between_median - reference)),
        "raw_min_adc": int(np.min(between_raw)),
        "raw_max_adc": int(np.max(between_raw)),
        "median_values_in_baseline_band": int(np.count_nonzero(median_in_baseline)),
        "raw_conversion_values_in_baseline_band": int(np.count_nonzero(raw_in_baseline)),
        "sample_records_with_any_raw_conversion_in_baseline_band": int(np.count_nonzero(np.any(raw_in_baseline, axis=1))),
        "minimum_absolute_median_departure_adc": int(np.min(np.abs(between_median - reference))),
        "minimum_absolute_raw_departure_adc": int(np.min(np.abs(between_raw - reference))),
        "median_samples_below_open_threshold": int(np.count_nonzero(median_below_open)),
        "median_below_open_threshold_runs": int(np.count_nonzero(
            median_below_open & np.r_[True, ~median_below_open[:-1]]
        )),
        "longest_median_below_open_threshold_samples": int(below_run_samples),
        "longest_median_below_open_threshold_ms_endpoint_span": float(below_run_ms),
        "longest_median_below_open_threshold_mm_endpoint_span": float(below_run_mm),
        "longest_median_in_baseline_band_samples": int(baseline_run_samples),
        "longest_median_in_baseline_band_ms_endpoint_span": float(baseline_run_ms),
        "longest_median_in_baseline_band_mm_endpoint_span": float(baseline_run_mm),
        "median_returned_to_baseline": bool(np.any(median_in_baseline)),
        "every_raw_conversion_stayed_outside_baseline": bool(not np.any(raw_in_baseline)),
        "plot": plot_path.name,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--analysis-root", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    root = args.analysis_root.resolve()
    output_dir = (args.output_dir or (root / "waveform-254-257")).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    base = load_module(root / "analyze_continuous_hall.py", "continuous_hall_base")
    odometry = load_module(root / "analyze_ir_odometry_calibration.py", "ir_odometry_base")

    capture = root / "sources" / "otto_navi_sync_20260928_101705.nsr"
    analysis_json = root / "continuous_hall_analysis.json"
    hall, _arrival_ir, _sequence_gaps, _ring_ranges = base.load_capture(capture)
    ir = odometry.load_ir(capture, base)
    global_offset = odometry.prepare_ir(ir)
    hall_position_mm = np.interp(
        hall["t_us"].astype(float),
        ir["aligned_global_us"].astype(float),
        ir["nominal_mm"],
    )

    analysis = json.loads(analysis_json.read_text())
    event_by_serial = {int(event["event_serial"]): event for event in analysis["events"]}
    results = [
        plot_pair(output_dir, hall, hall_position_mm, event_by_serial[first], event_by_serial[second])
        for first, second in PAIRS
    ]

    summary = {
        "capture": str(capture),
        "hall_sample_rate_hz": 1000,
        "raw_conversions_per_sample": 5,
        "ir_alignment": "captured_us plus minimum observed recorder-arrival offset",
        "ir_global_offset_us": int(global_offset),
        "distance_method": "linear interpolation of IR nominal_mm at Hall sample t_us",
        "opening_definition": "second consecutive median sample with same-sign absolute departure >= 70 ADC counts",
        "baseline_return_definition": "median within reference +/- 25 ADC counts, inclusive",
        "intervals": results,
    }
    (output_dir / "waveform_metrics.json").write_text(json.dumps(summary, indent=2) + "\n")
    with (output_dir / "waveform_metrics.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
