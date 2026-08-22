#!/usr/bin/env python3
"""Emit manifest.json: every plan test and global invariant -> its function.

    python3 -m tools.navlab.acceptance.make_manifest

The manifest is the audit trail that the frozen plan and the frozen harness
correspond. A plan family with no implementing function, or a function with no
plan family, is reported as a gap rather than passing unnoticed.
"""
import json
import pathlib

from . import families, prereq, navapi, invariants

# Every family named in docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md.
PLAN_FAMILIES = [
    'P0', 'S1', 'S2', 'S3', 'S4', 'S5',
    'U0', 'U1', 'U2', 'U3', 'U4', 'U4b', 'U4c', 'U5', 'U6', 'U7',
    'T0', 'T1a', 'T1b', 'T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'T6b', 'T6c',
    'T7', 'T8', 'T9', 'T10', 'T11', 'T12', 'T13', 'T14', 'T14b', 'T15',
    'T15.5', 'T16', 'T17', 'T18', 'T19', 'T20', 'T21', 'T22', 'T23',
]

# Invariants are enforced continuously inside invariants.Monitor rather than by
# a single family, so they map to the checking method, not to a test id.
INVARIANT_MAP = {
    'S1 no false confirmation': 'invariants.Monitor.check_confirmation',
    'S2 COMPLETE set contains the truth': 'invariants.Monitor.check_completeness',
    'S3 authority within conservative occupancy and separation':
        'invariants.Monitor.check_authority',
    'S4 stopping always available; no declaration demanded':
        'invariants.Monitor.note_status',
    'S5 branch-local elapsed never double-counted':
        'families.t6c_branch_timing',
    'no firmware label or receipt time in position reasoning':
        'selftests.run_selftests::H4 differential probe',
    'compression never grants more authority':
        'invariants.Monitor.check_publication',
    'stop classification': 'invariants.stop_classification',
}

GATE_MAP = {
    'U0': 'T0', 'U1': 'T1a', 'U2': 'T1a/T2 bound from prereq.W_DIR/W_BOTH',
    'U3': 'T2', 'U4': 'T3', 'U4b': 'T16', 'U4c': 'T21', 'U5': 'T7/T10',
    'U6': 'T1a/T16/T22', 'U7': 'reported in every family result data',
}


def build():
    impl = {}
    for fn in families.REGISTRY:
        impl[fn.test_id] = dict(
            function='families.%s' % fn.__name__,
            title=fn.title,
            gate=fn.gate,
            regime=getattr(fn, 'regime', ''),
            spec_ref=fn.spec_ref,
        )
    covered = set(impl)
    missing = [f for f in PLAN_FAMILIES
               if f not in covered and f not in GATE_MAP and not f.startswith('S')
               and f != 'P0']
    extra = [t for t in covered if t not in PLAN_FAMILIES
             and not t.startswith('P0') and not t.startswith('N')
             and not t.startswith('T1a.')]
    return dict(
        generated_from='docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md',
        spec='docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md',
        decision='docs/decisions/0042-*.md',
        map_source=prereq.REPORT['map_source'],
        computed_prerequisites=dict(W_dir=prereq.W_DIR, W_both=prereq.W_BOTH),
        navigator_required=True,
        navigator_present=navapi.load_navigator() is not None,
        prohibited_symbols=list(navapi.PROHIBITED_SYMBOLS),
        only_motion_order=navapi.ONLY_MOTION_ORDER,
        open_operator_policies=navapi.Policy().open_decisions(),
        implemented=impl,
        invariants=INVARIANT_MAP,
        usefulness_gate_to_family=GATE_MAP,
        plan_families_without_implementation=missing,
        implementations_without_plan_family=extra,
    )


def main():
    m = build()
    out = pathlib.Path(__file__).with_name('manifest.json')
    out.write_text(json.dumps(m, indent=2, sort_keys=False) + '\n')
    print('wrote %s: %d implemented, %d plan gap(s), %d extra'
          % (out, len(m['implemented']),
             len(m['plan_families_without_implementation']),
             len(m['implementations_without_plan_family'])))
    if m['plan_families_without_implementation']:
        print('GAPS:', m['plan_families_without_implementation'])


if __name__ == '__main__':
    main()
