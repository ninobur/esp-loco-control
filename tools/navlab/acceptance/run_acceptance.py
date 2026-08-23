#!/usr/bin/env python3
"""Run the frozen acceptance suite.

    python3 -m tools.navlab.acceptance.run_acceptance [--json OUT] [--quiet]
                                                      [--require-navigator]

With no navigator supplied (the state at freeze), the prerequisites and the
harness self-tests run and report, and every navigator-dependent family reports
NOT_IMPLEMENTED with a named reason. That is the expected result; it is not a
pass and it is not a failure of the design.

Supply a navigator with:  NGR_NAVIGATOR=my.module:Factory

`--require-navigator` is the acceptance GATE mode, for use once a navigator
exists. It changes no test: the families, the generated truth, the invariant
semantics and the expected outcomes are exactly the same objects, run in
exactly the same order. It changes only how the runner converts the tally into
an exit status. In that mode a missing or unloadable navigator, any FAIL and
any NOT_IMPLEMENTED are each non-zero, and NOT_DEMONSTRATED stays a distinct
reported status that is never counted as a pass.
"""
import argparse
import json
import os
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

    try:
        nav_factory = navapi.load_navigator()
    except Exception as e:                  # configured but unimportable
        nav_factory = None
        suite_failure = ('configured navigator failed to load: %s: %s'
                         % (type(e).__name__, e))
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


def _navigator_name():
    try:
        return str(navapi.load_navigator())
    except Exception as e:
        return 'FAILED TO LOAD: %s: %s' % (type(e).__name__, e)


def tally(results):
    t = {PASS: 0, FAIL: 0, NOT_IMPLEMENTED: 0, NOT_DEMONSTRATED: 0}
    for r in results:
        t[r.status] = t.get(r.status, 0) + 1
    return t


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--json', help='write the full machine-readable report here')
    ap.add_argument('--quiet', action='store_true')
    ap.add_argument('--require-navigator', action='store_true',
                    help='acceptance gate: a missing navigator, any FAIL and '
                         'any NOT_IMPLEMENTED all exit non-zero. '
                         'NOT_DEMONSTRATED stays distinct and never passes.')
    args = ap.parse_args()

    load_error = None
    if args.require_navigator:
        try:
            if navapi.load_navigator() is None:
                load_error = ('--require-navigator: no navigator configured; '
                              'set NGR_NAVIGATOR=<module:factory>')
        except Exception as e:                       # unimportable / missing attr
            load_error = ('--require-navigator: configured navigator %r failed '
                          'to load: %s: %s' % (os.environ.get('NGR_NAVIGATOR'),
                                               type(e).__name__, e))

    results, suite_failure = run()
    t = tally(results)

    if not args.quiet:
        print('NGR autonomous-acquisition acceptance suite')
        print('map: %s  markers=%d  circuit=%d mm'
              % (prereq.REPORT['map_source'], prereq.REPORT['dna_n'],
                 prereq.REPORT['circuit_mm']))
        print('computed uniqueness: W_dir=%s  W_both=%s'
              % (prereq.W_DIR, prereq.W_BOTH))
        name = _navigator_name()
        print('navigator under test: %s'
              % ('<none supplied>' if name == 'None' else name))
        if args.require_navigator:
            print('mode: --require-navigator '
                  '(FAIL and NOT_IMPLEMENTED both gate; '
                  'NOT_DEMONSTRATED reported distinctly)')
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
    if load_error:
        print(load_error)

    if args.json:
        with open(args.json, 'w') as fh:
            json.dump(dict(
                map=prereq.REPORT,
                navigator=_navigator_name(),
                require_navigator=bool(args.require_navigator),
                navigator_load_error=load_error,
                suite_failure=suite_failure,
                tally=t,
                results=[r.as_dict() for r in results]), fh, indent=2)

    # Ordinary mode: exit non-zero on FAIL or a suite failure; NOT_IMPLEMENTED
    # is expected and does NOT make the suite red, so CI distinguishes "not
    # built yet" from "built wrong".
    if args.require_navigator:
        # Gate mode. NOT_DEMONSTRATED is deliberately NOT in this sum: it is
        # reported distinctly and is neither a pass nor a gate failure.
        return 1 if (load_error or suite_failure
                     or t[FAIL] or t[NOT_IMPLEMENTED]) else 0
    return 1 if (t[FAIL] or suite_failure) else 0


if __name__ == '__main__':
    sys.exit(main())
