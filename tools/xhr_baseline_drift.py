#!/usr/bin/env python3
"""xhr_baseline_drift.py — the baseline rule under drift, steps and low speed.

ANALYSIS TOOLING. Diagnostic only. It decides nothing and changes no firmware.

Companion to tools/xhr_baseline_timing.py, which fixed WHEN a magnet-only
navigator may sample its baseline. This one asks whether that rule still holds
when the line MOVES and when the locomotive is barely moving.

WHAT IS REAL AND WHAT IS INJECTED
  The Sept 16 waveform and its event timing are never altered. A scenario adds
  an offset o(t) to raw AND the same o(t) to the reference line, so every magnet
  keeps its exact shape at its exact instant; only the line the navigator must
  track moves. A lock is scored against reference(t) + o(t).

  MEASURED rates come from tools/baseline_laps/out/observations.csv (Otto,
  2026-09-14, shadow_baseline while moving). STRESS rates are deliberately
  beyond anything this railway has been seen to do. A stress pass is a stress
  test, not field validation, and is labelled as such in every table.

THE REFERENCE LINE
  A rolling median over samples IN NORMAL MARKER CADENCE only (a magnet within
  2.5 s either side), held constant across stops. The plain rolling median of
  xhr_baseline_timing.py follows the locomotive down when it stands still on a
  magnet, which makes it score its own blind spot as zero error. Scoring may
  look forward in time; the navigator may not, and the replay never does.

STAGES
  --sources   the measured baseline histories found in the repo, and their rates
  --scenarios the offset set, with ADC headroom
  --coverage  how much track one collection spans, by phase
  --stops     stationary stretches: can a low-spread window sit off the line?
  --drift     every scenario against one rule
  --compare   rule variants, including motion requirements
  --all       every stage

    python3 tools/xhr_baseline_drift.py capture.xhr --session C3B93D0B --all
"""

import argparse
import csv
import datetime as dt
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xhr_baseline_timing as T   # noqa: E402

OBS = os.path.join(HERE, "baseline_laps", "out", "observations.csv")
# The one long stretch in observations.csv that is unambiguously temporal: the
# locomotive was moving 100% of the time and covered the whole loop in every
# two-minute bin, so position averages out of it.
RAMP_SESSION = "9950011-B20260914T120419"
RAMP_FROM_S, RAMP_TO_S = 120.0, 1440.0


# --------------------------------------------------------------------------
# measured drift
# --------------------------------------------------------------------------
def measured_sources():
    """Baseline histories in the repo, with the rate each one supports."""
    if not os.path.exists(OBS):
        return None, []
    rows = list(csv.DictReader(open(OBS)))
    by = {}
    for r in rows:
        if r["shadow_baseline"]:
            by.setdefault(r["session_id"], []).append(r)
    out = []
    for sid, g in sorted(by.items()):
        if len(g) < 300:
            continue
        t0 = dt.datetime.fromisoformat(g[0]["ts"])
        t = np.array([(dt.datetime.fromisoformat(r["ts"]) - t0).total_seconds() for r in g])
        sh = np.array([float(r["shadow_baseline"]) for r in g])
        mv = np.array([r["moving"] == "1" for r in g])
        if mv.sum() < 100:
            continue
        tm, sm = t[mv], sh[mv]
        slope = np.polyfit(tm, sm, 1)[0] * 60.0 if len(tm) > 50 else np.nan
        out.append((sid, t0, t[-1] / 60.0, len(g), sm.min(), sm.max(), slope))
    return rows, out


def measured_trajectory():
    """The real 2026-09-14 12:06-12:28 shadow trajectory, as 5 s medians."""
    rows = [r for r in csv.DictReader(open(OBS))
            if r["session_id"] == RAMP_SESSION and r["shadow_baseline"]
            and r["moving"] == "1"]
    t0 = dt.datetime.fromisoformat(rows[0]["ts"])
    t = np.array([(dt.datetime.fromisoformat(r["ts"]) - t0).total_seconds() for r in rows])
    sh = np.array([float(r["shadow_baseline"]) for r in rows])
    m = (t >= RAMP_FROM_S) & (t <= RAMP_TO_S)
    t, sh = t[m] - t[m][0], sh[m] - sh[m][0]
    g = np.arange(0, t[-1], 5.0)
    v, gg = [], []
    for a in g:
        w = sh[(t >= a) & (t < a + 5)]
        if len(w):
            gg.append(a)
            v.append(np.median(w))
    return np.array(gg), np.array(v)


