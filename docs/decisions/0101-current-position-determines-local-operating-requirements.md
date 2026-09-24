# 0101 - Current position determines local operating requirements

Status: Operator principle accepted (2026-09-23). First implementation prepared;
independent review and field acceptance pending. No deployment authorization implied.

## Decision

David: "Each MM segment has its own requirements. NAVI should know those as
thoroughly as he knows the MM map. What is required at mm15 should not depend
on a trigger at MM25."

NAVI asks what is required at its current position and direction. A pause
suspends automatic movement, not the operating meaning of the location.
Restart inside a station zone must use that zone's target and the remaining
stop requirement, not resume generic cruise because an earlier trigger was missed.

History remains necessary for obligations already fulfilled: a completed
station stop must not repeat merely because the train is still nearby. This
visit history is not permission to recognize the station in the first place.

## First Mechanism

POSITION_STATIONS_R1 evaluates the existing section PWM map and station regions
each AUTO service cycle. No per-MM values, station offsets, ramp constants or
dwell duration are retuned. It acquires any unserved station region directly
from current MM, including a start at its zero-ramp point. A deliberate pause
does not consume the active-phase watchdog; an already begun dwell continues
to count wall-clock stopped time. No generic cruise order is issued on GO.

These mechanisms are Codex's implementation choices for the authorized
principle, not separately field-accepted operator rulings. In particular,
late entry at/past the stop point is handled only within the existing +5-MM
station recovery region; outside it this change does not invent a stop mission.

This is the first application of position-based operating requirements, not
a speed governor, an arbitrary-destination planner or a replacement navigator.
Manual authority, hard protection and unresolved-location withdrawal remain.

## Evidence

`../NAVI_FINAL_RUN_ANALYSIS_20260923.md`: AUTO paused while crossing Patio's
CCW MM25 approach entry, resumed at MM24, then passed Patio at PWM90.
`../NAVI_POSITION_STATIONS_R1_20260923.md`: implementation, checks and review gate.
