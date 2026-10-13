#!/usr/bin/env python3
"""IR integrity and held-out Hall opening-map evaluation for 2026-09-28."""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
import math
import struct
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np


PRIMARY = {"ADVANCED", "ADVANCED_WITH_DISCREPANCY"}
HIGH_CONFIDENCE = "ADVANCED"
PITCH_MM = 9.652


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def stats(values):
    values = np.asarray([value for value in values if value is not None], dtype=float)
    if not len(values):
        return {"n": 0}
    return {
        "n": int(len(values)),
        "min": float(np.min(values)),
        "p05": float(np.quantile(values, 0.05)),
        "p50": float(np.median(values)),
        "p95": float(np.quantile(values, 0.95)),
        "max": float(np.max(values)),
        "mean": float(np.mean(values)),
        "sd": float(np.std(values, ddof=1)) if len(values) >= 2 else None,
    }


def load_ir(capture, base):
    rows = []
    for _received_us, packet in base.F.iter_capture(str(capture)):
        header, payload = base.F.parse_record(packet)
        if header.rec_type != base.F.REC_IR:
            continue
        item = base.F.parse_ir(payload)
        wire = dict(zip(base.WIRE_KEYS, struct.unpack(base.WIRE_FMT, item["wire"])))
        rows.append({
            "arrival_us": int(header.t0_us),
            "ring_drops": int(header.ring_drops),
            "health_fault": int(item["health_fault"]),
            "readiness": int(item["readiness"]),
            **{key: int(value) for key, value in wire.items()},
        })
    keys = rows[0].keys()
    return {key: np.asarray([row[key] for row in rows], dtype=np.int64) for key in keys}


def load_telemetry(paths):
    events = defaultdict(dict)
    for path in paths:
        with path.open(errors="replace") as stream:
            for line in stream:
                parts = line.rstrip("\n").split("\t", 2)
                if len(parts) != 3:
                    continue
                try:
                    item = json.loads(parts[2])
                except json.JSONDecodeError:
                    continue
                if not isinstance(item, dict):
                    continue
                serial = item.get("event_serial")
                if serial is None:
                    continue
                serial = int(serial)
                if parts[1].endswith("/nav/discrepancy"):
                    events[serial]["discrepancy"] = item
                elif parts[1].endswith("/nav/ir_compare") and item.get("steps") == 1:
                    events[serial]["compare"] = item
    return events


def prepare_ir(ir):
    arrival = ir["arrival_us"].astype(np.int64)
    captured = ir["captured_us"].astype(np.int64)
    raw_offset = arrival - captured
    global_offset = int(np.min(raw_offset))
    running_offset = np.minimum.accumulate(raw_offset)
    ir["aligned_global_us"] = captured + global_offset
    ir["aligned_online_us"] = captured + running_offset
    ir["excess_latency_us"] = raw_offset - global_offset
    ir["running_offset_us"] = running_offset
    ir["nominal_mm"] = ir["nominal_um"].astype(float) / 1000.0
    return global_offset


