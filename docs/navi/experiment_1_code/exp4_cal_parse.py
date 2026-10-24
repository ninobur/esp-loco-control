#!/usr/bin/env python3
"""Independent parse and reconciliation of the field-records/cal/*.txt
calibration corpus (16 files, June 29-30 2026). Not accepting the Codex
summary table -- re-deriving reference-section compatibility and the
MM060-079 directional claim directly from the raw records."""
import re, glob, json
import numpy as np

LINE_RE = re.compile(
    r"^MM(\d+)\s+obs=(\S+)\s+expected=(\S+)\s+pos=(\S+)\s+dir=(\S+)\s+dt=(\d+)\s+"
    r"dt_expected=(\d+)\s+timing_mode=(\S+)\s+effective_pwm_avg=(\d+)\s+seg_mm=(\d+)\s+"
    r"speed_mm_s=([\d.]+)\s+peakN=(\d+)\s+peakS=(\d+)\s+hallms=(\d+)"
)

def parse_file(path):
    loco = path.split("cal_")[1].split("_")[0]
    rows = []
    with open(path) as f:
        for line in f:
            m = LINE_RE.match(line.strip())
            if not m:
                continue
            mm, obs, exp, pos, dirf, dt, dt_exp, tmode, pwm, seg, speed, pN, pS, hallms = m.groups()
            rows.append({
                "loco": loco, "file": path, "mm": int(mm), "obs": obs, "expected": exp,
                "pos": pos, "dirf": dirf, "dt": int(dt), "dt_expected": int(dt_exp),
                "timing_mode": tmode, "pwm": int(pwm), "seg_mm": int(seg),
                "speed_mm_s": float(speed), "peakN": int(pN), "peakS": int(pS),
                "hallms": int(hallms),
            })
    return rows

def add_direction(rows):
    # direction inferred from consecutive MM progression within a file, per Codex's method
    for i in range(1, len(rows)):
        if rows[i]["file"] != rows[i-1]["file"]:
            rows[i]["direction"] = None
            continue
        prev_mm, cur_mm = rows[i-1]["mm"], rows[i]["mm"]
        d = cur_mm - prev_mm
        if d == 1 or d == -170:
            rows[i]["direction"] = "CW"
        elif d == -1 or d == 170:
            rows[i]["direction"] = "CCW"
        else:
            rows[i]["direction"] = None  # non-adjacent jump
    rows[0]["direction"] = None
    return rows

def main():
    all_rows = []
    for path in sorted(glob.glob("field-records/cal/*.txt")):
        all_rows.extend(parse_file(path))
    print(f"Total raw lines parsed: {len(all_rows)}")
    all_rows = add_direction(all_rows)

    # Strict filter, documented explicitly (same spirit as the precedent
    # HALL_SELECTIVITY methodology + Codex's stated exclusions):
    #   pos == EXACT_LOCKED           (not CONTESTED/DECLARED/STALE)
    #   obs == expected                (correct read)
    #   timing_mode == MOVING          (not SPEED_TRANSITION or STOPPED_INFERRED)
    #   seg_mm in [250, 360]           (adjacent single-interval passage, matches survey range 280-360)
    #   dt > 0                         (real elapsed time)
    #   direction is not None          (adjacent progression established)
    strict = [r for r in all_rows if r["pos"] == "EXACT_LOCKED" and r["obs"] == r["expected"]
              and r["timing_mode"] == "MOVING" and 250 <= r["seg_mm"] <= 360
              and r["dt"] > 0 and r["direction"] is not None]
    print(f"After strict filter (EXACT_LOCKED, obs==expected, MOVING, seg_mm 250-360, dt>0, "
          f"adjacent direction known): {len(strict)}")

    # Artifact filter from the precedent doc: speed within 0.6-1.6x the
    # median of its 4 neighbors in the same file (removes double-count
    # short-dt artifacts).
    by_file = {}
    for r in strict:
        by_file.setdefault(r["file"], []).append(r)
    kept = []
    removed_artifact = 0
    for f, rs in by_file.items():
        speeds = [r["speed_mm_s"] for r in rs]
        for i, r in enumerate(rs):
            lo, hi = max(0, i - 2), min(len(rs), i + 3)
            neighbors = speeds[lo:i] + speeds[i+1:hi]
            if not neighbors:
                kept.append(r); continue
            med = np.median(neighbors)
            if med > 0 and 0.6 * med <= r["speed_mm_s"] <= 1.6 * med:
                kept.append(r)
            else:
                removed_artifact += 1
    print(f"After neighbor-speed artifact filter: {len(kept)} (removed {removed_artifact})")

    json.dump(kept, open("exp4_cal_filtered.json", "w"))
    print("wrote exp4_cal_filtered.json")

    # breakdown by loco
    from collections import Counter
    print("by loco:", Counter(r["loco"] for r in kept))
    print("by direction:", Counter(r["direction"] for r in kept))
    pwm_bands = Counter()
    for r in kept:
        pwm_bands[(r["pwm"] // 10) * 10] += 1
    print("by pwm decade:", dict(sorted(pwm_bands.items())))

if __name__ == "__main__":
    main()
