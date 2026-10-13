#!/usr/bin/env python3
"""Research-only analysis of the 2026-09-28 continuous Otto Hall/IR capture."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

import numpy as np

REPO = Path("/Users/davidbrown/NGR/NGR-Files/navi-ewo-0-3-stop-integration")
sys.path.insert(0, str(REPO / "tools"))
import navi_sync_format as F  # noqa: E402

HALL_DTYPE = np.dtype([
    ("dt", "<u2"), ("raw", "<u2", (5,)), ("median", "<i2"),
    ("pwm_actual", "u1"), ("pwm_commanded", "u1"),
    ("flags", "u1"), ("reserved", "u1"),
])
WIRE_FMT = "<HBBI" + "Q" * 10 + "IIQBBHH"
WIRE_KEYS = (
    "magic", "version", "type", "sequence", "boot_id", "captured_us",
    "observed_rises", "completed_pulses", "inferred_added", "inferred_removed",
    "unreliable_samples", "saturated_samples", "sample_gaps", "open_aborts",
    "calibration_id", "pitch_um", "nominal_um", "optical_reason",
    "distance_validated", "span", "crc",
)

# Route map present in the recorder build (commit d54ab3f, 2026-09-27).
ROUTE_SPACING_MM = np.array([
    330,340,330,315,325,330,315,300,300,295,
    300,290,300,315,315,325,310,300,300,320,
    315,315,305,300,295,300,300,300,300,315,
    330,320,315,310,300,300,300,300,300,300,
    300,300,300,300,300,300,300,300,300,300,
    300,300,300,300,300,300,300,295,320,300,
    315,320,315,325,315,305,300,305,300,300,
    295,295,300,300,300,300,300,300,300,305,
    300,300,305,300,300,330,300,300,305,300,
    300,300,300,300,300,300,300,300,300,300,
    300,300,295,300,300,300,300,300,305,300,
    300,320,320,300,300,300,300,300,300,300,
    300,300,300,300,300,300,280,300,300,290,
    300,300,300,300,300,300,300,300,300,300,
    300,300,300,300,305,300,305,300,295,300,
    300,300,305,300,300,305,320,290,320,300,
    300,305,330,330,320,325,315,355,330,330,
    330,
], dtype=np.int32)
ROUTE_N = len(ROUTE_SPACING_MM)
CURRENT_ROUTE_SPACING_MM = ROUTE_SPACING_MM.copy()
CURRENT_ROUTE_SPACING_MM[11:14] = [335, 324, 360]


def count_records(path: Path) -> tuple[int, int]:
    hall_count = ir_count = 0
    for _received_us, data in F.iter_capture(str(path)):
        header, _payload = F.parse_record(data)
        if header.rec_type == F.REC_HALL:
            hall_count += header.n_items
        elif header.rec_type == F.REC_IR:
            ir_count += 1
    return hall_count, ir_count


def load_capture(path: Path):
    hall_count, ir_count = count_records(path)
    hall = {
        "t_us": np.empty(hall_count, np.int64),
        "boot_ms": np.empty(hall_count, np.float64),
        "serial": np.empty(hall_count, np.uint32),
        "raw": np.empty((hall_count, 5), np.uint16),
        "median": np.empty(hall_count, np.int16),
        "pwm": np.empty(hall_count, np.uint8),
        "direction": np.empty(hall_count, np.uint8),
        "nav_mm": np.empty(hall_count, np.uint8),
        "nav_dir": np.empty(hall_count, np.int8),
    }
    ir = {
        "t_us": np.empty(ir_count, np.int64),
        "nominal_mm": np.empty(ir_count, np.float64),
        "pulses": np.empty(ir_count, np.uint64),
        "sequence": np.empty(ir_count, np.uint32),
        "boot_id": np.empty(ir_count, np.uint64),
        "pitch_um": np.empty(ir_count, np.uint32),
        "health_fault": np.empty(ir_count, np.uint8),
        "readiness": np.empty(ir_count, np.uint8),
        "sample_gaps": np.empty(ir_count, np.uint64),
        "inferred_added": np.empty(ir_count, np.uint64),
        "inferred_removed": np.empty(ir_count, np.uint64),
    }
    hi = ii = 0
    sequence_gaps = []
    previous = {}
    ring_ranges = {}
    for _received_us, data in F.iter_capture(str(path)):
        header, payload = F.parse_record(data)
        key = (header.session_id, header.rec_type)
        prior = previous.get(key)
        if prior is not None and header.batch_seq > prior + 1:
            sequence_gaps.append((key, prior + 1, header.batch_seq - 1))
        previous[key] = max(prior if prior is not None else header.batch_seq,
                            header.batch_seq)
        low, high = ring_ranges.get(key, (header.ring_drops, header.ring_drops))
        ring_ranges[key] = (min(low, header.ring_drops),
                            max(high, header.ring_drops))
        if header.rec_type == F.REC_HALL:
            array = np.frombuffer(payload, HALL_DTYPE, header.n_items)
            count = len(array)
            times = np.empty(count, np.int64)
            times[0] = header.t0_us
            if count > 1:
                times[1:] = header.t0_us + np.cumsum(array["dt"][1:], dtype=np.int64)
            sl = slice(hi, hi + count)
            hall["t_us"][sl] = times
            hall["boot_ms"][sl] = header.t0_ms + (times - header.t0_us) / 1000.0
            hall["serial"][sl] = header.first_item_seq + np.arange(count, dtype=np.uint32)
            hall["raw"][sl] = array["raw"]
            hall["median"][sl] = array["median"]
            hall["pwm"][sl] = array["pwm_actual"]
            hall["direction"][sl] = np.where(array["flags"] & F.HALL_F_DIR_FWD, 1, 2)
            hall["nav_mm"][sl] = header.nav_mm
            hall["nav_dir"][sl] = header.nav_dir
            hi += count
        elif header.rec_type == F.REC_IR:
            item = F.parse_ir(payload)
            wire = dict(zip(WIRE_KEYS, struct.unpack(WIRE_FMT, item["wire"])))
            ir["t_us"][ii] = header.t0_us
            ir["nominal_mm"][ii] = wire["nominal_um"] / 1000.0
            ir["pulses"][ii] = wire["completed_pulses"]
            ir["sequence"][ii] = wire["sequence"]
            ir["boot_id"][ii] = wire["boot_id"]
            ir["pitch_um"][ii] = wire["pitch_um"]
            ir["health_fault"][ii] = item["health_fault"]
            ir["readiness"][ii] = item["readiness"]
            ir["sample_gaps"][ii] = wire["sample_gaps"]
            ir["inferred_added"][ii] = wire["inferred_added"]
            ir["inferred_removed"][ii] = wire["inferred_removed"]
            ii += 1
    assert hi == hall_count and ii == ir_count
    return hall, ir, sequence_gaps, ring_ranges


def load_events(paths: list[Path]):
    events = {}
    for path in paths:
        with path.open(errors="replace") as stream:
            for line in stream:
                parts = line.rstrip("\n").split("\t", 2)
                if len(parts) != 3 or parts[1] not in {
                    "ngr/loco/9950011/mm/marker",
                    "ngr/loco/9950011/nav/discrepancy",
                }:
                    continue
                try:
                    item = json.loads(parts[2])
                except json.JSONDecodeError:
                    continue
                if "event_serial" not in item:
                    continue
                serial = int(item["event_serial"])
                if parts[1].endswith("mm/marker"):
                    if item.get("boot_id") != "7F0F808772045AF3":
                        continue
                    events.setdefault(serial, {}).update(item)
                else:
                    events.setdefault(serial, {}).update({
                        "captured_us": item.get("captured_us"),
                        "pre_mm": item.get("pre_mm"),
                        "distance_assessable": item.get("distance_assessable"),
                        "ir_interval": item.get("ir_interval"),
                        "ir_reason": item.get("ir_reason"),
                    })
    return [events[key] for key in sorted(events) if "opened_ms" in events[key]]


def match_events(events, hall, ir):
    for event in events:
        opened_ms = float(event["opened_ms"])
        center = int(np.searchsorted(hall["boot_ms"], opened_ms))
        lo, hi = max(0, center - 12), min(len(hall["boot_ms"]), center + 13)
        candidates = np.arange(lo, hi)
        raw_match = candidates[hall["median"][candidates] == int(event["raw_open"])]
        baseline = int(event["base_open"])
        persistent = []
        for index in raw_match:
            if index == 0:
                continue
            current = int(hall["median"][index]) - baseline
            previous = int(hall["median"][index - 1]) - baseline
            if (abs(current) >= 70 and abs(previous) >= 70
                    and np.sign(current) == np.sign(previous)):
                persistent.append(index)
        pool = np.asarray(persistent) if persistent else (raw_match if len(raw_match) else candidates)
        index = int(pool[np.argmin(np.abs(hall["boot_ms"][pool] - opened_ms))])
        event["sample_index"] = index
        event["sample_serial"] = int(hall["serial"][index])
        event["sample_time_error_ms"] = float(hall["boot_ms"][index] - opened_ms)
        event["raw_matches"] = bool(hall["median"][index] == int(event["raw_open"]))
        current = int(hall["median"][index]) - baseline
        previous = int(hall["median"][index - 1]) - baseline if index else 0
        event["two_sample_opening_matches"] = bool(
            index > 0 and abs(current) >= 70 and abs(previous) >= 70
            and np.sign(current) == np.sign(previous)
        )
        event["ir_mm"] = float(np.interp(hall["t_us"][index], ir["t_us"], ir["nominal_mm"]))
        bracket = int(np.searchsorted(ir["t_us"], hall["t_us"][index]))
        if 0 < bracket < len(ir["t_us"]):
            left, right = bracket - 1, bracket
            same_frame = (
                ir["boot_id"][left] == ir["boot_id"][right]
                and ir["pitch_um"][left] == ir["pitch_um"][right]
                and ir["sequence"][right] > ir["sequence"][left]
                and ir["pulses"][right] >= ir["pulses"][left]
                and ir["sample_gaps"][right] == ir["sample_gaps"][left]
                and ir["inferred_added"][right] == ir["inferred_added"][left]
                and ir["inferred_removed"][right] == ir["inferred_removed"][left]
            )
            event["ir_bracket_ms"] = float((ir["t_us"][right] - ir["t_us"][left]) / 1000.0)
            event["ir_reliable"] = bool(same_frame and event["ir_bracket_ms"] <= 300)
        else:
            event["ir_bracket_ms"] = None
            event["ir_reliable"] = False


def route_distance(start_mm: int, end_mm: int, direction: int,
                   spacing=ROUTE_SPACING_MM):
    if direction not in (-1, 1) or start_mm == end_mm:
        return None, 0
    current = start_mm
    total = steps = 0
    while current != end_mm and steps <= ROUTE_N:
        if direction > 0:
            total += int(spacing[current])
            current = (current + 1) % ROUTE_N
        else:
            current = (current - 1) % ROUTE_N
            total += int(spacing[current])
        steps += 1
    return (total, steps) if current == end_mm else (None, steps)


def summarize(events, hall, ir, sequence_gaps, ring_ranges):
    primary_rulings = {"ADVANCED", "ADVANCED_WITH_DISCREPANCY"}
    mapped = [event for event in events if event.get("dir") in ("CW", "CCW")]
    primaries = [event for event in mapped if event.get("ruling") in primary_rulings]
    secondaries = [event for event in mapped if event.get("ruling") == "NON_LANDMARK_HALL"]
    for event in mapped:
        event["direction_sign"] = 1 if event["dir"] == "CW" else -1

    intervals = []
    prior = None
    for event in primaries:
        if prior is not None and event["direction_sign"] == prior["direction_sign"]:
            expected, steps = route_distance(int(prior["mm"]), int(event["mm"]),
                                             event["direction_sign"])
            expected_current, current_steps = route_distance(
                int(prior["mm"]), int(event["mm"]), event["direction_sign"],
                CURRENT_ROUTE_SPACING_MM,
            )
            assert steps == current_steps
            observed = event["ir_mm"] - prior["ir_mm"]
            between = [candidate for candidate in secondaries
                       if prior["opened_ms"] < candidate["opened_ms"] < event["opened_ms"]]
            row = {
                "from_serial": int(prior["event_serial"]),
                "to_serial": int(event["event_serial"]),
                "from_mm": int(prior["mm"]),
                "to_mm": int(event["mm"]),
                "direction": event["dir"],
                "map_mm": expected,
                "current_map_mm": expected_current,
                "map_steps": steps,
                "observed_mm": observed,
                "error_mm": observed - expected if expected is not None else None,
                "current_error_mm": (
                    observed - expected_current if expected_current is not None else None
                ),
                "ir_reliable": bool(prior["ir_reliable"] and event["ir_reliable"]),
                "secondary_serials": [int(candidate["event_serial"]) for candidate in between],
            }
            for candidate in between:
                candidate["preceding_primary_serial"] = int(prior["event_serial"])
                candidate["following_primary_serial"] = int(event["event_serial"])
                candidate["distance_from_primary_mm"] = candidate["ir_mm"] - prior["ir_mm"]
                candidate["expected_next_mm"] = expected
                candidate["current_expected_next_mm"] = expected_current
                candidate["candidate_error_mm"] = (
                    candidate["distance_from_primary_mm"] - expected
                    if expected is not None else None
                )
                candidate["current_candidate_error_mm"] = (
                    candidate["distance_from_primary_mm"] - expected_current
                    if expected_current is not None else None
                )
                candidate["opposite_to_primary"] = candidate["obs"] != prior["obs"]
                candidate["primary_obs"] = prior["obs"]
                candidate.update(quiet_metrics(prior, candidate, hall, ir))
            intervals.append(row)
        prior = event

    event_by_serial = {int(event["event_serial"]): event for event in events}
    adjacent = [row for row in intervals
                if row["map_steps"] == 1 and row["ir_reliable"]
                and event_by_serial[row["to_serial"]].get("distance_assessable") == 1
                and event_by_serial[row["to_serial"]].get("pre_mm") == row["from_mm"]]
    errors = np.array([row["error_mm"] for row in adjacent], dtype=float)
    current_errors = np.array([row["current_error_mm"] for row in adjacent], dtype=float)
    with_candidates = [row for row in adjacent if row["secondary_serials"]]
    secondary_by_serial = {int(event["event_serial"]): event for event in secondaries}
    closest_true = 0
    false_in_asymmetric = 0
    false_in_symmetric = 0
    candidate_distances = []
    for row in with_candidates:
        false_errors = []
        for serial in row["secondary_serials"]:
            candidate = secondary_by_serial[serial]
            if candidate.get("candidate_error_mm") is None or not candidate["ir_reliable"]:
                continue
            false_errors.append(abs(candidate["current_candidate_error_mm"]))
            candidate_distances.append(candidate["distance_from_primary_mm"])
            d = row["current_map_mm"]
            x = candidate["distance_from_primary_mm"]
            false_in_asymmetric += int(d - 10 <= x <= d + 30)
            false_in_symmetric += int(d * 0.85 <= x <= d * 1.15)
        if false_errors and abs(row["current_error_mm"]) < min(false_errors):
            closest_true += 1

    asymmetric_true = int(np.sum((current_errors >= -10) & (current_errors <= 30)))
    symmetric_true = int(np.sum(np.abs(current_errors) <= np.array(
        [row["current_map_mm"] for row in adjacent]
    ) * 0.15))

    repeated = {}
    for row in adjacent:
        key = (row["direction"], row["from_mm"], row["to_mm"])
        repeated.setdefault(key, []).append(row["current_error_mm"])
    repeated = {key: values for key, values in repeated.items() if len(values) >= 2}
    repeated_ranges = np.array([
        max(values) - min(values) for values in repeated.values()
    ], dtype=float)
    repeated_sds = np.array([
        np.std(values) for values in repeated.values()
    ], dtype=float)
    widest_key = max(repeated, key=lambda key: max(repeated[key]) - min(repeated[key]))

    classified_secondaries = [
        event for event in secondaries
        if event.get("preceding_primary_serial") is not None
        and event.get("ir_reliable")
        and event.get("distance_assessable") == 1
        and event.get("longest_quiet_mm") is not None
    ]
    for event in classified_secondaries:
        event["secondary_class"] = (
            "same_polarity_reopening" if not event["opposite_to_primary"]
            else "separate_or_ambiguous_opposite_field"
        )
    for primary_serial in sorted(set(
            int(event["preceding_primary_serial"])
            for event in classified_secondaries)):
        group = sorted((event for event in classified_secondaries
                        if int(event["preceding_primary_serial"]) == primary_serial),
                       key=lambda event: event["opened_ms"])
        opposite = [event for event in group if event["opposite_to_primary"]]
        if not opposite:
            continue
        first = opposite[0]
        if (first["pole_crossings_outside_deadband"] == 1
                and first["longest_quiet_mm"] < 15):
            site_distance = first["distance_from_primary_mm"]
            for event in opposite:
                if abs(event["distance_from_primary_mm"] - site_distance) <= 5:
                    event["secondary_class"] = "continuous_opposite_lobe"
    lobe_events = [event for event in classified_secondaries
                   if event["secondary_class"] == "continuous_opposite_lobe"]
    lobe_sites = []
    for primary_serial in sorted(set(
            int(event["preceding_primary_serial"]) for event in lobe_events)):
        group = sorted((event for event in lobe_events
                        if int(event["preceding_primary_serial"]) == primary_serial),
                       key=lambda event: event["distance_from_primary_mm"])
        for event in group:
            if (not lobe_sites
                    or lobe_sites[-1]["preceding_primary_serial"] != primary_serial
                    or abs(lobe_sites[-1]["distance_mm"]
                           - event["distance_from_primary_mm"]) > 5):
                lobe_sites.append({
                    "preceding_primary_serial": primary_serial,
                    "distance_mm": event["distance_from_primary_mm"],
                    "event_serials": [int(event["event_serial"])],
                    "longest_quiet_mm": event["longest_quiet_mm"],
                })
            else:
                lobe_sites[-1]["event_serials"].append(int(event["event_serial"]))
    lobe_distances = np.array([site["distance_mm"] for site in lobe_sites], dtype=float)
    nonlobe_opposite = [event for event in classified_secondaries
                        if event["secondary_class"] == "separate_or_ambiguous_opposite_field"]

    summary = {
        "capture": {
            "hall_samples": len(hall["median"]),
            "individual_adc_values": int(len(hall["median"]) * 5),
            "ir_snapshots": len(ir["t_us"]),
            "duration_s": float((hall["t_us"][-1] - hall["t_us"][0]) / 1e6),
            "sha256": hashlib.sha256(Path(args.capture).read_bytes()).hexdigest(),
            "sequence_gaps": sequence_gaps,
            "ring_drop_ranges": {str(key): value for key, value in ring_ranges.items()},
        },
        "event_matching": {
            "events": len(events),
            "raw_matches": sum(bool(event["raw_matches"]) for event in events),
            "two_sample_opening_matches": sum(
                bool(event["two_sample_opening_matches"]) for event in events
            ),
            "max_abs_time_error_ms": max(abs(float(event["sample_time_error_ms"])) for event in events),
        },
        "mapped_events": {
            "primaries": len(primaries),
            "secondaries": len(secondaries),
            "primary_by_direction": {
                "CW": sum(event["dir"] == "CW" for event in primaries),
                "CCW": sum(event["dir"] == "CCW" for event in primaries),
            },
            "secondary_by_direction": {
                "CW": sum(event["dir"] == "CW" for event in secondaries),
                "CCW": sum(event["dir"] == "CCW" for event in secondaries),
            },
        },
        "adjacent_primary_intervals": {
            "n": len(adjacent),
            "error_mm_against_capture_map": distribution(errors),
            "error_mm_against_current_map": distribution(current_errors),
            "current_error_by_direction": {
                direction: distribution(np.array([
                    row["current_error_mm"] for row in adjacent
                    if row["direction"] == direction
                ], dtype=float))
                for direction in ("CW", "CCW")
            },
            "asymmetric_D_minus_10_plus_30": {
                "inside": asymmetric_true,
                "below": int(np.sum(current_errors < -10)),
                "above": int(np.sum(current_errors > 30)),
                "outside": len(current_errors) - asymmetric_true,
            },
            "symmetric_15_percent": {
                "inside": symmetric_true,
                "outside": len(current_errors) - symmetric_true,
            },
            "intervals_with_secondary_candidates": len(with_candidates),
            "true_primary_closest_to_expected": closest_true,
            "false_candidates_inside_asymmetric": false_in_asymmetric,
            "false_candidates_inside_symmetric": false_in_symmetric,
        },
        "repeat_consistency": {
            "transition_groups_with_repeats": len(repeated),
            "observations_in_repeat_groups": sum(len(values) for values in repeated.values()),
            "within_transition_range_mm": distribution(repeated_ranges),
            "within_transition_sd_mm": distribution(repeated_sds),
            "widest_transition": {
                "direction": widest_key[0],
                "from_mm": widest_key[1],
                "to_mm": widest_key[2],
                "n": len(repeated[widest_key]),
                "range_mm": max(repeated[widest_key]) - min(repeated[widest_key]),
                "errors_mm": repeated[widest_key],
            },
        },
        "secondary_classification": {
            "measured_secondary_openings_classified": len(classified_secondaries),
            "continuous_opposite_lobe_event_count": len(lobe_events),
            "continuous_opposite_lobe_unique_sites": len(lobe_sites),
            "continuous_opposite_lobe_site_distances_mm": distribution(lobe_distances),
            "lobe_sites": lobe_sites,
            "inferred_class_rule": (
                "opposite polarity, exactly one pole crossing outside the 25-count "
                "deadband, and longest quiet span below 15 mm; repeated openings "
                "within 5 mm after the same primary are one spatial site"
            ),
            "max_lobe_quiet_mm": max((event["longest_quiet_mm"] for event in lobe_events),
                                     default=None),
            "min_nonlobe_opposite_quiet_mm": min(
                (event["longest_quiet_mm"] for event in nonlobe_opposite),
                default=None,
            ),
            "minimum_observed_genuine_interval_mm": float(np.min([
                row["observed_mm"] for row in adjacent
            ])),
            "measured_gap_lobe_to_genuine_mm": (
                float(np.min([row["observed_mm"] for row in adjacent])
                      - np.max(lobe_distances)) if len(lobe_distances) else None
            ),
        },
        "representative_profiles": representative_profile_metrics(events, hall, ir),
        "secondary_distance_mm": distribution(np.array(candidate_distances, dtype=float)),
    }
    return summary, mapped, primaries, secondaries, intervals


def quiet_metrics(primary, candidate, hall, ir, deadband=25):
    start = int(primary["sample_index"])
    stop = int(candidate["sample_index"])
    if stop <= start:
        return {}
    sample_slice = slice(start, stop + 1)
    values = hall["median"][sample_slice].astype(float)
    reference = float(primary["base_open"])
    quiet = np.abs(values - reference) <= deadband
    sample_times = hall["t_us"][sample_slice]
    sample_positions = np.interp(sample_times, ir["t_us"], ir["nominal_mm"])
    padded = np.r_[False, quiet, False]
    edges = np.flatnonzero(padded[1:] != padded[:-1])
    longest_ms = longest_mm = 0.0
    for run_start, run_stop in edges.reshape(-1, 2):
        if run_stop <= run_start:
            continue
        duration_ms = (sample_times[run_stop - 1] - sample_times[run_start]) / 1000.0
        distance_mm = sample_positions[run_stop - 1] - sample_positions[run_start]
        if duration_ms > longest_ms:
            longest_ms = float(duration_ms)
        if distance_mm > longest_mm:
            longest_mm = float(distance_mm)
    signs = np.sign(values - reference)
    nonzero = signs[np.abs(values - reference) > deadband]
    pole_crossings = int(np.sum(nonzero[1:] != nonzero[:-1])) if len(nonzero) > 1 else 0
    return {
        "time_from_primary_ms": float(candidate["opened_ms"] - primary["opened_ms"]),
        "longest_quiet_ms": longest_ms,
        "longest_quiet_mm": longest_mm,
        "pole_crossings_outside_deadband": pole_crossings,
    }


def representative_profile_metrics(events, hall, ir, deadband=25):
    """Report observed samples around selected openings without fitting a field model."""
    event_by_serial = {int(event["event_serial"]): event for event in events}
    hall_positions = np.interp(hall["t_us"], ir["t_us"], ir["nominal_mm"])
    specs = ((72, 74), (161, 162), (260, 261), (324, None))
    rows = []
    for primary_serial, secondary_serial in specs:
        primary = event_by_serial[primary_serial]
        origin = primary["ir_mm"]
        start = int(primary["sample_index"])
        stop = int(np.searchsorted(hall_positions, origin + 175, side="right"))
        stop = min(stop, len(hall_positions), start + 15000)
        x = hall_positions[start:stop] - origin
        departure = hall["median"][start:stop].astype(float) - float(primary["base_open"])
        primary_sign = int(np.sign(int(primary["raw_open"]) - int(primary["base_open"])))
        secondary = event_by_serial.get(secondary_serial) if secondary_serial else None
        primary_stop = (int(secondary["sample_index"]) - start + 1
                        if secondary is not None else len(departure))
        primary_part = departure[:primary_stop]
        primary_peak_i = (int(np.argmax(primary_part)) if primary_sign > 0
                          else int(np.argmin(primary_part)))

        def first_two_quiet(after):
            quiet = np.abs(departure[after:]) <= deadband
            hits = np.flatnonzero(quiet[:-1] & quiet[1:]) if len(quiet) > 1 else []
            return float(x[after + int(hits[0])]) if len(hits) else None

        row = {
            "primary_serial": primary_serial,
            "map_marker": int(primary["mm"]),
            "primary_polarity": primary["obs"],
            "primary_open_departure_counts": int(primary["raw_open"]) - int(primary["base_open"]),
            "primary_peak_departure_counts": float(departure[primary_peak_i]),
            "primary_peak_position_mm": float(x[primary_peak_i]),
            "first_two_sample_quiet_after_primary_peak_mm": first_two_quiet(primary_peak_i + 1),
            "secondary_serial": secondary_serial,
        }
        if secondary is not None:
            secondary_i = int(secondary["sample_index"]) - start
            opposite = departure[secondary_i:]
            opposite_sign = int(np.sign(int(secondary["raw_open"]) - int(secondary["base_open"])))
            opposite_peak_relative = (int(np.argmax(opposite)) if opposite_sign > 0
                                      else int(np.argmin(opposite)))
            opposite_peak_i = secondary_i + opposite_peak_relative
            row.update({
                "secondary_polarity": secondary["obs"],
                "secondary_open_position_mm": float(secondary["ir_mm"] - origin),
                "secondary_open_departure_counts": (
                    int(secondary["raw_open"]) - int(secondary["base_open"])
                ),
                "opposite_peak_departure_from_primary_reference_counts": float(
                    departure[opposite_peak_i]
                ),
                "opposite_peak_position_mm": float(x[opposite_peak_i]),
                "first_two_sample_quiet_after_opposite_peak_mm": first_two_quiet(
                    opposite_peak_i + 1
                ),
            })
        rows.append(row)
    return rows


def distribution(values):
    values = np.asarray(values, dtype=float)
    values = values[np.isfinite(values)]
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
        "sd": float(np.std(values)),
    }


def json_safe(value):
    if isinstance(value, (np.integer,)):
        return int(value)
    if isinstance(value, (np.floating,)):
        return float(value)
    if isinstance(value, np.ndarray):
        return value.tolist()
    raise TypeError(type(value).__name__)


def make_plots(output_dir: Path, hall, ir, events, intervals, summary):
    import matplotlib.pyplot as plt

    output_dir.mkdir(parents=True, exist_ok=True)
    event_by_serial = {int(event["event_serial"]): event for event in events}

    profile_specs = [
        (72, 74, "MM062: compact bipolar field"),
        (161, 162, "MM008: long opposite lobe"),
        (260, 261, "MM129: maximum lobe reach"),
        (324, None, "MM010: opening without a secondary"),
    ]
    hall_positions = np.interp(hall["t_us"], ir["t_us"], ir["nominal_mm"])
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), sharey=True)
    for axis, (primary_serial, secondary_serial, title) in zip(axes.flat, profile_specs):
        primary = event_by_serial[primary_serial]
        origin = primary["ir_mm"]
        center = int(primary["sample_index"])
        positions = hall_positions
        lo = center
        while lo > 0 and positions[lo] - origin > -35:
            lo -= 1
        hi = center
        while hi + 1 < len(positions) and positions[hi] - origin < 175:
            hi += 1
        # Bound stationary periods so one profile cannot dominate render time.
        hi = min(hi, center + 15000)
        sample = slice(lo, hi + 1)
        x = positions[sample] - origin
        reference = float(primary["base_open"])
        raw = hall["raw"][sample].astype(float) - reference
        median = hall["median"][sample].astype(float) - reference
        stride = max(1, len(x) // 3500)
        axis.scatter(np.repeat(x[::stride], 5), raw[::stride].reshape(-1),
                     s=1.2, color="#9aa0a6", alpha=0.12, rasterized=True,
                     label="five ADC conversions" if primary_serial == 72 else None)
        axis.plot(x, median, color="#185abc", linewidth=1.0,
                  label="median used by detector" if primary_serial == 72 else None)
        axis.axhline(70, color="#b3261e", linestyle="--", linewidth=0.9)
        axis.axhline(-70, color="#b3261e", linestyle="--", linewidth=0.9,
                     label="±70 opening threshold" if primary_serial == 72 else None)
        axis.axhspan(-25, 25, color="#188038", alpha=0.08,
                     label="±25 quiet band" if primary_serial == 72 else None)
        axis.axvline(0, color="#202124", linewidth=1.1,
                     label="primary opening" if primary_serial == 72 else None)
        if secondary_serial is not None:
            secondary = event_by_serial[secondary_serial]
            sx = secondary["ir_mm"] - origin
            axis.axvline(sx, color="#d93025", linewidth=1.1,
                         label="secondary opening" if primary_serial == 72 else None)
            axis.text(sx + 2, 220, f"secondary\n{sx:.1f} mm",
                      color="#b3261e", fontsize=8, va="top")
        # Mark observed extrema; these are samples, not fitted peaks.
        for which in (int(np.argmax(median)), int(np.argmin(median))):
            axis.plot(x[which], median[which], "o", ms=3.5, color="#7b1fa2")
        axis.set_title(title, fontsize=10)
        axis.set_xlim(-35, 175)
        axis.set_ylim(-260, 260)
        axis.grid(alpha=0.18)
        axis.set_xlabel("IR displacement from primary opening (mm)")
    axes[0, 0].set_ylabel("Hall departure from recorded reference (counts)")
    axes[1, 0].set_ylabel("Hall departure from recorded reference (counts)")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=5, fontsize=8,
               bbox_to_anchor=(0.5, -0.005))
    fig.suptitle("Measured Hall profiles; no Gaussian fit or inferred field boundary", fontsize=12)
    fig.tight_layout(rect=(0, 0.04, 1, 0.96))
    fig.savefig(output_dir / "representative_field_profiles.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    lobe_distances = np.array([
        site["distance_mm"]
        for site in summary["secondary_classification"]["lobe_sites"]
    ])
    event_lookup = event_by_serial
    eligible = [row for row in intervals
                if row["map_steps"] == 1 and row["ir_reliable"]
                and event_lookup[row["to_serial"]].get("distance_assessable") == 1
                and event_lookup[row["to_serial"]].get("pre_mm") == row["from_mm"]]
    errors = np.array([row["current_error_mm"] for row in eligible])
    repeat_groups = {}
    for row in eligible:
        key = (row["direction"], row["from_mm"], row["to_mm"])
        repeat_groups.setdefault(key, []).append(row["current_error_mm"])
    repeat_ranges = np.array([max(values) - min(values)
                              for values in repeat_groups.values() if len(values) >= 2])

    fig, axes = plt.subplots(1, 3, figsize=(13, 3.8))
    bins = np.arange(35, 141, 10)
    axes[0].hist(lobe_distances, bins=bins, color="#d93025", alpha=0.78,
                 edgecolor="white")
    axes[0].plot(lobe_distances, np.full_like(lobe_distances, 0.08), "|",
                 color="#202124", markersize=12)
    axes[0].axvline(np.max(lobe_distances), color="#202124", linestyle="--")
    axes[0].set_title("Primary → opposite-lobe openings")
    axes[0].set_xlabel("Displacement (mm)")
    axes[0].set_ylabel("Unique spatial sites")

    axes[1].hist(errors, bins=np.arange(-50, 41, 5), color="#185abc",
                 alpha=0.82, edgecolor="white")
    axes[1].axvspan(-10, 30, color="#188038", alpha=0.14,
                    label="D−10 to D+30")
    axes[1].axvline(-10, color="#188038", linestyle="--")
    axes[1].axvline(30, color="#188038", linestyle="--")
    axes[1].set_title("Genuine opening interval error")
    axes[1].set_xlabel("Observed − current mapped D (mm)")
    axes[1].set_ylabel("Intervals")
    axes[1].legend(fontsize=8)

    axes[2].hist(repeat_ranges, bins=np.arange(0, 56, 5), color="#7b1fa2",
                 alpha=0.8, edgecolor="white")
    axes[2].set_title("Repeat-pass spread by transition")
    axes[2].set_xlabel("Max − min observed error (mm)")
    axes[2].set_ylabel("Repeated transitions")
    for axis in axes:
        axis.grid(alpha=0.16)
    fig.tight_layout()
    fig.savefig(output_dir / "distance_distributions.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)

    true_errors = []
    false_errors = []
    for row in eligible:
        if not row["secondary_serials"]:
            continue
        true_errors.append(row["current_error_mm"])
        for serial in row["secondary_serials"]:
            event = event_lookup[serial]
            if event.get("ir_reliable") and event.get("current_candidate_error_mm") is not None:
                false_errors.append(event["current_candidate_error_mm"])
    rng = np.random.default_rng(0)
    fig, axis = plt.subplots(figsize=(9, 3.8))
    axis.axvspan(-10, 30, color="#188038", alpha=0.14, label="D−10 to D+30")
    axis.scatter(true_errors, 1 + rng.normal(0, 0.035, len(true_errors)),
                 color="#185abc", s=28, label="genuine next opening")
    axis.scatter(false_errors, rng.normal(0, 0.035, len(false_errors)),
                 color="#d93025", s=28, label="earlier secondary opening")
    axis.set_yticks([0, 1], ["secondary", "genuine"])
    axis.set_xlabel("Candidate displacement − current mapped D (mm)")
    axis.set_title("Intervals containing both a secondary and the genuine next opening")
    axis.grid(axis="x", alpha=0.2)
    axis.legend(loc="lower right", fontsize=8)
    fig.tight_layout()
    fig.savefig(output_dir / "candidate_window_comparison.png", dpi=180,
                bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--logs", nargs="+", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--plots-dir", type=Path)
    args = parser.parse_args()
    hall, ir, sequence_gaps, ring_ranges = load_capture(args.capture)
    events = load_events(args.logs)
    match_events(events, hall, ir)
    summary, mapped, primaries, secondaries, intervals = summarize(
        events, hall, ir, sequence_gaps, ring_ranges
    )
    result = {
        "summary": summary,
        "events": events,
        "intervals": intervals,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, default=json_safe) + "\n")
    if args.plots_dir:
        make_plots(args.plots_dir, hall, ir, events, intervals, summary)
    print(json.dumps(summary, indent=2, default=json_safe))
