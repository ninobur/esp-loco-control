#!/usr/bin/env python3
"""CTO role formation and 0037 nose-tail order inversion.

WHY THIS FILE EXISTS. Until 2026-08-16 the CTO pairing layer had NO test
coverage of any kind: the esp_now shim drops every send and never fires a
receive callback, so the peer registry was permanently empty and every
pairing path was dead code under replay. Roles, gaps, dwell and the
dissolution rules were argued in review and never once executed.

The harness `peer` / `peer_echo` / `cto` / `cto_dump` commands close that,
and these tests pin both the pre-existing behaviour (so 0037 cannot silently
break it) and the new rule.

0037, operator ruling 2026-08-16: a pairing whose nose-tail order has
inverted must dissolve. Observed in the field — Otto held LEADER while
sitting 12 markers BEHIND Toby and braking for him, because the pair latched
before Toby was paused and Otto then lapped the whole route. A latched role
is a claim about geometry; when the arcs contradict it the claim is false.
"""

import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ME, PEER = 9950011, 9950012          # harness compiles Otto's profile


def run(harness, lines):
    out = subprocess.run([str(harness)], input='\n'.join(lines) + '\n',
                         capture_output=True, text=True, timeout=60)
    states, events = [], []
    for line in out.stdout.splitlines():
        if not line.startswith('{'):
            continue
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if row.get('cto') is True:
            states.append(row)
        elif 'pub' in row and row['pub'].endswith('/state/cto'):
            events.append(json.loads(row['payload']))   # unguarded: must parse
    return states, events


def at(mm, peer_mm, direction='CW'):
    """Me at mm, peer at peer_mm, both travelling `direction`.

    The leading `advance` is not decoration. ctoAcceptEcho() stamps
    echoRxMs = millis(), and the echo validity test rejects echoRxMs == 0 as
    "never received" — so on a harness clock that starts at zero, a perfectly
    good echo is discarded. A locomotive is never at millis() == 0 by the
    time it pairs; the fixture reproduces that rather than special-casing it.
    """
    return ['advance 5000', 'dir ' + direction, f'declare {mm}',
            f'peer {PEER} {peer_mm} {direction}', 'cto']


def ticks(n):
    return ['cto'] * n


