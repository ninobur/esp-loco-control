#!/usr/bin/env python3
"""Circuit Express (decision 0038) — the mission lifecycle.

Written after review found two P1 defects in the first implementation, both of
which this file would have caught:

  * CE's severance was decoration. ceBegin() dissolved the pairing, but
    ctoEvaluateRoles() re-derives roles from geometry every 100 ms and the two
    trains are still inside CTO_PAIR_RANGE_MARKERS at that moment, so the pair
    re-latched immediately and the fleet held missions AND a pairing at once.
  * The LOCAL never exited CE. Termination was express-only, so the local ran
    at PWM 75 with 15 s dwells past the encounter, past re-pairing, for ever.

The lifecycle is therefore the thing under test, not the speeds.
"""
import json
import subprocess
import sys

PEER = 9950012


def run(harness, lines):
    out = subprocess.run([str(harness)], input='\n'.join(lines) + '\n',
                         capture_output=True, text=True, timeout=60)
    ce, events = [], []
    for line in out.stdout.splitlines():
        if not line.startswith('{'):
            continue
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if row.get('ce') is True:
            ce.append(row)
        elif 'pub' in row and row['pub'].endswith('/state/cto'):
            events.append(json.loads(row['payload']))
    return ce, events


def paired(mm, peer_mm, direction='CW'):
    """Me at mm, peer at peer_mm, one ctoService pass to latch a role."""
    return ['advance 5000', 'dir ' + direction, f'declare {mm}',
            f'peer {PEER} {peer_mm} {direction}', 'cto']


def peer_at(peer_mm, direction='CW'):
    return [f'peer {PEER} {peer_mm} {direction}', 'cto']


