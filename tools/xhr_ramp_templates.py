#!/usr/bin/env python3
"""xhr_ramp_templates.py — empirical ramp-down travel templates, per station and
direction, and their application to candidate Hall baseline windows.

ANALYSIS TOOLING. Diagnostic only. It changes no firmware.

WHAT IS OBSERVED AND WHAT IS ESTIMATED -- the distinction is kept everywhere.
  OBSERVED   a marker crossing: a sustained 70-count departure in the raw Hall
             trace, at a known time, whose distance from the previous marker is
             the SURVEYED spacing in RouteMap.h. Also observed: whether the
             locomotive came to rest inside a marker's field, which fixes its
             resting place to within about the field's own width.
  ESTIMATED  everything between two crossings, and everything after the last
             crossing. Between markers the position is interpolated; after the
             last crossing it is bounded by the next surveyed spacing, because
             that marker demonstrably was NOT reached.

NO SUBSTITUTION. Each station-direction pair is built only from its own runs.
Entry speed at the ramp signal runs from 91 mm/s (Grillers CW, a climb) to
170 mm/s (Grillers CCW, the same grade descending) -- a factor of 1.9 between
two directions at ONE platform. A pair with fewer than two runs is emitted with
`insufficient: true` and must not be used to stand in for another.

HALL UNIFORMITY IS A SUPPORTING CHECK ONLY. A quiet 200 ms window is consistent
with clean line and equally consistent with resting on the flat top of a magnet.
Travel is established from marker timing and surveyed spacing; the Hall's
steadiness is reported alongside and never as proof of movement.

    python3 tools/xhr_ramp_templates.py capture.xhr --session C3B93D0B \
        --templates templates.json --windows windows.csv
"""

import argparse
import csv
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xhr_baseline_timing as T          # noqa: E402
import xhr_baseline_drift as D           # noqa: E402
import xhr_baseline_eligibility as EL    # noqa: E402

STATIONS = {"Patio": 15, "Grillers": 63, "Arches": 108, "Bamboo": 157}
# Measured field extents from NAVI_BASELINE_TIMING_20260916_C3B93D0B, in mm.
TAIL_MM = {"p50": 36, "p95": 40, "max": 71}
LEAD_MM = {"p50": 10, "p99": 18, "max": 21}
SPEED_TOL = 0.13      # a speed carried one marker forward is good to +/-13%


def station_of(m):
    return min(STATIONS.items(),
               key=lambda kv: min(abs(m - kv[1]), 171 - abs(m - kv[1])))[0]


def spacing_step(sp, m, dirn):
    return int(sp[m % 171]) if dirn > 0 else int(sp[(m - 1) % 171])


def ramp_runs(S, truth, ref, sp):
    """One record per ZERO_RAMP episode. st_phase is X18's, used here purely as
    the RECORDED RAMP-DOWN SIGNAL -- the event the runs are aligned on."""
    ph, pa, nd, mm = S["phase"], S["pwm_a"], S["nav_dir"], S["nav_mm"]
    raw = S["raw"].astype(float)
    n = len(raw)
    z = (ph == 3).astype(np.int8)
    c = np.diff(np.concatenate(([0], z, [0])))
    out = []
    for a, b in zip(np.where(c == 1)[0], np.where(c == -1)[0]):
        rest = float(np.median(raw[b + 5000:min(n, b + 20000)]))
        line = float(ref[min(n - 1, b + 5000)])
        dirn = 1 if int(nd[a]) >= 0 else -1
        m0 = int(mm[a])
        cross = [(int(x - a), int(mm[x])) for x in truth if a <= x <= b + 6000]
        pre = [x for x in truth if x < a][-2:]
        v0 = (spacing_step(sp, int(mm[pre[0]]), dirn) / ((pre[1] - pre[0]) / 1000.0)
              if len(pre) == 2 else None)
        m, cum, pts = m0, 0, []
        for rel, _ in cross:
            cum += spacing_step(sp, m, dirn)
            m += dirn
            pts.append((rel, cum))
        out.append(dict(a=int(a), b=int(b), stn=station_of(m0), dirn=dirn, mm0=m0,
                        ramp_ms=int(b - a), pwm0=int(pa[a]), v0=v0, pts=pts,
                        rest=rest, line=line, onmag=bool(abs(rest - line) >= 70),
                        last_mm=m - dirn if pts else m0))
    return out


