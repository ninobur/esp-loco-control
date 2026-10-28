#!/usr/bin/env python3
"""Follow-up to replay_bank.py: (1) where a declaration arrives after the next one's,
(2) best-fitting width (largest |r| among 240/600/1500 at the event, offline, observation
only) against traversal speed. Usage as replay_bank.py; writes followup.json."""
import collections
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import replay_bank as rb   # noqa: E402
from replay_bank import corpus, mf, truth, DT  # noqa: E402

W3 = (0.24, 0.60, 1.50)


def best_width(cc, t):
    near = [x for x in cc if x['D'] in W3 and abs(x['t_peak'] - t) <= max(0.1, 0.4 * x['D'])]
    return max(near, key=lambda x: x['absr'])['D'] if near else None


def main():
    out = dict(out_of_order=[], width_speed=[])
    SP = truth.route_spacing()
    held = [n for n in corpus.sessions(['validation', 'transfer']) if '_20261027_' not in n]
    for n in held:
        s = corpus.load(n)
        tr = s['trace']
        cc = rb.candidates(tr, W3, 0.5)
        for k in ('240+600', '240+600+1500'):
            ev = rb.combine(cc, rb.PRIMARY[k], rb.uniform(rb.PRIMARY[k]))
            corpus.score(s, ev)
            for a, b in zip(ev[:-1], ev[1:]):
                if a['t_decl'] > b['t_decl']:
                    out['out_of_order'].append(dict(config=k, session=n[13:43], late=rb.ev_row(a), late_class=a['class'],
                                                    next=rb.ev_row(b), next_class=b['class']))
        ev = rb.combine(cc, W3, rb.uniform(W3))
        corpus.score(s, ev)
        g = sorted([e for e in ev if e['class'].startswith('magnet')], key=lambda e: e['t_peak'])
        for a, b in zip(g[:-1], g[1:]):
            ma, mb = s['refs'][a['ref']]['mm'], s['refs'][b['ref']]['mm']
            dist = SP[ma] if (mb - ma) % 171 == 1 else (SP[mb] if (ma - mb) % 171 == 1 else None)
            iv = b['t_peak'] - a['t_peak']
            if dist and 0.2 < iv < 10:
                out['width_speed'].append((best_width(cc, b['t_peak']), dist / iv, b['pwm']))
        print('done', n[13:43], flush=True)
    ws = out['width_speed']
    summ = {}
    for D in W3:
        v = [x[1] for x in ws if x[0] == D]
        summ[int(D * 1000)] = dict(n=len(v), speed_mm_s_median=round(float(np.median(v))) if v else None,
                                   speed_p5=round(float(np.percentile(v, 5))) if v else None,
                                   speed_p95=round(float(np.percentile(v, 95))) if v else None)
    out['width_speed_summary'] = summ
    sp = np.array([x[1] for x in ws])
    bands = [(0, 100), (100, 150), (150, 200), (200, 300), (300, 2000)]
    out['best_width_by_speed_band'] = {'%d-%d' % b: dict(collections.Counter(int(x[0] * 1000) for x in ws if b[0] <= x[1] < b[1]))
                                       for b in bands}
    del out['width_speed']
    Path(sys.argv[2] if len(sys.argv) > 2 else Path(__file__).resolve().parent / 'followup.json').write_text(json.dumps(out, indent=1, default=float))


if __name__ == '__main__':
    main()
