#!/usr/bin/env python3
"""xhr_baseline_eligibility.py — four movement-eligibility rules, replayed causally.

ANALYSIS TOOLING. Diagnostic only. It decides nothing and changes no firmware.

The raw-signal timing is FIXED and identical in all four rules, as derived in
tools/xhr_baseline_timing.py:

    departure   |raw - baseline| >= 70 counts for 5 consecutive samples
    close       |raw - baseline| < 70 held for 30 ms
    guard       80 ms after the close
    collect     200 raw samples
    accept      the median, only if max - min <= 32 counts

Only the ELIGIBILITY to start and keep a collection differs:

  R1  cadence gate only     the interval between this magnet's start and the
                            preceding magnet's start must be <= 3000 ms.
                            (the recommendation of NAVI_BASELINE_DRIFT)

  R2  PWM proxy only        actual PWM must exceed 30 at collection start and
                            for all 200 samples; PWM <= 30 or a direction change
                            is a LOSS OF MOVEMENT ELIGIBILITY.

  R3  cadence + clearing    the cadence gate, plus the loss-of-eligibility rule:
                            on PWM <= 30 or a direction change, abort any
                            collection, freeze the last valid baseline, and
                            CLEAR the cadence qualification, so that after a
                            resume the first magnet only re-arms the clock and a
                            SECOND magnet is needed to qualify an interval.

  R4  combined              R3 plus the PWM check during collection.

WHAT THE REPLAY MAY SEE
  The raw Hall value, the actual PWM, and the motor direction flag -- all three
  present on the ESP at that instant. It never sees a future sample, the offline
  reference line, X18's station phase, or any X18 ruling. X18's st_phase and
  nav_mm appear ONLY as offline labels in the breakout tables, never in a
  decision.

    python3 tools/xhr_baseline_eligibility.py capture.xhr --session C3B93D0B --all
"""

import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xhr_baseline_timing as T      # noqa: E402
import xhr_baseline_drift as D       # noqa: E402

THRESH, NEED, CLOSE_MS, GUARD_MS, N_SAMPLES, SPREAD_MAX = 70, 5, 30, 80, 200, 32
PHN = {0: "steady running", 1: "approach", 2: "zone", 3: "zero ramp",
       4: "dwell", 5: "departure"}


class Rule(object):
    def __init__(self, name, cadence_gate=False, pwm_gate=False, clearing=False,
                 cadence_ms=3000, pwm_min=30, thresh=THRESH, need=NEED,
                 close_ms=CLOSE_MS, guard_ms=GUARD_MS, n_samples=N_SAMPLES,
                 spread_max=SPREAD_MAX, prime_ms=2000):
        for k, v in locals().items():
            if k != "self":
                setattr(self, k, v)


