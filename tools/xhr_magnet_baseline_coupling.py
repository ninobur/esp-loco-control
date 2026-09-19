#!/usr/bin/env python3
"""xhr_magnet_baseline_coupling.py — does a magnet's absolute Hall peak move
with the local ordinary-track baseline, or stay fixed?

ANALYSIS TOOLING. Diagnostic only. Read-only over an X18 Hall recorder
capture (tools/xhr_receiver.py / tools/xhr_decode.py's own source format).
Decides nothing about firmware; answers one empirical question about the
waveform: when X18's local baseline changes, does the absolute Hall reading
at a magnet's peak change with it ("tide lifts the boat", H tracks B, D
stays put) or does the magnet stay near a fixed absolute level ("tide and
pier", H stays put, D moves opposite to B)?

Uses only ADVANCED rulings (confirmed genuine magnet, known MM identity via
nav_mm_after, known direction via nav_dir) from one session.

Per observation:
    B_entry  = entry_baseline   X18's OPERATIVE baseline (HallCapture_::
               baseline_, frozen at the instant this passage opened). This is
               what the shipped detector actually measures departures
               against.
    B_shadow = shadow_baseline  a second, independently-running rolling
               median carried in the same ruling record (HallCapture_::
               shadowBaseline_, "kept running even when fixedAfterPrime
               holds baseline_ static, for observability" -- recorder doc
               §2.4). NOT detection-authoritative and NOT protected against
               magnet-field contamination during a stop the way
               entry_baseline is (see DWELL_MS below) -- but it moves, and
               entry_baseline, in the one session this has been run against,
               essentially does not (see --report output).
    peak     = the recorded departure amplitude, |H - entry_baseline| by
               firmware construction (HallCapture_::close(), decision 0065:
               peak is read from the judged/oriented recording, never a lone
               sample).
    D_entry  = peak, signed by polarity (+peak if N, -peak if S)
    H        = entry_baseline + D_entry   -- the reconstructed absolute Hall
               ADC count at the peak sample. Fixed once; never recomputed
               from B_shadow. H is anchored to entry_baseline because that
               is what the firmware actually subtracted when it measured
               peak -- this is a reconstruction of a real quantity, not an
               independent second measurement of it.
    D_shadow = H - shadow_baseline   -- an alternative departure figure,
               computed directly from H and B_shadow (equal, by linearity of
               the mean, to the mean of per-observation (H-B_shadow), so
               averaging first or subtracting first gives the same answer).

Dwell filter: an ADVANCED ruling with duration_ms above DWELL_MS is a
station dwell (the passage stayed open because the locomotive parked at or
near the magnet), not an ordinary transit. shadow_baseline on a dwell can
walk into the magnet's own field (observed: three dwells at one marker
read 2135/2133/2063 while entry_baseline correctly stayed at 1935 throughout
-- entry_baseline's contamination guard held, shadow_baseline's did not).
These are excluded from every shadow_baseline computation.

    python3 tools/xhr_magnet_baseline_coupling.py capture.xhr --session C3B93D0B --report
"""

import argparse
import os
import sys
from collections import defaultdict

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xhr_format as F   # noqa: E402

DWELL_MS = 2000   # an ADVANCED ruling this long is a station dwell, not a transit


def load_observations(path, want_session):
    obs = []
    for _recv_us, data in F.iter_capture(path):
        try:
            hdr, payload = F.parse_record(data)
        except F.BadRecord:
            continue
        if hdr.session_id != want_session or hdr.rec_type != F.REC_RULING:
            continue
        r = F.parse_ruling(payload)
        if r["ruling"] != "ADVANCED" or not r["is_magnet"]:
            continue
        if r["nav_mm_after"] == F.MM_NA or r["nav_dir"] == 0:
            continue
        pol = r["polarity"]
        Be, Bs, peak = r["entry_baseline"], r["shadow_baseline"], r["peak"]
        D = peak if pol == "N" else -peak
        H = Be + D
        obs.append({
            "mm": r["nav_mm_after"], "dir": "CW" if r["nav_dir"] == 1 else "CCW",
            "pol": pol, "Be": Be, "Bs": Bs, "H": H, "D": D, "absD": peak,
            "dwell": r["duration_ms"] > DWELL_MS,
            "t_ms": r["opened_ms"], "t_min": r["opened_ms"] / 60000.0,
            "pwm": r["pwm_actual"], "dur": r["duration_ms"],
        })
    obs.sort(key=lambda o: o["t_ms"])
    return obs


