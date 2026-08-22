"""P0 - blocking map prerequisites. Computed, never assumed.

Acceptance plan P0: the uniqueness lengths used by confirmation (spec 3.11)
and by usefulness gate U2 are DERIVED here from the committed map, at every
rotational starting position and in both directions. The inherited claim that
"windows of length >= 10 are unique and W = 9 collides four ways" is re-derived,
not trusted.

If a uniqueness length does not exist at any window length <= W_MAX, the map
contains a route-wide alias, the corresponding acquisition mode is NOT
implementable, and that is reported as a blocking failure. The map is never
altered and the test is never weakened to accommodate it.
"""
from . import ngrmap as M

W_MAX = 24


def collision_table(w):
    """Returns (per_plane, cross_plane) collision maps for window length w.

    per_plane[step][string] -> [end markers]      (uniqueness within a plane)
    cross_plane[string]     -> [(dirname, end marker)]  (uniqueness across both)
    """
    per_plane = {M.CW: {}, M.CCW: {}}
    cross = {}
    for step in M.DIRS:
        for p in range(M.DNA_N):
            s = M.window(p, step, w)
            per_plane[step].setdefault(s, []).append(p)
            cross.setdefault(s, []).append((M.dirname(step), p))
    return per_plane, cross


def _max_multiplicity(d):
    return max(len(v) for v in d.values())


def compute():
    """Full sweep. Returns a report dict; no assertions, no assumptions."""
    rows = []
    w_dir = None
    w_both = None
    for w in range(1, W_MAX + 1):
        per_plane, cross = collision_table(w)
        cw_max = _max_multiplicity(per_plane[M.CW])
        ccw_max = _max_multiplicity(per_plane[M.CCW])
        both_max = _max_multiplicity(cross)
        row = dict(
            w=w,
            cw_max_multiplicity=cw_max,
            ccw_max_multiplicity=ccw_max,
            cross_plane_max_multiplicity=both_max,
            cw_colliding_strings=sum(1 for v in per_plane[M.CW].values() if len(v) > 1),
            ccw_colliding_strings=sum(1 for v in per_plane[M.CCW].values() if len(v) > 1),
            cross_plane_colliding_strings=sum(1 for v in cross.values() if len(v) > 1),
        )
        rows.append(row)
        if w_dir is None and cw_max == 1 and ccw_max == 1:
            w_dir = w
        if w_both is None and both_max == 1:
            w_both = w
    return dict(
        map_source='firmware/QUORUM/QUORUM.ino',
        dna_n=M.DNA_N,
        circuit_mm=M.CIRCUIT_MM,
        w_max_searched=W_MAX,
        W_dir=w_dir,
        W_both=w_both,
        table=rows,
    )


REPORT = compute()
W_DIR = REPORT['W_dir']
W_BOTH = REPORT['W_both']


def blocking_failures():
    """Empty list when the map supports both acquisition modes."""
    out = []
    if W_DIR is None:
        out.append('no directional uniqueness length exists at W <= %d: '
                   'orientation-known acquisition is NOT implementable on this map'
                   % W_MAX)
    if W_BOTH is None:
        out.append('no cross-plane uniqueness length exists at W <= %d: '
                   'orientation-unknown acquisition is NOT implementable on this map'
                   % W_MAX)
    return out


def shorter_window_ambiguity():
    """Every window length below the uniqueness bounds, with its ambiguity.

    This is evidence, not decoration: T12 builds its aliased cases from these
    measured collisions rather than from an assumed collision count.
    """
    out = []
    for row in REPORT['table']:
        if W_DIR is not None and row['w'] >= max(W_DIR, W_BOTH or 0):
            break
        out.append(row)
    return out


def aliased_examples(w, plane='dir', limit=8):
    """Concrete colliding positions at window length w, for T12 case building."""
    per_plane, cross = collision_table(w)
    src = per_plane[M.CW] if plane == 'dir' else cross
    groups = [v for v in src.values() if len(v) > 1]
    groups.sort(key=lambda v: -len(v))
    return groups[:limit]
