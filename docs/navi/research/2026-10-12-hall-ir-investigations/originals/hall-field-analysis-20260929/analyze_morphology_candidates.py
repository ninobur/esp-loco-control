#!/usr/bin/env python3
"""Held-out Hall morphology and causal candidate-selection replay."""

from __future__ import annotations

import argparse
import csv
import importlib.util
import json
import math
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np


PITCH_MM = 9.652
QUIET_COUNTS = 25
OPEN_COUNTS = 70


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def stats(values):
    a = np.asarray([v for v in values if v is not None and np.isfinite(v)], float)
    if not len(a):
        return {"n": 0}
    return {
        "n": int(len(a)), "min": float(np.min(a)),
        "p05": float(np.quantile(a, .05)), "p50": float(np.median(a)),
        "p95": float(np.quantile(a, .95)), "max": float(np.max(a)),
        "mean": float(np.mean(a)),
        "sd": float(np.std(a, ddof=1)) if len(a) > 1 else None,
    }


def first_run(mask, length=5, start=0):
    mask = np.asarray(mask, bool)
    if len(mask) < length:
        return None
    sums = np.convolve(mask.astype(np.int16), np.ones(length, np.int16), "valid")
    hits = np.flatnonzero((sums == length) & (np.arange(len(sums)) >= start))
    return int(hits[0]) if len(hits) else None


def last_run(mask, length=5):
    mask = np.asarray(mask, bool)
    if len(mask) < length:
        return None
    sums = np.convolve(mask.astype(np.int16), np.ones(length, np.int16), "valid")
    hits = np.flatnonzero(sums == length)
    return int(hits[-1]) if len(hits) else None


def shape_metrics(y, time_ms):
    y = np.asarray(y, float)
    time_ms = np.asarray(time_ms, float)
    if len(y) < 2:
        return {"monotonic": None, "backtrack": None, "line_rmse": None}
    diff = np.diff(y)
    total = float(np.sum(np.abs(diff)))
    monotonic = float(np.mean(diff >= -3.0))
    backtrack = float(np.sum(np.maximum(-diff, 0)) / total) if total else 0.0
    if time_ms[-1] > time_ms[0] and np.ptp(y) > 0:
        x = (time_ms - time_ms[0]) / (time_ms[-1] - time_ms[0])
        fit = y[0] + (y[-1] - y[0]) * x
        line_rmse = float(np.sqrt(np.mean((y - fit) ** 2)) / np.ptp(y))
    else:
        line_rmse = 0.0
    return {"monotonic": monotonic, "backtrack": backtrack,
            "line_rmse": line_rmse}


def event_features(events, hall, positions):
    rows = []
    n = len(hall["median"])
    t = hall["t_us"]
    for event in events:
        if event.get("ruling") not in {"ADVANCED", "NON_LANDMARK_HALL"}:
            continue
        if event.get("dir") not in {"CW", "CCW"} or not event.get("two_sample_opening_matches"):
            continue
        i = int(event["sample_index"])
        ref = float(event["base_open"])
        opening = float(hall["median"][i]) - ref
        sign = 1.0 if opening >= 0 else -1.0
        lo = max(0, i - 750)
        hi = min(n, i + 1501)
        departure = hall["median"][lo:hi].astype(float) - ref
        signed = departure * sign
        rel_i = i - lo

        quiet_start = last_run(np.abs(departure[:max(0, rel_i - 1)]) <= QUIET_COUNTS, 5)
        rise_start = (quiet_start + 5) if quiet_start is not None else 0
        exit_rel = first_run(signed < OPEN_COUNTS, 5, start=min(rel_i + 1, len(signed)))
        peak_stop = (exit_rel + 1) if exit_rel is not None else min(len(signed), rel_i + 500)
        peak_start = max(rise_start, rel_i - 1)
        if peak_stop <= peak_start:
            continue
        peak_rel = peak_start + int(np.argmax(signed[peak_start:peak_stop]))
        return_rel = first_run(np.abs(departure) <= QUIET_COUNTS, 5,
                               start=min(peak_rel + 1, len(departure)))
        opposite_rel = first_run(signed <= -OPEN_COUNTS, 2,
                                 start=min(peak_rel + 1, len(signed)))

        time_ms = (t[lo:hi] - t[i]) / 1000.0
        x_mm = positions[lo:hi] - positions[i]
        pre = slice(rise_start, rel_i + 1)
        rise = slice(rise_start, peak_rel + 1)
        pre_shape = shape_metrics(signed[pre], time_ms[pre])
        rise_shape = shape_metrics(signed[rise], time_ms[rise])
        raw_span = np.ptp(hall["raw"][lo:hi].astype(float), axis=1)
        local_lo, local_hi = max(0, i - 50), min(n - 1, i + 50)
        local_dt = (t[local_hi] - t[local_lo]) / 1e6
        speed = ((positions[local_hi] - positions[local_lo]) / local_dt
                 if local_dt > 0 else 0.0)

        pre_ms = float(time_ms[rel_i] - time_ms[rise_start])
        pre_mm = float(x_mm[rel_i] - x_mm[rise_start])
        rise_ms = float(time_ms[peak_rel] - time_ms[rise_start])
        rise_mm = float(x_mm[peak_rel] - x_mm[rise_start])
        peak = float(signed[peak_rel])
        pre_gain = float(signed[rel_i] - signed[rise_start])
        rise_gain = float(peak - signed[rise_start])

        source_mm = int(event.get("mm", event.get("pre_mm", 0)))
        if event["ruling"] == "NON_LANDMARK_HALL":
            source_mm = int(event.get("pre_mm", source_mm))
        label = 1 if event["ruling"] == "ADVANCED" else 0
        row = {
            "event_serial": int(event["event_serial"]),
            "label": label,
            "label_name": "genuine" if label else "secondary",
            "direction": event["dir"],
            "mm": int(event.get("mm", 0)),
            "source_mm": source_mm,
            "fold": source_mm % 5,
            "polarity": event["obs"],
            "opened_ms": int(event["opened_ms"]),
            "opening_position_mm": float(positions[i]),
            "speed_mm_s": float(speed),
            "opening_abs_counts": abs(opening),
            "pre_rise_duration_ms": pre_ms,
            "pre_rise_distance_mm": pre_mm,
            "pre_slope_counts_ms": pre_gain / pre_ms if pre_ms > 0 else None,
            "pre_slope_counts_mm": pre_gain / pre_mm if pre_mm > .05 else None,
            "pre_monotonic_fraction": pre_shape["monotonic"],
            "pre_backtrack_ratio": pre_shape["backtrack"],
            "pre_line_rmse_norm": pre_shape["line_rmse"],
            "pre_raw_span_median": float(np.median(raw_span[pre])),
            "peak_abs_counts": peak,
            "rise_duration_ms": rise_ms,
            "rise_distance_mm": rise_mm,
            "rise_slope_counts_ms": rise_gain / rise_ms if rise_ms > 0 else None,
            "rise_slope_counts_mm": rise_gain / rise_mm if rise_mm > .05 else None,
            "rise_monotonic_fraction": rise_shape["monotonic"],
            "rise_backtrack_ratio": rise_shape["backtrack"],
            "rise_line_rmse_norm": rise_shape["line_rmse"],
            "opening_to_peak_ms": float(time_ms[peak_rel]),
            "opening_to_peak_mm": float(x_mm[peak_rel]),
            "threshold_exit_found": int(exit_rel is not None),
            "threshold_width_ms": (float(time_ms[exit_rel]) if exit_rel is not None else None),
            "threshold_width_mm": (float(x_mm[exit_rel]) if exit_rel is not None else None),
            "return_found": int(return_rel is not None),
            "peak_to_return_ms": (float(time_ms[return_rel] - time_ms[peak_rel])
                                  if return_rel is not None else None),
            "peak_to_return_mm": (float(x_mm[return_rel] - x_mm[peak_rel])
                                  if return_rel is not None else None),
            "opposite_found": int(opposite_rel is not None),
            "peak_to_opposite_ms": (float(time_ms[opposite_rel] - time_ms[peak_rel])
                                    if opposite_rel is not None else None),
            "peak_to_opposite_mm": (float(x_mm[opposite_rel] - x_mm[peak_rel])
                                    if opposite_rel is not None else None),
        }
        rows.append(row)
    return rows