def ir_integrity(ir, global_offset):
    seq_delta = np.diff(ir["sequence"])
    capture_delta = np.diff(ir["captured_us"])
    arrival_delta = np.diff(ir["arrival_us"])
    pulse_delta = np.diff(ir["completed_pulses"])
    duration_s = (ir["captured_us"][-1] - ir["captured_us"][0]) / 1e6
    missing_frames = int(np.sum(np.maximum(seq_delta - 1, 0)))

    elapsed_s = (ir["captured_us"] - ir["captured_us"][0]) / 1e6
    minute = np.floor(elapsed_s / 60).astype(int)
    minute_p05 = [
        float(np.quantile(ir["excess_latency_us"][minute == bucket], 0.05))
        for bucket in np.unique(minute)
    ]
    counter_names = (
        "observed_rises", "completed_pulses", "inferred_added", "inferred_removed",
        "unreliable_samples", "saturated_samples", "sample_gaps", "open_aborts",
    )
    counter_change = {
        name: int(ir[name][-1] - ir[name][0]) for name in counter_names
    }
    moving = capture_delta > 0
    speed = pulse_delta[moving] * PITCH_MM / (capture_delta[moving] / 1e6)
    return {
        "snapshots": int(len(ir["sequence"])),
        "duration_s": float(duration_s),
        "pitch_um_values": sorted(set(map(int, ir["pitch_um"]))),
        "calibration_id_values": sorted(set(map(int, ir["calibration_id"]))),
        "nominal_product_mismatches": int(np.sum(
            ir["nominal_um"] != ir["completed_pulses"] * ir["pitch_um"]
        )),
        "sequence_duplicate_or_reverse": int(np.sum(seq_delta <= 0)),
        "accepted_sequence_gaps": int(np.sum(seq_delta > 1)),
        "missing_snapshot_sequence_positions": missing_frames,
        "capture_time_nonmonotonic": int(np.sum(capture_delta <= 0)),
        "arrival_time_nonmonotonic": int(np.sum(arrival_delta < 0)),
        "pulse_counter_regressions": int(np.sum(pulse_delta < 0)),
        "ring_drop_range": [int(np.min(ir["ring_drops"])), int(np.max(ir["ring_drops"]))],
        "counter_changes": counter_change,
        "optical_reason_counts": dict(Counter(map(int, ir["optical_reason"]))),
        "health_fault_counts": dict(Counter(map(int, ir["health_fault"]))),
        "readiness_counts": dict(Counter(map(int, ir["readiness"]))),
        "observed_minus_completed": stats(
            ir["observed_rises"] - ir["completed_pulses"]
        ),
        "snapshot_capture_interval_ms": stats(capture_delta / 1000.0),
        "accepted_pulse_delta_per_snapshot": stats(pulse_delta),
        "derived_snapshot_speed_mm_s": stats(speed),
        "clock_offset_min_us": global_offset,
        "excess_arrival_latency_ms": stats(ir["excess_latency_us"] / 1000.0),
        "minute_bin_latency_p05_ms": stats(np.asarray(minute_p05) / 1000.0),
    }


def add_event_positions(data, hall, ir, telemetry):
    ir_time = ir["aligned_global_us"].astype(float)
    arrival_time = ir["arrival_us"].astype(float)
    nominal = ir["nominal_mm"]
    seq_index = {int(seq): index for index, seq in enumerate(ir["sequence"])}
    logged_alignment_errors = []
    for event in data["events"]:
        event_time = int(hall["t_us"][int(event["sample_index"])])
        event["hall_local_us"] = event_time
        event["aligned_ir_mm"] = float(np.interp(event_time, ir_time, nominal))
        event["arrival_interpolated_mm"] = float(np.interp(event_time, arrival_time, nominal))
        record = telemetry.get(int(event["event_serial"]), {})
        compare = record.get("compare")
        discrepancy = record.get("discrepancy")
        if compare:
            event["onboard_ir_mm"] = compare.get("ir_nominal_mm")
            event["onboard_ir_pulses"] = compare.get("ir_pulses")
            event["onboard_ir_quality"] = compare.get("ir_quality")
            event["onboard_anchor_mm"] = compare.get("anchor_mm")
        if discrepancy:
            event["logged_ir_alignment_us"] = discrepancy.get("ir_alignment_us")
            event["logged_ir_seq"] = discrepancy.get("ir_seq")
            seq = discrepancy.get("ir_seq")
            if seq in seq_index and discrepancy.get("ir_alignment_us") is not None:
                index = seq_index[seq]
                reconstructed = int(ir["aligned_online_us"][index] - event_time)
                logged_alignment_errors.append(
                    reconstructed - int(discrepancy["ir_alignment_us"])
                )
    return stats(logged_alignment_errors)


