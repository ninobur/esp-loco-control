#!/usr/bin/env python3
"""IR Test A deterministic tests (spec §11.2).

Drives the REAL validation chain (irAcceptFrame) and loop-owned acceptance
(serviceIrRx) through the harness `ir` / `ir_raw` / `ir_dump` commands, and
the observation path through ordinary marker events.

What is NOT covered here, stated rather than implied away: live queue
pressure under a broker outage (bench/field item), and the exact route-span
values (asserted structurally — k0==0, strictly increasing — because
spacingMm lives in PROGMEM and the harness does not export it; the field
gate compares real distances).
"""

import json
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent


def run(harness, lines):
    out = subprocess.run([str(harness)], input='\n'.join(lines) + '\n',
                         capture_output=True, text=True, timeout=60)
    irs, pubs = [], []
    for line in out.stdout.splitlines():
        if not line.startswith('{'):
            continue
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if row.get('ir') is True:
            irs.append(row)
        elif 'pub' in row:
            pubs.append(row)
    return irs, pubs


def mmspeed(pubs):
    out = []
    for r in pubs:
        if r['pub'].endswith('/mm/speed'):
            out.append(json.loads(r['payload']))   # unguarded: must parse
    return out


def main():
    harness = sys.argv[1] if len(sys.argv) > 1 else '/tmp/quorum_harness'
    fails = []

    def check(name, cond, detail=''):
        print(f'    {"OK  " if cond else "FAIL"} {name}' +
              (f': {detail}' if detail and not cond else ''))
        if not cond:
            fails.append(f'ir test a: {name} — {detail}')

    BOOT = ['dir CW', 'declare 40']

    # --- the eight validation rejects, each with its own literal counter ----
    muts = [('len', 'blen'), ('magic', 'bver'), ('ver', 'bver'),
            ('bytes', 'bwire'), ('resv', 'bwire'), ('src', 'bsrc'),
            ('sensor', 'bsen'), ('target', 'btgt'), ('enum', 'benum'),
            ('enc', 'benc')]
    seq = BOOT + [f'ir_raw {m}' for m, _ in muts] + ['ir_dump']
    irs, _ = run(harness, seq)
    d = irs[-1]
    for ctr, want in (('blen', 1), ('bver', 2), ('bwire', 2), ('bsrc', 1),
                      ('bsen', 1), ('btgt', 1), ('benum', 1), ('benc', 1)):
        check(f'reject counter {ctr} == {want}', d.get(ctr) == want,
              f'{d.get(ctr)}')
    check('nothing malformed was accepted', d['acc'] == 0, f"acc={d['acc']}")

    # --- sequence discipline (§5.3) -----------------------------------------
    seq = BOOT + ['ir 1 7 100 3 -1', 'ir 1 7 100 3 -1', 'ir 0 7 100 3 -1',
                  'ir 5 7 120 3 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    d = irs[-1]
    check('duplicate counted and rejected', d['dup'] == 1 and d['acc'] == 2)
    check('out-of-order counted and rejected', d['ooo'] == 1)
    check('sequence gap counted', d['gaps'] == 3, f"gaps={d['gaps']}")

    # sequence WRAP: 0xFFFFFFFE -> 2 is forward under wrap-safe arithmetic.
    seq = BOOT + ['ir 4294967294 7 100 3 -1', 'ir 2 7 110 3 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    check('sequence wrap accepted as forward', irs[-1]['acc'] == 2,
          f"acc={irs[-1]['acc']}")

    # pulse-count WRAP is forward, not a regression (§12).
    seq = BOOT + ['ir 1 7 4294967290 3 -1', 'ir 2 7 5 3 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    check('pulse wrap accepted, not a regression',
          irs[-1]['acc'] == 2 and irs[-1]['regress'] == 0,
          f"acc={irs[-1]['acc']} regress={irs[-1]['regress']}")

    # a REAL regression inside one epoch is rejected and counted.
    seq = BOOT + ['ir 1 7 100 3 -1', 'ir 2 7 60 3 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    check('pulse regression rejected', irs[-1]['regress'] == 1 and
          irs[-1]['acc'] == 1)

    # --- sensor reboot: new epoch, no cross-epoch delta (§5.3) --------------
    seq = BOOT + ['ir 100 7 5000 3 -1', 'ir 1 9 3 3 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    check('reboot opens a new epoch', irs[-1]['reboot'] == 1 and
          irs[-1]['acc'] == 2)

    # --- freshness and speed truth (§6) --------------------------------------
    seq = BOOT + ['ir 1 7 100 3 25000', 'ir_dump', 'advance 600', 'ir_dump']
    irs, _ = run(harness, seq)
    check('fresh VALID is numeric', irs[0]['numeric'] == 1 and
          irs[0]['mmps'] == 250.0, f"{irs[0]}")
    check('stale VALID becomes LINK_STALE with null',
          irs[1]['state'] == 'LINK_STALE' and irs[1]['numeric'] == 0)

    seq = BOOT + ['ir 1 7 100 5 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    check('STOPPED is numeric zero', irs[0]['state'] == 'STOPPED' and
          irs[0]['numeric'] == 1 and irs[0]['mmps'] == 0.0)

    # No other state may be numeric — MARGINAL with sentinel:
    seq = BOOT + ['ir 1 7 100 1 -1', 'ir_dump']
    irs, _ = run(harness, seq)
    check('MARGINAL is null, never zero', irs[0]['numeric'] == 0)

    # --- mm/speed event records (§9.3), marker speed (§7) --------------------
    # Fresh IR just before each Hall event so projection succeeds (150 ms cap).
    ev = 'event 1200 {p} 90 90 150 170 0'
    DNA = None
    consts = subprocess.run([str(harness)], input='constants\n',
                            capture_output=True, text=True).stdout
    DNA = json.loads(consts.splitlines()[0])['dna']

    def pol(mm):
        return 'N' if DNA[mm % 171] else 'S'

    # Clock choreography matters: `event <dt>` advances the harness clock by
    # dt BEFORE detection, so a snapshot injected before it ages by the full
    # dt. Advance first, inject, then a short event — the snapshot is 100 ms
    # old at detection, inside the 150 ms projection cap.
    seq = ['dir CW', 'declare 40']
    t = 40
    for i in range(4):
        t += 1
        seq += ['advance 1100', f'ir {i+1} 7 {100+i*31} 3 25000',
                'event 100 %s 90 90 150 170 0' % pol(t)]
    irs, pubs = run(harness, seq + ['ir_dump'])
    recs = mmspeed(pubs)
    check('one mm/speed record per event', len(recs) == 4, f'{len(recs)}')
    r = recs[-1]
    check('record is revision 1 OBSERVE_ONLY',
          r['rev'] == 1 and r['agree'] == 'OBSERVE_ONLY')
    check('marker speed computed on second-and-later accepts',
          recs[0]['mmps'] is None and
          recs[1]['mmps'] is not None, f'{recs[0]["mmps"]}')
    check('route hypotheses k0..k4 present, k0 zero, increasing',
          r['k0'] == 0 and
          r['k1'] < r['k2'] < r['k3'] < r['k4'])
    check('IR distance present with fresh sensor', r['irv'] == 1 and
          r['irmm'] is not None, f'{r}')

    # wrap: CCW from mm 2 crosses 000/170 within k=4.
    seq = ['dir CCW', 'declare 2', 'ir 1 7 100 3 25000',
           'event 100 %s 90 90 150 170 0' % pol(1)]
    irs, pubs = run(harness, seq)
    recs = mmspeed(pubs)
    check('route spans cross the 170/000 wrap without error',
          recs and recs[0]['k4'] > recs[0]['k3'] > 0)

    # --- quarantine revisions (§8.3/§9.3): held -> discarded -----------------
    seq = ['dir CW', 'declare 40']
    t = 40
    for i in range(9):
        t += 1
        seq.append('event 2400 %s 90 90 180 175 0' % pol(t))
    opp = 'N' if pol(t + 1) == 'S' else 'S'
    seq += ['event 200 %s 90 90 40 45 0' % opp,        # floor phantom: held
            'event 2200 %s 90 90 150 170 0' % pol(t + 1)]  # successor convicts
    irs, pubs = run(harness, seq)
    recs = mmspeed(pubs)
    rev2 = [r for r in recs if r['rev'] == 2]
    check('held-then-discarded event gets a revision-2 record',
          len(rev2) == 1 and rev2[0]['disp'] == 'QUARANTINE_DISCARDED',
          f'{[(r["rev"], r["disp"]) for r in recs]}')

    # --- observer resets (§7): declaration and direction --------------------
    seq = ['dir CW', 'declare 40', 'ir 1 7 100 3 25000',
           ev.format(p=pol(41)), 'ir 2 7 131 3 25000', ev.format(p=pol(42)),
           'declare 42', 'ir_dump']
    irs, _ = run(harness, seq)
    check('declaration resets marker speed and IR anchor',
          irs[-1]['mm_valid'] == 0 and irs[-1]['anchor'] == 0)

    # --- static no-authority audit (§3, §11.2) --------------------------------
    src = (HERE.parent / 'QUORUM.ino').read_text()
    m = re.search(r'// ={70,}\n// IR TEST A — Stage-A receiver.*?'
                  r'#endif  // IR_TEST_A_ON — the disabled-build stubs live',
                  src, re.S)
    check('IR section located for audit', bool(m))
    if m:
        # Comments quote the forbidden list verbatim (that is the point of
        # §3's comment), so the audit strips comments and then looks for
        # actual CALLS and ASSIGNMENTS, not words.
        body = re.sub(r'//[^\n]*', '', m.group(0))
        calls = re.findall(
            r'\b(requestPwmOver|requestPwm|stationSetPhase|navDeclare|'
            r'enterNoQuorum|applyDirection|ctoDissolve|ctoLatch)\s*\(', body)
        assigns = re.findall(
            r'\b(navMm|navState|navDir|commandedPwm|actualPwm|autoRunning|'
            r'autoEnrolled|stPhase|ctoFleetHold|estopped)\s*=(?!=)', body)
        check('IR section makes no motor/navigation call', not calls,
              f'{calls}')
        check('IR section assigns no motor/navigation state', not assigns,
              f'{assigns}')

    return fails


if __name__ == '__main__':
    f = main()
    if f:
        print(f'\n{len(f)} failure(s)')
        sys.exit(1)
    print('    ir test a: all checks passed')
