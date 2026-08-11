#!/usr/bin/env python3
"""Turn a captured MQTT log into a deterministic QUORUM replay fixture.

The capture is the authority and is never modified. This reads it and writes
two files per fixture:

  <name>.replay    commands for harness.cpp (the real firmware, host-compiled)
  <name>.expected  what the locomotive actually published, for comparison

Reconstruction notes — the honest limits of this fixture:

  * Every received marker event is published by drainMarkers(), and the run
    records marker_pub_drops 0 and pub_drops 0 throughout, so the captured
    marker stream is COMPLETE. Rejected events appear too, which is required:
    a phantom that navMm never advanced for is still evidence.
  * dt is taken from the marker record, where it was computed as
    detectedAtMs - lastMarkerMs. Replaying those intervals reproduces the
    timing gate's inputs exactly.
  * pwmActualAtDetect is the marker record's "pwm".
  * pwmCommandedAtDetect is NOT in the marker record. It is reconstructed from
    the state/throttle timeline, sampled at the instant the event OPENED
    (§3 stamps both PWM values at open, not at drain). Open time is recovered
    as publish_ts - durationMs, which matters: the mm 82 event at capture line
    14785 carries a 959 ms pulse that opened BEFORE the throttle dropped
    120 -> 90, and sampling at publish time would wrongly read commanded=90 and
    turn an ACTIVE gate into RAMP.
    The reconstruction is then VERIFIED: the replay's computed timing_gate must
    equal the captured timing_gate for every event. verify_replay.py enforces
    this, so a bad reconstruction fails loudly rather than silently changing
    behaviour.
"""

import argparse
import json
import pathlib
import re
import sys

MARKER_RE = re.compile(r'^(\S+) (\S+) (.*)$')


def parse(path):
    """Yield (lineno, ts, topic, payload) for every record in the capture."""
    with open(path, 'r', errors='replace') as fh:
        for n, raw in enumerate(fh, 1):
            m = MARKER_RE.match(raw.rstrip('\n'))
            if not m:
                continue
            ts, topic, payload = m.groups()
            try:
                ts = float(ts)
            except ValueError:
                continue
            yield n, ts, topic, payload


def as_json(payload):
    try:
        return json.loads(payload)
    except (ValueError, TypeError):
        return None


# Decision events worth asserting on. AGREE/DISAGREE are included because they
# are the per-marker adjudication verdict, which is what a scoring change moves.
DECISION_EVENTS = {
    'QUORUM_OPEN', 'QUORUM_TIED', 'QUORUM_ADOPTED', 'QUORUM_CLOSED',
    'QUORUM_REOPENED', 'NO_QUORUM', 'PHANTOM_REJECTED', 'DECLARED',
    'AGREE', 'DISAGREE', 'ADOPTION_FAILED',
}


def throttle_timeline(capture):
    """(ts, commandedPwm) for the whole capture, in order.

    state/throttle is published by pubStateIntChanged() only when commandedPwm
    changes, so this timeline is complete and step-shaped.
    """
    tl = []
    for _, ts, topic, payload in parse(capture):
        if topic.endswith('/state/throttle'):
            try:
                tl.append((ts, int(payload.strip())))
            except ValueError:
                pass
    return tl


def commanded_at(tl, t):
    """commandedPwm in force at time t (last change at or before t)."""
    lo_i, hi_i, val = 0, len(tl) - 1, 0
    while lo_i <= hi_i:
        mid = (lo_i + hi_i) // 2
        if tl[mid][0] <= t:
            val = tl[mid][1]
            lo_i = mid + 1
        else:
            hi_i = mid - 1
    return val


