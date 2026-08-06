#!/usr/bin/env python3
"""Analyze an IR_DIAG capture log (post-CODEX line formats, 2026-08-06).

Input: mosquitto_sub capture lines of the form
    <ISO-timestamp> ngr/spoke/IR_SPEED_SENSOR/diag <LINE>
where <LINE> is one of PULSE / IDLE / STATS / PHASE / LATCH per
docs/IR_DIAG_DAYLIGHT_PREP.md (addendum formats). Raw serial dumps
(no timestamp/topic prefix) are also accepted.

Outputs the daylight-run decision inputs:
  - overall rh / fh medians and p10 from PULSE lines (ground truth)
  - per-phase rh / fh aggregated across PHASE windows (weighted by n)
  - miss rate estimate (interval > 1.8x rolling median), firmware counters
  - saturation, latch, contrast-loss, drop counts
  - the 0009 keep/revert comparison (rh median vs fh median)

Usage: python3 tools/ir_diag_analyze.py <logfile> [--from HH:MM:SS] [--to HH:MM:SS]
"""

import argparse
import re
import statistics
import sys

PULSE_RE = re.compile(
    r"PULSE\s+#\s*(?P<n>\d+)\s+int=\s*(?P<int>\d+)ms\s+w=\s*(?P<w>\d+)ms\s+"
    r"peak=\s*(?P<peak>[+-]\d+)\s+raw=\s*(?P<raw>\d+)\s+rm=\s*(?P<rm>[+-]\d+)\s+"
    r"fm=\s*(?P<fm>[+-]\d+)\s+rh=\s*(?P<rh>[+-]\d+)\s+fh=\s*(?P<fh>[+-]\d+|---)\s+"
    r"base=\s*(?P<base>\d+)\s+span=\s*(?P<span>\d+)\s+"
    r"(?P<qual>OK|MARGINAL|UNAVAIL)(?P<sat>\s+\*SATURATED\*)?"
)
IDLE_RE = re.compile(
    r"IDLE\s+raw=\s*(?P<raw>\d+)\s+env=\s*(?P<emin>\d+)/\s*(?P<emax>\d+)\s+"
    r"seen=\s*(?P<smin>\d+)/\s*(?P<smax>\d+)\s+base=\s*(?P<base>\d+)\s+"
    r"span=\s*(?P<span>\d+)\s+thr=\s*(?P<thi>[+-]\d+)/\s*(?P<tlo>[+-]\d+)\s+"
    r"pulses=\s*(?P<pulses>\d+)(?P<nc>\s+<-- NO USABLE CONTRAST)?"
)
STATS_RE = re.compile(
    r"STATS\s+(?P<up>\d+)s\s+n=(?P<n>\d+)\s+rate=(?P<rate>[\d.]+)/s\s+\|\s+"
    r"int med=(?P<imed>\d+)\s+min=(?P<imin>\d+)\s+max=(?P<imax>\d+)\s+jit=(?P<jit>\d+)\s+\|\s+"
    r"w med=(?P<wmed>\d+)\s+\|\s+peak med=(?P<pmed>-?\d+)\s+\|\s+"
    r"rh med=(?P<rhmed>[+-]\d+)\s+p10=(?P<rhp10>[+-]\d+)\s+\|\s+"
    r"fh med=\s*(?P<fhmed>[+-]\d+|---)\s+p10=\s*(?P<fhp10>[+-]\d+|---)\s+\|\s+"
    r"sat=(?P<sat>\d+)\s+miss=(?P<miss>\d+)\s+latch=(?P<latch>\d+)\s+"
    r"closs=(?P<closs>\d+)\s+drops=(?P<drops>\d+)\s+\|\s+"
    r"~(?P<mms>\d+)mm/s\s+~(?P<pkph>[\d.]+)pkph"
)
PHASE_RE = re.compile(
    r"PHASE\s+(?P<idx>\d+):\s+rh\s+(?P<rh>[+-]\d+)\s+fh\s+(?P<fh>[+-]\d+|---)\s+n=(?P<n>\d+)"
)
PHASE_NOTE_RE = re.compile(r"PHASE\s+note: index restarted")
LATCH_RE = re.compile(
    r"LATCH\s+#\s*(?P<n>\d+)\s+w=(?P<w>\d+)ms\s+fm=\s*(?P<fm>[+-]\d+)\s+"
    r"rm=\s*(?P<rm>[+-]\d+)\s+base=\s*(?P<base>\d+)\s+span=\s*(?P<span>\d+)"
)
PREFIX_RE = re.compile(r"^(?P<ts>\S+)\s+ngr/\S+\s+(?P<rest>.*)$")

SPOKES = 7


def signed_or_none(s):
    return None if s == "---" else int(s)


def med_p10(vals):
    if not vals:
        return None, None
    sv = sorted(vals)
    return statistics.median(sv), sv[max(0, int(len(sv) * 0.10) - (0 if len(sv) >= 10 else 0))]


