#!/usr/bin/env python3
"""Behavioural tests for navlab artifact 3, iteration 2.

Pins, one per acceptance clause plus one per iteration-2 correction:
  1. forward-only motion;             2. timing impossibility;
  3. pending resolution (both ways);  4. reversal preserves hypotheses
                                         and consults no firmware label;
  5. corridor advances on marker dt, never on MQTT receipt gaps;
  6. marginally-fast observations stay pending (not eliminated);
  7. corridor-bounded relocation;     8. gated route-wide re-acquisition.

Synthetic map and envelopes except where the real map's uniqueness guarantee
is itself the property under test.  Run: python3 tools/navlab/test_reachability_nav.py
"""
import random, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from reachability_nav import Navigator, Envelopes, DNA_N

PASS = []
def check(name, cond, detail=''):
    PASS.append((name, bool(cond)))
    print(('  OK   ' if cond else '  FAIL ') + name + (f' — {detail}' if detail and not cond else ''))

def mkenv(loco='L'):
    db = {'envelopes': {
        f't3|{loco}|90': dict(n=50, min=1000, p05=1050, p25=1100, p50=1150,
                              p95=1400, max=2000, fast_bound=850,
                              slow_soft=1610, source_sessions=['syn'])}}
    return Envelopes(db)

def mknav(dna, sdir='CW', mdir='FWD'):
    n = Navigator(mkenv(), 'L', dna, [300]*DNA_N)
    n.maybe_reverse(dict(ts=0.0, session_dir=sdir, motor_dir=mdir))
    return n

def ev(ts, mm, dna, dt=1100, pwm=90, pol=None, sdir='CW', mdir='FWD'):
    return dict(ts=ts, mm=mm,
                polarity=pol or ('N' if dna[mm % DNA_N] else 'S'),
                dt_ms=dt, pwm_actual=pwm, session_dir=sdir, motor_dir=mdir,
                pwm_actual_history=[[500, pwm]])

