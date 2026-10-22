#!/usr/bin/env python3
"""Causal replay of the non-IR target-acquisition recognizer against the
real Otto PWM-90 Hall stream.

Data: raw/otto_train.bin, boot 15549e9385faa4d5 (otto_train stream), as
catalogued in NAVI_CPR_0_1/sources/data_manifest.json. The file is not
committed to this repository (it never has been, on any branch -- see the
engineering report); point --data-root at a checkout that has it. Two
such checkouts were found and verified identical by SHA-256 while
preparing this experiment: see REPORT.md.

Methodology:
  - The recording is split into maximal contiguous stretches where every
    sample is PWM 90 or PWM 0 (ordinary running or an ordinary stop at
    constant commanded power -- this experiment's declared Stage-1
    scope). Transitions through any other PWM value end a stretch; they
    are not modelled here.
  - Each stretch long enough to contain multiple encounters is run as an
    INDEPENDENT trial, each with its own fresh recognizer instance.
  - Each trial's starting trusted position is NOT discovered by this
    experiment -- there is no independent surveyed ground truth in this
    recording. It is seeded from the first historical OPENING annotation
    at or after the trial's start, exactly standing in for what an
    operator's own declaration would supply at session start. This
    substitution, and its limits, are discussed in REPORT.md.
  - Historical annotations are used ONLY after the causal replay
    completes, for diagnostic comparison -- never fed into the
    recognizer's own decisions.
"""
from __future__ import annotations

import argparse
import csv
import json
import sys
from dataclasses import asdict
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import route_map
from annotations import load_openings, first_opening_at_or_after, BOOT_ID, STREAM
from target_acquisition import TargetAcquisition

DTYPE = np.dtype([('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'), ('pwm', 'u1'), ('direction', 'u1')])
MIN_TRIAL_SPAN_S = 15.0


def bins(native):
    """20-ms causal means. Timestamp is the bin END -- the first instant
    this mean is knowable. PWM for a bin is its median native PWM; a bin
    containing a genuine PWM transition is reported at that median, which
    the run-splitting below then excludes if it is not 90 or 0."""
    us = native['us']
    index = us // 20_000
    change = np.r_[True, index[1:] != index[:-1]]
    starts = np.flatnonzero(change)
    ends = np.r_[starts[1:], len(native)]
    out = []
    for lo, hi in zip(starts, ends):
        a = native[lo:hi]
        out.append(((int(index[lo]) + 1) * .02, float(a['adc'].mean()), int(np.median(a['pwm']))))
    return out


def find_trials(trace):
    """Maximal contiguous runs where every bin's pwm is 90 or 0."""
    ok = [1 if (pwm == 90 or pwm == 0) else 0 for _, _, pwm in trace]
    runs = []
    i = 0
    n = len(ok)
    while i < n:
        if ok[i]:
            j = i
            while j < n and ok[j]:
                j += 1
            runs.append((i, j))
            i = j
        else:
            i += 1
    runs = [(lo, hi) for lo, hi in runs if trace[hi - 1][0] - trace[lo][0] >= MIN_TRIAL_SPAN_S]
    runs.sort(key=lambda r: trace[r[1] - 1][0] - trace[r[0]][0], reverse=True)
    return runs


def seed_established_index(trace, lo, hi, seed_opening):
    """The bin index at or after which the seed annotation's OWN encounter
    has ended -- geographic progression must not begin before this. A
    trial's playback otherwise starts at `lo` (the trial's own start),
    which can precede the seed encounter's actual end; treating the seed
    magnet as already "confirmed" from `lo` is an unsupported assumption
    (the independent review's finding #2, confirmed by inspection --
    see REPORT.md). Returns None if the seed's end never falls inside
    [lo, hi) (the seed encounter itself is not fully contained in this
    trial's eligible stretch)."""
    times = [t for t, _, _ in trace[lo:hi]]
    for offset, t in enumerate(times):
        if t >= seed_opening.end_s:
            return lo + offset
    return None


