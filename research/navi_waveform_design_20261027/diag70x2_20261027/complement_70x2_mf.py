#!/usr/bin/env python3
"""Fourth question (David 2026-10-27): could 70x2 give an immediate observation and the matched filter
an independent confirmation? Research only, replay only.

For each 70x2 onset (firmware-exact, replay_70x2.detect70x2), look for a matched-filter event of the
SAME polarity whose template peak lies within [onset - 0.15 s, onset + 0.6 s]: the window in which every
leading onset's own peak fell (onset leads the peak by 26-171 ms). Report, per detector configuration
(single 240 as fielded; bank 240+600, replay only), how many leading onsets are confirmed, how many
non-detection onsets (duplicates, trailing, false, stationary) are also 'confirmed' by some event, and the
confirmation delay (event declaration time minus onset time).

Usage: python3 complement_70x2_mf.py <oct27 npz dir> [out.json]
"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf                 # noqa: E402
import replay_bank as rb  # noqa: E402
import replay_70x2 as r7  # noqa: E402

CONFIGS = {'mf240': (0.24,), 'bank240+600': (0.24, 0.60)}


def main():
    res = {}
    for p in sorted(Path(sys.argv[1]).glob('*.npz')):
        boot = p.stem.split('_')[3]
        if boot not in rb.DRIVEN27:
            continue
        native = np.load(p)['native']
        ons = r7.detect70x2(native)
        tr = mf.condition(native)
        tru, _ = rb.truth27(tr, boot)
        _, cls = r7.associate(ons, tru)
        cc = rb.candidates(tr, rb.ALL6, 0.5)
        out = {}
        for k, sc in CONFIGS.items():
            ev = rb.combine(cc, sc, rb.uniform(sc))
            ep = np.array([e['t_peak'] for e in ev]); epol = np.array([e['pol'] for e in ev])
            ed = np.array([e['t_decl'] for e in ev])
            stat = {}
            delays, lead_unconf = [], []
            for o, c in zip(ons, cls):
                kind = 'stationary' if o[5] == 0 else c[0]
                m = np.flatnonzero((epol == o[3]) & (ep >= o[2] - 0.15) & (ep <= o[2] + 0.6))
                s = stat.setdefault(kind, [0, 0])
                s[0] += 1
                if len(m):
                    s[1] += 1
                    if kind == 'leading':
                        delays.append(ed[m[0]] - o[2])
                elif kind == 'leading':
                    lead_unconf.append(dict(t=round(o[2], 3), pol=o[3], pwm=o[5]))
            out[k] = dict(onsets_confirmed={kk: dict(onsets=v[0], confirmed=v[1]) for kk, v in stat.items()},
                          leading_unconfirmed=lead_unconf,
                          confirm_delay_ms=dict(min=round(1e3 * min(delays)), median=round(1e3 * float(np.median(delays))),
                                                max=round(1e3 * max(delays))) if delays else None,
                          mf_events_without_leading_onset=[dict(t_peak=round(e['t_peak'], 3), pol=e['pol'], width_ms=int(e['scale'] * 1000))
                                                          for e in ev if not any(c[0] == 'leading' and o[3] == e['pol'] and e['t_peak'] - 0.6 <= o[2] <= e['t_peak'] + 0.15
                                                                                 for o, c in zip(ons, cls))])
        res[boot] = dict(label=rb.LABEL27[boot], **out)
        print(boot, json.dumps(res[boot]))
    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(json.dumps(res, indent=1))


if __name__ == '__main__':
    main()
