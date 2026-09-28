#!/usr/bin/env python3
"""Analyze native-rate synchronized Hall/IR spatial-reference candidates.

Research tooling only.  This script does not classify Hall encounters, choose
thresholds, or feed any result into NAVI.  It compares representations of the
same recorded Hall samples, using accepted cumulative IR pulses only as an
offline distance coordinate.
"""

from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import math
import statistics
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import navi_sync_format as F  # noqa: E402


IR_MM_PER_PULSE = 9.652
MIN_ZERO_RUN_US = 10_000_000
WIRE_FMT = "<HBBI" + "Q" * 10 + "IIQBBHH"
WIRE_KEYS = (
    "magic", "version", "type", "sequence", "boot_id", "captured_us",
    "observed_rises", "completed_pulses", "inferred_added", "inferred_removed",
    "unreliable_samples", "saturated_samples", "sample_gaps", "open_aborts",
    "calibration_id", "pitch_um", "nominal_um", "optical_reason",
    "distance_validated", "span", "crc",
)
assert struct.calcsize(WIRE_FMT) == 110


def wire_fields(raw: bytes) -> dict:
    return dict(zip(WIRE_KEYS, struct.unpack(WIRE_FMT, raw)))


def median(values):
    return statistics.median(values) if values else None


def weighted_median(values):
    """Return the first value at or above half the supplied sample weight."""
    if not values:
        return None
    ordered = sorted((value, weight) for value, weight in values if weight > 0)
    if not ordered:
        return None
    halfway = sum(weight for _value, weight in ordered) / 2.0
    accumulated = 0.0
    for value, weight in ordered:
        accumulated += weight
        if accumulated >= halfway:
            return value
    return ordered[-1][0]


def quantiles(values):
    values = sorted(value for value in values if value is not None)
    if not values:
        return {"n": 0, "p50": None, "p95": None, "max": None}
    return {
        "n": len(values),
        "p50": median(values),
        "p95": values[int((len(values) - 1) * 0.95)],
        "max": values[-1],
    }


def load_capture(path: Path):
    hall = []
    ir = []
    status = []
    datagrams = 0
    bad = 0
    gaps = 0
    missing = 0
    sessions = set()
    previous = {}

    for _recv_us, data in F.iter_capture(str(path)):
        datagrams += 1
        try:
            header, payload = F.parse_record(data)
        except F.BadRecord:
            bad += 1
            continue

        sessions.add((header.loco_id, header.session_id, header.loco_boot_id))
        key = (header.session_id, header.rec_type)
        old = previous.get(key)
        if old is not None and header.batch_seq > old + 1:
            gaps += 1
            missing += header.batch_seq - old - 1
        previous[key] = max(header.batch_seq, old if old is not None else header.batch_seq)

        if header.rec_type == F.REC_HALL:
            timestamp = header.t0_us
            for index, sample in enumerate(F.iter_hall(header, payload)):
                if index:
                    timestamp += sample["dt_us"]
                hall.append({
                    "t_us": timestamp,
                    "dt_us": sample["dt_us"],
                    "median": sample["median"],
                    "pwm_actual": sample["pwm_actual"],
                    "pwm_commanded": sample["pwm_commanded"],
                    "flags": sample["flags"],
                    "sample_seq": sample["sample_seq"],
                })
        elif header.rec_type == F.REC_IR:
            item = F.parse_ir(payload)
            wire = wire_fields(item["wire"])
            ir.append({
                "t_us": header.t0_us,
                "batch_seq": header.batch_seq,
                "nav_mm": header.nav_mm,
                "nav_dir": header.nav_dir,
                "ctx_flags": header.ctx_flags,
                "station_phase": header.station_phase,
                "health_fault": item["health_fault"],
                "readiness": item["readiness"],
                "pwm_actual": item["pwm_actual"],
                "pwm_commanded": item["pwm_commanded"],
                "wire": wire,
            })
        elif header.rec_type == F.REC_STATUS:
            item = F.parse_status(payload)
            item["t_us"] = header.t0_us
            item["batch_seq"] = header.batch_seq
            status.append(item)

    hall.sort(key=lambda row: row["t_us"])
    ir.sort(key=lambda row: row["t_us"])
    return {
        "hall": hall,
        "ir": ir,
        "status": status,
        "integrity": {
            "datagrams": datagrams,
            "bad": bad,
            "sequence_gaps": gaps,
            "missing_batches": missing,
            "sessions": [
                {"loco_id": loco, "session_id": session, "loco_boot_id": boot}
                for loco, session, boot in sorted(sessions)
            ],
        },
    }