# ==========================================================================
# THE REPLAY. One sample at a time; nothing from the future.
# ==========================================================================
def replay(raw, pwm, dirf, R):
    n = len(raw)
    base = float(np.median(raw[:R.prime_ms]))
    events, locks, rejects = [], [], []
    state, over_run = "IDLE", 0
    open_i = last_over = cstart = -1
    peak, peaksgn, span, ncoll = 0.0, 0, 0, 0
    guard_until = -1
    last_open = None            # None == cadence qualification cleared
    prior_iv = None
    prev_dir = int(dirf[R.prime_ms])
    buf = np.empty(R.n_samples, np.float64)

    i = R.prime_ms
    while i < n:
        # ---- loss of movement eligibility, tested every sample -------------
        if R.clearing:
            lost = None
            if int(dirf[i]) != prev_dir:
                lost = "direction change"
            elif pwm[i] <= R.pwm_min:
                lost = "pwm %d" % pwm[i]
            prev_dir = int(dirf[i])
            if lost is not None:
                if state in ("GUARD", "COLLECT"):
                    rejects.append((i, "eligibility lost in %s (%s)"
                                    % (state.lower(), lost)))
                    state, over_run = "IDLE", 0
                last_open = None            # clears the cadence qualification
        else:
            prev_dir = int(dirf[i])

        dev = raw[i] - base
        a = dev if dev >= 0 else -dev
        over = a >= R.thresh

        if state in ("IDLE", "GUARD", "COLLECT"):
            over_run = over_run + 1 if over else 0
            if over_run >= R.need:
                if state in ("GUARD", "COLLECT"):
                    rejects.append((i, "next magnet during %s" % state.lower()))
                o = i - R.need + 1
                prior_iv = (o - last_open) if last_open is not None else None
                last_open = o
                state, open_i, last_over = "OPEN", o, i
                peak, peaksgn, ncoll = a, (1 if dev > 0 else -1), 0
                i += 1
                continue

        if state == "OPEN":
            if over:
                last_over = i
                if a > peak:
                    peak, peaksgn = a, (1 if dev > 0 else -1)
            elif i - last_over >= R.close_ms:
                span = last_over - open_i
                events.append((open_i, last_over, peak,
                               "N" if peaksgn > 0 else "S", base, prior_iv))
                why = None
                if R.cadence_gate:
                    if prior_iv is None:
                        why = "no qualifying interval yet"
                    elif prior_iv > R.cadence_ms:
                        why = "prior interval %d ms" % prior_iv
                if why is None and R.pwm_gate and pwm[last_over] <= R.pwm_min:
                    why = "pwm %d at close" % pwm[last_over]
                if why is not None:
                    rejects.append((last_over, why))
                    state, over_run = "IDLE", 0
                else:
                    state, guard_until, over_run = "GUARD", last_over + R.guard_ms, 0
            i += 1
            continue

        if state == "GUARD":
            if i >= guard_until:
                if R.pwm_gate and pwm[i] <= R.pwm_min:
                    rejects.append((i, "pwm %d at collection start" % pwm[i]))
                    state, over_run = "IDLE", 0
                else:
                    state, cstart, ncoll = "COLLECT", i, 0
            i += 1
            continue

        if state == "COLLECT":
            if R.pwm_gate and pwm[i] <= R.pwm_min:
                rejects.append((i, "pwm %d during collection" % pwm[i]))
                state, over_run = "IDLE", 0
                i += 1
                continue
            buf[ncoll] = raw[i]
            ncoll += 1
            if ncoll >= R.n_samples:
                spread = float(buf.max() - buf.min())
                if spread <= R.spread_max:
                    locks.append((i, float(np.median(buf)), base, spread, span,
                                  prior_iv, cstart))
                    base = locks[-1][1]
                else:
                    rejects.append((i, "spread %.0f" % spread))
                state, over_run = "IDLE", 0
            i += 1
            continue
        i += 1
    return events, locks, rejects


RULES = [
    ("R1 cadence gate only (current)", Rule("R1", cadence_gate=True)),
    ("R2 PWM>30 alone",                Rule("R2", pwm_gate=True, clearing=True)),
    ("R3 cadence + clearing",          Rule("R3", cadence_gate=True, clearing=True)),
    ("R4 combined",                    Rule("R4", cadence_gate=True, pwm_gate=True,
                                             clearing=True)),
]


# --------------------------------------------------------------------------
def reason(r):
    s = r[1]
    if "eligibility" in s:
        return "eligibility"
    if "interval" in s or "qualifying" in s:
        return "cadence"
    if "pwm" in s:
        return "pwm"
    if "spread" in s:
        return "spread"
    return "collision"