# --------------------------------------------------------------------------
# scenarios
# --------------------------------------------------------------------------
def scenarios(n):
    def ramp(pm):
        return np.arange(n) * (pm / 60000.0)

    def ramp_hold(pm, rise_s, hold_s):
        t = np.arange(n) / 1000.0
        per = 2.0 * (rise_s + hold_s)
        ph = np.mod(t, per)
        amp = pm / 60.0 * rise_s
        up = np.clip(ph / rise_s, 0, 1) * amp
        dn = amp - np.clip((ph - rise_s - hold_s) / rise_s, 0, 1) * amp
        return np.where(ph < rise_s + hold_s, up, dn)

    def step(size, every_s, start_s=30.0):
        t = np.arange(n) / 1000.0
        k = np.maximum(np.floor((t - start_s) / every_s) + 1, 0)
        o = np.where(np.mod(k, 2) == 1, float(size), 0.0)
        o[t < start_s] = 0.0
        return o

    def sine(amp, per_s):
        return amp * np.sin(2 * np.pi * (np.arange(n) / 1000.0) / per_s)

    def meas(gain):
        g, v = measured_trajectory()
        return gain * np.interp(np.mod(np.arange(n) / 1000.0, g[-1]), g, v)

    return [
        ("flat (control)",                    "real",     np.zeros(n)),
        ("measured shape, tiled",             "measured", meas(1.0)),
        ("measured shape x2",                 "stress",   meas(2.0)),
        ("measured shape x4",                 "stress",   meas(4.0)),
        ("ramp +0.08/min (measured slow)",    "measured", ramp(0.078)),
        ("ramp +1.19/min (MEASURED MAX)",     "measured", ramp(1.192)),
        ("ramp -1.19/min",                    "measured", ramp(-1.192)),
        ("ramp +3/min",                       "stress",   ramp(3.0)),
        ("ramp +6/min",                       "stress",   ramp(6.0)),
        ("ramp +12/min",                      "stress",   ramp(12.0)),
        ("ramp-hold +30/min, 60 s up",        "stress",   ramp_hold(30.0, 60.0, 60.0)),
        ("ramp-hold +60/min, 30 s up",        "stress",   ramp_hold(60.0, 30.0, 60.0)),
        ("ramp-hold +120/min, 15 s up",       "stress",   ramp_hold(120.0, 15.0, 60.0)),
        ("sine +/-25, 900 s",                 "stress",   sine(25.0, 900.0)),
        ("step +/-20 every 120 s (measured)", "measured", step(20.0, 120.0)),
        ("step +/-32 every 120 s (measured)", "measured", step(32.0, 120.0)),
        ("step +/-60 every 120 s",            "stress",   step(60.0, 120.0)),
        ("step +/-120 every 120 s",           "stress",   step(120.0, 120.0)),
        ("step +/-32 every 20 s",             "stress",   step(32.0, 20.0)),
    ]


# --------------------------------------------------------------------------
# reference line that does not follow the locomotive into a stop
# --------------------------------------------------------------------------
def held_reference(raw, opens, half=2000, stride=50, excl=40, cadence_ms=2500):
    n = len(raw)
    prev = np.full(n, -10 ** 9)
    cur, j = -10 ** 9, 0
    for i in range(n):
        while j < len(opens) and opens[j] <= i:
            cur = opens[j]
            j += 1
        prev[i] = cur
    nxt = np.full(n, 10 ** 9)
    cur, j = 10 ** 9, len(opens) - 1
    for i in range(n - 1, -1, -1):
        while j >= 0 and opens[j] >= i:
            cur = opens[j]
            j -= 1
        nxt[i] = cur
    idx = np.arange(n)
    moving = ((idx - prev) < cadence_ms) & ((nxt - idx) < cadence_ms)
    anchors = np.arange(0, n, stride)
    rough = np.interp(idx, anchors,
                      np.array([np.median(raw[max(0, k - half):min(n, k + half)])
                                for k in anchors]))
    keep = (np.abs(raw - rough) < excl) & moving
    out, last = [], float(np.median(raw[:2000]))
    for k in anchors:
        lo, hi = max(0, k - half), min(n, k + half)
        w = raw[lo:hi][keep[lo:hi]]
        if len(w) >= 200:
            last = float(np.median(w))
        out.append(last)
    return np.interp(idx, anchors, np.array(out)), moving