class PulseCoordinate:
    def __init__(self, ir_rows):
        self.rows = ir_rows
        self.times = [row["t_us"] for row in ir_rows]

    def at(self, timestamp):
        index = bisect.bisect_right(self.times, timestamp) - 1
        if index < 0:
            return float(self.rows[0]["wire"]["completed_pulses"])
        if index >= len(self.rows) - 1:
            return float(self.rows[-1]["wire"]["completed_pulses"])
        left = self.rows[index]
        right = self.rows[index + 1]
        if right["t_us"] <= left["t_us"]:
            return float(left["wire"]["completed_pulses"])
        fraction = (timestamp - left["t_us"]) / (right["t_us"] - left["t_us"])
        return left["wire"]["completed_pulses"] + fraction * (
            right["wire"]["completed_pulses"] - left["wire"]["completed_pulses"]
        )

    def bracket_advances(self, timestamp):
        index = bisect.bisect_right(self.times, timestamp) - 1
        return (
            0 <= index < len(self.rows) - 1
            and self.rows[index + 1]["wire"]["completed_pulses"]
            > self.rows[index]["wire"]["completed_pulses"]
        )


def known_position_points(ir_rows):
    points = []
    previous = 255
    for row in ir_rows:
        nav_mm = row["nav_mm"]
        if nav_mm != previous and nav_mm != 255:
            points.append({
                "t_us": row["t_us"],
                "nav_mm": nav_mm,
                "pulses": row["wire"]["completed_pulses"],
                "ctx_flags": row["ctx_flags"],
            })
            previous = nav_mm
    return points


def spatial_representative(samples, start_pulses, bin_mm):
    bins = {}
    for sample in samples:
        distance_mm = (sample["pulses"] - start_pulses) * IR_MM_PER_PULSE
        key = math.floor(distance_mm / bin_mm)
        bins.setdefault(key, []).append(sample["value"])
    representatives = {
        key: median(values) for key, values in sorted(bins.items())
    }
    return median(list(representatives.values())), representatives


def interval_summary(hall, ir_rows, coordinate, start, end, start_mm, end_mm):
    hall_times = [row["t_us"] for row in hall]
    left = bisect.bisect_left(hall_times, start["t_us"])
    right = bisect.bisect_left(hall_times, end["t_us"])
    raw_samples = []
    progress_samples = []
    for row in hall[left:right]:
        pulses = coordinate.at(row["t_us"])
        sample = {"value": row["median"], "pulses": pulses, "dt_us": row["dt_us"]}
        raw_samples.append(sample)
        if row["pwm_actual"] > 0 and coordinate.bracket_advances(row["t_us"]):
            progress_samples.append(sample)

    result = {
        "from_mm": start_mm,
        "to_mm": end_mm,
        "start_us": start["t_us"],
        "end_us": end["t_us"],
        "duration_s": (end["t_us"] - start["t_us"]) / 1_000_000.0,
        "hall_samples": len(raw_samples),
        "moving_hall_samples": len(progress_samples),
        "ir_start_pulses": coordinate.at(start["t_us"]),
        "ir_end_pulses": coordinate.at(end["t_us"]),
        "ir_distance_mm": (coordinate.at(end["t_us"]) - coordinate.at(start["t_us"])) * IR_MM_PER_PULSE,
        "time_weighted_median": weighted_median(
            [(sample["value"], sample["dt_us"]) for sample in raw_samples]
        ),
        "pwm_positive_time_weighted_median": None,
        "ir_progress_median": weighted_median(
            [(sample["value"], sample["dt_us"]) for sample in progress_samples]
        ),
        "spatial": {},
    }

    # The list-index lookup above is correct but needlessly expensive for a
    # long interval. Recompute this one field in one pass without changing its
    # definition; this also keeps the implementation readable in the JSON.
    pwm_values = []
    for row in hall[left:right]:
        if row["pwm_actual"] > 0:
            pwm_values.append((row["median"], row["dt_us"]))
    result["pwm_positive_time_weighted_median"] = weighted_median(pwm_values)

    for bin_mm in (25, 50, 100):
        representative, bins = spatial_representative(
            raw_samples, result["ir_start_pulses"], bin_mm
        )
        result["spatial"][str(bin_mm)] = {
            "median_of_bin_representatives": representative,
            "nonempty_bins": len(bins),
            "bin_representatives": bins,
        }

    faults = []
    ir_times = [row["t_us"] for row in ir_rows]
    ir_left = bisect.bisect_left(ir_times, start["t_us"])
    ir_right = bisect.bisect_left(ir_times, end["t_us"])
    for row in ir_rows[ir_left:ir_right]:
        faults.append(row["health_fault"])
    result["ir_health_faults"] = dict(Counter(faults))
    return result


