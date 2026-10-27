#!/usr/bin/env python3
"""Supporting replay analyses for NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md.

Reuses the research harness in ../navi_matched_filter unchanged. Sections:
  1. scale / window options: recall, false, causal latency, travel during latency
  2. firmware-equivalent conditioning (median of each 1 ms burst) parity
  3. recorded tick spacing (NB: includes NSR transport drops)
  4. matcher variants replayed over recognized events
Run with NGR_NSR_NPZ and NGR_MF_CACHE set as in ../navi_matched_filter/README.md.
"""
import collections
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'navi_matched_filter'))
import corpus          # noqa: E402
import exp_continuous as ec  # noqa: E402
import mf              # noqa: E402
import truth           # noqa: E402

ROUTE = truth.route_polarity()
SP = truth.route_spacing()
M = dict(detrend='mean', r_min=.7, a_min=70)
CFGS = {
    'single240_p1.0': mf.Config(scales=(0.24,), pad=1.0, **M),
    'single240_p0.5': mf.Config(scales=(0.24,), pad=0.5, **M),
    'single300_p0.5': mf.Config(scales=(0.30,), pad=0.5, **M),
    'bank2_p1.0': mf.Config(scales=(0.24, 0.6), pad=1.0, **M),
    'bank3_p1.0': mf.Config(scales=(0.24, 0.6, 1.5), pad=1.0, **M),
    'bank6_p1.0': mf.Config(pad=1.0, **M),
}
PROPOSED = CFGS['single240_p0.5']


def band(p):
    return 'pwm<60' if p < 60 else ('60-84' if p < 85 else ('85-95' if p <= 95 else '>95'))


def mapped_mm(a, dirn, k):
    return sum((SP[(a + dirn * q) % 171] if dirn > 0 else SP[(a + dirn * q - 1) % 171]) for q in range(k))


def section1():
    res = collections.defaultdict(collections.Counter)
    lat, latmm = collections.defaultdict(list), collections.defaultdict(list)
    for n in corpus.sessions(['dev', 'validation', 'transfer']):
        s = corpus.load(n)
        tr = s['trace']
        rpwm = [tr.pwm[min(int(r['t'] / 0.002), len(tr.pwm) - 1)] for r in s['refs']]
        for name, cfg in CFGS.items():
            st = mf.scale_stack(tr, cfg)
            for mode in ('offline', 'causal'):
                ev = mf.events_from_stack(tr, *st, cfg) if mode == 'offline' else ec.causal_events(tr, st, cfg)
                c, missed = corpus.score(s, ev)
                key = (name, mode, 'heldout' if s['partition'] != 'dev' else 'dev')
                for k2, v2 in (('ref', c['ref_magnets']), ('rec', c['recognized']),
                               ('false', c['false']), ('dup', c['duplicate'])):
                    res[key][k2] += v2
                for j in missed:
                    res[key]['miss_' + band(rpwm[j])] += 1
                for j in range(len(s['refs'])):
                    res[key]['ref_' + band(rpwm[j])] += 1
                if mode != 'causal':
                    continue
                g = sorted([e for e in ev if e['class'].startswith('magnet')], key=lambda e: e['t_peak'])
                for a, b in zip(g[:-1], g[1:]):
                    ma, mb = s['refs'][a['ref']]['mm'], s['refs'][b['ref']]['mm']
                    d = b['direction']
                    if (((mb - ma) if d == 1 else (ma - mb)) % 171) != 1:
                        continue
                    dist = SP[ma] if d == 1 else SP[mb]
                    iv = b['t_peak'] - a['t_peak']
                    if not (0.2 < iv < 6):
                        continue
                    L = b['t_decl'] - b['t_peak']
                    lat[(name, band(b['pwm']))].append(1e3 * L)
                    latmm[(name, band(b['pwm']))].append(L * dist / iv)
    for k in sorted(res):
        v = res[k]
        bands = ' '.join('%s:%d/%d' % (b, v['miss_' + b], v['ref_' + b]) for b in ('pwm<60', '60-84', '85-95', '>95'))
        print('%-15s %-7s %-7s rec %d/%d (%.2f%%) false %d dup %d | miss by pwm %s' % (
            k[0], k[1], k[2], v['rec'], v['ref'], 100 * v['rec'] / v['ref'], v['false'], v['dup'], bands))
    for k in sorted(lat):
        a, m = np.array(lat[k]), np.array(latmm[k])
        print('%-15s %-7s n=%5d latency ms med %.0f p95 %.0f max %.0f | travel mm med %.0f p95 %.0f max %.0f' % (
            k[0], k[1], len(a), np.median(a), np.percentile(a, 95), a.max(),
            np.median(m), np.percentile(m, 95), m.max()))


