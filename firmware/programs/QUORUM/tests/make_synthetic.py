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
    'One magnet read twice: the duplicate arrives at DUP_DT=60 ms, far below '
    'the 350 ms physical floor. 1.16: QUARANTINED at once, then DISCARDED when '
    'the next genuine marker fits the phantom hypothesis. The odometer never '
    'advances for it and no disagreement is recorded — compare the 1.13-1.14 '
    'behaviour, where the conservation gate caught it only if the PWM model '
    'happened to be right (PHANTOM_REJECTED), and admitted it when wrong.',
    displaced(40, 0)[0][:9] + [ev(pol(46), dt=DUP_DT, peak=38, dur=40),
                               ev(pol(47)), ev(pol(48)), ev(pol(49)), 'dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_disagree': 0})

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


# --- the advisory is HARD_BOUND-only (decision 0023) -----------------------
# buildNoQuorumSnapshot() serves all three terminal reasons. These two cases
# pin the scope, because a reason gate that silently stopped working would
# otherwise be invisible: the advisory would simply appear more often.

# Three phantoms at mm 0 reach HARD_BOUND at navMm 24 with a full ring and an
# exact match at marker 21. Forcing a second terminal entry immediately after
# leaves the ring and navMm untouched, so the ONLY difference between the two
# snapshots is the reason. HARD_BOUND must advise 21; FORCED_BY_FIXTURE must
# advise nothing while still reporting advn 12 — proving the reason silenced
# it, not a short ring.
cmds = ['dir CW', 'declare 0', 'auto 1']
_t = 0
for _ in range(6):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
for _ in range(3):
    cmds.append(ev(pol(_t)))
for _ in range(15):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
cmds.append('noquorum FORCED_BY_FIXTURE')
add('syn_forced_by_fixture_no_advisory',
    'An operator-forced terminal entry immediately after a HARD_BOUND, with '
    'the same full ring and the same navMm. HARD_BOUND advises marker 21; the '
    'forced entry must advise nothing, because the ring behind a forced state '
    'can hold anything — including readings from an unrelated stretch — and a '
    'confident marker offered on that basis is the wrong hint this advisory '
    'exists to avoid being.',
    cmds + ['dump'],
    # The stream adopts and reopens once on its way to HARD_BOUND. That is
    # incidental — this fixture exists for the advisory scope, and the two
    # sequences are the assertion: same ring (advn 12 both times), same navMm,
    # advisory present under HARD_BOUND and absent under the forced reason.
    {'must_contain': ['NO_QUORUM'],
     'terminal_reasons': ['HARD_BOUND', 'FORCED_BY_FIXTURE'],
     'advisory_sequence': [21, None],
     'advn_sequence': [12, 12]})

# Two adoptions, both contradicted: handleFailedAdoption() fires twice and the
# second failure enters NO_QUORUM by its own path. Found by sweeping start
# positions and displacement triples through this harness; only this
# combination reaches it.
cmds = ['dir CW', 'declare 9', 'auto 1']
_t = 9
for _ in range(5):
    _t = (_t + 1) % DNA_N
    cmds.append(ev(pol(_t)))
for burst, tail in ((2, 4), (1, 4), (1, 18)):
    for _ in range(burst):
        cmds.append(ev(pol(_t)))          # phantoms: odometer runs ahead
    for _ in range(tail):
        _t = (_t + 1) % DNA_N
        cmds.append(ev(pol(_t)))
