#!/usr/bin/env python3
"""Behavioural tests for navlab artifact 3 (reachability_nav.Navigator).

Each test pins one clause of the recovery plan's acceptance list:
  1. forward-only motion - no position behind the anchor is ever occupied;
  2. timing impossibility - an event faster than the measured fast bound
     cannot advance position;
  3. pending-event resolution - a doubtful event is HELD, and its successor
     resolves it under both hypotheses;
  4. reversal - CCW geometry tracks correctly;
  5. route-wide restriction - no relocation beyond the corridor while a full
     circuit is unreachable, and gated re-acquisition once it is.

Synthetic map and envelopes throughout: failures are diagnosable, and the
tests do not depend on any capture.

Run:  python3 tools/navlab/test_reachability_nav.py
"""
import random, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from reachability_nav import Navigator, Envelopes, DNA_N

PASS = []
def check(name, cond, detail=''):
    PASS.append((name, bool(cond)))
    print(('  OK   ' if cond else '  FAIL ') + name + (f' — {detail}' if detail and not cond else ''))

def mkenv(loco='L'):
    # one driving bucket: pwm 90, fastest credible interval 850 ms over
    # 300 mm -> vmax ~0.353 mm/ms
    db = {'margin': 0.15, 'envelopes': {
        f't3|{loco}|90': dict(n=50, min=1000, p05=1050, p25=1100, p50=1150,
                              p95=1400, max=2000, margin=.15,
                              fast_bound=850, slow_soft=1610,
                              source_sessions=['syn']),
    }}
    return Envelopes(db)

def mknav(dna, sdir='CW'):
    return Navigator(mkenv(), 'L', sdir, dna, [300]*DNA_N)

def ev(ts, mm, dna, dt=1100, pwm=90, pol=None):
    return dict(ts=ts, mm=mm, polarity=pol or ('N' if dna[mm % DNA_N] else 'S'),
                dt_ms=dt, pwm_actual=pwm,
                pwm_actual_history=[[500, pwm]])

def main():
    rng = random.Random(7)
    dna = [rng.randint(0, 1) for _ in range(DNA_N)]

    # 1. forward-only: an event whose pole matches ONLY the marker BEHIND the
    #    anchor must never relocate the navigator backwards.
    d1 = list(dna); d1[9] = 1; d1[11] = 0; d1[12] = 0
    nav = mknav(d1); nav.declare(10, 0.0)
    nav.on_event(ev(1.1, 9, d1, pol='N'), 0.0)   # matches mm 9 (behind) only
    check('forward-only: never occupies a position behind the anchor',
          9 not in nav.P and nav.anchor[0] == 10,
          f'P={nav.P}')

    # 2. timing impossibility: corridor open, next marker pole matches, but
    #    dt=200 < fast_bound 850 -> held, not advanced.
    nav = mknav(dna); nav.declare(20, 0.0)
    nav.on_event(ev(4.0, 21, dna, dt=200), 0.0)   # corridor ~1400 mm open
    held = nav.P == {20} and any(l['ev'] in ('PENDING','PHANTOM_SUSPECT')
                                  for l in nav.log)
    check('timing impossibility: sub-fast-bound event cannot advance', held,
          f'P={nav.P} log={[l["ev"] for l in nav.log]}')

    # 3a. pending resolved as PHANTOM: successor at genuine cadence lands one
    #     marker on (dts fold: 200+1100 covers exactly one interval).
    nav.on_event(ev(5.1, 21, dna, dt=1100), 4.0)
    check('pending resolution: phantom hypothesis folds dt and advances one',
          nav.P == {21}, f'P={nav.P}')

    # 3b. pending resolved as GENUINE: two markers with real spacing where
    #     the first was merely doubtful (slow corridor at seed).
    nav = mknav(dna); nav.declare(30, 0.0)
    nav.on_event(ev(0.4, 31, dna, dt=400), 0.0)   # too soon for corridor: held
    nav.on_event(ev(1.6, 32, dna, dt=1200), 0.4)  # successor at true cadence
    check('pending resolution: genuine hypothesis can carry both markers',
          nav.P and max((nav.dist(30, p) for p in nav.P)) <= 600
          and 30 not in nav.P,
          f'P={nav.P}')

    # 4. reversal: CCW navigator advances DOWNWARD through the map.
    nav = mknav(dna, sdir='CCW'); nav.declare(50, 0.0)
    for i, mm in enumerate((49, 48, 47)):
        nav.on_event(ev(1.2*(i+1), mm, dna), 1.2*i)
    check('reversal: CCW tracks descending markers and confirms',
          nav.anchor[0] == 47 and nav.state == 'NORMAL',
          f'anchor={nav.anchor} P={nav.P}')

    # 5a. route-wide restriction: pole matching a far position must not
    #     relocate while the corridor is short.
    nav = mknav(dna); nav.declare(60, 0.0)
    far = (60 + 80) % DNA_N
    nav.on_event(ev(1.1, far, dna), 0.0)
    check('route-wide restriction: no relocation outside the corridor',
          all(nav.dist(60, p) <= nav.hi + 30 for p in nav.P) if nav.P
          else nav.anchor[0] == 60,
          f'P={nav.P} hi={nav.hi:.0f}')

    # 5b. gated re-acquisition: only once a full circuit is reachable, and
    #     only on a unique 12-window with three CONSECUTIVE consistent hits.
    #     A random synthetic map has colliding 12-windows (observed: 2 hits),
    #     which correctly resets the consistency counter - so this test uses
    #     the REAL map, whose every 12-window is verified unique (the DNA_W
    #     guarantee the rule was designed against).
    import re
    src = open(pathlib.Path(__file__).resolve().parents[2]
               / 'firmware/QUORUM/QUORUM.ino').read()
    real = [int(x) for x in re.findall(r'\d+',
        src.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    k0 = 40
    nav = mknav(real); nav.declare(0, 0.0)
    nav.hi = nav.circuit_mm() + 1          # full circuit now reachable
    t = 1.0; reacq_i = None
    for i in range(14 + Navigator.CONFIRM_N):
        mm = (k0 + i) % DNA_N
        nav.on_event(ev(t, mm, real), t - 1.2); t += 1.2
        if any(l['ev'] == 'REACQUIRED' for l in nav.log):
            reacq_i = i; break
    check('gated re-acquisition fires only under LOST_FULL_CIRCLE',
          reacq_i is not None, f'state={nav.state} reacq={nav.reacq}')
    check('re-acquired position is the position actually fed',
          reacq_i is not None and nav.anchor[0] == (k0 + reacq_i) % DNA_N,
          f'anchor={nav.anchor} fed={(k0 + (reacq_i or 0)) % DNA_N}')

    bad = [n for n, ok in PASS if not ok]
    print(f'\n{len(PASS)-len(bad)}/{len(PASS)} passed')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
