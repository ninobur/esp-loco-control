"""Reference labelling for continuous recordings — independent of the template.

The matched filter must be scored against something it did not produce. The
reference is built in three steps; none uses the template or the recognizer:

1. Excursion oracle (offline; sees the whole recording). A centred rolling
   median over ORACLE_BASE_S is the local level. Contiguous same-sign runs with
   |x - level| >= ORACLE_ENTER are excursions, kept when the peak reaches
   ORACLE_PEAK. An excursion is *clean* when it lasts >= CLEAN_MIN_S above the
   entry level and the level 50–400 ms before it agrees with the level 50–400
   ms after it to within CLEAN_STEP_FRAC of its amplitude. (Field recordings
   contain abrupt background steps and short noise bursts; the clean test keeps
   them out of the anchors, not out of the data.)
2. Route anchoring. Within each recorded-direction run, every ANCHOR_W
   consecutive clean excursions whose polarities match exactly one place in
   RouteMap.h ROUTE_POLARITY vote for that placement. Anchors are kept only in
   chains of >= CHAIN_MIN route-consecutive placements.
3. Expected magnets. Between two chains in the same run that are at most
   BRIDGE_MAX route steps apart, the route magnets the chains skipped are
   *expected* at times interpolated by surveyed spacing (ROUTE_SPACING_MM).

Scoring happens only inside anchored spans (first to last anchor of a bridged
chain group). Outside them nothing is claimed either way.
This is evidence about which route magnet produced an excursion, not physical
ground truth; anything it cannot place is left unplaced, not deleted.
"""
from __future__ import annotations

import re
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
ROUTE_H = ROOT.parents[1] / 'firmware' / 'programs' / 'NAVI_ONE' / 'variants' / 'NAVI_ONE_X20' / 'RouteMap.h'

ORACLE_BASE_S = 4.0
ORACLE_ENTER = 30.0
ORACLE_PEAK = 60.0
ORACLE_MERGE_S = 0.030
CLEAN_MIN_S = 0.040
CLEAN_STEP_FRAC = 0.4
ANCHOR_W = 10
CHAIN_MIN = 4
BRIDGE_MAX = 8
BRIDGE_TIME_TOL = 0.2
DIR_CW, DIR_CCW = 1, 2      # recorded native 'direction'; 1 = CW = increasing MM


def _table(name):
    text = ROUTE_H.read_text()
    body = re.search(name + r'\[ROUTE_N\]\s*=\s*\{(.*?)\};', text, re.S).group(1)
    v = np.array([int(x) for x in re.findall(r'\d+', body)])
    assert len(v) == 171
    return v


def route_polarity():
    return np.where(_table('ROUTE_POLARITY') == 1, 1, -1)   # +1 North (ADC rises)


def route_spacing():
    return _table('ROUTE_SPACING_MM')        # mm from marker i to i+1 (CW)


