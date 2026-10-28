#!/usr/bin/env python3
"""The 70x2 diagnostic detector of NAVI_WAVEFORM_0_1 (firmware-exact port, ../diag70x2_20261027/
replay_70x2.detect70x2) on Otto's Oct 27 evening capture, scored against the 527 map-confirmed
passages in results.json (when_it_fails.py). Research only.

Parity: replay onset total against the last state/loopstat diag_onsets for the boot.
Classes (replay_70x2.associate, unchanged): leading = the detection (same polarity, at most +50 ms
after the passage peak); duplicate = a second leading onset on one passage; trailing = opposite
polarity after the peak (the 200 ms level has followed the magnet); false = more than 0.6 s from any
passage, or any other combination. Onsets at applied PWM 0 are reported separately as stop onsets.
For each of the 13 recognizer misses: whether 70x2 detected it, its onset time, polarity, PWM, and
lead before the passage peak; if not detected, the largest |raw - level| on an evaluated read within 1 s.

Usage: python3 diag70x2_evening.py <capture.npz> <mqtt log> <BOOT> results.json [out.json]
"""
import collections
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'diag70x2_20261027'))
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import replay_70x2 as r7  # noqa: E402


# MM057's passage: when_it_fails.py's map alignment attached MM057 to the oracle excursion at 427.837 s,
# which is the hardware transient (raw 4012 -> 721, 427.770-427.842 s). Both that and the excursion at
# 427.577 s (S, amplitude 183, 82 ms, Hall down to 1736) had zero alignment cost; the alignment tie-broke
# to the later one. The 427.577 s excursion is the magnet: it is the passage-shaped one, it sits where the
# neighbouring intervals put MM057 (MM058 426.813 + 0.77 s; MM056 428.289 - 0.71 s), and a 150 ms copy
# scores it r -0.99, A -195. The passage time used here is corrected to it.
PASSAGE_TIME_CORRECTIONS = {57: 427.577}


def level_trace_max_dev(native, t, tp):
    raw = native['raw'].astype(np.int64); ser = native['serial'].astype(np.int64)
    ring = np.zeros(r7.WINDOW, np.int64); h = c = 0; lev = 0.0; best = (0.0, None)
    k0 = int(np.searchsorted(t, tp - 3.0)); k1 = int(np.searchsorted(t, tp + 1.0))
    for k in range(k0, k1):
        ring[h] = raw[k]; h = (h + 1) % r7.WINDOW; c = min(c + 1, r7.WINDOW)
        if ser[k] % r7.RECOMPUTE == 0 and c >= r7.MIN_SAMPLES:
            v = ring if c == r7.WINDOW else ring[:c]
            lev = float(np.partition(v, c // 2)[c // 2])
        if t[k] >= tp - 1.0 and ser[k] % r7.STRIDE == 0 and c == r7.WINDOW:
            d = float(raw[k]) - lev
            if abs(d) > abs(best[0]):
                best = (d, round(float(t[k]), 3))
    return best


def main():
    npz, log, boot, res = sys.argv[1:5]
    native = np.load(npz)['native']
    t = (native['t_us'].astype(np.int64) - int(native['t_us'][0])) / 1e6
    ons = r7.detect70x2(native)
    fw = None
    for line in open(log):
        p = line.rstrip('\n').split('\t', 2)
        if len(p) == 3 and p[1].endswith('state/loopstat'):
            fw = json.loads(p[2]).get('diag_onsets', fw)
    P = json.load(open(res))['passages']
    for p in P:
        if p['mm'] in PASSAGE_TIME_CORRECTIONS and not p['recognized'] and abs(p['t'] - PASSAGE_TIME_CORRECTIONS[p['mm']]) < 0.5:
            p['t'] = PASSAGE_TIME_CORRECTIONS[p['mm']]
    P.sort(key=lambda p: p['t'])
    tru = [(p['t'], p['pol']) for p in P]
    moving = [o for o in ons if o[5] > 0]
    stop = [o for o in ons if o[5] == 0]
    det, cls = r7.associate(moving, tru)
    cnt = collections.Counter(c[0] for c in cls)
    lead = [tru[j][0] - o[2] for j, o in det.items()]
    out = dict(replay_onsets=len(ons), firmware_diag_onsets=fw, passages=len(P), detected=len(det),
               onsets_moving=dict(cnt), onsets_at_stops=len(stop),
               stop_onsets=[dict(t=round(o[2], 3), pol=o[3], level=o[4]) for o in stop],
               false_moving=[dict(t=round(o[2], 3), pol=o[3], pwm=o[5], nearest_dt=round(c[2], 3)) for o, c in zip(moving, cls) if c[0] == 'false'],
               onset_before_peak_ms=dict(median=round(1e3 * float(np.median(lead))), min=round(1e3 * min(lead)), max=round(1e3 * max(lead))),
               missed_by_70x2=[dict(mm=P[j]['mm'], t=P[j]['t'], pwm=P[j]['pwm'], phase=P[j]['phase'], recognizer=P[j]['recognized'])
                               for j in range(len(P)) if j not in det])
    by_band = collections.OrderedDict()
    for lo, hi in ((0, 30), (30, 35), (35, 40), (40, 45), (45, 60), (60, 91), (91, 120), (120, 150), (150, 175), (175, 201)):
        js = [j for j in range(len(P)) if lo <= P[j]['pwm'] < hi]
        if js:
            by_band['%d-%d' % (lo, hi - 1)] = dict(passages=len(js), detected_70x2=sum(j in det for j in js),
                                                   recognized_waveform=sum(P[j]['recognized'] for j in js),
                                                   duplicates=sum(1 for c in cls if c[0] == 'duplicate' and c[1] in js),
                                                   trailing=sum(1 for c in cls if c[0] == 'trailing' and c[1] in js))
    out['by_pwm_band'] = by_band
    rm = []
    for j, p in enumerate(P):
        if p['recognized']:
            continue
        o = det.get(j)
        extra = [dict(t=round(x[2], 3), pol=x[3], cls=c[0]) for x, c in zip(moving, cls) if c[1] == j and x is not o]
        row = dict(mm=p['mm'], pol=p['pol'], t_peak=p['t'], pwm=p['pwm'], phase=p['phase'], detected=o is not None)
        if o is not None:
            row.update(onset_t=round(o[2], 3), onset_pol=o[3], onset_pwm=o[5], lead_ms=round(1e3 * (p['t'] - o[2])))
        else:
            row['max_dev_within_1s'] = level_trace_max_dev(native, t, p['t'])
        row['other_onsets_on_passage'] = extra
        rm.append(row)
    out['recognizer_misses'] = rm
    print(json.dumps({k: v for k, v in out.items() if k not in ('recognizer_misses', 'stop_onsets', 'by_pwm_band')}, indent=0))
    for k, v in by_band.items():
        print('PWM', k, v)
    for r in rm:
        print(json.dumps(r))
    print('stop onsets', json.dumps(out['stop_onsets']))
    if len(sys.argv) > 5:
        Path(sys.argv[5]).write_text(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