FEATURE_SETS = {
    "speed_only": ["speed_mm_s"],
    "opening_rise": [
        "opening_abs_counts", "pre_rise_duration_ms", "pre_rise_distance_mm",
        "pre_slope_counts_ms", "pre_slope_counts_mm", "pre_monotonic_fraction",
        "pre_backtrack_ratio", "pre_line_rmse_norm", "pre_raw_span_median",
        "speed_mm_s",
    ],
    "opening_rise_no_speed": [
        "opening_abs_counts", "pre_rise_duration_ms", "pre_rise_distance_mm",
        "pre_slope_counts_ms", "pre_slope_counts_mm", "pre_monotonic_fraction",
        "pre_backtrack_ratio", "pre_line_rmse_norm", "pre_raw_span_median",
    ],
    "rise_peak": [
        "opening_abs_counts", "pre_slope_counts_mm", "pre_monotonic_fraction",
        "pre_backtrack_ratio", "pre_line_rmse_norm", "pre_raw_span_median",
        "peak_abs_counts", "rise_duration_ms", "rise_distance_mm",
        "rise_slope_counts_ms", "rise_slope_counts_mm", "rise_monotonic_fraction",
        "rise_backtrack_ratio", "rise_line_rmse_norm", "opening_to_peak_ms",
        "opening_to_peak_mm", "threshold_width_ms", "threshold_width_mm",
        "speed_mm_s",
    ],
    "full_morphology": [
        "opening_abs_counts", "pre_slope_counts_mm", "pre_monotonic_fraction",
        "pre_backtrack_ratio", "pre_line_rmse_norm", "pre_raw_span_median",
        "peak_abs_counts", "rise_duration_ms", "rise_distance_mm",
        "rise_slope_counts_ms", "rise_slope_counts_mm", "rise_monotonic_fraction",
        "rise_backtrack_ratio", "rise_line_rmse_norm", "opening_to_peak_ms",
        "opening_to_peak_mm", "threshold_width_ms", "threshold_width_mm",
        "return_found", "peak_to_return_ms", "peak_to_return_mm",
        "opposite_found", "peak_to_opposite_ms", "peak_to_opposite_mm",
        "speed_mm_s",
    ],
    "full_morphology_no_speed": [
        "opening_abs_counts", "pre_slope_counts_mm", "pre_monotonic_fraction",
        "pre_backtrack_ratio", "pre_line_rmse_norm", "pre_raw_span_median",
        "peak_abs_counts", "rise_duration_ms", "rise_distance_mm",
        "rise_slope_counts_ms", "rise_slope_counts_mm", "rise_monotonic_fraction",
        "rise_backtrack_ratio", "rise_line_rmse_norm", "opening_to_peak_ms",
        "opening_to_peak_mm", "threshold_width_ms", "threshold_width_mm",
        "return_found", "peak_to_return_ms", "peak_to_return_mm",
        "opposite_found", "peak_to_opposite_ms", "peak_to_opposite_mm",
    ],
}


