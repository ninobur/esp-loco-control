#!/usr/bin/env python3
"""Diagnostic only: on the same capture, what would the 240+600 bank (Oct 27 bank replay rule) have
declared, and how does a 150 ms copy score the fast misses? Not a proposal.
Usage: python3 bank_check.py <capture.npz> results.json"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf                 # noqa: E402
import replay_bank as rb  # noqa: E402

tr = mf.condition(np.load(sys.argv[1])['native'])
P = json.load(open(sys.argv[2]))['passages']
truth = np.array([p['t'] for p in P])
cc = rb.candidates(tr, (0.15, 0.24, 0.60), 0.5)
for name, sc in (('240', (0.24,)), ('240+600', (0.24, 0.60)), ('150+240', (0.15, 0.24)), ('150+240+600', (0.15, 0.24, 0.60))):
    ev = rb.combine(cc, sc, rb.uniform(sc))
    et = np.array([e['t_peak'] for e in ev])
    hit = [i for i, t in enumerate(truth) if np.min(np.abs(et - t)) <= 0.3]
    pol_ok = sum(1 for i in hit if ev[int(np.argmin(np.abs(et - truth[i])))]['pol'] == P[i]['pol'])
    other = [dict(t=round(e['t_peak'], 3), pol=e['pol'], width=int(e['scale'] * 1000), pwm=int(e['pwm']), r=round(e['r'], 2), A=round(e['A'], 1))
             for e in ev if np.min(np.abs(truth - e['t_peak'])) > 0.3]
    missed = ['MM%03d@%d' % (P[i]['mm'], P[i]['pwm']) for i in range(len(P)) if i not in hit]
    print(name, 'events', len(ev), 'passages recognized', len(hit), '/', len(P), 'polarity ok', pol_ok,
          'missed', missed, 'events not at a map passage', other)
for p in P:
    if not p['recognized'] and p['pwm'] > 90:
        c = [q for q in cc if q['D'] == 0.15 and abs(q['t_peak'] - p['t']) < 0.2]
        b = max(c, key=lambda q: q['absr']) if c else None
        print('fast miss MM%03d pwm %d: 150 ms copy best r %s A %s' % (p['mm'], p['pwm'], b and round(b['r'], 2), b and round(b['A'], 1)))