def build_intervals(data):
    events = data["events"]
    event_by_serial = {int(event["event_serial"]): event for event in events}
    secondaries = [event for event in events if event.get("ruling") == "NON_LANDMARK_HALL"]
    rows = []
    for source in data["intervals"]:
        start = event_by_serial[source["from_serial"]]
        end = event_by_serial[source["to_serial"]]
        if (source["map_steps"] != 1 or start.get("ruling") != HIGH_CONFIDENCE
                or end.get("ruling") != HIGH_CONFIDENCE
                or end.get("distance_assessable") != 1
                or end.get("ir_interval") != "MM_REFERENCED_EPOCH"
                or end.get("pre_mm") != source["from_mm"]
                or end.get("onboard_ir_quality") != "MM_REFERENCED_EPOCH"
                or end.get("onboard_anchor_mm") != source["from_mm"]
                or end.get("onboard_ir_mm") is None):
            continue
        candidates = []
        for event in secondaries:
            if not (start["opened_ms"] < event["opened_ms"] < end["opened_ms"]):
                continue
            if (event.get("onboard_ir_quality") == "MM_REFERENCED_EPOCH"
                    and event.get("onboard_anchor_mm") == source["from_mm"]
                    and event.get("onboard_ir_mm") is not None):
                candidates.append({
                    "serial": int(event["event_serial"]),
                    "distance_mm": float(event["onboard_ir_mm"]),
                    "genuine": False,
                })
        candidates.append({
            "serial": int(end["event_serial"]),
            "distance_mm": float(end["onboard_ir_mm"]),
            "genuine": True,
        })
        candidates.sort(key=lambda item: (item["distance_mm"], item["serial"]))
        duration_s = (end["hall_local_us"] - start["hall_local_us"]) / 1e6
        onboard = float(end["onboard_ir_mm"])
        rows.append({
            "direction": source["direction"],
            "from_mm": int(source["from_mm"]),
            "to_mm": int(source["to_mm"]),
            "from_serial": int(source["from_serial"]),
            "to_serial": int(source["to_serial"]),
            "map_mm": float(source["map_mm"]),
            "onboard_mm": onboard,
            "onboard_pulses": int(end["onboard_ir_pulses"]),
            "aligned_interpolated_mm": end["aligned_ir_mm"] - start["aligned_ir_mm"],
            "arrival_interpolated_mm": (
                end["arrival_interpolated_mm"] - start["arrival_interpolated_mm"]
            ),
            "duration_s": duration_s,
            "speed_mm_s": onboard / duration_s,
            "from_alignment_us": start.get("logged_ir_alignment_us"),
            "to_alignment_us": end.get("logged_ir_alignment_us"),
            "candidates": candidates,
        })
    return rows


def candidate_select(expected, candidates):
    best = []
    best_error = None
    observed = []
    finalized_before_genuine = False
    for candidate in candidates:
        if best_error is not None and expected + best_error <= candidate["distance_mm"]:
            finalized_before_genuine = not any(item["genuine"] for item in best)
            break
        observed.append(candidate)
        error = abs(candidate["distance_mm"] - expected)
        if best_error is None or error < best_error - 1e-9:
            best_error = error
            best = [candidate]
        elif abs(error - best_error) <= 1e-9:
            best.append(candidate)
        if candidate["distance_mm"] >= expected:
            break
    all_errors = sorted(abs(item["distance_mm"] - expected) for item in candidates)
    margin = all_errors[1] - all_errors[0] if len(all_errors) >= 2 else None
    return {
        "correct": len(best) == 1 and best[0]["genuine"],
        "false": len(best) == 1 and not best[0]["genuine"],
        "tie": len(best) > 1,
        "winner_serials": [item["serial"] for item in best],
        "winner_error_mm": best_error,
        "margin_to_second_mm": margin,
        "finalized_before_genuine": finalized_before_genuine,
        "observed_candidates_before_final": len(observed),
    }