def matrix(rows, names, train_rows=None):
    source = train_rows if train_rows is not None else rows
    train_raw = np.array([[r.get(name, np.nan) if r.get(name) is not None else np.nan
                           for name in names] for r in source], float)
    med = np.nanmedian(train_raw, axis=0)
    med = np.where(np.isfinite(med), med, 0.0)
    q25 = np.nanquantile(train_raw, .25, axis=0)
    q75 = np.nanquantile(train_raw, .75, axis=0)
    scale = q75 - q25
    scale = np.where(np.isfinite(scale) & (scale > 1e-9), scale, 1.0)
    raw = np.array([[r.get(name, np.nan) if r.get(name) is not None else np.nan
                     for name in names] for r in rows], float)
    missing = ~np.isfinite(raw)
    raw = np.where(missing, med, raw)
    x = np.clip((raw - med) / scale, -8, 8)
    # Missingness can itself describe censoring, so expose it without hiding it.
    x = np.column_stack([x, missing.astype(float)])
    return np.column_stack([np.ones(len(x)), x])


def sigmoid(z):
    z = np.clip(z, -35, 35)
    return 1.0 / (1.0 + np.exp(-z))


def fit_logistic(x, y, ridge=0.4):
    y = np.asarray(y, float)
    class_weight = np.where(y == 1, .5 / max(np.mean(y == 1), 1e-9),
                            .5 / max(np.mean(y == 0), 1e-9))
    w = np.zeros(x.shape[1])
    penalty = np.eye(x.shape[1]) * ridge
    penalty[0, 0] = 0
    for _ in range(60):
        p = sigmoid(x @ w)
        grad = x.T @ (class_weight * (p - y)) / len(y) + penalty @ w / len(y)
        hweight = class_weight * p * (1 - p)
        hess = (x.T * hweight) @ x / len(y) + penalty / len(y)
        try:
            step = np.linalg.solve(hess + np.eye(len(w)) * 1e-8, grad)
        except np.linalg.LinAlgError:
            step = np.linalg.pinv(hess) @ grad
        w -= step
        if np.max(np.abs(step)) < 1e-7:
            break
    return w


def auc(y, score):
    y = np.asarray(y, int); score = np.asarray(score, float)
    pos = score[y == 1]; neg = score[y == 0]
    if not len(pos) or not len(neg):
        return None
    return float(np.mean(pos[:, None] > neg[None, :])
                 + .5 * np.mean(pos[:, None] == neg[None, :]))


def choose_threshold(y, score):
    candidates = np.unique(np.r_[0, score, 1])
    best = (None, -1.0)
    for threshold in candidates:
        pred = score >= threshold
        tpr = np.mean(pred[y == 1]) if np.any(y == 1) else 0
        tnr = np.mean(~pred[y == 0]) if np.any(y == 0) else 0
        value = (tpr + tnr) / 2
        if value > best[1]:
            best = (float(threshold), float(value))
    return best[0]


def metrics(y, score, pred):
    y = np.asarray(y, int); pred = np.asarray(pred, bool)
    tp = int(np.sum(pred & (y == 1))); fn = int(np.sum(~pred & (y == 1)))
    tn = int(np.sum(~pred & (y == 0))); fp = int(np.sum(pred & (y == 0)))
    tpr = tp / (tp + fn) if tp + fn else None
    tnr = tn / (tn + fp) if tn + fp else None
    return {
        "genuine_n": tp + fn, "secondary_n": tn + fp,
        "genuine_accepted": tp, "genuine_rejected": fn,
        "secondary_rejected": tn, "secondary_accepted": fp,
        "genuine_recall": tpr, "secondary_recall": tnr,
        "balanced_accuracy": (tpr + tnr) / 2 if tpr is not None and tnr is not None else None,
        "auc": auc(y, score),
    }


def grouped_cross_validation(rows):
    y = np.array([r["label"] for r in rows], int)
    outputs = {}
    for set_name, names in FEATURE_SETS.items():
        scores = np.full(len(rows), np.nan)
        predictions = np.zeros(len(rows), bool)
        thresholds = {}
        for fold in range(5):
            train_idx = np.array([i for i, r in enumerate(rows) if r["fold"] != fold])
            test_idx = np.array([i for i, r in enumerate(rows) if r["fold"] == fold])
            train_rows = [rows[i] for i in train_idx]
            test_rows = [rows[i] for i in test_idx]
            x_train = matrix(train_rows, names, train_rows)
            x_test = matrix(test_rows, names, train_rows)
            w = fit_logistic(x_train, y[train_idx])
            train_score = sigmoid(x_train @ w)
            threshold = choose_threshold(y[train_idx], train_score)
            test_score = sigmoid(x_test @ w)
            scores[test_idx] = test_score
            predictions[test_idx] = test_score >= threshold
            thresholds[str(fold)] = threshold
        outputs[set_name] = {
            "scores": scores, "predictions": predictions,
            "thresholds": thresholds, "metrics": metrics(y, scores, predictions),
        }
    return outputs


