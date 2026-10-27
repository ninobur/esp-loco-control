#!/usr/bin/env python3
"""Question B/C: two successive encounters built from recorded waveforms.

Library: isolated, clean, route-anchored passages from the dev partition
(neighbours >= 0.8 s away). Each passage's contribution is the conditioned
signal minus a straight line through its own flanks (50–400 ms outside the
oracle excursion), tapered to zero outside the excursion.

Synthetic trace = recorded parked background (dev) + passage 1 + passage 2
at a chosen centre-to-centre separation, optionally with a ramp or a step.
Every trace is labelled synthetic; nothing here is field evidence.
"""
import collections
import itertools
import json
from pathlib import Path

import numpy as np

import corpus
import mf
import resolve
from config import FROZEN

OUT = Path(__file__).resolve().parent / 'results'
SEPS_MS = (100, 125, 150, 175, 200, 250, 300, 400, 500, 750, 1000)
DT = mf.DT_S
SEED = 20261026


def library():
    lib, backgrounds = [], []
    for n in corpus.sessions(['dev']):
        s = corpus.load(n)
        tr, exc = s['trace'], s['exc']
        anchors = [r for r in s['refs'] if r['kind'] == 'anchor']
        ts = np.array([r['t'] for r in anchors])
        for j, r in enumerate(anchors):
            nb = np.abs(np.delete(ts, j) - r['t'])
            e = exc[r['k']]
            if (nb.min() if len(nb) else 9) < 0.8 or not e['clean']:
                continue
            i0, i1 = int((e['start'] - 0.4) / DT), int((e['end'] + 0.4) / DT)
            a0, a1 = int((e['start'] - 0.05) / DT), int((e['end'] + 0.05) / DT)
            if i0 < 0 or i1 >= len(tr.x):
                continue
            seg = tr.x[i0:i1]
            if not np.all(np.isfinite(seg)):
                continue
            idx = np.arange(i0, i1)
            flank = (idx < a0) | (idx > a1)
            p = np.polyfit(idx[flank], seg[flank], 1)
            b = seg - np.polyval(p, idx)
            b[flank] = 0.0
            pk = int(np.argmax(b * e['pol']))
            amp = float(b[pk] * e['pol'])
            half = np.flatnonzero(b * e['pol'] >= amp / 2)
            rise = (pk - half[0]) * DT
            fall = (half[-1] - pk) * DT
            lib.append(dict(b=b, peak=pk, pol=e['pol'], amp=amp, fwhm=(half[-1] - half[0] + 1) * DT,
                            asym=rise / max(fall, DT), pwm=e['pwm'], mm=r['mm'], src=n))
        # parked background: 6 s windows with pwm == 0 and small spread
        park = tr.pwm == 0
        L = int(6.0 / DT)
        for i in range(0, len(tr.x) - L, L):
            w = tr.x[i:i + L]
            if park[i:i + L].all() and np.all(np.isfinite(w)) and np.ptp(w) < 40:
                backgrounds.append(w - np.median(w))
    return lib, backgrounds