def evaluate_calibration(intervals):
    groups = defaultdict(list)
    for row in intervals:
        groups[(row["direction"], row["from_mm"], row["to_mm"])].append(row)
    trials = []
    for key, rows in groups.items():
        if len(rows) < 2:
            continue
        for index, row in enumerate(rows):
            training = [other for j, other in enumerate(rows) if j != index]
            calibrated = float(np.mean([other["onboard_mm"] for other in training]))
            aligned_calibrated = float(np.mean([
                other["aligned_interpolated_mm"] for other in training
            ]))
            physical = row["map_mm"]
            physical_selection = candidate_select(physical, row["candidates"])
            calibrated_selection = candidate_select(calibrated, row["candidates"])
            aligned_calibrated_selection = candidate_select(
                aligned_calibrated, row["candidates"]
            )
            trials.append({
                **{name: row[name] for name in (
                    "direction", "from_mm", "to_mm", "from_serial", "to_serial",
                    "map_mm", "onboard_mm", "onboard_pulses", "speed_mm_s",
                )},
                "training_passages": len(training),
                "calibrated_expected_mm": calibrated,
                "aligned_calibrated_expected_mm": aligned_calibrated,
                "physical_error_mm": row["onboard_mm"] - physical,
                "calibrated_error_mm": row["onboard_mm"] - calibrated,
                "aligned_observed_mm": row["aligned_interpolated_mm"],
                "aligned_physical_error_mm": row["aligned_interpolated_mm"] - physical,
                "aligned_calibrated_error_mm": (
                    row["aligned_interpolated_mm"] - aligned_calibrated
                ),
                "false_candidate_count": sum(not item["genuine"] for item in row["candidates"]),
                "candidates": row["candidates"],
                "false_candidate_distances_mm": ";".join(
                    f'{item["distance_mm"]:.3f}' for item in row["candidates"]
                    if not item["genuine"]
                ),
                "physical_selection": physical_selection,
                "calibrated_selection": calibrated_selection,
                "aligned_calibrated_selection": aligned_calibrated_selection,
                "physical_genuine_inside_15pct": (
                    physical * 0.85 <= row["onboard_mm"] <= physical * 1.15
                ),
                "calibrated_genuine_inside_15pct": (
                    calibrated * 0.85 <= row["onboard_mm"] <= calibrated * 1.15
                ),
                "aligned_calibrated_genuine_inside_15pct": (
                    aligned_calibrated * 0.85 <= row["onboard_mm"]
                    <= aligned_calibrated * 1.15
                ),
            })
    return trials, groups


def selection_summary(trials, prefix):
    selections = [row[f"{prefix}_selection"] for row in trials]
    multi = [row for row in trials if row["false_candidate_count"]]
    multi_selections = [row[f"{prefix}_selection"] for row in multi]
    return {
        "all_trials": len(trials),
        "correct": sum(item["correct"] for item in selections),
        "false": sum(item["false"] for item in selections),
        "ties": sum(item["tie"] for item in selections),
        "finalized_false_before_genuine": sum(
            item["finalized_before_genuine"] for item in selections
        ),
        "multi_candidate_trials": len(multi),
        "multi_candidate_correct": sum(item["correct"] for item in multi_selections),
        "multi_candidate_false": sum(item["false"] for item in multi_selections),
        "multi_candidate_ties": sum(item["tie"] for item in multi_selections),
        "multi_candidate_margin_at_most_one_pulse": sum(
            item["margin_to_second_mm"] is not None
            and item["margin_to_second_mm"] <= PITCH_MM
            for item in multi_selections
        ),
        "genuine_outside_15pct": sum(
            not row[f"{prefix}_genuine_inside_15pct"] for row in trials
        ),
    }


def calibration_summary(trials):
    physical = np.asarray([row["physical_error_mm"] for row in trials])
    calibrated = np.asarray([row["calibrated_error_mm"] for row in trials])
    aligned_physical = np.asarray([row["aligned_physical_error_mm"] for row in trials])
    aligned_calibrated = np.asarray([
        row["aligned_calibrated_error_mm"] for row in trials
    ])
    return {
        "trials": len(trials),
        "physical_map_error_mm": stats(physical),
        "calibrated_map_error_mm": stats(calibrated),
        "physical_rmse_mm": float(np.sqrt(np.mean(physical ** 2))),
        "calibrated_rmse_mm": float(np.sqrt(np.mean(calibrated ** 2))),
        "physical_mae_mm": float(np.mean(np.abs(physical))),
        "calibrated_mae_mm": float(np.mean(np.abs(calibrated))),
        "rmse_reduction_pct": float(100 * (1 - np.sqrt(np.mean(calibrated ** 2))
                                             / np.sqrt(np.mean(physical ** 2)))),
        "mae_reduction_pct": float(100 * (1 - np.mean(np.abs(calibrated))
                                            / np.mean(np.abs(physical)))),
        "offline_capture_aligned": {
            "physical_map_error_mm": stats(aligned_physical),
            "calibrated_map_error_mm": stats(aligned_calibrated),
            "physical_rmse_mm": float(np.sqrt(np.mean(aligned_physical ** 2))),
            "calibrated_rmse_mm": float(np.sqrt(np.mean(aligned_calibrated ** 2))),
            "physical_mae_mm": float(np.mean(np.abs(aligned_physical))),
            "calibrated_mae_mm": float(np.mean(np.abs(aligned_calibrated))),
        },
        "physical_selection": selection_summary(trials, "physical"),
        "calibrated_selection": selection_summary(trials, "calibrated"),
        "aligned_calibrated_selection": selection_summary(
            trials, "aligned_calibrated"
        ),
    }