def repeated_consistency(rows, predictions, feature_names):
    groups = defaultdict(list)
    for index, row in enumerate(rows):
        if row["label"] == 1:
            groups[(row["direction"], row["mm"])].append((index, row))
    repeated = [items for items in groups.values() if len(items) >= 2]
    result = {
        "repeated_genuine_magnet_groups": len(repeated),
        "prediction_all_accepted": 0, "prediction_all_rejected": 0,
        "prediction_mixed": 0, "features": {},
    }
    for items in repeated:
        values = [bool(predictions[index]) for index, _ in items]
        if all(values): result["prediction_all_accepted"] += 1
        elif not any(values): result["prediction_all_rejected"] += 1
        else: result["prediction_mixed"] += 1
    for name in feature_names:
        sds = []
        for items in repeated:
            values = [row.get(name) for _, row in items]
            values = [v for v in values if v is not None and np.isfinite(v)]
            if len(values) >= 2:
                sds.append(float(np.std(values, ddof=1)))
        result["features"][name] = stats(sds)
    return result


def separation_metrics(previous, current, hall, positions):
    a, b = int(previous["sample_index"]), int(current["sample_index"])
    if b <= a:
        return None
    reference = float(previous["base_open"])
    values = hall["median"][a:b + 1].astype(float) - reference
    quiet = np.abs(values) <= QUIET_COUNTS
    padded = np.r_[False, quiet, False]
    edges = np.flatnonzero(padded[1:] != padded[:-1]).reshape(-1, 2)
    longest_ms = longest_mm = 0.0
    for start, stop in edges:
        if stop - start < 5:
            continue
        longest_ms = max(longest_ms,
                         (hall["t_us"][a + stop - 1] - hall["t_us"][a + start]) / 1000.0)
        longest_mm = max(longest_mm,
                         positions[a + stop - 1] - positions[a + start])
    return {"longest_quiet_ms": float(longest_ms),
            "longest_quiet_mm": float(longest_mm),
            "opening_separation_mm": float(positions[b] - positions[a])}


def causal_select(expected, candidates):
    best = []
    best_error = None
    boundary = None
    observed = []
    for candidate in candidates:
        x = candidate["distance_mm"]
        if boundary is not None and x > boundary + 1e-9:
            return best, float(boundary), observed
        observed.append(candidate)
        error = abs(x - expected)
        if best_error is None or error < best_error - 1e-9:
            best_error = error; best = [candidate]
        elif abs(error - best_error) <= 1e-9:
            best.append(candidate)
        boundary = expected + best_error
        if x >= boundary - 1e-9:
            return best, float(x), observed
    return best, (float(boundary) if boundary is not None else None), observed