def build(capture, lo, hi, name, outdir, start_mm=None, start_dir='CW',
          auto=1, note='', timeline=None):
    cmds, expected = [], []
    tl = timeline if timeline is not None else throttle_timeline(capture)
    declared = False
    markers = 0

    cmds.append(f'# fixture: {name}')
    if note:
        cmds.append(f'# {note}')
    cmds.append(f'# source lines {lo}-{hi} of the capture')
    cmds.append(f'dir {start_dir}')

    for n, ts, topic, payload in parse(capture):
        if n < lo:
            continue
        if n > hi:
            break

        # Motor direction drives navDir through applyDirection(). The run
        # reverses at capture line 29672 (state/direction 0 = REVERSE) while
        # sessionDir stays CW, so navDir becomes CCW and the odometer counts
        # down. Without replaying this the tail of the run diverges entirely.
        if topic.endswith('/state/direction'):
            try:
                cmds.append(f'motor {int(payload.strip())}')
            except ValueError:
                pass
            continue

        d = as_json(payload)

        if topic.endswith('/state/nav') and d and d.get('event') == 'DECLARED':
            cmds.append(f'declare {d["mm"]}')
            cmds.append(f'auto {auto}')
            declared = True
            expected.append({'line': n, 'event': 'DECLARED', 'mm': d['mm']})
            continue

        if topic.endswith('/mm/marker') and d:
            if not declared:
                if start_mm is None:
                    continue
                cmds.append(f'declare {start_mm}')
                cmds.append(f'auto {auto}')
                declared = True
            cmds.append(
                'event {dt} {pol} {pwm} {cmd} {peak} {dur} {drift}'.format(
                    dt=d.get('dt', 0), pol=d.get('obs', 'S'),
                    pwm=d.get('pwm', 0),
                    cmd=commanded_at(tl, ts - d.get('ms', 0) / 1000.0),
                    peak=d.get('peak', 0), dur=d.get('ms', 0),
                    drift=d.get('drift', 0)))
            markers += 1
            expected.append({
                'line': n, 'event': 'MARKER', 'mm': d.get('mm'),
                'obs': d.get('obs'), 'timing_gate': d.get('timing_gate'),
                'dt': d.get('dt'), 'pwm': d.get('pwm'),
            })
            continue

        if topic.endswith('/state/nav') and d and d.get('event') in DECISION_EVENTS:
            rec = {'line': n, 'event': d['event'], 'mm': d.get('mm')}
            for k in ('state', 'scores', 'leader', 'runner_up', 'margin',
                      'eval', 'viable', 'reason', 'offset', 'old_mm', 'new_mm',
                      'streak', 'obs', 'expected'):
                if k in d:
                    rec[k] = d[k]
            expected.append(rec)
            continue

        if topic.endswith('/mm/no_quorum') and d:
            expected.append({'line': n, 'event': 'NO_QUORUM_SNAPSHOT',
                             'snapshot': d})
            cmds.append('snapshot')
            continue

    cmds.append('dump')

    outdir = pathlib.Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / f'{name}.replay').write_text('\n'.join(cmds) + '\n')
    (outdir / f'{name}.expected').write_text(
        json.dumps(expected, indent=1) + '\n')
    print(f'{name}: {markers} marker events, {len(expected)} expected records '
          f'(lines {lo}-{hi})')
    return markers


# ONE fixture, deliberately: the whole session.
#
# Per-incident slices were tried and dropped. A slice has to begin with a
# synthetic declaration, and navDeclare() resets lastConfirmed, the evidence
# ring, markersSinceConfirmed and the incident state — so a slice cannot
# reproduce the entry conditions of the incident it is named for. Incident C's
# evaluation, for instance, opened at mm 14 carrying ring content and an
# adoption history from thirty markers earlier.
#
# The full run reproduces every incident with its true entry conditions, so
# run_suite.py asserts A, B and C against it individually by capture line and
# published score vector. Purpose-built starts belong in make_synthetic.py,
# where a clean declaration is the intent rather than an artefact.
FIXTURES = [
    dict(name='full_run', lo=1, hi=29825, start_mm=13,
         note='entire session: 4 incidents (1 adoption, 3 HARD_BOUND)'),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--capture', default='../../../field-records/logs/'
                                         '20260810_IR_SPEED_LOCAL_1_2_otto.log')
    ap.add_argument('--outdir', default='fixtures')
    args = ap.parse_args()

    capture = pathlib.Path(args.capture)
    if not capture.exists():
        sys.exit(f'capture not found: {capture}')

    tl = throttle_timeline(capture)
    for f in FIXTURES:
        build(capture, f['lo'], f['hi'], f['name'], args.outdir,
              start_mm=f.get('start_mm'), note=f.get('note', ''), timeline=tl)


if __name__ == '__main__':
    main()