# --------------------------------------------------------------------------
# causal replay with optional motion requirements
# --------------------------------------------------------------------------
class Rule(object):
    def __init__(self, thresh=70, need=5, close_ms=30, guard_ms=80,
                 n_samples=200, spread_max=32, prime_ms=2000,
                 max_prior_iv_ms=3000, require_pwm=False, min_mm=None,
                 spacing_mm=304.0):
        for k, v in locals().items():
            if k != "self":
                setattr(self, k, v)


def replay(raw, rule, pwm=None):
    n = len(raw)
    base = float(np.median(raw[:rule.prime_ms]))
    events, locks, rejects = [], [], []
    state, over_run = "IDLE", 0
    open_i = last_over = cstart = -1
    peak, peaksgn, span = 0.0, 0, 0
    collect, guard_until = [], -1
    prev_open, prior_iv = None, None
    i = rule.prime_ms
    while i < n:
        dev = raw[i] - base
        a = dev if dev >= 0 else -dev
        over = a >= rule.thresh
        if state in ("IDLE", "GUARD", "COLLECT"):
            over_run = over_run + 1 if over else 0
            if over_run >= rule.need:
                if state != "IDLE":
                    rejects.append((i, "collided"))
                o = i - rule.need + 1
                prior_iv = (o - prev_open) if prev_open is not None else None
                prev_open = o
                state, open_i, last_over = "OPEN", o, i
                peak, peaksgn, collect = a, (1 if dev > 0 else -1), []
                i += 1
                continue
        if state == "OPEN":
            if over:
                last_over = i
                if a > peak:
                    peak, peaksgn = a, (1 if dev > 0 else -1)
            elif i - last_over >= rule.close_ms:
                span = last_over - open_i
                events.append((open_i, last_over, peak,
                               "N" if peaksgn > 0 else "S", base, prior_iv))
                why = None
                if rule.max_prior_iv_ms is not None and prior_iv is not None \
                        and prior_iv > rule.max_prior_iv_ms:
                    why = "prior interval %d ms" % prior_iv
                elif rule.min_mm is not None:
                    v = (rule.spacing_mm / (prior_iv / 1000.0)) if prior_iv else 0.0
                    if v * rule.n_samples / 1000.0 < rule.min_mm:
                        why = "covers %.0f mm" % (v * rule.n_samples / 1000.0)
                if why:
                    rejects.append((last_over, why))
                    state, over_run = "IDLE", 0
                else:
                    state, guard_until, over_run = "GUARD", last_over + rule.guard_ms, 0
            i += 1
            continue
        if state == "GUARD":
            if i >= guard_until:
                state, collect, cstart = "COLLECT", [], i
            i += 1
            continue
        if state == "COLLECT":
            collect.append(raw[i])
            if len(collect) >= rule.n_samples:
                c = np.asarray(collect, float)
                spread = float(c.max() - c.min())
                if rule.require_pwm and pwm is not None and (pwm[cstart:i + 1] == 0).any():
                    rejects.append((i, "pwm zero"))
                elif spread > rule.spread_max:
                    rejects.append((i, "spread %.0f" % spread))
                else:
                    locks.append((i, float(np.median(c)), base, spread, span,
                                  prior_iv, cstart))
                    base = locks[-1][1]
                state, over_run = "IDLE", 0
            i += 1
            continue
        i += 1
    return events, locks, rejects


