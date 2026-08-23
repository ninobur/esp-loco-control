"""Route substrate: the committed map, geometry, and the 342-bit space.

Read-only. The DNA polarity table, the per-interval spacing table and the
station table are parsed from `firmware/QUORUM/QUORUM.ino`, which is the single
committed source. Nothing here writes, patches or "corrects" the map.

The uniqueness lengths `W_DIR` and `W_BOTH` are **computed here from the
committed map**, per specification 3.3, not asserted. They are derived
independently of the acceptance harness's own prerequisite computation so that
navigator and harness agree by arithmetic rather than by import.
"""
import pathlib
import re

_ROOT = pathlib.Path(__file__).resolve().parents[3]
_INO = _ROOT / 'firmware' / 'QUORUM' / 'QUORUM.ino'

CW, CCW = +1, -1
DIRS = (CW, CCW)


def _table(src, marker):
    body = src.split(marker)[1].split('};')[0]
    return [int(x) for x in re.findall(r'\d+', body)]


def _load():
    src = _INO.read_text()
    dna = _table(src, 'const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')
    spc = _table(src, 'static const uint16_t spacingMm[DNA_N] PROGMEM = {')
    if len(dna) != len(spc):
        raise RuntimeError('map tables disagree in length: %d vs %d'
                           % (len(dna), len(spc)))
    if set(dna) - {0, 1}:
        raise RuntimeError('DNA is not binary polarity')
    stations = {}
    for name, centre in re.findall(r'\{"(\w+)",\s*(\d+),', src):
        stations.setdefault(name, int(centre))
    return dna, spc, stations


DNA, SPACING, STATIONS = _load()
DNA_N = len(DNA)
CIRCUIT_MM = sum(SPACING)
MIN_SPACING_MM = min(SPACING)

#: Operator ruling 2026-08-22: the normal launch region, never presumed.
LAUNCH_REGION = tuple(range(36, 46))
STATION_LOOKAHEAD_MARKERS = 12
FIRST_STATION = {CW: 'Grillers', CCW: 'Patio'}


def nxt(mm, step):
    return (mm + step) % DNA_N


def step_mm(mm, step):
    """Distance travelled leaving marker `mm` in direction `step`."""
    return SPACING[mm] if step > 0 else SPACING[(mm - 1) % DNA_N]


def markers_ahead(frm, to, step):
    return ((to - frm) * step) % DNA_N


def window(end_mm, step, w):
    """Polarity string of the last `w` markers ending at `end_mm`."""
    return tuple(DNA[(end_mm - i * step) % DNA_N] for i in range(w - 1, -1, -1))


# --- 3.3 verified uniqueness, computed over the committed map ---------------
_W_MAX = 24


def _uniqueness():
    w_dir = w_both = None
    for w in range(1, _W_MAX + 1):
        per = {CW: {}, CCW: {}}
        both = {}
        for step in DIRS:
            for p in range(DNA_N):
                s = window(p, step, w)
                per[step].setdefault(s, []).append(p)
                both.setdefault(s, []).append((p, step))
        if w_dir is None and all(
                max(len(v) for v in per[d].values()) == 1 for d in DIRS):
            w_dir = w
        if w_both is None and max(len(v) for v in both.values()) == 1:
            w_both = w
        if w_dir is not None and w_both is not None:
            break
    return w_dir, w_both


W_DIR, W_BOTH = _uniqueness()
if W_DIR is None or W_BOTH is None:                    # pragma: no cover
    raise RuntimeError(
        'the committed map contains a route-wide alias at W <= %d; the '
        'corresponding acquisition mode is not implementable on this map '
        '(spec 3.3). The map is not altered.' % _W_MAX)


def _index(w, planes):
    """string -> list of (end_mm, step) over the requested direction planes."""
    out = {}
    for step in planes:
        for p in range(DNA_N):
            out.setdefault(window(p, step, w), []).append((p, step))
    return out


#: Unique-window lookups. A string absent from the table matches no route
#: position at all, which is the ordinary outcome for a window that straddles a
#: reversal or a missed marker.
UNIQUE_DIR = {step: _index(W_DIR, (step,)) for step in DIRS}
UNIQUE_BOTH = _index(W_BOTH, DIRS)

#: Markers carrying each polarity bit, for the unbounded-window case (6.2).
POL_MARKERS = {b: tuple(m for m in range(DNA_N) if DNA[m] == b) for b in (0, 1)}


def pol_bit(polarity):
    """Detection polarity letter -> DNA bit. 'N' is bit 1, as the map stores."""
    return 1 if polarity == 'N' else 0


def all_hypotheses(step=None):
    if step is None:
        return {(m, d) for m in range(DNA_N) for d in DIRS}
    return {(m, step) for m in range(DNA_N)}


def occupancy_markers(hypotheses, extent):
    """Every marker any candidate could occupy, expanded by train extent."""
    out = set()
    for item in hypotheses:
        mm = item[0] if isinstance(item, tuple) else item
        for k in range(-extent, extent + 1):
            out.add((mm + k) % DNA_N)
    return out


def next_station(mm, step, lookahead):
    """First station at least `lookahead` markers ahead in `step`."""
    best = None
    for name, centre in STATIONS.items():
        ahead = markers_ahead(mm, centre, step)
        if ahead >= lookahead and (best is None or ahead < best[1]):
            best = (name, ahead)
    return best[0] if best else None
