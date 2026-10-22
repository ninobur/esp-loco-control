#!/usr/bin/env python3
"""Deterministic synthetic Hall fixtures for the NAVI_TRACER_0_1 Python/C++
equivalence harness. Each fixture is a CSV of (t, hall, pwm) rows fed
identically to both the validated Python recognizer (commit 6efc952) and the
ported C++ NaviTracerCore, so the two event streams can be diffed on exactly
the same input.

IMPORTANT, stated precisely: none of this is the real otto_train recording.
The raw recording (raw/otto_train.bin) that
research/navi_target_acquisition_20261021/REPORT.md's results are built from
is not committed to this repository on any branch and was not available in
this environment (it lives only on two machines outside this repository,
per that research's own README.md). Every fixture here is a synthetic
reconstruction built from the shapes, amplitudes and durations REPORT.md and
the research's own synthetic test suite describe -- not a replay of recorded
Hall samples. This is stated explicitly in NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md
and must not be read as validating against the real recording.
"""
from __future__ import annotations

import csv
import math
import os
import sys

import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")
os.makedirs(OUT, exist_ok=True)

BASELINE_LEVEL = 1800.0
DT = 0.02


def quiet_run(n, level=BASELINE_LEVEL, noise=1.0, t0=0.0, pwm=90, seed=0):
    rng = np.random.default_rng(seed)
    return [(round(t0 + i * DT, 6), level + rng.normal(0, noise), pwm) for i in range(n)]


def curve_run(n, t0, amplitude, width_s=0.15, level=BASELINE_LEVEL, pwm=90, seed=1):
    rng = np.random.default_rng(seed)
    out = []
    mu = t0 + n * DT / 2
    for i in range(n):
        t = t0 + i * DT
        y = level + amplitude * math.exp(-0.5 * ((t - mu) / width_s) ** 2) + rng.normal(0, 1.0)
        out.append((round(t, 6), y, pwm))
    return out


def write(name, rows):
    path = os.path.join(OUT, name)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t", "hall", "pwm"])
        for t, y, pwm in rows:
            w.writerow([f"{t:.6f}", f"{y:.6f}", pwm])
    print(f"wrote {path} ({len(rows)} rows)")


def fixture_basic_correct_sequence():
    """One quiet baseline, one correct-polarity curve, quiet tail.
    Mirrors test_correct_polarity_curve_is_acquired_and_closes."""
    rows = quiet_run(40)
    rows += curve_run(40, 0.8, -140.0, 0.2, seed=2)
    rows += quiet_run(20, t0=0.8 + 40 * DT, seed=3)
    write("basic_correct_sequence.csv", rows)


def fixture_wrong_polarity():
    """Opposite-polarity curve -> CONTRADICTION, no advance."""
    rows = quiet_run(40)
    rows += curve_run(40, 0.8, 140.0, 0.2, seed=4)
    rows += quiet_run(20, t0=0.8 + 40 * DT, seed=5)
    write("wrong_polarity.csv", rows)


def fixture_short_noise_excursion():
    """Correct sign, 0.04s active duration -- DISPOSED under 110ms, likely
    ACQUIRED under the duration floor. Both engines must agree, under each
    configuration, on which it is."""
    rows = quiet_run(40)
    rows += curve_run(4, 0.8, -140.0, 0.04, seed=6)
    rows += quiet_run(20, t0=0.8 + 4 * DT, seed=7)
    write("short_noise_excursion.csv", rows)


def fixture_extended_no_timeout():
    """8s open candidate -- no LOST/TIMEOUT exists; must still close normally."""
    rows = quiet_run(40)
    rows += curve_run(400, 0.8, -140.0, 2.0, seed=8)
    rows += quiet_run(20, t0=0.8 + 400 * DT, seed=9)
    write("extended_no_timeout.csv", rows)


def fixture_stop_restart_mid_candidate():
    """A candidate opens, PWM drops to 0 mid-curve (stall), then resumes --
    the candidate's evidence must survive the stall untouched.
    Mirrors test_stop_mid_candidate_preserves_evidence."""
    rows = quiet_run(40)
    rows += curve_run(20, 0.8, -140.0, 0.3, seed=10)
    # 100 PWM=0 bins with garbage ADC -- must not be observed at all.
    rows += [(round(0.8 + (20 + i) * DT, 6), 9999.0, 0) for i in range(100)]
    rows += curve_run(20, 0.8 + 20 * DT, -140.0, 0.3, seed=11)
    rows += quiet_run(20, t0=0.8 + 40 * DT, seed=12)
    write("stop_restart_mid_candidate.csv", rows)


def fixture_284s_anomaly_synthetic():
    """Synthetic reconstruction of the SHAPE described for the real Trial 0 /
    MM129 anomaly in REPORT.md (284.58s open candidate, ~80 ADC departed,
    fitted slope 13.26 ADC/s, slow near-flat creep) -- NOT a replay of the
    real recording (unavailable in this environment). Exercises the frozen-
    baseline regression: a long plateau must not falsely trigger RTB."""
    rows = quiet_run(40)
    rows += curve_run(20, 0.8, -140.0, 0.3, seed=13)  # rising departure
    plateau = []
    t0 = 0.8 + 20 * DT
    for i in range(14230):   # ~284.6s at 20ms/bin, matching the reported duration
        t = t0 + i * DT
        plateau.append((round(t, 6), BASELINE_LEVEL - 80.0 + (i * 0.0003), 90))
    rows += plateau
    rows += quiet_run(20, t0=t0 + 14230 * DT, seed=14)
    write("284s_anomaly_synthetic.csv", rows)


def fixture_multi_magnet_sequence_synthetic():
    """Several consecutive correct-polarity encounters in a row, polarity
    alternating per the REAL route map (RouteMap.h / route_map.py, identical
    on both sides), starting at MM100 direction CW -- a stand-in for
    sustained multi-magnet operation (what Trial 6 exercises in the real
    recording). NOT the real Trial 6 recording (unavailable in this
    environment): a synthetic sequence built to the SAME map, used only to
    check sequential-advancement and repeated-acquisition behavior end to
    end over many consecutive magnets."""
    # Route polarity for MM100..109 (copied from RouteMap.h / route_map.py,
    # both read directly off the committed table; see
    # docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md for the full derivation).
    route_polarity = [
        1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,
        0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
        0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,
        0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
        1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,
        1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
        1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,
        0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
        1,0,1,0,0,0,0,1,1,1,0,
    ]
    rows = quiet_run(40, seed=100)
    t = 0.8
    seed = 200
    for mm in range(100, 110):
        pol = route_polarity[mm % 171]
        amp = -140.0 if pol == 0 else 140.0
        curve = curve_run(30, t, amp, 0.15, seed=seed); seed += 1
        rows += curve
        t = curve[-1][0] + DT
        tail = quiet_run(40, t0=t, seed=seed); seed += 1
        rows += tail
        t = tail[-1][0] + DT
    write("multi_magnet_sequence_synthetic.csv", rows)
    return 100  # starting mm for the runner (direction CW/+1)


if __name__ == "__main__":
    fixture_basic_correct_sequence()
    fixture_wrong_polarity()
    fixture_short_noise_excursion()
    fixture_extended_no_timeout()
    fixture_stop_restart_mid_candidate()
    fixture_284s_anomaly_synthetic()
    fixture_multi_magnet_sequence_synthetic()
    print("done")
