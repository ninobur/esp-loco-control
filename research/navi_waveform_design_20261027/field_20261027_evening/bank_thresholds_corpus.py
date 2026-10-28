#!/usr/bin/env python3
"""Diagnostic only: width banks on the Oct 25-26 held-out corpus (corpus.score, as in the bank replay),
uniform 0.7 / 70 and with the non-240 copies raised to |r| >= 0.9. Lists every false event.
Usage: NGR_NSR_NPZ=<oct25-26 npz> NGR_MF_CACHE=<cache> python3 bank_thresholds_corpus.py"""
import collections
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import corpus             # noqa: E402
import replay_bank as rb  # noqa: E402

CONFIGS = [((0.24,), 0.7), ((0.24, 0.60, 1.50), 0.9), ((0.15, 0.24, 0.60, 1.50), 0.7), ((0.15, 0.24, 0.60, 1.50), 0.9)]
held = [n for n in corpus.sessions(['validation', 'transfer']) if '_20261027_' not in n]
agg = collections.defaultdict(collections.Counter)
falses = collections.defaultdict(list)
for n in held:
    s = corpus.load(n)
    tr = s['trace']
    cc = rb.candidates(tr, (0.15, 0.24, 0.60, 1.50), 0.5)
    for i, (sc, wide) in enumerate(CONFIGS):
        thr = {D: ((0.7, 70.0) if D == 0.24 else (wide, 70.0)) for D in sc}
        ev = rb.combine(cc, sc, thr)
        c, missed = corpus.score(s, ev)
        for q in ('ref_magnets', 'recognized', 'false', 'duplicate'):
            agg[i][q] += c[q]
        for j in missed:
            agg[i]['miss_' + rb.condition_of(tr.pwm[min(int(s['refs'][j]['t'] / rb.DT), len(tr.pwm) - 1)])] += 1
        falses[i] += [(n[13:43], round(e['t_peak'], 2), int(e['scale'] * 1000), round(e['r'], 2), round(e['A'], 1), int(e['pwm']))
                      for e in ev if e['class'] == 'false']
for i, (sc, wide) in enumerate(CONFIGS):
    print('%-18s wide|r|>=%.1f  %s' % ('+'.join(str(int(D * 1000)) for D in sc), wide, dict(agg[i])))
    for f in falses[i]:
        print('     false', f)