add('syn_second_adoption_failed',
    'A genuine SECOND_ADOPTION_FAILED, reached through handleFailedAdoption() '
    'rather than forced. That path REBASES the evidence ring to undo the '
    'failed adoption and rescores only three entries, leaving evalCount 3 '
    'while evRingLen may still be 12 — the code calls it the uncorrected '
    'frame. The advisory must stay silent there: the navigator has already '
    'been wrong about position once in this incident. '
    'HONEST LIMIT: this case proves the path is reached and is silent, but it '
    'does NOT discriminate the reason gate — the pre-fix firmware advised null '
    'here too. A sweep of 21375 synthetic streams (171 starts x 5 x 5 x 5 '
    'displacements) found no SECOND_ADOPTION_FAILED whose ring would have '
    'matched exactly, which appears structural: a ring that matches a window '
    'exactly is one that would have resolved rather than failed twice. '
    'syn_forced_by_fixture_no_advisory is what proves the gate.',
    cmds + ['dump'],
    {'must_contain': ['QUORUM_ADOPTED', 'QUORUM_REOPENED', 'NO_QUORUM'],
     'terminal_reason': 'SECOND_ADOPTION_FAILED',
     'advisory': None})


# --- the two close-pair orderings, from Toby QUORUM_1_6, 2026-08-11 ---------
# PR #4 / field-records/20260811_TOBY_QUORUM_1_6_CROSS_LOCO_FINDINGS.md.
#
# Toby reproduced Otto's Grillers phantom on a different locomotive with a
# different Hall installation, which places the source at the railway rather
# than in Otto's sensor. It also supplied the ordering that a timing-only rule
# cannot resolve. Both are pinned here because they differ in the thing that
# matters and NOT in the event count — so a suite that only counted accepted
# events would call them the same.
#
# Lead-in interval matters and is not cosmetic. On Toby the Grillers weak event
# was accepted only because the preceding interval was long (2532 ms,
# decelerating into the station); at an ordinary 1200 ms lead-in the same pair
# is rejected and the failure does not reproduce.

_TGT = 7                      # map polarity at mm 7 is S, mirroring Toby's mm 84
def _pair(lead, pair, n=6):
    """n lead events from mm 1, then the pair (whose genuine member occupies
    mm n+1), then five honest tail events from n+2. n=6 keeps the historical
    fixtures byte-stable; the conjunction fixtures need n=9 because the
    credential medians arm at eight accepted samples — exactly as on the
    railway, where history always exists by the time a phantom appears."""
    cmds = ['dir CW', 'declare 0', 'auto 1']
    t = 0
    for _ in range(n):
        t = (t + 1) % DNA_N
        cmds.append(ev(pol(t), dt=lead, peak=180, dur=175))
    for p_, dt_, pk_, du_ in pair:
        cmds.append(ev(p_, dt=dt_, peak=pk_, dur=du_))
    t = (n + 1) % DNA_N
    for _ in range(5):
        t = (t + 1) % DNA_N
        cmds.append(ev(pol(t), dt=lead, peak=180, dur=175))
    return cmds

_S = pol(_TGT)                # 'S'
_N = 'N' if _S == 'S' else 'S'

add('syn_pair_strong_then_weak',
    'Toby Event A, the Grillers ordering — THE defect 1.16 exists to fix. A '
    'genuine strong read, then a weak companion 303 ms later: below the 350 ms '
    'physical floor, so it is QUARANTINED instead of admitted, and the next '
    'genuine marker convicts it (fits the phantom frame). Count stays correct, '
    'zero disagreements. Under 1.13-1.14 this was final_mm 13 with 2 '
    'disagreements: the +1 count error that seeded every incident.',
    _pair(2500, [(_S, 2532, 170, 302), (_N, 303, 41, 42)]) + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_mm': 12,            # the CORRECT count — was 13
     'final_disagree': 0})      # was 2

# ---------------------------------------------------------------------------
# QUARANTINE PROPOSAL fixtures (docs/QUORUM_QUARANTINE_AND_SELF_RESOLUTION_PROPOSAL.md)
#
# These encode the operator's 2026-08-14 rule -- "if a magnet is low flux,
# occurs improbably soon, is the opposite pole of the magnet preceding, it
# should be discarded; the record should come from the next magnet with good
# credentials" -- and CODEX's addition of abnormal DURATION as a fourth
# credential.
#
# They assert TODAY'S behaviour, which is the defect. When quarantine is
# implemented these expectations change, and the diff between them IS the
# proof of what the change did. Written now so the baseline is captured from
# a build nobody has modified for the purpose.
# ---------------------------------------------------------------------------