def candidate_replay(data, features, hall, positions, base):
    feature_by_serial = {r["event_serial"]: r for r in features}
    event_by_serial = {int(e["event_serial"]): e for e in data["events"]}
    events = sorted(data["events"], key=lambda e: e["opened_ms"])
    rows = []
    # Same high-confidence adjacent truth set as the odometry investigation.
    for interval in data["intervals"]:
        source = event_by_serial[interval["from_serial"]]
        target = event_by_serial[interval["to_serial"]]
        if not (interval["map_steps"] == 1 and source.get("ruling") == "ADVANCED"
                and target.get("ruling") == "ADVANCED"
                and target.get("distance_assessable") == 1
                and target.get("ir_interval") == "MM_REFERENCED_EPOCH"
                and target.get("pre_mm") == interval["from_mm"]):
            continue
        source_pos = float(positions[int(source["sample_index"])])
        expected = float(interval["map_mm"])
        horizon = expected * 2.0
        candidates = []
        for event in events:
            if not (source["opened_ms"] < event["opened_ms"]):
                continue
            distance = float(positions[int(event["sample_index"])] - source_pos)
            if distance > horizon:
                break
            if event.get("ruling") not in {"ADVANCED", "NON_LANDMARK_HALL"}:
                continue
            if not event.get("two_sample_opening_matches"):
                continue
            candidates.append({
                "serial": int(event["event_serial"]), "distance_mm": distance,
                "genuine": int(event["event_serial"]) == int(target["event_serial"]),
                "polarity": event.get("obs"),
                "event": event,
                "morphology_score": feature_by_serial.get(
                    int(event["event_serial"]), {}).get("full_morphology_score"),
            })
        candidates.sort(key=lambda x: (x["distance_mm"], x["serial"]))
        if not any(c["genuine"] for c in candidates):
            continue
        target_candidate = next(c for c in candidates if c["genuine"])
        expected_polarity = target.get("obs")
        historical_eligible = [c for c in candidates
                               if c["polarity"] == expected_polarity
                               and expected * .85 <= c["distance_mm"] <= expected * 1.15]
        historical = historical_eligible[0] if historical_eligible else None
        raw_winners, raw_final_distance, raw_observed = causal_select(expected, candidates)
        independent=[]
        for candidate in candidates:
            if not independent:
                independent.append(candidate); continue
            separation=separation_metrics(independent[-1]["event"],candidate["event"],
                                          hall,positions)
            if (separation and separation["longest_quiet_ms"] >= 4.0
                    and separation["opening_separation_mm"] >= PITCH_MM - .05):
                independent.append(candidate)
        spaced=[]
        for candidate in candidates:
            if (not spaced or candidate["distance_mm"]-spaced[-1]["distance_mm"]
                    >= 3*PITCH_MM-.05):
                spaced.append(candidate)
        winners, final_distance, observed = causal_select(expected, independent)
        spaced_winners, spaced_final_distance, spaced_observed = causal_select(expected, spaced)
        selected = winners[0] if len(winners) == 1 else None
        raw_selected = raw_winners[0] if len(raw_winners) == 1 else None
        spaced_selected = spaced_winners[0] if len(spaced_winners) == 1 else None
        rows.append({
            "direction": interval["direction"], "from_mm": interval["from_mm"],
            "to_mm": interval["to_mm"], "from_serial": interval["from_serial"],
            "to_serial": interval["to_serial"], "expected_mm": expected,
            "genuine_distance_mm": target_candidate["distance_mm"],
            "raw_candidate_count": len(candidates),
            "raw_false_candidate_count": sum(not c["genuine"] for c in candidates),
            "candidate_count": len(independent),
            "false_candidate_count": sum(not c["genuine"] for c in independent),
            "candidate_serials": ";".join(str(c["serial"]) for c in candidates),
            "candidate_distances_mm": ";".join(f'{c["distance_mm"]:.3f}' for c in candidates),
            "independent_candidate_serials": ";".join(str(c["serial"]) for c in independent),
            "independent_candidate_distances_mm": ";".join(
                f'{c["distance_mm"]:.3f}' for c in independent),
            "historical_selected_serial": historical["serial"] if historical else None,
            "historical_correct": bool(historical and historical["genuine"]),
            "historical_false": bool(historical and not historical["genuine"]),
            "historical_undecided": historical is None,
            "historical_decision_mm": historical["distance_mm"] if historical else None,
            "ewo_raw_selected_serial": raw_selected["serial"] if raw_selected else None,
            "ewo_raw_correct": bool(raw_selected and raw_selected["genuine"]),
            "ewo_raw_false": bool(raw_selected and not raw_selected["genuine"]),
            "ewo_raw_tie": len(raw_winners) > 1,
            "ewo_raw_final_distance_mm": raw_final_distance,
            "ewo_raw_observed_false_count": sum(not c["genuine"] for c in raw_observed),
            "ewo_selected_serial": selected["serial"] if selected else None,
            "ewo_correct": bool(selected and selected["genuine"]),
            "ewo_false": bool(selected and not selected["genuine"]),
            "ewo_tie": len(winners) > 1,
            "ewo_final_distance_mm": final_distance,
            "ewo_beyond_expected_mm": (max(0.0, final_distance - expected)
                                       if final_distance is not None else None),
            "ewo_beyond_genuine_mm": (final_distance - target_candidate["distance_mm"]
                                      if final_distance is not None else None),
            "observed_before_final": len(observed),
            "ewo_observed_false_count": sum(not c["genuine"] for c in observed),
            "selected_polarity_matches": bool(selected and selected["polarity"] == expected_polarity),
            "ewo_spacing_selected_serial": spaced_selected["serial"] if spaced_selected else None,
            "ewo_spacing_correct": bool(spaced_selected and spaced_selected["genuine"]),
            "ewo_spacing_false": bool(spaced_selected and not spaced_selected["genuine"]),
            "ewo_spacing_tie": len(spaced_winners)>1,
            "ewo_spacing_final_distance_mm": spaced_final_distance,
            "ewo_spacing_observed_false_count": sum(not c["genuine"] for c in spaced_observed),
        })
    return rows


def replay_summary(rows):
    mixed = [r for r in rows if r["ewo_observed_false_count"]]
    raw_mixed = [r for r in rows if r["ewo_raw_observed_false_count"]]
    spaced_mixed = [r for r in rows if r["ewo_spacing_observed_false_count"]]
    def outcome(prefix, items):
        return {
            "trials": len(items), "correct": sum(r[f"{prefix}_correct"] for r in items),
            "false": sum(r[f"{prefix}_false"] for r in items),
            "undecided_or_tie": sum(r.get(f"{prefix}_undecided", r.get(f"{prefix}_tie", False))
                                    for r in items),
        }
    def delay(prefix):
        finals=[r[f"{prefix}_final_distance_mm"] for r in rows]
        return {
            "beyond_expected_mm":stats([max(0.0,float(final)-float(r["expected_mm"]))
                                        for r,final in zip(rows,finals) if final is not None]),
            "beyond_genuine_mm":stats([float(final)-float(r["genuine_distance_mm"])
                                       for r,final in zip(rows,finals) if final is not None]),
        }
    return {
        "historical_all": outcome("historical", rows),
        "ewo_raw_openings_all": outcome("ewo_raw", rows),
        "ewo_all": outcome("ewo", rows),
        "ewo_three_pulse_separation_all": outcome("ewo_spacing",rows),
        "historical_mixed": outcome("historical", mixed),
        "ewo_raw_openings_mixed": outcome("ewo_raw", raw_mixed),
        "ewo_mixed": outcome("ewo", mixed),
        "ewo_three_pulse_separation_mixed":outcome("ewo_spacing",spaced_mixed),
        "raw_mixed_trials": len(raw_mixed),
        "mixed_trials": len(mixed),
        "three_pulse_separation_mixed_trials":len(spaced_mixed),
        "ewo_raw_delay":delay("ewo_raw"),
        "ewo_baseline_return_delay":delay("ewo"),
        "ewo_three_pulse_separation_delay":delay("ewo_spacing"),
        "ewo_beyond_expected_mm": stats([r["ewo_beyond_expected_mm"] for r in rows]),
        "ewo_beyond_genuine_mm": stats([r["ewo_beyond_genuine_mm"] for r in rows]),
    }


