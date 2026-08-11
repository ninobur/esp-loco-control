#!/usr/bin/env python3
"""Build synthetic QUORUM replay fixtures for cases the 2026-08-10 capture
does not contain, and declare what each one must do.

The capture is authoritative for what DID happen. These cover what must NOT
happen, plus the case the capture conspicuously lacks: a legitimate recovery.
The run's only adoption was wrong, so without a synthetic control the suite
could be satisfied by firmware that simply never adopts.

Construction:

  * Events run at a steady cruise so the gate sits in ACTIVE and the
    conservation test is not tripped by accident. pwm 90 gives
    v = 3.90*90 - 99.2 = 252 mm/s; at ~300 mm spacing one interval is ~1190 ms,
    so dt = 1200 ms is one clean interval. A deliberate duplicate uses dt 60,
    so the pair sums to about one interval — the signature the gate looks for.

  * START POSITIONS ARE NOT ARBITRARY. Whether a given displacement actually
    provokes an incident depends on the DNA under the wheels: an offset only
    disagrees where the sequence differs at that lag. Each start below was
    found by sweeping all 171 route positions through this very harness and
    taking one that exhibits the intended behaviour; the sweep counts are
    recorded so the choice is auditable rather than lucky. For the
    outside-the-fence cases roughly 40% of starts provoke NO_QUORUM at all —
    at the rest the displacement passes unnoticed or is absorbed, which is
    itself worth knowing and is why the position is stated, not hidden.

The expectations attached to each fixture are asserted by run_suite.py.
"""

import argparse
import json
import pathlib

DNA = [
    1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0, 0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
    0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1, 0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
    1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0, 1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
    1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0, 0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
    1,0,1,0,0,0,0,1,1,1,0,
]
DNA_N = 171
CRUISE_PWM = 90
CRUISE_DT = 1200
DUP_DT = 60


def pol(mm):
    return 'N' if DNA[mm % DNA_N] else 'S'


def ev(p, dt=CRUISE_DT, pwm=CRUISE_PWM, peak=80, dur=150, drift=0):
    return f'event {dt} {p} {pwm} {pwm} {peak} {dur} {drift}'


def displaced(start, shift, phantom=False, lead=6, tail=20):
    """A clean lead, a displacement, then honest travel.

    shift markers missed  -> odometer falls BEHIND -> positive offset
    shift phantom events  -> odometer runs AHEAD   -> negative offset
    """
    cmds = ['dir CW', f'declare {start}', 'auto 1']
    t = start
    for _ in range(lead):
        t = (t + 1) % DNA_N
        cmds.append(ev(pol(t)))
    if phantom:
        for _ in range(shift):          # locomotive stationary, odometer runs
            cmds.append(ev(pol(t)))
    else:
        t = (t + shift) % DNA_N         # markers pass unseen
    for _ in range(tail):
        t = (t + 1) % DNA_N
        cmds.append(ev(pol(t)))
    return cmds, t


FIXTURES = {}


def add(name, note, cmds, expect):
    FIXTURES[name] = {'note': note, 'cmds': cmds, 'expect': expect}


# --- the control the capture lacks -----------------------------------------
cmds, _ = displaced(0, 1)
add('syn_ordinary_recovery',
    'ONE missed marker at mm 0. Offset +1 is inside the fence and QUORUM must '
    'recover unaided. Sweep: 137/171 starts adopt +1 here. This is the control '
    'the capture lacks — its only adoption was wrong, so without this a '
    'navigator that never adopts would pass everything else.',
    cmds + ['dump'],
    {'must_contain': ['QUORUM_OPEN', 'QUORUM_ADOPTED', 'QUORUM_CLOSED'],
     'must_not_contain': ['NO_QUORUM'],
     'adopted_offset': 1,
     'final_state': 'NORMAL'})

# --- displacements the fence cannot express --------------------------------
cmds, _ = displaced(10, 8)
add('syn_missed_burst_outside_fence',
    'Eight dropped markers at mm 10 — incident A\'s true offset. +8 is outside '
    'the fence, so no candidate can be correct. Sweep: 66/171 starts reach '
    'NO_QUORUM; none of them adopt. Must stop, must never adopt.',
    cmds + ['snapshot', 'dump'],
    {'must_contain': ['QUORUM_OPEN', 'NO_QUORUM'],
     'must_not_contain': ['QUORUM_ADOPTED'],
     'final_state': 'NO_QUORUM'})

cmds, _ = displaced(9, 2, phantom=True)
add('syn_phantom_pair_outside_fence',
    'Two phantom events at mm 9 — incident C\'s mechanism exactly. The '
    'odometer runs 2 AHEAD; the fence allows only -1, so -2 cannot be '
    'expressed. Sweep: 66/171 starts reach NO_QUORUM. Must stop, must never '
    'adopt.',
    cmds + ['snapshot', 'dump'],
    {'must_contain': ['QUORUM_OPEN', 'NO_QUORUM'],
     'must_not_contain': ['QUORUM_ADOPTED'],
     'final_state': 'NO_QUORUM'})