def main():
    harness = sys.argv[1] if len(sys.argv) > 1 else '/tmp/quorum_harness'
    fails = []

    def check(name, cond, detail=''):
        print(f'    {"OK  " if cond else "FAIL"} {name}' +
              (f': {detail}' if detail and not cond else ''))
        if not cond:
            fails.append(f'cto roles: {name} — {detail}')

    def role(states):
        return states[-1]['role'] if states else None

    # --- formation, the pre-0037 contract ----------------------------------
    # CW: mm increases. A peer at 52 is 12 ahead of me at 40.
    s, _ = run(harness, at(40, 52) + ['cto_dump'])
    check('peer ahead within range -> FOLLOWER', role(s) == 'FOLLOWER', role(s))

    s, _ = run(harness, at(40, 28) + ['cto_dump'])
    check('peer behind within range -> LEADER', role(s) == 'LEADER', role(s))

    s, _ = run(harness, at(40, 90) + ['cto_dump'])
    check('peer beyond pairing range -> no latch', role(s) == 'NONE', role(s))

    # A peer whose own navigation is discredited is not a partner.
    s, _ = run(harness, ['advance 5000', 'dir CW', 'declare 40',
                         f'peer {PEER} 52 CW 0', 'cto', 'cto_dump'])
    check('peer in NO_QUORUM -> no latch', role(s) == 'NONE', role(s))

    # Opposite direction is not a pairing either.
    s, _ = run(harness, ['advance 5000', 'dir CW', 'declare 40',
                         f'peer {PEER} 52 CCW', 'cto', 'cto_dump'])
    check('peer travelling the other way -> no latch', role(s) == 'NONE',
          role(s))

    # --- 0037: THE FIELD SCENARIO ------------------------------------------
    # Latch LEADER with the peer behind, then the peer ends up ahead — which
    # is what a lap does on a loop while the other locomotive is paused.
    seq = (at(40, 28) + ['cto_dump'] +
           [f'peer {PEER} 52 CW'] + ['cto', 'cto_dump'] * 4)
    s, ev = run(harness, seq)
    inverted = [e for e in ev if e.get('event') == 'CTO_ORDER_INVERTED']
    unpaired = [e for e in ev if e.get('event') == 'CTO_UNPAIRED']
    check('inversion is detected exactly once', len(inverted) == 1,
          f'{len(inverted)}')
    check('the dissolution names the reason',
          unpaired and unpaired[0].get('why') == 'ORDER_INVERTED',
          f'{unpaired}')
    check('the event reports the role it dissolved',
          inverted and inverted[0].get('was') == 'LEADER',
          f'{inverted}')
    check('the role is re-derived from real geometry',
          role(s) == 'FOLLOWER', role(s))

    # --- 0037: the confirmation gate ---------------------------------------
    # One pass and two passes must NOT dissolve. Three must.
    for n, want in ((1, False), (2, False), (3, True)):
        seq = at(40, 28) + [f'peer {PEER} 52 CW'] + ticks(n) + ['cto_dump']
        s, ev = run(harness, seq)
        fired = bool([e for e in ev if e.get('event') == 'CTO_ORDER_INVERTED'])
        check(f'{n} confirming pass(es) -> {"dissolve" if want else "hold"}',
              fired == want, f'fired={fired}')

    # --- 0037: the hysteresis, so a near-diametric pair cannot chatter -----
    # 171 markers round. A peer ~85 away sits at gap_ahead ~ gap_behind, where
    # the nearer side is noise. The margin must suppress it entirely.
    for peer_mm in (124, 125, 126, 127):
        seq = at(40, 28) + [f'peer {PEER} {peer_mm} CW'] + ticks(8) + ['cto_dump']
        s, ev = run(harness, seq)
        check(f'near-diametric peer at mm {peer_mm} does not dissolve',
              not [e for e in ev if e.get('event') == 'CTO_ORDER_INVERTED'])

    # --- pre-existing dissolutions still work ------------------------------
    seq = at(40, 52) + ['motor 0', 'cto', 'cto_dump']
    s, ev = run(harness, seq)
    whys = [e.get('why') for e in ev if e.get('event') == 'CTO_UNPAIRED']
    check('my direction change still dissolves',
          'DIRECTION_CHANGED_SINCE_LATCH' in whys, f'{whys}')

    seq = at(40, 52) + [f'peer {PEER} 52 CCW', 'cto', 'cto_dump']
    s, ev = run(harness, seq)
    whys = [e.get('why') for e in ev if e.get('event') == 'CTO_UNPAIRED']
    check("the partner's direction change still dissolves",
          'PARTNER_DIRECTION_CHANGED' in whys, f'{whys}')

    # The two OPERATOR dissolutions, through the real command handler. The
    # first version of this suite drove ctoEnabled directly and so never
    # executed either path — "all four dissolutions tested" was untrue.
    seq = at(40, 52) + ['cto_cmd clear', 'cto_dump']
    s, ev = run(harness, seq)
    whys = [e.get('why') for e in ev if e.get('event') == 'CTO_UNPAIRED']
    check('cmd/cto clear dissolves and forgets the peer',
          'OPERATOR_CLEAR' in whys and role(s) == 'NONE' and
          s[-1]['expected'] == 0, f'{whys} role={role(s)}')

    seq = at(40, 52) + ['cto_cmd off', 'cto_dump']
    s, ev = run(harness, seq)
    whys = [e.get('why') for e in ev if e.get('event') == 'CTO_UNPAIRED']
    check('cmd/cto off dissolves', 'CTO_OFF' in whys and role(s) == 'NONE',
          f'{whys} role={role(s)}')

    # The strict parser: "clear" disarms fleet protection, so anything after
    # the token must be refused rather than generously interpreted.
    seq = at(40, 52) + ['cto_cmd clear anything', 'cto_dump']
    s, ev = run(harness, seq)
    check('a command with trailing content is refused',
          [e for e in ev if e.get('why') == 'TRAILING_CONTENT'] and
          role(s) == 'FOLLOWER', f'role={role(s)}')

    # A partner that merely goes quiet must NOT dissolve — that is the fleet
    # stop's jurisdiction under 0031, and 0037 must not poach it. Sixty
    # seconds of silence, well past CTO_PEER_STALE_MS (3000); the first
    # version of this check used 10 s and proved only that the freshness
    # guard short-circuits.
    seq = at(40, 52) + ['advance 60000'] + ticks(40) + ['cto_dump']
    s, ev = run(harness, seq)
    check('a long-silent partner does not dissolve the pairing',
          not [e for e in ev if e.get('event') == 'CTO_UNPAIRED'] and
          role(s) == 'FOLLOWER', f'role={role(s)}')

    # THE STALENESS-GAP DEFECT (found in review, reproduced, now fixed).
    # Two inverted passes, then the partner goes silent long enough to be
    # stale, then ONE fresh inverted pass. Three samples, but not three
    # CONSECUTIVE observations — the middle of the run was unobserved, and
    # the counter used to survive it and dissolve on the very next pass.
    seq = (at(40, 28) + [f'peer {PEER} 52 CW'] + ticks(2) +
           ['advance 8000'] + ticks(3) +
           [f'peer {PEER} 52 CW', 'cto', 'cto_dump'])
    s, ev = run(harness, seq)
    check('a staleness gap restarts the confirmation run',
          not [e for e in ev if e.get('event') == 'CTO_ORDER_INVERTED'],
          f'{[e.get("event") for e in ev]}')

    # A partner whose own navigation is discredited must not move the role
    # either — its boundaries are derived from a position it does not
    # believe, and a discredited partner is 0031's business.
    seq = at(40, 28) + [f'peer {PEER} 52 CW 0'] + ticks(8) + ['cto_dump']
    s, ev = run(harness, seq)
    check('a partner in NO_QUORUM cannot invert the pairing',
          not [e for e in ev if e.get('event') == 'CTO_ORDER_INVERTED'] and
          role(s) == 'LEADER', f'role={role(s)}')

    # --- the dwell actually follows the role -------------------------------
    # This is the ONLY behaviour a role drives, so it is the whole reason the
    # inversion mattered: the 20 s platform courtesy was being paid by the
    # wrong locomotive.
    # 1.14A, "the leader stops waiting for the follower": the confirmed
    # follower dwells SHORTER (5 s), not longer. Asserting the code, not the
    # comment — this test first failed against a comment claiming 20 s, which
    # turned out to be stale wording reintroduced by a rebuild.
    seq = at(40, 52) + [f'peer_echo {PEER} LEADER {ME}', 'cto', 'cto_dump']
    s, _ = run(harness, seq)
    check('confirmed follower gets the short follower dwell',
          s and s[-1]['echo_confirmed'] == 1 and s[-1]['dwell_ms'] == 5000,
          f'confirmed={s[-1]["echo_confirmed"] if s else None} '
          f'dwell={s[-1]["dwell_ms"] if s else None}')

    # An UNCONFIRMED follower must fall back to solo rules — the echo is what
    # licenses the shorter dwell, not the latch alone.
    seq = at(40, 52) + ['cto', 'cto_dump']
    s, _ = run(harness, seq)
    check('unconfirmed follower uses the ordinary dwell',
          s and s[-1]['echo_confirmed'] == 0 and s[-1]['dwell_ms'] == 15000,
          f'dwell={s[-1]["dwell_ms"] if s else None}')

    seq = at(40, 28) + [f'peer_echo {PEER} FOLLOWER {ME}', 'cto', 'cto_dump']
    s, _ = run(harness, seq)
    check('confirmed leader keeps the ordinary dwell',
          s and s[-1]['dwell_ms'] == 15000,
          f'dwell={s[-1]["dwell_ms"] if s else None}')

    return fails


if __name__ == '__main__':
    f = main()
    if f:
        print(f'\n{len(f)} failure(s)')
        sys.exit(1)
    print('    cto roles: all checks passed')
