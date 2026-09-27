#!/usr/bin/env python3
"""Evaluate simple IR-spatial Hall reference candidates.

Research tooling only.  This script decodes existing capture formats, joins
only the genuinely synchronized September 20 Toby records, and reports
candidate statistics.  It does not label Return to Baseline, choose a Hall
threshold, or implement navigation behavior.
"""

from __future__ import annotations

import argparse
import bisect
import csv
import importlib.util
import json
import math
import statistics
import sys
from collections import Counter
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import xhr_format  # noqa: E402
from ir_scope_espnow_analyze import parse_file as parse_ir_log  # noqa: E402

QT_PATH = ROOT / "field-records/analysis/rtb_evidence_20260926/scripts/extract_passages.py"
QT_SPEC = importlib.util.spec_from_file_location("rtb_qt_extract", QT_PATH)
QT = importlib.util.module_from_spec(QT_SPEC)
assert QT_SPEC.loader is not None
QT_SPEC.loader.exec_module(QT)

IR_MM_PER_PULSE = 9.652
IR_MATCH_TOLERANCE_S = 0.75
SPATIAL_BIN_MM = (25, 50, 100)


def med(values):
    return statistics.median(values) if values else None


def pct(values, p):
    if not values:
        return None
    values = sorted(values)
    return values[min(len(values) - 1, int((len(values) - 1) * p))]


def iso_epoch(value):
    return datetime.fromisoformat(value).timestamp()


def quantiles(values):
    values = [v for v in values if v is not None]
    return {"n": len(values), "min": min(values) if values else None,
            "p05": pct(values, 0.05), "p50": med(values),
            "p95": pct(values, 0.95), "max": max(values) if values else None}


def summarize_xhr(path, session_id):
    """Summarize the complete XHR session without inventing spatial units."""
    rulings = []
    sample_count = 0
    first_t_ms = None
    last_end_ms = None
    bad_records = 0
    frames = 0

    for _recv, data in xhr_format.iter_capture(str(path)):
        frames += 1
        try:
            hdr, payload = xhr_format.parse_record(data)
        except xhr_format.BadRecord:
            bad_records += 1
            continue
        if hdr.session_id != session_id:
            continue
        if hdr.rec_type == xhr_format.REC_SAMPLES:
            if first_t_ms is None:
                first_t_ms = hdr.t0_ms
            sample_count += hdr.n_items
            end_ms = hdr.t0_ms + sum(s["dt_us"] for s in xhr_format.iter_samples(hdr, payload)) / 1000.0
            last_end_ms = end_ms
        elif hdr.rec_type == xhr_format.REC_RULING:
            ruling = xhr_format.parse_ruling(payload)
            ruling["session_sample_seq"] = ruling["sample_seq"]
            rulings.append(ruling)

    # Use wide, symmetric time windows only as a native-record diagnostic.
    # They are deliberately not presented as traveled distance.
    windows = []
    for index, ruling in enumerate(rulings):
        if ruling["ruling"] != "ADVANCED":
            continue
        anchor = ruling["sample_seq"]
        windows.append((anchor - 600, anchor - 400, index, "pre"))
        windows.append((anchor + 400, anchor + 600, index, "post"))
    windows.sort()
    collected = {(i, side): [] for _a, _b, i, side in windows}
    cursor = 0
    for _recv, data in xhr_format.iter_capture(str(path)):
        try:
            hdr, payload = xhr_format.parse_record(data)
        except xhr_format.BadRecord:
            continue
        if hdr.session_id != session_id or hdr.rec_type != xhr_format.REC_SAMPLES:
            continue
        for sample in xhr_format.iter_samples(hdr, payload):
            seq = sample["sample_seq"]
            while cursor < len(windows) and seq > windows[cursor][1]:
                cursor += 1
            look = cursor
            while look < len(windows) and windows[look][0] <= seq:
                start, end, i, side = windows[look]
                if seq <= end:
                    collected[(i, side)].append((sample["raw"], sample["pwm_actual"]))
                look += 1

    comparisons = []
    for i, ruling in enumerate(rulings):
        pre = collected.get((i, "pre"), [])
        post = collected.get((i, "post"), [])
        if not pre or not post:
            continue
        pre_med = med([x[0] for x in pre])
        post_med = med([x[0] for x in post])
        post_pwm = [x[0] for x in post if x[1] > 0]
        comparisons.append({
            "sample_seq": ruling["sample_seq"],
            "polarity": ruling["polarity"],
            "pwm_actual": ruling["pwm_actual"],
            "pre_median": pre_med,
            "post_median": post_med,
            "post_minus_pre": post_med - pre_med,
            "post_pwm_positive_median": med(post_pwm),
        })

    accepted = [r for r in rulings if r["ruling"] == "ADVANCED"]
    accepted_peaks = {p: [r["peak"] for r in accepted if r["polarity"] == p]
                      for p in ("N", "S")}
    return {
        "path": str(path),
        "session": f"{session_id:08X}",
        "frames_seen": frames,
        "bad_records_seen": bad_records,
        "samples": sample_count,
        "duration_s": ((last_end_ms - first_t_ms) / 1000.0
                       if first_t_ms is not None and last_end_ms is not None else None),
        "rulings": len(rulings),
        "advanced": len(accepted),
        "advanced_peak_counts": {k: quantiles(v) for k, v in accepted_peaks.items()},
        "native_time_window_comparisons": comparisons,
        "native_time_window_error_counts": {
            "n": len(comparisons),
            "abs_post_minus_pre": quantiles([abs(x["post_minus_pre"]) for x in comparisons]),
            "signed_post_minus_pre": quantiles([x["post_minus_pre"] for x in comparisons]),
        },
        "spatial_limit": "XHR contains no synchronized IR distance; sample/time windows are not spatial ground truth.",
    }