def build_templates(runs, sp):
    pairs = {}
    for r in runs:
        pairs.setdefault((r["stn"], "CW" if r["dirn"] > 0 else "CCW"), []).append(r)
    out = {}
    for (stn, dn), rs in sorted(pairs.items()):
        v0 = [r["v0"] for r in rs if r["v0"]]
        legs = {}
        for r in rs:
            for j, (rel, cum) in enumerate(r["pts"]):
                legs.setdefault(j, []).append((rel / 1000.0, cum))
        segs = []
        for j in sorted(legs):
            ts = [x[0] for x in legs[j]]
            ds = [x[1] for x in legs[j]]
            segs.append(dict(marker=j + 1, n_runs_observed=len(ts),
                             t_mean_s=round(float(np.mean(ts)), 3),
                             t_min_s=round(float(min(ts)), 3),
                             t_max_s=round(float(max(ts)), 3),
                             cumulative_mm=int(np.mean(ds)), observed=True))
        lastd = segs[-1]["cumulative_mm"] if segs else 0
        lastm = rs[0]["mm0"] + len(segs) * (1 if dn == "CW" else -1)
        gap = spacing_step(sp, lastm, 1 if dn == "CW" else -1)
        onmag = [r for r in rs if r["onmag"]]
        if onmag and not segs:
            lo, hi = gap - 25, gap + 25
            basis = ("OBSERVED: %d of %d runs come to rest inside the next marker's "
                     "field without closing its span, which fixes the travel at one "
                     "surveyed spacing" % (len(onmag), len(rs)))
        elif onmag:
            lo, hi = lastd, lastd + gap + 25
            basis = ("MIXED: %d of %d runs rest inside a marker's field"
                     % (len(onmag), len(rs)))
        else:
            lo, hi = lastd, lastd + gap
            basis = ("ESTIMATED: no further marker was crossed, so the remaining "
                     "travel is bounded above by the next surveyed spacing (%d mm)" % gap)
        out["%s_%s" % (stn, dn)] = dict(
            station=stn, direction=dn, runs=len(rs), insufficient=bool(len(rs) < 2),
            entry_speed_mm_s=(dict(mean=round(float(np.mean(v0)), 1),
                                   min=round(float(min(v0)), 1),
                                   max=round(float(max(v0)), 1), n=len(v0))
                              if v0 else None),
            ramp_pwm_at_signal=rs[0]["pwm0"],
            ramp_step_ms=200, ramp_to_zero_ms=int(np.mean([r["ramp_ms"] for r in rs])),
            observed_crossings=segs,
            final_leg=dict(basis=basis, next_spacing_mm=int(gap),
                           rest_inside_a_field_runs=len(onmag)),
            total_travel_mm=dict(low=int(lo), high=int(hi)),
            run_times_s=[round(r["ramp_ms"] / 1000.0, 2) for r in rs],
            source="xhr_20260916_191904.xhr session C3B93D0B")
    return out


def classify_windows(S, ref, truth, runs, templ, sp):
    raw = S["raw"].astype(float)
    pwm, mm, nd = S["pwm_a"], S["nav_mm"], S["nav_dir"]
    t = S["t_us"] / 1e6
    dirf = ((S["flags"] & 1) > 0).astype(np.int8)
    R5 = dict(EL.RULES)["R5 cadence + PWM sampling gate"]
    _ev, lk, _rj = EL.replay(raw, pwm, dirf, R5)
    rows = []
    for (iend, val, prev, spread, span, piv, cs) in lk:
        ws, we = cs, iend
        rr = next((r for r in runs if r["a"] <= ws <= r["b"] + 8000), None)
        pm = truth[truth <= ws]
        nm = truth[truth > we]
        pm = pm[-1] if len(pm) else None
        nm = nm[0] if len(nm) else None
        dirn = 1 if int(np.median(nd[ws:we + 1])) >= 0 else -1
        regime = v = vlo = vhi = None
        basis = ""
        if rr is not None:
            key = "%s_%s" % (rr["stn"], "CW" if rr["dirn"] > 0 else "CCW")
            regime = "ramp:" + key
            el = (ws - rr["a"]) / 1000.0
            cr = templ[key]["observed_crossings"]
            seg = next((c for c in cr if el <= c["t_max_s"]), None)
            if seg is not None:
                j = cr.index(seg)
                d0 = cr[j - 1]["cumulative_mm"] if j else 0
                t0 = cr[j - 1]["t_mean_s"] if j else 0.0
                v = (seg["cumulative_mm"] - d0) / max(1e-3, seg["t_mean_s"] - t0)
                vlo = (seg["cumulative_mm"] - d0) / max(1e-3, seg["t_max_s"] - t0)
                vhi = (seg["cumulative_mm"] - d0) / max(1e-3, seg["t_min_s"] - t0)
                basis = ("observed leg %d of the %s template (n=%d runs)"
                         % (seg["marker"], key, seg["n_runs_observed"]))
            else:
                basis = ("past the last observed crossing of %s: speed is NOT "
                         "measured there" % key)
        elif piv and piv <= 3000 and pm is not None:
            s = spacing_step(sp, int(mm[pm]), dirn)
            v = s / (piv / 1000.0)
            vlo, vhi = v * (1 - SPEED_TOL), v * (1 + SPEED_TOL)
            regime = "ordinary"
            basis = ("prior marker interval %d ms over surveyed spacing %d mm, "
                     "+/-%d%%" % (piv, s, int(SPEED_TOL * 100)))
        else:
            regime = "no cadence"
            basis = "no qualifying prior marker interval: travel not established"
        w = raw[ws:we]
        rows.append(dict(
            t_s=round(float(t[iend]), 3), regime=regime, station=(rr["stn"] if rr else ""),
            speed_mm_s=(round(v, 1) if v else None),
            dist_mm=(round(v * 0.2, 1) if v else None),
            dist_lo_mm=(round(vlo * 0.2, 1) if vlo else None),
            dist_hi_mm=(round(vhi * 0.2, 1) if vhi else None),
            mm_past_prev_magnet=(round((ws - pm) / 1000.0 * v, 1)
                                 if (v and pm is not None) else None),
            mm_before_next_magnet=(round((nm - we) / 1000.0 * v, 1)
                                   if (v and nm is not None) else None),
            hall_spread_counts=int(w.max() - w.min()), basis=basis))
    return rows


