#!/usr/bin/env python3
"""Host replay of the 70x2 diagnostic onset detector as compiled in NAVI_WAVEFORM_0_1. Research only.

Ported line for line from the sketch (impl/navi-waveform-recognizer-manual-20261027):
  NaviDiagnosticLevel.h  ring of the last 1000 values passed to add(), median recomputed every
                         100 add() calls (serial % 100 == 0) once >= 100 values are held; level held between recomputes.
  NAVI_WAVEFORM_0_1.ino  hallTask passes EVERY native 5 kHz raw read to diagLevel.add() (not the
                         tick median, despite the header comment), so the window is 1000 raw reads
                         = 200 ms and the recompute period is 100 reads = 20 ms. add() runs before
                         onSample() on the same read, so a recompute applies to that read.
  NaviCandidate.h        onSample() is first called on the read where haveLevel() becomes true
                         (serial 100 after boot); its stride counter evaluates every 5th call from
                         there, i.e. reads with serial % 5 == 0. Onset = second of two consecutive
                         evaluated reads with |raw - level| >= 70, same sign, not already in an
                         excursion; any evaluated read under 70 ends the excursion.

The NSR native stream starts about 1 s after boot (serial ~4900), so the replay's level warms up on
the first 100 recorded reads instead of on boot data; the first 200 ms of each capture are reported
separately. A serial gap in a capture (three captures have one) is bridged without the missing
reads, which the loco itself did see.

Usage: python3 replay_70x2.py <oct27 npz dir> <Pi_Log_Downloads dir> [out.json]
"""
import collections
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'bank_20261027'))
sys.path.insert(0, str(HERE.parents[1] / 'navi_matched_filter'))
import mf            # noqa: E402
import replay_bank as rb  # noqa: E402

THR, WINDOW, MIN_SAMPLES, RECOMPUTE, STRIDE = 70.0, 1000, 100, 100, 5
LOGS = ('9950011_20261027_155920.log', '9950011_20261027_161304.log',
        '9950012_20261027_162642.log', '9950012_20261027_163657.log')
FOCUS = {('CA662AD9DC993994', 'MM169'): 70.31, ('CA662AD9DC993994', 'MM113'): 406.88,
         ('563C8E21CDA1CABF', 'MM037'): 273.44}


