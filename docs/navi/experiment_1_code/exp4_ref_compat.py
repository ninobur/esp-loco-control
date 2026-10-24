#!/usr/bin/env python3
"""Independently check: are MM035-055 and MM115-125 compatible as a
combined speed reference at PWM>=70, and does that compatibility break
down below PWM 70? Also independently check the MM060-079 directional
claim."""
import json
import numpy as np
from collections import defaultdict

rows = json.load(open("exp4_cal_filtered.json"))

def in_range(mm, lo, hi):
    return lo <= mm <= hi

def pwm_band(pwm, edges):
    for lo, hi, name in edges:
        if lo <= pwm < hi:
            return name
    return None

BANDS = [(0,50,"<50"),(50,70,"50-69"),(70,100,"70-99"),(100,150,"100-149"),(150,250,"150-249")]

def summarize(sel):
    if not sel:
        return None
    v = np.array([r["speed_mm_s"] for r in sel])
    return {"n": len(v), "median": float(np.median(v)), "p25": float(np.percentile(v,25)),
            "p75": float(np.percentile(v,75))}

print("=== Reference-section compatibility: MM115-125 / MM035-055 ratio ===")
print("(grouped by loco, direction, pwm band; ratio of per-group medians)")
groups = defaultdict(lambda: {"ref1": [], "ref2": []})
for r in rows:
    band = pwm_band(r["pwm"], BANDS)
    if band is None:
        continue
    key = (r["loco"], r["direction"], band)
    if in_range(r["mm"], 35, 55):
        groups[key]["ref1"].append(r)
    if in_range(r["mm"], 115, 125):
        groups[key]["ref2"].append(r)

rows_out = []
for key, g in sorted(groups.items()):
    s1, s2 = summarize(g["ref1"]), summarize(g["ref2"])
    if s1 and s2 and s1["n"] >= 5 and s2["n"] >= 5:
        ratio = s2["median"] / s1["median"]
        rows_out.append((key, s1["n"], s2["n"], s1["median"], s2["median"], ratio))
        print(f"  loco={key[0]} dir={key[1]:4} pwm={key[2]:6}  "
              f"MM035-55 n={s1['n']:4} med={s1['median']:6.1f}   "
              f"MM115-125 n={s2['n']:4} med={s2['median']:6.1f}   ratio={ratio:.3f}")

ge70 = [r[5] for r in rows_out if r[0][2] in ("70-99","100-149","150-249")]
lt70 = [r[5] for r in rows_out if r[0][2] in ("<50","50-69")]
if ge70:
    print(f"\nPWM>=70 matched conditions: n_groups={len(ge70)} median_ratio={np.median(ge70):.3f} "
          f"range={min(ge70):.3f}-{max(ge70):.3f}")
if lt70:
    print(f"PWM<70 matched conditions:  n_groups={len(lt70)} median_ratio={np.median(lt70):.3f} "
          f"range={min(lt70):.3f}-{max(lt70):.3f}")

print("\n=== MM060-079 directional check (vs MM035-055 as local reference) ===")
groups2 = defaultdict(lambda: {"ref": [], "seg": []})
for r in rows:
    band = pwm_band(r["pwm"], BANDS)
    if band is None:
        continue
    key = (r["loco"], r["direction"], band)
    if in_range(r["mm"], 35, 55):
        groups2[key]["ref"].append(r)
    if in_range(r["mm"], 60, 79):
        groups2[key]["seg"].append(r)
for key, g in sorted(groups2.items()):
    s1, s2 = summarize(g["ref"]), summarize(g["seg"])
    if s1 and s2 and s1["n"] >= 5 and s2["n"] >= 5:
        ratio = s2["median"] / s1["median"]
        print(f"  loco={key[0]} dir={key[1]:4} pwm={key[2]:6}  "
              f"MM035-55 n={s1['n']:4} med={s1['median']:6.1f}   "
              f"MM060-79 n={s2['n']:4} med={s2['median']:6.1f}   ratio={ratio:.3f}")