_W_OPP = 'N' if pol(9) == 'S' else 'S'   # opposite of the last accepted lead
add('syn_quarantine_weak_companion',
    'The return-flux fingerprint: 662 ms against a 2400 ms norm is above the '
    'floor, so this proves the CONJUNCTION — opposite pole, interval 28% of '
    'trailing median, flux 18% of median. Quarantined, then discarded when '
    'the genuine successor fits the phantom frame; its interval folds into '
    'the successor. Was final_mm one high (the +1 defect) before 1.16.',
    _pair(2400, [(_W_OPP, 662, 43, 55), (pol(10), 1900, 196, 360)], n=9) + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_mm': 15,
     'final_disagree': 0})

_C_OPP = 'N' if pol(9) == 'S' else 'S'
add('syn_quarantine_crawl_long_duration',
    'The 2026-08-14 17:21 crawl artefact — STRONG (peak 239) so flux cannot '
    'catch it; 5212 ms long against a ~175 ms norm while arriving 463 ms '
    'after its predecessor. Duration is the credential that condemns it. '
    'Conjunction quarantines it; the successor convicts it.',
    _pair(3000, [(_C_OPP, 463, 239, 5212), (pol(10), 3008, 143, 361)], n=9) + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_mm': 15,
     'final_disagree': 0})

add('syn_quarantine_must_not_reject_genuine',
    'THE FALSE-POSITIVE GUARD. A genuinely dim marker (peak 60 vs median 190, '
    'well under the flux threshold) arriving at a NORMAL interval with NORMAL '
    'duration. Dimness alone is never a crime: accepted, not quarantined, '
    'exactly as before 1.16.',
    _pair(2400, [(_S, 2350, 60, 340), (_N, 2400, 190, 355)]) + ['dump'],
    {'must_not_contain': ['QUARANTINED', 'PHANTOM_REJECTED'],
     'final_mm': 13})

add('syn_pair_weak_then_strong',
    'Toby Event B: the weak map-INCONSISTENT read arrives FIRST at an ordinary '
    '1166 ms interval — no credential test can flag it (on time, and dimness '
    'alone is not a crime). The strong genuine read 115 ms later goes below '
    'the floor, is quarantined, and is discarded because the successor fits '
    'the phantom frame. IDENTITY ERRORS REMAIN OUT OF SCOPE (decision 0024 '
    'predicted this for every order-preserving rule): count correct, one '
    'wrong-polarity reading retained in the ring. This fixture asserts the '
    'limitation so nobody mistakes 1.16 for a fix it is not.',
    _pair(1200, [(_N, 1166, 40, 40), (_S, 115, 216, 189)]) + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_mm': 12,            # count correct
     'final_disagree': 1})      # the wrong-polarity bit: still there


# ---------------------------------------------------------------------------
# 1.16 ADVERSARIAL SET (decision 0024's own bar, finally met): cases built to
# BREAK quarantine, the dt-fold, arbitration, and self-resolution.
# Start positions cite sweep counts; the sweep scripts are committed under
# tests/sweeps/ so every count is re-runnable (CODEX review round).
# ---------------------------------------------------------------------------

def _steady(start, n, dt=2400, peak=180, dur=175, pwm=90):
    """n map-true events from start+1, training the credential medians."""
    cmds = ['dir CW', f'declare {start}', 'auto 1']
    t = start
    for _ in range(n):
        t = (t + 1) % DNA_N
        cmds.append(ev(pol(t), dt=dt, pwm=pwm, peak=peak, dur=dur))
    return cmds, t

_A = 40   # steady stretch used by most adversarials