def detect70x2(native):
    """Return onsets [(index into native, serial, t_s, pol, level, pwm)] exactly as the sketch."""
    raw = native['raw'].astype(np.int64)
    serial = native['serial'].astype(np.int64)
    t = (native['t_us'].astype(np.int64) - int(native['t_us'][0])) / 1e6
    ring = np.zeros(WINDOW, dtype=np.int64)
    head = count = 0
    level, have = 0.0, False
    pending, active, psign = False, False, 1
    out = []
    for k in range(len(raw)):
        v = raw[k]
        ring[head] = v
        head = (head + 1) % WINDOW
        if count < WINDOW:
            count += 1
        # add() count since boot equals the serial (both start at 1 and advance together), so the
        # sketch recomputes on reads with serial % 100 == 0; keep that phase.
        if serial[k] % RECOMPUTE == 0:
            if count >= MIN_SAMPLES:
                vals = ring if count == WINDOW else ring[:count]
                level = float(np.partition(vals, count // 2)[count // 2])  # nth_element, upper median
                have = True
        if not have or serial[k] % STRIDE != 0:
            continue
        dev = float(v) - level
        if abs(dev) >= THR:
            sign = 1 if dev >= 0 else -1
            if not active and pending and psign == sign:
                out.append((k, int(serial[k]), float(t[k]), sign, level, int(native['pwm'][k])))
                active = True
            pending, psign = True, sign
        else:
            pending = active = False
    return out


def firmware_counts(logdir):
    """Last diag_onsets value per boot from state/loopstat (cumulative counter since boot)."""
    rows = []
    for f in LOGS:
        for line in open(Path(logdir) / f):
            p = line.rstrip('\n').split('\t', 2)
            if len(p) == 3:
                rows.append(p)
    rows.sort(key=lambda r: r[0])
    boot, last = {}, {}
    for ts, top, pl in rows:
        loco = top.split('/')[2]
        if top.endswith('state/bootid'):
            boot[loco] = json.loads(pl)['boot_id']
        elif top.endswith('state/loopstat') and loco in boot:
            last[boot[loco]] = json.loads(pl).get('diag_onsets')
    return last


def associate(onsets, tru):
    """Each onset is assigned to the magnet whose template peak is nearest in time (d = onset - peak).
    leading   : same polarity as the magnet, d <= +0.05 s (the departure from background).
    trailing  : opposite polarity, d > 0 (the 200 ms level has followed the magnet; the signal's
                return to background then reads as a departure of the other sign).
    duplicate : a second leading onset on the same magnet.
    false     : |d| > 0.6 s (no magnet near), or any other combination (wrong sign before the peak,
                same sign well after it).
    A magnet is detected if it has a leading onset; its detection time/polarity are that onset's."""
    tt = np.array([x[0] for x in tru])
    det, cls = {}, []
    for o in onsets:
        j = int(np.argmin(np.abs(tt - o[2])))
        d = o[2] - tt[j]
        same = o[3] == tru[j][1]
        if abs(d) > 0.6:
            c = 'false'
        elif same and d <= 0.05:
            c = 'duplicate' if j in det else 'leading'
            det.setdefault(j, o)
        elif not same and d > 0:
            c = 'trailing'
        else:
            c = 'false'
        cls.append((c, j, d))
    return det, cls


def main():
    oct27, logdir = Path(sys.argv[1]), sys.argv[2]
    fw = firmware_counts(logdir)
    res = {}
    for p in sorted(oct27.glob('*.npz')):
        boot = p.stem.split('_')[3]
        native = np.load(p)['native']
        ons = detect70x2(native)
        gaps = int((np.diff(native['serial'].astype(np.int64)) != 1).sum())
        rec = dict(label=rb.LABEL27.get(boot, boot), replay_onsets=len(ons), firmware_diag_onsets=fw.get(boot),
                   serial_gaps=gaps, onsets_first_200ms=sum(o[2] < 0.2 for o in ons),
                   stationary_onsets=[dict(t=round(o[2], 3), pol=o[3], level=o[4]) for o in ons if o[5] == 0])
        if boot in rb.DRIVEN27:
            tr = mf.condition(native)
            tru, _ = rb.truth27(tr, boot)
            det, cls = associate(ons, tru)
            mf_ev = rb.combine(rb.candidates(tr, (0.24,), 0.5), (0.24,), rb.uniform((0.24,)))
            lead = [tru[j][0] - o[2] for j, o in det.items()]
            ntrail = collections.Counter(c[1] for c in cls if c[0] == 'trailing')
            rec.update(truth_magnets=len(tru), detected=len(det),
                       missed=[dict(t_peak=round(tru[j][0], 2), pol=tru[j][1]) for j in range(len(tru)) if j not in det],
                       leading=sum(c[0] == 'leading' for c in cls),
                       trailing=sum(c[0] == 'trailing' for c in cls),
                       trailing_per_magnet=dict(sorted(collections.Counter(ntrail.get(j, 0) for j in range(len(tru))).items())),
                       false=[dict(t=round(o[2], 3), pol=o[3], pwm=o[5], level=o[4], nearest_peak_dt=round(c[2], 3))
                              for o, c in zip(ons, cls) if c[0] == 'false'],
                       duplicates=[dict(t=round(o[2], 3), pol=o[3], pwm=o[5], dt_from_peak=round(c[2], 3))
                                   for o, c in zip(ons, cls) if c[0] == 'duplicate'],
                       onset_before_peak_ms=dict(min=round(1e3 * min(lead)), median=round(1e3 * float(np.median(lead))),
                                                 max=round(1e3 * max(lead))) if lead else None,
                       mf240_events=len(mf_ev))
            focus = {}
            for (b, mm), tp in FOCUS.items():
                if b != boot:
                    continue
                j = int(np.argmin(np.abs(np.array([x[0] for x in tru]) - tp)))
                o = det.get(j)
                near = [dict(t=round(x[2], 3), pol=x[3], pwm=x[5], level=x[4]) for x in ons if abs(x[2] - tp) < 1.5]
                i0 = int(np.searchsorted((native['t_us'] - native['t_us'][0]) / 1e6, tp - 1.0))
                i1 = int(np.searchsorted((native['t_us'] - native['t_us'][0]) / 1e6, tp + 1.0))
                seg = native['raw'][i0:i1].astype(float)
                focus[mm] = dict(truth_t_peak=round(tru[j][0], 3), truth_pol=tru[j][1],
                                 detected=o is not None,
                                 onset=None if o is None else dict(t=round(o[2], 3), serial=o[1], pol=o[3], pwm=o[5],
                                                                   level=o[4], lead_ms=round(1e3 * (tru[j][0] - o[2]))),
                                 onsets_within_1_5s=near,
                                 raw_min_max_within_1s=[int(seg.min()), int(seg.max())])
            rec['focus'] = focus
        res[boot] = rec
        print(boot, json.dumps(rec, indent=1))
    if len(sys.argv) > 3:
        Path(sys.argv[3]).write_text(json.dumps(res, indent=1))


if __name__ == '__main__':
    main()
