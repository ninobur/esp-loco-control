#!/usr/bin/env python3
"""Compare the firmware recognizer's causal candidate rule against the
research reference's causal_events, on a synthetic trace with competing
nearby peaks (Codex finding 3, Sam's assessment).

Builds a synthetic Hall signal (baseline + template-shaped bumps, some
closer together than the 120 ms NMS radius, with different amplitudes and
polarities), then:
  - research side: runs the research's causal_events (single 240 ms scale,
    pad 0.5, level-only, r>=0.7, |A|>=70) on the 500 Hz binned trace;
  - firmware side: feeds the same signal as 1 ms tick medians through the
    portable C++ core (replay_harness) and reads its declared events.
The two event lists must agree (passage time, polarity, r, A) — the
decision rule is the same, so any difference is an implementation bug.

usage: python3 compare_causal.py
  (run from tests/navi_waveform_recognizer; writes a temp .ticks.bin)
"""
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
RESEARCH = Path('/home/hatch/workspace/navi-matched-filter-20261026/research/navi_matched_filter')
SKETCH = HERE.parent.parent / 'NAVI_WAVEFORM_0_1'
sys.path.insert(0, str(RESEARCH))
import mf
import exp_continuous as ec

DT = 0.002          # 500 Hz bins, as in the research
BASE = 1900.0       # baseline ADC
# (peak time s, amplitude ADC counts, polarity implied by sign)
# A/B: same polarity, 80 ms apart (inside the 120 ms NMS radius), weaker
# earlier vs stronger earlier. They merge into one correlation peak, so
# each pair yields a single event — the decision logic is exercised by
# the merge, and both implementations must agree on the merged peak.
# C: opposite polarity, 100 ms apart — two distinct maxima, 140 ms apart
# (outside the NMS radius), so both are declared.
# F: opposite polarity, 50 ms apart — two distinct maxima ~130 ms apart,
# just outside the NMS radius: a boundary case for the suppression rule.
# D, E: lone / well-separated controls.
BUMPS = [
    (2.00, +150.0),   # A1 weaker, earlier
    (2.08, +180.0),   # A2 stronger, later (80 ms)
    (4.00, +180.0),   # B1 stronger, earlier
    (4.08, +150.0),   # B2 weaker, later (80 ms)
    (6.00, +170.0),   # C1 North
    (6.10, -170.0),   # C2 South (100 ms)
    (8.00, +170.0),   # D lone
    (10.00, +170.0),  # E1
    (11.50, -170.0),  # E2 (1.5 s later)
    (12.00, +170.0),  # F1 North
    (12.05, -170.0),  # F2 South (50 ms)
]
DUR = 13.0


def build_bins():
    """Synthetic 500 Hz signal: baseline + template-core bumps."""
    shape = mf.template_mean()
    core, ipk = mf.sampled_template(shape, 0.24, pad=0.0)
    # core is 120 samples; its peak is at index ipk. Normalise so the peak
    # is 1.0 (sampled_template already returns the mean curve scaled so its
    # 98th percentile is 1; use the actual peak value).
    peak = core[ipk]
    core = core / peak
    nb = int(DUR / DT)
    x = np.full(nb, BASE)
    t = (np.arange(nb) + 0.5) * DT
    for tpk, amp in BUMPS:
        # centre the core so its peak lands at tpk
        c0 = int(round(tpk / DT)) - ipk
        for k, v in enumerate(core):
            j = c0 + k
            if 0 <= j < nb:
                x[j] += amp * v
    return t, x


def research_events(t, x):
    cfg = mf.Config(scales=(0.24,), pad=0.5, detrend='mean',
                    r_min=0.7, a_min=70.0, nms=0.5)
    tr = mf.Trace(t=t, x=x, pwm=np.full(len(t), 90.0),
                  direction=np.ones(len(t)))
    stack = mf.scale_stack(tr, cfg)
    ev = ec.causal_events(tr, stack, cfg)
    return [(e['t_peak'], e['pol'], e['r'], e['A']) for e in ev]


def firmware_events(t, x):
    """Expand the 500 Hz bins to 1 ms tick medians (each bin value twice)
    and run the C++ replay harness."""
    ticks = []
    for j, v in enumerate(x):
        for k in range(2):
            tus = int((j * 2 + k) * 1000)
            ticks.append((tus, int(round(v))))
    binpath = HERE / 'fixtures' / 'competing.ticks.bin'
    binpath.parent.mkdir(exist_ok=True)
    with open(binpath, 'wb') as f:
        for tus, med in ticks:
            # Record layout must match replay_harness.cpp: (uint64 t_us,
            # int16 median), 10 bytes, little-endian. ('h' is int16; 'i'
            # is 4 bytes and would misalign every record.)
            f.write(struct.pack('<Qh', tus, med))
    harness = '/tmp/replay_harness'
    subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra',
                    f'-I{SKETCH}', str(HERE / 'replay_harness.cpp'),
                    '-o', harness], check=True)
    out = subprocess.run([harness, str(binpath)], capture_output=True,
                         text=True, check=True).stdout
    ev = []
    for line in out.strip().splitlines()[1:]:
        p, rec, r, a, pol = line.split(',')
        ev.append((int(p) / 1e6, int(pol), float(r), float(a)))
    return ev


def main():
    if not RESEARCH.is_dir():
        # The research corpus lives outside this repo; without it the
        # reference side cannot run. Skip (do not fail) so the harness
        # still runs end-to-end on a machine without the research tree.
        print('compare_causal: SKIPPED (research tree not found at '
              f'{RESEARCH})')
        return 0
    t, x = build_bins()
    ref = research_events(t, x)
    got = firmware_events(t, x)
    print(f'research causal_events: {len(ref)} events')
    for e in ref:
        print(f'  t_peak={e[0]:7.3f}s pol={e[1]:+d} r={e[2]:+.3f} A={e[3]:+7.1f}')
    print(f'firmware core:          {len(got)} events')
    for e in got:
        print(f'  t_peak={e[0]:7.3f}s pol={e[1]:+d} r={e[2]:+.3f} A={e[3]:+7.1f}')
    ok = len(ref) == len(got)
    if ok:
        for a, b in zip(ref, got):
            if (abs(a[0] - b[0]) > 0.002 or a[1] != b[1] or
                    abs(a[2] - b[2]) > 0.01 or abs(a[3] - b[3]) > 2.0):
                ok = False
                print(f'MISMATCH: research {a} vs firmware {b}')
    print('MATCH' if ok else 'MISMATCH: event lists differ')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
