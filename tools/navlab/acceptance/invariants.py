"""Global invariants, asserted continuously. Spec section 8, plan section S.

A single false confirmation fails the entire suite: Monitor raises
SuiteFailure, which the runner does not catch as an ordinary family failure.
"""
from . import ngrmap as M
from . import navapi as A

CLEAR_GAP_MARKERS = 6          # decision 0033 bubble, in markers
MIN_SPACING_MM = min(M.SPACING)  # shortest real interval: the conservative choice
DEFAULT_EXTENT_MARKERS = 2     # conservative train extent for harness maths


class SuiteFailure(Exception):
    """A violation severe enough to fail the whole suite, not one family."""


class Violation(Exception):
    """An ordinary family failure."""


# --------------------------------------------------------------------------
# S3 reference calculation. The harness computes separation independently of
# the navigator; the navigator may never grant more than this permits.
# --------------------------------------------------------------------------
def occupancy_markers(hypotheses, extent=DEFAULT_EXTENT_MARKERS):
    """Every marker any candidate could occupy, expanded by train extent."""
    out = set()
    for item in hypotheses:
        mm = item[0] if isinstance(item, tuple) else item
        for k in range(-extent, extent + 1):
            out.add((mm + k) % M.DNA_N)
    return out


def arcs_cover(arcs):
    """Marker set covered by published arcs, wrapping correctly."""
    out = set()
    for a, b in arcs or []:
        mm = a % M.DNA_N
        for _ in range(M.DNA_N):
            out.add(mm)
            if mm == b % M.DNA_N:
                break
            mm = (mm + 1) % M.DNA_N
    return out


def min_marker_gap(occ_a, occ_b):
    """Smallest marker separation between any pair. 0 => overlap."""
    if not occ_a or not occ_b:
        return M.DNA_N
    best = M.DNA_N
    for a in occ_a:
        for b in occ_b:
            d = min((a - b) % M.DNA_N, (b - a) % M.DNA_N)
            if d < best:
                best = d
                if best == 0:
                    return 0
    return best


def separation_satisfied(occ_a, occ_b):
    return min_marker_gap(occ_a, occ_b) > CLEAR_GAP_MARKERS