def synthetic_cases(morphology_metrics):
    rows = []
    for impostor in (270.0, 295.0, 299.0):
        ideal_winner = "genuine"  # genuine at exactly D has zero error
        qi = round(impostor / PITCH_MM) * PITCH_MM
        qg = round(300.0 / PITCH_MM) * PITCH_MM
        quantized = "genuine" if abs(qg - 300) < abs(qi - 300) else (
            "tie" if abs(abs(qg - 300) - abs(qi - 300)) < 1e-9 else "impostor")
        rows.append({
            "expected_mm": 300.0, "impostor_mm": impostor,
            "genuine_mm": 300.0, "historical_first_qualified": "impostor",
            "ideal_distance_selection": ideal_winner,
            "quantized_impostor_mm": qi, "quantized_genuine_mm": qg,
            "quantized_distance_selection": quantized,
            "separation_to_genuine_mm": 300.0 - impostor,
            "passes_provisional_15mm_independence": (300.0 - impostor) >= 15.0,
            "morphology_pairwise_tie_accuracy": morphology_metrics["auc"],
            "physical_status": "synthetic",
        })
    return rows


def morphology_rerank(replay, features, score_field, margin_mm=PITCH_MM):
    lookup={r["event_serial"]:r for r in features}
    correct=false=changed=eligible=0
    for row in replay:
        serials=[int(v) for v in row["candidate_serials"].split(";")]
        distances=[float(v) for v in row["candidate_distances_mm"].split(";")]
        final=float(row["ewo_raw_final_distance_mm"])
        candidates=[(serial,distance,abs(distance-float(row["expected_mm"])),
                     serial==int(row["to_serial"]))
                    for serial,distance in zip(serials,distances)
                    if distance<=final+.1]
        candidates.sort(key=lambda item:item[2])
        chosen=candidates[0]
        if len(candidates)>1 and candidates[1][2]-candidates[0][2]<=margin_mm:
            eligible+=1
            pool=[item for item in candidates if item[2]-candidates[0][2]<=margin_mm]
            replacement=max(pool,key=lambda item:float(lookup[item[0]][score_field]))
            changed+=replacement[0]!=chosen[0]
            chosen=replacement
        if chosen[3]:correct+=1
        else:false+=1
    return {"trials":len(replay),"near_distance_cases":eligible,
            "selection_changed":changed,"correct":correct,"false":false,
            "rule":f"rerank candidates within {margin_mm:.3f} mm of best distance"}


def spacing_sensitivity(replay):
    output=[]
    for pulses in range(0,11):
        correct=false=ties=0
        for row in replay:
            serials=[int(v) for v in row["candidate_serials"].split(";")]
            distances=[float(v) for v in row["candidate_distances_mm"].split(";")]
            candidates=[]
            for serial,distance in zip(serials,distances):
                item={"serial":serial,"distance_mm":distance,
                      "genuine":serial==int(row["to_serial"])}
                if (pulses==0 or not candidates or
                        distance-candidates[-1]["distance_mm"]>=pulses*PITCH_MM-.05):
                    candidates.append(item)
            winners,_final,_observed=causal_select(float(row["expected_mm"]),candidates)
            if len(winners)!=1:ties+=1
            elif winners[0]["genuine"]:correct+=1
            else:false+=1
        output.append({"minimum_separation_pulses":pulses,
                       "minimum_separation_mm":pulses*PITCH_MM,
                       "correct":correct,"false":false,"ties":ties})
    return output


def write_csv(path, rows):
    if not rows: return
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


