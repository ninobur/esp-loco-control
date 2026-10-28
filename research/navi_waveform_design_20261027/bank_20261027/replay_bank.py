#!/usr/bin/env python3
"""Replay of a time-scaled matched-filter bank (one waveform, several widths). Research only.

David's decision 2026-10-27: replay first. Question: can one magnetic waveform, stretched in
time, recognise every genuine magnet passage in the Oct 27 recordings without adding false
observations?

Bank: the existing mean template (template_mean), sampled at widths 240 / 600 / 1500 ms,
level-only normalisation, pad 0.5 (as fielded in NAVI_WAVEFORM_0_1). Configurations:
240 only, 240+600, 240+600+1500. Width is chosen by the Hall signal only.

COMBINING RULE (stated before running). Per width, every local maximum of |r| that passes that
width's thresholds is a candidate; it becomes knowable at ready = t + nms*D + L/2 (its own
window and its own suppression neighbourhood complete; nms = 0.5, L = window length).
Candidates from all widths are taken in ready order. A candidate is declared unless
(a) an already-declared event lies within max(its radius, the declared event's radius), or
(b) a stronger candidate (larger |r|, any width) that is already knowable lies within its radius.
Declarations are final: one observation, passage time = that width's template-peak time.
With uniform thresholds this is exactly exp_continuous.causal_events (checked in main()).

Truth.
  Oct 25-26: the research reference (corpus.score), held-out partitions, unchanged.
  Oct 27 driven runs: the single-240 events (identical to the on-loco firmware events, 410/410 on
  the two Otto CCW captures) plus the template-independent oracle excursions not near any of
  them. In the six driven runs those extra excursions are exactly the three known misses; the
  counts equal the operator's start-to-end magnet counts. Restart b (3D46) is not scored for
  recall (E-stop, direction change, no reliable truth); its events are listed.
  Stationary: every event while applied PWM is 0 is reported as a stationary observation.

Usage: NGR_NSR_NPZ=<oct25-26 npz> NGR_MF_CACHE=<cache> python3 replay_bank.py <oct27 npz dir> [out.json]
"""
import collections
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import corpus              # noqa: E402
import exp_continuous as ec  # noqa: E402
import mf                  # noqa: E402
import truth               # noqa: E402

DT = mf.DT_S
NMS = 0.5
SHAPE = mf.template_mean()
ALL6 = (0.15, 0.24, 0.38, 0.60, 0.95, 1.50)
PRIMARY = {'240': (0.24,), '240+600': (0.24, 0.60), '240+600+1500': (0.24, 0.60, 1.50)}
REPRO_P10 = {'bank2_p1.0': (0.24, 0.60), 'bank3_p1.0': (0.24, 0.60, 1.50), 'bank6_p1.0': ALL6}
LOW_R, LOW_A = 0.5, 20.0      # candidates are stored at these floors; configs filter upward
KNOWN_MISSES = {'563C8E21CDA1CABF': [(273.44, 'MM037')],
                'CA662AD9DC993994': [(70.31, 'MM169'), (406.88, 'MM113')]}
DRIVEN27 = ('CA662AD9DC993994', '563C8E21CDA1CABF', '7247956AF788A3E4', 'EDFAFAEA80CF3499',
            '51104A27DEA43752', 'CC9DF08D5F1552AA')
LABEL27 = {'CA662AD9DC993994': 'Otto run 1 CCW 004-005', '563C8E21CDA1CABF': 'Otto CCW 044-045',
           '7247956AF788A3E4': 'Otto CW 040-041', 'EDFAFAEA80CF3499': 'Toby CCW 035-036',
           '51104A27DEA43752': 'Toby CW 024-025', 'CC9DF08D5F1552AA': 'Otto control (Muse) MM121-039',
           '3D46D33098A9004A': 'Otto restart b (E-stop)', 'AFADA3EFA01C91F1': 'Otto parked (old build)',
           '90C059995820F1AB': 'Otto parked', 'B320624109FB8862': 'Toby parked (old build)',
           'D248E206532D8074': 'Toby parked'}


def window_len(D, pad):
    return len(mf.sampled_template(SHAPE, D, pad)[0]) * DT


