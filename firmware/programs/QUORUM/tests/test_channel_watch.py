#!/usr/bin/env python3
"""1.16Ra channel-watch and radio-instrumentation tests.

WHY THIS FILE EXISTS. The 1.16Ra increment was first justified by "byte-
identical output across all 43 replays". That is a true statement about
REGRESSION and a worthless one about CORRECTNESS: the self-review round
established that the capture corpus never reaches ctoService() at all, so
not one replay rendered the new fields or entered the new branch. The
instrumentation was reviewed and unexercised.

These tests drive the watch deliberately through the harness commands
`wifi_channel`, `advance` and `cto`, and assert the four rules the review
round put into the firmware: RATE (1 Hz), RANGE (1..14 only), CONFIRM (three
consecutive readings), SCOPE (silent when CTO is off, and never touching the
operator warning slot).
"""

import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent


def run(harness, lines):
    out = subprocess.run([str(harness)], input='\n'.join(lines) + '\n',
                         capture_output=True, text=True, timeout=60)
    pubs = []
    for line in out.stdout.splitlines():
        if not line.startswith('{'):
            continue
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if 'pub' in row:
            pubs.append((row['pub'], row['payload']))
    return pubs


def cto_events(pubs):
    """Parsed state/cto payloads. A payload that will not parse is a failure
    in itself — that is the truncation bug the 288-byte buffer produced."""
    out = []
    for topic, payload in pubs:
        if topic.endswith('/state/cto'):
            out.append(json.loads(payload))          # deliberately unguarded
    return out


def warnings(pubs):
    return [p for t, p in pubs if t.endswith('/state/warning')]


def boot(channel=11):
    return ['dir CW', 'declare 40', f'wifi_channel {channel}', 'cto',
            'advance 1500', 'cto']


def settle(n=4):
    out = []
    for _ in range(n):
        out += ['advance 1500', 'cto']
    return out


def main():
    harness = sys.argv[1] if len(sys.argv) > 1 else '/tmp/quorum_harness'
    fails = []

    def check(name, cond, detail=''):
        print(f'    {"OK  " if cond else "FAIL"} {name}' +
              (f': {detail}' if detail and not cond else ''))
        if not cond:
            fails.append(f'channel watch: {name} — {detail}')

    # --- 1. latch on first association, no event ---------------------------
    ev = cto_events(run(harness, boot(11) + settle()))
    changed = [e for e in ev if e.get('event') == 'CTO_CHANNEL_CHANGED']
    status = [e for e in ev if e.get('event') == 'CTO_STATUS']
    check('first association latches silently', not changed,
          f'unexpected {changed}')
    check('status reports the channel', status and status[-1].get('ch') == 11,
          f'ch={status[-1].get("ch") if status else None}')

    # --- 2. the confirmation gate ------------------------------------------
    # ONE reading of a new channel, then back. A transient must not alarm.
    ev = cto_events(run(harness, boot(11) +
                        ['wifi_channel 6', 'advance 1500', 'cto',
                         'wifi_channel 11'] + settle()))
    check('one transient reading does not alarm',
          not [e for e in ev if e.get('event') == 'CTO_CHANNEL_CHANGED'])

    # TWO consecutive — still below the bar.
    ev = cto_events(run(harness, boot(11) +
                        ['wifi_channel 6',
                         'advance 1500', 'cto', 'advance 1500', 'cto',
                         'wifi_channel 11'] + settle()))
    check('two consecutive readings do not alarm',
          not [e for e in ev if e.get('event') == 'CTO_CHANNEL_CHANGED'])

    # THREE consecutive — exactly one event, correct from/to, counter at 1.
    ev = cto_events(run(harness, boot(11) + ['wifi_channel 6'] + settle(6)))
    changed = [e for e in ev if e.get('event') == 'CTO_CHANNEL_CHANGED']
    check('three consecutive readings alarm exactly once', len(changed) == 1,
          f'{len(changed)} events')
    if changed:
        c = changed[0]
        check('the alarm names both channels and counts once',
              c.get('from') == 11 and c.get('to') == 6 and c.get('changes') == 1,
              f'from={c.get("from")} to={c.get("to")} chg={c.get("changes")}')

    # --- 3. RANGE: out-of-band values are "unknown", never a channel -------
    for bad in (0, 15, 99, -1):
        ev = cto_events(run(harness, boot(11) +
                            [f'wifi_channel {bad}'] + settle(6)))
        ch = [e for e in ev if e.get('event') == 'CTO_CHANNEL_CHANGED']
        st = [e for e in ev if e.get('event') == 'CTO_STATUS']
        check(f'channel {bad} is treated as unknown, not a change', not ch,
              f'alarmed with {ch}')
        check(f'channel {bad} does not overwrite the reference',
              st and st[-1].get('ch') == 11,
              f'ch={st[-1].get("ch") if st else None}')

    # --- 4. SCOPE: silent when CTO is off ----------------------------------
    ev = cto_events(run(harness, boot(11) +
                        ['cto_off', 'wifi_channel 6'] + settle(6)))
    check('no alarm while CTO is off',
          not [e for e in ev if e.get('event') == 'CTO_CHANNEL_CHANGED'])

    # --- 5. SCOPE: the operator warning slot is never touched --------------
    # A diagnostic must not overwrite a live operator message; the warning
    # slot is single and carries SELF_RESOLVED's "BEGIN to resume" prompt.
    pubs = run(harness, boot(11) + ['wifi_channel 6'] + settle(6))
    check('the operator warning slot is not hijacked', not warnings(pubs),
          f'{warnings(pubs)}')

    # --- 6. every CTO payload is valid JSON carrying the new fields --------
    ev = cto_events(pubs)      # parses unguarded: truncation fails here
    st = [e for e in ev if e.get('event') == 'CTO_STATUS']
    check('status carries the instrumentation fields',
          st and all(k in st[-1] for k in ('txd', 'txf', 'ch', 'chg')),
          f'{sorted(st[-1]) if st else None}')

    return fails


if __name__ == '__main__':
    f = main()
    if f:
        print(f'\n{len(f)} failure(s)')
        sys.exit(1)
    print('    channel watch: all checks passed')