# (a) hard genuine acceleration: intervals halve and halve again. Every event
# is on time in absolute terms (>=560 ms >> 350) and normal in flux/duration,
# so neither the floor nor the conjunction may fire. The third leg of the
# conjunction (dim OR long) is what protects genuine acceleration.
_c, _t = _steady(_A, 9)
for _dt in (2400, 1700, 1200, 900, 700, 560):
    _t = (_t + 1) % DNA_N
    _c.append(ev(pol(_t), dt=_dt))
add('syn_adv_accel_max',
    'Maximum genuine acceleration: 2400 -> 560 ms across six markers, all '
    'map-true, normal flux and duration. QUARANTINE MUST STAY SILENT — the '
    'conjunction\'s dim-OR-long leg is what protects genuine acceleration. '
    'The legacy conservation gate (PWM model, decision 0024\'s documented '
    'defect, untouched by 1.16) still rejects the fastest step; that loss is '
    'asserted here as the pre-existing cost it is, not hidden.',
    _c + ['dump'],
    {'must_not_contain': ['QUARANTINED', 'NO_QUORUM'],
     'must_contain': ['PHANTOM_REJECTED'],   # the 0024 defect, pinned honestly
     'final_mm': (_A + 14) % DNA_N,          # one genuine marker lost to it
     'final_disagree': 0})

# (b) missed marker, then acceleration. The odometer falls one behind, the
# following honest markers disagree, and the fence adopts +1. Acceleration
# after the miss must not be mistaken for phantom traffic.
_c, _t = _steady(_A, 8)
_t = (_t + 2) % DNA_N                       # one marker passes unseen
_c.append(ev(pol(_t), dt=4800))
for _dt in (1400, 1100, 900, 800, 800, 800, 800, 800):
    _t = (_t + 1) % DNA_N
    _c.append(ev(pol(_t), dt=_dt))
add('syn_adv_missed_then_accel',
    'A missed marker (4800 ms gap) followed by acceleration. The +1 offset '
    'must be adopted by evaluation, with no quarantine and no terminal: '
    'lengthened intervals are never phantom evidence.',
    _c + ['dump'],
    {'must_contain': ['QUORUM_ADOPTED'],
     'must_not_contain': ['QUARANTINED', 'NO_QUORUM'],
     'adopted_offset': 1})

# (c) the dt-fold: phantom discarded, then the genuine remainder of that same
# interval. The successor's conservation test must see the FULL physical
# interval (200+2200), not the half measured from the phantom.
_c, _t = _steady(_A, 9)
_c.append(ev('N' if pol((_t+1)%DNA_N)=='S' else 'S', dt=200, peak=40, dur=45))
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=2200))
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=2400))
add('syn_adv_phantom_then_remainder',
    'A floor-failing phantom then the genuine remainder of the same physical '
    'interval. Asserts the dt-fold: the successor is judged on 200+2200 ms, '
    'passes conservation, and the count stays exact with zero disagreements.',
    _c + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_mm': (_A + 11) % DNA_N,
     'final_disagree': 0})

# (d) two phantoms in a row. The first pending is displaced by the second
# (itself floor-failing); the genuine successor convicts the second. Both
# discarded, count exact.
_c, _t = _steady(_A, 9)
_p1 = 'N' if pol((_t+1)%DNA_N)=='S' else 'S'
_c.append(ev(_p1, dt=250, peak=40, dur=45))
_c.append(ev('N' if _p1=='S' else 'S', dt=180, peak=38, dur=40))
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=2000))
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=2400))
add('syn_adv_double_phantom',
    'Two consecutive floor-failing phantoms, then honest travel. Both must be '
    'discarded — a train of garbage must never commit anything — and the '
    'genuine successors must land on the exact count. CODEX 1.16 review '
    'finding 4: the second phantom is now credential-checked BEFORE it may '
    'testify, so the first discard reads SUCCESSOR_SUSPECT (an unfit witness '
    'triggers the primary verdict), and the genuine successor then convicts '
    'the second by polarity.',
    _c + ['dump'],
    {'must_contain': ['QUARANTINE_DISCARDED'],
     'must_not_contain': ['NO_QUORUM'],
     'discard_reasons': ['SUCCESSOR_SUSPECT', 'SUCCESSOR_FITS_PHANTOM'],
     'final_mm': (_A + 11) % DNA_N,
     'final_disagree': 0})