def score(raw, line, pwm, dirf, R, truth, nav_mm, nav_dir, spacing, stops):
    ev, lk, rj = replay(raw, pwm, dirf, R)
    n = len(raw)
    o = {"locks": len(lk), "rejects": len(rj), "ev": ev, "lk": lk, "rj": rj,
         "rej_by": {k: sum(1 for r in rj if reason(r) == k)
                    for k in ("cadence", "pwm", "eligibility", "spread", "collision")}}
    if lk:
        li = np.array([l[0] for l in lk])
        err = np.array([l[1] for l in lk]) - line[li]
        cov = []
        for (i, val, prev, spread, span, piv, cs) in lk:
            m0 = int(nav_mm[cs])
            dn = int(np.median(nav_dir[cs:i + 1]))
            s = (int(spacing[m0 % 171]) if dn > 0 else int(spacing[(m0 - 1) % 171])) \
                if m0 <= 170 else 304
            v = (s / (piv / 1000.0)) if piv else 0.0
            cov.append(v * R.n_samples / 1000.0)
        cov = np.array(cov)
        g = np.diff(np.concatenate(([R.prime_ms], li, [n]))).astype(float)
        run = g[g < 5000]
        o.update(err=err, li=li, cov=cov,
                 err_p50=float(np.percentile(np.abs(err), 50)),
                 err_p95=float(np.percentile(np.abs(err), 95)),
                 err_p99=float(np.percentile(np.abs(err), 99)),
                 err_max=float(np.abs(err).max()),
                 n_gt20=int((np.abs(err) > 20).sum()),
                 cov_lt20=int((cov < 20).sum()), cov_min=float(cov.min()),
                 stale_p50=float(np.median(run)) if len(run) else np.nan,
                 stale_p95=float(np.percentile(run, 95)) if len(run) else np.nan,
                 stale_run_max=float(run.max()) if len(run) else np.nan,
                 stale_max=float(g.max()))
    else:
        o.update(err=np.array([]), li=np.array([], int), cov=np.array([]),
                 err_p50=np.nan, err_p95=np.nan, err_p99=np.nan, err_max=np.nan,
                 n_gt20=0, cov_lt20=0, cov_min=np.nan, stale_p50=np.nan,
                 stale_p95=np.nan, stale_run_max=np.nan, stale_max=np.nan)
    evs = np.array([e[0] for e in ev]) if ev else np.array([], int)
    used = np.zeros(len(truth), bool)
    extra = []
    for s in evs:
        j = int(np.argmin(np.abs(truth - s)))
        if abs(truth[j] - s) <= 150 and not used[j]:
            used[j] = True
        else:
            extra.append(s)
    marg = np.array([e[2] for e in ev]) if ev else np.array([])
    o.update(matched=int(used.sum()), missed=int((~used).sum()), extra=len(extra),
             margin_min=float(marg.min()) if len(marg) else np.nan, margins=marg)
    f1, f2 = [], []
    for send in stops:
        for k, mg in enumerate(truth[truth > send][:2]):
            det = bool(len(evs) and np.abs(evs - mg).min() <= 150)
            m = float(marg[int(np.argmin(np.abs(evs - mg)))]) if len(evs) else np.nan
            (f1 if k == 0 else f2).append((mg, det, m))
    o.update(first_after_stop=f1, second_after_stop=f2,
             f1_missed=sum(1 for _, dd, _ in f1 if not dd),
             f2_missed=sum(1 for _, dd, _ in f2 if not dd),
             f1_margin=float(min([m for _, dd, m in f1 if dd], default=np.nan)),
             f2_margin=float(min([m for _, dd, m in f2 if dd], default=np.nan)))
    return o


def stops_from_pwm(pwm, pwm_min=30, min_ms=2000):
    z = (pwm <= pwm_min).astype(np.int8)
    c = np.diff(np.concatenate(([0], z, [0])))
    return [int(e) for s, e in zip(np.where(c == 1)[0], np.where(c == -1)[0])
            if e - s >= min_ms]


# --------------------------------------------------------------------------
def st_movement(S, truth):
    pwm, t = S["pwm_a"], S["t_us"] / 1e6
    dirf = ((S["flags"] & 1) > 0).astype(np.int8)
    n = len(pwm)
    print("== the two movement proxies, as evidence ==")
    print("  motor direction flag (F_DIR_FWD): FWD %.1f%% of samples, %d transitions"
          % (100.0 * dirf.mean(), int((np.diff(dirf) != 0).sum())))
    if int((np.diff(dirf) != 0).sum()) == 0:
        print("  => the direction-change clause NEVER FIRES in this session and is UNTESTED.")
        print("     X18's nav_dir does flip once, at t~2307 s, inside a 79 s stop: the")
        print("     locomotive was physically turned, not commanded to reverse.")
    prevmag = np.full(n, -n, np.int64)
    cur, j = -n, 0
    for i in range(n):
        while j < len(truth) and truth[j] <= i:
            cur = truth[j]
            j += 1
        prevmag[i] = cur
    print("\n  Does PWM stay above the threshold while Otto is NOT advancing?")
    for thr in (24, 30, 40):
        stuck = (pwm > thr) & ((np.arange(n) - prevmag) > 3000)
        z = stuck.astype(np.int8)
        c = np.diff(np.concatenate(([0], z, [0])))
        runs = [(x, y) for x, y in zip(np.where(c == 1)[0], np.where(c == -1)[0])
                if y - x > 500]
        print("    PWM>%d with no magnet for over 3 s: %d stretches, %.1f s total,"
              " longest %.1f s" % (thr, len(runs), sum(y - x for x, y in runs) / 1000.0,
                                   max((y - x) / 1000.0 for x, y in runs) if runs else 0))
    print("\n  How many magnets arrive at low PWM (i.e. rolling but throttled back)?")
    for thr in (24, 30, 40):
        at = int((pwm[truth] <= thr).sum())
        still = sum(1 for k in range(len(truth) - 1)
                    if pwm[truth[k]] <= thr and (truth[k + 1] - truth[k]) < 3000)
        print("    PWM<=%2d at the magnet start: %d of %d; followed by another within"
              " 3 s: %d" % (thr, at, len(truth), still))


