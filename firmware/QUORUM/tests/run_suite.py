#!/usr/bin/env python3
"""QUORUM replay regression suite.

Builds the host harness (real firmware, host-compiled), then runs:

  1. FIDELITY + REGRESSION — the whole 2026-08-10 session replayed against
     what the locomotive actually published.
  2. INCIDENT GOLDENS — A, B and C asserted individually.
  3. EVIDENCE PROPERTIES — what the map says about each incident's ring. The
     map and constants are read back FROM THE COMPILED FIRMWARE, so a future
     map edit that introduced a window collision fails here instead of being
     certified by a stale copy.
  4. SYNTHETIC CASES — negatives the capture does not contain, plus the
     legitimate-recovery control.
  5. COUNTERFACTUAL — drops the two Bamboo phantom events from the real event
     stream and shows the whole incident-C chain disappears. This is what makes
     them causal rather than merely correlated, and it is the acceptance target
     for the low-PWM phantom proposal.

Usage:  python3 run_suite.py [--harness /path/to/binary]
"""

import argparse
import json
import os
import pathlib
import subprocess
import sys

import qrun

HERE = pathlib.Path(__file__).resolve().parent
CAPTURE = HERE.parent.parent.parent / 'field-records/logs/20260810_IR_SPEED_LOCAL_1_2_otto.log'

# The map and the advisory's parameters come FROM THE COMPILED FIRMWARE, read
# back through the harness's `constants` command. Nothing here is a copy.
#
# A hand-transcribed NGR_DNA1 was the obvious way to write these checks and the
# wrong one: a future map edit could introduce a window collision — the exact
# thing §3 exists to catch — while a stale copy went on certifying the old
# table. Loaded once in main() and injected here.
DNA = []
DNA_N = 0
DNA_W = 0
REACQ_WINDOW_MARKERS = 0


def load_firmware_constants(harness):
    """Read the map and constants out of the compiled sketch."""
    global DNA, DNA_N, DNA_W, REACQ_WINDOW_MARKERS
    rows = qrun.run_lines(harness, ['constants'], label='constants')
    c = next((r for r in rows if 'dna' in r), None)
    if c is None:
        sys.exit('harness did not report firmware constants — cannot verify '
                 'the map without a second copy, which is what this avoids')
    DNA = c['dna']
    DNA_N = c['dna_n']
    DNA_W = c['dna_w']
    REACQ_WINDOW_MARKERS = c['reacq']
    if len(DNA) != DNA_N:
        sys.exit(f'firmware reported {len(DNA)} DNA entries for DNA_N {DNA_N}')
    return c

