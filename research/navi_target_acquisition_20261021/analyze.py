#!/usr/bin/env python3
"""Offline-only analysis of a replay's events.csv: diagnostic comparison
against historical annotations (never fed into the recognizer), timing
statistics, and a representative plot. Run after replay.py.
"""
from __future__ import annotations

import argparse
import csv
import json
import statistics as st
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from annotations import load_openings
from replay import DTYPE, bins


def load_events(path: Path):
    return list(csv.DictReader(open(path)))


def _overlap(a, b):
    return max(0.0, min(a[1], b[1]) - max(a[0], b[0]))


def diagnostic_annotation_agreement(events, openings, overlap_threshold=0.30):
    """For each CLOSE_AND_ADVANCE, match it to the historical OPENING
    annotation whose own [start_s, end_s] interval overlaps the
    candidate's own [start_s, close_s] interval the most, as a FRACTION
    OF THE ANNOTATION'S OWN DURATION (the same method and threshold as
    98db802's own replay script) -- not nearest-single-timestamp, which
    conflates different stages of an encounter (independent review
    finding #3; confirmed as a real methodological weakness of the
    original analysis, corrected here).

    Reports GEOGRAPHIC agreement (does the matched annotation's mm equal
    the recognizer's target_mm) and EVENT-TIMING agreement (how closely
    the recognizer's own start/acquired/close timestamps align with the
    matched annotation's own start/end) SEPARATELY, per instruction.

    DIAGNOSTIC ONLY -- these annotations are historical ADC-derived
    comparison hypotheses (a15036f), not independent physical ground
    truth, and are not used anywhere in the recognizer's own decisions.
    """
    closes = [e for e in events if e["kind"] == "CLOSE_AND_ADVANCE"]
    geo_agree, geo_disagree, no_match = 0, 0, 0
    disagreements = []
    timing_offsets_start = []
    timing_offsets_end = []
    for e in closes:
        cand_start, cand_end = float(e["start_s"]), float(e["t_s"])
        cand_len = max(cand_end - cand_start, 1e-9)
        best = None
        best_frac = 0.0
        for o in openings:
            ann_len = max(o.end_s - o.start_s, 1e-9)
            frac = _overlap((cand_start, cand_end), (o.start_s, o.end_s)) / ann_len
            if frac > best_frac:
                best_frac, best = frac, o
        if best is None or best_frac < overlap_threshold:
            no_match += 1
            continue
        if best.mm == int(e["target_mm"]):
            geo_agree += 1
        else:
            geo_disagree += 1
            disagreements.append(dict(
                recognizer_close_s=cand_end, recognizer_mm=int(e["target_mm"]),
                annotation_mm=best.mm, overlap_frac=round(best_frac, 3),
                annotation_interval=[best.start_s, best.end_s],
                recognizer_interval=[cand_start, cand_end],
            ))
        timing_offsets_start.append(cand_start - best.start_s)
        timing_offsets_end.append(cand_end - best.end_s)
    result = dict(
        method="interval overlap (candidate [start_s, close_s] vs annotation [start_s, end_s]), "
               f">= {overlap_threshold:.0%} of the annotation's own duration -- not nearest-timestamp.",
        total_closes=len(closes),
        geographic_agree=geo_agree, geographic_disagree=geo_disagree,
        no_matching_annotation=no_match,
        disagreements=disagreements,
    )
    if timing_offsets_start:
        result["event_timing"] = dict(
            start_offset_s=dict(min=min(timing_offsets_start), median=st.median(timing_offsets_start),
                                 max=max(timing_offsets_start)),
            close_offset_from_annotation_end_s=dict(
                min=min(timing_offsets_end), median=st.median(timing_offsets_end), max=max(timing_offsets_end)),
        )
    return result


def timing_stats(events):
    by_target = {}
    for e in events:
        key = (e["trial"], e["target_mm"], e["t_s"])
    opens = {}
    stats = []
    for e in events:
        if e["kind"] == "OPEN_CANDIDATE":
            opens[(e["trial"], e["target_mm"])] = float(e["t_s"])
    # Walk chronologically per trial to pair each CLOSE/ACQUIRED with its
    # own candidate's OPEN_CANDIDATE (same trial, same target, latest
    # open at or before it).
    per_trial = {}
    for e in events:
        per_trial.setdefault(e["trial"], []).append(e)
    for trial, rows in per_trial.items():
        rows.sort(key=lambda r: float(r["t_s"]))
        last_open_t = None
        for r in rows:
            if r["kind"] == "OPEN_CANDIDATE":
                last_open_t = float(r["t_s"])
            elif r["kind"] == "CLOSE_AND_ADVANCE" and last_open_t is not None:
                acquired_t = float(r.get("acquired_s", r["t_s"]))
                stats.append(dict(
                    trial=trial, target_mm=r["target_mm"],
                    first_departure_s=last_open_t,
                    acquired_s=acquired_t,
                    closed_s=float(r["t_s"]),
                    time_to_acquire_s=round(acquired_t - last_open_t, 3),
                    time_to_close_s=round(float(r["t_s"]) - last_open_t, 3),
                ))
                last_open_t = None
    return stats