def st_compare(raw, ref, S, truth, spacing, stops):
    pwm = S["pwm_a"]
    dirf = ((S["flags"] & 1) > 0).astype(np.int8)
    print("\n== the four rules on the real waveform (no injected offset) ==")
    print("  %-32s %6s %5s | %6s %6s %6s %6s | %6s %6s | %8s %8s | %5s %5s %6s | %5s %5s"
          % ("rule", "locks", "rej", "p50", "p95", "p99", "max", "<20mm", "min mm",
             "stale95", "staleMax", "miss", "extra", "margin", "1st", "2nd"))
    out = {}
    for nm, R in RULES:
        s = score(raw, ref, pwm, dirf, R, truth, S["nav_mm"], S["nav_dir"], spacing, stops)
        out[nm] = s
        print("  %-32s %6d %5d | %6.1f %6.1f %6.1f %6.1f | %6d %6.0f | %8.0f %8.0f"
              " | %5d %5d %6.0f | %5d %5d"
              % (nm, s["locks"], s["rejects"], s["err_p50"], s["err_p95"],
                 s["err_p99"], s["err_max"], s["cov_lt20"], s["cov_min"],
                 s["stale_p95"], s["stale_run_max"], s["missed"], s["extra"],
                 s["margin_min"], s["f1_missed"], s["f2_missed"]))
        print("      rejects: %s" % s["rej_by"])
    return out


def st_breakout(raw, ref, S, truth, spacing, stops, out):
    ph = S["phase"]
    print("\n== breakout by phase (X18 st_phase as an OFFLINE LABEL only) ==")
    print("  %-32s %-16s %6s %7s %7s %7s" % ("rule", "phase", "locks", "p95", "max", "<20mm"))
    for nm, _ in RULES:
        s = out[nm]
        for pv in sorted(PHN):
            idx = [k for k, l in enumerate(s["lk"])
                   if int(np.median(ph[l[6]:l[0] + 1])) == pv]
            if not idx:
                continue
            e = np.abs(s["err"][idx])
            print("  %-32s %-16s %6d %7.1f %7.1f %7d"
                  % (nm, PHN[pv], len(idx), np.percentile(e, 95), e.max(),
                     int((s["cov"][idx] < 20).sum())))
        print()
    t = S["t_us"] / 1e6
    print("  first / second magnet after each of the %d stops (stops from PWM<=30 alone):"
          % len(stops))
    for nm, _ in RULES:
        s = out[nm]
        print("    %-32s 1st missed %d (min margin %.0f) ; 2nd missed %d (min margin %.0f)"
              % (nm, s["f1_missed"], s["f1_margin"], s["f2_missed"], s["f2_margin"]))


def st_sensitivity(raw, ref, S, truth, spacing, stops):
    pwm = S["pwm_a"]
    dirf = ((S["flags"] & 1) > 0).astype(np.int8)
    print("\n== PWM threshold sensitivity ==")
    print("  PWM is evidence of drive, not proof of displacement, so the threshold")
    print("  matters only if PWM at a collection instant is informative. It is not:")
    s = score(raw, ref, pwm, dirf, RULES[0][1], truth, S["nav_mm"], S["nav_dir"],
              spacing, stops)
    v = np.array([pwm[l[6]] for l in s["lk"]])
    bands = ((0, 21), (21, 25), (25, 31), (31, 41), (41, 51), (51, 256))
    print("    PWM at collection start: %s"
          % {"%d-%d" % (a, b): int(((v >= a) & (v < b)).sum()) for a, b in bands})
    print("\n  %6s | %-34s | %-22s" % ("PWM >", "combined rule (R4)", "PWM proxy alone (R2)"))
    print("  %6s | %6s %5s %6s %6s %6s | %6s %5s %6s %6s"
          % ("", "locks", "rej", "max", "<20mm", "miss", "locks", "rej", "<20mm", "min mm"))
    for X in (20, 24, 30, 40, 50):
        r4 = Rule("c", cadence_gate=True, clearing=True, pwm_gate=True, pwm_min=X)
        r2 = Rule("p", pwm_gate=True, clearing=True, pwm_min=X)
        a = score(raw, ref, pwm, dirf, r4, truth, S["nav_mm"], S["nav_dir"], spacing, stops)
        b = score(raw, ref, pwm, dirf, r2, truth, S["nav_mm"], S["nav_dir"], spacing, stops)
        print("  %6d | %6d %5d %6.1f %6d %6d | %6d %5d %6d %6.0f"
              % (X, a["locks"], a["rejects"], a["err_max"], a["cov_lt20"], a["missed"],
                 b["locks"], b["rejects"], b["cov_lt20"], b["cov_min"]))


