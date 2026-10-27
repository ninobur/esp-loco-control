#!/usr/bin/env python3
"""Question D / config selection: score threshold x amplitude floor x window.

Selection uses the dev partition only. The chosen configuration is then
applied, unchanged, to validation and transfer (see exp_continuous.py).
"""
import collections
import json
import sys
from pathlib import Path

import numpy as np

import corpus
import mf

OUT = Path(__file__).resolve().parent / 'results'
R_MIN = (0.5, 0.6, 0.7, 0.8, 0.9)
A_MIN = (0, 20, 30, 40, 50, 70, 90, 110, 130)
WINDOWS = [('mean', 0.5), ('mean', 1.0), ('line', 0.5), ('line', 1.0)]


def run(partitions):
    rows = []
    sess = [corpus.load(n) for n in corpus.sessions(partitions)]
    for det, pad in WINDOWS:
        base = mf.Config(detrend=det, pad=pad)
        stacks = [mf.scale_stack(s['trace'], base) for s in sess]
        for r_min in R_MIN:
            for a_min in A_MIN:
                cfg = mf.Config(detrend=det, pad=pad, r_min=r_min, a_min=a_min)
                tot = collections.Counter()
                amps = []
                for s, (R, A, po) in zip(sess, stacks):
                    ev = mf.events_from_stack(s['trace'], R, A, po, cfg)
                    c, _ = corpus.score(s, ev)
                    tot.update(c)
                    amps += [abs(e['A']) for e in ev if e['class'].startswith('magnet')]
                rows.append(dict(detrend=det, pad=pad, r_min=r_min, a_min=a_min,
                                 recognized=tot['recognized'], ref=tot['ref_magnets'],
                                 missed=tot['missed'], false=tot['false'],
                                 duplicate=tot['duplicate'],
                                 amp_p01=float(np.quantile(amps, .01)) if amps else None,
                                 amp_min=float(min(amps)) if amps else None))
                print(det, pad, r_min, a_min, rows[-1]['recognized'], rows[-1]['ref'],
                      rows[-1]['false'], rows[-1]['duplicate'], flush=True)
    return rows


if __name__ == '__main__':
    parts = sys.argv[1:] or ['dev']
    OUT.mkdir(exist_ok=True)
    rows = run(parts)
    (OUT / ('sweep_%s.json' % '_'.join(parts))).write_text(json.dumps(rows, indent=1))
