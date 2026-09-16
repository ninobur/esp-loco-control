#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// X19: THERE IS NO COMPLETED-PASSAGE DURATION FLOOR.
//
// Decision 0085's operator-approved 82 ms is the time between a 70-count entry
// crossing and a 25-count exit crossing of a GLOBAL reference. X19 has neither
// crossing: events are framed by departure from a local minimum and end when a
// fixed window expires. The number therefore has no definition in this build
// and is deliberately not carried across -- the 2026-09-15 replay measured
// that transplanting it literally rejects genuine magnets, including Otto's
// known ~83 ms borderline passage.
//
// What X19 does instead is MEASURE. Every candidate reports two widths on
// diag/excursion -- w_caliper_ms (samples at or beyond 25 counts from the
// excursion's local zero) and w_frac_ms (samples at or beyond 34% of its own
// peak) -- so 0085 can be re-derived from this build's own field evidence
// rather than assumed. Measured conversions from the replay, for reference
// when that re-derivation happens: w_caliper / X18 duration ~ 0.93,
// w_frac / X18 duration ~ 0.83.
//
// When a value is derived, it goes in DetectorConfig::widthFloorMs (default 0
// = disabled) and nowhere else. Do not reintroduce a constant here.
//
// The X18 value is retained ONLY as a documented historical reference so that
// a diff against X18 shows what was removed rather than what was renamed. It
// is not referenced by any X19 code path; `grep NAVI_PASSAGE_FLOOR_MS` over
// this sketch returns this file alone.
// ---------------------------------------------------------------------------
static constexpr uint16_t NAVI_X18_PASSAGE_FLOOR_MS = 82;   // historical, unused
