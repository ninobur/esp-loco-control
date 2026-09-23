#!/usr/bin/env python3
"""Extract a COMPLETE per-locomotive session from any NGR capture into a
stateful harness replay.

extract_fixture.py owns the reconstruction doctrine (dt from the marker
record, commanded PWM sampled at event OPEN from the throttle timeline,
fidelity verified downstream). This tool generalises the intake:

  * both capture formats — the Mac-side `mosquitto_sub -F '%U %t %p'` logs
    (space-separated, epoch timestamps) and the Pi runlog `all_*.log`
    (tab-separated, ISO timestamps);
  * one locomotive per replay — captures interleave the fleet, and a replay
    that fed Otto's markers into Toby's session would be nonsense;
  * mid-session direction changes — `state/session_direction` becomes a `dir`
    command and `state/direction` a `motor` command, because the 2026-08-13/14
    sessions reverse repeatedly and the tail diverges without them.

The capture is the authority and is never modified.

Usage:
  python3 extract_session.py --capture all_20260814.log --loco 9950012 \\
      --name toby_20260814 --outdir fixtures/
"""

import argparse
import datetime
import json
import pathlib

DECISION_EVENTS = {
    'QUORUM_OPEN', 'QUORUM_TIED', 'QUORUM_ADOPTED', 'QUORUM_CLOSED',
    'QUORUM_REOPENED', 'NO_QUORUM', 'PHANTOM_REJECTED', 'DECLARED',
    'AGREE', 'DISAGREE', 'ADOPTION_FAILED',
}


def parse(path):
    """Yield (lineno, epoch_ts, topic, payload) from either capture format."""
    with open(path, 'r', errors='replace') as fh:
        for n, raw in enumerate(fh, 1):
            raw = raw.rstrip('\n')
            if not raw or raw.startswith('#'):
                continue
            if '\t' in raw:                       # Pi runlog: ISO \t topic \t payload
                parts = raw.split('\t', 2)
                if len(parts) < 3:
                    continue
                ts_s, topic, payload = parts
                try:
                    ts = datetime.datetime.fromisoformat(ts_s).timestamp()
                except ValueError:
                    continue
            else:                                 # Mac capture: epoch topic payload
                parts = raw.split(' ', 2)
                if len(parts) < 3:
                    continue
                ts_s, topic, payload = parts
                try:
                    ts = float(ts_s)
                except ValueError:
                    continue
            yield n, ts, topic, payload


def as_json(payload):
    try:
        return json.loads(payload)
    except (ValueError, TypeError):
        return None


def extract(capture, loco, name, outdir, min_markers=50):
    """One replay per BOOT SESSION. A capture spanning a reflash concatenates
    sessions whose firmware state reset at each boot; feeding them to one
    harness process replays continuity the locomotive never had (found the
    hard way: Toby's 2026-08-13 log spans two flashes, and the concatenated
    replay diverged at the first seam). state/bootid marks the cut — the
    firmware republishes it on every MQTT connect, so only treat it as a boot
    when the session has already produced markers."""
    prefix = f'ngr/loco/{loco}/'
    rows = [(n, ts, topic[len(prefix):], payload)
            for n, ts, topic, payload in parse(capture)
            if topic.startswith(prefix)]
    if not rows:
        raise SystemExit(f'no traffic for loco {loco} in {capture}')

    segments, cur, markers_in_cur = [], [], 0
    last_uptime = None
    for r in rows:
        _, _, sub, payload = r
        boot = False
        if sub == 'alert':
            d = as_json(payload)
            up = d.get('uptime_ms') if d else None
            if up is not None:
                if last_uptime is not None and up < last_uptime:
                    boot = True                      # uptime regressed: reboot
                last_uptime = up
        if boot and markers_in_cur:
            segments.append(cur)
            cur, markers_in_cur = [], 0
        cur.append(r)
        if sub == 'mm/marker':
            markers_in_cur += 1
    if cur:
        segments.append(cur)

    total = 0
    made = 0
    for i, seg in enumerate(segments, 1):
        nmark = sum(1 for _, _, sub, _ in seg if sub == 'mm/marker')
        if nmark < min_markers:
            continue
        made += 1
        total += _emit(seg, f'{name}_s{made:02d}', outdir)
    if made == 0:
        raise SystemExit(f'{name}: no segment reached {min_markers} markers')
    return total


