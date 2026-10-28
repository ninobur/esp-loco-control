#!/usr/bin/env python3
"""Diagnostic only: width banks on Otto's Oct 27 evening run, with uniform thresholds (0.7 / 70) and
with the non-240 copies raised to |r| >= 0.9 (the bank replay's §4 setting). Scored against the 527
map-confirmed passages (MM057 at its corrected time, 427.577 s). An event more than 0.3 s from every
passage, or a second event on one passage, is counted as false / duplicate.
Usage: python3 bank_thresholds_evening.py <capture.npz> results.json"""
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
for p in P:
    if p['mm'] == 57 and not p['recognized']:
        p['t'] = 427.577
tt = np.array(sorted(p['t'] for p in P))
cc = rb.candidates(tr, (0.15, 0.24, 0.60, 1.50), 0.5)
for sc in ((0.24,), (0.24, 0.60), (0.24, 0.60, 1.50), (0.15, 0.24), (0.15, 0.24, 0.60), (0.15, 0.24, 0.60, 1.50)):
    for wide in (0.7, 0.9):
        if sc == (0.24,) and wide == 0.9:
            continue
        thr = {D: ((0.7, 70.0) if D == 0.24 else (wide, 70.0)) for D in sc}
        ev = rb.combine(cc, sc, thr)
        taken, false, dup = set(), [], 0
        for e in sorted(ev, key=lambda e: -e['absr']):
            j = int(np.argmin(np.abs(tt - e['t_peak'])))
            if abs(tt[j] - e['t_peak']) > 0.3:
                false.append((round(e['t_peak'], 3), int(e['scale'] * 1000), round(e['r'], 2), round(e['A'], 1)))
            elif j in taken:
                dup += 1
                false.append(('dup', round(e['t_peak'], 3), int(e['scale'] * 1000), round(e['r'], 2), round(e['A'], 1)))
            else:
                taken.add(j)
        missed = [round(tt[j], 2) for j in range(len(tt)) if j not in taken]
        print('%-22s wide|r|>=%.1f  recognized %d/527  false+dup %d %s  missed %s' % (
            '+'.join(str(int(D * 1000)) for D in sc), wide, len(taken), len(false), false, missed))
