#!/usr/bin/env python3
"""Investigation of the Oct 25-26 held-out reference magnets that no tested width bank recognizes.
Research only; no firmware or specification change.

Configurations (as in field_20261027_evening/bank_thresholds_corpus.py): 240; 240+600+1500 with the
wide copies at |r| >= 0.9; 150+240+600+1500 at 0.7 and at 0.9. A reference magnet is investigated
when it is missed by all four.

For each: session, loco, partition, reference kind (anchor = a clean oracle excursion placed on the
route; expected = interpolated between anchor chains, no clean excursion was found there), MM,
polarity, time, applied PWM, phase, direction; the oracle excursion nearest the reference time (any
sign, within 0.6 s); best candidate per width (150/240/600/1500) at the research floors; every
recognizer event of the widest configuration within 1 s and how the scorer classified it; and:

  mechanism  no_signal    no oracle excursion >= 60 counts of the reference polarity within the
                          tolerance window (nothing magnet-like to recognize)
             scored_elsewhere  a recognizer event of the right polarity lies within 0.3 s but the
                          scorer gave it to another reference or called it false/duplicate (timing)
             below_threshold   a candidate exists at the passage but no width reaches |r| 0.7 and
                          |A| 70
             capture_gap  no width has a score at the passage and the recording lost reads within
                          0.5 s of it (the window holds missing data)
             suppressed   a candidate passing its width's thresholds exists within 0.3 s but the
                          combining rule did not declare it
  70x2       logged       the loco's own diag/navi_encounter rows (the Oct 25-26 builds' 70x2 path)
                          with opening time within [-0.6, +0.15] s of the reference
             replay       NAVI_WAVEFORM_0_1's diagnostic 70x2 (firmware-exact port) on the capture:
                          same-polarity onset within [-0.6, +0.05] s of the reference

Usage: NGR_NSR_NPZ=<npz dir> NGR_MF_CACHE=<cache> python3 investigate_misses.py <Pi_Log_Downloads> out.json
"""
import collections
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parent / 'diag70x2_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import corpus             # noqa: E402
import mf                 # noqa: E402
import replay_70x2 as r7  # noqa: E402
import replay_bank as rb  # noqa: E402

DT = mf.DT_S
WIDTHS = (0.15, 0.24, 0.60, 1.50)
CONFIGS = [((0.24,), 0.7), ((0.24, 0.60, 1.50), 0.9), ((0.15, 0.24, 0.60, 1.50), 0.7), ((0.15, 0.24, 0.60, 1.50), 0.9)]


def thr(sc, wide):
    return {D: ((0.7, 70.0) if D == 0.24 else (wide, 70.0)) for D in sc}


def logged_encounters(logdir):
    out = collections.defaultdict(list)
    for f in sorted(Path(logdir).glob('99500*_2026102[56]_*.log')):
        for line in open(f):
            if 'diag/navi_encounter' not in line:
                continue
            try:
                e = json.loads(line.rstrip('\n').split('\t', 2)[2])
            except (ValueError, IndexError):
                continue
            if 'opening_us' in e:
                out[e['boot']].append(e)
    return out


def phase_at(tr, t):
    k = min(int(t / DT), len(tr.pwm) - 1)
    b = tr.pwm[max(k - int(1 / DT), 0)]
    a = tr.pwm[min(k + int(1 / DT), len(tr.pwm) - 1)]
    return 'accelerating' if a > b + 3 else 'decelerating' if a < b - 3 else 'steady'