def candidates(tr, scales, pad):
    """Per-width local maxima of |r| at the low floors, with everything combining needs."""
    out = []
    for D in scales:
        T, ipk = mf.sampled_template(SHAPE, D, pad)
        r, A = mf.matched_filter(tr.x, T, 'mean')
        absr = np.where(np.isfinite(r), np.abs(r), -1.0)
        ok = (absr >= LOW_R) & np.isfinite(A) & (np.abs(A) >= LOW_A)
        lm = np.flatnonzero(ok & (absr >= np.roll(absr, 1)) & (absr > np.roll(absr, -1)))
        po = ipk - len(T) // 2
        L = len(T) * DT
        for i in lm:
            ip = int(np.clip(i + po, 0, len(tr.t) - 1))
            out.append(dict(i=int(i), D=D, absr=float(absr[i]), r=float(r[i]), A=float(A[i]),
                            t=float(tr.t[i]), t_peak=float(tr.t[ip]),
                            ready=float(tr.t[i] + NMS * D + L / 2), pwm=float(tr.pwm[i]),
                            direction=int(tr.direction[i])))
    return out


def combine(cands, scales, thr):
    """Causal combining rule (module docstring). thr: {D: (r_min, a_min)}."""
    c = sorted((x for x in cands if x['D'] in scales and x['absr'] >= thr[x['D']][0]
                and abs(x['A']) >= thr[x['D']][1]), key=lambda x: x['ready'])
    declared, known = [], []
    for x in c:
        known.append(x)
        rad = int(NMS * x['D'] / DT)
        if any(abs(x['i'] - d['i']) <= max(rad, int(NMS * d['D'] / DT)) for d in declared):
            continue
        if any(abs(x['i'] - k['i']) <= rad and k['absr'] > x['absr'] and k['ready'] <= x['ready'] for k in known):
            continue
        e = dict(x)
        e.update(scale=x['D'], pol=int(np.sign(x['A'])), t_decl=x['ready'])
        declared.append(e)
    return sorted(declared, key=lambda e: e['t'])


def uniform(scales, r=0.7, a=70.0):
    return {D: (r, a) for D in scales}


def condition_of(pwm):
    return 'stationary' if pwm == 0 else ('slow' if pwm < 60 else 'cruise')


# ---------------------------------------------------------------- Oct 27 truth and scoring
def truth27(tr, boot):
    base = combine(candidates(tr, (0.24,), 0.5), (0.24,), uniform((0.24,)))
    t = [(e['t_peak'], e['pol']) for e in base if e['pwm'] > 0 or boot in DRIVEN27]
    exc, _ = truth.oracle(tr)
    et = np.array([e['t_peak'] for e in base])
    extra = [x for x in exc if not len(et) or np.min(np.abs(et - x['t'])) > 0.3]
    moving_extra = [x for x in extra if x['pwm'] > 0]
    t += [(x['t'], x['pol']) for x in moving_extra]
    known = sorted(m[0] for m in KNOWN_MISSES.get(boot, []))
    found = sorted(round(x['t'], 2) for x in moving_extra)
    if boot in DRIVEN27:
        assert np.allclose(found, known, atol=0.05), (boot, found, known)
    return sorted(t), extra


def score27(ev, tru):
    tt = np.array([x[0] for x in tru])
    taken, cls = {}, []
    for e in sorted(ev, key=lambda e: -e['absr']):
        tol = max(0.10, 0.4 * e['scale'])
        j = int(np.argmin(np.abs(tt - e['t_peak']))) if len(tt) else -1
        if j >= 0 and abs(tt[j] - e['t_peak']) <= tol:
            if j in taken:
                e['class'] = 'duplicate'
            else:
                taken[j] = e
                e['class'] = 'magnet' if e['pol'] == tru[j][1] else 'polarity_error'
            e['truth_t'] = float(tt[j])
        else:
            e['class'] = 'false'
    missed = [j for j in range(len(tru)) if j not in taken]
    return taken, missed


# ---------------------------------------------------------------- reporting helpers
def ev_row(e):
    return dict(t=round(e['t_peak'], 3), width_ms=int(e['scale'] * 1000), r=round(e['r'], 3),
                A=round(e['A'], 1), pwm=e['pwm'], condition=condition_of(e['pwm']),
                latency_ms=round(1e3 * (e['t_decl'] - e['t_peak'])))


def r_a_at(tr, t, pad=0.5, scales=(0.24, 0.60, 1.50), win=0.15):
    """Best |r| (and its A) of each width within +/- win of time t — diagnostic, no thresholds."""
    out = {}
    i0 = int(t / DT)
    for D in scales:
        T, ipk = mf.sampled_template(SHAPE, D, pad)
        po = ipk - len(T) // 2
        a, b = max(0, i0 - int((win + 3) / DT)), min(len(tr.x), i0 + int((win + 3) / DT))
        r, A = mf.matched_filter(tr.x[a:b], T, 'mean')
        tp = (a + np.arange(len(r)) + po + 0.5) * DT    # template-peak time of each window centre
        sel = np.flatnonzero((np.abs(tp - t) <= win) & np.isfinite(r))
        k = sel[np.argmax(np.abs(r[sel]))]
        out[int(D * 1000)] = (round(float(r[k]), 3), round(float(A[k]), 1))
    return out


