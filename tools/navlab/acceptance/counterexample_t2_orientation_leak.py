#!/usr/bin/env python3
"""Minimal demonstration: T2 declares the direction it exists to withhold.

    python3 -m tools.navlab.acceptance.counterexample_t2_orientation_leak

T2's own registration says what it is: "orientation-unknown startup,
movement externally authorised" (spec 4.4 / U3). Its streams are built by
`_all_marker_streams(mode=A.MODE_UNKNOWN)`, the same helper T1 ("orientation-
KNOWN route-wide startup") also uses -- and every stream that helper builds
carries `start_dir`, the GENERATOR's ground truth, kept on the stream so the
harness can score the final acquired position against it. That field was
never meant to be an operator declaration.

`_sweep`, the one driver both T1 and T2 go through, called
`nav.start(stream.start_mode, policy, mm=stream.start_mm_declared,
direction=stream.start_dir)` unconditionally -- so T1 and T2 declared the
identical direction to whatever navigator was under test. Per spec 4.2/4.4, a
declared direction seeds ACQ_ROUTE_WIDE (one direction-plane, `M.DNA_N`
hypotheses); withholding it seeds UNLOCATED (both planes, `2 * M.DNA_N`). T2
was therefore never exercising spec 4.4 at all -- it ran the same
orientation-KNOWN startup as T1, on streams that merely happened to carry
`require_acquisition`/`bound` values tuned for the wider search.

This is a defect in the harness's own wiring: what `_sweep` puts on the wire
to `NavigatorContract.start()`, not anything about how a particular
navigator interprets it. It is demonstrated with a probe that implements
nothing of `NavigatorContract` except `start()`, and that only to record the
`direction` it was called with -- no navigator, hostnav's or any other's,
is imported or run. Reproducing the historical call directly against the
probe shows what value the driver actually put on the wire; running today's
`families.t2_unknown` through the same probe shows what it puts there now.

Exit status: 0 when the demonstration holds. Non-zero only if `_all_marker_streams`
stops producing a directional stream at all, in which case this evidence has
expired.

Whether the leak still exists is REPORTED by driving today's
`families.t2_unknown` through the probe, never gated: the correction this
evidence justifies removes the leak, so gating on its absence would make the
evidence invalidate itself the moment it was acted on.

Provenance note: the harness correction this evidence justifies (T2 passing
`declare_direction=False`) was committed together with this file at `fcd6ef6`,
combining evidence and correction in one commit rather than landing the
evidence first. This file's replacement is a cleanup of that ordering
mistake, not a rewrite of `fcd6ef6` itself.
"""
import sys

from . import families as F
from . import navapi as A
from . import ngrmap as M


class _StartRecorded(Exception):
    """Raised by `_DirectionProbe.start()` immediately after recording, to
    short-circuit the frozen driver before it demands anything else of the
    navigator. The probe answers exactly one question -- what direction was
    it started with -- and nothing downstream of that call is exercised."""


class _DirectionProbe:
    """The minimal fragment of `NavigatorContract` needed to observe what a
    driver puts on the wire to `start()`. It implements no other method: not
    `observe`, not `status`, not `operator`. It asserts nothing about
    acquisition, motion, or any other navigator behaviour, and it is not
    hostnav or any other implementation -- it is not a navigator at all.
    """

    def __init__(self, log):
        self._log = log

    def start(self, mode, policy, mm=None, direction=None):
        self._log.append(direction)
        raise _StartRecorded()


def check(label, ok, detail=''):
    print('  %s  %s%s' % ('OK  ' if ok else 'FAIL', label,
                          '' if ok else '  -- ' + detail))
    return ok


def _record_one(mode, mm, direction):
    """Feed exactly one `start()` call to a fresh probe; return what it saw."""
    log = []
    try:
        _DirectionProbe(log).start(mode, A.Policy(), mm=mm, direction=direction)
    except _StartRecorded:
        pass
    return log[-1] if log else '<no call recorded>'


def main():
    print(__doc__.strip().splitlines()[0])
    print()
    ok = True

    streams = F._all_marker_streams(seed0=5000, mode=A.MODE_UNKNOWN)
    stream = streams[0]
    ok &= check('T2 streams carry a non-null generator ground-truth direction',
                stream.start_dir in M.DIRS)

    # Reproduce the historical `_sweep` call directly against the probe --
    # hardcoded here so the demonstration stays valid regardless of whatever
    # families.py does today.
    leaked = _record_one(stream.start_mode, stream.start_mm_declared,
                         stream.start_dir)
    ok &= check('reproducing the historical `_sweep` call records the '
                'generator\'s ground truth as `direction`',
                leaked == stream.start_dir, 'recorded %r' % (leaked,))

    withheld = _record_one(stream.start_mode, stream.start_mm_declared, None)
    ok &= check('withholding direction instead records None',
                withheld is None, 'recorded %r' % (withheld,))

    print()
    if not ok:
        print('NOT DEMONSTRATED. This evidence has expired; do not inherit '
              'the harness correction it justified.')
        return 1

    print('DEFECT DEMONSTRATED. The historical `_sweep` call put the '
          'generator\'s ground truth on the wire')
    print('as an operator-declared `direction`, which spec 4.2 seeds as one '
          'direction-plane (%d hypotheses)' % M.DNA_N)
    print('rather than spec 4.4\'s UNLOCATED (%d, both planes) T2 exists to '
          'exercise.' % (2 * M.DNA_N))

    # Reported, never gated: drive today's actual frozen family through the
    # same probe and see what it puts on the wire now.
    log = []

    def factory():
        return _DirectionProbe(log)

    try:
        F.t2_unknown(factory, A.Policy())
    except _StartRecorded:
        pass

    print()
    if log and log[0] is None:
        print('RESOLVED. Running families.t2_unknown today calls start() '
              'with direction=None;')
        print('T2 now begins UNLOCATED, as spec 4.4 requires.')
    else:
        print('NOT YET RESOLVED. families.t2_unknown still calls start() '
              'with direction=%r.' % (log[0] if log else '<no call recorded>'))

    return 0


if __name__ == '__main__':
    sys.exit(main())
