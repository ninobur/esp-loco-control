#!/usr/bin/env python3
"""Diff two event-stream CSVs (Python reference vs. C++ port) produced by
run_python_reference.py / equivalence_runner.cpp. Exits 0 and prints PASS
only if every event matches in order: same kind, same target_mm, and every
numeric field within TOL (floating-point precision only -- not a tolerance
for behavioral differences).

Usage: compare_events.py PYTHON.csv CPP.csv [--tol 1e-6]
"""
from __future__ import annotations

import csv
import sys

TOL = 1e-6
NUMERIC_FIELDS = ["t", "sign", "matches_target", "start_s", "acquired_s",
                  "duration_s", "acquired", "peak_departure"]


def load(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def close(a: str, b: str, tol: float) -> bool:
    if a == b:
        return True
    if a == "" or b == "":
        return a == b
    try:
        return abs(float(a) - float(b)) <= tol
    except ValueError:
        return a == b


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--tol")]
    tol = TOL
    for a in sys.argv[1:]:
        if a.startswith("--tol="):
            tol = float(a.split("=", 1)[1])
    py_path, cpp_path = args[0], args[1]
    py_rows, cpp_rows = load(py_path), load(cpp_path)

    ok = True
    if len(py_rows) != len(cpp_rows):
        print(f"FAIL: event count differs: python={len(py_rows)} cpp={len(cpp_rows)}")
        ok = False
    n = min(len(py_rows), len(cpp_rows))
    for i in range(n):
        p, c = py_rows[i], cpp_rows[i]
        if p["kind"] != c["kind"] or p["target_mm"] != c["target_mm"]:
            print(f"FAIL event {i}: kind/target_mm differ: python={p['kind']}/{p['target_mm']} "
                  f"cpp={c['kind']}/{c['target_mm']}")
            ok = False
            continue
        for field in NUMERIC_FIELDS:
            if not close(p.get(field, ""), c.get(field, ""), tol):
                print(f"FAIL event {i} ({p['kind']}): field {field} differs: "
                      f"python={p.get(field)!r} cpp={c.get(field)!r}")
                ok = False
    if ok:
        print(f"PASS: {n} events, identical kind sequence and target_mm, "
              f"all numeric fields within {tol}")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
