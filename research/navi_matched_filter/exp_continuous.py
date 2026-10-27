#!/usr/bin/env python3
"""Continuous replay: final scores, ledger, latency (E), scale (F), Level 4, §10.

Every result here uses the configuration frozen from dev (config.FROZEN)
unless a row names another one for comparison.
"""
import collections
import csv
import json
from pathlib import Path

import numpy as np

import corpus
import mf
import truth
from config import FROZEN, LINE_ALT, MUSE_LIKE

OUT = Path(__file__).resolve().parent / 'results'
DT = mf.DT_S
PARTS = ('level1', 'dev', 'validation', 'transfer')


# ---------------------------------------------------------------- scale (F)
def interval_scaled(trace, stack, cfg, k_ratio, fallback='bank'):
    """Pick, at each time, the bank scale nearest k_ratio x last interval.

    The 'last interval' is between the two most recent events this procedure
    itself accepted (causal in spirit; no reference used). Before two events
    exist the full bank is used (fallback)."""
    R, A, po = stack
    scales = np.array(cfg.scales)
    ev_all = mf.events_from_stack(trace, R, A, po, cfg)       # candidates at all scales
    out = []
    for e in ev_all:
        if len(out) >= 2:
            want = k_ratio * (out[-1]['t_peak'] - out[-2]['t_peak'])
            s = int(np.argmin(np.abs(np.log(scales / max(want, 1e-3)))))
            # re-evaluate this event at the predicted scale only
            i = e['i']
            w = slice(max(0, i - int(.5 * scales[s] / DT)), i + int(.5 * scales[s] / DT) + 1)
            r = np.abs(np.nan_to_num(R[s, w]))
            if not len(r) or r.max() < cfg.r_min:
                continue
            j = w.start + int(np.argmax(r))
            if not np.isfinite(A[s, j]) or abs(A[s, j]) < cfg.a_min:
                continue
            ipk = int(np.clip(j + po[s], 0, len(trace.t) - 1))
            e = dict(e, i=j, t=float(trace.t[j]), t_peak=float(trace.t[ipk]), scale=float(scales[s]),
                     r=float(R[s, j]), A=float(A[s, j]), pol=int(np.sign(A[s, j])))
        if out and abs(e['t_peak'] - out[-1]['t_peak']) < 0.5 * e['scale']:
            continue
        out.append(e)
    return out


# ------------------------------------------------------------ causal (E)
def causal_events(trace, stack, cfg):
    """Greedy causal emulation of the offline selection.

    A per-scale local maximum (i, s) becomes knowable at
        ready = t_i + nms*D_s + L_s/2
    (its own window and its own NMS neighbourhood complete). Candidates are
    processed in ready order; one is declared unless an already-declared event
    is within NMS distance, or a stronger candidate that is already knowable
    lies within its NMS radius. Declarations are final."""
    R, A, po = stack
    cand = []
    for s, D in enumerate(cfg.scales):
        absr = np.where(np.isfinite(R[s]), np.abs(R[s]), -1)
        ok = (absr >= cfg.r_min) & np.isfinite(A[s]) & (np.abs(A[s]) >= cfg.a_min)
        lm = np.flatnonzero(ok & (absr >= np.roll(absr, 1)) & (absr > np.roll(absr, -1)))
        rad = cfg.nms * D
        L = len(mf.sampled_template(mf.template_mean(), D, cfg.pad)[0]) * DT
        for i in lm:
            cand.append((trace.t[i] + rad + L / 2, int(i), s, float(absr[i])))
    cand.sort()
    declared = []
    known = []
    for ready, i, s, ar in cand:
        known.append((i, s, ar, ready))
        rad = int(cfg.nms * cfg.scales[s] / DT)
        if any(abs(i - d['i']) <= max(rad, int(cfg.nms * d['scale'] / DT)) for d in declared):
            continue
        if any(abs(i - j) <= rad and a2 > ar and r2 <= ready for j, s2, a2, r2 in known):
            continue
        ipk = int(np.clip(i + po[s], 0, len(trace.t) - 1))
        declared.append(dict(i=i, t=float(trace.t[i]), t_peak=float(trace.t[ipk]),
                             scale=float(cfg.scales[s]), r=float(R[s, i]), A=float(A[s, i]),
                             pol=int(np.sign(A[s, i])), pwm=float(trace.pwm[i]),
                             direction=int(trace.direction[i]), t_decl=float(ready)))
    return sorted(declared, key=lambda e: e['t'])