# (e) THE REINSTATEMENT PROOF (CODEX: discard must not mean erase). A genuine
# marker whose credentials look damning — conjunction fires — but whose
# successor testifies for it: commit, full count, no disagreement. Needs a
# spot where pol(T+1) != pol(T) (opposite-pole credential) and
# pol(T+2) != pol(T+1) (successor must NOT fit the phantom frame).
_T = None
for _cand in range(DNA_N):
    b = (_cand + 9) % DNA_N                 # where the lead ENDS
    if pol((b+1)%DNA_N) != pol(b) and pol((b+2)%DNA_N) != pol((b+1)%DNA_N):
        _T=_cand; break
assert _T is not None
_c, _t = _steady(_T, 9, dt=3000)
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=900, peak=40, dur=60))   # genuine but damning
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=3000))                   # the witness
_t=(_t+1)%DNA_N; _c.append(ev(pol(_t), dt=3000))
add('syn_adv_false_reject_commit',
    'A GENUINE marker that trips the conjunction (dim, 0.3x interval, '
    'opposite pole) — and whose successor fits the genuine frame only. '
    'Arbitration must COMMIT it back: the reversible-discard requirement, '
    'proven. Full count, zero disagreements.',
    _c + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_COMMITTED'],
     'must_not_contain': ['QUARANTINE_DISCARDED', 'NO_QUORUM'],
     'final_disagree': 0})

# (f) reversal while an event is held: the arbitration frame is void, the
# pending event must be discarded, and nothing may commit.
_c, _t = _steady(_A, 9)
_c.append(ev('N' if pol((_t+1)%DNA_N)=='S' else 'S', dt=200, peak=40, dur=45))
_c.append('motor 0')                                    # reverse mid-quarantine
add('syn_adv_reversal_mid_quarantine',
    'Direction change while an event is quarantined. The pending event is '
    'discarded (the "next marker" changed identity) and must never commit.',
    _c + ['dump'],
    {'must_contain': ['QUARANTINE_DISCARDED'],
     'must_not_contain': ['QUARANTINE_COMMITTED', 'NO_QUORUM']})

# (g) SELF-RESOLUTION, END TO END. Forced terminal, then clean travel: twelve
# fresh events age the corrupted ring out, the route-wide unique match fires,
# three confirmations, SELF_RESOLVED back to NORMAL. W=12 windows are unique
# route-wide (the suite proves that map fact separately), so any honest
# continuation resolves.
_c, _t = _steady(_A, 6)
_c.append('noquorum')                                   # FORCED_BY_FIXTURE
for _ in range(16):
    _t=(_t+1)%DNA_N
    _c.append(ev(pol(_t)))
add('syn_adv_self_resolution',
    'The 2026-08-14 Toby scenario in miniature: a terminal, then honest '
    'markers. Twelve age out the ring, the wide match is unique and holds '
    'across three consistent matches in all (the candidate plus two further '
    'advances — wording per CODEX finding 6), SELF_RESOLVED, state NORMAL — '
    'no operator declaration. '
    'syn_adv_self_resolution_interlock asserts the motor side.',
    _c + ['dump'],
    {'must_contain': ['NO_QUORUM', 'SELF_RESOLVED'],
     'final_state': 'NORMAL',
     'final_mm': _t})


# ---------------------------------------------------------------------------
# CODEX 1.16 REVIEW ROUND (2026-08-14). Findings 1-4: coverage that did not
# exist, for mechanisms the review found integration defects around. Every
# start position below was found by sweeping this harness — the sweep
# scripts are committed under tests/sweeps/ with their headline counts, so
# none of this is hand-picked or unreproducible.
# ---------------------------------------------------------------------------