# The three HARD_BOUND incidents, as published by the locomotive.
INCIDENTS = {
    'A': dict(line=14945, mm=100, scores=[7, 3, 7, 8, 4, 6], leader=2,
              runner_up=-1, margin=1,
              ring='N S S N S S N S S S N N', true_offset=8, true_mm=108,
              advisory=None),   # +8 lies outside the +/-5 advisory window
    'B': dict(line=19608, mm=87, scores=[3, 6, 6, 5, 5, 6], leader=0,
              runner_up=1, margin=0,
              ring='S S S S N S S N S S N N', true_offset=None, true_mm=None,
              advisory=None),   # latched readings match nothing at any width
    'C': dict(line=25092, mm=23, scores=[5, 3, 5, 5, 6, 6], leader=3,
              runner_up=4, margin=0,
              ring='S S S S N N N N S S N S', true_offset=-5, true_mm=18,
              advisory=18),     # exact, unique, and inside +/-5
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


def check_incident(tag, decisions, snapshots):
    """Assert one incident's published NO_QUORUM decision and advisory."""
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

    # The advisory, from the retained terminal record.
    snap = None
    for s_ in snapshots:
        try:
            cand = json.loads(s_['snapshot']) if s_['snapshot'] else None
        except ValueError:
            continue
        if cand and cand.get('mm') == inc['mm'] and cand.get('e') == 'NO_QUORUM':
            snap = cand
    if snap is None:
        fails.append(f'incident {tag}: no retained snapshot captured')
        return fails
    got = snap.get('adv')
    if got != inc['advisory']:
        fails.append(f'incident {tag}: advisory {got}, expected {inc["advisory"]}'
                     + (' — a wrong non-null advisory is a BLOCKING DEFECT'
                        if got is not None else ''))
    if snap.get('advw') != DNA_W or snap.get('advr') != REACQ_WINDOW_MARKERS:
        fails.append(f'incident {tag}: advisory audit fields advw/advr = '
                     f'{snap.get("advw")}/{snap.get("advr")}, expected '
                     f'{DNA_W}/{REACQ_WINDOW_MARKERS}')
    if len(json.dumps(snap, separators=(',', ':'))) > 512:
        fails.append(f'incident {tag}: snapshot exceeds the 512-byte buffer')
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
    # A missing final dump means the assertion has nothing to check. Skipping
    # it silently would let a truncated run report success, so it fails here.
    needs_final = [k for k in ('final_state', 'final_mm', 'final_dir')
                   if k in exp]
    if needs_final and final is None:
        fails.append(f'{name}: no final state dump, so {needs_final} could not '
                     'be checked — treated as failure, not skipped')
    elif final is not None:
        for key, field in (('final_state', 'state'), ('final_mm', 'mm'),
                           ('final_dir', 'dir')):
            if key in exp and final[field] != exp[key]:
                fails.append(f'{name}: final {field} {final[field]}, '
                             f'expected {exp[key]}')

    if 'snapshot_desired' in exp:
        snaps = su['snapshots']
        if not snaps:
            fails.append(f'{name}: expected snapshot_desired '
                         f'{exp["snapshot_desired"]} but no snapshot was emitted')
        elif snaps[-1]['snapshot_desired'] != exp['snapshot_desired']:
            fails.append(f'{name}: snapshot_desired '
                         f'{snaps[-1]["snapshot_desired"]}, '
                         f'expected {exp["snapshot_desired"]}')

    snaps = qrun.snapshots_json(su)

    if 'advisory' in exp:
        adv = snaps[-1].get('adv') if snaps else None
        if not snaps:
            fails.append(f'{name}: expected advisory {exp["advisory"]} but no '
                         'retained snapshot was emitted')
        elif adv != exp['advisory']:
            fails.append(f'{name}: advisory {adv}, expected {exp["advisory"]}'
                         + (' — a wrong non-null advisory is a BLOCKING DEFECT'
                            if adv is not None else ''))

    # Ordered advisories, one per terminal entry. Pins the HARD_BOUND-only
    # scope: the same ring under a different reason must go silent.
    if 'advisory_sequence' in exp:
        got = [s.get('adv') for s in snaps]
        if got != exp['advisory_sequence']:
            fails.append(f'{name}: advisory sequence {got}, expected '
                         f'{exp["advisory_sequence"]} — the advisory is '
                         'HARD_BOUND-only (decision 0023)')
    if 'advn_sequence' in exp:
        got = [s.get('advn') for s in snaps]
        if got != exp['advn_sequence']:
            fails.append(f'{name}: advn sequence {got}, expected '
                         f'{exp["advn_sequence"]} — advn proves a null advisory '
                         'came from the reason gate, not a short ring')

    reasons = [d.get('reason') for d in su['decisions']
               if d['event'] == 'NO_QUORUM']
    if 'terminal_reason' in exp and exp['terminal_reason'] not in reasons:
        fails.append(f'{name}: terminal reasons {reasons}, expected to include '
                     f'{exp["terminal_reason"]} — the fixture no longer reaches '
                     'the path it exists to cover')
    if 'terminal_reasons' in exp and reasons != exp['terminal_reasons']:
        fails.append(f'{name}: terminal reasons {reasons}, expected '
                     f'{exp["terminal_reasons"]} — the fixture no longer '
                     'reaches the paths it exists to cover')
    return fails, sorted(set(ev))


# The two events at these capture lines are the phantoms accepted during the
# Bamboo departure, localised by change-point fit (0 mismatches over 39 events).
PHANTOM_LINES = {24662, 24669}


def counterfactual(harness):
    """Replay the real run with the two phantom events removed.

    The empirical test of causation. If those two events are what put the
    odometer 2 ahead, dropping them must remove the wrong adoption at capture
    line 24800 and the HARD_BOUND at 25092, while leaving incidents A and B —
    which happen elsewhere, for other reasons — untouched.

    This is also the acceptance target for the low-PWM phantom proposal: any
    rule that rejects exactly these two events inherits this outcome.
    """
    expected = json.loads((HERE / 'fixtures/full_run.expected').read_text())
    lines = [e['line'] for e in expected if e.get('event') == 'MARKER']
    src = (HERE / 'fixtures/full_run.replay').read_text().splitlines()
    out, i = [], 0
    for ln in src:
        if ln.startswith('event'):
            drop = lines[i] in PHANTOM_LINES
            i += 1
            if drop:
                continue
        out.append(ln)
    su = qrun.summarise(qrun.run_lines(harness, out))
    adoptions = [d for d in su['decisions'] if d['event'] == 'QUORUM_ADOPTED']
    noquorum = sorted(d['mm'] for d in su['decisions']
                      if d['event'] == 'NO_QUORUM')
    fails = []
    if adoptions:
        fails.append(f'counterfactual: still adopts {adoptions} — the phantoms '
                     'would then not be the sole cause of the wrong adoption')
    if 23 in noquorum:
        fails.append('counterfactual: incident C still occurs — the phantoms '
                     'would then not be its sole cause')
    if noquorum != [87, 100]:
        fails.append(f'counterfactual: NO_QUORUM at mm {noquorum}, expected '
                     '[87, 100] — incidents A and B must be unaffected')
    print(f'    dropped the {len(PHANTOM_LINES)} phantom events at capture '
          f'lines {sorted(PHANTOM_LINES)}')
    print(f'    adoptions: {len(adoptions)} (was 1 — the wrong +3 at line 24800)')
    print(f'    NO_QUORUM at mm {noquorum} (was [23, 87, 100]) — incident C gone')
    return fails


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--harness', default='/tmp/quorum_harness',
                    help='where to build the harness (or which prebuilt '
                         'binary to use, with --no-build)')
    ap.add_argument('--no-build', action='store_true',
                    help='use --harness as-is instead of rebuilding it. Lets '
                         'the suite run against a deliberately altered '
                         'firmware, which is how the map checks in section 3 '
                         'are shown to be able to fail.')
    args = ap.parse_args()
    failures = []

    print('== build ==')
    if args.no_build:
        if not os.path.exists(args.harness):
            sys.exit(f'{args.harness} not found')
        print(f'    using prebuilt harness {args.harness} (--no-build)')
    else:
        warn = build(args.harness)
        print(f'    harness built, {len(warn)} warnings')
    consts = load_firmware_constants(args.harness)
    print(f"    firmware constants: DNA_N {consts['dna_n']}, DNA_W "
          f"{consts['dna_w']}, REACQ +/-{consts['reacq']}, "
          f"QUORUM_MAX {consts['quorum_max']}, margin {consts['quorum_margin']}, "
          f"offsets {consts['offsets']}")
    if consts['offsets'] != [-1, 0, 1, 2, 3, 4]:
        failures.append(f"QUORUM_OFFSETS is {consts['offsets']}, not the "
                        'reviewed fence — fence width is a coupled defect and '
                        'must not move without its own decision record')

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
        f = check_incident(tag, decisions, su['snapshots'])
        inc = INCIDENTS[tag]
        print(f'    incident {tag} (capture line {inc["line"]}, mm {inc["mm"]}): '
              + ('OK' if not f else 'FAIL')
              + f'  advisory={inc["advisory"]}'
              + (f' (true position {inc["true_mm"]})' if inc['true_mm'] else ''))
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

    print('\n== 5. counterfactual: are the two Bamboo phantoms causal? ==')
    failures += counterfactual(args.harness)

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
