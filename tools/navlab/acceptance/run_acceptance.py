#!/usr/bin/env python3
"""Run the frozen acceptance suite.

    python3 -m tools.navlab.acceptance.run_acceptance [--json OUT] [--quiet]

With no navigator supplied (the state at freeze), the prerequisites and the
harness self-tests run and report, and every navigator-dependent family reports
NOT_IMPLEMENTED with a named reason. That is the expected result; it is not a
pass and it is not a failure of the design.

Supply a navigator with:  NGR_NAVIGATOR=my.module:Factory
"""
import argparse
import json
import sys

from . import families, prereq, navapi
from .invariants import SuiteFailure
from .result import PASS, FAIL, NOT_IMPLEMENTED, NOT_DEMONSTRATED, Result
from .selftests import run_selftests


def run(policy=None, include_selftests=True):
    policy = policy or navapi.Policy()
    results = []
    suite_failure = None

    blocking = prereq.blocking_failures()
    if blocking:
        for b in blocking:
            results.append(Result('P0.BLOCK', 'map prerequisite', FAIL, b,
                                  gate='prerequisite'))
        return results, 'map prerequisites failed; the map is NOT altered and '
    if include_selftests:
        results.extend(run_selftests(policy))

    nav_factory = navapi.load_navigator()
    for fn in families.REGISTRY:
        try:
            results.append(fn(nav_factory, policy))
        except SuiteFailure as e:
            suite_failure = str(e)
            results.append(Result(fn.test_id, fn.title, FAIL,
                                  'SUITE FAILURE: %s' % e, gate=fn.gate))
            break
        except Exception as e:                       # harness bug, not a verdict
            results.append(Result(fn.test_id, fn.title, FAIL,
                                  'harness error: %s: %s'
                                  % (type(e).__name__, e), gate=fn.gate))
    return results, suite_failure


def tally(results):
    t = {PASS: 0, FAIL: 0, NOT_IMPLEMENTED: 0, NOT_DEMONSTRATED: 0}
    for r in results:
        t[r.status] = t.get(r.status, 0) + 1
    return t


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--json', help='write the full machine-readable report here')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    results, suite_failure = run()
    t = tally(results)

    if not args.quiet:
        print('NGR autonomous-acquisition acceptance suite')
        print('map: %s  markers=%d  circuit=%d mm'
              % (prereq.REPORT['map_source'], prereq.REPORT['dna_n'],
                 prereq.REPORT['circuit_mm']))
        print('computed uniqueness: W_dir=%s  W_both=%s'
              % (prereq.W_DIR, prereq.W_BOTH))
        nav = navapi.load_navigator()
        print('navigator under test: %s'
              % ('<none supplied>' if nav is None else nav))
        print('-' * 78)
        for r in results:
            print('%-18s %-20s %s' % (r.status, r.test_id, r.title))
            if r.detail:
                print('%18s   %s' % ('', r.detail[:200]))
        print('-' * 78)
    print('PASS=%d  FAIL=%d  NOT_IMPLEMENTED=%d  NOT_DEMONSTRATED=%d'
          % (t[PASS], t[FAIL], t[NOT_IMPLEMENTED], t[NOT_DEMONSTRATED]))
    if suite_failure:
        print('SUITE FAILURE: %s' % suite_failure)

    if args.json:
        with open(args.json, 'w') as fh:
            json.dump(dict(
                map=prereq.REPORT,
                navigator=str(navapi.load_navigator()),
                suite_failure=suite_failure,
                tally=t,
                results=[r.as_dict() for r in results]), fh, indent=2)

    # Exit non-zero on FAIL or a suite failure; NOT_IMPLEMENTED is expected and
    # does NOT make the suite red, so CI distinguishes "not built yet" from
    # "built wrong".
    return 1 if (t[FAIL] or suite_failure) else 0


if __name__ == '__main__':
    sys.exit(main())
