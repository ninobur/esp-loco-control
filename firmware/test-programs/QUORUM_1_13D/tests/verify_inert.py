#!/usr/bin/env python3
"""Prove a diagnostic-only change is behaviourally inert.

Builds the harness twice — once against the current QUORUM.ino, once against
QUORUM.ino at a given git ref — and requires the two to produce byte-identical
output on every fixture, after removing the fields the change is allowed to
add.

This is the check that enforces decision 0023's contract for the HARD_BOUND
advisory: it may publish, and it may do nothing else. If a future edit makes
the advisory move navMm, adopt a candidate, touch the throttle or the station
machine, alter NAV_NO_QUORUM or change any published decision, the outputs
diverge and this fails.

  python3 verify_inert.py --base HEAD~1 --allow-added adv,advw,advr,advn
"""

import argparse
import glob
import os
import pathlib
import re
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
SKETCH = HERE.parent


def build_at_ref(ref, workdir):
    """Check QUORUM.ino out at `ref` beside the current config headers and
    build the CURRENT harness against it."""
    workdir = pathlib.Path(workdir)
    if workdir.exists():
        shutil.rmtree(workdir)
    (workdir / 'tests').mkdir(parents=True)
    src = subprocess.run(
        ['git', 'show', f'{ref}:firmware/QUORUM/QUORUM.ino'],
        capture_output=True, text=True, cwd=SKETCH)
    if src.returncode != 0:
        sys.exit(f'cannot read QUORUM.ino at {ref}: {src.stderr}')
    (workdir / 'QUORUM.ino').write_text(src.stdout)
    for h in ('LocoConfig.h', 'LL_LocoConfig_9950011.h',
              'LL_LocoConfig_9950012.h', 'credentials.h'):
        p = SKETCH / h
        if p.exists():
            shutil.copy(p, workdir / h)
    # The SAME harness drives both, so any difference is the sketch's.
    shutil.copytree(HERE / 'shim', workdir / 'tests/shim')
    shutil.copy(HERE / 'harness.cpp', workdir / 'tests/harness.cpp')
    out = workdir / 'harness_bin'
    r = subprocess.run(
        ['clang++', '-std=c++17', '-Wno-format',
         '-I', str(workdir / 'tests/shim'), '-I', str(workdir),
         '-o', str(out), str(workdir / 'tests/harness.cpp')],
        capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-3000:])
        sys.exit(f'baseline harness build FAILED at {ref}')
    return out


def run(binary, path, which):
    """Run one fixture. A crash must fail loudly.

    Comparing two truncated outputs that happen to be truncated identically
    would report 'IDENTICAL' and prove nothing, so a non-zero exit is fatal
    here rather than a difference to be reported.
    """
    r = subprocess.run([str(binary)], stdin=open(path), capture_output=True,
                       text=True)
    if r.returncode != 0:
        detail = (f'signal {-r.returncode}' if r.returncode < 0
                  else f'exit {r.returncode}')
        sys.exit(f'{which} harness failed with {detail} on '
                 f'{os.path.basename(path)}\n'
                 f'  last stderr: {r.stderr.strip()[-1500:]}')
    return r.stdout.splitlines()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base', default='HEAD~1',
                    help='git ref holding the reference QUORUM.ino')
    ap.add_argument('--current', default='/tmp/quorum_harness',
                    help='harness built from the working tree')
    ap.add_argument('--allow-added', default='adv,advw,advr,advn',
                    help='comma-separated JSON fields the change may add')
    ap.add_argument('--workdir', default='/tmp/quorum_inert_base')
    args = ap.parse_args()

    if not os.path.exists(args.current):
        sys.exit(f'{args.current} not found — run run_suite.py first')

    fields = [f.strip() for f in args.allow_added.split(',') if f.strip()]
    # Payloads are nested JSON, so a field appears as \"adv\":18 with literal
    # backslashes. \\* matches however many escaping levels there are; each
    # field is stripped independently so order and adjacency do not matter.
    patterns = [re.compile(r',?\\*"' + re.escape(f) + r'\\*":(?:null|-?\d+)')
                for f in fields]

    def strip(line):
        for p in patterns:
            line = p.sub('', line)
        return line

    base = build_at_ref(args.base, args.workdir)
    fixtures = sorted(glob.glob(str(HERE / 'fixtures/*.replay')))
    if not fixtures:
        sys.exit('no fixtures — run run_suite.py first')

    print(f'reference: QUORUM.ino at {args.base}')
    print(f'allowed added fields: {fields}\n')
    failures = 0
    for f in fixtures:
        a = [strip(l) for l in run(base, f, 'baseline')]
        b = [strip(l) for l in run(args.current, f, 'current')]
        same = a == b
        print(f'  {"IDENTICAL" if same else "*** DIFFERS ***":>16}  '
              f'{os.path.basename(f)}')
        if not same:
            failures += 1
            for i, (x, y) in enumerate(zip(a, b)):
                if x != y:
                    print(f'      first diff, line {i}:')
                    print(f'        base {x[:180]}')
                    print(f'        new  {y[:180]}')
                    break
    print()
    if failures:
        print(f'FAIL — {failures} fixture(s) changed behaviour. A diagnostic-only '
              'change must alter nothing but the fields it declares.')
        return 1
    print('PASS — behaviourally inert: no decision, odometer value, navigator '
          'state or publication differs.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