def speed_effects(trials):
    groups = defaultdict(list)
    for row in trials:
        groups[(row["direction"], row["from_mm"], row["to_mm"])].append(row)
    centered_speed = []
    centered_physical_error = []
    centered_calibrated_error = []
    centered_aligned_physical_error = []
    centered_aligned_calibrated_error = []
    for rows in groups.values():
        speeds = np.asarray([row["speed_mm_s"] for row in rows])
        physical = np.asarray([row["physical_error_mm"] for row in rows])
        calibrated = np.asarray([row["calibrated_error_mm"] for row in rows])
        aligned_physical = np.asarray([
            row["aligned_physical_error_mm"] for row in rows
        ])
        aligned_calibrated = np.asarray([
            row["aligned_calibrated_error_mm"] for row in rows
        ])
        centered_speed.extend((speeds - np.mean(speeds)).tolist())
        centered_physical_error.extend((physical - np.mean(physical)).tolist())
        centered_calibrated_error.extend((calibrated - np.mean(calibrated)).tolist())
        centered_aligned_physical_error.extend(
            (aligned_physical - np.mean(aligned_physical)).tolist()
        )
        centered_aligned_calibrated_error.extend(
            (aligned_calibrated - np.mean(aligned_calibrated)).tolist()
        )
    return {
        "speed_mm_s": stats([row["speed_mm_s"] for row in trials]),
        "within_interval_speed_vs_physical_error_r": float(np.corrcoef(
            centered_speed, centered_physical_error
        )[0, 1]),
        "within_interval_speed_vs_calibrated_error_r": float(np.corrcoef(
            centered_speed, centered_calibrated_error
        )[0, 1]),
        "within_interval_speed_vs_aligned_physical_error_r": float(np.corrcoef(
            centered_speed, centered_aligned_physical_error
        )[0, 1]),
        "within_interval_speed_vs_aligned_calibrated_error_r": float(np.corrcoef(
            centered_speed, centered_aligned_calibrated_error
        )[0, 1]),
    }


def measurement_comparison(intervals):
    onboard_minus_aligned = np.asarray([
        row["onboard_mm"] - row["aligned_interpolated_mm"] for row in intervals
    ])
    arrival_minus_aligned = np.asarray([
        row["arrival_interpolated_mm"] - row["aligned_interpolated_mm"]
        for row in intervals
    ])
    alignments = [
        abs(value) / 1000.0 for row in intervals
        for value in (row["from_alignment_us"], row["to_alignment_us"])
        if value is not None
    ]
    return {
        "onboard_nearest_snapshot_minus_aligned_interpolation_mm": stats(
            onboard_minus_aligned
        ),
        "arrival_time_interpolation_minus_capture_aligned_mm": stats(
            arrival_minus_aligned
        ),
        "absolute_logged_event_alignment_ms": stats(alignments),
        "one_pulse_mm": PITCH_MM,
        "two_endpoint_uniform_phase_sd_mm": PITCH_MM / math.sqrt(6),
    }


def repeatability_summary(groups):
    repeated = [rows for rows in groups.values() if len(rows) >= 2]
    onboard_sd = [np.std([row["onboard_mm"] for row in rows], ddof=1)
                  for rows in repeated]
    aligned_sd = [np.std([row["aligned_interpolated_mm"] for row in rows], ddof=1)
                  for rows in repeated]
    onboard_range = [np.ptp([row["onboard_mm"] for row in rows])
                     for rows in repeated]
    aligned_range = [np.ptp([row["aligned_interpolated_mm"] for row in rows])
                     for rows in repeated]
    return {
        "onboard_group_sd_mm": stats(onboard_sd),
        "onboard_group_range_mm": stats(onboard_range),
        "capture_aligned_group_sd_mm": stats(aligned_sd),
        "capture_aligned_group_range_mm": stats(aligned_range),
        "groups_by_direction": dict(Counter(key[0] for key in groups)),
        "interval_observations_by_direction": dict(Counter(
            row["direction"] for rows in groups.values() for row in rows
        )),
    }