def main():
    harness = sys.argv[1] if len(sys.argv) > 1 else '/tmp/quorum_harness'
    fails = []

    def check(name, cond, detail=''):
        print(f'    {"OK  " if cond else "FAIL"} {name}' +
              (f': {detail}' if detail and not cond else ''))
        if not cond:
            fails.append(f'ce: {name} — {detail}')

    def evs(events, name):
        return [e for e in events if e.get('event') == name]

    # --- assignment ---------------------------------------------------------
    # CW, mm increases. Peer at 52 is 12 ahead of me at 40 -> I FOLLOW -> LOCAL.
    ce, ev = run(harness, paired(40, 52) + ['ce', 'ce_dump'])
    check('follower takes the LOCAL mission',
          ce and ce[-1]['mission'] == 'LOCAL', ce[-1] if ce else 'no dump')
    check('LOCAL cruises 75', ce and ce[-1]['cruise'] == 75,
          ce[-1]['cruise'] if ce else '')
    check('LOCAL keeps the ordinary 15 s dwell',
          ce and ce[-1]['dwell'] == 15000, ce[-1]['dwell'] if ce else '')

    # Peer at 28 is 12 BEHIND me at 40 -> I LEAD -> EXPRESS.
    ce, ev = run(harness, paired(40, 28) + ['ce', 'ce_dump'])
    check('leader takes the EXPRESS mission',
          ce and ce[-1]['mission'] == 'EXPRESS', ce[-1] if ce else 'no dump')
    check('EXPRESS cruises 110', ce and ce[-1]['cruise'] == 110,
          ce[-1]['cruise'] if ce else '')
    check('EXPRESS dwells 5 s', ce and ce[-1]['dwell'] == 5000,
          ce[-1]['dwell'] if ce else '')
    check('CE_BEGIN is published', len(evs(ev, 'CE_BEGIN')) == 1,
          [e.get('event') for e in ev])

    # --- refusal ------------------------------------------------------------
    # No peer at all: no role, nothing to sever, no basis for who runs fast.
    ce, ev = run(harness, ['advance 5000', 'dir CW', 'declare 40', 'cto',
                           'ce', 'ce_dump'])
    check('CE refused without a role',
          ce and ce[-1]['mission'] == 'NONE', ce[-1] if ce else 'no dump')
    check('refusal says why', len(evs(ev, 'CE_REFUSED')) == 1,
          [e.get('event') for e in ev])

    # --- P1-A: severance must STICK -----------------------------------------
    # The pair is dissolved at assignment, and the trains are still well inside
    # pairing range. Without the formation inhibit the role re-latches on the
    # very next pass. Ticking repeatedly at unchanged geometry is the test.
    ce, ev = run(harness, paired(40, 28) + ['ce'] + peer_at(28) * 6
                 + ['ce_dump'])
    check('severance holds: no re-pair while the mission runs',
          ce and ce[-1]['role'] == 'NONE', ce[-1] if ce else 'no dump')
    check('mission survives the ticks it was severed for',
          ce and ce[-1]['mission'] == 'EXPRESS', ce[-1] if ce else '')

    # --- P1-B: both locomotives end, and only after separating ---------------
    # Still adjacent -> CE must NOT end, or it dies ~100 ms after it began.
    ce, ev = run(harness, paired(40, 28) + ['ce'] + peer_at(28) * 4
                 + ['ce_dump'])
    check('CE does not end while the fleet is still closed up',
          ce and ce[-1]['mission'] == 'EXPRESS', ce[-1] if ce else '')
    check('no premature CE_END', not evs(ev, 'CE_END'),
          [e.get('event') for e in ev])

    # Separate well beyond the slow threshold, then close up again.
    apart = peer_at(120) * 3          # ~80 markers away: clearly separated
    closed = peer_at(34) * 3          # back inside CTO_SLOW_GAP_MARKERS
    ce, ev = run(harness, paired(40, 28) + ['ce'] + apart + closed
                 + ['ce_dump'])
    check('EXPRESS ends CE once the fleet closes up again',
          ce and ce[-1]['mission'] == 'NONE', ce[-1] if ce else '')
    check('CE_END is published with a reason', len(evs(ev, 'CE_END')) == 1,
          [e.get('event') for e in ev])

    # The same must hold for the LOCAL — the defect that motivated this file.
    ce, ev = run(harness, paired(40, 52) + ['ce'] + peer_at(150) * 3
                 + peer_at(46) * 3 + ['ce_dump'])
    check('LOCAL ends CE too (not express-only)',
          ce and ce[-1]['mission'] == 'NONE', ce[-1] if ce else '')

    # --- NO_QUORUM freezes the lifecycle, it does not advance it -------------
    # Both branches act on gap evidence measured from navMm. A locomotive that
    # does not know where it is has none, so the mission must neither arm nor
    # end. Found in self-review before flashing: guarding only inside
    # ceNearPeer() made it WORSE, because the arming branch reads
    # !ceNearPeer() and would have armed on the stale odometer.
    ce, ev = run(harness, paired(40, 28) + ['ce', 'noquorum']
                 + peer_at(120) * 3 + peer_at(34) * 3 + ['ce_dump'])
    check('NO_QUORUM does not let CE arm-and-end on a stale odometer',
          ce and ce[-1]['mission'] == 'EXPRESS', ce[-1] if ce else '')
    check('no CE_END while position is unusable', not evs(ev, 'CE_END'),
          [e.get('event') for e in ev])

    # --- operator exits ------------------------------------------------------
    ce, ev = run(harness, paired(40, 28) + ['ce', 'cto_cmd clear', 'ce_dump'])
    check('cto clear ends CE', ce and ce[-1]['mission'] == 'NONE',
          ce[-1] if ce else '')
    ce, ev = run(harness, paired(40, 28) + ['ce', 'cto_cmd off', 'ce_dump'])
    check('cto off ends CE', ce and ce[-1]['mission'] == 'NONE',
          ce[-1] if ce else '')

    # --- no mission, no change ----------------------------------------------
    ce, _ = run(harness, paired(40, 28) + ['ce_dump'])
    check('without CE the cruise constant is untouched',
          ce and ce[-1]['cruise'] == 90, ce[-1]['cruise'] if ce else '')

    print('    ce mission: ' + ('all checks passed' if not fails
                                else f'{len(fails)} failure(s)'))
    return fails


if __name__ == '__main__':
    sys.exit(1 if main() else 0)
