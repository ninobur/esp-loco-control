"""Corpus loading, partitions and scoring against the route-anchored reference.

Partitions are by whole recording session and were fixed before any
recognizer threshold was examined:
  level1      9950011_20261025_133357 (Muse's 96.8 s file) — reproduction only
  dev         Otto, Oct 25 sessions — the only data thresholds may be chosen on
  validation  Otto, Oct 26 sessions — held out
  transfer    Toby, Oct 25 + 26 — held out; different locomotive and sensor
The template itself comes from the Oct 15 Otto curves (aligned_curves.csv),
which no partition here contains.
"""
from __future__ import annotations

import os
import pickle
from pathlib import Path

import numpy as np

import mf
import truth

NSR_DIR = Path(os.environ.get('NGR_NSR_NPZ', ''))
CACHE = Path(os.environ.get('NGR_MF_CACHE', '/tmp/ngr_mf_cache'))
LEVEL1 = '9950011_20261025_133357'
MIN_SAMPLES = 50000


def partition_of(name):
    if name == LEVEL1:
        return 'level1'
    loco = '9950011' if '9950011' in name else '9950012'
    day = '20261025' if '_20261025_' in name else '20261026'
    if loco == '9950012':
        return 'transfer'
    return 'dev' if day == '20261025' else 'validation'


def load(name):
    """Conditioned trace + oracle excursions + route reference (cached)."""
    CACHE.mkdir(parents=True, exist_ok=True)
    c = CACHE / (name + '.v3.pkl')
    if c.exists():
        return pickle.loads(c.read_bytes())
    native = np.load(NSR_DIR / (name + '.npz'))['native']
    tr = mf.condition(native, name=name)
    exc, level = truth.oracle(tr)
    refs, spans = truth.reference(exc)
    out = dict(name=name, trace=tr, exc=exc, refs=refs, spans=spans,
               partition=partition_of(name))
    c.write_bytes(pickle.dumps(out))
    return out


def sessions(partitions=None):
    names = []
    for p in sorted(NSR_DIR.glob('*.npz')):
        n = p.stem
        with np.load(p) as z:
            if len(z['native']) < MIN_SAMPLES:
                continue
        if partitions is None or partition_of(n) in partitions:
            names.append(n)
    return names


def in_spans(t, spans, margin=0.3):
    return any(a - margin <= t <= b + margin for a, b, _ in spans)


def score(sess, ev, anchor_tol=0.06):
    """Classify recognizer events against the route reference.

    Inside anchored spans: 'magnet' (right place, right polarity),
    'wrong_polarity', 'duplicate' (second event on one magnet) or 'false'.
    Outside spans: 'unscored'. Every reference magnet without an event is a
    miss. Matching is one-to-one, strongest |r| first."""
    refs, spans = sess['refs'], sess['spans']
    rt = np.array([r['t'] for r in refs]) if refs else np.zeros(0)
    taken = {}
    for e in ev:
        e['class'], e['ref'], e['dt'] = 'unscored', None, None
    for e in sorted(ev, key=lambda e: -abs(e['r'])):
        if not in_spans(e['t_peak'], spans):
            continue
        e['class'] = 'false'
        if not len(rt):
            continue
        order = np.argsort(np.abs(rt - e['t_peak']))[:3]
        for j in order:
            r = refs[j]
            tol = (max(anchor_tol, 0.25 * e['scale']) if r['kind'] == 'anchor'
                   else 0.3 * r['interval'])
            if abs(rt[j] - e['t_peak']) > tol:
                continue
            if j in taken:
                e['class'] = 'duplicate'
            else:
                taken[j] = e
                # Position decides recognition; a polarity that disagrees with
                # RouteMap.h is kept as its own class (field evidence, e.g.
                # MM066/067/069 on Oct 26), not counted as a miss.
                e['class'] = 'magnet' if e['pol'] == r['pol'] else 'magnet_map_polarity_differs'
            e['ref'], e['dt'] = int(j), float(e['t_peak'] - rt[j])
            break
    missed = [j for j in range(len(refs)) if j not in taken]
    c = {k: sum(e['class'] == k for e in ev) for k in
         ('magnet', 'magnet_map_polarity_differs', 'duplicate', 'false', 'unscored')}
    c['recognized'] = c['magnet'] + c['magnet_map_polarity_differs']
    c.update(ref_magnets=len(refs), ref_expected=sum(r['kind'] == 'expected' for r in refs),
             missed=len(missed), missed_expected=sum(refs[j]['kind'] == 'expected' for j in missed))
    return c, missed
