"""Harness self-tests: deliberately defective test doubles, each of which the
suite MUST reject.

These doubles are not navigators and are not a step toward one. Each is broken
in exactly one way, exists only inside this module, and its purpose is to fail.
If the harness ever passes one of them, the harness is not measuring anything.
"""
from . import ngrmap as M
from . import navapi as A
from . import invariants as I
from .generate import Generator
from .navapi import NavStatus, PeerReport
from .result import Result, PASS, FAIL


class _Base:
    """Minimal double. Tracks nothing real; each subclass breaks one rule."""
    def __init__(self):
        self.mm = None
        self.step = M.CW
        self.h = set()
        self.n = 0
        self.stops = 0

    def start(self, mode, policy, mm=None, direction=None):
        self.step = direction or M.CW
        self.mm = mm
        self.h = {(mm, self.step)} if mm is not None else set()

    def peer_report(self, rep):
        self.peer = rep

    def operator(self, cmd, **kw):
        return True

    def observe(self, d):
        self.n += 1

    def status(self):
        return NavStatus(nav_state=A.POSITIONED,
                         movement_state=A.FULL_AUTHORITY,
                         hypotheses=set(self.h), complete=True)


class FalseConfirmer(_Base):
    """Confirms a position that is not the true one. Must trip S1."""
    def observe(self, d):
        self.n += 1
        if self.mm is not None:
            self.mm = M.nxt(self.mm, self.step)
            self.h = {(self.mm, self.step)}

    def status(self):
        st = super().status()
        if self.n == 5:
            st.just_confirmed = True
            st.confirmed_mm = (self.mm + 9) % M.DNA_N     # deliberately wrong
            st.confirmed_dir = self.step
        return st


class UnderApproximator(_Base):
    """Marks a set COMPLETE that excludes the truth. Must trip S2."""
    def observe(self, d):
        self.n += 1
        if self.mm is not None:
            self.mm = M.nxt(self.mm, self.step)
        self.h = {((self.mm + 40) % M.DNA_N, self.step)}   # truth excluded

    def status(self):
        return NavStatus(nav_state=A.RECOVERING,
                         movement_state=A.RECOVERING_WITH_AUTHORITY,
                         hypotheses=set(self.h), complete=True)


class UnsafePeerAuthority(_Base):
    """Grants motion against an unbounded / too-close peer. Must trip S3."""
    def status(self):
        return NavStatus(nav_state=A.ACQUIRING_ORIENTED,
                         movement_state=A.RECOVERING_WITH_AUTHORITY,
                         hypotheses={(mm, M.CW) for mm in range(M.DNA_N)},
                         complete=True, commanded_speed=60.0)


class ReceiptTimeContaminated(_Base):
    """Uses the decoy MQTT receipt time. Must be caught differentially."""
    def observe(self, d):
        self.n += 1
        self.mm = (d.decoy_mqtt_recv_ts or 0) % M.DNA_N
        self.h = {(self.mm, self.step)}


class FirmwareLabelContaminated(_Base):
    """Uses the decoy firmware label. Must be caught differentially."""
    def observe(self, d):
        self.n += 1
        self.mm = (d.decoy_firmware_mm or 0) % M.DNA_N
        self.h = {(self.mm, self.step)}


LAUNCH_HOLD = 'this module deliberately defines a prohibited symbol'


class ProhibitedLaunchHold(_Base):
    """Defines LAUNCH_HOLD at module level. Must be caught structurally."""


class StopOnly(_Base):
    """Never acquires; always stops. Safe, useless. Must fail CLEAN families."""
    def status(self):
        return NavStatus(nav_state=A.UNLOCATED,
                         movement_state=A.STOPPED_FOR_NAVIGATION_SAFETY,
                         hypotheses=set(), complete=True, commanded_speed=0.0)


class OverCompressor(_Base):
    """Publishes a minimal-looking arc that drops candidates. Must trip T15."""
    def start(self, mode, policy, mm=None, direction=None):
        super().start(mode, policy, mm, direction)
        self.h = {(10, M.CW), (11, M.CW), (80, M.CW), (81, M.CW)}

    def status(self):
        return NavStatus(nav_state=A.RECOVERING,
                         movement_state=A.RECOVERING_WITH_AUTHORITY,
                         hypotheses=set(self.h), complete=True,
                         occupancy_arcs=[(10, 11)])       # 80/81 dropped


class DemandsDeclaration(_Base):
    """Demands a manual MM declaration. Spec 7.6 forbids it."""
    def status(self):
        st = super().status()
        st.manual_declaration_required = True
        return st


# ---------------------------------------------------------------------------
def _run(double_cls, stream, policy):
    nav = double_cls()
    nav.start(A.MODE_EXACT, policy, mm=stream.start_mm, direction=stream.start_dir)
    mon = I.Monitor(stream, policy)
    try:
        mon.run(nav)
    except I.SuiteFailure as e:
        return 'SUITE_FAILURE', str(e), mon
    return 'COMPLETED', '', mon