def make_plots(output, features, cv, replay, synthetic, hall, positions, data):
    import matplotlib.pyplot as plt
    output.mkdir(parents=True, exist_ok=True)
    genuine = [r for r in features if r["label"] == 1]
    secondary = [r for r in features if r["label"] == 0]
    fields = [
        ("peak_abs_counts", "Peak departure (counts)"),
        ("pre_slope_counts_mm", "Pre-opening slope (counts/mm)"),
        ("opening_to_peak_mm", "Opening → peak (mm)"),
        ("rise_backtrack_ratio", "Rise backtrack ratio"),
        ("peak_to_return_mm", "Peak → quiet return (mm)"),
        ("peak_to_opposite_mm", "Peak → opposite threshold (mm)"),
    ]
    fig, axes = plt.subplots(2, 3, figsize=(12, 7))
    for ax, (field, label) in zip(axes.flat, fields):
        a = [r[field] for r in genuine if r[field] is not None and np.isfinite(r[field])]
        b = [r[field] for r in secondary if r[field] is not None and np.isfinite(r[field])]
        both = np.asarray(a + b)
        lo, hi = np.quantile(both, [.01, .99]) if len(both) else (0, 1)
        bins = np.linspace(lo, hi if hi > lo else lo + 1, 25)
        ax.hist(a, bins=bins, density=True, alpha=.55, label="genuine", color="#188038")
        ax.hist(b, bins=bins, density=True, alpha=.55, label="secondary", color="#d93025")
        ax.set_xlabel(label); ax.set_ylabel("Density"); ax.grid(alpha=.15)
    axes[0, 0].legend()
    fig.suptitle("Measured morphology overlaps; labels derive from geographic history")
    fig.tight_layout()
    fig.savefig(output / "morphology_feature_distributions.png", dpi=180,
                bbox_inches="tight"); plt.close(fig)

    fig, ax = plt.subplots(figsize=(6, 5))
    y = np.array([r["label"] for r in features])
    for name, color in (("speed_only", "#9aa0a6"), ("opening_rise", "#f9ab00"),
                        ("rise_peak", "#185abc"), ("full_morphology", "#7b1fa2")):
        score = cv[name]["scores"]
        thresholds = np.r_[np.inf, np.sort(np.unique(score))[::-1], -np.inf]
        tpr=[]; fpr=[]
        for threshold in thresholds:
            pred=score>=threshold
            tpr.append(np.mean(pred[y==1])); fpr.append(np.mean(pred[y==0]))
        ax.plot(fpr,tpr,label=f'{name} (AUC {cv[name]["metrics"]["auc"]:.3f})',color=color)
    ax.plot([0,1],[0,1],"--",color="#5f6368"); ax.set(xlabel="Secondary accepted",
        ylabel="Genuine accepted",xlim=(0,1),ylim=(0,1)); ax.grid(alpha=.15); ax.legend()
    ax.set_title("Five-fold held-out magnet-identity ROC")
    fig.tight_layout(); fig.savefig(output / "morphology_heldout_roc.png",dpi=180,
                                    bbox_inches="tight"); plt.close(fig)

    mixed = [r for r in replay if r["false_candidate_count"]]
    if mixed:
        example=max(mixed,key=lambda r:r["false_candidate_count"])
        xs=[float(x) for x in example["independent_candidate_distances_mm"].split(";")
            if float(x)<=float(example["ewo_final_distance_mm"])+.1]
        fig,ax=plt.subplots(figsize=(9,3))
        ax.scatter(xs,[1]*len(xs),s=65,color="#d93025")
        ax.scatter([example["genuine_distance_mm"]],[1],s=85,color="#188038",zorder=3,
                   label="genuine")
        ax.axvline(example["expected_mm"],ls="--",color="#202124",label="mapped expectation")
        ax.axvline(example["ewo_final_distance_mm"],ls=":",color="#185abc",label="EWO final boundary")
        ax.set_yticks([]);ax.set_xlabel("Capture-aligned displacement from last genuine opening (mm)")
        ax.set_title(f'MM{example["from_mm"]:03d}→MM{example["to_mm"]:03d} causal replay')
        ax.legend();ax.grid(axis="x",alpha=.15);fig.tight_layout()
        fig.savefig(output/"actual_candidate_replay.png",dpi=180,bbox_inches="tight");plt.close(fig)

    fig,ax=plt.subplots(figsize=(9,3.7))
    for j,row in enumerate(synthetic):
        y0=2-j
        imp=row["impostor_mm"]
        ax.scatter([imp],[y0],marker="x",s=90,color="#d93025")
        ax.scatter([300],[y0],s=70,color="#188038")
        ax.plot([imp,300],[y0,y0],color="#9aa0a6",lw=1)
        ax.text(imp,y0+.13,f'{imp:.0f}',ha="center",fontsize=8)
    ax.axvline(300,ls="--",color="#202124",label="expected and genuine 300 mm")
    ax.set_yticks([0,1,2]);ax.set_yticklabels(["299-mm impostor","295-mm impostor","270-mm impostor"])
    ax.set_xlim(255,307);ax.set_xlabel("Synthetic candidate position (mm)")
    ax.set_title("Synthetic expected-polarity impostors; geometry only")
    ax.grid(axis="x",alpha=.15);ax.legend();fig.tight_layout()
    fig.savefig(output/"synthetic_impostor_scenarios.png",dpi=180,bbox_inches="tight");plt.close(fig)

    event_lookup={int(e["event_serial"]):e for e in data["events"]}
    trace_specs=[
        (424,[425,426],245,345,"MM108→109: genuine opening then same-field reopening"),
        (253,[254,255,256,257],0,570,"MM125→127: overlapping observations without baseline return"),
    ]
    fig,axes=plt.subplots(2,1,figsize=(11,6.5))
    for ax,(origin_serial,serials,xlo,xhi,title) in zip(axes,trace_specs):
        origin=event_lookup[origin_serial]; origin_i=int(origin["sample_index"])
        origin_x=positions[origin_i]; ref=float(origin["base_open"])
        lo=int(np.searchsorted(positions,origin_x+xlo,side="left"))
        hi=int(np.searchsorted(positions,origin_x+xhi,side="right"))
        lo=max(0,lo);hi=min(len(positions),hi)
        x=positions[lo:hi]-origin_x
        y=hall["median"][lo:hi].astype(float)-ref
        ax.plot(x,y,color="#185abc",lw=1)
        ax.axhline(70,color="#b3261e",ls="--",lw=.8)
        ax.axhline(-70,color="#b3261e",ls="--",lw=.8)
        ax.axhspan(-25,25,color="#188038",alpha=.09)
        for serial in serials:
            e=event_lookup[serial]; ex=positions[int(e["sample_index"])]-origin_x
            color="#188038" if e["ruling"]=="ADVANCED" else "#d93025"
            ax.axvline(ex,color=color,lw=1.1)
            ax.text(ex+1,ax.get_ylim()[1]*.72,
                    f'{serial} {"genuine" if e["ruling"]=="ADVANCED" else "secondary"}',
                    rotation=90,va="top",fontsize=8,color=color)
        ax.set_xlim(xlo,xhi);ax.set_ylabel("Departure from source reference (counts)")
        ax.set_title(title);ax.grid(alpha=.15)
    axes[-1].set_xlabel("Capture-aligned displacement from source opening (mm)")
    fig.tight_layout();fig.savefig(output/"candidate_separation_traces.png",dpi=180,
                                   bbox_inches="tight");plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("analysis_json", type=Path)
    parser.add_argument("--base-script", type=Path, required=True)
    parser.add_argument("--odometry-script", type=Path, required=True)
    parser.add_argument("--logs", nargs="+", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    base = load_module(args.base_script, "continuous_hall")
    odo = load_module(args.odometry_script, "odometry_analysis")
    data = json.loads(args.analysis_json.read_text())
    hall, _old_ir, _gaps, _rings = base.load_capture(args.capture)
    ir = odo.load_ir(args.capture, base); odo.prepare_ir(ir)
    telemetry = odo.load_telemetry(args.logs)
    odo.add_event_positions(data, hall, ir, telemetry)
    positions = np.interp(hall["t_us"], ir["aligned_global_us"], ir["nominal_mm"])
    features = event_features(data["events"], hall, positions)
    cv = grouped_cross_validation(features)
    for name, result in cv.items():
        for row, score, pred in zip(features, result["scores"], result["predictions"]):
            row[f"{name}_score"] = float(score); row[f"{name}_prediction"] = int(pred)

    full = cv["full_morphology"]
    consistency = repeated_consistency(
        features, full["predictions"],
        ["peak_abs_counts", "pre_slope_counts_mm", "opening_to_peak_mm",
         "peak_to_return_mm", "rise_backtrack_ratio"],
    )

    # Evaluate observed quiet separation for consecutive labeled openings.
    by_time = sorted([e for e in data["events"] if int(e["event_serial"]) in
                      {r["event_serial"] for r in features}], key=lambda e:e["opened_ms"])
    separations=[]
    for a,b in zip(by_time,by_time[1:]):
        result=separation_metrics(a,b,hall,positions)
        if result:
            separations.append({"from_serial":int(a["event_serial"]),
                                "to_serial":int(b["event_serial"]),
                                "to_label":"genuine" if b["ruling"]=="ADVANCED" else "secondary",
                                **result})
    replay = candidate_replay(data, features, hall, positions, base)
    synthetic = synthetic_cases(full["metrics"])

    feature_summary={}
    for field in ["opening_abs_counts","peak_abs_counts","pre_slope_counts_mm",
                  "opening_to_peak_mm","rise_backtrack_ratio","peak_to_return_mm",
                  "peak_to_opposite_mm","speed_mm_s"]:
        feature_summary[field]={
            "genuine":stats([r[field] for r in features if r["label"]==1]),
            "secondary":stats([r[field] for r in features if r["label"]==0]),
        }
    y_all=np.array([r["label"] for r in features],int)
    speed_strata={}
    for stratum,mask in {
        "moving_at_least_80_mm_s":np.array([r["speed_mm_s"]>=80 for r in features]),
        "slow_or_stationary_below_80_mm_s":np.array([r["speed_mm_s"]<80 for r in features]),
    }.items():
        speed_strata[stratum]={}
        for name,result in cv.items():
            speed_strata[stratum][name]=metrics(
                y_all[mask],result["scores"][mask],result["predictions"][mask]
            )
    sep_genuine=[r for r in separations if r["to_label"]=="genuine"]
    sep_secondary=[r for r in separations if r["to_label"]=="secondary"]
    summary={
        "labels":{"genuine":sum(r["label"]==1 for r in features),
                  "secondary":sum(r["label"]==0 for r in features),
                  "basis":"ADVANCED versus NON_LANDMARK_HALL; provisional geographic labels"},
        "feature_distributions":feature_summary,
        "held_out_classification":{name:result["metrics"] for name,result in cv.items()},
        "classification_by_speed":speed_strata,
        "repeat_consistency":consistency,
        "candidate_separation":{
            "longest_quiet_before_genuine_mm":stats([r["longest_quiet_mm"] for r in sep_genuine]),
            "longest_quiet_before_secondary_mm":stats([r["longest_quiet_mm"] for r in sep_secondary]),
            "secondary_pairs_passing_15mm_quiet":sum(r["longest_quiet_mm"]>=15 for r in sep_secondary),
            "genuine_pairs_failing_15mm_quiet":sum(r["longest_quiet_mm"]<15 for r in sep_genuine),
            "criterion_status":"diagnostic only; uses prior event reference and unvalidated IR displacement",
        },
        "candidate_replay":replay_summary(replay),
        "candidate_spacing_sensitivity":spacing_sensitivity(replay),
        "morphology_incremental_replay":{
            "opening_rise":morphology_rerank(replay,features,"opening_rise_score"),
            "rise_peak":morphology_rerank(replay,features,"rise_peak_score"),
            "full_morphology":morphology_rerank(replay,features,"full_morphology_score"),
            "status":"post-hoc sensitivity, not independently validated",
        },
        "synthetic":synthetic,
    }
    args.output_dir.mkdir(parents=True,exist_ok=True)
    write_csv(args.output_dir/"morphology_features.csv",features)
    pred_rows=[{"event_serial":r["event_serial"],"label_name":r["label_name"],
                "source_mm":r["source_mm"],"fold":r["fold"],
                **{f"{name}_score":r[f"{name}_score"] for name in FEATURE_SETS},
                **{f"{name}_prediction":r[f"{name}_prediction"] for name in FEATURE_SETS}}
               for r in features]
    write_csv(args.output_dir/"held_out_morphology_predictions.csv",pred_rows)
    write_csv(args.output_dir/"candidate_replay.csv",replay)
    write_csv(args.output_dir/"candidate_separation.csv",separations)
    write_csv(args.output_dir/"synthetic_impostor_cases.csv",synthetic)
    (args.output_dir/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    make_plots(args.output_dir/"plots",features,cv,replay,synthetic,hall,positions,data)
    print(json.dumps(summary,indent=2))


if __name__ == "__main__":
    main()