def tick_condition(nat):
    t = nat['t_us'].astype(np.int64)
    raw = nat['raw'].astype(float)
    new = np.concatenate([[True], np.diff(t) > 300])
    starts = np.flatnonzero(new)
    ends = np.concatenate([starts[1:], [len(t)]])
    med = np.array([np.median(raw[a:b]) for a, b in zip(starts, ends)])
    fake = np.zeros(len(med), dtype=nat.dtype)
    fake['t_us'] = t[starts]
    fake['raw'] = np.round(med).astype(np.int16)
    fake['pwm'] = nat['pwm'][starts]
    fake['direction'] = nat['direction'][starts]
    return mf.condition(fake, median_n=1), ends - starts, np.diff(t[starts])


def section2_3():
    tot, sz, gaps = collections.Counter(), collections.Counter(), []
    for n in corpus.sessions(['validation', 'transfer']):
        s = corpus.load(n)
        nat = np.load(corpus.NSR_DIR / (n + '.npz'))['native']
        tr2, sizes, dtt = tick_condition(nat)
        sz.update(np.minimum(sizes, 7).tolist())
        gaps.append(dtt)
        ev, _ = mf.recognize(tr2, PROPOSED)
        c, _ = corpus.score(s, ev)
        for k in ('ref_magnets', 'recognized', 'false', 'duplicate'):
            tot[k] += c[k]
    print('tick-median conditioning, held-out:', dict(tot))
    print('samples per recorded 1 ms burst (7 = 7+):', dict(sz))
    g = np.concatenate(gaps) / 1000.0
    print('recorded tick spacing ms: median %.3f p99.9 %.2f max %.1f  >2ms %d of %d' % (
        np.median(g), np.percentile(g, 99.9), g.max(), (g > 2).sum(), len(g)))


def section4():
    geo = collections.defaultdict(collections.Counter)
    for n in corpus.sessions(['dev', 'validation', 'transfer']):
        s = corpus.load(n)
        part = s['partition']
        ev, _ = mf.recognize(s['trace'], PROPOSED)
        corpus.score(s, ev)
        for (a, b, dirn) in s['spans']:
            first = [r for r in s['refs'] if r['kind'] == 'anchor' and abs(r['t'] - a) < 1e-6]
            if not first:
                continue
            for mode in ('P8_next_polarity_required', 'advance_every_credible',
                         'V1_time2_polarity_required', 'V2_time2_advance_on_location'):
                pos, t_last, rate = first[0]['mm'], a, None
                for e in ev:
                    if not (a < e['t_peak'] <= b + 0.3):
                        continue
                    gen = e['class'].startswith('magnet')
                    tm = s['refs'][e['ref']]['mm'] if gen and e['ref'] is not None else None
                    k = 1
                    if mode.startswith('V') and rate is not None:
                        dt = e['t_peak'] - t_last
                        pred = {q: rate * mapped_mm(pos, dirn, q) for q in (1, 2)}
                        k = min(pred, key=lambda q: abs(np.log(max(dt, 1e-3) / pred[q])))
                        if not (0.6 <= dt / pred[k] <= 1.6):
                            geo[(mode, part)]['unmatched_timing_' + ('genuine' if gen else 'spurious')] += 1
                            continue
                    tgt = (pos + dirn * k) % 171
                    agree = e['pol'] == ROUTE[tgt]
                    hold = (not agree) and (mode in ('P8_next_polarity_required', 'V1_time2_polarity_required')
                                            or (mode.startswith('V2') and rate is None))
                    if hold:
                        geo[(mode, part)]['held_polarity'] += 1
                        continue
                    geo[(mode, part)]['correct_adv' if tm == tgt else 'incorrect_adv'] += 1
                    if k == 2:
                        geo[(mode, part)]['skip_one'] += 1
                    if not agree:
                        geo[(mode, part)]['advanced_polarity_disagrees'] += 1
                    if mode.startswith('V'):
                        rate = (e['t_peak'] - t_last) / mapped_mm(pos, dirn, k)
                    pos, t_last = tgt, e['t_peak']
    for k in sorted(geo):
        print(k, dict(geo[k]))


if __name__ == '__main__':
    print('== 1. scale/window options'); section1()
    print('== 2/3. firmware-equivalent conditioning, tick spacing'); section2_3()
    print('== 4. matcher variants (single240_p0.5 events)'); section4()