def main():
    logdir, outp = sys.argv[1], sys.argv[2]
    enc = logged_encounters(logdir)
    held = [n for n in corpus.sessions(['validation', 'transfer']) if '_20261027_' not in n]
    rows = []
    for n in held:
        s = corpus.load(n)
        tr = s['trace']
        cc = rb.candidates(tr, WIDTHS, 0.5)
        missed_sets, evs = [], []
        for sc, wide in CONFIGS:
            ev = rb.combine(cc, sc, thr(sc, wide))
            _, missed = corpus.score(s, ev)
            missed_sets.append(set(missed))
            evs.append(ev)
        common = sorted(set.intersection(*missed_sets))
        if not common:
            continue
        boot = n.split('_')[3]   # navi_session_<loco>_<boot>_...
        native = None
        ev_w = evs[2]   # widest config at 0.7, classes from its scoring
        corpus.score(s, ev_w)
        for j in common:
            r = s['refs'][j]
            t = r['t']
            k = min(int(t / DT), len(tr.pwm) - 1)
            tol = 0.3 * r.get('interval', 1.0)
            exc = [x for x in s['exc'] if abs(x['t'] - t) <= 0.6]
            ex_same = [x for x in exc if x['pol'] == r['pol'] and abs(x['t'] - t) <= max(tol, 0.15)]
            best = {}
            for D in WIDTHS:
                c = [q for q in cc if q['D'] == D and abs(q['t_peak'] - t) <= max(tol, 0.5 * D + 0.1)]
                b = max(c, key=lambda q: q['absr']) if c else None
                best[str(int(D * 1000))] = None if b is None else dict(t=round(b['t_peak'], 3), r=round(b['r'], 2), A=round(b['A'], 1))
            near_ev = [dict(t=round(e['t_peak'], 3), pol=e['pol'], width=int(e['scale'] * 1000), r=round(e['r'], 2), A=round(e['A'], 1),
                            cls=e['class'], given_to=None if e['ref'] is None else dict(mm=s['refs'][e['ref']]['mm'], kind=s['refs'][e['ref']]['kind'],
                                                                                         t=round(s['refs'][e['ref']]['t'], 3)))
                       for e in ev_w if abs(e['t_peak'] - t) <= 1.0]
            passing = [q for q in cc if abs(q['t_peak'] - t) <= 0.3 and np.sign(q['A']) == r['pol']
                       and q['absr'] >= 0.7 and abs(q['A']) >= 70]
            if not ex_same:
                mech = 'no_signal'
            elif any(e['pol'] == r['pol'] and abs(e['t'] - t) <= 0.3 for e in near_ev):
                mech = 'scored_elsewhere'
            elif passing:
                mech = 'suppressed'
            else:
                mech = 'below_threshold'
            # 70x2 logged by the loco (native already loaded above)
            t0 = tr.t0_us
            lg = [dict(dt=round((e['opening_us'] - t0) / 1e6 - t, 3), result=e['result'], pol=e['polarity'], mm=e.get('mm'),
                       expected=e.get('expected_mm'), pwm=e.get('opening_pwm'), peak=e.get('peak_adc'))
                  for e in enc.get(boot, []) if -0.6 <= (e['opening_us'] - t0) / 1e6 - t <= 0.15]
            if native is None:
                native = np.load(corpus.NSR_DIR / (n + '.npz'))['native']
                ons = r7.detect70x2(native)
            rp = [dict(dt=round(o[2] - t, 3), pol=o[3], pwm=o[5]) for o in ons if -0.6 <= o[2] - t <= 0.05]
            if native is None:
                native = np.load(corpus.NSR_DIR / (n + '.npz'))['native']
                ons = r7.detect70x2(native)
            w = (tr.t >= t - 0.8) & (tr.t <= t + 0.8)
            nan_ms = int(np.isnan(tr.x[w]).sum() * DT * 1000)
            ns = native['serial'].astype(np.int64); nt = (native['t_us'].astype(np.int64) - int(native['t_us'][0])) / 1e6
            gi = np.flatnonzero(np.diff(ns) != 1)
            gaps = [dict(dt=round(float(nt[i]) - t, 3), missing_reads=int(ns[i + 1] - ns[i] - 1)) for i in gi if abs(nt[i] - t) <= 1.5]
            # capture_gap: reads missing from the recording inside the passage's template windows leave
            # every width without a finite score there (mf.matched_filter returns NaN for such windows)
            if all(v is None for v in best.values()) and any(abs(g['dt']) <= 0.5 for g in gaps):
                mech = 'capture_gap'
            rows.append(dict(session=n, missing_conditioned_ms_within_0_8s=nan_ms, serial_gaps_within_1_5s=gaps, loco='Otto' if '_9950011_' in n else 'Toby', partition=s['partition'],
                             mm=r['mm'], pol=r['pol'], kind=r['kind'], t=round(t, 3), interval=round(r.get('interval', 0), 3),
                             pwm=int(tr.pwm[k]), phase=phase_at(tr, t), direction=int(tr.direction[k]),
                             excursions=[dict(dt=round(x['t'] - t, 3), pol=x['pol'], amp=round(x['amp'], 1),
                                              width_ms=round(1e3 * (x['end'] - x['start'])), clean=x['clean'],
                                              level_step=round(x['level_step'], 1), mm=x.get('mm')) for x in exc],
                             best_by_width=best, events_within_1s=near_ev, mechanism=mech,
                             x70_logged=lg, x70_logged_session=len(enc.get(boot, [])) > 0,
                             x70_replay_same_pol=[o for o in rp if o['pol'] == r['pol']], x70_replay_any=rp))
            print(json.dumps(rows[-1]))
    Path(outp).write_text(json.dumps(rows, indent=1))
    print('total', len(rows), collections.Counter(r['mechanism'] for r in rows), collections.Counter(r['kind'] for r in rows))


if __name__ == '__main__':
    main()