def score(raw, line, rule, truth, pwm=None):
    ev, lk, rj = replay(raw, rule, pwm)
    o = {"locks": len(lk), "rejects": len(rj), "lk": lk, "rj": rj, "ev": ev}
    if lk:
        li = np.array([l[0] for l in lk])
        err = np.array([l[1] for l in lk]) - line[li]
        g = np.diff(li).astype(float)
        run = g[g < 5000]
        o.update(err=err, li=li, err_p99=float(np.percentile(np.abs(err), 99)),
                 err_max=float(np.abs(err).max()),
                 n_gt20=int((np.abs(err) > 20).sum()),
                 n_gt70=int((np.abs(err) > 70).sum()),
                 carry_p50=float(np.median(run)) if len(run) else np.nan,
                 carry_max=float(run.max()) if len(run) else np.nan)
    else:
        o.update(err=np.array([]), li=np.array([], int), err_p99=np.nan,
                 err_max=np.nan, n_gt20=0, n_gt70=0,
                 carry_p50=np.nan, carry_max=np.nan)
    evs = np.array([e[0] for e in ev]) if ev else np.array([], int)
    used = np.zeros(len(truth), bool)
    extra, dts = [], []
    for s in evs:
        j = int(np.argmin(np.abs(truth - s)))
        if abs(truth[j] - s) <= 150 and not used[j]:
            used[j] = True
            dts.append(s - truth[j])
        else:
            extra.append(s)
    o.update(matched=int(used.sum()), missed=int((~used).sum()), extra=len(extra),
             extra_idx=extra, dt=np.array(dts) if dts else np.array([0]),
             margin_min=float(min(e[2] for e in ev)) if ev else np.nan)
    return o


# --------------------------------------------------------------------------
# stages
# --------------------------------------------------------------------------
def st_sources():
    print("== measured baseline histories found in this repo ==")
    print("  source: tools/baseline_laps/out/observations.csv  (Otto 9950011,")
    print("          2026-09-14, X16 FLOOR82 field-test sessions; shadow_baseline")
    print("          is the locomotive's own rolling median of the line)")
    rows, out = measured_sources()
    if rows is None:
        print("  NOT FOUND -- synthetic scenarios only, and nothing below is measured.")
        return
    print("  %-30s %8s %7s %7s %8s %12s" % ("session", "start", "min", "shadow", "range",
                                            "trend/min"))
    for sid, t0, mins, n, lo, hi, slope in out:
        mark = "  <== fastest clean ramp" if sid == RAMP_SESSION else ""
        print("  %-30s %8s %7.0f %7.0f %8.0f %+12.3f%s"
              % (sid, t0.strftime("%H:%M:%S"), mins, lo, hi - lo, slope, mark))
    g, v = measured_trajectory()
    print("\n  The one stretch that is unambiguously TEMPORAL: %s," % RAMP_SESSION)
    print("  12:06-12:28. The locomotive was moving 100%% of the time and covered")
    print("  the whole loop in every two-minute bin, so position averages out.")
    print("    %+.0f counts over %.0f min = %+.3f counts/min"
          % (v[-1] - v[0], g[-1] / 60.0, (v[-1] - v[0]) / (g[-1] / 60.0)))
    print("    per-5 s increments: p50 %+.0f p95 %+.0f max %+.0f counts"
          % (*np.percentile(np.diff(v), [50, 95]), np.diff(v).max()))
    print("\n  Other repo logs checked and NOT used:")
    print("    field-records/logs/20260820_morning_session.log -- the 'env' field")
    print("    spans 182 h with excursions to 16275 counts; it is not a clean")
    print("    Hall line and would inject nonsense as if it were drift.")
    print("    firmware/.../Hall_Baseline_Laps.txt -- one stationary session")
    print("    (14:35-15:16 at MM40, PWM 0), fixed 1949 / shadow 1936 throughout;")
    print("    no motion, so no drift rate can be taken from it.")


def st_scenarios(raw):
    n = len(raw)
    print("\n== offset scenarios ==")
    print("  %-36s %-9s %9s %9s  %s" % ("scenario", "kind", "min", "max", "ADC check"))
    for nm, k, o in scenarios(n):
        r = raw + o
        print("  %-36s %-9s %+9.1f %+9.1f  %d..%d %s"
              % (nm, k, o.min(), o.max(), r.min(), r.max(),
                 "ok" if r.min() >= 0 and r.max() <= 4095 else "** CLIPS **"))