def run_selftests(policy):
    """Each entry: the harness must CATCH the named defect."""
    out = []
    stream = Generator(31337).clean_run(40, M.CW, 14)
    stream.start_mode, stream.start_mm_declared = A.MODE_EXACT, 40

    # 1. false confirmation -> SuiteFailure
    kind, msg, mon = _run(FalseConfirmer, stream, policy)
    out.append(Result(
        'H1', 'harness catches a deliberately false confirmation',
        PASS if kind == 'SUITE_FAILURE' else FAIL,
        msg or 'no SuiteFailure raised', gate='self-test'))

    # 2. under-approximated COMPLETE set -> S2 failure
    kind, msg, mon = _run(UnderApproximator, stream, policy)
    caught = any('S2' in f for f in mon.failures)
    out.append(Result('H2', 'harness catches an under-approximated COMPLETE set',
                      PASS if caught else FAIL,
                      mon.failures[0] if mon.failures else 'not caught',
                      gate='self-test'))

    # 3. unsafe peer authority -> S3 failure
    s3 = Generator(31338).clean_run(40, M.CW, 8)
    s3.peer_reports = [PeerReport(t_report=0, peer_id='p',
                                  bounded_region=tuple(range(42, 46)))]
    kind, msg, mon = _run(UnsafePeerAuthority, s3, policy)
    caught = any('S3' in f for f in mon.failures)
    out.append(Result('H3', 'harness catches unsafe peer authority',
                      PASS if caught else FAIL,
                      mon.failures[0] if mon.failures else 'not caught',
                      gate='self-test'))

    # 3b. an unbounded peer must not be treated as a bound
    unbounded = I.peer_occupancy_now(
        PeerReport(t_report=0, peer_id='p', commanded_stopped=True,
                   decoy_claimed_mm=100), t_now=0)
    out.append(Result('H3b', 'a stopped peer with no region yields no bound',
                      PASS if unbounded is None else FAIL,
                      'peer_occupancy_now returned %r' % (unbounded,),
                      gate='self-test'))

    # 3c. staleness enlarges, immobilised does not
    fresh = I.peer_occupancy_now(PeerReport(0, 'p', bounded_region=(100, 101)), 0)
    stale = I.peer_occupancy_now(PeerReport(0, 'p', bounded_region=(100, 101)), 30000)
    latch = I.peer_occupancy_now(
        PeerReport(0, 'p', bounded_region=(100, 101), immobilised=True), 600000)
    ok = (len(stale) > len(fresh)) and (len(latch) == len(fresh))
    out.append(Result('H3c', 'stale peer info enlarges; immobilised does not',
                      PASS if ok else FAIL,
                      'fresh=%d stale=%d immobilised=%d'
                      % (len(fresh), len(stale), len(latch)), gate='self-test'))

    # 4. receipt-time and firmware-label contamination -> differential probe
    for cls, label, key in ((ReceiptTimeContaminated, 'receipt-time', 'decoy_mqtt_recv_ts'),
                            (FirmwareLabelContaminated, 'firmware-label', 'decoy_firmware_mm')):
        a = Generator(31339).clean_run(40, M.CW, 10)
        b = Generator(31339).clean_run(40, M.CW, 10)
        for ev in b.events:                       # same stream, different decoys
            setattr(ev.detection, key,
                    ((getattr(ev.detection, key) or 0) + 77) % 100000)
        ha, hb = [], []
        for stream_x, sink in ((a, ha), (b, hb)):
            nav = cls()
            nav.start(A.MODE_EXACT, policy, mm=40, direction=M.CW)
            for ev in stream_x.events:
                nav.observe(ev.detection)
                sink.append(frozenset(nav.status().hypotheses))
        out.append(Result(
            'H4.%s' % label, 'harness catches %s contamination' % label,
            PASS if ha != hb else FAIL,
            'identical behaviour under differing decoys means no contamination '
            'was detectable' if ha == hb else 'behaviour diverged with the '
            'decoy: the double consumed prohibited evidence',
            gate='self-test'))

    # 5. prohibited LAUNCH_HOLD -> structural scan
    import inspect
    src = inspect.getsource(inspect.getmodule(ProhibitedLaunchHold))
    found = any(sym in src for sym in A.PROHIBITED_SYMBOLS)
    out.append(Result('H5', 'harness catches a prohibited LAUNCH_HOLD symbol',
                      PASS if found else FAIL,
                      'structural scan %s the symbol'
                      % ('found' if found else 'MISSED'), gate='self-test'))

    # 6. stop-only navigator presented as success -> CLEAN family must reject
    nav = StopOnly()
    nav.start(A.MODE_LAUNCH_REGION, policy, direction=M.CW)
    clean = Generator(31340).clean_run(40, M.CW, 16)
    clean.clean = True
    mon = I.Monitor(clean, policy)
    mon.run(nav)
    cls_ = I.stop_classification(mon, clean)
    acquired = nav.status().nav_state == A.POSITIONED
    rejected = (not acquired) and cls_ == 'MODEL_DEFECT_STOP'
    out.append(Result('H6', 'harness rejects a stop-only navigator',
                      PASS if rejected else FAIL,
                      'stop classification %s; acquisition %s' % (cls_, acquired),
                      gate='self-test'))

    # 7. over-compressed occupancy -> publication check
    kind, msg, mon = _run(OverCompressor, Generator(31341).clean_run(40, M.CW, 4),
                          policy)
    hit = [f for f in mon.failures if 'omits' in f]
    out.append(Result('H7', 'harness catches occupancy compression that drops '
                      'candidates', PASS if hit else FAIL,
                      hit[0] if hit else 'not caught',
                      gate='self-test'))

    # 8. demanding a manual declaration
    kind, msg, mon = _run(DemandsDeclaration, Generator(31342).clean_run(40, M.CW, 4),
                          policy)
    caught = any('demanded a manual MM declaration' in f for f in mon.failures)
    out.append(Result('H8', 'harness catches a demand for a manual declaration',
                      PASS if caught else FAIL,
                      mon.failures[0] if mon.failures else 'not caught',
                      gate='self-test'))
    return out
