#!/usr/bin/env python3
"""Oct 27 evening Otto run (boot 044A3972942824CE): when does the single-240 waveform recognizer fail?
Research only; no firmware or matcher change.

Truth is built from two independent sources and must agree:
  MAP   Per declared segment (start_interval commands; no hand manipulation between them, David), the
        recognizer's polarity sequence is aligned to RouteMap.h in the declared direction by dynamic
        programming: an event either matches the next-or-later route magnet of the same polarity
        (each route magnet stepped over = one miss, cost 1) or is an extra (cost 1). Minimum cost.
  ORACLE  Template-independent Hall excursions (truth.oracle) with no recognizer event within 0.3 s.
A miss is accepted when the map places a skipped magnet between two matched events AND an oracle
excursion of the map's polarity lies between them in time. Each passage (recognized or missed) is then
described by applied PWM, phase (accelerating / steady / decelerating, from the PWM 1 s before and after),
whether it is the first magnet after a standstill, its excursion width, and r/A for the 240 template.

Usage: python3 when_it_fails.py <capture.npz> <capture.json> <RouteMap.h> [out.json]
"""
import json
import re
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf                 # noqa: E402
import replay_bank as rb  # noqa: E402
import truth              # noqa: E402

DT = mf.DT_S


def route(path):
    s = Path(path).read_text()
    return [1 if int(x) else -1 for x in re.search(r'ROUTE_POLARITY\[ROUTE_N\] = \{(.*?)\};', s, re.S).group(1).split(',') if x.strip()]


def align(items, R, start, d, max_skip=12):
    """DP over time-ordered items (pol, kind); kind 'ev' = recognizer event, 'ox' = oracle-only excursion.
    State = route offset of the last matched magnet (offset 0 = the first expected magnet).
    Costs: a route magnet stepped over with nothing matched to it = 1 (an unexplained gap);
    a recognizer event left unmatched = 1 (a false recognition); an oracle excursion left unmatched = 0
    (it may be a disturbance) and matched = 0 (it then marks a genuine magnet the recognizer missed).
    Ties prefer more matched items. Returns per-item offset or None, and the total cost."""
    N = len(R)
    INF = (10 ** 9, 0)
    cost = {-1: (0, 0)}
    back = []
    for p, kind in items:
        new, bp = {}, {}
        for o, (c, m) in cost.items():
            cu = (c + (1 if kind == 'ev' else 0), m)
            if cu < new.get(o, INF):
                new[o], bp[o] = cu, (o, None)
            for k in range(o + 1, o + 2 + max_skip):
                if R[(start + d * k) % N] == p:
                    cc = (c + (k - o - 1), m - 1)
                    if cc < new.get(k, INF):
                        new[k], bp[k] = cc, (o, k)
        cost = new
        back.append(bp)
    end = min(cost, key=lambda o: (cost[o], -o))
    out, o = [], end
    for bp in reversed(back):
        prev, m = bp[o]
        out.append(m)
        o = prev
    return list(reversed(out)), cost[end][0]


def describe(tr, t, i=None):
    k = min(int(t / DT), len(tr.pwm) - 1)
    pw = tr.pwm
    before = pw[max(k - int(1 / DT), 0)]
    after = pw[min(k + int(1 / DT), len(pw) - 1)]
    phase = 'accelerating' if after > before + 3 else 'decelerating' if after < before - 3 else 'steady'
    # standstill: when did the loco last have PWM 0 before this passage?
    z = np.flatnonzero(pw[:k] == 0)
    since_stop = (k - z[-1]) * DT if len(z) else None
    return dict(pwm=int(pw[k]), phase=phase, s_since_standstill=None if since_stop is None else round(since_stop, 1))


