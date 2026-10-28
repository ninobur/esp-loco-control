#!/usr/bin/env python3
"""Where did each standstill leave the Hall sensor: on ordinary track, or inside a magnet's field?
Compares the median Hall level during each standstill (PWM 0 for >= 1 s) with the median level of the
moving background in the 5 s before it (excluding +-0.6 s around recognized passages).
Usage: python3 standstills.py <capture.npz> results.json"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf  # noqa: E402

tr = mf.condition(np.load(sys.argv[1])['native'])
P = json.load(open(sys.argv[2]))['passages']
pt = np.array([p['t'] for p in P])
mov = tr.pwm > 0
ch = np.flatnonzero(np.diff(mov.astype(int))) + 1
for s in np.split(np.arange(len(mov)), ch):
    if mov[s[0]] or len(s) * mf.DT_S < 1.0 or s[0] == 0:
        continue
    a, b = tr.t[s[0]], tr.t[s[-1]]
    lvl = np.nanmedian(tr.x[s])
    w = (tr.t >= a - 5) & (tr.t < a) & mov
    if len(pt):
        w &= np.min(np.abs(tr.t[:, None] - pt[None, :]), axis=1) > 0.6
    bg = np.nanmedian(tr.x[w]) if w.any() else np.nan
    last = [p for p in P if p['t'] < a][-1:] 
    nxt = [p for p in P if p['t'] > b][:1]
    print('standstill %7.1f-%7.1f  level %6.0f  moving background %6.0f  dev %+5.0f   last passage %s  next %s' % (
        a, b, lvl, bg, lvl - bg,
        ('MM%03d %.1f s before' % (last[0]['mm'], a - last[0]['t'])) if last else '-',
        ('MM%03d %.1f s after, %s' % (nxt[0]['mm'], nxt[0]['t'] - b, 'recognized' if nxt[0]['recognized'] else 'MISSED')) if nxt else '-'))
