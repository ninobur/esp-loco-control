"""Conservative occupancy, peer bounds and separation -- specification 3.12.

On-device truth is the bitmap. Everything published is a demonstrable
superset of it, so authority derived from the published form is never greater
than authority derived from the bitmap (the conservatism invariant, 3.12).

Peer motion information enters in exactly one direction (3.12.2): it may
enlarge a bound or invalidate one. It may never create a bound and it may
never independently grant authority. `PEER_COMMANDED_STOPPED` appears in no
decision here at all -- stopping is a claim about velocity and the hazard is
about position.
"""
from . import params as P
from .route import DNA_N, CIRCUIT_MM, MIN_SPACING_MM, occupancy_markers


def covering_arcs(markers, max_arcs=P.OCC_ARCS_MAX):
    """Up to `max_arcs` wrapping arcs whose union covers every marker.

    Obtained by splitting at the largest circular gaps. Disjoint islands and
    the 170/0 wrap are ordinary cases here, not edge cases.
    """
    if not markers:
        return []
    ms = sorted(markers)
    k = min(max_arcs, len(ms))
    gaps = []
    for i, m in enumerate(ms):
        gaps.append((((ms[(i + 1) % len(ms)] - m) % DNA_N), i))
    gaps.sort(reverse=True)
    # Split only at real gaps: cutting a contiguous run would publish the same
    # cover as several arcs for no gain, and the arc count is contract budget.
    cuts = [i for g, i in gaps[:k] if g > 1]
    if not cuts:
        cuts = [gaps[0][1]]
    cuts.sort()
    out = []
    for j, ci in enumerate(cuts):
        start = ms[(ci + 1) % len(ms)]
        end = ms[cuts[(j + 1) % len(cuts)]]
        out.append((start, end))
    return out


def arc_span_mm(markers):
    """Length of the shortest circular arc containing every marker."""
    if not markers:
        return 0
    ms = sorted(markers)
    if len(ms) == 1:
        return 0
    biggest = 0
    for i, m in enumerate(ms):
        biggest = max(biggest, (ms[(i + 1) % len(ms)] - m) % DNA_N)
    return (DNA_N - biggest) * (CIRCUIT_MM / float(DNA_N))


def route_wide(markers):
    """3.12: an over-long covering arc is published as route-wide."""
    if not markers:
        return False
    return arc_span_mm(markers) > P.ROUTE_WIDE_FRACTION * CIRCUIT_MM


def min_marker_gap(occ_a, occ_b):
    if not occ_a or not occ_b:
        return DNA_N
    best = DNA_N
    for a in occ_a:
        for b in occ_b:
            d = min((a - b) % DNA_N, (b - a) % DNA_N)
            if d < best:
                best = d
                if best == 0:
                    return 0
    return best


def separated(occ_a, occ_b):
    """Decision 0033: bubble plus six markers clear, worst-case pair."""
    return min_marker_gap(occ_a, occ_b) > P.CLEAR_GAP_MARKERS


def peer_occupancy(report, t_now):
    """The peer's conservative occupancy now, or None meaning unbounded.

    A peer with no authoritative region yields no bound however precise its
    own navigation claim: neither a peer's claimed marker nor its reported
    speed or direction is read as position, only `bounded_region` from an
    authoritative source. Between valid bounds the occupancy grows at its own
    envelope fast bound; `PEER_IMMOBILISED` holds that growth at zero while
    the latch holds, which is precisely what the latch buys.
    """
    if report is None or getattr(report, 'bounded_region', None) is None:
        return None
    base = set(report.bounded_region)
    if getattr(report, 'immobilised', False):
        return occupancy_markers(base, P.TRAIN_EXTENT_MARKERS)
    age = max(0, t_now - report.t_report)
    grow_mm = P.V_PEER_MAX_MM_PER_MS * age
    # Conservative both ways: the shortest interval on the map buys the most
    # markers for a given distance, and zero age must buy zero markers.
    grow = -(-int(grow_mm) // MIN_SPACING_MM)
    if grow >= DNA_N // 2:
        return None                        # stale beyond usefulness
    out = set()
    for mm in base:
        for k in range(-grow, grow + 1):
            out.add((mm + k) % DNA_N)
    return occupancy_markers(out, P.TRAIN_EXTENT_MARKERS)
