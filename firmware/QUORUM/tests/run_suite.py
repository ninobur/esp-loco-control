#!/usr/bin/env python3
"""QUORUM replay regression suite.

Builds the host harness (real firmware, host-compiled), then runs:

  1. FIDELITY + REGRESSION — the whole 2026-08-10 session replayed against
     what the locomotive actually published.
  2. INCIDENT GOLDENS — A, B and C asserted individually.
  3. EVIDENCE PROPERTIES — what the DNA map says about each incident's ring,
     independent of firmware. These are the facts an advisory would rely on,
     checked here so a map change cannot silently invalidate it.
  4. SYNTHETIC CASES — negatives the capture does not contain, plus the
     legitimate-recovery control.

Usage:  python3 run_suite.py [--harness /path/to/binary]
"""

import argparse
import json
import pathlib
import subprocess
import sys

import qrun

HERE = pathlib.Path(__file__).resolve().parent
CAPTURE = HERE.parent.parent.parent / 'field-records/logs/20260810_IR_SPEED_LOCAL_1_2_otto.log'

DNA = [
    1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0, 0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
    0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1, 0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
    1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0, 1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
    1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0, 0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
    1,0,1,0,0,0,0,1,1,1,0,
]
DNA_N = 171
REACQ_WINDOW_MARKERS = 5      # mirrors QUORUM.ino:1349
DNA_W = 12                    # mirrors QUORUM.ino:1348

# The three HARD_BOUND incidents, as published by the locomotive.
INCIDENTS = {
    'A': dict(line=14945, mm=100, scores=[7, 3, 7, 8, 4, 6], leader=2,
              runner_up=-1, margin=1,
              ring='N S S N S S N S S S N N', true_offset=8, true_mm=108),
    'B': dict(line=19608, mm=87, scores=[3, 6, 6, 5, 5, 6], leader=0,
              runner_up=1, margin=0,
              ring='S S S S N S S N S S N N', true_offset=None, true_mm=None),
    'C': dict(line=25092, mm=23, scores=[5, 3, 5, 5, 6, 6], leader=3,
              runner_up=4, margin=0,
              ring='S S S S N N N N S S N S', true_offset=-5, true_mm=18),
}


def build(harness_path):
    src = HERE / 'harness.cpp'
    cmd = ['clang++', '-std=c++17', '-Wno-format',
           '-I', str(HERE / 'shim'), '-I', str(HERE.parent),
           '-o', harness_path, str(src)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-4000:])
        sys.exit('harness build FAILED')
    warn = [l for l in r.stderr.splitlines() if 'warning:' in l]
    return warn


# --- evidence properties, computed from the map ----------------------------
def exact_matches(ring_bits, window=None, nav_mm=None):
    """Markers whose preceding DNA_W window matches the ring exactly.

    window=None searches the whole route; an integer searches +/-window of
    nav_mm, which is what dnaMatch() does with REACQ_WINDOW_MARKERS.
    """
    hits = []
    ends = range(DNA_N) if window is None else [
        (nav_mm + off) % DNA_N for off in range(-window, window + 1)]
    for end in ends:
        start = (end - (DNA_W - 1)) % DNA_N
        if all(DNA[(start + i) % DNA_N] == ring_bits[i] for i in range(DNA_W)):
            hits.append(end)
    return sorted(set(hits))


def ring_bits(ring):
    return [1 if c == 'N' else 0 for c in ring.split()]


def check_evidence_properties():
    """Facts about the map that any advisory design depends on."""
    fails = []
    # Window uniqueness: the premise of exact-or-silent.
    for w in (9, 10, 12):
        seen = {}
        for s in range(DNA_N):
            seen.setdefault(tuple(DNA[(s + i) % DNA_N] for i in range(w)), []).append(s)
        dup = sum(1 for v in seen.values() if len(v) > 1)
        if w == 9 and dup == 0:
            fails.append('W=9 is unique — the collision case cannot be '
                         'demonstrated; re-derive the safe window width')
        if w in (10, 12) and dup != 0:
            fails.append(f'W={w} has {dup} collisions — exact-or-silent is '
                         'NOT safe at this window width')
        print(f'    DNA windows W={w:>2}: {len(seen)} distinct of {DNA_N}, '
              f'{dup} collisions')

    for tag, inc in INCIDENTS.items():
        bits = ring_bits(inc['ring'])
        allhits = exact_matches(bits)
        bounded = exact_matches(bits, window=REACQ_WINDOW_MARKERS,
                                nav_mm=inc['mm'])
        exp_all = [inc['true_mm']] if inc['true_mm'] is not None else []
        if allhits != exp_all:
            fails.append(f'incident {tag}: route-wide exact match {allhits}, '
                         f'expected {exp_all}')
        print(f'    incident {tag}: route-wide exact match {allhits or "none"}, '
              f'bounded +/-{REACQ_WINDOW_MARKERS} {bounded or "none"}')
    return fails


def check_incident(tag, decisions):
    """Assert one incident's published NO_QUORUM decision."""
    inc = INCIDENTS[tag]
    got = [d for d in decisions
           if d['event'] == 'NO_QUORUM' and d.get('mm') == inc['mm']]
    if len(got) != 1:
        return [f'incident {tag}: expected exactly one NO_QUORUM at mm '
                f'{inc["mm"]}, got {len(got)}']
    d = got[0]
    fails = []
    for k in ('scores', 'leader', 'runner_up', 'margin'):
        if d.get(k) != inc[k]:
            fails.append(f'incident {tag}: {k} = {d.get(k)}, expected {inc[k]}')
    if d.get('reason') != 'HARD_BOUND':
        fails.append(f'incident {tag}: reason = {d.get("reason")}, '
                     'expected HARD_BOUND')
    if d.get('eval') != 12:
        fails.append(f'incident {tag}: eval = {d.get("eval")}, expected 12')
    return fails


