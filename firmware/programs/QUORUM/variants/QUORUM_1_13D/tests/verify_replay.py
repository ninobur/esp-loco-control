#!/usr/bin/env python3
"""Run a QUORUM replay fixture through the host-compiled firmware and compare
its behaviour against what the locomotive actually published.

Two jobs, in order:

 1. FIDELITY. Every replayed event must land on the same odometer value and
    the same timing-gate verdict as the capture. This is what proves the
    fixture's reconstructed pwmCommandedAtDetect is right; without it a replay
    could diverge silently and every downstream assertion would be worthless.

 2. BEHAVIOUR. The sequence of adjudication decisions — opens, ties,
    adoptions, phantom rejections, NO_QUORUM entries and their snapshots —
    must match the capture.

Exit status is non-zero if either job fails.
"""

import argparse
import json
import pathlib
import subprocess
import sys

DECISIONS = {'QUORUM_OPEN', 'QUORUM_TIED', 'QUORUM_ADOPTED', 'QUORUM_CLOSED',
             'QUORUM_REOPENED', 'NO_QUORUM', 'PHANTOM_REJECTED',
             'ADOPTION_FAILED'}

# The one reconstruction QUORUM's telemetry cannot pin down exactly.
#
# pwmCommandedAtDetect is sampled at event OPEN and is never published, so the
# fixture recovers it as publish_ts - durationMs against the state/throttle
# timeline. That is exact for ordinary markers, but five events in the run
# carry multi-second pulses (859-3556 ms of dt against 3372-3556 ms pulses),
# all of them inside the defective mm 66-82 stretch where the detector latches.
# For those the open instant straddles a throttle change and cannot be
# recovered from the capture at all.
#
# Consequence is nil and is asserted, not assumed: all five land on the same
# odometer value as the capture, and the full decision sequence is unchanged
# (RAMP and ACTIVE both accept the event; only the conservation test differs,
# and no PHANTOM_REJECTED is gained or lost). They are pinned here by capture
# line so any NEW divergence — which would mean a real behaviour change — fails.
KNOWN_GATE_DIVERGENCE = {
    14664: ('RAMP',    'ACTIVE'),   # pwm 97,  dt 1972, 3453 ms pulse
    14676: ('NO_PREV', 'ACTIVE'),   # follow-on: RAMP invalidates the predecessor
    19583: ('RAMP',    'ACTIVE'),   # pwm 117, dt 859,  3372 ms pulse
    19585: ('NO_PREV', 'ACTIVE'),   # follow-on
    29803: ('RAMP',    'ACTIVE'),   # pwm 115, dt 863
}


def run(harness, replay):
    out = subprocess.run([harness], stdin=open(replay), capture_output=True,
                         text=True, timeout=300)
    if out.returncode != 0:
        sys.exit(f'harness exited {out.returncode}\n{out.stderr[-2000:]}')
    rows = []
    for line in out.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except ValueError:
            sys.exit(f'unparseable harness output: {line[:200]}')
    return rows


def decisions_from(rows):
    """Adjudication decisions the replay produced, in order."""
    got = []
    for r in rows:
        if 'pub' not in r:
            continue
        try:
            d = json.loads(r['payload'])
        except ValueError:
            continue
        ev = d.get('event')
        if ev in DECISIONS:
            rec = {'event': ev, 'mm': d.get('mm')}
            for k in ('scores', 'leader', 'runner_up', 'margin', 'eval',
                      'viable', 'reason', 'offset', 'old_mm', 'new_mm'):
                if k in d:
                    rec[k] = d[k]
            got.append(rec)
    return got


def decisions_expected(expected):
    got = []
    for e in expected:
        if e.get('event') in DECISIONS:
            rec = {'event': e['event'], 'mm': e.get('mm')}
            for k in ('scores', 'leader', 'runner_up', 'margin', 'eval',
                      'viable', 'reason', 'offset', 'old_mm', 'new_mm'):
                if k in e:
                    rec[k] = e[k]
            got.append(rec)
    return got


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--harness', required=True)
    ap.add_argument('--fixture', required=True,
                    help='fixture basename, e.g. fixtures/full_run')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    base = pathlib.Path(args.fixture)
    rows = run(args.harness, str(base) + '.replay')
    expected = json.loads((base.with_suffix('.expected')).read_text())

    failures = []

    # --- 1. fidelity: odometer and gate, event by event --------------------
    ev_rows = [r for r in rows if r.get('ev')]
    ev_exp = [e for e in expected if e.get('event') == 'MARKER']
    if len(ev_rows) != len(ev_exp):
        failures.append(f'event count: replay {len(ev_rows)} vs capture {len(ev_exp)}')
    mm_bad = gate_bad = 0
    gate_known = 0
    first_bad = None
    for got, want in zip(ev_rows, ev_exp):
        if got['mm'] != want['mm']:
            mm_bad += 1
            if first_bad is None:
                first_bad = (want['line'], 'mm', got['mm'], want['mm'])
        if got['gate'] != want['timing_gate']:
            known = KNOWN_GATE_DIVERGENCE.get(want['line'])
            if known == (want['timing_gate'], got['gate']):
                gate_known += 1
            else:
                gate_bad += 1
                if first_bad is None:
                    first_bad = (want['line'], 'gate', got['gate'],
                                 want['timing_gate'])
    n = max(1, len(ev_exp))
    if mm_bad:
        failures.append(f'odometer mismatch on {mm_bad}/{len(ev_exp)} events '
                        '— the replay diverged from the locomotive')
    if gate_bad:
        failures.append(f'UNEXPECTED timing-gate mismatch on {gate_bad}/{len(ev_exp)} '
                        f'events ({100.0*gate_bad/n:.2f}%) — not in the pinned set')
    if first_bad:
        failures.append(f'first divergence at capture line {first_bad[0]}: '
                        f'{first_bad[1]} replay={first_bad[2]} capture={first_bad[3]}')

    # --- 2. behaviour: the decision sequence -------------------------------
    got_d, want_d = decisions_from(rows), decisions_expected(expected)
    if len(got_d) != len(want_d):
        failures.append(f'decision count: replay {len(got_d)} vs capture {len(want_d)}')
    for i, (g, w) in enumerate(zip(got_d, want_d)):
        if g != w:
            failures.append(f'decision {i} differs:\n    replay  {g}\n    capture {w}')
            break

    if not args.quiet:
        print(f'  events   : {len(ev_rows)} replayed, '
              f'{len(ev_exp)-mm_bad}/{len(ev_exp)} odometer match, '
              f'{len(ev_exp)-gate_bad-gate_known}/{len(ev_exp)} gate exact'
              + (f' (+{gate_known} pinned)' if gate_known else ''))
        summary = {}
        for d in got_d:
            summary[d['event']] = summary.get(d['event'], 0) + 1
        print(f'  decisions: {summary}')

    if failures:
        print(f'FAIL {base.name}')
        for f in failures:
            print(f'    {f}')
        return 1
    print(f'PASS {base.name}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