def by_mm(subset):
    d = defaultdict(list)
    for o in subset:
        d[o["mm"]].append(o)
    return d


def within_magnet_slope(subset, bkey, hkey="H", min_n=2):
    """Panel 'within' (fixed-effects) estimator: demean each magnet's own B
    and H before pooling, so the fitted slope reflects only within-magnet
    movement -- each magnet's own average level is absorbed, which is what
    'controlling for MM identity' means here."""
    d = by_mm(subset)
    bp, hp = [], []
    nmag = 0
    for _mm, rows in d.items():
        if len(rows) < min_n:
            continue
        Bv = np.array([r[bkey] for r in rows], dtype=float)
        Hv = np.array([r[hkey] for r in rows], dtype=float)
        bp.extend(Bv - Bv.mean())
        hp.extend(Hv - Hv.mean())
        nmag += 1
    bp = np.array(bp); hp = np.array(hp)
    if len(bp) < 3 or np.sum(bp * bp) == 0:
        return None
    slope = np.sum(bp * hp) / np.sum(bp * bp)
    resid = hp - slope * bp
    dof = max(len(bp) - nmag - 1, 1)
    se = np.sqrt(np.sum(resid ** 2) / dof / np.sum(bp * bp))
    ss_tot = np.sum(hp ** 2)
    r2 = 1 - np.sum(resid ** 2) / ss_tot if ss_tot > 0 else float("nan")
    return {"slope": slope, "se": se, "r2": r2, "n_obs": len(bp), "n_mag": nmag,
            "ci95": (slope - 1.96 * se, slope + 1.96 * se)}


def direction_pairs(subset):
    """Per magnet observed in both directions: leg-mean B_shadow/H in each
    direction, and the derived departure D = H - B_shadow (computed from the
    leg means directly, which is exactly the mean of per-observation D by
    linearity)."""
    d = by_mm(subset)
    pairs = []
    for mm, rows in d.items():
        rc = [r for r in rows if r["dir"] == "CW"]
        rw = [r for r in rows if r["dir"] == "CCW"]
        if not rc or not rw:
            continue
        Bc, Bw = np.mean([r["Bs"] for r in rc]), np.mean([r["Bs"] for r in rw])
        Hc, Hw = np.mean([r["H"] for r in rc]), np.mean([r["H"] for r in rw])
        Dc, Dw = Hc - Bc, Hw - Bw
        pairs.append({"mm": mm, "Bc": Bc, "Bw": Bw, "Hc": Hc, "Hw": Hw,
                      "Dc": Dc, "Dw": Dw, "dB": Bw - Bc, "dH": Hw - Hc, "dD": Dw - Dc,
                      "nc": len(rc), "nw": len(rw)})
    return pairs