def summarize_qt(path, session_id):
    """Summarize the complete QT Hall session; no IR is embedded in QT."""
    samples = 0
    first_seq = None
    last_seq = None
    first_t = None
    last_t = None
    sample_dt = []
    decisions = []
    frames = 0
    truncated = False
    for _recv, fields, rec_type, n_items, payload in QT.iter_qt(path):
        frames += 1
        if fields[5] != session_id:
            continue
        if rec_type == QT.QT_SAMPLE_REC:
            if len(payload) < n_items * QT.QT_SAMPLE.size:
                truncated = True
                continue
            if first_seq is None:
                first_seq = fields[7]
                first_t = fields[8]
            last_seq = fields[7] + n_items - 1
            last_t = fields[8] + sum(QT.QT_SAMPLE.unpack_from(payload, i * QT.QT_SAMPLE.size)[0]
                                     for i in range(n_items))
            samples += n_items
            for i in range(n_items):
                sample_dt.append(QT.QT_SAMPLE.unpack_from(payload, i * QT.QT_SAMPLE.size)[0])
        elif rec_type == QT.QT_DECISION_REC and len(payload) >= QT.QT_DECISION.size:
            decisions.append(QT.unwrap_qt_decision(payload[:QT.QT_DECISION.size]))

    accepted = [d for d in decisions if d["kind"] == "ACCEPT_EVENT"]
    peaks = {p: [d["event_peak"] for d in accepted if d["observed_polarity"] == p]
             for p in ("N", "S")}
    return {
        "path": str(path),
        "session": f"{session_id:08X}",
        "frames_seen": frames,
        "samples": samples,
        "sample_seq_first": first_seq,
        "sample_seq_last": last_seq,
        "duration_s": ((last_t - first_t) / 1000.0 if first_t is not None and last_t is not None else None),
        "sample_dt_ms": quantiles(sample_dt),
        "decisions": len(decisions),
        "accepted_events": len(accepted),
        "accepted_peak_counts": {k: quantiles(v) for k, v in peaks.items()},
        "truncated_frame_seen": truncated,
        "spatial_limit": "QT contains no synchronized IR distance; waveform samples cannot be assigned physical travel here.",
    }


def read_hall_status(path, ir_rows):
    times = [row["pi_t"] for row in ir_rows]
    out = []
    unmatched = 0
    for line in path.open(errors="replace"):
        parts = line.rstrip("\n").split("\t", 2)
        if len(parts) != 3 or not parts[1].endswith("/alert"):
            continue
        try:
            payload = json.loads(parts[2])
            t = iso_epoch(parts[0])
        except (ValueError, json.JSONDecodeError):
            continue
        if payload.get("reason") != "STATUS" or "baseline" not in payload:
            continue
        i = bisect.bisect_left(times, t)
        choices = [(abs(times[j] - t), ir_rows[j])
                   for j in (i - 1, i, i + 1) if 0 <= j < len(ir_rows)]
        if not choices or min(choices, key=lambda x: x[0])[0] > IR_MATCH_TOLERANCE_S:
            unmatched += 1
            continue
        delta_s, ir = min(choices, key=lambda x: x[0])
        out.append({
            "t": t,
            "baseline": payload["baseline"],
            "pwm": payload.get("pwm"),
            "moving": payload.get("moving"),
            "mm": payload.get("mm"),
            "ir_pulse": ir["pulses"],
            "ir_match_delta_s": delta_s,
        })
    return out, unmatched


def spatial_candidates(observations, bin_mm):
    if not observations:
        return None, 0
    origin = observations[0]["ir_pulse"]
    bins = {}
    for obs in observations:
        travel_mm = (obs["ir_pulse"] - origin) * IR_MM_PER_PULSE
        key = math.floor(travel_mm / bin_mm)
        bins.setdefault(key, []).append(obs["baseline"])
    reps = [med(values) for values in bins.values()]
    return med(reps), len(reps)