def main():
    npz, js, rm = sys.argv[1:4]
    nat = np.load(npz)['native']
    tr = mf.condition(nat)
    acts = json.loads(Path(js).read_text())['actions']
    t0 = int(nat['t_us'][0])
    R = route(rm)
    decl, sdir = [], 0
    for a in acts:
        if a['kind'] != 2 or not a['topic']:
            continue
        leaf = a['topic'].split('/')[-1]
        if leaf == 'session_direction':
            sdir = 1 if a['payload'] == 'CW' else -1
        elif leaf == 'start_interval':
            x, y = (int(v) for v in a['payload'].split('-'))
            decl.append(((a['t_us'] - t0) / 1e6, y if sdir < 0 else x, sdir, a['payload']))
    ev = rb.combine(rb.candidates(tr, (0.24, 0.60, 1.50), 0.5), (0.24,), rb.uniform((0.24,)))
    exc, _ = truth.oracle(tr)
    et = np.array([e['t_peak'] for e in ev])
    extra = [x for x in exc if np.min(np.abs(et - x['t'])) > 0.3]
    cands = rb.candidates(tr, (0.24, 0.60, 1.50), 0.5)
    passages, segs, used = [], [], set()
    for si, (td, last, d, payload) in enumerate(decl):
        tend = decl[si + 1][0] if si + 1 < len(decl) else tr.t[-1] + 1
        items = [('ev', e) for e in ev if td <= e['t_peak'] < tend]
        items += [('ox', x) for x in extra if td <= x['t'] < tend and x['pwm'] > 0]
        items.sort(key=lambda it: it[1]['t_peak'] if it[0] == 'ev' else it[1]['t'])
        start = (last + d) % len(R)                     # the first expected magnet
        offs, c = align([(it[1]['pol'], it[0]) for it in items], R, start, d)
        n_ev = sum(it[0] == 'ev' for it in items)
        seg = dict(declared=payload, t=round(td, 1), dir=d, events=n_ev, cost=c,
                   magnets_spanned=(max(o for o in offs if o is not None) + 1) if any(o is not None for o in offs) else 0,
                   unmatched_events=[], gaps=[])
        prev_o = -1
        for (kind, x), o in zip(items, offs):
            if o is None:
                if kind == 'ev':
                    seg['unmatched_events'].append(dict(t=round(x['t_peak'], 3), pol=x['pol'], r=round(x['r'], 2), A=round(x['A'], 1), pwm=int(x['pwm'])))
                continue
            for k in range(prev_o + 1, o):
                seg['gaps'].append(dict(mm=(start + d * k) % len(R), before_t=round(x['t_peak'] if kind == 'ev' else x['t'], 3)))
            prev_o = o
            mm = (start + d * o) % len(R)
            if kind == 'ev':
                passages.append(dict(seg=si, mm=mm, t=round(x['t_peak'], 3), pol=x['pol'], recognized=True,
                                     r240=round(x['r'], 2), A240=round(x['A'], 1), **describe(tr, x['t_peak'])))
            else:
                used.add(round(x['t'], 3))
                near = {}
                for D in (0.24, 0.60, 1.50):
                    cc = [q for q in cands if q['D'] == D and abs(q['t_peak'] - x['t']) < 0.5 * D + 0.1]
                    bq = max(cc, key=lambda q: q['absr']) if cc else None
                    near[str(int(D * 1000))] = None if bq is None else dict(r=round(bq['r'], 2), A=round(bq['A'], 1))
                passages.append(dict(seg=si, mm=mm, t=round(x['t'], 3), pol=x['pol'], recognized=False,
                                     width_ms=round(1e3 * (x['end'] - x['start'])), amp=round(x['amp'], 1),
                                     best_at_width=near, **describe(tr, x['t'])))
        seg['misses'] = sum(1 for q in passages if q['seg'] == si and not q['recognized'])
        segs.append(seg)
    unexplained = [dict(t=round(x['t'], 3), pol=x['pol'], amp=round(x['amp'], 1), width_ms=round(1e3 * (x['end'] - x['start'])), pwm=int(x['pwm']))
                   for x in extra if round(x['t'], 3) not in used]
    passages.sort(key=lambda p: p.get('t', 0))
    out = dict(segments=segs, passages=passages, oracle_only_not_a_map_magnet=unexplained,
               firmware_parity='see replay_vs_firmware output')
    for s in segs:
        print(json.dumps(s))
    print('misses:')
    for p in passages:
        if not p['recognized']:
            print('  ', json.dumps(p))
    print('oracle-only, not a map magnet:', json.dumps(unexplained))
    if len(sys.argv) > 4:
        Path(sys.argv[4]).write_text(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