def report(path, want_session):
    obs = load_observations(path, want_session)
    clean = [o for o in obs if not o["dwell"]]
    dwells = [o for o in obs if o["dwell"]]
    print("session %08X: %d ADVANCED rulings with known MM+direction; %d excluded as station dwells (duration_ms>%d)"
          % (want_session, len(obs), len(dwells), DWELL_MS))
    for o in dwells:
        print("   dwell: t=%.2fmin mm=%d dir=%s dur=%dms entry_baseline=%d shadow_baseline=%d"
              % (o["t_min"], o["mm"], o["dir"], o["dur"], o["Be"], o["Bs"]))

    cw = [o for o in clean if o["dir"] == "CW"]
    ccw = [o for o in clean if o["dir"] == "CCW"]
    print("\nclean n=%d  CW=%d (%.2f-%.2f min)  CCW=%d (%.2f-%.2f min)"
          % (len(clean), len(cw), min(o['t_min'] for o in cw), max(o['t_min'] for o in cw),
             len(ccw), min(o['t_min'] for o in ccw), max(o['t_min'] for o in ccw)))
    print("PWM actual: CW mean=%.1f median=%.1f | CCW mean=%.1f median=%.1f  (speed-confound check)"
          % (np.mean([o['pwm'] for o in cw]), np.median([o['pwm'] for o in cw]),
             np.mean([o['pwm'] for o in ccw]), np.median([o['pwm'] for o in ccw])))
    Be_all = sorted(set(o["Be"] for o in clean))
    print("entry_baseline distinct values across the WHOLE session: %s" % Be_all)
    print("shadow_baseline range: CW [%d,%d]  CCW [%d,%d]"
          % (min(o["Bs"] for o in cw), max(o["Bs"] for o in cw),
             min(o["Bs"] for o in ccw), max(o["Bs"] for o in ccw)))

    for pol in ("N", "S"):
        print("\n" + "=" * 78)
        print("POLARITY", pol)
        sub = [o for o in clean if o["pol"] == pol]
        subcw = [o for o in cw if o["pol"] == pol]
        subccw = [o for o in ccw if o["pol"] == pol]
        print("n=%d (CW=%d CCW=%d), unique magnets=%d" % (len(sub), len(subcw), len(subccw), len(by_mm(sub))))

        we = within_magnet_slope(sub, "Be")
        if we:
            print("within-magnet slope(H ~ entry_baseline), all obs: %.3f (se=%.3f, R2=%.3f, n_obs=%d, n_mag=%d)"
                  % (we["slope"], we["se"], we["r2"], we["n_obs"], we["n_mag"]))

        wcw = within_magnet_slope(subcw, "Bs")
        if wcw:
            print("within-magnet slope(H ~ shadow_baseline), CW-only: %.3f (se=%.3f, CI=%.3f..%.3f, R2=%.3f, n_obs=%d, n_mag=%d)"
                  % (wcw["slope"], wcw["se"], wcw["ci95"][0], wcw["ci95"][1], wcw["r2"], wcw["n_obs"], wcw["n_mag"]))
        wccw = within_magnet_slope(subccw, "Bs")
        if wccw:
            print("within-magnet slope(H ~ shadow_baseline), CCW-only: %.3f (se=%.3f, CI=%.3f..%.3f, R2=%.3f, n_obs=%d, n_mag=%d)"
                  % (wccw["slope"], wccw["se"], wccw["ci95"][0], wccw["ci95"][1], wccw["r2"], wccw["n_obs"], wccw["n_mag"]))

        pairs = direction_pairs(sub)
        if pairs:
            dBs = np.array([p["dB"] for p in pairs]); dHs = np.array([p["dH"] for p in pairs]); dDs = np.array([p["dD"] for p in pairs])
            ratios = dHs / dBs
            print("CW-leg vs CCW-leg per magnet (n=%d magnets in both directions):" % len(pairs))
            print("   dB(shadow) mean=%.2f sd=%.2f   dH mean=%.2f sd=%.2f   dD(shadow) mean=%.2f sd=%.2f"
                  % (dBs.mean(), dBs.std(ddof=1), dHs.mean(), dHs.std(ddof=1), dDs.mean(), dDs.std(ddof=1)))
            print("   per-magnet ratio dH/dB: mean=%.3f median=%.3f sd=%.3f min=%.3f max=%.3f"
                  % (ratios.mean(), np.median(ratios), ratios.std(ddof=1), ratios.min(), ratios.max()))
            print("   fraction of magnets with ratio in [0.7,1.3] (boat-like): %.2f   in [-0.3,0.3] (pier-like): %.2f"
                  % (np.mean((ratios >= 0.7) & (ratios <= 1.3)), np.mean((ratios >= -0.3) & (ratios <= 0.3))))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--session", required=True, help="session id in hex, e.g. C3B93D0B")
    ap.add_argument("--report", action="store_true", help="print the analysis (default if no other flag)")
    args = ap.parse_args()
    report(args.capture, int(args.session, 16))


if __name__ == "__main__":
    main()
