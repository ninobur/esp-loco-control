#!/usr/bin/env python3
"""For each investigated miss: the same loco's other passages of the same magnet in the held-out corpus,
with the oracle amplitude and width and whether the fielded 240 recognized them.
Usage: NGR_NSR_NPZ=.. NGR_MF_CACHE=.. python3 same_magnet_other_laps.py misses.json"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import corpus             # noqa: E402
import replay_bank as rb  # noqa: E402

M = json.load(open(sys.argv[1]))
want = {(m['loco'], m['mm']) for m in M}
held = [n for n in corpus.sessions(['validation', 'transfer']) if '_20261027_' not in n]
seen = {k: [] for k in want}
for n in held:
    loco = 'Otto' if '_9950011_' in n else 'Toby'
    if not any(l == loco for l, _ in want):
        continue
    s = corpus.load(n)
    tr = s['trace']
    ev = rb.combine(rb.candidates(tr, (0.24,), 0.5), (0.24,), rb.uniform((0.24,)))
    _, missed = corpus.score(s, ev)
    for j, r in enumerate(s['refs']):
        if (loco, r['mm']) not in want:
            continue
        ex = [x for x in s['exc'] if x['pol'] == r['pol'] and abs(x['t'] - r['t']) <= 0.15]
        x = max(ex, key=lambda x: x['amp']) if ex else None
        seen[(loco, r['mm'])].append((n.split('_')[3][:8], round(r['t'], 1), int(tr.pwm[min(int(r['t'] / rb.DT), len(tr.pwm) - 1)]),
                                      None if x is None else round(x['amp']), None if x is None else round(1e3 * (x['end'] - x['start'])),
                                      j not in missed))
for k in sorted(want):
    v = seen[k]
    print(k, 'passages', len(v), 'recognized by 240', sum(x[5] for x in v))
    for x in v:
        print('    ', x)
