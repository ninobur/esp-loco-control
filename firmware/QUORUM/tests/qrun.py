#!/usr/bin/env python3
"""Shared helper: run a replay through the host-compiled firmware and
summarise what the navigator did.

Every failure mode here is loud. A harness that crashes after emitting partial
output would otherwise look like a short but successful run, and the assertions
downstream — which mostly check that bad things are ABSENT — would pass on the
strength of the crash. That is the one way this suite could report success
while proving nothing, so all three of these raise:

  * a non-zero exit status,
  * a line of output that is not valid JSON,
  * output with no trailing state dump when one was requested.
"""

import json
import subprocess


class HarnessError(RuntimeError):
    """The harness failed. Never swallow this — it invalidates the run."""


DECISIONS = {'QUORUM_OPEN', 'QUORUM_TIED', 'QUORUM_ADOPTED', 'QUORUM_CLOSED',
             'QUORUM_REOPENED', 'NO_QUORUM', 'PHANTOM_REJECTED',
             'ADOPTION_FAILED', 'DECLARED'}


def run_lines(harness, lines, timeout=300, label=''):
    """Run a list of harness commands; return parsed output rows.

    Raises HarnessError on non-zero exit, a crash signal, unparseable output,
    or a timeout.
    """
    text = '\n'.join(lines) + '\n'
    where = f' ({label})' if label else ''
    try:
        out = subprocess.run([str(harness)], input=text, capture_output=True,
                             text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise HarnessError(f'harness timed out after {timeout}s{where}')

    if out.returncode != 0:
        # Negative codes are signals: a segfault mid-run lands here.
        detail = (f'signal {-out.returncode}' if out.returncode < 0
                  else f'exit {out.returncode}')
        raise HarnessError(
            f'harness failed with {detail}{where}\n'
            f'  last stderr: {out.stderr.strip()[-1500:]}\n'
            f'  produced {len(out.stdout.splitlines())} output lines before failing')

    rows = []
    for i, line in enumerate(out.stdout.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except ValueError as exc:
            raise HarnessError(
                f'harness emitted unparseable output at line {i}{where}: '
                f'{line[:200]!r} ({exc})')

    # A 'dump' command must produce a state row. Truncated output otherwise
    # reads as a short-but-clean run.
    wanted_dumps = sum(1 for l in lines if l.strip() == 'dump')
    got_dumps = sum(1 for r in rows if 'state' in r and 'mm' in r)
    if wanted_dumps and got_dumps < wanted_dumps:
        raise HarnessError(
            f'harness produced {got_dumps} state dumps but {wanted_dumps} were '
            f'requested{where} — output is truncated, so any assertion that '
            'passed did so on missing evidence')
    return rows


def run_file(harness, path, timeout=300):
    with open(path) as fh:
        return run_lines(harness, fh.read().splitlines(), timeout,
                         label=str(path).split('/')[-1])


def summarise(rows):
    """Decisions, navigator states, and retained snapshots."""
    decisions, states, snapshots = [], [], []
    for r in rows:
        if 'pub' in r:
            try:
                d = json.loads(r['payload'])
            except ValueError as exc:
                raise HarnessError(
                    f'unparseable payload from the firmware: '
                    f'{r["payload"][:200]!r} ({exc})')
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


def snapshots_json(summary):
    """Retained NO_QUORUM snapshots, parsed, in the order they were built."""
    out = []
    for s in summary['snapshots']:
        if not s['snapshot']:
            continue
        try:
            out.append(json.loads(s['snapshot']))
        except ValueError as exc:
            raise HarnessError(
                f'unparseable retained snapshot: {s["snapshot"][:200]!r} ({exc})')
    return out
