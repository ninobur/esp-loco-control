#!/usr/bin/env python3
"""Measure observed Hall ensemble/baseline extents; never writes firmware."""
from __future__ import annotations
import csv, json
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "research/NAVI_CPR_0_1/data"
OUT = ROOT / "research/navi_wolf_pack/width_study"

# Measurement rule, deliberately independent of RVM's accepted-window bounds.
PRE = 10                 # preceding 200 ms observed baseline candidate
RETURN = 10              # following 200 ms observed baseline candidate
MIN_DEPARTURE = 12.0     # ADC; a measurement convention, not a recognizer rule
MAX_BASELINE_RANGE = 16.0
MAX_BASELINE_SLOPE = 35.0  # ADC/s; "nearly horizontal" operationalization


def line_stats(t, y):
    p = np.polyfit(t - t.mean(), y, 1)
    fit = np.polyval(p, t - t.mean())
    return p[0], float(np.max(np.abs(y-fit)))


def baseline_ok(t, y):
    slope, residual = line_stats(t, y)
    return abs(slope) <= MAX_BASELINE_SLOPE and residual <= MAX_BASELINE_RANGE


def measure(name: str):
    d = np.load(DATA / f"{name}.npz")
    t, y, n = d["t"].astype(float), d["hall"].astype(float), d["n"].astype(int)
    records, i = [], PRE
    while i + RETURN < len(t):
        prior_t, prior_y = t[i-PRE:i], y[i-PRE:i]
        if not baseline_ok(prior_t, prior_y): i += 1; continue
        level = np.polyval(np.polyfit(prior_t-prior_t.mean(), prior_y, 1), t[i]-prior_t.mean())
        noise = max(2., 1.4826*np.median(abs(prior_y-np.median(prior_y))))
        threshold = max(MIN_DEPARTURE, 4*noise)
        if not (abs(y[i]-level) > threshold and abs(y[i+1]-level) > threshold and abs(y[i+2]-level) > threshold):
            i += 1; continue
        start = i
        # End is the first subsequent *observed* flat baseline run, not a projected crossing.
        j = i + 3
        while j + RETURN < len(t) and not baseline_ok(t[j:j+RETURN], y[j:j+RETURN]): j += 1
        if j + RETURN >= len(t):
            records.append(dict(recording=name, start_s=t[start], end_s=None, status="right_censored")); break
        end = j
        records.append(dict(recording=name, start_s=float(t[start]), end_s=float(t[end]),
                            width_s=float(t[end]-t[start]), width_bins=int(end-start),
                            width_samples=int(n[start:end].sum()), polarity="N" if y[start:j].mean()>level else "S",
                            pre_baseline_s=float(t[start]-t[max(0,start-PRE)]),
                            post_baseline_s=None, status="measured"))
        i = end + RETURN
    measured = [r for r in records if r["status"] == "measured"]
    for a, b in zip(measured, measured[1:]): a["post_baseline_s"] = float(b["start_s"] - a["end_s"])
    return records


def q(values):
    a=np.asarray(values, dtype=float)
    return dict(n=len(a), median=float(np.median(a)), minimum=float(a.min()), maximum=float(a.max()),
                p05=float(np.quantile(a,.05)), p95=float(np.quantile(a,.95))) if len(a) else dict(n=0)


def rvm_windows(name):
    # These are original RVM-window records retained by CPR, not ensemble labels.
    rows=list(csv.DictReader((DATA / f"{name}_historical.csv").open()))
    result=[]
    for r in rows:
        try:
            if r["result"] not in {"MM", "RECOGNIZED_NO_CLAIM"}: continue
            result.append(dict(start_s=float(r["start_us"])/1e6, end_s=float(r["end_us"])/1e6,
                               width_s=(float(r["end_us"])-float(r["start_us"]))/1e6))
        except ValueError: pass
    return result