def zero_progress_runs(ir_rows, hall, coordinate):
    runs = []
    start = None
    for index in range(1, len(ir_rows)):
        same = (
            ir_rows[index]["wire"]["completed_pulses"]
            == ir_rows[index - 1]["wire"]["completed_pulses"]
        )
        if same and start is None:
            start = index - 1
        if not same and start is not None:
            end = index - 1
            if ir_rows[end]["t_us"] - ir_rows[start]["t_us"] >= MIN_ZERO_RUN_US:
                runs.append((ir_rows[start], ir_rows[end]))
            start = None
    if start is not None:
        end = len(ir_rows) - 1
        if ir_rows[end]["t_us"] - ir_rows[start]["t_us"] >= MIN_ZERO_RUN_US:
            runs.append((ir_rows[start], ir_rows[end]))

    hall_times = [row["t_us"] for row in hall]
    output = []
    for first, last in runs:
        left = bisect.bisect_left(hall_times, first["t_us"])
        right = bisect.bisect_right(hall_times, last["t_us"])
        values = [row["median"] for row in hall[left:right]]
        output.append({
            "start_us": first["t_us"],
            "end_us": last["t_us"],
            "duration_s": (last["t_us"] - first["t_us"]) / 1_000_000.0,
            "pulses": first["wire"]["completed_pulses"],
            "ir_distance_mm": 0.0,
            "hall_samples": len(values),
            "hall_median": median(values),
            "hall_min": min(values) if values else None,
            "hall_max": max(values) if values else None,
            "nav_mm": first["nav_mm"],
            "ctx_flags": first["ctx_flags"],
        })
    return output


def summarize_following(intervals):
    for index, item in enumerate(intervals[:-1]):
        if item["from_mm"] == 55 and item["to_mm"] == 56:
            following = intervals[index + 1]
            hall_values = following.pop("_hall_values", [])
            result = {
                "reference_interval": "55->56",
                "following_interval": "56->57",
                "candidate_references": {},
                "following_hall_min": min(hall_values) if hall_values else None,
                "following_hall_max": max(hall_values) if hall_values else None,
                "following_hall_span": (
                    max(hall_values) - min(hall_values) if hall_values else None
                ),
            }
            candidates = {
                "time_weighted": item["time_weighted_median"],
                "pwm_positive": item["pwm_positive_time_weighted_median"],
                "spatial_25": item["spatial"]["25"]["median_of_bin_representatives"],
                "spatial_50": item["spatial"]["50"]["median_of_bin_representatives"],
                "spatial_100": item["spatial"]["100"]["median_of_bin_representatives"],
            }
            for name, value in candidates.items():
                result["candidate_references"][name] = {
                    "value": value,
                    "following_min_residual": min(hall_values) - value if hall_values else None,
                    "following_max_residual": max(hall_values) - value if hall_values else None,
                }
            return result
    return None


