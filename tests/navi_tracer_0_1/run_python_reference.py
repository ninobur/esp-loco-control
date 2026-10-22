#!/usr/bin/env python3
"""Run the validated Python recognizer (commit 6efc952,
research/navi-target-acquisition-pwm90-20261021,
research/navi_target_acquisition_20261021/target_acquisition.py) over a
fixture CSV, and write its event stream as CSV in the same format
equivalence_runner.cpp (the C++ side) produces, for a direct diff.

The reference module and its route_map.py dependency are fetched from the
named commit via `git show` at run time rather than vendored into this
branch, so there is no possibility of the reference drifting from what was
actually independently validated.

Usage:
  run_python_reference.py FIXTURE.csv LAST_CONFIRMED_MM DIRECTION DURATION_S OUT.csv
"""
from __future__ import annotations

import csv
import importlib.util
import math
import subprocess
import sys
import tempfile
import os

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RESEARCH_COMMIT = "6efc952"
RESEARCH_PATH = "research/navi_target_acquisition_20261021"


def git_show(path: str) -> str:
    out = subprocess.run(
        ["git", "show", f"{RESEARCH_COMMIT}:{path}"],
        cwd=REPO_ROOT, capture_output=True, text=True, check=True,
    )
    return out.stdout


def load_reference_module():
    tmpdir = tempfile.mkdtemp(prefix="navi_tracer_pyref_")
    route_map_src = git_show(f"{RESEARCH_PATH}/route_map.py")
    target_acq_src = git_show(f"{RESEARCH_PATH}/target_acquisition.py")
    with open(os.path.join(tmpdir, "route_map.py"), "w") as f:
        f.write(route_map_src)
    with open(os.path.join(tmpdir, "target_acquisition.py"), "w") as f:
        f.write(target_acq_src)
    sys.path.insert(0, tmpdir)
    spec = importlib.util.spec_from_file_location(
        "target_acquisition", os.path.join(tmpdir, "target_acquisition.py"))
    mod = importlib.util.module_from_spec(spec)
    sys.modules["target_acquisition"] = mod   # dataclass needs this registered before exec
    spec.loader.exec_module(mod)
    return mod


def main():
    fixture_path, last_mm, direction, duration_s, out_path = sys.argv[1:6]
    last_mm = int(last_mm); direction = int(direction); duration_s = float(duration_s)

    ta_mod = load_reference_module()
    ta = ta_mod.TargetAcquisition(
        last_confirmed_mm=last_mm, direction=direction, min_acquire_duration_s=duration_s)

    with open(fixture_path) as f:
        reader = csv.DictReader(f)
        rows = [(float(r["t"]), float(r["hall"]), int(r["pwm"])) for r in reader]

    events = []
    for t, hall, pwm in rows:
        hall_or_none = None if math.isnan(hall) else hall
        before = len(ta.events)
        ta.observe(t, hall_or_none, pwm)
        events.extend(ta.events[before:])

    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["kind", "t", "target_mm", "sign", "matches_target",
                    "start_s", "acquired_s", "duration_s", "acquired", "peak_departure"])
        for e in events:
            d = e.detail
            w.writerow([
                e.kind, f"{e.t:.6f}", e.target_mm,
                d.get("sign", d.get("resolved_sign", "")),
                int(d["matches_target"]) if "matches_target" in d else "",
                f"{d['start_s']:.6f}" if "start_s" in d else "",
                f"{d['acquired_s']:.6f}" if "acquired_s" in d else "",
                f"{d['duration_s']:.6f}" if "duration_s" in d else "",
                int(d["acquired"]) if "acquired" in d else "",
                f"{d['peak_departure']:.6f}" if "peak_departure" in d else "",
            ])
    print(f"python reference: {len(events)} events -> {out_path}")


if __name__ == "__main__":
    main()
