#!/usr/bin/env python3
"""xhr_baseline_stop_recovery.py — R5 across stops, with line shifts injected.

ANALYSIS TOOLING. Diagnostic only. It changes no firmware.

R5 (tools/xhr_baseline_eligibility.py) freezes its baseline across a stop: the
cadence gate refuses to re-baseline until two magnets have re-established a
qualifying interval, and the PWM sampling gate refuses to collect below PWM 30.
This asks what happens when the LINE moves while the locomotive stands still.

INJECTION. For one recorded stop and one shift, the offset ramps linearly from
0 at the stop's start to delta at its end, then holds. A ramp, not a step: a
step inside the stop would itself be an edge the detector could see, which would
confound "the line moved while you stood still" with "something passed". The
same offset is added to raw and to the held reference line.

THE RECOVERY (--recovery). A frozen baseline that falls far enough behind is
not merely inaccurate: past 70 counts the clean line itself reads as a
departure, the span never closes, no collection can start, and the error can
only grow. The recovery arms when no baseline has been accepted for STALE_MS,
then requires EXCURSIONS fresh band-breaking excursions before it will re-prime.
An excursion is the quiet run restarting -- the signal breaking out of a
BAND-count window -- which happens only when field structure passes the sensor.
A locomotive that has just come to rest produces no further excursions however
recently it was moving, so it can never qualify. That is what stops the recovery
sampling a stationary magnet.

    python3 tools/xhr_baseline_stop_recovery.py capture.xhr --session C3B93D0B --all
"""

import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import xhr_baseline_timing as T        # noqa: E402
import xhr_baseline_drift as D         # noqa: E402
import xhr_baseline_eligibility as EL  # noqa: E402

PP_MS, PP_MIN = 3000, 100.0     # trailing peak-to-peak: motion, baseline-free
QUIET_MAX, BAND = 2500, 40      # the quiet run: running never exceeds ~2.3 s
STALE_MS = 10000                # no baseline accepted for this long -> arm
EXCURSIONS = 2                  # fresh excursions required after arming
DELTAS = [0, 20, -20, 32, -32, 50, -50, 60, -60, 70, -70]


def trailing_pp(raw, w=PP_MS, stride=100):
    n = len(raw)
    idx = np.arange(0, n, stride)
    v = np.array([(lambda s: s.max() - s.min())(raw[max(0, i - w):i + 1]) for i in idx])
    out = np.empty(n)
    for j, i in enumerate(idx):
        out[i:min(n, i + stride)] = v[j]
    return out


def quiet_run(x, band=BAND):
    """How long raw has stayed inside a `band`-count window, ending at i."""
    n = len(x)
    out = np.empty(n, np.int32)
    lo = hi = x[0]
    start = 0
    for i in range(n):
        v = x[i]
        a = min(lo, v)
        b = max(hi, v)
        if b - a <= band:
            lo, hi = a, b
        else:
            j = i
            lo = hi = v
            while j > 0:
                aa = min(lo, x[j - 1])
                bb = max(hi, x[j - 1])
                if bb - aa > band:
                    break
                lo, hi = aa, bb
                j -= 1
            start = j
        out[i] = i - start
    return out


def replay(raw, pwm, dirf, R, pp=None, qr=None, recovery=False):
    """Causal. One sample at a time; nothing from the future."""
    if recovery:
        if pp is None: pp = trailing_pp(raw)
        if qr is None: qr = quiet_run(raw)
    n = len(raw)
    base = float(np.median(raw[:R.prime_ms]))
    events, locks, rejects, reprimes = [], [], [], []
    state, over_run = "IDLE", 0
    open_i = last_over = cstart = -1
    peak, peaksgn, span, ncoll = 0.0, 0, 0, 0
    guard_until = -1
    last_open = None
    prior_iv = None
    prev_dir = int(dirf[R.prime_ms])
    buf = np.empty(R.n_samples, np.float64)
    last_ok = R.prime_ms
    armed, exc = False, 0

    i = R.prime_ms
    while i < n:
        if R.clearing:
            lost = None
            if int(dirf[i]) != prev_dir: lost = "direction change"
            elif pwm[i] <= R.pwm_min: lost = "pwm %d" % pwm[i]
            prev_dir = int(dirf[i])
            if lost is not None:
                if state in ("GUARD", "COLLECT"):
                    rejects.append((i, "eligibility lost in %s (%s)" % (state.lower(), lost)))
                    state, over_run = "IDLE", 0
                last_open = None
        else:
            prev_dir = int(dirf[i])

        if recovery and (i - last_ok) >= STALE_MS:
            if not armed:
                armed, exc = True, 0
            if i > 0 and qr[i] < qr[i - 1]:
                exc += 1
            if exc >= EXCURSIONS and pp[i] >= PP_MIN and qr[i] <= QUIET_MAX \
                    and i >= R.n_samples:
                w = raw[i - R.n_samples:i]
                if (w.max() - w.min()) <= R.spread_max:
                    nb = float(np.median(w))
                    reprimes.append((i, nb, base, i - last_ok))
                    base, last_ok, armed, exc = nb, i, False, 0
                    state, over_run, last_open = "IDLE", 0, None
                    i += 1
                    continue

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
                if a > peak: peak, peaksgn = a, (1 if dev > 0 else -1)
            elif i - last_over >= R.close_ms:
                span = last_over - open_i
                events.append((open_i, last_over, peak, "N" if peaksgn > 0 else "S",
                               base, prior_iv))
                why = None
                if R.cadence_gate:
                    if prior_iv is None: why = "no qualifying interval yet"
                    elif prior_iv > R.cadence_ms: why = "prior interval %d ms" % prior_iv
                if why is None and R.pwm_gate and R.pwm_at_close and pwm[last_over] <= R.pwm_min:
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
            buf[ncoll] = raw[i]; ncoll += 1
            if ncoll >= R.n_samples:
                spread = float(buf.max() - buf.min())
                if spread <= R.spread_max:
                    locks.append((i, float(np.median(buf)), base, spread, span,
                                  prior_iv, cstart))
                    base, last_ok, armed, exc = locks[-1][1], i, False, 0
                else:
                    rejects.append((i, "spread %.0f" % spread))
                state, over_run = "IDLE", 0
            i += 1
            continue
        i += 1
    return events, locks, rejects, reprimes


