#pragma once

// Exact oriented, baseline-relative samples published by Otto after the
// 2026-09-12 Bamboo CCW withdrawal.  This is slot 0 of the six-passage crash
// window, not a hand-shaped substitute.
//
// Telemetry: MM151 N had just advanced.  This N event opened 558 ms later,
// lasted 47 ms, was judged peak 86 / ratio 0.483, and caused WRONG_MAGNET
// while MM150 S was expected.  The genuine MM150 S followed 689 ms later.
static constexpr short BAMBOO_20260912_TRANSIENT[] = {
  14,12,13,14,15,12,15,12,36,40,46,52,78,86,93,31,34,41,45,45,
  50,58,61,64,62,54,61,58,63,57,63,59,63,52,52,58,64,61,62,61,
  52,56,61,60,53,48,49,50,54,61,63,-1,-1,-1,-4,-2,-1,-2,-1,-1
};
static constexpr unsigned BAMBOO_20260912_TRANSIENT_N =
  sizeof(BAMBOO_20260912_TRANSIENT) / sizeof(BAMBOO_20260912_TRANSIENT[0]);
static constexpr unsigned BAMBOO_20260912_ENTRY_COUNTS = 70;
static constexpr unsigned BAMBOO_20260912_DURATION_MS = 47;
static constexpr unsigned BAMBOO_20260912_GAP_MS = 558;

