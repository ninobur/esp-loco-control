#!/usr/bin/env python3
"""Diagnostic only: the same width banks as bank_thresholds_evening.py on the six Oct 27 afternoon
driven runs (truth and scoring: replay_bank.truth27 / score27), plus stationary events in the parked
boots. Usage: python3 bank_thresholds_afternoon.py <oct27 npz dir>"""
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf                 # noqa: E402
import replay_bank as rb  # noqa: E402

CONFIGS = [((0.24, 0.60), 0.7), ((0.15, 0.24, 0.60, 1.50), 0.7), ((0.15, 0.24, 0.60, 1.50), 0.9)]
tot = {i: [0, 0, 0, 0, 0] for i in range(len(CONFIGS))}   # truth, recognized, false, dup, stationary(parked)
for p in sorted(Path(sys.argv[1]).glob('*.npz')):
    boot = p.stem.split('_')[3]
    tr = mf.condition(np.load(p)['native'])
    cc = rb.candidates(tr, (0.15, 0.24, 0.60, 1.50), 0.5)
    tru = rb.truth27(tr, boot)[0] if boot in rb.DRIVEN27 else None
    for i, (sc, wide) in enumerate(CONFIGS):
        thr = {D: ((0.7, 70.0) if D == 0.24 else (wide, 70.0)) for D in sc}
        ev = rb.combine(cc, sc, thr)
        if tru is None:
            tot[i][4] += sum(1 for e in ev if e['pwm'] == 0)
            continue
        taken, missed = rb.score27(ev, tru)
        tot[i][0] += len(tru); tot[i][1] += len(taken)
        tot[i][2] += sum(e['class'] == 'false' for e in ev); tot[i][3] += sum(e['class'] == 'duplicate' for e in ev)
        if missed or any(e['class'] in ('false', 'duplicate') for e in ev):
            print(rb.LABEL27[boot], '+'.join(str(int(D * 1000)) for D in sc), wide, 'missed', [round(tru[j][0], 2) for j in missed],
                  'false/dup', [(round(e['t_peak'], 2), e['class'], int(e['scale'] * 1000), round(e['r'], 2), round(e['A'], 1), int(e['pwm'])) for e in ev if e['class'] in ('false', 'duplicate')])
for i, (sc, wide) in enumerate(CONFIGS):
    t = tot[i]
    print('%-18s wide|r|>=%.1f  recognized %d/%d  false %d  duplicates %d  parked-boot stationary events %d' % (
        '+'.join(str(int(D * 1000)) for D in sc), wide, t[1], t[0], t[2], t[3], t[4]))