def offset(n, s, e, delta):
    o = np.zeros(n)
    if delta == 0: return o
    o[s:e] = np.linspace(0.0, float(delta), e - s)
    o[e:] = float(delta)
    return o


def probe(raw, ref, pwm, dirf, R, truth, s, e, delta, recovery, tail=90000):
    n = len(raw)
    stop = min(n, e + tail)
    o = offset(n, s, e, delta)[:stop]
    r = raw[:stop] + o
    line = ref[:stop] + o
    ev, lk, rj, rp = replay(r, pwm[:stop], dirf[:stop], R, recovery=recovery)
    evs = np.array([x[0] for x in ev]) if ev else np.array([], int)
    marg = np.array([x[2] for x in ev]) if ev else np.array([])
    nxt = truth[(truth > e) & (truth < stop)][:2]
    got = []
    for m in nxt:
        if len(evs) and np.abs(evs - m).min() <= 150:
            j = int(np.argmin(np.abs(evs - m)))
            got.append((True, float(marg[j])))
        else:
            got.append((False, np.nan))
    while len(got) < 2: got.append((False, np.nan))
    after = [l for l in lk if l[0] > e]
    fl = next((x for x in rp if x[0] > s), None)
    bad = [(x[0], float(x[1] - line[x[0]])) for x in rp
           if x[0] >= s and abs(x[1] - line[x[0]]) > 20]
    bad += [(l[0], float(l[1] - line[l[0]])) for l in lk
            if l[0] >= s and abs(l[1] - line[l[0]]) > 20]
    fd = sum(1 for x in evs if x >= s and np.abs(truth - x).min() > 150)
    return dict(first=got[0], second=got[1],
                first_lock_ms=(after[0][0] - e) if after else None,
                first_lock_err=(after[0][1] - line[after[0][0]]) if after else None,
                reprime=(fl[0] - e, fl[1] - line[fl[0]]) if fl else None,
                false_detections=fd, bad_locks=bad)


def stops_of(pwm, truth, n, pwm_min=30, min_ms=2000):
    z = (pwm <= pwm_min).astype(np.int8)
    c = np.diff(np.concatenate(([0], z, [0])))
    out = [(int(s), int(min(e, n - 1)))
           for s, e in zip(np.where(c == 1)[0], np.where(c == -1)[0]) if e - s >= min_ms]
    return [(s, e) for s, e in out if e < n - 1000 and len(truth[truth > e]) >= 2]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture")
    ap.add_argument("--session", required=True)
    ap.add_argument("--stops", action="store_true")
    ap.add_argument("--grid", action="store_true")
    ap.add_argument("--recovery", action="store_true")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()

    S = T.load_session(args.capture, int(args.session, 16))
    raw = S["raw"].astype(float)
    pwm = S["pwm_a"]
    dirf = ((S["flags"] & 1) > 0).astype(np.int8)
    plain = T.reference_line(raw)
    truth = np.array([e[0] for e in T.detect(raw - plain)])
    ref, _ = D.held_reference(raw, truth)
    st = stops_of(pwm, truth, len(raw))
    t = S["t_us"] / 1e6
    R5 = dict(EL.RULES)["R5 cadence + PWM sampling gate"]
    print("session %s: %d magnets, %d usable stops" % (args.session, len(truth), len(st)))

    if args.stops or args.all:
        print("\n== the recorded stops ==")
        print("  %3s %9s %8s %9s %10s  %s" % ("#", "end s", "dur s", "line", "resting",
                                              "note"))
        for k, (s, e) in enumerate(st):
            off = np.median(raw[s + 1000:e - 500]) - ref[e]
            print("  %3d %9.1f %8.1f %9.0f %+10.0f  %s"
                  % (k, t[e], (e - s) / 1000.0, ref[e], off,
                     "resting ON a magnet -- the span cannot close"
                     if abs(off) >= 70 else ""))
    if args.grid or args.all:
        for rec_on in (False, True) if (args.recovery or args.all) else (False,):
            print("\n== first two magnets after each stop%s ==" %
                  (" -- WITH the recovery" if rec_on else ""))
            print("  '.' both  '1-' first missed  '2-' second missed  'X' both missed")
            print("  %4s | %s" % ("stop", " ".join("%4s" % ("%+d" % x) for x in DELTAS)))
            for k, (s, e) in enumerate(st):
                row = []
                for dl in DELTAS:
                    r = probe(raw, ref, pwm, dirf, R5, truth, s, e, dl, rec_on)
                    a, b = r["first"][0], r["second"][0]
                    row.append("." if (a and b) else
                               ("X" if not (a or b) else ("1-" if not a else "2-")))
                print("  %4d | %s" % (k, " ".join("%4s" % x for x in row)))


if __name__ == "__main__":
    main()