def st_coverage(S, ref, truth, rules):
    raw = S["raw"].astype(float)
    sp = T.route_spacing()
    PHN = T.__dict__.get("PHN") or {0: "IDLE", 1: "APPROACH", 2: "ZONE",
                                    3: "ZERO_RAMP", 4: "DWELL", 5: "DEPART"}
    print("\n== how much track does one 200 ms collection span? ==")
    print("   speed from the replay's OWN prior marker interval; spacing from RouteMap.h")
    for nm, R in rules:
        ev, lk, rj = replay(raw, R, S["pwm_a"])
        err = np.array([l[1] for l in lk]) - ref[np.array([l[0] for l in lk])]
        print("   %s" % nm)
        print("     %-11s %6s %6s %6s %6s %8s %9s" % ("phase", "n", "p50", "p5", "min",
                                                      "<20 mm", "|err| max"))
        acc = []
        for j, (i, val, prev, spread, span, piv, cs) in enumerate(lk):
            m0 = int(S["nav_mm"][cs])
            dn = int(np.median(S["nav_dir"][cs:i + 1]))
            s = (int(sp[m0 % 171]) if dn > 0 else int(sp[(m0 - 1) % 171])) if m0 <= 170 else 304
            v = (s / (piv / 1000.0)) if piv else 0.0
            acc.append((PHN[int(np.median(S["phase"][cs:i + 1]))],
                        v * R.n_samples / 1000.0, abs(err[j])))
        for lbl in ("IDLE", "APPROACH", "ZONE", "ZERO_RAMP", "DWELL", "DEPART"):
            g = [a for a in acc if a[0] == lbl]
            if not g:
                continue
            dd = np.array([a[1] for a in g])
            print("     %-11s %6d %6.0f %6.0f %6.0f %6d %9.1f"
                  % (lbl, len(g), np.median(dd), np.percentile(dd, 5), dd.min(),
                     (dd < 20).sum(), max(a[2] for a in g)))
        dd = np.array([a[1] for a in acc])
        print("     %-11s %6d %6.0f %6.0f %6.0f %6d %9.1f"
              % ("ALL", len(acc), np.median(dd), np.percentile(dd, 5), dd.min(),
                 (dd < 20).sum(), np.abs(err).max()))


def st_stops(S, ref):
    raw = S["raw"].astype(float)
    pwm, t = S["pwm_a"], S["t_us"] / 1e6
    z = (pwm == 0).astype(np.int8)
    ch = np.diff(np.concatenate(([0], z, [0])))
    st, en = np.where(ch == 1)[0], np.where(ch == -1)[0]
    print("\n== stationary stretches: can a low-spread window sit off the line? ==")
    print("   the hazard: 200 ms of rock-steady samples that are NOT the line but")
    print("   the flat part of a magnet the locomotive has come to rest on.")
    print("   %9s %8s %7s %10s %11s %8s" % ("t start", "dur(s)", "mm", "held line",
                                            "worst dev", "spread"))
    haz = 0
    for a, b in zip(st, en):
        if b - a < 2000:
            continue
        best = None
        for s in range(a + 200, b - 200, 50):
            w = raw[s:s + 200]
            spd = w.max() - w.min()
            md = np.median(w) - ref[s + 100]
            if spd <= 40 and (best is None or abs(md) > abs(best[0])):
                best = (md, spd)
        if best is None:
            continue
        md, spd = best
        if abs(md) > 20:
            haz += 1
        print("   %9.1f %8.1f %7d %10.0f %+11.0f %8.0f  %s"
              % (t[a], (b - a) / 1000.0, int(S["nav_mm"][a]), ref[a], md, spd,
                 "** %+.0f counts, spread only %.0f **" % (md, spd) if abs(md) > 20 else ""))
    print("   stretches where a spread test alone would NOT have noticed: %d" % haz)


def st_drift(raw, ref, truth, pwm, rules):
    n = len(raw)
    for nm, R in rules:
        print("\n== %s ==" % nm)
        print("   %-36s %-9s %6s %6s %7s %5s %6s %6s %8s"
              % ("scenario", "kind", "locks", "rej", "max|e|", ">20", "missed",
                 "extra", "carry p50"))
        for sc, k, o in scenarios(n):
            s = score(raw + o, ref + o, R, truth, pwm)
            fl = "  <<<" if (s["err_max"] > 20 or s["missed"] > 5 or s["extra"] > 20) else ""
            print("   %-36s %-9s %6d %6d %7.1f %5d %6d %6d %8.0f%s"
                  % (sc, k, s["locks"], s["rejects"], s["err_max"], s["n_gt20"],
                     s["missed"], s["extra"], s["carry_p50"], fl))
        s = score(raw, ref, R, truth, pwm)
        print("   flat control: detection dt p1 %+.0f p50 %+.0f p99 %+.0f ms"
              " (min %+.0f max %+.0f); smallest detection margin %.0f counts vs a"
              " %d-count threshold" % (*np.percentile(s["dt"], [1, 50, 99]),
                                       s["dt"].min(), s["dt"].max(),
                                       s["margin_min"], R.thresh))


