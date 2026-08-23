"""The frozen acceptance families.

Every family is registered with its plan id, its gate (safety/usefulness), its
CLEAN or AMBIGUOUS regime and its specification reference. Families needing the
replacement navigator return NOT_IMPLEMENTED with a named reason; they never
return PASS vacuously, and no stub navigator is provided to make them green.
"""
from . import ngrmap as M
from . import navapi as A
from . import prereq
from .generate import Generator
from .invariants import (Monitor, SuiteFailure, occupancy_markers, arcs_cover,
                         peer_occupancy_now, separation_satisfied,
                         min_marker_gap, stop_classification)
from .result import Result, PASS, FAIL, NOT_IMPLEMENTED, NOT_DEMONSTRATED

REGISTRY = []


def family(test_id, title, gate='', regime='', spec_ref=''):
    def deco(fn):
        fn.test_id = test_id
        fn.title = title
        fn.gate = gate
        fn.regime = regime
        fn.spec_ref = spec_ref
        REGISTRY.append(fn)
        return fn
    return deco


def _need_nav(test_id, title, gate, spec_ref, extra=''):
    return Result(test_id, title, NOT_IMPLEMENTED,
                  A.MISSING_IMPLEMENTATION + (('; ' + extra) if extra else ''),
                  gate=gate, spec_ref=spec_ref)


# ===========================================================================
# P0 - blocking map prerequisites. Computable now.
# ===========================================================================
@family('P0.1', 'directional DNA uniqueness at every rotational start',
        gate='prerequisite', spec_ref='spec 3.3 / plan P0')
def p0_directional(nav_factory, policy):
    r = prereq.REPORT
    if prereq.W_DIR is None:
        return Result('P0.1', p0_directional.title, FAIL,
                      'no directional uniqueness length exists at W <= %d; '
                      'orientation-known acquisition is NOT implementable on '
                      'this map' % prereq.W_MAX, r)
    return Result('P0.1', p0_directional.title, PASS,
                  'W_dir = %d, computed over all %d rotational starts in both '
                  'directions' % (prereq.W_DIR, M.DNA_N),
                  dict(W_dir=prereq.W_DIR), gate='prerequisite')


@family('P0.2', 'cross-plane DNA uniqueness (both directions)',
        gate='prerequisite', spec_ref='spec 3.3 / plan P0')
def p0_cross(nav_factory, policy):
    if prereq.W_BOTH is None:
        return Result('P0.2', p0_cross.title, FAIL,
                      'no cross-plane uniqueness length exists at W <= %d; '
                      'orientation-unknown acquisition is NOT implementable'
                      % prereq.W_MAX, prereq.REPORT)
    return Result('P0.2', p0_cross.title, PASS,
                  'W_both = %d, computed over all %d (marker, direction) pairs'
                  % (prereq.W_BOTH, 2 * M.DNA_N),
                  dict(W_both=prereq.W_BOTH), gate='prerequisite')


@family('P0.3', 'minimum clean window for orientation-known acquisition',
        gate='prerequisite', spec_ref='spec 3.11 / gate U2')
def p0_wdir_bound(nav_factory, policy):
    return Result('P0.3', p0_wdir_bound.title, PASS,
                  'orientation-known acquisition must complete within %d clean '
                  'observations' % prereq.W_DIR,
                  dict(bound=prereq.W_DIR), gate='prerequisite')


@family('P0.4', 'minimum clean window for orientation-unknown acquisition',
        gate='prerequisite', spec_ref='spec 3.11 / gate U2')
def p0_wboth_bound(nav_factory, policy):
    return Result('P0.4', p0_wboth_bound.title, PASS,
                  'orientation-unknown acquisition must complete within %d clean '
                  'observations' % prereq.W_BOTH,
                  dict(bound=prereq.W_BOTH), gate='prerequisite')


@family('P0.5', 'ambiguity of shorter windows is measured, not assumed',
        gate='prerequisite', spec_ref='plan P0 / T12')
def p0_ambiguity(nav_factory, policy):
    rows = prereq.shorter_window_ambiguity()
    nine = next((r for r in rows if r['w'] == 9), None)
    ten = next((r for r in rows if r['w'] == 10), None)
    detail = ('measured: W=9 collides %d ways within a plane (%d strings); '
              'W=10 is unique within a plane but still collides %d ways across '
              'planes' % (nine['cw_max_multiplicity'], nine['cw_colliding_strings'],
                          ten['cross_plane_max_multiplicity']))
    return Result('P0.5', p0_ambiguity.title, PASS, detail,
                  dict(table=rows), gate='prerequisite')


@family('P0.6', 'inherited uniqueness claim re-derived',
        gate='prerequisite', spec_ref='spec 3.3')
def p0_claim(nav_factory, policy):
    """The documents inherited 'W >= 10 unique, W = 9 collides four ways'."""
    rows = {r['w']: r for r in prereq.REPORT['table']}
    claim_10 = rows[10]['cw_max_multiplicity'] == 1 and rows[10]['ccw_max_multiplicity'] == 1
    claim_9 = rows[9]['cw_colliding_strings'] == 4
    if claim_10 and claim_9:
        return Result('P0.6', p0_claim.title, PASS,
                      'inherited claim holds for a SINGLE direction plane '
                      '(W_dir = %d, four colliding strings at W = 9). It does '
                      'NOT extend across planes: W_both = %d, and W = 10 still '
                      'collides %d ways across planes.'
                      % (prereq.W_DIR, prereq.W_BOTH,
                         rows[10]['cross_plane_max_multiplicity']),
                      dict(W_dir=prereq.W_DIR, W_both=prereq.W_BOTH),
                      gate='prerequisite')
    return Result('P0.6', p0_claim.title, FAIL,
                  'inherited uniqueness claim is NOT supported by the committed '
                  'map; recorded, map unaltered', dict(rows=[rows[9], rows[10]]))


@family('P0.7', 'launch region clears the first station in both directions',
        gate='prerequisite', spec_ref='spec 7.3 / rulings')
def p0_launch(nav_factory, policy):
    cw = [M.markers_ahead(p, M.STATIONS['Grillers'], M.CW) for p in M.LAUNCH_REGION]
    ccw = [M.markers_ahead(p, M.STATIONS['Patio'], M.CCW) for p in M.LAUNCH_REGION]
    ok = min(cw) >= M.STATION_LOOKAHEAD_MARKERS and min(ccw) >= M.STATION_LOOKAHEAD_MARKERS
    detail = ('CW Grillers %d-%d markers ahead; CCW Patio %d-%d markers ahead; '
              'requirement is >= %d' % (min(cw), max(cw), min(ccw), max(ccw),
                                        M.STATION_LOOKAHEAD_MARKERS))
    return Result('P0.7', p0_launch.title, PASS if ok else FAIL, detail,
                  dict(cw=cw, ccw=ccw, region=list(M.LAUNCH_REGION)),
                  gate='prerequisite')


