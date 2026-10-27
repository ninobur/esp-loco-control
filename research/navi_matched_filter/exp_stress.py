#!/usr/bin/env python3
"""Question A: baseline independence under offsets, drift, steps, noise, spikes.

The same recorded validation sessions are modified and re-run with the frozen
configuration. Offsets/drift/steps are added to the conditioned series; noise
and spikes are added to the native samples *before* the median-of-5, because
that is where they occur physically. The reference (route anchors) comes from
the unmodified recording, so only the recognizer sees the disturbance.
"""
import collections
import json
import sys
from pathlib import Path

import numpy as np

import corpus
import mf
from config import FROZEN, LINE_ALT

OUT = Path(__file__).resolve().parent / 'results'
RNG_SEED = 20261026


def disturb_series(tr, kind, p, rng):
    x = tr.x.copy()
    t = tr.t
    if kind == 'offset':
        x = x + p
    elif kind == 'sine_drift':          # amplitude 100 counts, period p seconds
        x = x + 100.0 * np.sin(2 * np.pi * t / p + rng.uniform(0, 2 * np.pi))
    elif kind == 'ramp_drift':          # constant slope p counts/s (sawtooth, 400 span)
        x = x + (p * t) % 400.0 - 200.0
    elif kind == 'steps':               # Poisson steps every ~5 s, +/- p, bounded walk
        n = rng.poisson(t[-1] / 5.0)
        at = np.sort(rng.uniform(0, t[-1], n))
        level = np.zeros_like(t)
        cur = 0.0
        for a in at:
            cur = np.clip(cur + rng.choice([-p, p]), -2 * p, 2 * p)
            level[t >= a] = cur
        x = x + level
    out = mf.Trace(t=t, x=x, pwm=tr.pwm, direction=tr.direction, t0_us=tr.t0_us, name=tr.name)
    return out


def disturb_native(nat, kind, p, rng):
    nat = nat.copy()
    raw = nat['raw'].astype(float)
    if kind == 'noise':
        raw = raw + rng.normal(0, p, len(raw))
    elif kind == 'spikes':              # p spikes per second, single samples, 150–400 counts
        dur = (nat['t_us'][-1] - nat['t_us'][0]) / 1e6
        n = rng.poisson(p * dur)
        idx = rng.integers(0, len(raw), n)
        raw[idx] += rng.choice([-1, 1], n) * rng.uniform(150, 400, n)
    elif kind == 'spike_pairs':         # two adjacent samples (defeats median-of-5? no: 2 < 3)
        dur = (nat['t_us'][-1] - nat['t_us'][0]) / 1e6
        n = rng.poisson(p * dur)
        idx = rng.integers(0, len(raw) - 3, n)
        s = rng.choice([-1, 1], n) * rng.uniform(150, 400, n)
        raw[idx] += s
        raw[idx + 1] += s
        raw[idx + 2] += s
    nat['raw'] = np.clip(np.round(raw), -32768, 32767).astype(np.int16)
    return nat


CONDITIONS = [('none', 0), ('offset', 100), ('offset', -100),
              ('sine_drift', 60), ('sine_drift', 10), ('sine_drift', 3),
              ('ramp_drift', 5), ('ramp_drift', 20), ('ramp_drift', 100),
              ('steps', 25), ('steps', 50), ('steps', 100), ('steps', 200),
              ('noise', 5), ('noise', 10), ('noise', 20),
              ('spikes', 2), ('spikes', 20), ('spike_pairs', 2)]


def run(partitions, cfgs):
    rows = []
    sess = [corpus.load(n) for n in corpus.sessions(partitions)]
    for kind, p in CONDITIONS:
        rng = np.random.default_rng(RNG_SEED)
        for cname, cfg in cfgs.items():
            tot = collections.Counter()
            for s in sess:
                if kind in ('noise', 'spikes', 'spike_pairs'):
                    nat = np.load(corpus.NSR_DIR / (s['name'] + '.npz'))['native']
                    tr = mf.condition(disturb_native(nat, kind, p, rng))
                elif kind == 'none':
                    tr = s['trace']
                else:
                    tr = disturb_series(s['trace'], kind, p, rng)
                ev, _ = mf.recognize(tr, cfg)
                c, _ = corpus.score(s, ev)
                tot.update(c)
            rows.append(dict(condition=kind, param=p, config=cname, recognized=tot['recognized'],
                             ref=tot['ref_magnets'], false=tot['false'], duplicate=tot['duplicate']))
            print(rows[-1], flush=True)
    return rows


if __name__ == '__main__':
    parts = sys.argv[1:] or ['validation']
    OUT.mkdir(exist_ok=True)
    rows = run(parts, {'frozen_mean': FROZEN, 'line_alt': LINE_ALT})
    (OUT / ('stress_%s.json' % '_'.join(parts))).write_text(json.dumps(rows, indent=1))