def st_compare(raw, ref, truth, pwm):
    n = len(raw)
    SC = scenarios(n)
    MEAS = [s for s in SC if s[1] in ("real", "measured")]
    NO120 = [s for s in SC if "120 every" not in s[0]]
    VAR = [
        ("Sept proposal: g40 N200 spr40, no gate", Rule(guard_ms=40, spread_max=40,
                                                        max_prior_iv_ms=None)),
        ("  + prior interval <= 3000 ms",          Rule(guard_ms=40, spread_max=40)),
        ("  + window covers >= 20 mm",             Rule(guard_ms=40, spread_max=40,
                                                        max_prior_iv_ms=None, min_mm=20)),
        ("  + pwm > 0 throughout",                 Rule(guard_ms=40, spread_max=40,
                                                        max_prior_iv_ms=None,
                                                        require_pwm=True)),
        ("guard 80, spread 32, prior-iv  (V10)",   Rule()),
        ("  drop the spread test",                 Rule(spread_max=10 ** 6)),
        ("  drop the motion gate",                 Rule(max_prior_iv_ms=None)),
        ("  N 200 -> 150",                         Rule(n_samples=150)),
        ("  N 200 -> 100",                         Rule(n_samples=100)),
        ("  guard 80 -> 40",                       Rule(guard_ms=40)),
        ("  guard 80 -> 120",                      Rule(guard_ms=120)),
        ("  spread 32 -> 24",                      Rule(spread_max=24)),
    ]
    print("\n== rule variants ==")
    print("   left: measured scenarios only.  right: all scenarios except the")
    print("   +/-120-count step, which breaks every variant (see the report).")
    print("   %-40s | %6s %6s %7s %4s | %6s %7s %6s %6s"
          % ("variant", "locks", "rej", "max|e|", ">20", "locks", "max|e|",
             "missed", "extra"))
    for nm, R in VAR:
        m = [score(raw + o, ref + o, R, truth, pwm) for _, _, o in MEAS]
        a = [score(raw + o, ref + o, R, truth, pwm) for _, _, o in NO120]
        print("   %-40s | %6d %6d %7.1f %4d | %6d %7.1f %6d %6d"
              % (nm, min(s["locks"] for s in m), max(s["rejects"] for s in m),
                 max(s["err_max"] for s in m), max(s["n_gt20"] for s in m),
                 min(s["locks"] for s in a), max(s["err_max"] for s in a),
                 max(s["missed"] for s in a), max(s["extra"] for s in a)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--session", required=True)
    for s in ("sources", "scenarios", "coverage", "stops", "drift", "compare", "all"):
        ap.add_argument("--" + s, action="store_true")
    args = ap.parse_args()
    pick = [s for s in ("sources", "scenarios", "coverage", "stops", "drift", "compare")
            if getattr(args, s) or args.all] or ["sources"]

    if "sources" in pick:
        st_sources()
    if pick == ["sources"]:
        return

    S = T.load_session(args.capture, int(args.session, 16))
    raw = S["raw"].astype(float)
    plain = T.reference_line(raw)
    opens = np.array([e[0] for e in T.detect(raw - plain)])
    ref, moving = held_reference(raw, opens)
    print("\n(reference line: held across stops; %.1f%% of the session is in normal"
          " marker cadence)" % (100.0 * moving.mean()))
    rules = [("Sept proposal (g40, N200, spread 40, no motion gate)",
              Rule(guard_ms=40, spread_max=40, max_prior_iv_ms=None)),
             ("V10 (g80, N200, spread 32, prior interval <= 3000 ms)", Rule())]
    if "scenarios" in pick:
        st_scenarios(raw)
    if "coverage" in pick:
        st_coverage(S, ref, opens, rules)
    if "stops" in pick:
        st_stops(S, ref)
    if "drift" in pick:
        st_drift(raw, ref, opens, S["pwm_a"], rules)
    if "compare" in pick:
        st_compare(raw, ref, opens, S["pwm_a"])


if __name__ == "__main__":
    main()