def run_trial(trial_id, trace, start_idx, hi, seed_opening, min_acquire_duration_s):
    """`start_idx` is the bin index geographic progression begins at --
    AFTER the seed encounter's own end (see seed_established_index), never
    the trial's raw start. The recognizer's first BASELINE phase begins
    here, exactly as an operator declaration at this instant would."""
    ta = TargetAcquisition(last_confirmed_mm=seed_opening.mm, direction=seed_opening.direction,
                            min_acquire_duration_s=min_acquire_duration_s)
    rows = []
    for t, hall, pwm in trace[start_idx:hi]:
        ta.observe(t, hall, pwm)
    # Collect a flat, chronological event trace with trial/boot/stream identity.
    for e in ta.events:
        rows.append(dict(
            trial=trial_id, boot=BOOT_ID, stream=STREAM,
            t_s=round(e.t, 3), kind=e.kind, target_mm=e.target_mm,
            **{k: (round(v, 4) if isinstance(v, float) else v) for k, v in e.detail.items()},
        ))
    return ta, rows


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--data-root", type=Path, required=True,
                    help="Directory containing raw/otto_train.bin and data/train_annotations.csv")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--min-acquire-duration", type=float, default=0.11,
                    help="Seconds of sustained active departure required before a "
                         "matching candidate is provisionally ACQUIRED (default: the "
                         "assignment's own 110 ms PWM-90 starting point).")
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    raw_path = args.data_root / "raw" / "otto_train.bin"
    ann_path = args.data_root / "data" / "train_annotations.csv"
    native = np.memmap(raw_path, dtype=DTYPE, mode="r")
    trace = bins(native)
    openings = load_openings(ann_path)

    trials = find_trials(trace)
    all_rows = []
    trial_summaries = []
    for i, (lo, hi) in enumerate(trials):
        t_start = trace[lo][0]
        t_end = trace[hi - 1][0]
        seed = first_opening_at_or_after(openings, t_start)
        if seed is None or seed.start_s > t_end:
            trial_summaries.append(dict(
                trial=i, t_start_s=round(t_start, 2), t_end_s=round(t_end, 2),
                span_s=round(t_end - t_start, 2), status="SKIPPED_NO_SEED_ANNOTATION",
            ))
            continue
        start_idx = seed_established_index(trace, lo, hi, seed)
        if start_idx is None:
            # The seed encounter's own end falls outside this trial's
            # eligible stretch (e.g. a PWM transition cut the stretch
            # short mid-encounter) -- do not infer it ended anyway.
            trial_summaries.append(dict(
                trial=i, t_start_s=round(t_start, 2), t_end_s=round(t_end, 2),
                span_s=round(t_end - t_start, 2), status="SKIPPED_SEED_ENCOUNTER_NOT_CLOSED_IN_TRIAL",
                seed_mm=seed.mm, seed_enc_id=seed.enc_id,
            ))
            continue
        t_progress_start = trace[start_idx][0]
        if t_end - t_progress_start < MIN_TRIAL_SPAN_S:
            trial_summaries.append(dict(
                trial=i, t_start_s=round(t_start, 2), t_end_s=round(t_end, 2),
                span_s=round(t_end - t_start, 2), status="SKIPPED_TOO_SHORT_AFTER_SEED",
                seed_mm=seed.mm, seed_enc_id=seed.enc_id,
                t_progress_start_s=round(t_progress_start, 2),
                post_seed_span_s=round(t_end - t_progress_start, 2),
            ))
            continue
        ta, rows = run_trial(i, trace, start_idx, hi, seed, args.min_acquire_duration)
        all_rows += rows
        n_candidates = sum(1 for e in ta.events if e.kind == "OPEN_CANDIDATE")
        n_closed = sum(1 for e in ta.events if e.kind == "CLOSE_AND_ADVANCE")
        n_contra = sum(1 for e in ta.events if e.kind == "CONTRADICTION")
        n_disposed = sum(1 for e in ta.events if e.kind == "DISPOSED")
        trial_summaries.append(dict(
            trial=i, t_start_s=round(t_start, 2), t_end_s=round(t_end, 2),
            span_s=round(t_end - t_start, 2), status="RUN",
            seed_mm=seed.mm, seed_enc_id=seed.enc_id, seed_direction=seed.direction,
            t_progress_start_s=round(t_progress_start, 2),
            post_seed_span_s=round(t_end - t_progress_start, 2),
            final_last_confirmed_mm=ta.last_confirmed_mm, final_target_mm=ta.target_mm,
            n_open_candidates=n_candidates, n_closed_advances=n_closed,
            n_contradictions=n_contra, n_disposed_short=n_disposed,
            open_at_end=ta.candidate is not None,
        ))

    fieldnames = sorted({k for r in all_rows for k in r.keys()})
    with open(args.out / "events.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for r in all_rows:
            w.writerow(r)
    (args.out / "trial_summary.json").write_text(json.dumps(trial_summaries, indent=2) + "\n")

    skip_reasons = {}
    for t in trial_summaries:
        if t["status"] != "RUN":
            skip_reasons[t["status"]] = skip_reasons.get(t["status"], 0) + 1
    overall = dict(
        scope="Otto, boot 15549e9385faa4d5 (otto_train), PWM-90/PWM-0-only contiguous stretches, non-IR.",
        min_acquire_duration_s=args.min_acquire_duration,
        n_trials=sum(1 for t in trial_summaries if t["status"] == "RUN"),
        n_trials_skipped=sum(1 for t in trial_summaries if t["status"] != "RUN"),
        skip_reasons=skip_reasons,
        total_close_and_advance=sum(t.get("n_closed_advances", 0) for t in trial_summaries),
        total_contradictions=sum(t.get("n_contradictions", 0) for t in trial_summaries),
        total_disposed_short=sum(t.get("n_disposed_short", 0) for t in trial_summaries),
        limitation=("Starting position per trial is seeded from a historical annotation standing in "
                    "for an operator declaration, not independently verified. Geographic progression "
                    "begins only after that seed encounter's own end (t_progress_start_s), not at the "
                    "trial's raw start -- everything from there on is this recognizer's own causal "
                    "decision."),
    )
    (args.out / "summary.json").write_text(json.dumps(overall, indent=2) + "\n")
    print(json.dumps(overall, indent=2))


if __name__ == "__main__":
    main()
