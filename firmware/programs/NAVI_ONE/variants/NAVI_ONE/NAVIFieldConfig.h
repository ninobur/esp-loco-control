#pragma once
#include <stdint.h>

// Operator-approved experimental field-test value, 2026-09-12.
// This is shared by the sketch and every acquisition gate so the test suite
// cannot silently exercise a different duration floor from the flashed image.
static constexpr uint16_t NAVI_PASSAGE_FLOOR_MS = 82;