def loop_distances(data, hall, ir):
    primaries = [event for event in data["events"]
                 if event.get("ruling") == HIGH_CONFIDENCE
                 and event.get("dir") == "CW" and event.get("mm") == 0]
    rows = []
    for left, right in zip(primaries, primaries[1:]):
        distance = right["aligned_ir_mm"] - left["aligned_ir_mm"]
        inferred_laps = max(1, int(round(distance / 52150.0)))
        rows.append({
            "mm": 0,
            "from_serial": int(left["event_serial"]),
            "to_serial": int(right["event_serial"]),
            "distance_mm": distance,
            "inferred_laps": inferred_laps,
            "route_reference_mm": inferred_laps * 52150.0,
            "error_pct": 100 * (distance / (inferred_laps * 52150.0) - 1),
        })
    return rows


def mm115_detail(intervals, ir, data):
    event_by_serial = {int(event["event_serial"]): event for event in data["events"]}
    seq_index = {int(seq): index for index, seq in enumerate(ir["sequence"])}
    rows = []
    for row in intervals:
        if row["direction"] != "CW" or row["from_mm"] != 115 or row["to_mm"] != 116:
            continue
        start = event_by_serial[row["from_serial"]]
        end = event_by_serial[row["to_serial"]]
        start_index = seq_index.get(start.get("logged_ir_seq"))
        end_index = seq_index.get(end.get("logged_ir_seq"))
        counters = {}
        missing = None
        if start_index is not None and end_index is not None:
            for name in ("unreliable_samples", "saturated_samples", "sample_gaps",
                         "open_aborts", "inferred_added", "inferred_removed"):
                counters[name] = int(ir[name][end_index] - ir[name][start_index])
            seq = ir["sequence"][start_index:end_index + 1]
            missing = int(np.sum(np.maximum(np.diff(seq) - 1, 0)))
        rows.append({
            **{key: row[key] for key in (
                "from_serial", "to_serial", "onboard_pulses", "onboard_mm",
                "aligned_interpolated_mm", "arrival_interpolated_mm", "duration_s",
                "speed_mm_s", "from_alignment_us", "to_alignment_us",
            )},
            "missing_snapshot_positions_between_endpoints": missing,
            **{f"delta_{key}": value for key, value in counters.items()},
        })
    return rows


def write_csv(path, rows):
    if not rows:
        return
    flat = []
    for row in rows:
        flat.append({key: value for key, value in row.items()
                     if not isinstance(value, (dict, list))})
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(flat[0]))
        writer.writeheader()
        writer.writerows(flat)