def rolling_median(x, n):
    """Centred rolling median over n bins, NaN-aware, via block decimation."""
    step = max(n // 40, 1)
    idx = np.arange(0, len(x), step)
    h = n // 2
    out = np.empty(len(idx))
    for k, i in enumerate(idx):
        w = x[max(0, i - h):i + h + 1]
        w = w[np.isfinite(w)]
        out[k] = np.median(w) if len(w) else np.nan
    return np.interp(np.arange(len(x)), idx, out)


def oracle(trace, dt=0.002):
    x = trace.x
    level = rolling_median(x, int(ORACLE_BASE_S / dt))
    d = x - level
    sgn = np.where(d >= ORACLE_ENTER, 1, np.where(d <= -ORACLE_ENTER, -1, 0))
    sgn[~np.isfinite(d)] = 0
    edges = np.flatnonzero(np.diff(np.concatenate([[0], sgn, [0]])) != 0)
    runs = [[a, b, sgn[a]] for a, b in zip(edges[:-1], edges[1:]) if sgn[a] != 0]
    merged = []
    for r in runs:
        if merged and r[2] == merged[-1][2] and (r[0] - merged[-1][1]) * dt <= ORACLE_MERGE_S:
            merged[-1][1] = r[1]
        else:
            merged.append(r)
    ev = []
    for a, b, s in merged:
        seg = d[a:b] * s
        k = int(np.nanargmax(seg))
        amp = float(seg[k])
        if amp < ORACLE_PEAK:
            continue
        pre = x[max(0, a - int(.4 / dt)):max(0, a - int(.05 / dt))]
        post = x[b + int(.05 / dt):b + int(.4 / dt)]
        pre, post = pre[np.isfinite(pre)], post[np.isfinite(post)]
        step = abs(np.median(post) - np.median(pre)) if len(pre) and len(post) else np.inf
        clean = ((b - a) * dt >= CLEAN_MIN_S) and step <= CLEAN_STEP_FRAC * amp
        ev.append(dict(i=a + k, t=float(trace.t[a + k]), pol=int(s), amp=amp,
                       start=float(trace.t[a]), end=float(trace.t[b - 1]),
                       level_step=float(step), clean=bool(clean),
                       pwm=float(trace.pwm[a + k]), direction=int(trace.direction[a + k])))
    return ev, level


def _runs_by_direction(ev):
    runs, cur = [], []
    for k, e in enumerate(ev):
        if cur and ev[cur[-1]]['direction'] != e['direction']:
            runs.append(cur)
            cur = []
        cur.append(k)
    if cur:
        runs.append(cur)
    return runs


def _chains(ev, run, dirn, route):
    """Anchor clean excursions of one run; return chains [(k, mm), ...]."""
    N = len(route)
    clean = [k for k in run if ev[k]['clean']]
    pol = np.array([ev[k]['pol'] for k in clean])
    votes = [dict() for _ in clean]
    shifts = np.array([[(m + dirn * q) % N for q in range(ANCHOR_W)] for m in range(N)])
    rp = route[shifts]                                   # N x W polarity windows
    for j in range(len(clean) - ANCHOR_W + 1):
        hits = np.flatnonzero((rp == pol[j:j + ANCHOR_W]).all(axis=1))
        if len(hits) == 1:
            for q in range(ANCHOR_W):
                mm = int(shifts[hits[0], q])
                votes[j + q][mm] = votes[j + q].get(mm, 0) + 1
    cand = []
    for j, k in enumerate(clean):
        if votes[j]:
            mm, c = max(votes[j].items(), key=lambda kv: kv[1])
            if c >= 2:
                cand.append((k, mm))
    chains, cur = [], cand[:1]
    for (ka, ma), (kb, mb) in zip(cand[:-1], cand[1:]):
        if ((mb - ma) * dirn) % N == 1:
            cur.append((kb, mb))
        else:
            chains.append(cur)
            cur = [(kb, mb)]
    if cur:
        chains.append(cur)
    return [c for c in chains if len(c) >= CHAIN_MIN]


def reference(ev, route=None, spacing=None):
    """Build reference magnets and scored spans from oracle excursions."""
    route = route_polarity() if route is None else route
    spacing = route_spacing() if spacing is None else spacing
    N = len(route)
    refs, spans = [], []
    for e in ev:
        e['mm'] = None
    for run in _runs_by_direction(ev):
        d0 = ev[run[0]]['direction']
        options = [1] if d0 == DIR_CW else [-1] if d0 == DIR_CCW else [1, -1]
        best = max((_chains(ev, run, dn, route) for dn in options),
                   key=lambda ch: sum(len(c) for c in ch))
        dirn = options[0] if len(options) == 1 else (
            1 if best == _chains(ev, run, 1, route) else -1)
        groups = []
        for ch in best:
            if groups:
                kb, mb = groups[-1][-1]
                ka, ma = ch[0]
                step = ((ma - mb) * dirn) % N
                gap = ev[ka]['t'] - ev[kb]['t']
                tail = [ev[k]['t'] for k, _ in groups[-1][-4:]]
                head = [ev[k]['t'] for k, _ in ch[:4]]
                local = np.median(np.concatenate([np.diff(tail), np.diff(head)])) if (
                    len(tail) > 1 and len(head) > 1) else np.nan
                # bridge only when the skipped count agrees with the local interval
                if (1 < step <= BRIDGE_MAX and gap < 15.0 and np.isfinite(local)
                        and abs(gap / step - local) <= BRIDGE_TIME_TOL * local):
                    groups[-1].extend(ch)
                    continue
            groups.append(list(ch))
        for g in groups:
            spans.append((ev[g[0][0]]['t'], ev[g[-1][0]]['t'], dirn))
            for (ka, ma), (kb, mb) in zip(g[:-1], g[1:]):
                refs.append(dict(mm=ma, pol=int(route[ma]), t=ev[ka]['t'], kind='anchor', k=ka))
                ev[ka]['mm'] = ma
                step = ((mb - ma) * dirn) % N
                if step > 1:
                    # cumulative surveyed distance along direction of travel
                    mms = [(ma + dirn * q) % N for q in range(step + 1)]
                    seg = [spacing[m] if dirn > 0 else spacing[(m - 1) % N] for m in mms[:-1]]
                    cum = np.concatenate([[0], np.cumsum(seg)]) / np.sum(seg)
                    ta, tb = ev[ka]['t'], ev[kb]['t']
                    for q in range(1, step):
                        refs.append(dict(mm=int(mms[q]), pol=int(route[mms[q]]),
                                         t=float(ta + cum[q] * (tb - ta)), kind='expected',
                                         interval=float((tb - ta) / step)))
            kl, ml = g[-1]
            refs.append(dict(mm=ml, pol=int(route[ml]), t=ev[kl]['t'], kind='anchor', k=kl))
            ev[kl]['mm'] = ml
    refs.sort(key=lambda r: r['t'])
    # local interval for every reference (for tolerance)
    ts = np.array([r['t'] for r in refs])
    for j, r in enumerate(refs):
        nb = np.diff(ts[max(0, j - 1):j + 2])
        r.setdefault('interval', float(np.median(nb)) if len(nb) else 1.0)
    return refs, spans
