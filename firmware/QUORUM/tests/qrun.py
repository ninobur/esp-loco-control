#!/usr/bin/env python3
"""Shared helper: run a replay through the host-compiled firmware and
summarise what the navigator did. Used by run_suite.py and make_synthetic.py.
"""

import json
import subprocess

DECISIONS = {'QUORUM_OPEN', 'QUORUM_TIED', 'QUORUM_ADOPTED', 'QUORUM_CLOSED',
             'QUORUM_REOPENED', 'NO_QUORUM', 'PHANTOM_REJECTED',
             'ADOPTION_FAILED', 'DECLARED'}


def run_lines(harness, lines, timeout=300):
    """Run a list of harness commands; return parsed output rows."""
    text = '\n'.join(lines) + '\n'
    out = subprocess.run([harness], input=text, capture_output=True,
                         text=True, timeout=timeout)
    rows = []
    for line in out.stdout.splitlines():
        line = line.strip()
        if line:
            try:
                rows.append(json.loads(line))
            except ValueError:
                pass
    return rows


def run_file(harness, path, timeout=300):
    with open(path) as fh:
        return run_lines(harness, fh.read().splitlines(), timeout)


def summarise(rows):
    """Decisions, final navigator state, and the retained snapshot."""
    decisions, states, snapshots = [], [], []
    for r in rows:
        if 'pub' in r:
            try:
                d = json.loads(r['payload'])
            except ValueError:
                continue
            if d.get('event') in DECISIONS:
                decisions.append(d)
        elif 'state' in r and 'mm' in r:
            states.append(r)
        elif 'snapshot' in r:
            snapshots.append(r)
    return {
        'decisions': decisions,
        'events': [d['event'] for d in decisions],
        'final': states[-1] if states else None,
        'states': states,
        'snapshots': snapshots,
    }


def counts(summary):
    c = {}
    for e in summary['events']:
        c[e] = c.get(e, 0) + 1
    return c