def _straddle(start, k1, k2, ccw=False):
    """The suffix-rescue shape (decision 0035 §2): a missed marker opens an
    evaluation toward +1, then a same-pole insertion mid-window returns the
    true offset to 0. Pre-insertion entries vote +1, post-insertion entries
    vote 0, the margin never reaches 2, and the board arrives at the hard
    bound straddled — the 17:21 [8,8,8,4,7,3] shape, synthesised."""
    step = -1 if ccw else 1
    # No motor command: the harness default is FWD, and sessionDir alone sets
    # the travel sense — 'motor 0' (REV) would flip navDir straight back to
    # CW. The first draft of this fixture made exactly that mistake, and the
    # strict final_dir/final_mm assertions are what caught it.
    cmds = ['dir ' + ('CCW' if ccw else 'CW'), f'declare {start}', 'auto 1']
    t = start
    for _ in range(k1):
        t = (t + step) % DNA_N; cmds.append(ev(pol(t)))
    t = (t + 2*step) % DNA_N
    cmds.append(ev(pol(t), dt=4800))              # one marker missed
    for _ in range(k2):
        t = (t + step) % DNA_N; cmds.append(ev(pol(t)))
    cmds.append(ev(pol(t), dt=1200))              # same-pole insertion
    for _ in range(18):
        t = (t + step) % DNA_N; cmds.append(ev(pol(t)))
    return cmds, t

# (h) THE POSITIVE RESCUE (finding 3: this decision path had zero tests while
# holding position authority). Sweep: 39/855 CW straddles fire the rescue;
# start 36 runs the board to eval 12 with margin never above 1, rescues
# offset 0 on a suffix of 9, and ends NORMAL at the TRUE position.
_c, _t = _straddle(36, 5, 2)
add('syn_adv_rescue_straddle',
    'The suffix rescue, fired for real: a straddled board reaches the hard '
    'bound (margin never 2 across twelve evaluations), the newest-7 suffix '
    'fits exactly one candidate, and the rescue adopts it — ending NORMAL at '
    'the true position where pre-1.16 firmware went terminal. The final '
    'scores [7,9,8,6,6,4] are the synthetic twin of the 17:21 board.',
    _c + ['dump'],
    {'must_contain': ['QUORUM_SUFFIX_RESCUE', 'QUORUM_ADOPTED', 'QUORUM_CLOSED'],
     'must_not_contain': ['NO_QUORUM'],
     'adopted_offset': 0,
     'final_state': 'NORMAL',
     'final_mm': _t})

# (i) rescue in CCW, and across the index wrap (finding 3: MM170/MM000).
# The strict CCW sweep (855 runs, final state/dir/mm all verified) found 53
# hits, NONE of which cross the wrap — so direction and seam are covered by
# two fixtures rather than one, and that limit is stated instead of implied
# away. (An earlier draft claimed a CCW wrap hit; it was an artefact of a
# 'motor 0' command silently flipping navDir back to CW, caught by the
# strict assertions below.)
_c, _t = _straddle(50, 5, 2, ccw=True)
add('syn_adv_rescue_ccw',
    'The rescue in CCW: the straddle descending from mm 50, board to the '
    'hard bound, exactly-one suffix fit, NORMAL at true mm 23 with '
    'direction CCW asserted — navDir sign handling inside suffixLenAt() '
    'is load-bearing here.',
    _c + ['dump'],
    {'must_contain': ['QUORUM_SUFFIX_RESCUE'],
     'must_not_contain': ['NO_QUORUM'],
     'final_state': 'NORMAL',
     'final_dir': 'CCW',
     'final_mm': _t})