def analyze(path: Path):
    decoded = load_capture(path)
    hall = decoded["hall"]
    ir_rows = decoded["ir"]
    status = decoded["status"]
    coordinate = PulseCoordinate(ir_rows)
    points = known_position_points(ir_rows)
    intervals = []
    hall_times = [row["t_us"] for row in hall]
    for start, end in zip(points, points[1:]):
        item = interval_summary(hall, ir_rows, coordinate, start, end,
                                start["nav_mm"], end["nav_mm"])
        left = bisect.bisect_left(hall_times, start["t_us"])
        right = bisect.bisect_left(hall_times, end["t_us"])
        item["_hall_values"] = [row["median"] for row in hall[left:right]]
        intervals.append(item)

    comparisons = []
    for item in intervals:
        progress = item["ir_progress_median"]
        if progress is None:
            continue
        values = {
            "time_weighted": item["time_weighted_median"],
            "pwm_positive": item["pwm_positive_time_weighted_median"],
            "spatial_25": item["spatial"]["25"]["median_of_bin_representatives"],
            "spatial_50": item["spatial"]["50"]["median_of_bin_representatives"],
            "spatial_100": item["spatial"]["100"]["median_of_bin_representatives"],
        }
        comparisons.append({
            "from_mm": item["from_mm"],
            "to_mm": item["to_mm"],
            "moving_reference": progress,
            "errors": {
                name: abs(value - progress) if value is not None else None
                for name, value in values.items()
            },
            "spatial_bins": {
                str(width): item["spatial"][str(width)]["nonempty_bins"]
                for width in (25, 50, 100)
            },
        })

    error_summary = {}
    for name in ("time_weighted", "pwm_positive", "spatial_25", "spatial_50", "spatial_100"):
        errors = [row["errors"][name] for row in comparisons if row["errors"][name] is not None]
        error_summary[name] = quantiles(errors)

    covered_error_summary = {}
    for name in ("spatial_25", "spatial_50", "spatial_100"):
        errors = [
            row["errors"][name] for row in comparisons
            if row["errors"][name] is not None
            and row["spatial_bins"][name.removeprefix("spatial_")] >= 3
        ]
        covered_error_summary[name] = quantiles(errors)

    auto_running = next((row["t_us"] for row in ir_rows if row["ctx_flags"] & 0x04), None)
    first_known = points[0]["t_us"] if points else None
    first_pulse = next((row["t_us"] for row in ir_rows
                        if row["wire"]["completed_pulses"] > ir_rows[0]["wire"]["completed_pulses"]), None)

    following = summarize_following(intervals)
    for item in intervals:
        item.pop("_hall_values", None)

    status_keys = (
        "hall_ring_drops", "ir_ring_drops", "ir_input_queue_drops",
        "udp_failures", "max_hall_gap_us", "datagrams_sent",
    )
    status_summary = {
        "first": {key: status[0].get(key) for key in status_keys} if status else {},
        "last": {key: status[-1].get(key) for key in status_keys} if status else {},
        "max": {
            key: max((row.get(key, 0) for row in status), default=None)
            for key in status_keys
        },
    }

    return {
        "tool": "tools/analyze_nsr_spatial_hall_reference.py",
        "scope": "research/evaluation only; no encounter recognition or NAVI behavior",
        "source": str(path),
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "ir_mm_per_pulse": IR_MM_PER_PULSE,
        "capture_integrity": decoded["integrity"],
        "status_counters": status_summary,
        "counts": {
            "hall_samples": len(hall),
            "ir_snapshots": len(ir_rows),
            "status_records": len(status),
            "known_position_points": len(points),
            "hall_time_us": {"first": hall[0]["t_us"], "last": hall[-1]["t_us"]},
            "ir_time_us": {"first": ir_rows[0]["t_us"], "last": ir_rows[-1]["t_us"]},
        },
        "derived_anchors": {
            "first_known_nav_us": first_known,
            "first_pulse_advance_us": first_pulse,
            "first_auto_running_us": auto_running,
        },
        "zero_progress_runs": zero_progress_runs(ir_rows, hall, coordinate),
        "interval_error_summary_vs_ir_progress_reference": error_summary,
        "interval_error_summary_with_at_least_three_spatial_bins": covered_error_summary,
        "following_magnet_test": following,
        "intervals": intervals,
        "method": {
            "time_weighted": "Hall median samples weighted by recorded dt_us.",
            "pwm_positive": "Same dt_us-weighted median restricted to applied PWM > 0.",
            "ir_progress_reference": "Diagnostic comparator: PWM > 0 Hall samples whose enclosing accepted IR snapshots advance cumulative pulses.",
            "spatial": "Map each Hall sample to linearly interpolated accepted IR cumulative-pulse distance; take one unweighted Hall median per nonempty fixed-distance bin, then the median of bin representatives.",
            "spatial_bins_mm": [25, 50, 100],
            "not_production": "Bin widths and the IR-progress comparator are diagnostic choices, not firmware constants or navigation rules.",
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-label", default=None,
                        help="path to report as provenance instead of the local copy")
    args = parser.parse_args()
    result = analyze(args.capture)
    if args.source_label:
        result["source"] = args.source_label
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({
        "output": str(args.output),
        "hall_samples": result["counts"]["hall_samples"],
        "ir_snapshots": result["counts"]["ir_snapshots"],
        "sequence_gaps": result["capture_integrity"]["sequence_gaps"],
        "missing_batches": result["capture_integrity"]["missing_batches"],
        "zero_progress_runs": len(result["zero_progress_runs"]),
    }, indent=2))


if __name__ == "__main__":
    main()