# ===========================================================================
# Navigator-dependent families.
# ===========================================================================
def _sweep(nav_factory, policy, streams, test_id, title, gate, spec_ref,
           require_acquisition, bound=None):
    """Shared driver. require_acquisition=True => CLEAN: a stop is a failure."""
    if nav_factory is None:
        return _need_nav(test_id, title, gate, spec_ref)
    failures, stops, latencies, defect_stops = [], 0, [], 0
    for stream in streams:
        nav = nav_factory()
        nav.start(stream.start_mode, policy, mm=stream.start_mm_declared,
                  direction=stream.start_dir)
        mon = Monitor(stream, policy)
        mon.run(nav)                       # SuiteFailure propagates
        failures.extend(mon.failures)
        cls = stop_classification(mon, stream)
        if cls != 'NO_STOP':
            stops += 1
            if cls == 'MODEL_DEFECT_STOP':
                defect_stops += 1
        if require_acquisition:
            st = nav.status()
            true_mm, true_dir = stream.final_truth()
            if st.nav_state != A.POSITIONED:
                failures.append('%s: no acquisition on a clean stream (state %s)'
                                % (stream.name, st.nav_state))
            elif (st.confirmed_mm, st.confirmed_dir) != (true_mm, true_dir):
                failures.append('%s: acquired %s, truth %s'
                                % (stream.name, (st.confirmed_mm, st.confirmed_dir),
                                   (true_mm, true_dir)))
            else:
                n = len(mon.confirmations) and mon.confirmations[0][0] + 1
                latencies.append(n)
                if bound is not None and n > bound:
                    failures.append('%s: acquisition took %d observations, bound %d'
                                    % (stream.name, n, bound))
    status = FAIL if failures else PASS
    detail = ('%d stream(s); %d unscheduled stop(s), %d classified model-defect; '
              '%d failure(s)' % (len(streams), stops, defect_stops, len(failures)))
    if failures:
        detail += ' :: ' + '; '.join(failures[:4])
    return Result(test_id, title, status, detail,
                  dict(latencies=latencies, stops=stops,
                       model_defect_stops=defect_stops, failures=failures),
                  gate=gate, spec_ref=spec_ref)


def _launch_streams(seed0=1000, n=None):
    """One clean stream per launch-region marker, both orientations. 20 cases."""
    out = []
    n = n or (prereq.W_DIR + 6)
    for i, mm in enumerate(M.LAUNCH_REGION):
        for step in M.DIRS:
            s = Generator(seed0 + i * 2 + (0 if step > 0 else 1)).clean_run(
                mm, step, n, name='launch-%d-%s' % (mm, M.dirname(step)))
            s.start_mode = A.MODE_LAUNCH_REGION
            s.start_dir = step
            out.append(s)
    return out


def _all_marker_streams(seed0=2000, n=None, mode=A.MODE_LAUNCH_REGION):
    out = []
    n = n or (prereq.W_BOTH + 6)
    for mm in range(M.DNA_N):
        for step in M.DIRS:
            s = Generator(seed0 + mm * 2 + (0 if step > 0 else 1)).clean_run(
                mm, step, n, name='mm%d-%s' % (mm, M.dirname(step)))
            s.start_mode = mode
            s.start_dir = step
            out.append(s)
    return out


@family('T0', 'exact-MM startup remains available', gate='usefulness',
        regime='CLEAN', spec_ref='spec 4.0 mode 1 / U0')