def fmt(v):
    return "---" if v is None else f"{v:+.0f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile")
    ap.add_argument("--from", dest="t_from", help="HH:MM:SS local, inclusive")
    ap.add_argument("--to", dest="t_to", help="HH:MM:SS local, inclusive")
    args = ap.parse_args()

    pulses = []          # dicts per PULSE line
    stats_lines = []     # dicts per STATS line
    phase_acc = {i: {"rh_wsum": 0.0, "fh_wsum": 0.0, "rh_n": 0, "fh_n": 0} for i in range(SPOKES)}
    phase_windows = 0
    phase_restarts = 0
    latches = []
    idle_nc = 0
    idle_total = 0
    unparsed = []

    with open(args.logfile, errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip():
                continue
            m = PREFIX_RE.match(line)
            ts = None
            body = line
            if m and ("PULSE" in line or "IDLE" in line or "STATS" in line
                      or "PHASE" in line or "LATCH" in line):
                ts = m.group("ts")
                body = m.group("rest")
            if ts is not None:
                hms = ts[11:19] if len(ts) >= 19 else None
                if hms:
                    if args.t_from and hms < args.t_from:
                        continue
                    if args.t_to and hms > args.t_to:
                        continue

            pm = PULSE_RE.search(body)
            if pm:
                d = {k: (signed_or_none(v) if k in ("fh",) else v) for k, v in pm.groupdict().items()}
                for k in ("n", "int", "w", "peak", "raw", "rm", "fm", "rh", "base", "span"):
                    if d[k] is not None and not isinstance(d[k], int):
                        d[k] = int(d[k])
                d["sat"] = bool(pm.group("sat"))
                d["ts"] = ts
                pulses.append(d)
                continue
            sm = STATS_RE.search(body)
            if sm:
                d = sm.groupdict()
                d["ts"] = ts
                stats_lines.append(d)
                continue
            phm = PHASE_RE.search(body)
            if phm:
                idx = int(phm.group("idx"))
                if idx < SPOKES:
                    n = int(phm.group("n"))
                    rh = int(phm.group("rh"))
                    fh = signed_or_none(phm.group("fh"))
                    if n > 0:
                        phase_acc[idx]["rh_wsum"] += rh * n
                        phase_acc[idx]["rh_n"] += n
                        if fh is not None:
                            phase_acc[idx]["fh_wsum"] += fh * n
                            phase_acc[idx]["fh_n"] += n
                    if idx == 0:
                        phase_windows += 1
                continue
            if PHASE_NOTE_RE.search(body):
                phase_restarts += 1
                continue
            lm = LATCH_RE.search(body)
            if lm:
                d = {k: int(v) for k, v in lm.groupdict().items()}
                d["ts"] = ts
                latches.append(d)
                continue
            im = IDLE_RE.search(body)
            if im:
                idle_total += 1
                if im.group("nc"):
                    idle_nc += 1
                continue
            if any(t in body for t in ("PULSE", "STATS", "LATCH")):
                unparsed.append(body[:120])

    print(f"=== IR_DIAG analysis: {args.logfile} ===")
    if not pulses:
        print("No PULSE lines parsed. IDLE lines:", idle_total,
              f"({idle_nc} NO USABLE CONTRAST)")
        if unparsed:
            print("Unparsed candidates (format drift?):")
            for u in unparsed[:5]:
                print("  ", u)
        return

    span_ts = (pulses[0]["ts"], pulses[-1]["ts"])
    print(f"\nPULSE lines: {len(pulses)}   first {span_ts[0]}  last {span_ts[1]}")

    # Overall headrooms from pulse ground truth
    rh_vals = [p["rh"] for p in pulses]
    fh_vals = [p["fh"] for p in pulses if p["fh"] is not None]
    rh_med, rh_p10 = med_p10(rh_vals)
    fh_med, fh_p10 = med_p10(fh_vals)
    print(f"\n-- Headrooms (from PULSE lines) --")
    print(f"rh  med={fmt(rh_med)}  p10={fmt(rh_p10)}  n={len(rh_vals)}")
    print(f"fh  med={fmt(fh_med)}  p10={fmt(fh_p10)}  n={len(fh_vals)} "
          f"({len(pulses) - len(fh_vals)} excluded '---')")
    neg_rh = sum(1 for v in rh_vals if v <= 0)
    neg_fh = sum(1 for v in fh_vals if v <= 0)
    print(f"non-positive: rh {neg_rh}  fh {neg_fh}")

    # Edge-slew margins for reference
    rm_med, rm_p10 = med_p10([p["rm"] for p in pulses])
    fm_med, fm_p10 = med_p10([p["fm"] for p in pulses])
    print(f"(edge-slew reference: rm med={fmt(rm_med)} p10={fmt(rm_p10)}, "
          f"fm med={fmt(fm_med)} p10={fmt(fm_p10)})")

    # 0009 verdict input
    print(f"\n-- Decision 0009 input (symmetric thresholds keep/revert) --")
    if fh_med is not None:
        print(f"rh med {fmt(rh_med)} vs fh med {fmt(fh_med)}  "
              f"(ratio {rh_med / fh_med:.2f}x)" if fh_med else "fh med is 0")
        print("criterion on record: if rh sits FAR ABOVE fh in daylight, revert to asymmetric.")
    else:
        print("no valid fh — cannot form the comparison")

    # Interval / miss analysis (1.8x rolling median of last 8 valid intervals)
    ints = [p["int"] for p in pulses if p["int"] > 0]
    miss_est = 0
    window = []
    for iv in ints:
        if len(window) >= 4:
            med = statistics.median(window)
            if iv > 1.8 * med:
                miss_est += 1
        window.append(iv)
        if len(window) > 8:
            window.pop(0)
    if ints:
        print(f"\n-- Intervals --")
        print(f"n={len(ints)}  med={statistics.median(ints):.0f}ms  "
              f"min={min(ints)}  max={max(ints)}")
        print(f"host miss estimate (>1.8x rolling med of 8): {miss_est} "
              f"({100.0 * miss_est / len(ints):.1f}% of intervals)")
        print("2026-08-05 baseline to beat: 6.7% steady-fast miss rate")

    # Saturation / quality
    sat_pulses = sum(1 for p in pulses if p["sat"])
    qual = {}
    for p in pulses:
        qual[p["qual"]] = qual.get(p["qual"], 0) + 1
    raw_pinned = sum(1 for p in pulses if p["raw"] >= 4000)
    print(f"\n-- Quality --")
    print(f"quality counts: {qual}")
    print(f"*SATURATED* pulses: {sat_pulses}   raw>=4000 at rise: {raw_pinned} "
          f"(pinned >=4000 is the sun-blinded signature)")

    # Firmware counters: n/sat/miss are per-window (reset each STATS);
    # latch/closs/drops are cumulative — take the last line for those.
    if stats_lines:
        s = stats_lines[-1]
        tot_n = sum(int(x["n"]) for x in stats_lines)
        tot_sat = sum(int(x["sat"]) for x in stats_lines)
        tot_miss = sum(int(x["miss"]) for x in stats_lines)
        print(f"\n-- Firmware counters ({len(stats_lines)} STATS windows) --")
        print(f"summed per-window: n={tot_n}  sat={tot_sat}  miss={tot_miss}")
        print(f"cumulative (last line): latch={s['latch']}  closs={s['closs']}  "
              f"drops={s['drops']}")
        if tot_n + tot_miss > 0:
            print(f"firmware miss rate: {100.0 * tot_miss / (tot_n + tot_miss):.1f}% "
                  f"(miss / (n+miss)) — vs 6.7% baseline")
        print(f"last window speed: ~{s['mms']}mm/s ~{s['pkph']}pkph (provisional circumference)")

    # Per-phase profile
    print(f"\n-- Per-phase profile (weighted across {phase_windows} windows, "
          f"{phase_restarts} restarts noted) --")
    rows = []
    for i in range(SPOKES):
        a = phase_acc[i]
        rh_m = a["rh_wsum"] / a["rh_n"] if a["rh_n"] else None
        fh_m = a["fh_wsum"] / a["fh_n"] if a["fh_n"] else None
        rows.append((i, rh_m, fh_m, a["rh_n"]))
        print(f"PHASE {i}: rh {fmt(rh_m):>5}  fh {fmt(fh_m):>5}  n={a['rh_n']}")
    valid_fh_rows = [r for r in rows if r[2] is not None and r[3] > 0]
    if valid_fh_rows:
        weakest = min(valid_fh_rows, key=lambda r: r[2])
        print(f"weakest fh phase: {weakest[0]} ({fmt(weakest[2])})")
        print("darkness baseline was FLAT (rh +345..+500, fh est +190..+295); "
              "a daylight dip = aperture/optics, not geometry")

    # Latches
    if latches:
        print(f"\n-- LATCH discards: {len(latches)} --")
        for l in latches[:10]:
            print(f"  {l.get('ts','')} w={l['w']}ms fm={l['fm']:+d} rm={l['rm']:+d} "
                  f"base={l['base']} span={l['span']}")
        print("  (raw parked mid-band under healthy span = genuine stop; "
              "band itself wrong = real latch. latch= uninterpretable as fault "
              "count without a motion witness.)")

    if unparsed:
        print(f"\n!! {len(unparsed)} unparsed PULSE/STATS/LATCH-like lines "
              f"(possible format drift) — first few:")
        for u in unparsed[:5]:
            print("  ", u)


if __name__ == "__main__":
    main()