def plots(output, ir, intervals, trials, mm115):
    import matplotlib.pyplot as plt

    output.mkdir(parents=True, exist_ok=True)
    capture_delta = np.diff(ir["captured_us"]) / 1000.0
    pulse_delta = np.diff(ir["completed_pulses"])
    fig, axes = plt.subplots(2, 2, figsize=(12, 7.5))
    axes[0, 0].hist(ir["excess_latency_us"] / 1000.0, bins=60, color="#185abc")
    axes[0, 0].set_xlabel("IR arrival latency above minimum (ms)")
    axes[0, 0].set_ylabel("Snapshots")
    axes[0, 1].hist(capture_delta, bins=np.arange(70, 141, 2), color="#188038")
    axes[0, 1].set_xlabel("Type-5 capture interval (ms)")
    axes[0, 1].set_ylabel("Snapshot pairs")
    axes[1, 0].hist(pulse_delta, bins=np.arange(-0.5, 7.5, 1), color="#7b1fa2")
    axes[1, 0].set_xlabel("Completed-pulse increment per accepted snapshot")
    axes[1, 0].set_ylabel("Snapshot pairs")
    delta = [row["onboard_mm"] - row["aligned_interpolated_mm"] for row in intervals]
    axes[1, 1].hist(delta, bins=np.arange(-16, 16.5, 1), color="#d93025")
    axes[1, 1].axvline(0, color="#202124", lw=0.8)
    axes[1, 1].set_xlabel("Onboard nearest snapshot − aligned interpolation (mm)")
    axes[1, 1].set_ylabel("Hall intervals")
    for axis in axes.flat:
        axis.grid(alpha=0.15)
    fig.tight_layout()
    fig.savefig(output / "ir_timing_and_quantization.png", dpi=180, bbox_inches="tight")
    plt.close(fig)

    physical = [row["physical_error_mm"] for row in trials]
    calibrated = [row["calibrated_error_mm"] for row in trials]
    aligned_physical = [row["aligned_physical_error_mm"] for row in trials]
    aligned_calibrated = [row["aligned_calibrated_error_mm"] for row in trials]
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.2), sharey=True)
    bins = np.arange(-50, 51, 5)
    for axis, first, second, title in (
        (axes[0], physical, calibrated, "NAVI integer-pulse intervals"),
        (axes[1], aligned_physical, aligned_calibrated,
         "Offline capture-time interpolation"),
    ):
        axis.hist(first, bins=bins, alpha=0.65, color="#d93025",
                  label="capture-era physical map")
        axis.hist(second, bins=bins, alpha=0.65, color="#185abc",
                  label="held-out opening map")
        axis.axvline(0, color="#202124", lw=0.8)
        axis.set_xlabel("Genuine opening prediction error (mm)")
        axis.set_title(title)
        axis.grid(alpha=0.15)
    axes[0].set_ylabel("Held-out passages")
    axes[1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(output / "physical_vs_calibrated_prediction.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    multi = [row for row in trials if row["false_candidate_count"]]
    if multi:
        example = max(multi, key=lambda row: row["false_candidate_count"])
        false_distances = [item["distance_mm"] for item in example["candidates"]
                           if not item["genuine"]]
        fig, axis = plt.subplots(figsize=(9, 2.8))
        axis.scatter(false_distances, [1] * len(false_distances), marker="x",
                     s=70, color="#d93025", label="false Hall candidates")
        axis.scatter([example["onboard_mm"]], [1], marker="o", s=75,
                     color="#188038", label="genuine opening")
        axis.axvline(example["map_mm"], color="#202124", ls="--",
                     label=f'physical map {example["map_mm"]:.0f} mm')
        axis.axvline(example["calibrated_expected_mm"], color="#185abc", ls=":",
                     label=f'onboard calibration {example["calibrated_expected_mm"]:.1f} mm')
        axis.axvline(example["aligned_calibrated_expected_mm"], color="#7b1fa2",
                     ls="-.", label=("offline calibration "
                                      f'{example["aligned_calibrated_expected_mm"]:.1f} mm'))
        axis.set_yticks([])
        axis.set_xlabel("Distance from previous accepted Hall opening (mm)")
        axis.set_title(f'MM{example["from_mm"]:03d}→MM{example["to_mm"]:03d}, '
                       f'events {example["from_serial"]}→{example["to_serial"]}')
        axis.legend(ncol=2, fontsize=8, loc="lower left")
        axis.grid(axis="x", alpha=0.15)
        fig.tight_layout()
        fig.savefig(output / "candidate_selection_example.png", dpi=180,
                    bbox_inches="tight")
        plt.close(fig)

    mixed = [row for row in trials if row["false_candidate_count"]]
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.2), sharey=True)
    for axis, expected_key, title in (
        (axes[0], "map_mm", "Physical map expectation"),
        (axes[1], "calibrated_expected_mm", "Calibrated opening expectation"),
    ):
        for row_index, row in enumerate(mixed):
            expected = row[expected_key]
            original = next(item for item in intervals
                            if item["to_serial"] == row["to_serial"])
            for candidate in original["candidates"]:
                axis.scatter(candidate["distance_mm"] - expected, row_index,
                             color="#185abc" if candidate["genuine"] else "#d93025",
                             marker="o" if candidate["genuine"] else "x", s=28)
        axis.axvline(0, color="#202124", lw=0.8)
        axis.set_xlabel("Candidate distance − expectation (mm)")
        axis.set_title(title)
        axis.grid(alpha=0.15)
    axes[0].set_ylabel("Mixed-candidate held-out encounter")
    fig.tight_layout()
    fig.savefig(output / "candidate_selection_comparison.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    if mm115:
        x = np.arange(len(mm115))
        width = 0.25
        fig, axis = plt.subplots(figsize=(8.5, 4.4))
        axis.bar(x - width, [row["onboard_mm"] for row in mm115], width,
                 label="NAVI integer-pulse distance", color="#185abc")
        axis.bar(x, [row["aligned_interpolated_mm"] for row in mm115], width,
                 label="capture-time aligned interpolation", color="#188038")
        axis.bar(x + width, [row["arrival_interpolated_mm"] for row in mm115], width,
                 label="arrival-time interpolation", color="#f9ab00")
        axis.axhline(300, color="#202124", ls="--", label="mapped 300 mm")
        axis.set_xticks(x, [f"events {row['from_serial']}→{row['to_serial']}\n"
                            f"{row['speed_mm_s']:.0f} mm/s" for row in mm115])
        axis.set_ylabel("Opening-to-opening distance (mm)")
        axis.set_title("MM115→116: variation is present in integer pulse counts")
        axis.legend(fontsize=8)
        axis.grid(axis="y", alpha=0.15)
        fig.tight_layout()
        fig.savefig(output / "mm115_116_diagnostic.png", dpi=180,
                    bbox_inches="tight")
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("analysis_json", type=Path)
    parser.add_argument("--base-script", type=Path, required=True)
    parser.add_argument("--logs", nargs="+", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    base = load_module(args.base_script, "continuous_hall")
    data = json.loads(args.analysis_json.read_text())
    hall, _old_ir, _gaps, _rings = base.load_capture(args.capture)
    ir = load_ir(args.capture, base)
    global_offset = prepare_ir(ir)
    telemetry = load_telemetry(args.logs)
    alignment_reconstruction = add_event_positions(data, hall, ir, telemetry)
    intervals = build_intervals(data)
    trials, groups = evaluate_calibration(intervals)
    mm115 = mm115_detail(intervals, ir, data)
    loops = loop_distances(data, hall, ir)

    repeated = {key: rows for key, rows in groups.items() if len(rows) >= 2}
    interval_means = []
    for (direction, from_mm, to_mm), rows in sorted(groups.items()):
        values = [row["onboard_mm"] for row in rows]
        aligned_values = [row["aligned_interpolated_mm"] for row in rows]
        interval_means.append({
            "direction": direction,
            "from_mm": from_mm,
            "to_mm": to_mm,
            "passages": len(rows),
            "map_mm": rows[0]["map_mm"],
            "calibrated_opening_mm": float(np.mean(values)),
            "median_opening_mm": float(np.median(values)),
            "sd_mm": float(np.std(values, ddof=1)) if len(values) >= 2 else None,
            "range_mm": float(np.ptp(values)),
            "mean_offset_from_physical_map_mm": float(np.mean(values) - rows[0]["map_mm"]),
            "aligned_calibrated_opening_mm": float(np.mean(aligned_values)),
            "aligned_sd_mm": (float(np.std(aligned_values, ddof=1))
                              if len(aligned_values) >= 2 else None),
            "aligned_range_mm": float(np.ptp(aligned_values)),
        })

    summary = {
        "ir_integrity": ir_integrity(ir, global_offset),
        "alignment_log_reconstruction_error_us": alignment_reconstruction,
        "measurement_comparison": measurement_comparison(intervals),
        "high_confidence_intervals": len(intervals),
        "directed_interval_groups": len(groups),
        "repeated_directed_interval_groups": len(repeated),
        "repeatability": repeatability_summary(groups),
        "calibration": calibration_summary(trials),
        "speed_effects": speed_effects(trials),
        "full_loop_aligned_interpolation_mm": stats([row["distance_mm"] for row in loops]),
        "full_loop_count": len(loops),
        "single_loop_route_reference_mm": 52150,
        "mm115_to_116": mm115,
        "limitations": {
            "individual_pulse_timestamps_recorded": False,
            "distance_validated_flag": sorted(set(map(int, ir["distance_validated"]))),
            "independent_physical_truth_in_september_28_capture": False,
        },
    }

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(args.output_dir / "calibrated_opening_intervals.csv", interval_means)
    flat_trials = []
    for row in trials:
        flat = {key: value for key, value in row.items()
                if not isinstance(value, dict)}
        for prefix in ("physical", "calibrated", "aligned_calibrated"):
            selection = row[f"{prefix}_selection"]
            for key, value in selection.items():
                if key != "winner_serials":
                    flat[f"{prefix}_{key}"] = value
        flat_trials.append(flat)
    write_csv(args.output_dir / "held_out_candidate_trials.csv", flat_trials)
    write_csv(args.output_dir / "mm115_116_diagnostic.csv", mm115)
    write_csv(args.output_dir / "full_loop_distances.csv", loops)
    (args.output_dir / "ir_calibration_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n"
    )
    plots(args.output_dir / "plots", ir, intervals, trials, mm115)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
