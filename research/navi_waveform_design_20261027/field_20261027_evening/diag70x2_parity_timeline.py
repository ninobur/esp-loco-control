#!/usr/bin/env python3
"""Where do the replay's 70x2 onsets and the firmware's diag_onsets counter diverge in time?
Firmware: cumulative diag_onsets from state/loopstat (about 1 Hz, Pi wall clock). Replay: cumulative
onsets on the capture clock. The wall-to-capture offset is chosen to minimise the mismatch over the
whole run; the step where the difference changes is then reported with the capture's serial gaps.
Usage: python3 diag70x2_parity_timeline.py <capture.npz> <mqtt log>"""
import datetime as dt
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'diag70x2_20261027'))
import replay_70x2 as r7  # noqa: E402

native = np.load(sys.argv[1])['native']
ons = r7.detect70x2(native)
ot = np.array([o[2] for o in ons])
fw = []
for line in open(sys.argv[2]):
    p = line.rstrip('\n').split('\t', 2)
    if len(p) == 3 and p[1].endswith('state/loopstat'):
        fw.append((dt.datetime.fromisoformat(p[0]).timestamp(), json.loads(p[2])['diag_onsets']))
fw = np.array(fw)
best = None
for off in np.arange(fw[0, 0] - 5, fw[0, 0] + 5, 0.02):
    rep = np.searchsorted(ot, fw[:, 0] - off, side='right')
    err = np.abs(rep - fw[:, 1]).sum()
    if best is None or err < best[0]:
        best = (err, off)
off = best[1]
rep = np.searchsorted(ot, fw[:, 0] - off, side='right')
d = fw[:, 1] - rep
print('best offset gives total |diff| %d; final firmware %d replay %d' % (best[0], fw[-1, 1], len(ot)))
ch = np.flatnonzero(np.diff(d) != 0)
for i in ch:
    print('firmware - replay changes %+d -> %+d between capture t %.1f and %.1f s' % (d[i], d[i + 1], fw[i, 0] - off, fw[i + 1, 0] - off))
s = native['serial'].astype(np.int64)
t = (native['t_us'].astype(np.int64) - int(native['t_us'][0])) / 1e6
for i in np.flatnonzero(np.diff(s) != 1):
    print('serial gap at %.3f s: %d reads missing' % (t[i], s[i + 1] - s[i] - 1))