def prefix_events(trace, cfg, q):
    """Early recognition: template truncated at its peak + q*D (causal window).

    Score is computed on the window [core start - pad*D, peak + q*D] only; the
    decision is available when that window ends, plus nothing else."""
    shape = mf.template_mean()
    R, A, PO, LAT = [], [], [], []
    for D in cfg.scales:
        T, ipk = mf.sampled_template(shape, D, cfg.pad)
        cut = min(len(T), ipk + int(q * D / DT) + 1)
        Tp = T[:cut]
        if len(Tp) % 2 == 0:
            Tp = Tp[1:]
            ipk -= 1
        r, a = mf.matched_filter(trace.x, Tp, cfg.detrend)
        # amplitude from a prefix fit is in template units already
        R.append(r)
        A.append(a)
        PO.append(ipk - len(Tp) // 2)
        LAT.append((len(Tp) - 1 - ipk) * DT)
    ev = mf.events_from_stack(trace, np.array(R), np.array(A), np.array(PO), cfg)
    lat = dict(zip(cfg.scales, LAT))
    for e in ev:
        e['t_decl'] = e['t_peak'] + lat[e['scale']] + cfg.nms * e['scale']
    return ev


# ----------------------------------------------------- geography (§10)
def geographic(sess, ev, route, mode, gate=None):
    """Replay one expected-next-magnet tracker over recognized events.

    Starts at the first anchor of each scored span with its MM known (an
    initial declaration). Modes:
      A  recognition only (every event counts; no position kept)
      B  + polarity/sequence: accept iff polarity == route polarity of next MM
      C  + B + reachability: also require dt >= 0.5 x last confirmed interval
      G  + B + strict lower gate (gate(prev_interval) seconds)"""
    stats = collections.Counter()
    refs = sess['refs']
    for (a, b, dirn) in sess['spans']:
        first = [r for r in refs if r['kind'] == 'anchor' and abs(r['t'] - a) < 1e-6]
        if not first:
            continue
        pos, t_last, last_int = first[0]['mm'], a, None
        for e in ev:
            if not (a < e['t_peak'] <= b + 0.3):
                continue
            genuine = e['class'].startswith('magnet')
            stats['genuine_detected' if genuine else 'spurious_seen'] += 1
            if mode == 'A':
                continue
            nxt = (pos + dirn) % len(route)
            ok = e['pol'] == route[nxt]
            dt = e['t_peak'] - t_last
            if ok and mode == 'C' and last_int is not None and dt < 0.5 * last_int:
                ok = False
            if ok and mode == 'G' and last_int is not None and dt < gate(last_int):
                ok = False
            if not ok:
                stats['genuine_suppressed' if genuine else 'spurious_rejected'] += 1
                continue
            truth_mm = refs[e['ref']]['mm'] if genuine and e['ref'] is not None else None
            if truth_mm == nxt:
                stats['confirmations'] += 1
            else:
                stats['incorrect_advancements'] += 1
            pos, last_int, t_last = nxt, dt, e['t_peak']
    return stats


# ------------------------------------------------------------- level 4
def level4(sess):
    """Inter-magnet return-to-level and close-pair statistics (no recognizer)."""
    tr, refs = sess['trace'], sess['refs']
    anchors = [r for r in refs if r['kind'] == 'anchor']
    out = []
    for r0, r1 in zip(anchors[:-1], anchors[1:]):
        gap = r1['t'] - r0['t']
        if gap > 5 or gap <= 0:
            continue
        e0 = sess['exc'][r0['k']]
        e1 = sess['exc'][r1['k']]
        pre = tr.x[int((e0['start'] - .35) / DT):int((e0['start'] - .05) / DT)]
        mid = tr.x[int((e0['end'] + .05) / DT):int((e1['start'] - .05) / DT)]
        pre, mid = pre[np.isfinite(pre)], mid[np.isfinite(mid)]
        if len(pre) < 20 or len(mid) < 5:
            out.append(dict(gap=gap, mid_len=len(mid) * DT, dev=np.nan, overlap=len(mid) < 5))
            continue
        out.append(dict(gap=gap, mid_len=len(mid) * DT,
                        dev=float(np.max(np.abs(mid - np.median(pre)))), overlap=False,
                        t=r0['t'], mm0=r0['mm']))
    return out


def main():
    OUT.mkdir(exist_ok=True)
    route = truth.route_polarity()
    k_ratio = None
    results = dict(configs={}, latency={}, prefix={}, scale={}, geo={}, level4={}, centres={})
    ledger = []
    cfgs = {'FROZEN': FROZEN, 'MUSE_LIKE': MUSE_LIKE, 'LINE_ALT': LINE_ALT,
            'single_240ms': mf.Config(scales=(0.24,), detrend='mean', pad=1.0, r_min=.7, a_min=70),
            'bank3': mf.Config(scales=(0.24, 0.6, 1.5), detrend='mean', pad=1.0, r_min=.7, a_min=70)}
    # k_ratio for interval scaling: dev only — best scale / preceding interval
    rat = []
    for n in corpus.sessions(['dev']):
        s = corpus.load(n)
        ev, _ = mf.recognize(s['trace'], FROZEN)
        corpus.score(s, ev)
        g = [e for e in ev if e['class'].startswith('magnet')]
        rat += [b['scale'] / (b['t_peak'] - a['t_peak']) for a, b in zip(g[:-1], g[1:])
                if 0.3 < b['t_peak'] - a['t_peak'] < 6]
    k_ratio = float(np.median(rat))
    results['k_ratio_dev'] = k_ratio

    tot = collections.defaultdict(collections.Counter)
    lat = collections.defaultdict(list)
    pre = collections.defaultdict(collections.Counter)
    prelat = collections.defaultdict(list)
    geo = collections.defaultdict(collections.Counter)
    l4 = collections.defaultdict(list)
    centre_rows = []
    for n in corpus.sessions(PARTS):
        s = corpus.load(n)
        part = s['partition']
        tr = s['trace']
        for cname, cfg in cfgs.items():
            stack = mf.scale_stack(tr, cfg)
            ev = mf.events_from_stack(tr, *stack, cfg)
            c, missed = corpus.score(s, ev)
            tot[(cname, part)].update(c)
            if cname == 'FROZEN':
                for e in ev:
                    ledger.append(dict(session=n, partition=part, t_peak_s=round(e['t_peak'], 4),
                                       scale_s=e['scale'], score_r=round(e['r'], 4),
                                       amplitude_adc=round(e['A'], 1), polarity='N' if e['pol'] > 0 else 'S',
                                       pwm=e['pwm'], direction=e['direction'], classification=e['class'],
                                       ref_mm=(s['refs'][e['ref']]['mm'] if e['ref'] is not None else ''),
                                       ref_kind=(s['refs'][e['ref']]['kind'] if e['ref'] is not None else ''),
                                       timing_error_ms=(round(1e3 * e['dt'], 1) if e['dt'] is not None else '')))
                for j in missed:
                    r = s['refs'][j]
                    ledger.append(dict(session=n, partition=part, t_peak_s=round(r['t'], 4),
                                       classification='MISSED', ref_mm=r['mm'], ref_kind=r['kind'],
                                       polarity='N' if r['pol'] > 0 else 'S'))
                for e in ev:
                    if e['class'].startswith('magnet') and s['refs'][e['ref']]['kind'] == 'anchor':
                        lat[(part, 'timing_err_ms')].append(1e3 * e['dt'])
                # causal emulation
                cev = causal_events(tr, stack, FROZEN)
                cc, _ = corpus.score(s, cev)
                tot[('FROZEN_causal', part)].update(cc)
                for e in cev:
                    if e['class'].startswith('magnet'):
                        lat[(part, 'causal_latency_ms')].append(1e3 * (e['t_decl'] - e['t_peak']))
                # interval-scaled
                iev = interval_scaled(tr, stack, FROZEN, k_ratio)
                ic, _ = corpus.score(s, iev)
                tot[('interval_scaled', part)].update(ic)
                # geography
                for mode, gate in (('A', None), ('B', None), ('C', None),
                                   ('G_fixed720', lambda li: 0.72),
                                   ('G_072prev', lambda li: 0.72 * li)):
                    geo[(mode, part)].update(geographic(s, ev, route, mode[0], gate))
                # centres: per (direction, mm) intervals at steady PWM for reproducibility
                g = [e for e in ev if e['class'].startswith('magnet') and 85 <= e['pwm'] <= 95]
                for a, b in zip(g[:-1], g[1:]):
                    ra, rb = s['refs'][a['ref']], s['refs'][b['ref']]
                    ea = s['exc'][ra['k']] if ra['kind'] == 'anchor' else None
                    eb = s['exc'][rb['k']] if rb['kind'] == 'anchor' else None
                    if ea is None or eb is None or not (0.5 < b['t_peak'] - a['t_peak'] < 3):
                        continue
                    centre_rows.append(dict(loco=n[13:20], dir=a['direction'], mm=(ra['mm'], rb['mm']),
                                            template=b['t_peak'] - a['t_peak'],
                                            extremum=eb['t'] - ea['t'],
                                            onset=eb['start'] - ea['start']))
                if part in ('dev', 'validation', 'transfer', 'level1'):
                    l4[part] += level4(s)
        if part in ('validation', 'transfer'):
            for q in (0.0, 0.15, 0.3):
                pev = prefix_events(tr, FROZEN, q)
                pc, _ = corpus.score(s, pev)
                pre[(q, part)].update(pc)
                prelat[(q, part)] += [1e3 * (e['t_decl'] - e['t_peak']) for e in pev
                                      if e['class'].startswith('magnet')]
        print(n, flush=True)

    def q(v):
        v = np.asarray(v, float)
        return dict(n=int(len(v)), median=float(np.median(v)), p95=float(np.quantile(v, .95)),
                    worst=float(np.max(np.abs(v)) if len(v) else np.nan)) if len(v) else {}
    results['configs'] = {'%s|%s' % k: dict(v) for k, v in tot.items()}
    results['latency'] = {'%s|%s' % k: q(v) for k, v in lat.items()}
    results['prefix'] = {'q=%.2f|%s' % k: dict(v) for k, v in pre.items()}
    results['prefix_latency'] = {'q=%.2f|%s' % k: q(v) for k, v in prelat.items()}
    results['geo'] = {'%s|%s' % k: dict(v) for k, v in geo.items()}
    # centre reproducibility: same (loco, dir, mm pair) across laps
    grp = collections.defaultdict(list)
    for r in centre_rows:
        grp[(r['loco'], r['dir'], r['mm'])].append(r)
    rep = {}
    for key in ('template', 'extremum', 'onset'):
        cv = [np.std([r[key] for r in v]) / np.mean([r[key] for r in v])
              for v in grp.values() if len(v) >= 4]
        sd = [1e3 * np.std([r[key] for r in v]) for v in grp.values() if len(v) >= 4]
        rep[key] = dict(groups=len(cv), median_cv=float(np.median(cv)), median_sd_ms=float(np.median(sd)))
    results['centres'] = rep
    for part, rows in l4.items():
        dev = np.array([r['dev'] for r in rows if np.isfinite(r['dev'])])
        results['level4'][part] = dict(
            pairs=len(rows), overlapping=int(sum(r['overlap'] for r in rows)),
            gap_lt_500ms=int(sum(r['gap'] < 0.5 for r in rows)),
            not_within_20=int(np.sum(dev > 20)), not_within_50=int(np.sum(dev > 50)),
            not_within_100=int(np.sum(dev > 100)), median_dev=float(np.median(dev)))
    (OUT / 'continuous.json').write_text(json.dumps(results, indent=1, default=float))
    keys = ['session', 'partition', 't_peak_s', 'scale_s', 'score_r', 'amplitude_adc', 'polarity',
            'pwm', 'direction', 'classification', 'ref_mm', 'ref_kind', 'timing_error_ms']
    with open(OUT / 'event_ledger.csv', 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        for row in ledger:
            w.writerow(row)
    print(json.dumps(results, indent=1, default=float))


if __name__ == '__main__':
    main()