def summarize_paired(ir_path, hall_path, analysis_path):
    ir_rows = parse_ir_log(str(ir_path))
    ir_rows.sort(key=lambda row: row["pi_t"])
    statuses, unmatched = read_hall_status(hall_path, ir_rows)
    analysis = json.loads(analysis_path.read_text())
    intervals = []
    for interval in analysis["intervals"]:
        start = iso_epoch(interval["start"])
        end = start + interval["seconds"]
        obs = [x for x in statuses if start <= x["t"] <= end]
        time_median = med([x["baseline"] for x in obs])
        pwm_obs = [x["baseline"] for x in obs if (x["pwm"] or 0) > 0]
        item = {
            "start": interval["start"],
            "start_mm": interval["start_mm"],
            "end_mm": interval["end_mm"],
            "seconds": interval["seconds"],
            "ir_pulses": interval["pulses"],
            "ir_nominal_mm": interval["nominal_mm"],
            "map_mm": interval["map_mm"],
            "matched_status_count": len(obs),
            "time_median": time_median,
            "pwm_positive_median": med(pwm_obs),
            "moving_zero_observations": sum(x["moving"] == 0 for x in obs),
            "pwm_positive_without_pulse_advance": 0,
            "spatial": {},
        }
        for bin_mm in SPATIAL_BIN_MM:
            candidate, bins = spatial_candidates(obs, bin_mm)
            item["spatial"][str(bin_mm)] = {
                "median": candidate,
                "nonempty_bins": bins,
                "difference_from_time_median": (candidate - time_median
                    if candidate is not None and time_median is not None else None),
            }
        for left, right in zip(obs, obs[1:]):
            if (left["pwm"] or 0) > 0 and right["ir_pulse"] == left["ir_pulse"]:
                item["pwm_positive_without_pulse_advance"] += 1
        intervals.append(item)

    previous_spatial_50 = None
    for item in intervals:
        item["previous_spatial_50_median"] = previous_spatial_50
        current = item["spatial"]["50"]["median"]
        if current is not None:
            previous_spatial_50 = current

    multi = [x for x in intervals if x["matched_status_count"] >= 2]
    return {
        "ir_source": str(ir_path),
        "hall_source": str(hall_path),
        "analysis_source": str(analysis_path),
        "ir_mm_per_pulse": IR_MM_PER_PULSE,
        "ir_packets": len(ir_rows),
        "ir_first_epoch": ir_rows[0]["pi_t"] if ir_rows else None,
        "ir_last_epoch": ir_rows[-1]["pi_t"] if ir_rows else None,
        "ir_first_pulses": ir_rows[0]["pulses"] if ir_rows else None,
        "ir_last_pulses": ir_rows[-1]["pulses"] if ir_rows else None,
        "hall_status_matched": len(statuses),
        "hall_status_unmatched": unmatched,
        "match_delta_s": quantiles([x["ir_match_delta_s"] for x in statuses]),
        "interval_count": len(intervals),
        "intervals_with_two_or_more_status": len(multi),
        "intervals_with_moving_zero_status": sum(x["moving_zero_observations"] > 0 for x in intervals),
        "intervals_with_pwm_positive_no_pulse_advance": sum(
            x["pwm_positive_without_pulse_advance"] > 0 for x in intervals),
        "candidate_differences": {
            str(bin_mm): quantiles([
                abs(x["spatial"][str(bin_mm)]["difference_from_time_median"])
                for x in multi if x["spatial"][str(bin_mm)]["difference_from_time_median"] is not None
            ]) for bin_mm in SPATIAL_BIN_MM
        },
        "previous_reference_differences": quantiles([
            abs(x["spatial"]["50"]["median"] - x["previous_spatial_50_median"])
            for x in intervals
            if x["spatial"]["50"]["median"] is not None
            and x["previous_spatial_50_median"] is not None
        ]),
        "intervals": intervals,
        "interpretation_limits": [
            "The paired Hall stream is approximately 1 Hz status telemetry, not native Hall waveform samples.",
            "The complete Otto and Toby Hall captures have no synchronized IR distance, so they cannot independently score spatial accuracy.",
            "No interval in this paired record contains a verified stationary dwell with zero IR progress and enough matched Hall samples to test dwell de-weighting.",
        ],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xhr", type=Path, required=True)
    ap.add_argument("--xhr-session", default="C3B93D0B")
    ap.add_argument("--qt", type=Path, required=True)
    ap.add_argument("--qt-session", default="D7651658")
    ap.add_argument("--paired-ir", type=Path, required=True)
    ap.add_argument("--paired-hall", type=Path, required=True)
    ap.add_argument("--paired-analysis", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    result = {
        "tool": "tools/evaluate_spatial_hall_reference.py",
        "scope": "research/evaluation only; no RTB or navigation decision",
        "raw": {
            "otto_xhr": summarize_xhr(args.xhr, int(args.xhr_session, 16)),
            "toby_qt": summarize_qt(args.qt, int(args.qt_session, 16)),
        },
        "paired": summarize_paired(args.paired_ir, args.paired_hall, args.paired_analysis),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({
        "output": str(args.output),
        "otto_advanced": result["raw"]["otto_xhr"]["advanced"],
        "toby_accepted": result["raw"]["toby_qt"]["accepted_events"],
        "paired_intervals": result["paired"]["interval_count"],
        "paired_intervals_with_2_status": result["paired"]["intervals_with_two_or_more_status"],
    }, indent=2))


if __name__ == "__main__":
    main()
