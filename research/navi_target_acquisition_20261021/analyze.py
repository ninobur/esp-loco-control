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


def diagnostic_annotation_agreement(events, openings, window_s=0.5):
    """For each CLOSE_AND_ADVANCE, find the nearest OPENING annotation
    within `window_s` of its closure time and report mm agreement.
    DIAGNOSTIC ONLY -- these annotations are historical ADC-derived
    comparison hypotheses (a15036f), not independent physical ground
    truth, and are not used anywhere in the recognizer itself."""
    closes = [e for e in events if e["kind"] == "CLOSE_AND_ADVANCE"]
    agree, disagree, no_annotation = 0, 0, 0
    disagreements = []
    for e in closes:
        t = float(e["t_s"])
        nearest = min(openings, key=lambda o: abs(o.start_s - t), default=None)
        if nearest is None or abs(nearest.start_s - t) > window_s:
            no_annotation += 1
            continue
        if nearest.mm == int(e["target_mm"]):
            agree += 1
        else:
            disagree += 1
            disagreements.append(dict(t_s=t, recognizer_mm=int(e["target_mm"]),
                                       annotation_mm=nearest.mm, dt=round(nearest.start_s - t, 3)))
    return dict(total_closes=len(closes), agree=agree, disagree=disagree,
                no_annotation_within_window=no_annotation, disagreements=disagreements)


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


def plot_example(data_root: Path, out_path: Path, t_lo: float, t_hi: float, events, title):
    native = np.memmap(data_root / "raw" / "otto_train.bin", dtype=DTYPE, mode="r")
    trace = bins(native)
    xs = [t for t, _, _ in trace if t_lo <= t <= t_hi]
    ys = [h for t, h, _ in trace if t_lo <= t <= t_hi]
    fig, ax = plt.subplots(figsize=(12, 4))
    ax.plot(xs, ys, color="#3366cc", linewidth=0.8, label="Hall (20ms mean)")
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
            ax.text(t, max(ys), f"MM{e['target_mm']}", rotation=90, fontsize=7,
                    va="top", ha="right", color="#109618")
        elif e["kind"] == "CONTRADICTION":
            ax.axvline(t, color="#dc3912", linestyle="--", linewidth=1.2)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("Hall ADC (20ms mean)")
    ax.set_title(title)
    handles = [
        plt.Line2D([0], [0], color="#3366cc", label="Hall trace"),
        plt.Line2D([0], [0], color="#999999", linestyle=":", label="OPEN_CANDIDATE"),
        plt.Line2D([0], [0], color="#ff9900", linestyle="--", label="ACQUIRED"),
        plt.Line2D([0], [0], color="#109618", label="CLOSE_AND_ADVANCE"),
        plt.Line2D([0], [0], color="#dc3912", linestyle="--", label="CONTRADICTION"),
    ]
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

    print(args.label, json.dumps(diag, indent=2))
    print(json.dumps(timing_summary, indent=2))


if __name__ == "__main__":
    main()