def _emit(rows, name, outdir):

    # Throttle timeline for commanded-at-open reconstruction (this segment).
    tl = []
    for _, ts, sub, payload in rows:
        if sub == 'state/throttle':
            try:
                tl.append((ts, int(payload.strip())))
            except ValueError:
                pass

    def commanded_at(t):
        lo_i, hi_i, val = 0, len(tl) - 1, 0
        while lo_i <= hi_i:
            mid = (lo_i + hi_i) // 2
            if tl[mid][0] <= t:
                val = tl[mid][1]
                lo_i = mid + 1
            else:
                hi_i = mid - 1
        return val

    cmds, expected = [], []
    cmds.append(f'# session replay: {name}')
    declared = False
    markers = 0
    last_dir = None

    for n, ts, sub, payload in rows:
        if sub == 'state/session_direction':
            d = payload.strip()
            if d in ('CW', 'CCW') and d != last_dir:
                cmds.append(f'dir {d}')
                last_dir = d
            continue

        if sub == 'state/direction':
            try:
                cmds.append(f'motor {int(payload.strip())}')
            except ValueError:
                pass
            continue

        d = as_json(payload)

        if sub == 'state/nav' and d and d.get('event') == 'DECLARED':
            if last_dir is None:
                # Direction arrived before this tool's window or was retained;
                # DECLARED carries it, so recover from the record itself.
                dd = d.get('dir')
                if dd in ('CW', 'CCW'):
                    cmds.append(f'dir {dd}')
                    last_dir = dd
            cmds.append(f'declare {d["mm"]}')
            declared = True
            expected.append({'line': n, 'event': 'DECLARED', 'mm': d['mm']})
            continue

        if sub == 'state/auto':
            v = payload.strip()
            if v in ('0', '1') and declared:
                cmds.append(f'auto {v}')
            continue

        if sub == 'mm/marker' and d:
            if not declared:
                continue                      # pre-declaration noise
            cmds.append(
                'event {dt} {pol} {pwm} {cmd} {peak} {dur} {drift}'.format(
                    dt=d.get('dt', 0), pol=d.get('obs', 'S'),
                    pwm=d.get('pwm', 0),
                    cmd=commanded_at(ts - d.get('ms', 0) / 1000.0),
                    peak=d.get('peak', 0), dur=d.get('ms', 0),
                    drift=d.get('drift', 0)))
            markers += 1
            expected.append({
                'line': n, 'event': 'MARKER', 'mm': d.get('mm'),
                'obs': d.get('obs'), 'timing_gate': d.get('timing_gate'),
                'dt': d.get('dt'), 'pwm': d.get('pwm'),
            })
            continue

        if sub == 'state/nav' and d and d.get('event') in DECISION_EVENTS:
            rec = {'line': n, 'event': d['event'], 'mm': d.get('mm')}
            for k in ('state', 'scores', 'leader', 'runner_up', 'margin',
                      'eval', 'viable', 'reason', 'offset', 'old_mm', 'new_mm',
                      'streak', 'obs', 'expected'):
                if k in d:
                    rec[k] = d[k]
            expected.append(rec)
            continue

        if sub == 'mm/no_quorum' and d:
            expected.append({'line': n, 'event': 'NO_QUORUM_SNAPSHOT',
                             'snapshot': d})
            continue

    cmds.append('dump')
    out = pathlib.Path(outdir)
    out.mkdir(parents=True, exist_ok=True)
    (out / f'{name}.replay').write_text('\n'.join(cmds) + '\n')
    (out / f'{name}.expected').write_text(json.dumps(expected, indent=1) + '\n')
    print(f'{name}: {markers} marker events, {len(expected)} expected records')
    return markers


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', required=True)
    ap.add_argument('--loco', required=True)
    ap.add_argument('--name', required=True)
    ap.add_argument('--outdir', default='fixtures')
    a = ap.parse_args()
    extract(a.capture, a.loco, a.name, a.outdir)


if __name__ == '__main__':
    main()