_c, _t = _straddle(150, 3, 3)
add('syn_adv_rescue_cw_wrap',
    'The rescue in CW crossing the wrap upward (start 150, ends true mm 5). '
    'Same contract as syn_adv_rescue_straddle, exercised over the modular '
    'seam instead of the middle of the route.',
    _c + ['dump'],
    {'must_contain': ['QUORUM_SUFFIX_RESCUE'],
     'must_not_contain': ['NO_QUORUM'],
     'final_state': 'NORMAL',
     'final_mm': _t})

# (j) the rescue REFUSES an excluded candidate (finding 3). A correct +1
# adoption is broken by an insertion during validation (REOPENED excludes
# +1), a second miss restores the true offset to +1, and at the hard bound
# +1 fits with a PERFECT suffix of 12 — and is still refused, because it
# already failed adoption once this incident. Terminal; self-resolution is
# the designed recovery from there, on fresh evidence with no exclusions.
_c = ['dir CW', 'declare 56', 'auto 1']
_t = 56
for _ in range(5):
    _t = (_t + 1) % DNA_N; _c.append(ev(pol(_t)))
_t = (_t + 2) % DNA_N; _c.append(ev(pol(_t), dt=4800))     # miss -> true +1
for _ in range(4):
    _t = (_t + 1) % DNA_N; _c.append(ev(pol(_t)))          # adopts +1 (correct)
_t = (_t + 1) % DNA_N; _c.append(ev(pol(_t)))              # validating
_c.append(ev(pol(_t), dt=1200))                            # insertion breaks it
for _ in range(2):
    _t = (_t + 1) % DNA_N; _c.append(ev(pol(_t)))
_t = (_t + 2) % DNA_N; _c.append(ev(pol(_t), dt=4800))     # second miss -> +1 again
for _ in range(14):
    _t = (_t + 1) % DNA_N; _c.append(ev(pol(_t)))
add('syn_adv_rescue_refuses_excluded',
    'A candidate that failed adoption is NOT rescued, even on a perfect '
    'suffix. +1 is adopted correctly, an insertion during validation reopens '
    'the incident (excluding +1), a second miss makes +1 true again — and at '
    'the hard bound, with +1 fitting all twelve ring entries, the rescue '
    'refuses it and the terminal proceeds. One failed adoption per candidate '
    'per incident; recovery belongs to self-resolution, which starts from '
    'fresh evidence and no exclusions.',
    _c + ['snapshot', 'dump'],
    {'must_contain': ['QUORUM_ADOPTED', 'QUORUM_REOPENED', 'NO_QUORUM'],
     'must_not_contain': ['QUORUM_SUFFIX_RESCUE'],
     'terminal_reason': 'HARD_BOUND',
     'final_state': 'NO_QUORUM'})

# (k) THE COMMIT SURVIVES THE LEGACY GATE (finding 2). At pwm 60 with a
# 2200 ms cadence, a committed pending event's folded interval lands INSIDE
# the PWM conservation gate's reject band at 4 of 26 polarity-eligible
# starts (spacing-dependent — which is the arbitrariness that condemns the
# model). Pre-fix firmware committed the event and then PHANTOM_REJECTED it
# one line later: mm one short, 2 disagreements. vouched=true bypasses the
# veto: the map authenticated the event; the model does not get to overrule
# the map.
_T2 = 7
_c = ['dir CW', f'declare {_T2}', 'auto 1']
_t = _T2
for _ in range(9):
    _t = (_t + 1) % DNA_N; _c.append(ev(pol(_t), dt=2200, pwm=60))
_t = (_t + 1) % DNA_N
_c.append(ev(pol(_t), dt=800, pwm=60, peak=40, dur=60))    # genuine, damning
_t = (_t + 1) % DNA_N
_c.append(ev(pol(_t), dt=2200, pwm=60))                    # the witness
for _ in range(3):
    _t = (_t + 1) % DNA_N; _c.append(ev(pol(_t), dt=2200, pwm=60))