def decimated_scores(tr, D, k, pad=0.5):
    """Same stretched template, run on the k-times block-averaged series (fewer taps)."""
    n = len(tr.x) // k
    xb = tr.x[:n * k].reshape(n, k)
    xd = np.where(np.isfinite(xb).all(axis=1), np.nanmean(xb, axis=1), np.nan)
    T, ipk = mf.sampled_template(SHAPE, D, pad, dt=DT * k)
    r, A = mf.matched_filter(xd, T, 'mean')
    return r, A, len(T), ipk - len(T) // 2


def main():
    out = {}
    oct27 = Path(sys.argv[1])
    res = collections.defaultdict(dict)

    # ---------- 0. parity of the combining rule with the research causal emulation (uniform thresholds)
    s0 = corpus.load(corpus.sessions(['validation'])[0])
    for name, sc in (('240', (0.24,)), ('240+600+1500', (0.24, 0.60, 1.50))):
        cfg = mf.Config(scales=sc, pad=0.5, detrend='mean', r_min=.7, a_min=70)
        a = ec.causal_events(s0['trace'], mf.scale_stack(s0['trace'], cfg), cfg)
        b = combine(candidates(s0['trace'], sc, 0.5), sc, uniform(sc))
        assert [round(e['t_peak'], 4) for e in a] == [round(e['t_peak'], 4) for e in b], name
    print('combining rule == exp_continuous.causal_events on', s0['name'][:40])

    # ---------- 1. Oct 25-26 held-out corpus
    held = [n for n in corpus.sessions(['validation', 'transfer']) if '_20261027_' not in n]
    allc = [n for n in corpus.sessions() if '_20261027_' not in n]
    if __import__('os').environ.get('ONLY27'):   # quick check of the Oct 27 part only
        held, allc = [], []
    grid_r, grid_a = (0.70, 0.75, 0.80, 0.85, 0.90), (70.0, 100.0, 130.0)
    agg = collections.defaultdict(collections.Counter)
    lat = collections.defaultdict(list)
    falses = collections.defaultdict(list)
    width_speed = []
    per_scale_dups = collections.Counter()
    for n in allc:
        s = corpus.load(n)
        tr = s['trace']
        is_held = n in held
        c05 = candidates(tr, ALL6, 0.5)
        sets = [(k, sc, 0.5, c05) for k, sc in PRIMARY.items()] + [('single240_p0.5', (0.24,), 0.5, c05)]
        if is_held:
            c10 = candidates(tr, ALL6, 1.0)
            sets += [(k, sc, 1.0, c10) for k, sc in REPRO_P10.items()]
        for k, sc, pad, cc in sets:
            ev = combine(cc, sc, uniform(sc))
            stat = [e for e in ev if e['pwm'] == 0]
            agg[(k, 'all')]['stationary_events'] += len(stat)
            agg[(k, 'all')]['stationary_s'] += float((tr.pwm == 0).sum() * DT)
            if not is_held:
                continue
            c, missed = corpus.score(s, ev)
            for q in ('ref_magnets', 'recognized', 'false', 'duplicate'):
                agg[(k, 'held')][q] += c[q]
            for e in ev:
                if e['class'] == 'false':
                    agg[(k, 'held')]['false_' + condition_of(e['pwm'])] += 1
                    falses[k].append(dict(session=n[13:43], **ev_row(e)))
                if e['class'].startswith('magnet'):
                    lat[(k, int(e['scale'] * 1000))].append(e['t_decl'] - e['t_peak'])
            for j in missed:
                agg[(k, 'held')]['miss_' + condition_of(tr.pwm[min(int(s['refs'][j]['t'] / DT), len(tr.pwm) - 1)])] += 1
            # declarations arriving after the next event's declaration
            tdec = [e['t_decl'] for e in ev]
            agg[(k, 'held')]['decl_out_of_order'] += sum(1 for a, b in zip(tdec[:-1], tdec[1:]) if a > b)
            if k in PRIMARY:
                # before combining: each width alone, matched to the reference
                obs = collections.Counter()
                for D in sc:
                    e1 = combine(cc, (D,), uniform((D,)))
                    corpus.score(s, e1)
                    for e in e1:
                        if e['class'].startswith('magnet') or e['class'] == 'duplicate':
                            obs[e['ref']] += 1
                per_scale_dups[k] += sum(v - 1 for v in obs.values() if v > 1)
            if k == 'single240_p0.5':
                continue
        # threshold sensitivity on the held-out corpus (wide widths only raised)
        if is_held:
            for k in ('240+600', '240+600+1500'):
                sc = PRIMARY[k]
                for rr in grid_r:
                    for aa in grid_a:
                        thr = {D: ((0.7, 70.0) if D == 0.24 else (rr, aa)) for D in sc}
                        ev = combine(c05, sc, thr)
                        c, _ = corpus.score(s, ev)
                        g = agg[('thr', k, rr, aa)]
                        g['recognized'] += c['recognized']
                        g['ref'] += c['ref_magnets']
                        g['false'] += c['false']
                        g['stationary_events'] += sum(e['pwm'] == 0 for e in ev)
            # fitted width vs speed, six-width bank (observation only)
            ev6 = combine(c05, ALL6, uniform(ALL6))
            corpus.score(s, ev6)
            SP = truth.route_spacing()
            g = sorted([e for e in ev6 if e['class'].startswith('magnet')], key=lambda e: e['t_peak'])
            for a, b in zip(g[:-1], g[1:]):
                ma, mb = s['refs'][a['ref']]['mm'], s['refs'][b['ref']]['mm']
                if ((mb - ma) % 171) == 1:
                    dist = SP[ma]
                elif ((ma - mb) % 171) == 1:
                    dist = SP[mb]
                else:
                    continue
                iv = b['t_peak'] - a['t_peak']
                if 0.2 < iv < 8:
                    width_speed.append((b['scale'], dist / iv, b['pwm']))
        print('done', n[13:43], flush=True)

    out['corpus'] = {('%s|%s' % k if len(k) == 2 else '|'.join(map(str, k))): dict(v) for k, v in agg.items()}
    out['corpus_false_events'] = falses
    out['corpus_latency_ms'] = {'%s|%d' % k: dict(n=len(v), median=round(1e3 * np.median(v)), p95=round(1e3 * np.percentile(v, 95)),
                                                  max=round(1e3 * max(v))) for k, v in lat.items()}
    out['corpus_before_combining_extra_observations'] = dict(per_scale_dups)
    ws = np.array(width_speed) if width_speed else np.zeros((0, 3))
    out['width_vs_speed_6bank'] = {int(D * 1000): dict(n=int((ws[:, 0] == D).sum()),
                                                       speed_mm_s_median=round(float(np.median(ws[ws[:, 0] == D, 1])), 0) if (ws[:, 0] == D).any() else None,
                                                       pwm_median=float(np.median(ws[ws[:, 0] == D, 2])) if (ws[:, 0] == D).any() else None)
                                   for D in ALL6}
    if len(ws) > 2:
        rk = lambda v: np.argsort(np.argsort(v))
        out['width_vs_speed_spearman'] = round(float(np.corrcoef(rk(ws[:, 0]), rk(1 / ws[:, 1]))[0, 1]), 3)

    # ---------- 2. Oct 27 recordings
    o27 = {}
    for p in sorted(oct27.glob('*.npz')):
        boot = p.stem.split('_')[3]
        tr = mf.condition(np.load(p)['native'])
        cc = candidates(tr, ALL6, 0.5)
        tru, extra = truth27(tr, boot)
        rec = dict(label=LABEL27.get(boot, boot), duration_s=round(float(tr.t[-1]), 1),
                   stationary_s=round(float((tr.pwm == 0).sum() * DT), 1),
                   truth_magnets=len(tru) if boot in DRIVEN27 else None, configs={})
        for k, sc in PRIMARY.items():
            ev = combine(cc, sc, uniform(sc))
            row = dict(events=len(ev), stationary_events=[ev_row(e) for e in ev if e['pwm'] == 0])
            if boot in DRIVEN27:
                taken, missed = score27(ev, tru)
                row.update(recognized=len(taken), missed=[round(tru[j][0], 2) for j in missed],
                           false=[ev_row(e) for e in ev if e['class'] == 'false'],
                           duplicates=sum(e['class'] == 'duplicate' for e in ev),
                           polarity_errors=sum(e['class'] == 'polarity_error' for e in ev),
                           winning_width={str(int(D * 1000)): sum(1 for e in ev if e['scale'] == D and e['class'] == 'magnet') for D in sc},
                           latency_ms={str(int(D * 1000)): sorted({round(1e3 * (e['t_decl'] - e['t_peak'])) for e in ev if e['scale'] == D}) for D in sc},
                           decl_out_of_order=sum(1 for a, b in zip([e['t_decl'] for e in ev][:-1], [e['t_decl'] for e in ev][1:]) if a > b))
                obs = collections.Counter()
                for D in sc:
                    e1 = combine(cc, (D,), uniform((D,)))
                    t1, _ = score27(e1, tru)
                    for e in e1:
                        if e['class'] in ('magnet', 'duplicate', 'polarity_error'):
                            obs[round(e['truth_t'], 3)] += 1
                row['before_combining_extra_observations'] = sum(v - 1 for v in obs.values() if v > 1)
                row['after_combining_duplicates'] = row['duplicates']
            else:
                row['all_events'] = [ev_row(e) for e in ev]
            rec['configs'][k] = row
        if boot in KNOWN_MISSES:
            rec['known_misses'] = {name: dict(t=t, r_A_by_width=r_a_at(tr, t))
                                   for t, name in KNOWN_MISSES[boot]}
            for name in rec['known_misses']:
                t = rec['known_misses'][name]['t']
                rec['known_misses'][name]['recognized_by'] = {
                    k: any(abs(e['t_peak'] - t) <= max(.1, .4 * e['scale']) for e in combine(cc, sc, uniform(sc)))
                    for k, sc in PRIMARY.items()}
                rec['known_misses'][name]['threshold_survival'] = {
                    '%.2f/%d' % (rr, aa): any(abs(e['t_peak'] - t) <= max(.1, .4 * e['scale'])
                                              for e in combine(cc, PRIMARY['240+600+1500'],
                                                               {D: ((0.7, 70.0) if D == 0.24 else (rr, aa)) for D in PRIMARY['240+600+1500']}))
                    for rr in grid_r for aa in grid_a}
        if boot in DRIVEN27:
            thr_rows = {}
            for rr in grid_r:
                for aa in grid_a:
                    sc = PRIMARY['240+600+1500']
                    ev = combine(cc, sc, {D: ((0.7, 70.0) if D == 0.24 else (rr, aa)) for D in sc})
                    taken, missed = score27(ev, tru)
                    thr_rows['%.2f/%d' % (rr, aa)] = dict(recognized=len(taken), false=sum(e['class'] == 'false' for e in ev),
                                                          stationary=sum(e['pwm'] == 0 for e in ev))
            rec['threshold_sensitivity_240+600+1500'] = thr_rows
            # decimation check on matched events of the 3-width bank
            ev = combine(cc, PRIMARY['240+600+1500'], uniform(PRIMARY['240+600+1500']))
            score27(ev, tru)
            dec = {}
            for D, kf in ((0.60, 3), (1.50, 6)):
                T, ipk = mf.sampled_template(SHAPE, D, 0.5)
                rf, Af = mf.matched_filter(tr.x, T, 'mean')
                rd, Ad, taps, pod = decimated_scores(tr, D, kf)
                po = ipk - len(T) // 2
                drs, das, flips = [], [], 0
                pts = [dict(i=int(round(e['t_peak'] / DT - 0.5)) - po) for e in ev] + [dict(i=int(t / DT) - po, A=0, r=0) for t, _ in KNOWN_MISSES.get(boot, [])]
                for e in pts:
                    i = e['i']
                    lo, hi = max(0, i - 3 * kf), min(len(rf), i + 3 * kf)
                    jf = lo + int(np.nanargmax(np.abs(rf[lo:hi])))
                    jd0 = jf // kf
                    jd = max(0, jd0 - 1) + int(np.nanargmax(np.abs(rd[max(0, jd0 - 1):jd0 + 2])))
                    drs.append(abs(rd[jd] - rf[jf]))
                    das.append(abs(Ad[jd] - Af[jf]))
                    pf = abs(rf[jf]) >= 0.7 and abs(Af[jf]) >= 70
                    pdd = abs(rd[jd]) >= 0.7 and abs(Ad[jd]) >= 70
                    flips += pf != pdd
                dec[str(int(D * 1000))] = dict(decimation=kf, taps=taps, points=len(pts),
                                               max_dr=round(float(max(drs)), 4) if drs else None,
                                               max_dA=round(float(max(das)), 2) if das else None,
                                               threshold_flips=flips)
            rec['decimation_check'] = dec
        o27[boot] = rec
        print('done', boot, flush=True)
    out['oct27'] = o27
    txt = json.dumps(out, indent=1, default=float)
    (Path(sys.argv[2]) if len(sys.argv) > 2 else HERE / 'results.json').write_text(txt)


if __name__ == '__main__':
    main()