def peer_occupancy_now(report, t_now, v_peer_max=0.305):
    """Spec 3.12.2: reports ENLARGE or INVALIDATE. Never create a bound.

    Returns a marker set, or None meaning 'unbounded' -- which is what a peer
    with no authoritative region always yields, however precise its own claim.
    """
    if report is None or report.bounded_region is None:
        return None                       # unbounded: no report can create one
    base = set(report.bounded_region)
    if report.immobilised:
        return occupancy_markers(base)    # latch holds the expansion at zero
    age_ms = max(0, t_now - report.t_report)
    grow_mm = v_peer_max * age_ms
    # Conservative: assume the shortest interval on the map, so a given
    # distance buys the MOST markers. Zero age must buy zero markers, or a
    # fresh bound would be indistinguishable from a stale one.
    grow_markers = -(-int(grow_mm) // MIN_SPACING_MM)      # ceiling division
    if grow_markers >= M.DNA_N // 2:
        return None                       # stale beyond usefulness => unbounded
    out = set()
    for mm in base:
        for k in range(-grow_markers, grow_markers + 1):
            out.add((mm + k) % M.DNA_N)
    return occupancy_markers(out)


# --------------------------------------------------------------------------
class Monitor:
    """Drives one stream against one navigator, asserting every invariant."""

    def __init__(self, stream, policy, extent=DEFAULT_EXTENT_MARKERS):
        self.stream = stream
        self.policy = policy
        self.extent = extent
        self.failures = []
        self.false_confirmations = []
        self.confirmations = []
        self.stops = []
        self.speed_changes = []
        self.statuses = []
        self.peer_state = None

    # -- S1 ------------------------------------------------------------------
    def check_confirmation(self, st, truth):
        if not st.just_confirmed:
            return
        self.confirmations.append((truth.index, st.confirmed_mm, st.confirmed_dir))
        if truth.is_genuine:
            true_mm, true_dir = truth.true_mm, truth.true_dir
        else:
            true_mm, true_dir = truth.true_mm, truth.true_dir
        if st.confirmed_mm != true_mm or st.confirmed_dir != true_dir:
            self.false_confirmations.append(
                dict(event=truth.index, claimed=(st.confirmed_mm, st.confirmed_dir),
                     truth=(true_mm, true_dir)))
            raise SuiteFailure(
                'S1 FALSE CONFIRMATION at event %d: navigator claims %s, truth is %s'
                % (truth.index, (st.confirmed_mm, st.confirmed_dir),
                   (true_mm, true_dir)))

    # -- S2 ------------------------------------------------------------------
    def check_completeness(self, st, truth):
        if not st.complete:
            return
        if not st.hypotheses:
            return
        if (truth.true_mm, truth.true_dir) not in st.hypotheses:
            self.failures.append(
                'S2 under-approximation at event %d: truth %s not in COMPLETE set '
                '(|H|=%d)' % (truth.index, (truth.true_mm, truth.true_dir),
                              len(st.hypotheses)))

    # -- S3 ------------------------------------------------------------------
    def check_authority(self, st, t_now):
        moving = (st.movement_state in (A.FULL_AUTHORITY,
                                        A.RECOVERING_WITH_AUTHORITY,
                                        A.SPEED_LIMITED_FOR_UNCERTAINTY)
                  and (st.commanded_speed or 0) > 0)
        if not moving:
            return
        ours = occupancy_markers(st.hypotheses, self.extent)
        theirs = peer_occupancy_now(self.peer_state, t_now)
        if theirs is None:
            if self.peer_state is not None and self.policy.fleet_stop_on_unbounded_occupancy:
                self.failures.append(
                    'S3 authority granted against an UNBOUNDED peer occupancy '
                    '(movement_state=%s)' % st.movement_state)
            return
        if not st.complete and self.policy.fleet_stop_on_unbounded_occupancy:
            self.failures.append(
                'S3 authority granted while own set is INCOMPLETE, which 3.12 '
                'requires be treated as route-wide')
            return
        if not separation_satisfied(ours, theirs):
            self.failures.append(
                'S3 authority granted with candidate pair separation %d <= %d '
                'markers' % (min_marker_gap(ours, theirs), CLEAR_GAP_MARKERS))

    # -- compression conservatism (T15.5) ------------------------------------
    def check_publication(self, st):
        if st.occupancy_arcs is None:
            return
        true_occ = occupancy_markers(st.hypotheses, self.extent)
        published = arcs_cover(st.occupancy_arcs)
        missing = true_occ - published
        if missing:
            self.failures.append(
                'published occupancy omits %d candidate marker(s) - compression '
                'granted more authority than the bitmap' % len(missing))

    # -- STOP availability / posture ----------------------------------------
    def note_status(self, st):
        if st.movement_state not in A.MOVEMENT_STATES:
            self.failures.append('unknown movement_state %r' % st.movement_state)
        if st.movement_state == A.STOPPED_FOR_NAVIGATION_SAFETY:
            self.stops.append(len(self.statuses))
        if st.manual_declaration_required:
            self.failures.append(
                'navigator demanded a manual MM declaration (spec 7.6 forbids)')
        self.statuses.append(st)

    # -- driver --------------------------------------------------------------
    def run(self, nav):
        for truth in self.stream.events:
            for rep in self.stream.peer_reports:
                if rep.t_report <= truth.detection.t_detect:
                    self.peer_state = rep
                    nav.peer_report(rep)
            nav.observe(truth.detection)
            st = nav.status()
            self.note_status(st)
            self.check_confirmation(st, truth)
            self.check_completeness(st, truth)
            self.check_authority(st, truth.detection.t_detect)
            self.check_publication(st)
        return self


def stop_classification(monitor, stream):
    """Plan 'Stop classification': safe / model-defect / none."""
    if not monitor.stops:
        return 'NO_STOP'
    return 'MODEL_DEFECT_STOP' if stream.clean else 'SAFE_STOP'
