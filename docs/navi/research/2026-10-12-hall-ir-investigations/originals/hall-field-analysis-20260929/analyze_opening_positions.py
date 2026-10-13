#!/usr/bin/env python3
"""Per-magnet and per-interval Hall opening repeatability analysis."""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
import math
from collections import defaultdict
from pathlib import Path

import numpy as np


HIGH_CONFIDENCE_RULING = "ADVANCED"


def load_base_module(path: Path):
    spec = importlib.util.spec_from_file_location("continuous_hall", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def sample_sd(values):
    return float(np.std(values, ddof=1)) if len(values) >= 2 else None


def finite_stats(values):
    values = np.asarray([value for value in values if value is not None], dtype=float)
    if not len(values):
        return {"n": 0}
    return {
        "n": int(len(values)),
        "mean": float(np.mean(values)),
        "median": float(np.median(values)),
        "sd": sample_sd(values),
        "min": float(np.min(values)),
        "max": float(np.max(values)),
        "range": float(np.max(values) - np.min(values)),
    }


def persistent_run_start(mask, positions, minimum_span_mm=3.0):
    if not len(mask):
        return None
    padded = np.r_[False, mask, False]
    edges = np.flatnonzero(padded[1:] != padded[:-1]).reshape(-1, 2)
    for start, stop in edges:
        if stop - start >= 2 and positions[stop - 1] - positions[start] >= minimum_span_mm:
            return int(start)
    return None


def profile_measurements(events, hall, ir):
    positions = np.interp(hall["t_us"], ir["t_us"], ir["nominal_mm"])
    rows = []
    for event in events:
        # A discrepancy advance has the wrong polarity for the mapped magnet in
        # this run, so it cannot support a per-physical-magnet profile identity.
        if event.get("ruling") != HIGH_CONFIDENCE_RULING or not event.get("ir_reliable"):
            continue
        index = int(event["sample_index"])
        origin = float(event["ir_mm"])
        stop = int(np.searchsorted(positions, origin + 80.0, side="right"))
        stop = min(stop, len(positions), index + 15000)
        if stop <= index + 2:
            continue
        x = positions[index:stop] - origin
        departure = hall["median"][index:stop].astype(float) - float(event["base_open"])
        opening_departure = int(event["raw_open"]) - int(event["base_open"])
        sign = 1 if opening_departure > 0 else -1
        signed = departure * sign

        exit_relative = persistent_run_start(signed[1:] < 70, x[1:])
        exit_relative = exit_relative + 1 if exit_relative is not None else None
        peak_stop = exit_relative + 1 if exit_relative is not None else len(signed)
        peak_relative = int(np.argmax(signed[:peak_stop]))
        peak_reliable = exit_relative is not None
        quiet_relative = persistent_run_start(
            np.abs(departure[peak_relative + 1:]) <= 25,
            x[peak_relative + 1:],
        )
        quiet_relative = (quiet_relative + peak_relative + 1
                          if quiet_relative is not None else None)
        first_threshold_x = float(positions[index - 1] - origin) if index else 0.0
        rows.append({
            "event_serial": int(event["event_serial"]),
            "sample_index": index,
            "opened_ms": int(event["opened_ms"]),
            "mm": int(event["mm"]),
            "direction": event["dir"],
            "polarity": event["obs"],
            "opening_ir_mm": origin,
            "event_reference_counts": int(event["base_open"]),
            "opening_departure_counts": opening_departure,
            "first_threshold_sample_mm": first_threshold_x,
            "peak_offset_mm": float(x[peak_relative]) if peak_reliable else None,
            "peak_departure_counts": (
                float(departure[peak_relative]) if peak_reliable else None
            ),
            "unbounded_candidate_peak_offset_mm": float(x[peak_relative]),
            "unbounded_candidate_peak_departure_counts": float(departure[peak_relative]),
            "primary_peak_reliable": int(peak_reliable),
            "threshold_exit_mm": float(x[exit_relative]) if exit_relative is not None else None,
            "above_threshold_width_mm": (
                float(x[exit_relative] - first_threshold_x)
                if exit_relative is not None else None
            ),
            "quiet_return_mm": float(x[quiet_relative]) if quiet_relative is not None else None,
            "threshold_exit_censored": int(exit_relative is None),
        })
    return rows, positions


def aggregate_magnets(profile_rows):
    groups = defaultdict(list)
    for row in profile_rows:
        groups[(row["mm"], row["direction"])].append(row)
    output = []
    for (mm, direction), rows in sorted(groups.items(), key=lambda item: (item[0][1], item[0][0])):
        polarity_counts = defaultdict(int)
        for row in rows:
            polarity_counts[row["polarity"]] += 1
        polarity = max(polarity_counts, key=polarity_counts.get)
        peak = finite_stats([row["peak_offset_mm"] for row in rows])
        peak_amplitude = finite_stats([
            abs(row["peak_departure_counts"]) for row in rows
            if row["peak_departure_counts"] is not None
        ])
        width = finite_stats([row["above_threshold_width_mm"] for row in rows])
        quiet = finite_stats([row["quiet_return_mm"] for row in rows])
        opening = finite_stats([row["opening_departure_counts"] for row in rows])
        output.append({
            "mm": mm,
            "direction": direction,
            "passages": len(rows),
            "modal_polarity": polarity,
            "polarity_consistency_pct": 100.0 * polarity_counts[polarity] / len(rows),
            "opening_departure_mean_counts": opening["mean"],
            "opening_departure_sd_counts": opening["sd"],
            "peak_offset_n": peak["n"],
            "peak_offset_mean_mm": peak.get("mean"),
            "peak_offset_median_mm": peak.get("median"),
            "peak_offset_sd_mm": peak.get("sd"),
            "peak_offset_range_mm": peak.get("range"),
            "peak_abs_departure_mean_counts": peak_amplitude.get("mean"),
            "peak_abs_departure_median_counts": peak_amplitude.get("median"),
            "peak_abs_departure_sd_counts": peak_amplitude.get("sd"),
            "threshold_width_n": width["n"],
            "threshold_width_mean_mm": width.get("mean"),
            "threshold_width_sd_mm": width.get("sd"),
            "threshold_width_range_mm": width.get("range"),
            "quiet_return_n": quiet["n"],
            "quiet_return_mean_mm": quiet.get("mean"),
            "quiet_return_sd_mm": quiet.get("sd"),
            "censored_threshold_exits": sum(row["threshold_exit_censored"] for row in rows),
        })
    return output


def eligible_intervals(data):
    event_by_serial = {int(event["event_serial"]): event for event in data["events"]}
    return [
        row for row in data["intervals"]
        if row["map_steps"] == 1
        and row["ir_reliable"]
        and event_by_serial[row["from_serial"]].get("ruling") == HIGH_CONFIDENCE_RULING
        and event_by_serial[row["to_serial"]].get("ruling") == HIGH_CONFIDENCE_RULING
        and event_by_serial[row["to_serial"]].get("distance_assessable") == 1
        and event_by_serial[row["to_serial"]].get("ir_interval") == "MM_REFERENCED_EPOCH"
        and event_by_serial[row["to_serial"]].get("pre_mm") == row["from_mm"]
    ]


def aggregate_intervals(rows):
    groups = defaultdict(list)
    for row in rows:
        groups[(row["direction"], row["from_mm"], row["to_mm"])].append(row)
    output = []
    for (direction, from_mm, to_mm), items in sorted(groups.items()):
        observed = np.asarray([row["observed_mm"] for row in items], dtype=float)
        errors = np.asarray([row["error_mm"] for row in items], dtype=float)
        current_errors = np.asarray([row["current_error_mm"] for row in items], dtype=float)
        output.append({
            "direction": direction,
            "from_mm": from_mm,
            "to_mm": to_mm,
            "passages": len(items),
            "mapped_distance_mm": items[0]["map_mm"],
            "current_mapped_distance_mm": items[0]["current_map_mm"],
            "observed_mean_mm": float(np.mean(observed)),
            "observed_median_mm": float(np.median(observed)),
            "observed_sd_mm": sample_sd(observed),
            "observed_range_mm": float(np.ptp(observed)),
            "error_mean_mm": float(np.mean(errors)),
            "error_median_mm": float(np.median(errors)),
            "error_sd_mm": sample_sd(errors),
            "error_min_mm": float(np.min(errors)),
            "error_max_mm": float(np.max(errors)),
            "error_range_mm": float(np.ptp(errors)),
            "current_map_error_mean_mm": float(np.mean(current_errors)),
            "current_map_error_sd_mm": sample_sd(current_errors),
        })
    return output, groups


def variance_and_calibration(interval_groups):
    repeated = {key: rows for key, rows in interval_groups.items() if len(rows) >= 2}
    values = []
    within_ss = 0.0
    group_means = {}
    for key, rows in repeated.items():
        errors = np.asarray([row["error_mm"] for row in rows], dtype=float)
        values.extend(errors)
        mean = float(np.mean(errors))
        group_means[key] = mean
        within_ss += float(np.sum((errors - mean) ** 2))
    values = np.asarray(values)
    grand = float(np.mean(values))
    between_ss = sum(
        len(repeated[key]) * (mean - grand) ** 2
        for key, mean in group_means.items()
    )
    total_ss = float(np.sum((values - grand) ** 2))
    pooled_within_sd = math.sqrt(within_ss / (len(values) - len(repeated)))

    uncalibrated = []
    leave_one_out = []
    for rows in repeated.values():
        errors = np.asarray([row["error_mm"] for row in rows], dtype=float)
        for index, error in enumerate(errors):
            other_mean = float(np.mean(np.delete(errors, index)))
            uncalibrated.append(float(error))
            leave_one_out.append(float(error - other_mean))
    uncalibrated = np.asarray(uncalibrated)
    leave_one_out = np.asarray(leave_one_out)
    return {
        "repeated_interval_groups": len(repeated),
        "observations": int(len(values)),
        "between_interval_fraction_of_error_variance": between_ss / total_ss,
        "pooled_within_interval_sd_mm": pooled_within_sd,
        "uncalibrated_map_rmse_mm": float(np.sqrt(np.mean(uncalibrated ** 2))),
        "uncalibrated_map_mae_mm": float(np.mean(np.abs(uncalibrated))),
        "leave_one_passage_out_calibrated_rmse_mm": float(np.sqrt(np.mean(leave_one_out ** 2))),
        "leave_one_passage_out_calibrated_mae_mm": float(np.mean(np.abs(leave_one_out))),
        "leave_one_passage_out_calibrated_p95_abs_mm": float(
            np.quantile(np.abs(leave_one_out), 0.95)
        ),
        "uncalibrated_errors": uncalibrated.tolist(),
        "leave_one_out_errors": leave_one_out.tolist(),
    }


def field_explanation(interval_rows, magnet_rows):
    magnet = {(row["direction"], row["mm"]): row for row in magnet_rows}
    x_peak = []
    x_width = []
    y = []
    weights = []
    used = []
    for row in interval_rows:
        if row["passages"] < 2:
            continue
        start = magnet.get((row["direction"], row["from_mm"]))
        end = magnet.get((row["direction"], row["to_mm"]))
        if (not start or not end or start["peak_offset_median_mm"] is None
                or end["peak_offset_median_mm"] is None):
            continue
        peak_difference = start["peak_offset_median_mm"] - end["peak_offset_median_mm"]
        if start["threshold_width_mean_mm"] is None or end["threshold_width_mean_mm"] is None:
            continue
        width_difference = start["threshold_width_mean_mm"] - end["threshold_width_mean_mm"]
        x_peak.append(peak_difference)
        x_width.append(width_difference)
        y.append(row["error_mean_mm"])
        weights.append(row["passages"])
        used.append(row)

    def regression(x, y):
        x = np.asarray(x, dtype=float)
        y = np.asarray(y, dtype=float)
        design = np.column_stack([np.ones(len(x)), x])
        beta = np.linalg.lstsq(design, y, rcond=None)[0]
        predicted = design @ beta
        ss_res = float(np.sum((y - predicted) ** 2))
        ss_tot = float(np.sum((y - np.mean(y)) ** 2))
        return {
            "n": len(x),
            "intercept": float(beta[0]),
            "slope": float(beta[1]),
            "correlation": float(np.corrcoef(x, y)[0, 1]),
            "r_squared": 1 - ss_res / ss_tot,
        }

    return {
        "peak_offset_difference_model": regression(x_peak, y),
        "threshold_width_difference_model": regression(x_width, y),
    }


def rank_values(values):
    values = np.asarray(values, dtype=float)
    order = np.argsort(values, kind="mergesort")
    ranks = np.empty(len(values), dtype=float)
    start = 0
    while start < len(values):
        stop = start + 1
        while stop < len(values) and values[order[stop]] == values[order[start]]:
            stop += 1
        ranks[order[start:stop]] = (start + stop - 1) / 2.0
        start = stop
    return ranks


def correlation_summary(x, y):
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    return {
        "n": int(len(x)),
        "pearson_r": float(np.corrcoef(x, y)[0, 1]),
        "spearman_r": float(np.corrcoef(rank_values(x), rank_values(y))[0, 1]),
    }


def amplitude_relationship(profile_rows, magnet_rows):
    cw_profiles = [row for row in profile_rows
                   if row["direction"] == "CW" and row["primary_peak_reliable"]]
    absolute_peak = [abs(row["peak_departure_counts"]) for row in cw_profiles]
    offsets = [row["peak_offset_mm"] for row in cw_profiles]

    cw_magnets = [row for row in magnet_rows
                  if row["direction"] == "CW" and row["peak_offset_n"]]
    profile_groups = defaultdict(list)
    for row in cw_profiles:
        profile_groups[row["mm"]].append(row)
    magnet_amplitude = []
    magnet_offset = []
    centered_amplitude = []
    centered_offset = []
    for aggregate in cw_magnets:
        rows = profile_groups[aggregate["mm"]]
        amplitudes = np.asarray([abs(row["peak_departure_counts"]) for row in rows])
        group_offsets = np.asarray([row["peak_offset_mm"] for row in rows])
        magnet_amplitude.append(float(np.median(amplitudes)))
        magnet_offset.append(float(np.median(group_offsets)))
        centered_amplitude.extend((amplitudes - np.mean(amplitudes)).tolist())
        centered_offset.extend((group_offsets - np.mean(group_offsets)).tolist())
    return {
        "all_cw_passages": correlation_summary(absolute_peak, offsets),
        "cw_magnet_medians": correlation_summary(magnet_amplitude, magnet_offset),
        "within_magnet_centered_passages": correlation_summary(
            centered_amplitude, centered_offset
        ),
    }


def direction_comparison(magnet_rows):
    lookup = {(row["direction"], row["mm"]): row for row in magnet_rows}
    pairs = []
    for mm in range(171):
        cw = lookup.get(("CW", mm))
        ccw = lookup.get(("CCW", mm))
        if (cw and ccw and cw["peak_offset_median_mm"] is not None
                and ccw["peak_offset_median_mm"] is not None):
            pairs.append({
                "mm": mm,
                "cw_peak_offset_mm": cw["peak_offset_median_mm"],
                "ccw_peak_offset_mm": ccw["peak_offset_median_mm"],
                "ccw_minus_cw_mm": (
                    ccw["peak_offset_median_mm"] - cw["peak_offset_median_mm"]
                ),
                "cw_passages": cw["passages"],
                "ccw_passages": ccw["passages"],
            })
    differences = np.asarray([row["ccw_minus_cw_mm"] for row in pairs])
    return pairs, {
        "paired_magnets": len(pairs),
        "ccw_passages_per_magnet": sorted(set(row["ccw_passages"] for row in pairs)),
        "difference_ccw_minus_cw_mm": finite_stats(differences),
        "paired_correlation": (
            float(np.corrcoef(
                [row["cw_peak_offset_mm"] for row in pairs],
                [row["ccw_peak_offset_mm"] for row in pairs],
            )[0, 1]) if len(pairs) >= 2 else None
        ),
    }


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def write_magnet_markdown(path, magnet_rows):
    def number(value, digits=2):
        return "—" if value is None else f"{value:.{digits}f}"

    lines = [
        "# Per-magnet opening-to-primary-peak measurements",
        "",
        "Only high-confidence `ADVANCED` events are assigned to a physical mapped "
        "magnet. A peak is usable only when the same-polarity ≥70-count region has "
        "a sustained spatial exit, preventing a later lobe from being selected.",
        "",
    ]
    for direction in ("CW", "CCW"):
        lines.extend([
            f"## {direction}",
            "",
            "| Magnet | Passes | Usable peaks | Median mm | Mean mm | SD mm | Range mm | Median peak counts | Censored |",
            "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
        ])
        for row in sorted((item for item in magnet_rows
                           if item["direction"] == direction), key=lambda item: item["mm"]):
            lines.append(
                f"| MM{row['mm']:03d} | {row['passages']} | {row['peak_offset_n']} | "
                f"{number(row['peak_offset_median_mm'])} | "
                f"{number(row['peak_offset_mean_mm'])} | "
                f"{number(row['peak_offset_sd_mm'])} | "
                f"{number(row['peak_offset_range_mm'])} | "
                f"{number(row['peak_abs_departure_median_counts'], 1)} | "
                f"{row['censored_threshold_exits']} |"
            )
        lines.append("")
    path.write_text("\n".join(lines) + "\n")


def plot_results(output_dir, profile_rows, magnet_rows, interval_rows, calibration,
                 hall, positions):
    import matplotlib.pyplot as plt

    output_dir.mkdir(parents=True, exist_ok=True)
    cw_intervals = sorted((row for row in interval_rows if row["direction"] == "CW"),
                          key=lambda row: row["from_mm"])
    fig, (top, bottom) = plt.subplots(2, 1, figsize=(13, 7), sharex=True)
    x = np.asarray([row["from_mm"] for row in cw_intervals])
    means = np.asarray([row["error_mean_mm"] for row in cw_intervals])
    sds = np.asarray([row["error_sd_mm"] or 0 for row in cw_intervals])
    top.errorbar(x, means, yerr=sds, fmt="o", ms=3.5, lw=0.8, capsize=1.5,
                 color="#185abc", ecolor="#8ab4f8")
    top.axhline(0, color="#202124", lw=0.9)
    top.set_ylabel("Mean interval error (mm)\n± repeat SD")
    top.set_title("CW opening-to-opening error by capture-era mapped interval")
    top.grid(alpha=0.18)
    bottom.plot(x, [row["error_range_mm"] for row in cw_intervals], "o-", ms=3,
                lw=0.8, color="#7b1fa2")
    bottom.set_xlabel("Starting map marker (interval MM → MM+1)")
    bottom.set_ylabel("Repeat range (mm)")
    bottom.grid(alpha=0.18)
    fig.tight_layout()
    fig.savefig(output_dir / "per_interval_offsets.png", dpi=180, bbox_inches="tight")
    plt.close(fig)

    cw_magnets = sorted((row for row in magnet_rows
                         if row["direction"] == "CW" and row["peak_offset_n"]),
                        key=lambda row: row["mm"])
    fig, axis = plt.subplots(figsize=(13, 4.6))
    x = np.asarray([row["mm"] for row in cw_magnets])
    medians = np.asarray([row["peak_offset_median_mm"] for row in cw_magnets])
    half_ranges = np.asarray([row["peak_offset_range_mm"] / 2 for row in cw_magnets])
    axis.errorbar(x, medians, yerr=half_ranges, fmt="o", ms=3.2, lw=0.7,
                  capsize=1.3, color="#188038", ecolor="#81c995")
    axis.set_xlabel("Mapped magnet marker")
    axis.set_ylabel("Opening → same-polarity extremum (mm)\nmedian ± half repeat range")
    axis.set_title("Opening position within each CW recorded field profile")
    axis.grid(alpha=0.18)
    fig.tight_layout()
    fig.savefig(output_dir / "per_magnet_profile_offsets.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    fig, axis = plt.subplots(figsize=(8.5, 4.2))
    bins = np.arange(-65, 66, 5)
    axis.hist(calibration["uncalibrated_errors"], bins=bins, alpha=0.68,
              color="#d93025", label="current mapped distance")
    axis.hist(calibration["leave_one_out_errors"], bins=bins, alpha=0.68,
              color="#185abc", label="interval mean from other passages")
    axis.axvline(0, color="#202124", lw=0.8)
    axis.set_xlabel("Opening-to-opening prediction error (mm)")
    axis.set_ylabel("Passages")
    axis.set_title("Leave-one-passage-out test of an opening-distance map")
    axis.legend()
    axis.grid(alpha=0.16)
    fig.tight_layout()
    fig.savefig(output_dir / "calibrated_map_cross_validation.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    profile_groups = defaultdict(list)
    for row in profile_rows:
        if row["direction"] == "CW" and row["primary_peak_reliable"]:
            profile_groups[row["mm"]].append(row)
    magnet_offsets = np.asarray([row["peak_offset_median_mm"] for row in cw_magnets])
    magnet_amplitudes = np.asarray([
        np.median([abs(item["peak_departure_counts"])
                   for item in profile_groups[row["mm"]]])
        for row in cw_magnets
    ])
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    axes[0].hist(magnet_offsets, bins=np.arange(0, 66, 3), color="#188038",
                 alpha=0.82, edgecolor="white")
    axes[0].axvline(np.median(magnet_offsets), color="#202124", ls="--")
    axes[0].set_xlabel("Median opening-to-primary-peak distance (mm)")
    axes[0].set_ylabel("CW magnets")
    axes[0].set_title("Distribution across MM000–MM170")
    axes[1].scatter(magnet_amplitudes, magnet_offsets, s=20, alpha=0.75,
                    color="#185abc")
    axes[1].set_xlabel("Median absolute primary-peak departure (counts)")
    axes[1].set_ylabel("Median opening-to-peak distance (mm)")
    axes[1].set_title("Peak amplitude versus opening-to-peak distance")
    for axis in axes:
        axis.grid(alpha=0.16)
    fig.tight_layout()
    fig.savefig(output_dir / "opening_peak_distribution_and_amplitude.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    eligible_magnets = [row for row in cw_magnets
                        if row["peak_offset_n"] >= 3 and row["peak_offset_sd_mm"] is not None]
    best = min(eligible_magnets, key=lambda row: row["peak_offset_sd_mm"])
    worst = max(eligible_magnets, key=lambda row: row["peak_offset_sd_mm"])
    uncensored = [row for row in cw_magnets
                  if row["peak_offset_n"] >= 2 and row["censored_threshold_exits"] == 0]
    narrow = min(uncensored, key=lambda row: row["peak_offset_median_mm"])
    broad = max(uncensored, key=lambda row: row["peak_offset_median_mm"])
    examples = ((best, "Most consistent"), (worst, "Least consistent"),
                (narrow, "Narrowest median leading edge"),
                (broad, "Broadest median leading edge"))
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), sharey=True)
    for axis, (aggregate, label) in zip(axes.flat, examples):
        selected = [row for row in profile_rows
                    if row["direction"] == "CW" and row["mm"] == aggregate["mm"]]
        for row in selected:
            index = int(row["sample_index"])
            stop = min(int(np.searchsorted(positions, row["opening_ir_mm"] + 60)),
                       index + 15000, len(positions))
            xrow = positions[index:stop] - row["opening_ir_mm"]
            sign = 1 if row["opening_departure_counts"] > 0 else -1
            yrow = (hall["median"][index:stop].astype(float)
                    - row["event_reference_counts"]) * sign
            stride = max(1, len(xrow) // 1200)
            line, = axis.plot(xrow[::stride], yrow[::stride], lw=0.9, alpha=0.72)
            if row["primary_peak_reliable"]:
                axis.plot(row["peak_offset_mm"], abs(row["peak_departure_counts"]), "o",
                          ms=4, color=line.get_color())
        axis.axhline(70, color="#b3261e", ls="--", lw=0.8)
        axis.axvline(0, color="#202124", lw=0.8)
        axis.axvline(aggregate["peak_offset_median_mm"], color="#188038",
                     ls=":", lw=1.0)
        axis.set_xlim(-1, 65)
        axis.set_title(
            f"{label}: MM{aggregate['mm']:03d}\n"
            f"median {aggregate['peak_offset_median_mm']:.1f} mm, "
            f"SD {aggregate['peak_offset_sd_mm']:.2f} mm, n={aggregate['passages']}"
        )
        axis.set_xlabel("Distance from recorded opening (mm)")
        axis.grid(alpha=0.16)
    axes[0, 0].set_ylabel("Signed Hall departure (counts)")
    axes[1, 0].set_ylabel("Signed Hall departure (counts)")
    fig.tight_layout()
    fig.savefig(output_dir / "profile_repeatability_examples.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)
    return best, worst, narrow, broad


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("analysis_json", type=Path)
    parser.add_argument("--base-script", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    base = load_base_module(args.base_script)
    data = json.loads(args.analysis_json.read_text())
    hall, ir, _sequence_gaps, _ring_ranges = base.load_capture(args.capture)
    profiles, positions = profile_measurements(data["events"], hall, ir)
    magnets = aggregate_magnets(profiles)
    interval_observations = eligible_intervals(data)
    intervals, interval_groups = aggregate_intervals(interval_observations)
    calibration = variance_and_calibration(interval_groups)
    explanation = field_explanation(intervals, magnets)
    amplitude = amplitude_relationship(profiles, magnets)
    direction_pairs, direction = direction_comparison(magnets)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(args.output_dir / "magnet_opening_variation.csv", magnets)
    write_magnet_markdown(args.output_dir / "magnet_opening_variation.md", magnets)
    write_csv(args.output_dir / "interval_opening_distances.csv", intervals)
    write_csv(args.output_dir / "opening_passages.csv", profiles)
    write_csv(args.output_dir / "cw_ccw_peak_comparison.csv", direction_pairs)
    best, worst, narrow, broad = plot_results(
        args.output_dir / "plots", profiles, magnets, intervals,
        calibration, hall, positions,
    )

    repeated_intervals = [row for row in intervals
                          if row["passages"] >= 3 and row["error_sd_mm"] is not None]
    most_consistent = sorted(repeated_intervals, key=lambda row: row["error_sd_mm"])[:10]
    least_consistent = sorted(repeated_intervals, key=lambda row: row["error_sd_mm"],
                              reverse=True)[:10]
    repeated_magnets = [row for row in magnets
                        if (row["direction"] == "CW" and row["peak_offset_n"] >= 3
                            and row["peak_offset_sd_mm"] is not None)]
    summary = {
        "profile_events": len(profiles),
        "magnet_direction_groups": len(magnets),
        "cw_magnets": sum(row["direction"] == "CW" for row in magnets),
        "ccw_magnets": sum(row["direction"] == "CCW" for row in magnets),
        "reliable_primary_peaks": sum(row["primary_peak_reliable"] for row in profiles),
        "censored_primary_profiles": sum(not row["primary_peak_reliable"]
                                         for row in profiles),
        "cw_magnets_with_peak_estimate": sum(
            row["direction"] == "CW" and row["peak_offset_n"] > 0 for row in magnets
        ),
        "eligible_interval_observations": len(interval_observations),
        "interval_direction_groups": len(intervals),
        "calibration": {key: value for key, value in calibration.items()
                        if not key.endswith("errors")},
        "field_explanation": explanation,
        "amplitude_relationship": amplitude,
        "direction_comparison": direction,
        "cw_magnet_peak_offset_distribution_mm": finite_stats([
            row["peak_offset_median_mm"] for row in magnets
            if row["direction"] == "CW" and row["peak_offset_median_mm"] is not None
        ]),
        "profile_peak_offset_sd_distribution_mm": finite_stats([
            row["peak_offset_sd_mm"] for row in repeated_magnets
        ]),
        "profile_repeatability_counts": {
            "sd_at_most_2_mm": sum(row["peak_offset_sd_mm"] <= 2
                                    for row in repeated_magnets),
            "sd_at_most_5_mm": sum(row["peak_offset_sd_mm"] <= 5
                                    for row in repeated_magnets),
            "sd_above_10_mm": sum(row["peak_offset_sd_mm"] > 10
                                   for row in repeated_magnets),
            "eligible_cw_magnets": len(repeated_magnets),
        },
        "interval_error_sd_distribution_mm": finite_stats([
            row["error_sd_mm"] for row in repeated_intervals
        ]),
        "most_consistent_profile": best,
        "least_consistent_profile": worst,
        "narrowest_profile": narrow,
        "broadest_profile": broad,
        "most_consistent_intervals": most_consistent,
        "least_consistent_intervals": least_consistent,
    }
    (args.output_dir / "opening_position_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n"
    )
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
