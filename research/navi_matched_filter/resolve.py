"""Question C: three ways to separate successive / overlapping encounters.

All three start from the same per-scale scores (mf.scale_stack) and use the
frozen thresholds; they differ only in how candidates become events.

A  local-maximum selection  — per-scale local maxima; a wider-scale candidate
   is discarded when its template core contains qualifying narrower-scale
   candidates on *both* sides of its centre (the midpoint signature); the rest
   is ordinary non-maximum suppression.
B  reconstruction (matching pursuit) — accept the single best candidate,
   subtract its fitted template contribution, rescore the residual, repeat.
C  joint matching — within each candidate cluster compare 0, 1 and 2 template
   explanations (local level + amplitudes by least squares) with BIC.
"""
from __future__ import annotations

import numpy as np

import mf

DT = mf.DT_S


def _candidates(R, A, cfg, r_floor=None):
    """Per-scale local maxima of |r| that pass the thresholds."""
    r_floor = cfg.r_min if r_floor is None else r_floor
    out = []
    for s, D in enumerate(cfg.scales):
        absr = np.where(np.isfinite(R[s]), np.abs(R[s]), -1)
        ok = (absr >= r_floor) & np.isfinite(A[s]) & (np.abs(A[s]) >= cfg.a_min)
        lm = ok & (absr >= np.roll(absr, 1)) & (absr > np.roll(absr, -1))
        rad = max(int(0.5 * D / DT), 1)
        idx = np.flatnonzero(lm)
        idx = idx[np.argsort(-absr[idx])]
        taken = []
        for i in idx:
            if all(abs(i - j) > rad for j in taken):
                taken.append(i)
        out += [(int(i), s, float(R[s, i]), float(A[s, i])) for i in taken]
    return out


def _event(trace, cfg, po, i, s, r, a):
    ipk = int(np.clip(i + po[s], 0, len(trace.t) - 1))
    return dict(i=i, t=float(trace.t[i]), t_peak=float(trace.t[ipk]), scale=float(cfg.scales[s]),
                r=r, A=a, pol=int(np.sign(a)), pwm=float(trace.pwm[i]),
                direction=int(trace.direction[i]))


def method_baseline(trace, cfg):
    return mf.recognize(trace, cfg)[0]


def method_A(trace, cfg, stack=None):
    R, A, po = stack if stack is not None else mf.scale_stack(trace, cfg)
    cand = _candidates(R, A, cfg)
    keep = []
    for c in cand:
        i, s = c[0], c[1]
        half = int(0.5 * cfg.scales[s] / DT)
        left = [d for d in cand if d[1] < s and i - half <= d[0] < i]
        right = [d for d in cand if d[1] < s and i < d[0] <= i + half]
        if left and right:
            continue                     # wide template spanning two narrower fits
        keep.append(c)
    keep.sort(key=lambda c: -abs(c[2]))
    taken = []
    for c in keep:
        rad = int(cfg.nms * cfg.scales[c[1]] / DT)
        if all(abs(c[0] - d[0]) > max(rad, int(cfg.nms * cfg.scales[d[1]] / DT)) for d in taken):
            taken.append(c)
    return [_event(trace, cfg, po, *c) for c in sorted(taken)]


def _templates(cfg):
    shape = cfg.shape if cfg.shape is not None else mf.template_mean()
    return [mf.sampled_template(shape, D, cfg.pad)[0] for D in cfg.scales]


def method_B(trace, cfg, max_iter=6):
    Ts = _templates(cfg)
    x = trace.x.copy()
    events = []
    for _ in range(max_iter):
        tr = mf.Trace(t=trace.t, x=x, pwm=trace.pwm, direction=trace.direction)
        R, A, po = mf.scale_stack(tr, cfg)
        cand = _candidates(R, A, cfg)
        cand = [c for c in cand if all(abs(c[0] - e['i']) > int(0.25 * cfg.scales[c[1]] / DT)
                                       for e in events)]
        if not cand:
            break
        i, s, r, a = max(cand, key=lambda c: abs(c[2]))
        events.append(_event(trace, cfg, po, i, s, r, a))
        T = Ts[s]
        h = len(T) // 2
        lo, hi = max(0, i - h), min(len(x), i + h + 1)
        x[lo:hi] -= a * T[h - (i - lo):h + (hi - i)]
    return sorted(events, key=lambda e: e['t'])


def method_C(trace, cfg, max_cand=10, r_floor=0.5, stack=None):
    Ts = _templates(cfg)
    R, A, po = stack if stack is not None else mf.scale_stack(trace, cfg)
    cand = _candidates(R, A, cfg, r_floor=r_floor)
    if not cand:
        return []
    # clusters: candidates whose windows overlap
    cand.sort()
    clusters, cur, cur_end = [], [], -1
    for c in cand:
        h = len(Ts[c[1]]) // 2
        if cur and c[0] - h > cur_end:
            clusters.append(cur)
            cur, cur_end = [], -1
        cur.append(c)
        cur_end = max(cur_end, c[0] + h)
    clusters.append(cur)
    x = trace.x
    sig = _noise_sigma(x)
    events = []
    for cl in clusters:
        cl = sorted(cl, key=lambda c: -abs(c[2]))[:max_cand]
        lo = max(0, min(c[0] - len(Ts[c[1]]) // 2 for c in cl))
        hi = min(len(x), max(c[0] + len(Ts[c[1]]) // 2 + 1 for c in cl))
        y = x[lo:hi]
        if not np.all(np.isfinite(y)):
            continue
        n = len(y)
        cols = [_place(Ts[c[1]], c[0], lo, hi) for c in cl]
        best = (_bic(y, [], sig), [])
        for a in range(len(cl)):
            best = min(best, (_bic(y, [cols[a]], sig, p_extra=3), [a]), key=lambda b: b[0])
        for a in range(len(cl)):
            for b in range(a + 1, len(cl)):
                if abs(cl[a][0] - cl[b][0]) * DT < 0.04:
                    continue
                best = min(best, (_bic(y, [cols[a], cols[b]], sig, p_extra=6), [a, b]),
                           key=lambda q: q[0])
        chosen = best[1]
        if not chosen:
            continue
        amps = _fit(y, [cols[k] for k in chosen])[1:]
        for k, amp in zip(chosen, amps):
            if abs(amp) >= cfg.a_min:
                i, s, r, _ = cl[k]
                events.append(_event(trace, cfg, po, i, s, r, float(amp)))
    return sorted(events, key=lambda e: e['t'])


def _place(T, i, lo, hi):
    col = np.zeros(hi - lo)
    h = len(T) // 2
    a, b = max(lo, i - h), min(hi, i + h + 1)
    col[a - lo:b - lo] = T[h - (i - a):h + (b - i)]
    return col


def _fit(y, cols):
    X = np.column_stack([np.ones(len(y))] + cols)
    coef, *_ = np.linalg.lstsq(X, y, rcond=None)
    return coef


def _bic(y, cols, sig, p_extra=0):
    coef = _fit(y, cols)
    X = np.column_stack([np.ones(len(y))] + cols)
    rss = float(np.sum((y - X @ coef) ** 2))
    n = len(y)
    return rss / sig ** 2 + (1 + p_extra) * np.log(n)


def _noise_sigma(x):
    d = np.diff(x[np.isfinite(x)])
    return max(1.4826 * np.median(np.abs(d - np.median(d))) / np.sqrt(2), 0.5)