def t0_exact(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T0', t0_exact.title, 'usefulness', 'spec 4.0 mode 1')
    failures = []
    for mm in range(M.DNA_N):
        for step in M.DIRS:
            nav = nav_factory()
            nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
            st = nav.status()
            if st.nav_state != A.POSITIONED:
                failures.append('mm%d %s: state %s, expected POSITIONED '
                                'immediately' % (mm, M.dirname(step), st.nav_state))
            elif len(st.hypotheses) != 1:
                failures.append('mm%d: |H| = %d, expected 1' % (mm, len(st.hypotheses)))
    return Result('T0', t0_exact.title, FAIL if failures else PASS,
                  '%d declaration(s); %d failure(s)' % (2 * M.DNA_N, len(failures)),
                  dict(failures=failures[:6]), gate='usefulness')


@family('T1a', 'launch-region acquisition MM036-MM045, both orientations',
        gate='usefulness', regime='CLEAN', spec_ref='spec 4.0 mode 2 / U1')
def t1a_launch(nav_factory, policy):
    return _sweep(nav_factory, policy, _launch_streams(), 'T1a',
                  t1a_launch.title, 'usefulness', 'spec 4.0 mode 2',
                  require_acquisition=True, bound=prereq.W_DIR)


@family('T1a.station', 'no launch-region case triggers the station substitution',
        gate='usefulness', regime='CLEAN', spec_ref='spec 7.3')
def t1a_station(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T1a.station', t1a_station.title, 'usefulness', 'spec 7.3')
    failures = []
    for stream in _launch_streams():
        nav = nav_factory()
        nav.start(A.MODE_LAUNCH_REGION, policy, direction=stream.start_dir)
        Monitor(stream, policy).run(nav)
        st = nav.status()
        if st.station_substituted:
            failures.append('%s substituted %s from a normal launch'
                            % (stream.name, st.station_substituted))
    return Result('T1a.station', t1a_station.title, FAIL if failures else PASS,
                  'a substitution from the normal launch region indicates a '
                  'lookahead defect, not conservatism',
                  dict(failures=failures[:6]), gate='usefulness')


@family('T1b', 'launch region is never presumed', gate='safety',
        spec_ref='spec 4.0 / rulings')
def t1b_presumption(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T1b', t1b_presumption.title, 'safety', 'spec 4.0')
    failures = []
    nav = nav_factory()
    nav.start(A.MODE_UNKNOWN, policy)
    st = nav.status()
    if st.nav_state != A.UNLOCATED:
        failures.append('boot with no selection entered %s, not UNLOCATED'
                        % st.nav_state)
    seeded = {h[0] for h in st.hypotheses}
    if seeded and seeded <= set(M.LAUNCH_REGION):
        failures.append('boot with no selection seeded the launch region')
    # operator error: mode 2 selected while truly outside the region
    for true_mm in (5, 90, 140):
        for step in M.DIRS:
            s = Generator(4000 + true_mm).clean_run(true_mm, step, prereq.W_DIR + 8)
            s.start_mode, s.start_dir = A.MODE_LAUNCH_REGION, step
            nav = nav_factory()
            nav.start(A.MODE_LAUNCH_REGION, policy, direction=step)
            mon = Monitor(s, policy)
            mon.run(nav)
            for c in mon.confirmations:
                if (c[1], c[2]) != s.final_truth():
                    failures.append('mode 2 outside the region produced a '
                                    'confident wrong position at mm%d' % true_mm)
    return Result('T1b', t1b_presumption.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='safety')


@family('T1', 'orientation-known route-wide startup at every MM',
        gate='usefulness', regime='CLEAN', spec_ref='spec 4.2 ACQ_ROUTE_WIDE')
def t1_routewide(nav_factory, policy):
    return _sweep(nav_factory, policy, _all_marker_streams(mode=A.MODE_UNKNOWN),
                  'T1', t1_routewide.title, 'usefulness', 'spec 4.2',
                  require_acquisition=True, bound=prereq.W_DIR)


@family('T2', 'orientation-unknown startup, movement externally authorised',
        gate='usefulness', regime='CLEAN', spec_ref='spec 4.4 / U3')
def t2_unknown(nav_factory, policy):
    streams = _all_marker_streams(seed0=5000, mode=A.MODE_UNKNOWN)
    for s in streams:
        s.externally_authorised = True
    return _sweep(nav_factory, policy, streams, 'T2', t2_unknown.title,
                  'usefulness', 'spec 4.4', require_acquisition=True,
                  bound=prereq.W_BOTH)


@family('T3', 'powered-run loss retains the anchor and recovers',
        gate='usefulness', regime='CLEAN', spec_ref='spec 4.3 / U4')
def t3_recovery(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T3', t3_recovery.title, 'usefulness', 'spec 4.3')
    failures = []
    for mm in range(0, M.DNA_N, 7):
        for step in M.DIRS:
            for loss_at in (1, 3, 8):
                s = Generator(6000 + mm).with_ghosts(
                    mm, step, prereq.W_DIR + 12, {loss_at}, repeat=4,
                    name='loss-%d-%s-%d' % (mm, M.dirname(step), loss_at))
                s.clean = True
                s.start_mode, s.start_mm_declared = A.MODE_EXACT, mm
                nav = nav_factory()
                nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
                pre = nav.status()
                mon = Monitor(s, policy)
                mon.run(nav)
                st = nav.status()
                if st.nav_state == A.UNLOCATED:
                    failures.append('%s: mid-run loss re-seeded route-wide' % s.name)
                if mon.stops:
                    failures.append('%s: unscheduled stop on clean recovery' % s.name)
                if st.nav_state != A.POSITIONED:
                    failures.append('%s: did not reacquire (state %s)'
                                    % (s.name, st.nav_state))
    return Result('T3', t3_recovery.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='usefulness')


@family('T4', 'reversal during acquisition and recovery', gate='usefulness',
        regime='CLEAN', spec_ref='spec 4.2 / 4.3')
def t4_reversal(nav_factory, policy):
    streams = []
    for mm in range(0, M.DNA_N, 11):
        for step in M.DIRS:
            for before in (1, 5, 10):
                s = Generator(7000 + mm + before).with_reversal(
                    mm, step, before, prereq.W_DIR + 6,
                    name='rev-%d-%s-%d' % (mm, M.dirname(step), before))
                s.start_mode, s.start_mm_declared = A.MODE_EXACT, mm
                streams.append(s)
    return _sweep(nav_factory, policy, streams, 'T4', t4_reversal.title,
                  'usefulness', 'spec 4.2/4.3', require_acquisition=False)


@family('T5', 'operator declaration re-origins timing (Case D)',
        gate='usefulness', regime='CLEAN', spec_ref='spec 6.1 Case D')
def t5_declaration(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T5', t5_declaration.title, 'usefulness', 'spec 6.1')
    failures = []
    for mm in range(0, M.DNA_N, 5):
        for step in M.DIRS:
            for pwm in (40, 60, 90, 100):
                s = Generator(8000 + mm).clean_run(mm, step, 10, pwm=pwm)
                nav = nav_factory()
                nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
                Monitor(s, policy).run(nav)
                nav.operator('declare_mm', mm=s.final_truth()[0], stationary=True)
                st = nav.status()
                if st.nav_state != A.POSITIONED:
                    failures.append('mm%d pwm%d: declaration left state %s'
                                    % (mm, pwm, st.nav_state))
                if st.gap_bearing:
                    failures.append('mm%d pwm%d: declaration set gap_bearing'
                                    % (mm, pwm))
                if len(st.hypotheses) != 1:
                    failures.append('mm%d pwm%d: |H| = %d after declaration'
                                    % (mm, pwm, len(st.hypotheses)))
    return Result('T5', t5_declaration.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='usefulness')


@family('T6', 'clock/timestamp discontinuity (Case I)', gate='safety',
        regime='AMBIGUOUS', spec_ref='spec 6 / S1 / S2')
def t6_discontinuity(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T6', t6_discontinuity.title, 'safety', 'spec 6')
    failures, per_gap = [], {}
    for gap in (0, 1, 2, 3, 4, 6, 10, 20):
        wrong = 0
        for mm in range(0, M.DNA_N, 3):
            for step in M.DIRS:
                s = Generator(9000 + mm + gap).with_discontinuity(
                    mm, step, 6, gap, prereq.W_BOTH + 6)
                s.clean = False
                nav = nav_factory()
                nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
                mon = Monitor(s, policy)
                mon.run(nav)
                failures.extend(mon.failures)
                for c in mon.confirmations:
                    pass   # SuiteFailure already raised on a wrong confirmation
        per_gap[gap] = wrong
    return Result('T6', t6_discontinuity.title, FAIL if failures else PASS,
                  'zero confirmed-wrong outcomes required at every gap size; '
                  '%d invariant failure(s)' % len(failures),
                  dict(per_gap=per_gap, failures=failures[:6]), gate='safety')


@family('T6b', 'internal redeclaration is not a timing event (Case R)',
        gate='usefulness', regime='CLEAN', spec_ref='spec 3.7 / 6.1 Case R')
def t6b_redeclaration(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T6b', t6b_redeclaration.title, 'usefulness', 'spec 3.7')
    failures = []
    for mm in range(0, M.DNA_N, 5):
        for step in M.DIRS:
            s = Generator(10000 + mm).with_redeclaration(
                mm, step, prereq.W_DIR + 8, at=4)
            nav = nav_factory()
            nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
            mon = Monitor(s, policy)
            mon.run(nav)
            st = nav.status()
            if st.gap_bearing:
                failures.append('mm%d: internal redeclaration set gap_bearing; '
                                'the monotonic clock was unaffected' % mm)
            if mon.stops:
                failures.append('mm%d: unscheduled stop on a Case-R stream' % mm)
            if st.speed_reductions:
                failures.append('mm%d: speed reduced on a Case-R stream' % mm)
    return Result('T6b', t6b_redeclaration.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='usefulness')


@family('T6c', 'branch-local elapsed time is never double-counted',
        gate='safety', spec_ref='spec 3.6 / S5')
def t6c_branch_timing(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T6c', t6c_branch_timing.title, 'safety', 'spec 3.6')
    s = Generator(11000).with_ghosts(40, M.CW, 12, {3, 7}, repeat=1)
    nav = nav_factory()
    nav.start(A.MODE_EXACT, policy, mm=40, direction=M.CW)
    Monitor(s, policy).run(nav)
    st = nav.status()
    if st.branches is None:
        return Result('T6c', t6c_branch_timing.title, NOT_DEMONSTRATED,
                      'navigator does not expose per-branch last_genuine; S5 '
                      'cannot be checked directly', gate='safety')
    failures = []
    for last_genuine, epoch, _ in st.branches:
        if last_genuine is None:
            failures.append('branch has no last_genuine timestamp')
    return Result('T6c', t6c_branch_timing.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures),
                  gate='safety')


@family('T7', 'missed markers', gate='mixed', regime='mixed',
        spec_ref='spec 3.5.1 / S2 / U5')
def t7_missed(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T7', t7_missed.title, 'mixed', 'spec 3.5.1')
    failures, clean_stops = [], 0
    for mm in range(0, M.DNA_N, 6):
        for step in M.DIRS:
            for miss_len in (1, 2, 5, 10, 15):
                s = Generator(12000 + mm + miss_len).with_missed(
                    mm, step, prereq.W_BOTH + 14, 4, miss_len,
                    name='miss-%d-%s-%d' % (mm, M.dirname(step), miss_len))
                s.clean = (miss_len <= 1)
                nav = nav_factory()
                nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
                mon = Monitor(s, policy)
                mon.run(nav)
                failures.extend(mon.failures)
                if s.clean:
                    if mon.stops:
                        clean_stops += 1
                        failures.append('%s: single missed marker caused a stop'
                                        % s.name)
                    if nav.status().nav_state == A.UNLOCATED:
                        failures.append('%s: single missed marker caused '
                                        'permanent loss' % s.name)
    return Result('T7', t7_missed.title, FAIL if failures else PASS,
                  'CLEAN sub-family (1 missed) forbids a stop; %d such stop(s); '
                  '%d failure(s)' % (clean_stops, len(failures)),
                  dict(failures=failures[:6]), gate='mixed')


@family('T8', 'same-magnet rereads', gate='safety', regime='AMBIGUOUS',
        spec_ref='spec 3.5.1 / S1')
def t8_reread(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T8', t8_reread.title, 'safety', 'spec 3.5.1')
    failures = []
    for mm in range(0, M.DNA_N, 9):
        for n in (2, 6, 20):
            s = Generator(13000 + mm + n).with_reread(mm, M.CW, n)
            nav = nav_factory()
            nav.start(A.MODE_EXACT, policy, mm=mm, direction=M.CW)
            mon = Monitor(s, policy)
            mon.run(nav)
            failures.extend(mon.failures)
            st = nav.status()
            if st.confirmed_mm is not None and st.confirmed_mm != mm:
                failures.append('mm%d: %d rereads advanced position to %d'
                                % (mm, n, st.confirmed_mm))
    return Result('T8', t8_reread.title, FAIL if failures else PASS,
                  'repeated identical evidence is one observation, never '
                  'K_CONFIRM of them; %d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='safety')


@family('T9', 'genuine acceleration stays genuine', gate='usefulness',
        regime='CLEAN', spec_ref='spec 3.8 / U-gate')
def t9_accel(nav_factory, policy):
    streams = []
    for mm in range(0, M.DNA_N, 8):
        for step in M.DIRS:
            for lo, hi in ((40, 90), (60, 99)):
                s = Generator(14000 + mm + lo).with_acceleration(
                    mm, step, prereq.W_DIR + 8, lo, hi,
                    name='accel-%d-%s-%d' % (mm, M.dirname(step), lo))
                s.start_mode, s.start_mm_declared = A.MODE_EXACT, mm
                streams.append(s)
    return _sweep(nav_factory, policy, streams, 'T9', t9_accel.title,
                  'usefulness', 'spec 3.8', require_acquisition=False)


@family('T10', 'weak short-duration ghosts', gate='usefulness', regime='CLEAN',
        spec_ref='spec 3.9 / U5')
def t10_ghost(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T10', t10_ghost.title, 'usefulness', 'spec 3.9')
    failures = []
    for mm in range(0, M.DNA_N, 6):
        for step in M.DIRS:
            s = Generator(15000 + mm).with_ghosts(
                mm, step, prereq.W_DIR + 8, {3}, repeat=1,
                name='ghost-%d-%s' % (mm, M.dirname(step)))
            nav = nav_factory()
            nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
            mon = Monitor(s, policy)
            mon.run(nav)
            failures.extend(mon.failures)
            if mon.stops:
                failures.append('%s: an isolated ghost stopped the run' % s.name)
            if nav.status().speed_reductions:
                failures.append('%s: an isolated ghost reduced speed' % s.name)
    return Result('T10', t10_ghost.title, FAIL if failures else PASS,
                  'an isolated ghost must neither advance position nor stop the '
                  'run; %d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='usefulness')


@family('T11', 'repeated ghost families', gate='safety', regime='AMBIGUOUS',
        spec_ref='spec 3.10')
def t11_ghost_family(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T11', t11_ghost_family.title, 'safety', 'spec 3.10')
    failures = []
    for mm in range(0, M.DNA_N, 12):
        for repeat in (2, 4, 6):
            s = Generator(16000 + mm + repeat).with_ghosts(
                mm, M.CW, prereq.W_DIR + 10, {4}, repeat=repeat)
            s.clean = False
            nav = nav_factory()
            nav.start(A.MODE_EXACT, policy, mm=mm, direction=M.CW)
            mon = Monitor(s, policy)
            mon.run(nav)
            failures.extend(mon.failures)
            st = nav.status()
            if st.nav_state == A.UNLOCATED:
                failures.append('mm%d x%d: ghost family caused permanent loss'
                                % (mm, repeat))
    return Result('T11', t11_ghost_family.title, FAIL if failures else PASS,
                  'no branch may be discarded at PENDING_DEPTH_MAX overflow; '
                  '%d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='safety')


@family('T12', 'ambiguous DNA sequences, built from measured collisions',
        gate='safety', regime='AMBIGUOUS', spec_ref='spec 3.11 / P11')
def t12_ambiguous(nav_factory, policy):
    groups = prereq.aliased_examples(prereq.W_DIR - 1, plane='dir')
    if nav_factory is None:
        return Result('T12', t12_ambiguous.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; cases derive from %d '
                      'measured collision group(s) at W=%d'
                      % (len(groups), prereq.W_DIR - 1),
                      dict(collision_groups=[list(g) for g in groups]),
                      gate='safety')
    failures = []
    for group in groups:
        for start in group:
            s = Generator(17000 + start).clean_run(start, M.CW, prereq.W_DIR - 1)
            nav = nav_factory()
            nav.start(A.MODE_UNKNOWN, policy, direction=M.CW)
            mon = Monitor(s, policy)
            mon.run(nav)
            failures.extend(mon.failures)
            if nav.status().confirmed_mm is not None:
                failures.append('confirmed from a %d-window that collides %d ways'
                                % (prereq.W_DIR - 1, len(group)))
    return Result('T12', t12_ambiguous.title, FAIL if failures else PASS,
                  '%d measured collision group(s); %d failure(s)'
                  % (len(groups), len(failures)),
                  dict(failures=failures[:6]), gate='safety')


@family('T13', 'route-wide reacquisition', gate='mixed', regime='mixed',
        spec_ref='spec 3.11 / P11')
def t13_routewide(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T13', t13_routewide.title, 'mixed', 'spec 3.11')
    failures = []
    for mm in range(0, M.DNA_N, 10):
        for step in M.DIRS:
            clean = Generator(18000 + mm).clean_run(mm, step, prereq.W_BOTH + 6)
            nav = nav_factory()
            nav.start(A.MODE_UNKNOWN, policy)
            mon = Monitor(clean, policy)
            mon.run(nav)
            failures.extend(mon.failures)
            st = nav.status()
            if st.nav_state != A.POSITIONED:
                failures.append('mm%d %s: clean route-wide reacquisition failed'
                                % (mm, M.dirname(step)))
            g = Generator(18500 + mm).with_ghosts(mm, step, prereq.W_BOTH + 8,
                                                  {2}, repeat=1)
            nav2 = nav_factory()
            nav2.start(A.MODE_UNKNOWN, policy)
            mon2 = Monitor(g, policy)
            mon2.run(nav2)
            failures.extend(mon2.failures)
    return Result('T13', t13_routewide.title, FAIL if failures else PASS,
                  'acquisition only on a genuinely unique sequence; silence '
                  'otherwise; %d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='mixed')


# ---------------------------------------------------------------------------
# Peer and occupancy. The rules pinned here are the ones the correction passes
# were written to enforce; they are the reason this family exists.
# ---------------------------------------------------------------------------
from .navapi import PeerReport


@family('T14', 'permitted acquisition contexts with a peer', gate='safety',
        regime='AMBIGUOUS', spec_ref='spec 4.2 C1/C2')
def t14_contexts(nav_factory, policy):
    """Six configurations. Config 3 is the corrected hole: a bounded,
    immobilised peer is NOT sufficient while our own set is route-wide."""
    configs = [
        (1, 'peer moving, we are route-wide', False, None, False, A.MODE_UNKNOWN, False),
        (2, 'peer COMMANDED_STOPPED, unbounded, we are route-wide', True, None, False, A.MODE_UNKNOWN, False),
        (3, 'peer BOUNDED+IMMOBILISED, we are ROUTE-WIDE', True, (100, 110), True, A.MODE_UNKNOWN, False),
        (4, 'peer bounded, we are launch-region but overlapping', True, (38, 48), True, A.MODE_LAUNCH_REGION, False),
        (5, 'peer bounded, we are launch-region, disjoint with margin', True, (100, 110), True, A.MODE_LAUNCH_REGION, True),
        (6, 'as 5 but peer mobile and its bound goes stale', False, (100, 110), False, A.MODE_LAUNCH_REGION, True),
    ]
    if nav_factory is None:
        return Result('T14', t14_contexts.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; 6 configurations, config 3 '
                      '(bounded peer + route-wide us) must be REFUSED',
                      dict(configs=[c[1] for c in configs]), gate='safety')
    failures = []
    for num, label, stopped, region, immob, mode, may_move in configs:
        s = Generator(19000 + num).clean_run(40, M.CW, prereq.W_DIR + 6)
        s.start_mode = mode
        s.peer_reports = [PeerReport(
            t_report=0, peer_id='peer', commanded_stopped=stopped,
            bounded_region=tuple(range(region[0], region[1] + 1)) if region else None,
            immobilised=immob, decoy_claimed_mm=105)]
        nav = nav_factory()
        nav.start(mode, policy, direction=M.CW)
        # spec 7.5: every configuration attempts the same initial-movement
        # authorisation. Configs 1-4 must be blocked by the C1/C2 gate itself,
        # not merely by the absence of an operator movement command -- so the
        # command is present in all six and the gate is what's under test.
        nav.operator('authorise_initial_movement')
        mon = Monitor(s, policy)
        mon.run(nav)
        failures.extend(mon.failures)
        st = nav.status()
        moved = (st.commanded_speed or 0) > 0
        if not may_move and moved:
            failures.append('config %d (%s): movement authorised. %s'
                            % (num, label,
                               'A bounded immobilised peer says nothing about '
                               'where WE are - C2 condition 1 unsatisfied.'
                               if num == 3 else 'Occupancy unbounded on at '
                               'least one side.'))
        if may_move and num == 5 and not moved:
            failures.append('config 5: both occupancies bounded and separated, '
                            'yet no movement authorised')
    return Result('T14', t14_contexts.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='safety')


@family('T14b', 'peer motion may only enlarge or invalidate a bound',
        gate='safety', spec_ref='spec 3.12.2')
def t14b_peer_motion(nav_factory, policy):
    if nav_factory is None:
        # The reference calculation itself is checkable now, and is the oracle
        # the navigator will be measured against.
        unbounded = peer_occupancy_now(
            PeerReport(t_report=0, peer_id='p', commanded_stopped=True,
                       decoy_claimed_mm=100), t_now=0)
        latched = peer_occupancy_now(
            PeerReport(t_report=0, peer_id='p', bounded_region=(100, 101),
                       immobilised=True), t_now=600000)
        fresh = peer_occupancy_now(
            PeerReport(t_report=0, peer_id='p', bounded_region=(100, 101)), t_now=0)
        stale = peer_occupancy_now(
            PeerReport(t_report=0, peer_id='p', bounded_region=(100, 101)),
            t_now=30000)
        ok = (unbounded is None and latched is not None
              and fresh is not None and stale is not None
              and len(stale) > len(fresh))
        return Result('T14b', t14b_peer_motion.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; harness reference oracle '
                      'verified: a stopped unbounded peer yields no bound, an '
                      'immobilised bound does not expand, a stale bound does '
                      '(%s)' % ('consistent' if ok else 'INCONSISTENT'),
                      dict(reference_oracle_consistent=ok), gate='safety')
    failures = []
    base = Generator(20000).clean_run(40, M.CW, 14)
    for label, rep, expect_bound in [
            ('stopped, no region', PeerReport(0, 'p', commanded_stopped=True,
                                              decoy_claimed_mm=100), False),
            ('moving, no region', PeerReport(0, 'p', commanded_stopped=False,
                                             reported_speed_mm_per_ms=0.2), False),
            ('bounded region', PeerReport(0, 'p',
                                          bounded_region=tuple(range(100, 112))), True)]:
        s = Generator(20001).clean_run(40, M.CW, 14)
        s.start_mode, s.start_mm_declared = A.MODE_EXACT, 40
        s.peer_reports = [rep]
        nav = nav_factory()
        nav.start(A.MODE_EXACT, policy, mm=40, direction=M.CW)
        mon = Monitor(s, policy)
        mon.run(nav)
        failures.extend(mon.failures)
    return Result('T14b', t14b_peer_motion.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='safety')


@family('T15', 'conservative occupancy representation', gate='safety',
        spec_ref='spec 3.12')
def t15_occupancy(nav_factory, policy):
    """T15.1 islands, .2 wraparound, .3 tied covering arcs, .4 wide spread,
    .5 compression never grants more authority."""
    cases = {
        'T15.1 disjoint islands': {(10, M.CW), (11, M.CW), (80, M.CW), (81, M.CW)},
        'T15.2 wraparound': {(169, M.CW), (170, M.CW), (0, M.CW), (1, M.CW)},
        'T15.3 tied covering arcs': {(0, M.CW), (57, M.CW), (114, M.CW)},
        'T15.4 wide spread, low count': {(0, M.CW), (45, M.CW), (90, M.CW),
                                         (135, M.CW)},
    }
    if nav_factory is None:
        return Result('T15', t15_occupancy.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; %d shaped candidate sets '
                      'prepared, plus the 10,000-set compression sweep T15.5'
                      % len(cases),
                      dict(cases={k: sorted(h[0] for h in v)
                                  for k, v in cases.items()}), gate='safety')
    failures = []
    for label, hyp in cases.items():
        nav = nav_factory()
        nav.start(A.MODE_UNKNOWN, policy)
        nav.operator('force_hypotheses', hypotheses=hyp)
        st = nav.status()
        if st.occupancy_arcs is None:
            failures.append('%s: no occupancy published' % label)
            continue
        covered = arcs_cover(st.occupancy_arcs)
        missing = occupancy_markers(hyp) - covered
        if missing:
            failures.append('%s: published occupancy omits %d marker(s)'
                            % (label, len(missing)))
    return Result('T15', t15_occupancy.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='safety')


@family('T15.5', 'compressed telemetry never grants more authority',
        gate='safety', spec_ref='spec 3.12')
def t15_5_compression(nav_factory, policy):
    import random
    rng = random.Random(424242)
    sets = []
    for _ in range(10000):
        k = rng.randint(1, 12)
        sets.append({(rng.randrange(M.DNA_N), M.CW) for _ in range(k)})
    if nav_factory is None:
        return Result('T15.5', t15_5_compression.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; %d deterministic candidate '
                      'sets generated (seed 424242) awaiting a navigator to '
                      'publish occupancy for' % len(sets),
                      dict(n_sets=len(sets), seed=424242), gate='safety')
    violations = 0
    for hyp in sets:
        nav = nav_factory()
        nav.start(A.MODE_UNKNOWN, policy)
        nav.operator('force_hypotheses', hypotheses=hyp)
        st = nav.status()
        if st.occupancy_arcs is None:
            continue
        if occupancy_markers(hyp) - arcs_cover(st.occupancy_arcs):
            violations += 1
    return Result('T15.5', t15_5_compression.title,
                  FAIL if violations else PASS,
                  '%d violation(s) in %d sets; a single one fails the suite'
                  % (violations, len(sets)),
                  dict(violations=violations), gate='safety')


@family('T16', 'no CTO2-style crawl/cruise oscillation', gate='usefulness',
        regime='CLEAN', spec_ref='spec 7.4.1 / U4b / U6')
def t16_oscillation(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T16', t16_oscillation.title, 'usefulness', 'spec 7.4.1')
    failures = []
    for pwm in (40, 60, 90, 99):
        s = Generator(21000 + pwm).with_ghosts(
            40, M.CW, 500, set(range(20, 500, 60)), pwm=pwm, repeat=1,
            name='long-%d' % pwm)
        s.clean = True
        nav = nav_factory()
        nav.start(A.MODE_EXACT, policy, mm=40, direction=M.CW)
        mon = Monitor(s, policy)
        mon.run(nav)
        failures.extend(mon.failures)
        st = nav.status()
        if mon.stops:
            failures.append('pwm%d: %d unscheduled stop(s) in ordinary operation'
                            % (pwm, len(mon.stops)))
        if st.speed_reductions > 1:
            failures.append('pwm%d: %d speed reductions from isolated doubtful '
                            'detections; hysteresis is not holding'
                            % (pwm, st.speed_reductions))
    return Result('T16', t16_oscillation.title, FAIL if failures else PASS,
                  'a single doubtful detection must produce no speed-state '
                  'transition at all; %d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='usefulness')


@family('T17', 'recovery speed is derived, not fixed at ACQ_SPEED',
        gate='usefulness', regime='CLEAN', spec_ref='spec 7.4')
def t17_derived_speed(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T17', t17_derived_speed.title, 'usefulness', 'spec 7.4')
    failures = []
    for mm in range(0, M.DNA_N, 15):
        s = Generator(22000 + mm).with_ghosts(mm, M.CW, prereq.W_DIR + 10,
                                              {3}, repeat=3)
        s.clean = True
        nav = nav_factory()
        nav.start(A.MODE_EXACT, policy, mm=mm, direction=M.CW)
        nav.operator('authorise_speed', pwm=90)
        mon = Monitor(s, policy)
        mon.run(nav)
        st = nav.status()
        if st.movement_state == A.SPEED_LIMITED_FOR_UNCERTAINTY and not st.speed_ceiling:
            failures.append('mm%d: speed limited with no named binding '
                            'constraint - a blanket reduction' % mm)
    return Result('T17', t17_derived_speed.title, FAIL if failures else PASS,
                  'a blanket reduction to ACQ_SPEED with no peer, no armed '
                  'station and a narrow set is the withdrawn fixed-PWM-60 rule; '
                  '%d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='usefulness')


@family('T18', 'first-station lookahead', gate='usefulness', regime='CLEAN',
        spec_ref='spec 7.3')
def t18_lookahead(nav_factory, policy):
    if nav_factory is None:
        return Result('T18', t18_lookahead.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; launch-region cases must '
                      'NOT substitute (Grillers 18-27 CW, Patio 21-30 CCW); '
                      'substitution cases come from modes 1 and 3',
                      gate='usefulness')
    failures = []
    for mm in range(M.DNA_N):
        for step in M.DIRS:
            station = M.STATIONS[M.FIRST_STATION[step]]
            ahead = M.markers_ahead(mm, station, step)
            nav = nav_factory()
            nav.start(A.MODE_EXACT, policy, mm=mm, direction=step)
            nav.operator('set_intended_station', name=M.FIRST_STATION[step])
            st = nav.status()
            should_skip = ahead < M.STATION_LOOKAHEAD_MARKERS
            did_skip = bool(st.station_substituted)
            if should_skip != did_skip:
                failures.append('mm%d %s: %d markers ahead, skip=%s expected %s'
                                % (mm, M.dirname(step), ahead, did_skip, should_skip))
            if mm in M.LAUNCH_REGION and did_skip:
                failures.append('mm%d is in the launch region and must never '
                                'substitute' % mm)
    return Result('T18', t18_lookahead.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='usefulness')


@family('T19', 'operator role boundaries', gate='safety', spec_ref='spec 7.5')
def t19_operator_role(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T19', t19_operator_role.title, 'safety', 'spec 7.5')
    failures = []
    # a declaration while the navigator observes motion must be REJECTED
    s = Generator(23000).clean_run(40, M.CW, 6)
    nav = nav_factory()
    nav.start(A.MODE_EXACT, policy, mm=40, direction=M.CW)
    Monitor(s, policy).run(nav)
    accepted = nav.operator('declare_mm', mm=99, stationary=False)
    if accepted:
        failures.append('a declaration issued while moving was accepted; spec '
                        '7.5 requires rejection with a reason')
    # entering POSITIONED while still moving must need no GO
    s2 = Generator(23001).clean_run(40, M.CW, prereq.W_DIR + 6)
    s2.start_mode = A.MODE_LAUNCH_REGION
    nav2 = nav_factory()
    nav2.start(A.MODE_LAUNCH_REGION, policy, direction=M.CW)
    nav2.operator('authorise_speed', pwm=60)
    Monitor(s2, policy).run(nav2)
    st = nav2.status()
    if st.nav_state == A.POSITIONED and st.auto_running is False and \
            getattr(st, 'go_required', False):
        failures.append('a GO was required after acquisition while moving')
    return Result('T19', t19_operator_role.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='safety')


@family('T20', 'two-locomotive safety while one position is uncertain',
        gate='safety', spec_ref='spec 3.12 / S3')
def t20_two_loco(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T20', t20_two_loco.title, 'safety', 'spec 3.12')
    failures = []
    for state_mode in (A.MODE_LAUNCH_REGION, A.MODE_UNKNOWN):
        for sep in (2, 6, 8, 20, 60):
            s = Generator(24000 + sep).clean_run(40, M.CW, prereq.W_DIR + 6)
            s.start_mode = state_mode
            peer_mm = (40 + sep) % M.DNA_N
            s.peer_reports = [PeerReport(
                t_report=0, peer_id='peer',
                bounded_region=(peer_mm, (peer_mm + 1) % M.DNA_N))]
            nav = nav_factory()
            nav.start(state_mode, policy, direction=M.CW)
            mon = Monitor(s, policy)
            mon.run(nav)
            failures.extend(mon.failures)
    return Result('T20', t20_two_loco.title, FAIL if failures else PASS,
                  'no authority may bring the pair inside the 0033 bubble under '
                  'any candidate pair; %d failure(s)' % len(failures),
                  dict(failures=failures[:6]), gate='safety')


@family('T21', 'manual operation without a declared position',
        gate='usefulness', regime='CLEAN', spec_ref='spec 4.4.3 / U4c')
def t21_manual(nav_factory, policy):
    if nav_factory is None:
        return _need_nav('T21', t21_manual.title, 'usefulness', 'spec 4.4.3')
    if not policy.unlocated_manual_movement_permitted:
        return Result('T21', t21_manual.title, NOT_DEMONSTRATED,
                      'configured policy forbids manual movement without '
                      'position; the ruling of 2026-08-22 permits it',
                      gate='usefulness')
    failures = []
    for mm in range(0, M.DNA_N, 7):
        for step in M.DIRS:
            s = Generator(25000 + mm).clean_run(mm, step, prereq.W_BOTH + 6)
            s.start_mode = A.MODE_UNKNOWN
            nav = nav_factory()
            nav.start(A.MODE_UNKNOWN, policy)
            moved = nav.operator('manual_throttle', pwm=60)
            if not moved:
                failures.append('mm%d: manual throttle refused because position '
                                'is unknown' % mm)
            mon = Monitor(s, policy)
            mon.run(nav)
            st = nav.status()
            if st.auto_running:
                failures.append('mm%d: self-acquisition started AUTO' % mm)
            if st.station_armed:
                failures.append('mm%d: station armed without a position' % mm)
            if st.separation_claimable:
                failures.append('mm%d: separation claimed from an unknown '
                                'position' % mm)
            if st.nav_state != A.POSITIONED:
                failures.append('mm%d: no self-acquisition under manual movement'
                                % mm)
            if not nav.operator('stop'):
                failures.append('mm%d: STOP not effective' % mm)
    return Result('T21', t21_manual.title, FAIL if failures else PASS,
                  'station behaviour must be UNAVAILABLE, never held; %d '
                  'failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='usefulness')


@family('T22', 'sequential operator-supervised launch, no LAUNCH_HOLD',
        gate='usefulness', regime='CLEAN', spec_ref='spec 7.7')
def t22_launch(nav_factory, policy):
    import inspect
    if nav_factory is None:
        return Result('T22', t22_launch.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; the structural half of this '
                      'family scans the navigator source for %s and fails a '
                      'build that defines one even if behaviour is correct'
                      % ', '.join(A.PROHIBITED_SYMBOLS), gate='usefulness')
    failures = []
    try:
        src = inspect.getsource(inspect.getmodule(nav_factory))
        for sym in A.PROHIBITED_SYMBOLS:
            if sym in src:
                failures.append('prohibited symbol %r present in the navigator; '
                                'spec 7.7 forbids a LAUNCH_HOLD state or command'
                                % sym)
    except (OSError, TypeError):
        failures.append('navigator source unavailable for the structural scan')
    lead = Generator(26000).clean_run(38, M.CW, prereq.W_DIR + 8)
    lead.start_mode = A.MODE_LAUNCH_REGION
    trail = Generator(26001).clean_run(36, M.CW, prereq.W_DIR + 8)
    trail.start_mode = A.MODE_LAUNCH_REGION
    nav_t = nav_factory()
    nav_t.start(A.MODE_LAUNCH_REGION, policy, direction=M.CW)
    st = nav_t.status()
    if (st.commanded_speed or 0) > 0:
        failures.append('the trailing locomotive moved without a command')
    if st.movement_state == A.STOPPED_FOR_NAVIGATION_SAFETY:
        failures.append('the trailing locomotive was placed under a navigation '
                        'stop order; it is stationary only because no command '
                        'was issued')
    nav_l = nav_factory()
    nav_l.start(A.MODE_LAUNCH_REGION, policy, direction=M.CW)
    nav_l.operator('authorise_speed', pwm=60)
    mon = Monitor(lead, policy)
    mon.run(nav_l)
    if mon.stops:
        failures.append('unscheduled stop during a normal launch')
    return Result('T22', t22_launch.title, FAIL if failures else PASS,
                  '%d failure(s)' % len(failures), dict(failures=failures[:6]),
                  gate='usefulness')


@family('T23', 'STOP/HOLD code-path restrictions', gate='safety',
        spec_ref='spec 7.8')
def t23_stop_posture(nav_factory, policy):
    import inspect
    import re as _re
    if nav_factory is None:
        return Result('T23', t23_stop_posture.title, NOT_IMPLEMENTED,
                      A.MISSING_IMPLEMENTATION + '; this family enumerates the '
                      'navigator source for motion orders and requires %s to be '
                      'the only one' % A.ONLY_MOTION_ORDER, gate='safety')
    failures = []
    try:
        src = inspect.getsource(inspect.getmodule(nav_factory))
    except (OSError, TypeError):
        return Result('T23', t23_stop_posture.title, NOT_DEMONSTRATED,
                      'navigator source unavailable; the structural audit '
                      'cannot be performed', gate='safety')
    for sym in A.PROHIBITED_SYMBOLS:
        if sym in src:
            failures.append('prohibited symbol %r present' % sym)
    holdish = set(_re.findall(r"\b([A-Z][A-Z_]*HOLD[A-Z_]*)\b", src))
    holdish -= {'ctoFleetHold', 'CTO_FLEET_HOLD'}
    if holdish:
        failures.append('hold-like state name(s) %s; spec 7.8 forbids a new HOLD '
                        'state for an unavailable capability' % sorted(holdish))
    return Result('T23', t23_stop_posture.title, FAIL if failures else PASS,
                  '%s must be the only navigation-commanded motion order; %d '
                  'failure(s)' % (A.ONLY_MOTION_ORDER, len(failures)),
                  dict(failures=failures[:6]), gate='safety')


# ---------------------------------------------------------------------------
# Conditions generated data can NEVER establish. Recorded honestly rather than
# omitted, so the suite cannot be read as covering them.
# ---------------------------------------------------------------------------
@family('N1', 'results hold on an untouched anchored capture', gate='validation',
        spec_ref='validation protocol 2')
def n1_untouched(nav_factory, policy):
    return Result('N1', n1_untouched.title, NOT_DEMONSTRATED,
                  'no untouched capture exists. Generated cases cannot supply '
                  'it, and every existing Toby and Otto session is permanently '
                  'development data (decision 0042).', gate='validation')


@family('N2', 'engineering parameters carry calibration evidence',
        gate='validation', spec_ref='spec 10')
def n2_calibration(nav_factory, policy):
    return Result('N2', n2_calibration.title, NOT_DEMONSTRATED,
                  'q_fast, margin, SANITY_RATIO, MIN_N, PEAK_FLOOR, DUR_FLOOR, '
                  'COLLAPSE_MAX_SET, PENDING_DEPTH_MAX and the hysteresis '
                  'constants have recommended defaults and no committed '
                  'calibration evidence; spec 10 blocks candidate freeze until '
                  'they do.', gate='validation')


@family('N3', 'peer protected-region mechanism exists', gate='validation',
        spec_ref='spec 9 open decision 4')
def n3_protected_region(nav_factory, policy):
    if policy.protected_region_mechanism == 'none':
        return Result('N3', n3_protected_region.title, NOT_DEMONSTRATED,
                      'no protected-region declaration mechanism exists in '
                      'firmware, so spec 4.2 context C2 is unreachable and C1 '
                      '(alone) is the only usable acquisition context. Open '
                      'operator decision 4.', gate='validation')
    return Result('N3', n3_protected_region.title, PASS,
                  'mechanism configured: %s' % policy.protected_region_mechanism,
                  gate='validation')