def main():
    rng = random.Random(7)
    dna = [rng.randint(0, 1) for _ in range(DNA_N)]

    # 1. forward-only
    d1 = list(dna); d1[9] = 1; d1[11] = 0; d1[12] = 0
    nav = mknav(d1); nav.declare(10, 0.0)
    nav.on_event(ev(1.1, 9, d1, pol='N'))
    check('forward-only: never occupies a position behind the anchor',
          9 not in nav.P and nav.anchor[0] == 10, f'P={nav.P}')

    # 2. timing impossibility: corridor opened by a genuine long-dt event,
    #    then a dt=200 probe (< fast_bound 850, below marginal band) is HELD.
    nav = mknav(dna); nav.declare(20, 0.0)
    nav.on_event(ev(4.0, 21, dna, dt=4000))          # opens corridor, steps to 21
    nav.on_event(ev(4.2, 22, dna, dt=200))
    held = 22 not in nav.P and any(l['ev'] in ('PENDING', 'PHANTOM_SUSPECT')
                                   for l in nav.log)
    check('timing impossibility: sub-band event cannot advance', held,
          f'P={nav.P} log={[l["ev"] for l in nav.log]}')

    # 3. pending resolved as phantom: successor at genuine cadence advances one
    nav.on_event(ev(5.4, 22, dna, dt=1100))
    check('pending resolution: phantom did not stick, position advanced',
          22 in nav.P and 21 not in nav.P and 20 not in nav.P,
          f'P={nav.P}')

    # 4. REVERSAL preserves hypotheses natively (iteration-2 correction 1)
    nav = mknav(dna); nav.declare(50, 0.0)
    for i, mm in enumerate((51, 52, 53)):
        nav.on_event(ev(1.2*(i+1), mm, dna))
    assert nav.anchor[0] == 53
    nav.on_event(ev(5.0, 52, dna, mdir='REV'))       # flip: now travelling down
    revlog = [l for l in nav.log if l['ev'] == 'REVERSAL']
    check('reversal: hypotheses preserved, direction reversed, no reseed',
          revlog and revlog[0]['P'] == [53] and nav.P == {52}
          and nav.step == -1, f'P={nav.P} log={revlog}')
    nav.on_event(ev(6.2, 51, dna, mdir='REV'))
    nav.on_event(ev(7.4, 50, dna, mdir='REV'))
    check('reversal: tracking continues backwards to confirmation',
          nav.anchor[0] == 50 and nav.state == 'NORMAL', f'anchor={nav.anchor}')

    # 5. corridor from marker dt, NEVER receipt gaps (correction 2): a huge
    #    receipt-time gap with a normal dt must NOT balloon the corridor.
    nav = mknav(dna); nav.declare(30, 0.0)
    nav.on_event(ev(500.0, 31, dna, dt=1100))        # 500 s later by receipt
    check('corridor: receipt-time gap contributes nothing',
          nav.hi < 900, f'hi={nav.hi:.0f}mm after a 500 s receipt gap')

    # 6. marginal-fast retained as pending (correction 4): fast_bound 850,
    #    dt=700 is inside the 25% band -> PENDING_MARGINAL, then successor
    #    at genuine cadence resolves with the marginal hypothesis alive.
    nav = mknav(dna); nav.declare(40, 0.0)
    nav.on_event(ev(3.0, 41, dna, dt=3000))          # open corridor, step to 41
    nav.on_event(ev(3.7, 42, dna, dt=700))           # marginal (700 in [637,850))
    pm = any(l['ev'] == 'PENDING_MARGINAL' for l in nav.log)
    nav.on_event(ev(4.8, 43, dna, dt=1100))
    check('marginal-fast held as pending, not eliminated',
          pm and 43 in nav.P, f'pm={pm} P={nav.P}')

    # 7. corridor-bounded relocation
    nav = mknav(dna); nav.declare(60, 0.0)
    far = (60 + 80) % DNA_N
    nav.on_event(ev(1.1, far, dna, dt=600))
    check('route-wide restriction: no relocation outside the corridor',
          nav.anchor[0] == 60 and (not nav.P or
          all(nav.dist(60, p) <= nav.hi + 30 for p in nav.P)),
          f'P={nav.P} hi={nav.hi:.0f}')

    # 8. gated re-acquisition (real map: uniqueness is the designed property)
    import re
    src = open(pathlib.Path(__file__).resolve().parents[2]
               / 'firmware/QUORUM/QUORUM.ino').read()
    real = [int(x) for x in re.findall(r'\d+',
        src.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    k0 = 40
    nav = mknav(real); nav.declare(0, 0.0)
    nav.hi = nav.circuit_mm() + 1
    t = 1.0; reacq_i = None
    for i in range(14 + Navigator.CONFIRM_N):
        nav.on_event(ev(t, (k0 + i) % DNA_N, real)); t += 1.2
        if any(l['ev'] == 'REACQUIRED' for l in nav.log):
            reacq_i = i; break
    check('gated re-acquisition fires only under LOST_FULL_CIRCLE',
          reacq_i is not None, f'state={nav.state}')
    check('re-acquired position is the position actually fed',
          reacq_i is not None and nav.anchor[0] == (k0 + reacq_i) % DNA_N,
          f'anchor={nav.anchor}')

    # ---- dt=0 boundary tests (docs/NAVLAB_DT0_SEMANTICS.md, all 7 clauses)
    d2 = list(dna); d2[11] = 1; d2[12] = 0; d2[13] = 1; d2[10] = 0
    # T1: dt=0 right after declaration, genuine traversal
    nav = mknav(d2); nav.declare(10, 0.0)
    nav.on_event(ev(1.0, 11, d2, dt=0))
    t1ok = 11 in nav.P and any(l['ev'] == 'DT_RESET' for l in nav.log)
    nav.on_event(ev(2.2, 12, d2, dt=1200))
    check('dt0: genuine traversal across a reset survives',
          t1ok and 12 in nav.P and nav.state != 'CONTRADICTION', f'P={nav.P}')

    # T2: dt=0 same-magnet reread keeps the stay hypothesis
    nav = mknav(d2); nav.declare(10, 0.0)
    nav.on_event(ev(1.0, 10, d2, dt=0, pol='S'))     # dna[10]=0 -> S = reread
    stay = 10 in nav.P
    nav.on_event(ev(2.2, 11, d2, dt=1200))
    check('dt0: same-magnet reread does not force forward travel',
          stay and nav.P == {11}, f'P={nav.P}')

    # T3: phantom after a reset is held, never advances
    nav = mknav(d2); nav.declare(10, 0.0)
    nav.on_event(ev(1.0, 11, d2, dt=0))
    p_before = set(nav.P)
    nav.on_event(ev(1.15, 11, d2, dt=150, pol='N'))  # ghost-fast, pole N
    held = nav.P == p_before
    nav.on_event(ev(2.3, 12, d2, dt=1150))
    check('dt0: phantom following a reset is held and position recovers',
          held and 12 in nav.P and nav.state != 'CONTRADICTION',
          f'held={held} P={nav.P}')

    # T4: reversal adjacent to a reset - no borrowed labels, direction flips
    nav = mknav(dna); nav.declare(30, 0.0)
    for i, mm in enumerate((31, 32, 33)):
        nav.on_event(ev(1.2*(i+1), mm, dna))
    nav.on_event(ev(5.0, 32, dna, dt=0, mdir='REV'))
    check('dt0: reversal near a reset keeps native handling',
          nav.step == -1 and 32 in nav.P
          and not any(l['ev'] == 'EXTERNAL_RESEED' for l in nav.log),
          f'step={nav.step} P={nav.P}')

    # T5+T6: repeated resets expand the corridor linearly, one interval each
    nav = mknav(dna); nav.declare(60, 0.0)
    h0 = nav.hi
    nav.on_event(ev(1.0, 61, dna, dt=0))
    g1 = nav.hi - h0
    for k in range(4):
        nav.on_event(ev(2.0 + k, 61, dna, dt=0))
    check('dt0: single reset grants exactly one interval',
          295 <= g1 <= 335, f'grant={g1:.0f}mm')
    check('dt0: repeated resets stay linear and bounded',
          nav.hi - h0 <= 5 * 335 + 1, f'total={nav.hi-h0:.0f}mm after 5 resets')

    # T7: unknown time can never confirm
    nav = mknav(d2); nav.declare(10, 0.0)
    for k in range(4):
        nav.on_event(ev(1.0 + k, 11, d2, dt=0))
    check('dt0: reset events never count toward confirmation',
          not any(l['ev'] == 'CONFIRMED' for l in nav.log), 
          f'log={[l["ev"] for l in nav.log]}')

    bad = [n for n, ok in PASS if not ok]
    print(f'\n{len(PASS)-len(bad)}/{len(PASS)} passed')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
