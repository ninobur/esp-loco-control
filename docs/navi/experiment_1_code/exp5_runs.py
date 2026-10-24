#!/usr/bin/env python3
"""Identify individual continuous runs at EXACTLY PWM 60 or PWM 90 from
the cal corpus, preserving each run separately (no cross-run averaging,
no PWM bands). Report per-run segment speeds and section ratios."""
import re, glob, json
import numpy as np
from collections import defaultdict

LINE_RE = re.compile(
    r"^MM(\d+)\s+(?:[A-Za-z]+\s+)?obs=(\S+)\s+expected=(\S+)\s+pos=(\S+)\s+dir=(\S+)\s+dt=(\d+)\s+"
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

TARGET_PWM = (60, 90)

def find_runs(rows):
    """Maximal contiguous stretches within one file: pos=EXACT_LOCKED,
    obs==expected, timing_mode=MOVING, pwm exactly 60 or exactly 90 (same
    value for the whole run), seg_mm 250-360, dt>0, MM progression
    adjacent (+1/-1, with wrap)."""
    runs = []
    cur = []
    cur_pwm = None
    cur_dir = None

    def flush():
        nonlocal cur, cur_pwm, cur_dir
        if len(cur) >= 3:  # need at least 2 segments to compute a ratio
            runs.append({"loco": cur[0]["loco"], "file": cur[0]["file"],
                         "pwm": cur_pwm, "direction": cur_dir, "records": list(cur)})
        cur = []; cur_pwm = None; cur_dir = None

    for i, r in enumerate(rows):
        qualifies = (r["pos"] == "EXACT_LOCKED" and r["obs"] == r["expected"]
                     and r["timing_mode"] == "MOVING" and r["pwm"] in TARGET_PWM
                     and 250 <= r["seg_mm"] <= 360 and r["dt"] > 0)
        if not qualifies:
            flush()
            continue
        if not cur:
            cur = [r]; cur_pwm = r["pwm"]; continue
        prev_mm = cur[-1]["mm"]
        d = r["mm"] - prev_mm
        direction = "CW" if (d == 1 or d == -170) else ("CCW" if (d == -1 or d == 170) else None)
        if direction is None or r["pwm"] != cur_pwm:
            flush()
            cur = [r]; cur_pwm = r["pwm"]
            continue
        if cur_dir is None:
            cur_dir = direction
        elif direction != cur_dir:
            flush()
            cur = [r]; cur_pwm = r["pwm"]; cur_dir = None
            continue
        cur.append(r)
    flush()
    return runs


def section_median(records, lo, hi):
    vals = [r["speed_mm_s"] for r in records if lo <= r["mm"] <= hi]
    if len(vals) < 2:
        return None, len(vals)
    return float(np.median(vals)), len(vals)


def main():
    all_rows = []
    for path in sorted(glob.glob("field-records/cal/*.txt")):
        all_rows.extend(parse_file(path))

    runs = []
    by_file = defaultdict(list)
    for r in all_rows:
        by_file[r["file"]].append(r)
    for f, rs in by_file.items():
        runs.extend(find_runs(rs))

    print(f"Total qualifying continuous constant-PWM(60|90) runs (>=3 records): {len(runs)}")
    summary = defaultdict(list)
    for run in runs:
        key = (run["loco"], run["pwm"], run["direction"])
        summary[key].append(run)

    print("\nBy (loco, pwm, direction):")
    for key, rs in sorted(summary.items()):
        total_records = sum(len(r["records"]) for r in rs)
        print(f"  loco={key[0]} pwm={key[1]} dir={key[2]}: {len(rs)} runs, {total_records} records total, "
              f"lengths={[len(r['records']) for r in rs]}")

    print("\n=== Per-run detail: MM range covered, section medians, ratio ===")
    out = []
    for run in runs:
        mms = [r["mm"] for r in run["records"]]
        ref_med, ref_n = section_median(run["records"], 35, 55)
        seg_med, seg_n = section_median(run["records"], 60, 79)
        ratio = (seg_med / ref_med) if (ref_med and seg_med) else None
        rec = {"loco": run["loco"], "pwm": run["pwm"], "direction": run["direction"],
               "file": run["file"], "n_records": len(run["records"]),
               "mm_range": [min(mms), max(mms)],
               "ref_MM035_055_median": ref_med, "ref_MM035_055_n": ref_n,
               "seg_MM060_079_median": seg_med, "seg_MM060_079_n": seg_n,
               "ratio_MM060_079_over_ref": ratio,
               "segment_speeds": [{"mm": r["mm"], "speed_mm_s": r["speed_mm_s"],
                                   "dt_ms": r["dt"], "seg_mm": r["seg_mm"]}
                                  for r in run["records"]]}
        out.append(rec)
        if ref_med or seg_med:
            print(f"  loco={run['loco']} pwm={run['pwm']} dir={run['direction']:3} "
                  f"file={run['file'].split('/')[-1]:40} mm=[{min(mms)}-{max(mms)}] n={len(run['records']):3}  "
                  f"ref(35-55) n={ref_n} med={ref_med}  seg(60-79) n={seg_n} med={seg_med}  ratio={ratio}")

    json.dump(out, open("exp5_runs.json", "w"), indent=1)
    print(f"\nwrote exp5_runs.json ({len(out)} runs)")

if __name__ == "__main__":
    main()
