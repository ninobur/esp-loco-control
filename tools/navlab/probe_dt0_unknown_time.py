#!/usr/bin/env python3
"""Development probe: is the dt=0 one-interval grant fail-safe?

Iteration-3 correction item 3. `docs/NAVLAB_DT0_SEMANTICS.md` calls the
one-interval corridor grant "bounded" and "fail safe". A dt-chain reset means
traversal time is UNKNOWN; it does not establish that only one interval
elapsed. This probe asks what the navigator does when the unknown time in fact
covered several intervals - the missed-marker / prolonged-unknown-time case.

It is a DEVELOPMENT PROBE, not validation: the events are synthetic and the
map/spacing are the real committed ones. It changes nothing; it reports.

  python3 tools/navlab/probe_dt0_unknown_time.py [--db tools/navlab/db/timing_db_v1.json]
"""
import argparse, json, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from reachability_nav import Navigator, Envelopes, DNA_N


def real_map():
    src = open(pathlib.Path(__file__).resolve().parents[2]
               / 'firmware/QUORUM/QUORUM.ino').read()
    dna = [int(x) for x in re.findall(r'\d+',
        src.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    spc = [int(x) for x in re.findall(r'\d+',
        src.split('static const uint16_t spacingMm[DNA_N] PROGMEM = {')[1].split('};')[0])]
    return dna, spc


def ev(ts, mm, dna, dt, pwm=60):
    return dict(ts=ts, mm=mm, polarity='N' if dna[mm % DNA_N] else 'S',
                dt_ms=dt, pwm_actual=pwm, session_dir='CW', motor_dir='FWD',
                pwm_actual_history=[[dt + 500, pwm], [0, pwm]])


def trial(env, dna, spc, loco, anchor, gap, n_after=6, dt=1500, control=False):
    """Confirmed at `anchor`; the dt-chain resets while the locomotive has in
    fact advanced `gap` intervals; then it keeps running normally.

    control=True runs the SAME event stream with no reset at all, so that any
    misbehaviour can be attributed to the dt=0 rule rather than to the
    navigator's ordinary corridor/collapse behaviour."""
    nav = Navigator(env, loco, dna, spc)
    nav.maybe_reverse(dict(ts=0.0, session_dir='CW', motor_dir='FWD'))
    nav.declare(anchor, 0.0)
    true_mm = (anchor + gap) % DNA_N
    if control:
        # no reset: the locomotive simply runs on from the confirmed anchor
        true_mm = anchor
    else:
        nav.on_event(ev(10.0, true_mm, dna, dt=0))
    t = 10.0
    seen = 0
    confs = []                     # (confirmed mm, TRUE mm at that instant)
    for i in range(1, n_after + 1):
        t += dt / 1000.0
        true_mm = (true_mm + 1) % DNA_N
        nav.on_event(ev(t, true_mm, dna, dt=dt))
        if nav.state == 'CONTRADICTION':
            return dict(outcome='CONTRADICTION', at_event=i, true_mm=true_mm)
        new = [l for l in nav.log if l['ev'] == 'CONFIRMED'][seen:]
        seen += len(new)
        # every confirmation is judged against the position the locomotive
        # actually occupied when that event was emitted - not against where it
        # ended up later.
        confs += [(c['mm'], true_mm) for c in new]
    if confs:
        wrong = [(m, tm) for m, tm in confs if m != tm]
        if wrong:
            m, tm = wrong[0]
            err = (m - tm) % DNA_N
            return dict(outcome='CONFIRMED', mm=m, true_mm=tm,
                        error_markers=min(err, DNA_N - err), correct=False,
                        confirmations=len(confs))
        return dict(outcome='CONFIRMED', mm=confs[-1][0], true_mm=confs[-1][1],
                    error_markers=0, correct=True, confirmations=len(confs))
    return dict(outcome='UNRESOLVED', true_mm=true_mm,
                P=sorted(nav.P)[:8], state=nav.state)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--db', default=str(pathlib.Path(__file__).resolve().parent
                                        / 'db/timing_db_v1.json'))
    ap.add_argument('--loco', default='9950011')
    ap.add_argument('--out')
    args = ap.parse_args()
    dna, spc = real_map()
    env = Envelopes(json.load(open(args.db)))

    rows = []
    for gap in ('control', 0, 1, 2, 3, 4, 6, 10):
        res = [trial(env, dna, spc, args.loco, a, 0 if gap == 'control' else gap,
                     control=(gap == 'control')) for a in range(DNA_N)]
        false_conf = [r for r in res if r['outcome'] == 'CONFIRMED' and not r['correct']]
        good_conf = [r for r in res if r['outcome'] == 'CONFIRMED' and r['correct']]
        stops = [r for r in res if r['outcome'] == 'CONTRADICTION']
        unres = [r for r in res if r['outcome'] == 'UNRESOLVED']
        errs = sorted({r['error_markers'] for r in false_conf})
        rows.append(dict(true_intervals_during_unknown_time=gap,
                         anchors_tried=len(res),
                         confirmed_correct=len(good_conf),
                         confirmed_WRONG=len(false_conf),
                         stopped_contradiction=len(stops),
                         unresolved=len(unres),
                         wrong_position_error_markers=errs))
        print('gap=%-7s  correct %-4d  FALSE CONFIRMATIONS %-4d  stopped %-4d  '
              'unresolved %-4d  error sizes %s'
              % (str(gap), len(good_conf), len(false_conf), len(stops), len(unres), errs))

    verdict = ('REFUTED: the one-interval grant is not fail-safe. When the '
               'unknown time covered more than one interval, the true position '
               'lay outside the granted corridor and the navigator went on to '
               'CONFIRM a position behind the locomotive rather than stopping.'
               if any(r['confirmed_WRONG'] for r in rows) else
               'not refuted by this probe')
    print('\n' + verdict)
    if args.out:
        json.dump(dict(probe='dt0_unknown_time', kind='development probe, '
                       'synthetic events on the real committed map',
                       loco=args.loco, dt_ms_after_reset=1500, pwm=60,
                       rows=rows, verdict=verdict), open(args.out, 'w'), indent=1)
        print('->', args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