def plot_example(data_root: Path, out_path: Path, t_lo: float, t_hi: float, events, title, openings=None):
    native = np.memmap(data_root / "raw" / "otto_train.bin", dtype=DTYPE, mode="r")
    trace = bins(native)
    xs = [t for t, _, _ in trace if t_lo <= t <= t_hi]
    ys = [h for t, h, _ in trace if t_lo <= t <= t_hi]
    fig, ax = plt.subplots(figsize=(14, 5))
    ax.plot(xs, ys, color="#3366cc", linewidth=0.8, label="Hall (20ms mean)")
    ymax = max(ys) if ys else 0
    for e in events:
        t = float(e["t_s"])
        if not (t_lo <= t <= t_hi):
            continue
        if e["kind"] == "OPEN_CANDIDATE":
            ax.axvline(t, color="#999999", linestyle=":", linewidth=0.8)
        elif e["kind"] == "ACQUIRED":
            ax.axvline(t, color="#ff9900", linestyle="--", linewidth=1.2)
        elif e["kind"] == "CLOSE_AND_ADVANCE":
            ax.axvline(t, color="#109618", linewidth=1.4)
            ax.text(t, ymax, f"MM{e['target_mm']}", rotation=90, fontsize=7,
                    va="top", ha="right", color="#109618")
        elif e["kind"] == "CONTRADICTION":
            ax.axvline(t, color="#dc3912", linestyle="--", linewidth=1.2)
    if openings:
        ymin = min(ys) if ys else 0
        for o in openings:
            if t_lo <= o.start_s <= t_hi:
                ax.axvline(o.start_s, color="#9467bd", linestyle="-.", linewidth=0.8, alpha=0.6)
                ax.text(o.start_s, ymin, f"ann MM{o.mm}", rotation=90, fontsize=6,
                        va="bottom", ha="left", color="#9467bd")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("Hall ADC (20ms mean)")
    ax.set_title(title)
    handles = [
        plt.Line2D([0], [0], color="#3366cc", label="Hall trace"),
        plt.Line2D([0], [0], color="#999999", linestyle=":", label="OPEN_CANDIDATE"),
        plt.Line2D([0], [0], color="#ff9900", linestyle="--", label="ACQUIRED"),
        plt.Line2D([0], [0], color="#109618", label="CLOSE_AND_ADVANCE (recognizer mm)"),
        plt.Line2D([0], [0], color="#dc3912", linestyle="--", label="CONTRADICTION"),
    ]
    if openings:
        handles.append(plt.Line2D([0], [0], color="#9467bd", linestyle="-.", label="historical annotation mm"))
    ax.legend(handles=handles, loc="upper right", fontsize=8)
    fig.tight_layout()
    fig.savefig(out_path, dpi=130)
    plt.close(fig)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--data-root", type=Path, required=True)
    p.add_argument("--results", type=Path, required=True)
    p.add_argument("--label", type=str, required=True)
    args = p.parse_args()

    events = load_events(args.results / "events.csv")
    openings = load_openings(args.data_root / "data" / "train_annotations.csv")

    diag = diagnostic_annotation_agreement(events, openings)
    diag["caveat"] = ("Annotation agreement is a diagnostic comparison against historical "
                       "ADC-derived hypotheses, not a false-positive/false-negative rate "
                       "against independent physical ground truth (a15036f).")
    (args.results / "annotation_diagnostic.json").write_text(json.dumps(diag, indent=2) + "\n")

    stats = timing_stats(events)
    if stats:
        to_acquire = [s["time_to_acquire_s"] for s in stats]
        to_close = [s["time_to_close_s"] for s in stats]
        timing_summary = dict(
            n=len(stats),
            time_to_acquire_s=dict(min=min(to_acquire), median=st.median(to_acquire), max=max(to_acquire)),
            time_to_close_s=dict(min=min(to_close), median=st.median(to_close), max=max(to_close)),
        )
    else:
        timing_summary = dict(n=0)
    (args.results / "timing_stats.json").write_text(json.dumps(timing_summary, indent=2) + "\n")
    (args.results / "timing_per_event.csv").write_text(
        "trial,target_mm,first_departure_s,acquired_s,closed_s,time_to_acquire_s,time_to_close_s\n" +
        "\n".join(",".join(str(s[k]) for k in
                           ["trial", "target_mm", "first_departure_s", "acquired_s", "closed_s",
                            "time_to_acquire_s", "time_to_close_s"]) for s in stats) + "\n"
    )

    plot_example(args.data_root, args.results / "example_trial6_mm126_137.png",
                 1420.5, 1438.5, [e for e in events if e["trial"] == "6"],
                 f"Trial 6, MM126-137 ({args.label})")
    plot_example(args.data_root, args.results / "example_trial6_geographic_lag.png",
                 1419.5, 1442.5, [e for e in events if e["trial"] == "6"],
                 f"Trial 6, geographic lag region, recognizer vs historical annotation mm ({args.label})",
                 openings=openings)

    print(args.label, json.dumps(diag, indent=2))
    print(json.dumps(timing_summary, indent=2))


if __name__ == "__main__":
    main()
