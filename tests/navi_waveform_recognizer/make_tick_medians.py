#!/usr/bin/env python3
"""Build 1 ms tick medians from recorded native Hall samples, exactly as the
firmware's hallTask does (median of the reads in each 1 ms tick), for the host
replay harness.

Input:  an .npz from extract_nsr.py (a `native` structured array: serial,
        t_us, raw, pwm, direction), or the plotted-samples CSV (panels of
        continuous 5 kHz native samples).
Output: <stem>.ticks.bin — one record per tick: int16 median, uint64 t_us
        (the first sample's time in the tick, the firmware's firstUs
        convention), plus <stem>.ticks.json with the manifest.
Ticks with no samples are omitted (a gap); the core's gap handling
invalidates any window straddling one. Ticks with 1-4 samples take the
median of what is there; ticks with more take the median of all of them.

usage:
  make_tick_medians.py level1.npz
  make_tick_medians.py panels.csv
"""
import csv
import json
import struct
import sys
from pathlib import Path

import numpy as np


def ticks_from_native(native):
    """Recover the firmware's 1 ms ticks: hallTask takes exactly 5 reads per
    tick, incrementing `serial` each time, so the sorted native samples chunk
    into groups of 5 in array order. The tick median is the median of the 5;
    the tick time is the first sample's t_us (the firmware's firstUs).
    A trailing partial tick (fewer than 5 samples) is dropped."""
    t = native['t_us'].astype(np.int64)
    raw = native['raw'].astype(np.int64)
    t0 = int(t[0])
    n_ticks = len(raw) // 5
    medians = np.empty(n_ticks, dtype='<i2')
    times = np.empty(n_ticks, dtype='<u8')
    for i in range(n_ticks):
        vals = raw[5 * i:5 * i + 5]
        medians[i] = int(np.median(vals))
        times[i] = int(t[5 * i])
    return medians, times, t0


def ticks_from_csv(path):
    """Panels of continuous native samples; each panel is its own run."""
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            rows.append((int(r['serial']), int(r['source_us']),
                         int(r['raw_adc']), int(r['pwm']), int(r['direction'])))
    rows.sort()
    # split into panels on large serial jumps (> 1000) — the CSV's panels
    panels, cur = [], [rows[0]]
    for prev, r in zip(rows, rows[1:]):
        if r[0] - prev[0] > 1000:
            panels.append(cur)
            cur = [r]
        else:
            cur.append(r)
    panels.append(cur)
    all_med, all_t, meta = [], [], []
    for p in panels:
        native = np.zeros(len(p), dtype=[('serial', '<u4'), ('t_us', '<u8'),
                                          ('raw', '<i2'), ('pwm', 'u1'),
                                          ('direction', 'u1')])
        for i, (s, u, raw, pwm, d) in enumerate(p):
            native[i] = (s, u, raw, pwm, d)
        med, tt, t0 = ticks_from_native(native)
        all_med.append(med)
        all_t.append(tt)
        meta.append({'t0_us': t0, 'ticks': int(len(med)),
                     'duration_s': float((tt[-1] - tt[0]) / 1e6),
                     'pwm_min': int(min(x[3] for x in p)),
                     'pwm_max': int(max(x[3] for x in p))})
    return (np.concatenate(all_med), np.concatenate(all_t),
            {'panels': meta})


def main():
    src = Path(sys.argv[1])
    if src.suffix == '.npz':
        native = np.load(src)['native']
        med, tt, t0 = ticks_from_native(native)
        meta = {'source': src.name, 't0_us': int(t0), 'ticks': int(len(med)),
                'duration_s': float((tt[-1] - tt[0]) / 1e6),
                'native_samples': int(len(native))}
    elif src.suffix == '.csv':
        med, tt, meta = ticks_from_csv(src)
        meta['source'] = src.name
    else:
        raise SystemExit('expected .npz or .csv')
    out = src.with_suffix('').with_suffix('.ticks')
    with open(str(out) + '.bin', 'wb') as f:
        for m, u in zip(med, tt):
            f.write(struct.pack('<qh', int(u), int(m)))
    meta['ticks_file'] = str(out) + '.bin'
    with open(str(out) + '.json', 'w') as f:
        json.dump(meta, f, indent=1)
    print(f"{src.name}: {len(med)} ticks, "
          f"{meta.get('duration_s', 0):.1f} s -> {out}.bin")


if __name__ == '__main__':
    main()
