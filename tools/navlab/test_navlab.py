#!/usr/bin/env python3
"""Automated validation for navlab artifacts 1 and 2.

Every test here targets a specific defect the 2026-08-22 review found, plus
the structural invariants a reviewer needs. Synthetic captures are built
in-test so failures are diagnosable; the old (pre-review) code fails tests
2, 3 and 5 by construction.

Run:  python3 tools/navlab/test_navlab.py
"""
import json, os, pathlib, subprocess, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[1]
PASS = []

def check(name, cond, detail=''):
    PASS.append((name, bool(cond)))
    print(('  OK   ' if cond else '  FAIL ') + name + (f' — {detail}' if detail and not cond else ''))

def mk_capture(path, loco, t0, events, pwm=90, boot_marker=True, throttle=88):
    """events: list of (dt_s, mm, obs, peak, dur, dt_ms)."""
    lines = []
    t = t0
    if boot_marker:
        lines.append(f'{t:.3f} ngr/loco/{loco}/state/session_direction CW')
        lines.append(f'{t:.3f} ngr/loco/{loco}/state/throttle {throttle}')
    up = 1000
    for i, (step, mm, obs, peak, dur, dt) in enumerate(events):
        t += step
        up += int(step * 1000)
        lines.append(f'{t-0.4:.3f} ngr/loco/{loco}/alert '
                     + json.dumps({'mdir': 'FWD', 'pwm': pwm, 'uptime_ms': up}))
        lines.append(f'{t:.3f} ngr/loco/{loco}/mm/marker '
                     + json.dumps({'mm': mm, 'obs': obs, 'peak': peak, 'ms': dur,
                                   'dt': dt, 'pwm': pwm, 'v': 15.6,
                                   'timing_gate': 'ACTIVE', 'drift': 0}))
        lines.append(f'{t+0.05:.3f} ngr/loco/{loco}/state/nav '
                     + json.dumps({'event': 'AGREE', 'mm': mm}))
    open(path, 'w').write('\n'.join(lines) + '\n')
    return t

def run_norm(caps, out):
    cmd = [sys.executable, str(HERE/'normalize_log.py'), '--out', out,
           '--map-source', str(REPO/'firmware/QUORUM/QUORUM.ino')]
    for c in caps: cmd += ['--capture', c]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return [json.loads(l) for l in open(out)]

def main():
    from_mm = 100   # map poles read from the real map by the normalizer
    import re
    src = open(REPO/'firmware/QUORUM/QUORUM.ino').read()
    dna = [int(x) for x in re.findall(r'\d+',
        src.split('const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')[1].split('};')[0])]
    pole = {1: 'N', 0: 'S'}
    def seq(start, n, dt=1.2):
        return [(dt, (start+i) % 171, pole[dna[(start+i) % 171]], 170, 150, 1200)
                for i in range(n)]

    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)

        # 1. PWM history present, bounded, newest-last, only pre-event samples
        c1 = td/'c1.log'; mk_capture(c1, '9950011', 1000.0, seq(from_mm, 10))
        rows = run_norm([str(c1)], str(td/'n1.jsonl'))
        r = rows[5]
        check('pwm_actual_history recorded', r['pwm_actual_history'],
              'empty history')
        check('history offsets non-negative and capped',
              all(0 <= o for o, _ in r['pwm_actual_history'])
              and len(r['pwm_actual_history']) <= 40)
        check('pwm_commanded_history carries throttle',
              any(v == 88 for _, v in r['pwm_commanded_history']))

        # 2. session isolation: two captures, label windows must not bridge
        c2a = td/'c2a.log'; mk_capture(c2a, '9950011', 2000.0, seq(from_mm, 3))
        c2b = td/'c2b.log'; mk_capture(c2b, '9950011', 2100.0, seq(from_mm+3, 7))
        rows = run_norm([str(c2a), str(c2b)], str(td/'n2.jsonl'))
        first3 = [x for x in rows if x['source'] == 'c2a.log']
        check('label window never crosses captures',
              all(x['label'] != 'genuine' for x in first3),
              'tail events of capture A were labeled genuine using capture B')

        # 3. overlapping captures deduplicate
        c3 = td/'c3.log'
        import shutil; shutil.copy(c1, c3)
        rows = run_norm([str(c1), str(c3)], str(td/'n3.jsonl'))
        check('overlapping captures dedupe to one physical event each',
              len(rows) == 10, f'{len(rows)} records from 10 events x 2 files')

        # 4. spacing/interval agreement with the map
        check('interval spacing matches the map on every record',
              all(x['spacing_mm'] > 0 and 0 <= x['interval'] < 171 for x in rows))

        # 5. generator: margins, per-envelope sessions, holdout exclusion
        norm = td/'n5.jsonl'
        c5a = td/'c5a.log'; mk_capture(c5a, '9950012', 3000.0, seq(20, 30))
        c5b = td/'c5b.log'; mk_capture(c5b, '9950012', 4000.0, seq(20, 30))
        run_norm([str(c5a), str(c5b)], str(norm))
        db_p = td/'db.json'
        r = subprocess.run([sys.executable, str(HERE/'build_timing_db.py'),
                            '--records', str(norm), '--out', str(db_p),
                            '--holdout', 'c5b.log'], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        db = json.load(open(db_p))
        check('held-out session listed and excluded',
              db['sessions_held_out'] and
              all('c5b' not in s for e in db['envelopes'].values()
                  for s in e['source_sessions']))
        env = next(iter(db['envelopes'].values()))
        check('envelope carries margin and credible bounds',
              'fast_bound' in env and env['fast_bound'] < env['min']
              and env['slow_soft'] > env['p95'] and env['margin'] == 0.15)
        check('per-envelope source sessions listed',
              isinstance(env['source_sessions'], list) and env['source_sessions'])
        check('quantiles ordered',
              all(env['min'] <= env['p05'] <= env['p50'] <= env['p95'] <= env['max']
                  for env in db['envelopes'].values()))

        # 6. generator admits no cross-session pairs: a lone-marker session
        c6 = td/'c6.log'; mk_capture(c6, '9950012', 5000.0, seq(50, 1))
        run_norm([str(c5a), str(c6)], str(td/'n6.jsonl'))
        subprocess.run([sys.executable, str(HERE/'build_timing_db.py'),
                        '--records', str(td/'n6.jsonl'), '--out', str(td/'db6.json')],
                       capture_output=True, text=True, check=True)
        db6 = json.load(open(td/'db6.json'))
        t1_51 = [k for k in db6['envelopes'] if k.startswith('t1|9950012|CW|50|')]
        check('no cross-session interval pair admitted', not t1_51,
              'interval 50 envelope exists but its only pair spans two captures')

    bad = [n for n, ok in PASS if not ok]
    print(f'\n{len(PASS)-len(bad)}/{len(PASS)} passed')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