add('syn_adv_commit_survives_gate',
    'CODEX finding 2, reproduced then fixed: a quarantined genuine event is '
    'committed by its successor and must NOT then be re-rejected by the '
    'legacy PWM conservation gate. On the pre-fix build this exact stream '
    'ends mm 20 with 2 disagreements (committed, then killed on the model '
    '0024 already convicted); fixed, it ends mm 21 with 0.',
    _c + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_COMMITTED'],
     'must_not_contain': ['PHANTOM_REJECTED', 'NO_QUORUM'],
     'final_mm': 21,
     'final_disagree': 0})

# (l) THE SLOW PHANTOM FAMILY (finding 4). Two conjunction-level phantoms —
# too slow for the floor — in a row. The second is credential-checked before
# it may testify (raw interval), found unfit, and the elder is discarded on
# the primary verdict (SUCCESSOR_SUSPECT). The second is then held on the
# FOLDED interval, and the genuine successor convicts it by polarity. Each
# unfit witness discards its elder and is held in turn: the family folds up.
_c = ['dir CW', f'declare {_T2}', 'auto 1']
_t = _T2
for _ in range(9):
    _t = (_t + 1) % DNA_N
    _c.append(ev(pol(_t), dt=2400, peak=180, dur=175))      # arm the medians
_p1 = 'N' if pol(_t) == 'S' else 'S'                        # opposite of ring-last
_c.append(ev(_p1, dt=480, peak=40, dur=50))                 # slow phantom 1: held
_c.append(ev(_p1, dt=450, peak=38, dur=48))                 # slow phantom 2: unfit
_t = (_t + 1) % DNA_N
_c.append(ev(pol(_t), dt=1470, peak=180, dur=175))          # genuine remainder
for _ in range(4):
    _t = (_t + 1) % DNA_N
    _c.append(ev(pol(_t), dt=2400, peak=180, dur=175))
add('syn_adv_slow_phantom_family',
    'Two slow phantoms the floor cannot touch, back to back. The witness '
    'credential check (finding 4) is what breaks the chain: an unfit witness '
    'may not testify, so the elder is discarded SUCCESSOR_SUSPECT and the '
    'witness is held in its turn on the folded interval, until a genuine '
    'event finally testifies. Exact count, zero disagreements.',
    _c + ['dump'],
    {'must_contain': ['QUARANTINED', 'QUARANTINE_DISCARDED'],
     'must_not_contain': ['QUARANTINE_COMMITTED', 'NO_QUORUM'],
     'discard_reasons': ['SUCCESSOR_SUSPECT', 'SUCCESSOR_FITS_PHANTOM'],
     'final_mm': 21,
     'final_disagree': 0})

# (m) THE RESUME INTERLOCK (finding 1). Self-resolution restores knowledge,
# never motion: autoRunning must drop at SELF_RESOLVED so serviceStations()
# cannot re-request cruise on a lost-and-found locomotive. The harness never
# calls serviceStations() — which is exactly why the pre-fix defect was
# invisible here — so the dump now reports the flag itself and this fixture
# pins it: auto goes in 1 and must come out 0.
_c, _t = _steady(40, 6)
_c.append('dump')                        # pre-terminal: auto reads 1 here
_c.append('noquorum FORCED_BY_FIXTURE')
for _ in range(16):
    _t = (_t + 1) % DNA_N
    _c.append(ev(pol(_t)))
add('syn_adv_self_resolution_interlock',
    'AUTO is dropped BY self-resolution, not merely not-resumed. Sixteen '
    'honest events after a forced terminal: SELF_RESOLVED fires once the '
    'unique route-wide match holds for three consistent events, the resume '
    'interlock (finding 1) drops autoRunning, and the final dump must read '
    'auto 0, state NORMAL. Resuming requires a deliberate BEGIN.',
    _c + ['dump'],
    {'must_contain': ['NO_QUORUM', 'SELF_RESOLVED'],
     'final_state': 'NORMAL',
     'final_auto': 0,
     'final_mm': _t})

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
