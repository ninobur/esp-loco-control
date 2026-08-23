#!/usr/bin/env python3
"""Minimal demonstration: T2 declares the direction it exists to withhold.

    python3 -m tools.navlab.acceptance.counterexample_t2_orientation_leak

T2's own registration says what it is: "orientation-unknown startup,
movement externally authorised" (spec 4.4 / U3). Its streams are built by
`_all_marker_streams(mode=A.MODE_UNKNOWN)`, the same helper T1 ("orientation-
KNOWN route-wide startup") also uses -- and every stream that helper builds
carries `start_dir`, the GENERATOR's ground truth, so the harness can score
final position against it. That field was never meant to be an operator
declaration.

`_sweep`, the one driver both T1 and T2 go through, called
`nav.start(stream.start_mode, policy, mm=stream.start_mm_declared,
direction=stream.start_dir)` unconditionally. `Navigator.start()` treats a
declared direction as ACQ_ROUTE_WIDE: 171 hypotheses, not the 342 of
UNLOCATED. T2 was therefore never exercising spec 4.4 at all -- it ran the
same orientation-KNOWN startup as T1, on streams that merely happened to
carry `require_acquisition`/`bound` values tuned for the wider search.

This is a defect in the harness's own wiring, not a claim about what any
navigator does with the input. It is demonstrated directly against
`Navigator.start()` / `.status()`, which only reflect (mode, direction) into
`nav_state` and the seeded hypothesis set -- no acquisition logic runs.

Exit status: 0 when the demonstration holds. Non-zero only if the committed
map changes size, in which case the expected hypothesis counts must be
re-derived.

Whether the leak still exists is REPORTED by actually running today's
`families.t2_unknown` through a direction-recording navigator, never gated:
the correction this evidence justifies removes the leak, so gating on its
absence would make the evidence invalidate itself the moment it was acted on.
"""
import sys

from . import families as F
from . import navapi as A
from . import ngrmap as M
from tools.navlab.hostnav.navigator import Navigator


def check(label, ok, detail=''):
    print('  %s  %s%s' % ('OK  ' if ok else 'FAIL', label,
                          '' if ok else '  -- ' + detail))
    return ok


def main():
    print(__doc__.strip().splitlines()[0])
    print()
    ok = True

    streams = F._all_marker_streams(seed0=5000, mode=A.MODE_UNKNOWN)
    stream = streams[0]
    ok &= check('T2 streams carry a non-null generator ground-truth direction',
                stream.start_dir in M.DIRS)

    # Reproduce the buggy call directly -- this is the historical `_sweep`
    # body, hardcoded here so the demonstration stays valid regardless of
    # whatever families.py does today.
    leaked = Navigator()
    leaked.start(stream.start_mode, A.Policy(), mm=stream.start_mm_declared,
                direction=stream.start_dir)
    st = leaked.status()
    ok &= check('passing the generator\'s ground truth as `direction` yields '
                'ACQUIRING_ORIENTED', st.nav_state == A.ACQUIRING_ORIENTED,
                'got %s' % st.nav_state)
    ok &= check('...seeded with 171 hypotheses (one plane), not 342',
                len(st.hypotheses) == M.DNA_N,
                '|H| = %d' % len(st.hypotheses))

    withheld = Navigator()
    withheld.start(stream.start_mode, A.Policy(), mm=stream.start_mm_declared,
                   direction=None)
    st2 = withheld.status()
    ok &= check('withholding direction instead yields UNLOCATED with 342',
                st2.nav_state == A.UNLOCATED and len(st2.hypotheses) == 2 * M.DNA_N,
                'got %s, |H| = %d' % (st2.nav_state, len(st2.hypotheses)))

    print()
    if not ok:
        print('NOT DEMONSTRATED. This evidence has expired; do not inherit '
              'the harness correction it justified.')
        return 1

    print('DEFECT DEMONSTRATED. T2 as wired declares the ground-truth '
          'direction to the navigator, so it')
    print('never exercises spec 4.4 orientation-unknown startup -- it runs '
          'T1 a second time.')

    # Reported, never gated.
    calls = []
    orig_start = Navigator.start

    def spy(self, mode, policy, mm=None, direction=None):
        calls.append(direction)
        return orig_start(self, mode, policy, mm=mm, direction=direction)

    Navigator.start = spy
    try:
        F.t2_unknown(Navigator, A.Policy())
    finally:
        Navigator.start = orig_start

    print()
    if calls and all(d is None for d in calls):
        print('RESOLVED. Running families.t2_unknown today starts every '
              'navigator with direction=None;')
        print('T2 now begins UNLOCATED, as spec 4.4 requires.')
    else:
        leaking = sum(1 for d in calls if d is not None)
        print('NOT YET RESOLVED. families.t2_unknown still declares a '
              'direction on %d of %d starts.' % (leaking, len(calls)))

    return 0


if __name__ == '__main__':
    sys.exit(main())