def st_drift(raw, ref, S, truth, spacing, stops):
    pwm = S["pwm_a"]
    dirf = ((S["flags"] & 1) > 0).astype(np.int8)
    n = len(raw)
    SC = D.scenarios(n)
    res = {nm: [(sc, k, score(raw + o, ref + o, pwm, dirf, R, truth, S["nav_mm"],
                              S["nav_dir"], spacing, stops)) for sc, k, o in SC]
           for nm, R in RULES}
    print("\n== drift scenarios (measured rates and clearly labelled stress) ==")
    for grp, keep in (("MEASURED", lambda k: k in ("real", "measured")),
                      ("STRESS, excluding the +/-120 step", lambda k: k == "stress")):
        print("\n  %s -- worst case over the group" % grp)
        print("  %-32s %6s %6s %6s %5s %6s %6s %6s %7s %6s"
              % ("rule", "locks", "rej", "max|e|", ">20", "<20mm", "miss", "extra",
                 "margin", "1st/2nd"))
        for nm, _ in RULES:
            rows = [s for sc, k, s in res[nm] if keep(k) and "120 every" not in sc]
            print("  %-32s %6d %6d %6.1f %5d %6d %6d %6d %7.0f %6s"
                  % (nm, min(s["locks"] for s in rows), max(s["rejects"] for s in rows),
                     max(s["err_max"] for s in rows), max(s["n_gt20"] for s in rows),
                     max(s["cov_lt20"] for s in rows), max(s["missed"] for s in rows),
                     max(s["extra"] for s in rows), min(s["margin_min"] for s in rows),
                     "%d/%d" % (max(s["f1_missed"] for s in rows),
                                max(s["f2_missed"] for s in rows))))
    print("\n  every scenario where R4 (combined) is WORSE than R1 (current):")
    print("  %-36s %-9s | %-22s | %-22s" % ("scenario", "kind", "R1", "R4"))
    for i in range(len(res[RULES[0][0]])):
        sc, k, a = res[RULES[0][0]][i]
        _, _, b = res[RULES[3][0]][i]
        worse = (b["missed"] > a["missed"] or b["extra"] > a["extra"]
                 or b["locks"] < a["locks"] or b["err_max"] > a["err_max"])
        if not worse:
            continue
        print("  %-36s %-9s | locks %5d miss %3d extra %4d | locks %5d miss %3d extra %4d"
              % (sc, k, a["locks"], a["missed"], a["extra"],
                 b["locks"], b["missed"], b["extra"]))
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--session", required=True)
    for s in ("movement", "compare", "breakout", "sensitivity", "drift", "all"):
        ap.add_argument("--" + s, action="store_true")
    args = ap.parse_args()
    pick = [s for s in ("movement", "compare", "breakout", "sensitivity", "drift")
            if getattr(args, s) or args.all] or ["compare"]

    S = T.load_session(args.capture, int(args.session, 16))
    raw = S["raw"].astype(float)
    plain = T.reference_line(raw)
    truth = np.array([e[0] for e in T.detect(raw - plain)])
    ref, moving = D.held_reference(raw, truth)
    spacing = T.route_spacing()
    stops = stops_from_pwm(S["pwm_a"])
    print("session %s: %d samples, %d magnets, %d stops (PWM<=30 for >=2 s)"
          % (args.session, len(raw), len(truth), len(stops)))
    if "movement" in pick:
        st_movement(S, truth)
    out = None
    if set(pick) & {"compare", "breakout"}:
        out = st_compare(raw, ref, S, truth, spacing, stops)
    if "breakout" in pick:
        st_breakout(raw, ref, S, truth, spacing, stops, out)
    if "sensitivity" in pick:
        st_sensitivity(raw, ref, S, truth, spacing, stops)
    if "drift" in pick:
        st_drift(raw, ref, S, truth, spacing, stops)


if __name__ == "__main__":
    main()
