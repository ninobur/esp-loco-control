#!/usr/bin/env python3
"""Fidelity check and old-vs-new behavioural diff for session replays.

Two jobs, same doctrine as verify_replay.py generalised to arbitrary sessions:

FIDELITY (--fidelity): the OLD binary replaying a session must land on the
same odometer value for every marker and produce the identical decision
sequence the locomotive published. Timing-gate label mismatches are reported
but tolerated ONLY when both fatal criteria hold — that is the documented
commanded-PWM reconstruction ambiguity, asserted harmless rather than assumed.

DIFF (--old/--new): run both binaries over the same replay and enumerate every
changed outcome, because "enumerate every changed acceptance, rejection,
adoption and terminal outcome" is the bar decision 0024 set and this is the
tool that meets it. Output is a list for a human to judge, not a verdict.
"""

import argparse
import json
import pathlib
import subprocess
import sys

DECISIONS = {'QUORUM_OPEN', 'QUORUM_TIED', 'QUORUM_ADOPTED', 'QUORUM_CLOSED',
             'QUORUM_REOPENED', 'NO_QUORUM', 'PHANTOM_REJECTED',
             'ADOPTION_FAILED', 'DECLARED',
             # quarantine vocabulary (new build; absent from old output)
             'QUARANTINED', 'QUARANTINE_DISCARDED', 'QUARANTINE_COMMITTED',
             'SELF_RESOLVED'}


def run(harness, replay):
    out = subprocess.run([str(harness)], stdin=open(replay),
                         capture_output=True, text=True, timeout=600)
    if out.returncode != 0:
        sys.exit(f'{harness} exited {out.returncode} on {replay}\n'
                 f'{out.stderr[-2000:]}')
    rows = []
    for line in out.stdout.splitlines():
        line = line.strip()
        if line:
            rows.append(json.loads(line))
    return rows


def split(rows):
    """Harness stdout rows:
      marker rows   {ev, gate, mm, dt, dt_expected, state}
      publish rows  {pub: topic, payload: json-string, [retain]}
      dump row      {state, mm, agree, disagree, ring_len, ...}
    Decisions live inside published state/nav payloads."""
    markers = [r for r in rows if 'gate' in r and 'ev' in r]
    decisions = []
    for r in rows:
        if r.get('pub', '').endswith('state/nav'):
            try:
                d = json.loads(r['payload'])
            except (ValueError, TypeError, KeyError):
                continue
            if d.get('event') in DECISIONS:
                decisions.append(d)
        elif r.get('pub', '').endswith('state/cto'):
            pass
    dump = None
    for r in rows:
        if 'ring_len' in r and 'ev' not in r:
            dump = r
    return markers, decisions, dump


def fidelity(harness, fixture):
    rows = run(harness, f'{fixture}.replay')
    markers, decisions, dump = split(rows)
    expected = json.loads(pathlib.Path(f'{fixture}.expected').read_text())
    exp_markers = [e for e in expected if e['event'] == 'MARKER']
    exp_dec = [e for e in expected if e['event'] in DECISIONS]

    n = min(len(markers), len(exp_markers))
    odo_bad = gate_bad = 0
    first = None
    for i in range(n):
        r, e = markers[i], exp_markers[i]
        if r.get('mm') != e.get('mm'):
            odo_bad += 1
            first = first or ('odometer', e['line'], r.get('mm'), e.get('mm'))
        elif r.get('gate') != e.get('timing_gate'):
            gate_bad += 1
            first = first or ('gate', e['line'], r.get('gate'), e.get('timing_gate'))

    dec_r = [(d.get('event'), d.get('mm')) for d in decisions]
    dec_e = [(d.get('event'), d.get('mm')) for d in exp_dec]
    dec_ok = dec_r == dec_e

    ok = (odo_bad == 0 and dec_ok and len(markers) == len(exp_markers))
    tag = 'FIDELITY OK' if ok else 'FIDELITY FAIL'
    print(f'{tag}  {pathlib.Path(fixture).name}: {len(markers)}/{len(exp_markers)} '
          f'events, odo_bad={odo_bad}, gate_label_diffs={gate_bad} (tolerated), '
          f'decisions {"match" if dec_ok else "DIFFER %d vs %d" % (len(dec_r), len(dec_e))}')
    if not ok and first:
        print(f'    first divergence: {first}')
    if not dec_ok:
        for i in range(max(len(dec_r), len(dec_e))):
            a = dec_r[i] if i < len(dec_r) else None
            b = dec_e[i] if i < len(dec_e) else None
            if a != b:
                print(f'    decision {i}: replay={a} capture={b}')
                break
    return ok


def diff(old, new, fixture):
    ro = run(old, f'{fixture}.replay')
    rn = run(new, f'{fixture}.replay')
    mo, do_, dmo = split(ro)
    mn, dn, dmn = split(rn)
    name = pathlib.Path(fixture).name
    print(f'== {name} ==')
    print(f'  events: old {len(mo)}  new {len(mn)} '
          f'(a quarantined-and-discarded event still emits a record)')

    # decision-stream diff, aligned by simple LCS-free walk: report as two
    # streams when they differ anywhere
    seq_o = [(d.get('event'), d.get('mm')) for d in do_]
    seq_n = [(d.get('event'), d.get('mm')) for d in dn]
    only_o = [x for x in seq_o if x not in seq_n]
    only_n = [x for x in seq_n if x not in seq_o]
    from collections import Counter
    co, cn = Counter(e for e, _ in seq_o), Counter(e for e, _ in seq_n)
    print(f'  decision counts old: {dict(sorted(co.items()))}')
    print(f'  decision counts new: {dict(sorted(cn.items()))}')
    if dmo and dmn:
        for k in ('mm', 'agree', 'disagree', 'nav'):
            a, b = dmo.get(k), dmn.get(k)
            mark = '' if a == b else '   <-- CHANGED'
            print(f'  final {k:9}: old {a}  new {b}{mark}')
    if only_o:
        print(f'  decisions LOST vs old ({len(only_o)}):')
        for e in only_o[:20]:
            print(f'    - {e}')
    if only_n:
        print(f'  decisions GAINED vs old ({len(only_n)}):')
        for e in only_n[:30]:
            print(f'    + {e}')
    print()
    return {'fixture': name, 'old_events': len(mo), 'new_events': len(mn),
            'old_counts': dict(co), 'new_counts': dict(cn),
            'lost': only_o, 'gained': only_n,
            'final_old': dmo, 'final_new': dmn}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fidelity', help='harness binary for a fidelity check')
    ap.add_argument('--old')
    ap.add_argument('--new')
    ap.add_argument('fixtures', nargs='+', help='fixture paths WITHOUT extension')
    a = ap.parse_args()
    if a.fidelity:
        bad = [f for f in a.fixtures if not fidelity(a.fidelity, f)]
        sys.exit(1 if bad else 0)
    if a.old and a.new:
        out = [diff(a.old, a.new, f) for f in a.fixtures]
        print(json.dumps(out)[:0], end='')   # results printed above
        return
    ap.error('need --fidelity, or --old and --new')


if __name__ == '__main__':
    main()