def compare(ensembles, windows):
    out=[]
    for e in ensembles:
        if e.get("status") != "measured": continue
        midpoint=(e["start_s"]+e["end_s"])/2
        candidates=[w for w in windows if w["end_s"] >= e["start_s"] and w["start_s"] <= e["end_s"]]
        if not candidates: continue
        w=min(candidates,key=lambda x:abs((x["start_s"]+x["end_s"])/2-midpoint))
        out.append(dict(**e, rvm_start_s=w["start_s"], rvm_end_s=w["end_s"], rvm_width_s=w["width_s"],
                        start_delta_s=w["start_s"]-e["start_s"], end_delta_s=w["end_s"]-e["end_s"],
                        window_minus_ensemble_s=w["width_s"]-e["width_s"]))
    return out


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    all_records=[]; comparisons=[]
    for name in ("otto_cw","otto_ccw","toby_cw"):
        rec=measure(name); all_records.extend(rec); comparisons.extend(compare(rec,rvm_windows(name)))
    with (OUT/"ensembles.csv").open("w",newline="") as f:
        keys=sorted({k for r in all_records for k in r}); w=csv.DictWriter(f,keys); w.writeheader(); w.writerows(all_records)
    with (OUT/"rvm_window_comparison.csv").open("w",newline="") as f:
        keys=sorted({k for r in comparisons for k in r}); w=csv.DictWriter(f,keys); w.writeheader(); w.writerows(comparisons)
    summary={}
    for loco, names in {"Otto": {"otto_cw","otto_ccw"}, "Toby":{"toby_cw"}}.items():
        x=[r for r in all_records if r["recording"] in names and r["status"]=="measured"]
        c=[r for r in comparisons if r["recording"] in names]
        summary[loco]={"recordings":sorted(names),"ensembles":len(x),"width_seconds":q([r["width_s"] for r in x]),
            "width_20ms_bins":q([r["width_bins"] for r in x]),"width_native_samples":q([r["width_samples"] for r in x]),
            "baseline_seconds":q([r["post_baseline_s"] for r in x if r["post_baseline_s"] is not None]),
            "rvm_window_minus_ensemble_seconds":q([r["window_minus_ensemble_s"] for r in c])}
        summary[loco]["by_polarity"]={p:q([r["width_s"] for r in x if r["polarity"]==p]) for p in ("N","S")}
    (OUT/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    lines=["# NGR magnet ensemble width study", "", "## Evidence coverage", "", "Direct native Hall: Otto CW and CCW, Toby CW, all 2026-10-19. No raw 2026-10-20/21 Hall stream was recoverable locally; it is not represented by a width estimate.", "", "Boundary convention: three successive samples departing a preceding observed 200 ms near-horizontal line by at least 12 ADC (or four robust-noise units); end = first following observed 200 ms near-horizontal line. This is a measurement convention, not an RVM rule. Right-censored/ambiguous traces remain in the CSV.", "", "## Distribution", "", "| Locomotive | ensembles | median width s | 5–95% s | min–max s | median baseline s |", "|---|---:|---:|---:|---:|---:|"]
    for loco,s in summary.items():
        w,b=s["width_seconds"],s["baseline_seconds"]
        lines.append(f"| {loco} | {w['n']} | {w['median']:.3f} | {w['p05']:.3f}–{w['p95']:.3f} | {w['minimum']:.3f}–{w['maximum']:.3f} | {b.get('median',float('nan')):.3f} |")
    lines += ["", "| Locomotive | polarity | n | median width s | 5–95% s |", "|---|---|---:|---:|---:|"]
    for loco,s in summary.items():
        for p,z in s["by_polarity"].items(): lines.append(f"| {loco} | {p} | {z['n']} | {z['median']:.3f} | {z['p05']:.3f}–{z['p95']:.3f} |")
    lines += ["", "## RVM-window comparison", "", "Negative `window_minus_ensemble_seconds` means the accepted RVM window is shorter than the independently measured ensemble. The comparison is overlap-matched only; it does not declare either boundary physically true.", "", "| Locomotive | matched | median window − ensemble s | 5–95% s |", "|---|---:|---:|---:|"]
    for loco,s in summary.items():
        z=s["rvm_window_minus_ensemble_seconds"]; lines.append(f"| {loco} | {z['n']} | {z.get('median',float('nan')):.3f} | {z.get('p05',float('nan')):.3f}–{z.get('p95',float('nan')):.3f} |")
    lines += ["", "IR distance is unavailable in the portable October-19 Hall arrays, so this study reports no IR widths. Polarity is retained per ensemble in `ensembles.csv`; speed/stop state is not available consistently enough in these arrays to support a valid stratified result.", "", "The long/merged and right-censored cases are evidence of ambiguous baseline return, not discarded failures. These measurements describe this corpus only and do not establish a firmware requirement."]
    (OUT/"REPORT.md").write_text("\n".join(lines)+"\n")
    # Dependency-free SVG visual audit: representative short, median, long traces.
    candidates=[r for r in all_records if r["status"]=="measured"]
    chosen=[]
    for recording in ("otto_cw","toby_cw"):
        group=sorted([r for r in candidates if r["recording"]==recording],key=lambda r:r["width_s"])
        if group: chosen.extend([group[0],group[len(group)//2],group[-1]])
    width, panel_h, margin = 900, 145, 55
    svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{panel_h*len(chosen)}">', '<style>text{font:12px sans-serif}.trace{fill:none;stroke:#222;stroke-width:1}</style>']
    for panel,r in enumerate(chosen):
        d=np.load(DATA/f"{r['recording']}.npz"); mask=(d['t']>=r['start_s']-.4)&(d['t']<=r['end_s']+.4)
        tt,yy=d['t'][mask],d['hall'][mask]; xmin,xmax=tt[0],tt[-1]; ymin,ymax=yy.min()-5,yy.max()+5; y0=panel*panel_h
        x=lambda z: margin+(z-xmin)/(xmax-xmin)*(width-2*margin); y=lambda z: y0+panel_h-24-(z-ymin)/(ymax-ymin)*(panel_h-48)
        svg.append(f'<rect x="{x(r["start_s"]):.1f}" y="{y0+20}" width="{x(r["end_s"])-x(r["start_s"]):.1f}" height="{panel_h-44}" fill="#f4b183" opacity=".5"/>')
        svg.append('<polyline class="trace" points="'+' '.join(f'{x(a):.1f},{y(b):.1f}' for a,b in zip(tt,yy))+'"/>')
        svg.append(f'<text x="{margin}" y="{y0+15}">{r["recording"]} {r["polarity"]}: observed ensemble {r["width_s"]:.3f} s</text>')
    svg.append('</svg>'); (OUT/"representative_tracings.svg").write_text(''.join(svg))

if __name__=="__main__": main()