def synth(bg, p1, p2, sep_s, amp2=1.0, rev2=False, background='stable', rng=None):
    x = bg.copy() + 1950.0
    c1 = int(2.0 / DT)
    c2 = c1 + int(round(sep_s / DT))
    truth = []
    for c, p, scale, rev in ((c1, p1, 1.0, False), (c2, p2, amp2, rev2)):
        b = p['b'][::-1] if rev else p['b']
        pk = len(b) - 1 - p['peak'] if rev else p['peak']
        lo = c - pk
        a, z = max(lo, 0), min(lo + len(b), len(x))
        x[a:z] += scale * b[a - lo:z - lo]
        truth.append(dict(t=(c + 0.5) * DT, pol=p['pol'], fwhm=p['fwhm']))
    t = (np.arange(len(x)) + 0.5) * DT
    if background == 'ramp':
        x += 60.0 * (t - t[0])                       # 60 counts/s
    elif background == 'step':
        x[(c1 + c2) // 2:] += 80.0                   # +80 counts between the two
    tr = mf.Trace(t=t, x=x, pwm=np.full(len(x), 90.0), direction=np.ones(len(x)))
    return tr, truth


def classify(ev, truth, sep_s):
    tol = [min(0.4 * sep_s, max(0.03, 0.5 * tt['fwhm'])) for tt in truth]
    hits = [[e for e in ev if abs(e['t_peak'] - tt['t']) <= tol[k] and e['pol'] == tt['pol']]
            for k, tt in enumerate(truth)]
    matched = {id(e) for h in hits for e in h}
    extra = [e for e in ev if id(e) not in matched]
    mid = [e for e in extra if truth[0]['t'] < e['t_peak'] < truth[1]['t']]
    loc = [e['t_peak'] - tt['t'] for h, tt in zip(hits, truth) for e in h[:1]]
    if not ev:
        out = 'none'
    elif any(len(h) > 1 for h in hits):
        out = 'duplicate'
    elif mid:
        out = 'midpoint'
    elif all(len(h) == 1 for h in hits) and not extra:
        out = 'two_correct'
    elif len(ev) == 1:
        out = 'merged'
    elif all(len(h) == 1 for h in hits):
        out = 'two_plus_extra'
    else:
        out = 'one_found' if sum(len(h) == 1 for h in hits) == 1 else 'mislocated'
    return out, loc


def run(n_draws=3):
    lib, bgs = library()
    rng = np.random.default_rng(SEED)
    fast = [p for p in lib if p['pwm'] >= 85]
    slow = [p for p in lib if 35 <= p['pwm'] <= 70]
    meta = dict(library=len(lib), fast=len(fast), slow=len(slow), backgrounds=len(bgs),
                fast_fwhm_ms=float(np.median([p['fwhm'] for p in fast]) * 1e3),
                slow_fwhm_ms=float(np.median([p['fwhm'] for p in slow]) * 1e3))
    print(meta, flush=True)
    methods = {'baseline': resolve.method_baseline, 'A_local_max': resolve.method_A,
               'B_pursuit': resolve.method_B, 'C_joint': resolve.method_C}
    rows = []
    for pols, amp, width, asym, bgk in itertools.product(
            ('NN', 'SS', 'NS', 'SN'), ('equal', 'half'), ('equal', 'unequal'),
            ('same', 'reversed'), ('stable', 'ramp', 'step')):
        for d in range(n_draws):
            want = [1 if ch == 'N' else -1 for ch in pols]
            pool2 = fast if width == 'equal' else slow
            p1 = rng.choice([p for p in fast if p['pol'] == want[0]])
            p2 = rng.choice([p for p in pool2 if p['pol'] == want[1]])
            bg = bgs[rng.integers(len(bgs))]
            a2 = (p1['amp'] / p2['amp']) if amp == 'equal' else 0.5 * p1['amp'] / p2['amp']
            for sep in SEPS_MS:
                tr, truth = synth(bg, p1, p2, sep / 1e3, amp2=a2, rev2=(asym == 'reversed'),
                                  background=bgk)
                stack = mf.scale_stack(tr, FROZEN)
                for mname, fn in methods.items():
                    if mname in ('A_local_max', 'C_joint'):
                        ev = fn(tr, FROZEN, stack=stack)
                    else:
                        ev = fn(tr, FROZEN)
                    out, loc = classify(ev, truth, sep / 1e3)
                    rows.append(dict(pols=pols, amp=amp, width=width, asym=asym, background=bgk,
                                     draw=d, sep_ms=sep, method=mname, outcome=out,
                                     n_events=len(ev), loc_err_ms=[1e3 * v for v in loc],
                                     src1=p1['src'][13:29] + ':MM%03d' % p1['mm'],
                                     src2=p2['src'][13:29] + ':MM%03d' % p2['mm']))
    return meta, rows


def summarise(rows):
    tab = collections.defaultdict(collections.Counter)
    for r in rows:
        tab[(r['method'], r['sep_ms'])][r['outcome']] += 1
    return {'%s@%d' % k: dict(v) for k, v in sorted(tab.items())}


if __name__ == '__main__':
    OUT.mkdir(exist_ok=True)
    meta, rows = run()
    summ = summarise(rows)
    (OUT / 'overlap_rows.json').write_text(json.dumps(rows))
    (OUT / 'overlap_summary.json').write_text(json.dumps(dict(meta=meta, summary=summ), indent=1))
    for k, v in summ.items():
        print(k, v)
