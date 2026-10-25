#pragma once
// ---------------------------------------------------------------------------
// NaviTimingProfiles — approved, persistent timing profiles for NAVI.
//
// Each profile describes ONE directed mapped interval at ONE tested operating
// setting. It is intentionally empty today. The clean fixed-PWM CW/CCW runs
// at 60 and 90 are the evidence used to populate this file; no timing number
// is inferred from spacing, a different direction, or a different PWM. A
// profile is not a stop threshold. It is recognition evidence used with the
// Hall ensemble, polarity, and completion.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace navi_power {

struct TimingProfileDefinition {
  uint8_t fromMm;
  int8_t dir;
  uint8_t pwm;
  uint32_t expectedMs;
  uint32_t toleranceMs;
};

// Deliberately empty until field evidence defines normal variation. Keep one
// storage element because standard C++ does not permit a zero-length array.
static constexpr TimingProfileDefinition TIMING_PROFILES[1] = {};
static constexpr uint16_t TIMING_PROFILE_COUNT = 0;

}  // namespace navi_power