cmds, _ = displaced(0, 5, phantom=True)
add('syn_phantom_five_outside_fence',
    'Five phantoms at mm 0 — incident C\'s compounded state (offset -5), the '
    'one the exact-window advisory is expected to identify. Sweep: 77/171 '
    'starts reach NO_QUORUM.',
    cmds + ['snapshot', 'dump'],
    {'must_contain': ['QUORUM_OPEN', 'NO_QUORUM'],
     'must_not_contain': ['QUORUM_ADOPTED'],
     'final_state': 'NO_QUORUM'})

# --- corrupted observation -------------------------------------------------
cmds = ['dir CW', 'declare 0', 'auto 1']
_t = 0
for _ in range(6):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
for _ in range(18):
    cmds.append(ev('S'))                       # detector latched low
add('syn_latched_polarity',
    'The detector latches and every reading returns the same polarity — '
    'incident B\'s mode. No offset explains it. Sweep: 73/171 starts reach '
    'NO_QUORUM with no adoption. Must stop, must never adopt, and (Phase 2) '
    'must advise nothing.',
    cmds + ['snapshot', 'dump'],
    {'must_contain': ['QUORUM_OPEN', 'NO_QUORUM'],
     'must_not_contain': ['QUORUM_ADOPTED'],
     'final_state': 'NO_QUORUM',
     'advisory': None})

# One corrupted bit inside an otherwise exact window. Built on the -5 case so
# it reaches NO_QUORUM, then one reading in the evidence window is flipped.
cmds = ['dir CW', 'declare 0', 'auto 1']
_t = 0
for _ in range(6):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
for _ in range(5):
    cmds.append(ev(pol(_t)))                   # five phantoms -> offset -5
for i in range(20):
    _t = (_t + 1) % DNA_N
    p = pol(_t)
    if i == 12:                                # exactly one corrupted reading
        p = 'N' if p == 'S' else 'S'
    cmds.append(ev(p))
add('syn_one_corrupt_bit',
    'The -5 displacement with exactly ONE reading corrupted, so the evidence '
    'ring is a near miss rather than an exact window. Guards the '
    'exact-or-silent contract: a single bad bit must silence the advisory, '
    'never shift it to a neighbouring marker.',
    cmds + ['snapshot', 'dump'],
    {'must_contain': ['NO_QUORUM'],
     'must_not_contain': ['QUORUM_ADOPTED'],
     'final_state': 'NO_QUORUM',
     'advisory': None})

# --- gate behaviour --------------------------------------------------------
cmds = ['dir CW', 'declare 20', 'auto 1']
_t = 20
for _ in range(6):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
cmds.append(ev(pol(_t), dt=DUP_DT))            # same magnet read twice
for _ in range(8):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
add('syn_duplicate_event',
    'One magnet read twice, the second arriving 60 ms later so the pair sums '
    'to about one interval. The conservation test must reject it as '
    'PHANTOM_REJECTED and leave the odometer alone; no incident may open.',
    cmds + ['dump'],
    {'must_contain': ['PHANTOM_REJECTED'],
     'must_not_contain': ['QUORUM_OPEN', 'QUORUM_ADOPTED', 'NO_QUORUM'],
     'final_mm': 34,
     'final_state': 'NORMAL'})

# --- boundaries ------------------------------------------------------------
cmds, _t = displaced(10, 8, tail=5)            # open an incident, then reverse
cmds.append('dump')
cmds.append('motor 0')                         # DIRECTION_REVERSE
cmds.append('dump')
for _ in range(8):
    _t = (_t - 1) % DNA_N
    cmds.append(ev(pol(_t)))
add('syn_reversal_mid_incident',
    'Direction reverses while an evaluation is open. applyDirection() must '
    'recompute navDir to CCW and step the odometer back along the OLD '
    'direction, because the next marker met after a reversal is the one just '
    'left. No adoption may survive the flip.',
    cmds + ['dump'],
    {'must_not_contain': ['QUORUM_ADOPTED'],
     'final_dir': 'CCW'})

cmds, _t = displaced(10, 8, tail=5)
cmds.append('dump')
cmds.append(f'declare {_t}')                   # operator reads the marker
cmds.append('dump')
for _ in range(6):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
add('syn_declaration_mid_incident',
    'The operator declares position while an evaluation is open. The '
    'declaration is authoritative: state returns to NORMAL, the incident and '
    'ring are cleared, and the retained snapshot is armed to CLEAR (2).',
    cmds + ['snapshot', 'dump'],
    {'must_not_contain': ['QUORUM_ADOPTED', 'NO_QUORUM'],
     'final_state': 'NORMAL',
     'snapshot_desired': 2})


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--outdir', default='fixtures')
    args = ap.parse_args()
    out = pathlib.Path(args.outdir)
    out.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, f in sorted(FIXTURES.items()):
        body = ([f'# synthetic fixture: {name}'] +
                [f'# {ln}' for ln in f['note'].split('. ') if ln] +
                ['# generated by make_synthetic.py — not from the capture'] +
                f['cmds'])
        (out / f'{name}.replay').write_text('\n'.join(body) + '\n')
        manifest[name] = {'note': f['note'], 'expect': f['expect']}
        n = sum(1 for l in f['cmds'] if l.startswith('event'))
        print(f'{name}: {n} events')
    (out / 'synthetic_manifest.json').write_text(
        json.dumps(manifest, indent=1) + '\n')


if __name__ == '__main__':
    main()
