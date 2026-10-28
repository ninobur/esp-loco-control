#!/usr/bin/env python3
"""Moving-background Hall level (median, excluding +-0.4 s around reference magnets) in 10 s windows,
for the sessions that hold the investigated misses. Shows whether the background stepped where the
misses lie. Usage: NGR_NSR_NPZ=.. NGR_MF_CACHE=.. python3 background_levels.py <boot8> [<boot8> ...]"""
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import corpus  # noqa: E402

for b in sys.argv[1:]:
    n = [x for x in corpus.sessions() if b in x][0]
    s = corpus.load(n)
    tr = s['trace']
    rt = np.array([r['t'] for r in s['refs']])
    far = np.ones(len(tr.t), bool)
    for t in rt:
        far &= np.abs(tr.t - t) > 0.4
    print(b, 'duration %.0f s' % tr.t[-1])
    end = min(tr.t[-1], 200.0)
    for a in np.arange(0, end, 10):
        w = (tr.t >= a) & (tr.t < a + 10) & far
        x = tr.x[w]
        x = x[np.isfinite(x)]
        if len(x) < 100:
            continue
        print('   %5.0f-%5.0f s  pwm %3d  background median %6.0f  5th-95th pct %6.0f-%6.0f' % (
            a, a + 10, int(np.median(tr.pwm[w])), np.median(x), np.percentile(x, 5), np.percentile(x, 95)))
