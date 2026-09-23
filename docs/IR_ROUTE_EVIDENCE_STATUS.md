# Route-distance evidence status

`firmware/common/IrRouteEvidence.h` now evaluates same/next/later candidate
travel against a supplied distance range. It reads surveyed CW segment lengths,
supports CW/CCW and wraparound, and returns UNKNOWN/POSSIBLE/EXCLUDED per
candidate. It neither changes route position nor chooses Hall identity.

`tools/test_ir_route_evidence.cpp` uses the actual NAVI_SIMPLIFIED RouteMap:
171 anchors x 2 directions = 342 cases. Tests passed for same marker, next
marker, and a later marker with one omission, using synthetic +/-10 mm IR
ranges and +/-20 mm combined Hall/map extent. Unknown IR bounds or unknown
extent leave all candidates UNKNOWN. These numerical bounds are test inputs,
NOT measured physical error claims.

Build with C++11 and include paths `firmware/common` and
`firmware/programs/NAVI_SIMPLIFIED`.

The helper assumes travel progressed in the supplied direction. Wheel IR does
not independently determine direction; the caller must establish monotonic
direction and reset/reject comparisons spanning reversals. Localization extent
must bound both Hall endpoints plus map error over the candidate travel.

No running NAVI sketch consumes this helper yet. Enabling it with an invented
error budget would violate the measurement contract. Field calibration and
independently measured error envelopes remain prerequisites. The current
distance contract defaults to unknown bounds, so this module cannot manufacture
a marker identity or justify advancement from current unvalidated counts.
