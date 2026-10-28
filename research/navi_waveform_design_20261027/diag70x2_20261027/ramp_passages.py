#!/usr/bin/env python3
"""Ramp-up and slow-down passages per driven Oct 27 run, and whether the fielded single-240 matched
filter recognized each one. Research only.

A passage is 'up' if its template peak precedes the first PWM >= 90, 'down' if it follows the last
PWM >= 90; passages between those at PWM >= 90 are cruise and not listed. PWM is the applied PWM at
the passage's peak (20 ms conditioned trace). Recognized = a single-240 event within 0.15 s of the
truth peak (truth as in replay_bank.truth27).

Usage: python3 ramp_passages.py <oct27 npz dir>
"""
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf                 # noqa: E402
import replay_bank as rb  # noqa: E402


def main():
    for p in sorted(Path(sys.argv[1]).glob('*.npz')):
        boot = p.stem.split('_')[3]
        if boot not in rb.DRIVEN27:
            continue
        tr = mf.condition(np.load(p)['native'])
        tru, _ = rb.truth27(tr, boot)
        ev = rb.combine(rb.candidates(tr, (0.24,), 0.5), (0.24,), rb.uniform((0.24,)))
        et = np.array([e['t_peak'] for e in ev])
        i90 = np.flatnonzero(tr.pwm >= 90)
        first90, last90 = tr.t[i90[0]], tr.t[i90[-1]]
        print(rb.LABEL27[boot], 'magnets', len(tru))
        for phase in ('up', 'down'):
            rows = []
            for tp, pol in tru:
                if (phase == 'up' and tp >= first90) or (phase == 'down' and tp <= last90):
                    continue
                P = int(tr.pwm[min(int(tp / mf.DT_S), len(tr.pwm) - 1)])
                rec = bool(len(et)) and np.min(np.abs(et - tp)) < 0.15
                rows.append((round(tp, 2), P, 'Y' if rec else 'MISS'))
            print('  ', phase, len(rows), 'recognized', sum(r[2] == 'Y' for r in rows), rows)


if __name__ == '__main__':
    main()
