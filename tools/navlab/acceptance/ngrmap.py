"""The real committed NGR map: 171 markers, polarity DNA and spacing.

Read-only. The map is parsed from firmware/QUORUM/QUORUM.ino, which is the
single committed source of both tables; nothing here writes, patches or
"corrects" it. If a prerequisite test fails against this map, the map stands
and the test records the failure (acceptance plan P0).
"""
import re
import pathlib

_ROOT = pathlib.Path(__file__).resolve().parents[3]
_INO = _ROOT / 'firmware' / 'QUORUM' / 'QUORUM.ino'


def _extract(src, marker):
    body = src.split(marker)[1].split('};')[0]
    return [int(x) for x in re.findall(r'\d+', body)]


def _load():
    src = _INO.read_text()
    dna = _extract(src, 'const uint8_t NGR_DNA1[DNA_N] PROGMEM = {')
    spc = _extract(src, 'static const uint16_t spacingMm[DNA_N] PROGMEM = {')
    if len(dna) != len(spc):
        raise RuntimeError('map tables disagree in length: %d vs %d'
                           % (len(dna), len(spc)))
    if set(dna) - {0, 1}:
        raise RuntimeError('DNA is not binary polarity')
    return dna, spc


DNA, SPACING = _load()
DNA_N = len(DNA)
CIRCUIT_MM = sum(SPACING)

CW, CCW = +1, -1
DIRS = (CW, CCW)


def dirname(step):
    return 'CW' if step > 0 else 'CCW'


def nxt(mm, step):
    return (mm + step) % DNA_N


def step_mm(mm, step):
    """Distance travelled leaving marker `mm` in direction `step`."""
    return SPACING[mm] if step > 0 else SPACING[(mm - 1) % DNA_N]


def dist(a, b, step):
    """Distance from marker a to marker b travelling in `step`."""
    d, mm = 0, a
    for _ in range(DNA_N):
        if mm == b:
            return d
        d += step_mm(mm, step)
        mm = nxt(mm, step)
    return d


def window(end_mm, step, w):
    """The polarity string of the last `w` markers ending at `end_mm`."""
    return tuple(DNA[(end_mm - i * step) % DNA_N] for i in range(w - 1, -1, -1))


def markers_ahead(frm, to, step):
    """Marker count from frm to to travelling in step."""
    return ((to - frm) * step) % DNA_N


# --- stations, from the same committed firmware table ------------------------
STATIONS = {'Patio': 15, 'Grillers': 63, 'Arches': 107, 'Bamboo': 157}

# --- operator rulings, 2026-08-22 -------------------------------------------
LAUNCH_REGION = tuple(range(36, 46))          # MM036-MM045 inclusive, 10 markers
STATION_LOOKAHEAD_MARKERS = 12
FIRST_STATION = {CW: 'Grillers', CCW: 'Patio'}

assert len(LAUNCH_REGION) == 10