def verdict(r):
    if r["speed_mm_s"] is None:
        return "TRAVEL NOT ESTABLISHED"
    a, b = r["mm_past_prev_magnet"], r["mm_before_next_magnet"]
    if a is None or b is None:
        return "CLEARANCE NOT ESTABLISHED"
    if a >= TAIL_MM["max"] and b >= LEAD_MM["max"]:
        return "clears worst observed field"
    if a >= TAIL_MM["p95"] and b >= LEAD_MM["p99"]:
        return "clears p95 field"
    if a >= TAIL_MM["p50"] and b >= LEAD_MM["p50"]:
        return "clears typical field"
    return "INSIDE the typical field"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--session", required=True)
    ap.add_argument("--templates", help="write the templates here as JSON")
    ap.add_argument("--windows", help="write the window table here as CSV")
    args = ap.parse_args()

    S = T.load_session(args.capture, int(args.session, 16))
    raw = S["raw"].astype(float)
    plain = T.reference_line(raw)
    truth = np.array([e[0] for e in T.detect(raw - plain)])
    ref, _ = D.held_reference(raw, truth)
    sp = T.route_spacing()
    runs = ramp_runs(S, truth, ref, sp)
    templ = build_templates(runs, sp)

    print("== ramp-down runs found: %d ==" % len(runs))
    print("  %-9s %-4s %6s %9s  %s" % ("station", "dir", "runs", "entry mm/s",
                                       "conservative total travel (mm)"))
    for k, v in sorted(templ.items()):
        es = v["entry_speed_mm_s"]
        print("  %-9s %-4s %6d %9s  %d - %d%s"
              % (v["station"], v["direction"], v["runs"],
                 "%.0f" % es["mean"] if es else "-",
                 v["total_travel_mm"]["low"], v["total_travel_mm"]["high"],
                 "   ** INSUFFICIENT (single run) **" if v["insufficient"] else ""))
    rows = classify_windows(S, ref, truth, runs, templ, sp)
    for r in rows:
        r["verdict"] = verdict(r)
    from collections import Counter
    print("\n== candidate 200 ms baseline windows: %d ==" % len(rows))
    for k, v in Counter(r["regime"] for r in rows).most_common():
        print("  %-24s %d" % (k, v))
    print("\n  verdicts:")
    for k, v in Counter(r["verdict"] for r in rows).most_common():
        print("  %-30s %5d  %5.1f%%" % (k, v, 100.0 * v / len(rows)))
    o = [r for r in rows if r["regime"] == "ordinary" and r["dist_mm"]]
    dd = np.array([r["dist_mm"] for r in o])
    print("\n  ordinary running, distance covered by the window:"
          " p1 %.0f p50 %.0f p99 %.0f mm" % tuple(np.percentile(dd, [1, 50, 99])))
    if args.templates:
        json.dump(templ, open(args.templates, "w"), indent=2)
        print("\nwrote %s" % args.templates)
    if args.windows:
        with open(args.windows, "w", newline="") as fh:
            wtr = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
            wtr.writeheader()
            wtr.writerows(rows)
        print("wrote %s (%d rows)" % (args.windows, len(rows)))


if __name__ == "__main__":
    main()
