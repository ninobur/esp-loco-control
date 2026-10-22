"""The surveyed route map, copied verbatim for offline research use.

Source: firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/RouteMap.h, commit
3a8953c (research/navi-rvm-shape-claude-20261018 lineage). That file is
itself generated from QUORUM.ino's original survey, corrected by David's
2026-10-02 remeasurement of intervals 011-012, 012-013 and 013-014 only.

This is a read-only copy for research replay. It does not modify firmware
and is not itself a governing declaration -- RouteMap.h remains that.
"""
from __future__ import annotations

ROUTE_N = 171

# 1 = North, 0 = South.
ROUTE_POLARITY = [
    1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0,
    0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
    0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0,
    1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0,
    1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1,
    1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0,
    1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0,
]

# Surveyed spacing in mm between marker i and marker i+1 (CW). Sums to
# 52264 mm (reflects David's 2026-10-02 remeasurement of 011-012, 012-013,
# 013-014; the rest is QUORUM's original survey).
ROUTE_SPACING_MM = [
    330, 340, 330, 315, 325, 330, 315, 300, 300, 295,
    300, 335, 324, 360, 315, 325, 310, 300, 300, 320,
    315, 315, 305, 300, 295, 300, 300, 300, 300, 315,
    330, 320, 315, 310, 300, 300, 300, 300, 300, 300,
    300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
    300, 300, 300, 300, 300, 300, 300, 295, 320, 300,
    315, 320, 315, 325, 315, 305, 300, 305, 300, 300,
    295, 295, 300, 300, 300, 300, 300, 300, 300, 305,
    300, 300, 305, 300, 300, 330, 300, 300, 305, 300,
    300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
    300, 300, 295, 300, 300, 300, 300, 300, 305, 300,
    300, 320, 320, 300, 300, 300, 300, 300, 300, 300,
    300, 300, 300, 300, 300, 300, 280, 300, 300, 290,
    300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
    300, 300, 300, 300, 305, 300, 305, 300, 295, 300,
    300, 300, 305, 300, 300, 305, 320, 290, 320, 300,
    300, 305, 330, 330, 320, 325, 315, 355, 330, 330,
    330,
]

assert len(ROUTE_POLARITY) == ROUTE_N
assert len(ROUTE_SPACING_MM) == ROUTE_N


def route_mod(v: int) -> int:
    return v % ROUTE_N


def next_marker(mm: int, direction: int) -> int:
    """direction: +1 = CW (ascending), -1 = CCW (descending)."""
    return route_mod(mm + direction)


def polarity_at(mm: int) -> int:
    return ROUTE_POLARITY[route_mod(mm)]


def span_mm(mm: int, direction: int) -> int:
    """Mapped distance from mm to the next marker in the given direction."""
    if direction > 0:
        return ROUTE_SPACING_MM[route_mod(mm)]
    return ROUTE_SPACING_MM[route_mod(mm - 1)]