def check_synthetic(harness, name, spec):
    su = qrun.summarise(qrun.run_file(harness, HERE / 'fixtures' / f'{name}.replay'))
    ev, final = su['events'], su['final']
    exp, fails = spec['expect'], []
    for want in exp.get('must_contain', []):
        if want not in ev:
            fails.append(f'{name}: missing {want} (got {sorted(set(ev))})')
    for bad in exp.get('must_not_contain', []):
        if bad in ev:
            fails.append(f'{name}: produced {bad}, which must never happen')
    if 'adopted_offset' in exp:
        ad = [d for d in su['decisions'] if d['event'] == 'QUORUM_ADOPTED']
        got = ad[0].get('offset') if ad else None
        if got != exp['adopted_offset']:
            fails.append(f'{name}: adopted offset {got}, '
                         f'expected {exp["adopted_offset"]}')
    if 'final_state' in exp and final and final['state'] != exp['final_state']:
        fails.append(f'{name}: final state {final["state"]}, '
                     f'expected {exp["final_state"]}')
    if 'final_mm' in exp and final and final['mm'] != exp['final_mm']:
        fails.append(f'{name}: final mm {final["mm"]}, expected {exp["final_mm"]}')
    if 'final_dir' in exp and final and final['dir'] != exp['final_dir']:
        fails.append(f'{name}: final dir {final["dir"]}, expected {exp["final_dir"]}')
    if 'snapshot_desired' in exp:
        snaps = su['snapshots']
        got = snaps[-1]['snapshot_desired'] if snaps else None
        if got != exp['snapshot_desired']:
            fails.append(f'{name}: snapshot_desired {got}, '
                         f'expected {exp["snapshot_desired"]}')
    if 'advisory' in exp:
        adv = advisory_of(su)
        if adv != exp['advisory']:
            fails.append(f'{name}: advisory {adv}, expected {exp["advisory"]} '
                         '— a wrong non-null advisory is a blocking defect')
    return fails, sorted(set(ev))


def advisory_of(summary):
    """The advisory marker in the retained snapshot, or None.

    Absent until Phase 2 adds it; None is then the correct reading for
    'advised nothing'.
    """
    for s in reversed(summary['snapshots']):
        try:
            d = json.loads(s['snapshot']) if s['snapshot'] else {}
        except ValueError:
            continue
        if 'adv' in d:
            return d['adv']
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--harness', default='/tmp/quorum_harness')
    args = ap.parse_args()
    failures = []

    print('== build ==')
    warn = build(args.harness)
    print(f'    harness built, {len(warn)} warnings')
    for w in warn[:5]:
        print(f'      {w}')

    print('\n== regenerate fixtures ==')
    for script in ('extract_fixture.py', 'make_synthetic.py'):
        r = subprocess.run([sys.executable, str(HERE / script)],
                           capture_output=True, text=True, cwd=HERE)
        if r.returncode != 0:
            print(r.stdout, r.stderr)
            sys.exit(f'{script} FAILED')
        for line in r.stdout.strip().splitlines():
            print(f'    {line}')

    print('\n== 1. fidelity + regression: full 2026-08-10 session ==')
    r = subprocess.run([sys.executable, str(HERE / 'verify_replay.py'),
                        '--harness', args.harness,
                        '--fixture', str(HERE / 'fixtures/full_run')],
                       capture_output=True, text=True, cwd=HERE)
    print(r.stdout.rstrip())
    if r.returncode != 0:
        failures.append('full_run replay does not match the capture')

    su = qrun.summarise(qrun.run_file(args.harness,
                                      HERE / 'fixtures/full_run.replay'))
    decisions = su['decisions']

    print('\n== 2. incident goldens ==')
    for tag in ('A', 'B', 'C'):
        f = check_incident(tag, decisions)
        inc = INCIDENTS[tag]
        print(f'    incident {tag} (capture line {inc["line"]}, mm {inc["mm"]}): '
              + ('OK' if not f else 'FAIL'))
        failures += f
    n_adopt = sum(1 for d in decisions if d['event'] == 'QUORUM_ADOPTED')
    if n_adopt != 1:
        failures.append(f'expected exactly 1 adoption in the run, got {n_adopt}')
    print(f'    adoptions in the run: {n_adopt} (the one at capture line 24800, '
          'which was wrong)')

    print('\n== 3. evidence properties (map, not firmware) ==')
    failures += check_evidence_properties()

    print('\n== 4. synthetic cases ==')
    manifest = json.loads(
        (HERE / 'fixtures/synthetic_manifest.json').read_text())
    for name, spec in sorted(manifest.items()):
        f, ev = check_synthetic(args.harness, name, spec)
        print(f'    {"OK  " if not f else "FAIL"} {name}: {ev}')
        failures += f

    print('\n== result ==')
    if failures:
        for f in failures:
            print(f'    FAIL {f}')
        print(f'\n{len(failures)} failure(s)')
        return 1
    print('    all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
